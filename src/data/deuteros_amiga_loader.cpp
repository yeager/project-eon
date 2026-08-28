#include "data/deuteros_amiga_loader.hpp"
#include "data/sha256.hpp"

#include <algorithm>
#include <span>
#include <string>
#include <stdexcept>

namespace eon {
namespace {

std::uint16_t big16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) throw std::runtime_error("Truncated 68000 word");
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8U) | bytes[offset + 1]);
}

std::uint32_t big32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(big16(bytes, offset)) << 16U) | big16(bytes, offset + 2);
}

void require_word(std::span<const std::uint8_t> bytes, std::size_t offset, std::uint16_t opcode) {
    if (big16(bytes, offset) != opcode) {
        throw std::runtime_error("Unexpected Deuteros loader opcode at offset " + std::to_string(offset)
            + ", wanted " + std::to_string(opcode) + ", got " + std::to_string(big16(bytes, offset)));
    }
}

void require_long(std::span<const std::uint8_t> bytes, std::size_t offset, std::uint32_t value) {
    if (big32(bytes, offset) != value) throw std::runtime_error("Unexpected Deuteros loader longword");
}

} // namespace

DeuterosAmigaLoadPlan parse_deuteros_amiga_load_plan(const AmigaAdf& disk) {
    if (disk.kind() != AmigaDiskKind::dos || !disk.boot_checksum_valid()) {
        throw std::runtime_error("Not a verified Deuteros Amiga system disk");
    }
    const auto boot = disk.boot_block();
    // move.l #length,$24(a1); move.l #destination,$28(a1)
    require_word(boot, 0x32, 0x237c);
    require_word(boot, 0x3a, 0x237c);
    // move.l #track,d7; mulu.w #track_size,d7
    require_word(boot, 0x42, 0x2e3c);
    require_word(boot, 0x48, 0xcefc);
    // movea.l #entry,a0
    require_word(boot, 0x82, 0x207c);
    const auto loader_length = big32(boot, 0x34);
    const auto loader_destination = big32(boot, 0x3c);
    const auto loader_track = big32(boot, 0x44);
    const auto track_size = static_cast<std::uint32_t>(big16(boot, 0x4a));
    const auto loader_entry = big32(boot, 0x84);
    if (loader_length != track_size || track_size != AmigaAdf::sector_size * AmigaAdf::sectors_per_track) {
        throw std::runtime_error("Deuteros loader track geometry mismatch");
    }
    const AmigaLoadStage loader{loader_track * track_size, loader_length,
        loader_destination, loader_entry};
    if (loader.entry_address < loader.destination
        || loader.entry_address - loader.destination >= loader.length) {
        throw std::runtime_error("Deuteros bootstrap entry outside loaded track");
    }

    // Boot passes D0=0, selecting the first absolute function pointer at
    // memory $12a36. Translate it back into the loaded ADF track.
    require_word(boot, 0x88, 0x7000); // moveq #0,d0
    constexpr std::uint32_t profile_table_address = 0x12a36;
    const auto table_offset = loader.disk_offset + profile_table_address - loader.destination;
    const auto table = disk.bytes(table_offset, 8);
    const auto parse_profile = [&](std::size_t index) {
        const auto profile_address = big32(table, index * 4);
        if (profile_address < loader.destination
            || profile_address - loader.destination + 20 > loader.length) {
            throw std::runtime_error("Deuteros bootstrap profile outside loader track");
        }
        const auto profile_offset = loader.disk_offset + profile_address - loader.destination;
        const auto profile = disk.bytes(profile_offset, 20);
        // move.l #destination,d1; move.l #length,d0; move.l #track,d2; rts
        require_word(profile, 0, 0x223c);
        require_word(profile, 6, 0x203c);
        require_word(profile, 12, 0x243c);
        require_word(profile, 18, 0x4e75);
        const auto destination = big32(profile, 2);
        const auto length = big32(profile, 8);
        const auto track = big32(profile, 14);
        const auto disk_offset = track * track_size;
        static_cast<void>(disk.bytes(disk_offset, length));
        return DeuterosAmigaBootstrapProfile{disk_offset, length, destination};
    };
    const auto profile_zero = parse_profile(0);
    const auto title_handoff_profile = parse_profile(1);
    const auto stage_bytes = disk.bytes(profile_zero.disk_offset, profile_zero.length);
    require_word(stage_bytes, 0, 0x4ef9); // jmp absolute long
    const auto entry = big32(stage_bytes, 2);
    const AmigaLoadStage main_stage{profile_zero.disk_offset, profile_zero.length,
        profile_zero.destination, entry};

    // Decode the earliest main-stage facts from raw loaded bytes.  This has
    // deliberately no emulator or host-side state: it anchors the post-title
    // re-entry at $21734 without inventing the meaning of any service/cell.
    const auto main_offset = [&](std::uint32_t address) -> std::size_t {
        if (address < main_stage.destination || address - main_stage.destination >= main_stage.length) {
            throw std::runtime_error("Deuteros main-stage address outside raw load");
        }
        return static_cast<std::size_t>(address - main_stage.destination);
    };
    const auto entry_bytes = stage_bytes.subspan(main_offset(entry));
    require_word(entry_bytes, 0x00, 0x23c9); // move.l a1,$20976
    require_long(entry_bytes, 0x02, 0x20976);
    require_word(entry_bytes, 0x06, 0x33c0); // move.w d0,$21704
    require_long(entry_bytes, 0x08, 0x21704);
    require_word(entry_bytes, 0x0c, 0x2e7c); // movea.l #$22296,a7
    require_long(entry_bytes, 0x0e, 0x22296);
    require_word(entry_bytes, 0x1a, 0x203c); // move.l #$7fff0,d0
    require_long(entry_bytes, 0x1c, 0x7fff0);
    constexpr std::array<std::uint32_t, 2> initialization_calls{0x20068, 0x2013a};
    constexpr std::array<std::size_t, 2> call_offsets{0x28, 0x2e};
    for (std::size_t index = 0; index < initialization_calls.size(); ++index) {
        require_word(entry_bytes, call_offsets[index], 0x4eb9);
        require_long(entry_bytes, call_offsets[index] + 2, initialization_calls[index]);
    }
    // The first recurring loop begins at $217f6 and initializes two words,
    // enables a scheduler word, calls two services, then probes hardware
    // addresses/bits exactly as the original code does.
    constexpr std::uint32_t loop_address = 0x217f6;
    const auto loop_bytes = stage_bytes.subspan(main_offset(loop_address));
    require_word(loop_bytes, 0x00, 0x4e71);
    require_word(loop_bytes, 0x02, 0x4eb9);
    require_long(loop_bytes, 0x04, 0x22a5a);
    require_word(loop_bytes, 0x08, 0x33fc);
    require_long(loop_bytes, 0x0c, 0x21720);
    require_word(loop_bytes, 0x10, 0x33fc);
    require_long(loop_bytes, 0x14, 0x2171e);
    require_word(loop_bytes, 0x18, 0x33fc);
    if (big16(loop_bytes, 0x1a) != 1 || big32(loop_bytes, 0x1c) != 0x210f2) {
        throw std::runtime_error("Unexpected Deuteros scheduler enable setup");
    }
    require_word(loop_bytes, 0x20, 0x4eb9);
    require_long(loop_bytes, 0x22, 0x21276);
    require_word(loop_bytes, 0x26, 0x4eb9);
    require_long(loop_bytes, 0x28, 0x21380);
    require_word(loop_bytes, 0x2c, 0x0839); // btst #10,$dff016
    if (big16(loop_bytes, 0x2e) != 10 || big32(loop_bytes, 0x30) != 0xdff016) {
        throw std::runtime_error("Unexpected Deuteros custom input probe");
    }
    // Later in the same loop bit 6 at $bfe001 is probed. Keep both addresses
    // raw rather than assigning platform-control names.
    require_word(loop_bytes, 0x68, 0x0839);
    if (big16(loop_bytes, 0x6a) != 6 || big32(loop_bytes, 0x6c) != 0xbfe001) {
        throw std::runtime_error("Unexpected Deuteros CIA input probe");
    }
    // Values above two at $21982 branch to $2181c, the scheduler call rather
    // than one of the bootstrap exits. Decode the scheduler's fixed layout
    // and its wait dispatch from the raw stage. This provides a bounded
    // evidence model for the resumed path; the VM remains responsible for
    // executing only already understood channel opcodes.
    constexpr std::uint32_t scheduler_address = 0x21380;
    const auto scheduler = stage_bytes.subspan(main_offset(scheduler_address));
    require_word(scheduler, 0x00, 0x41f9); // lea $210f8,a0
    require_long(scheduler, 0x02, 0x210f8);
    require_word(scheduler, 0x06, 0x3e39); // move.w $21248,d7
    require_long(scheduler, 0x08, 0x21248);
    require_word(scheduler, 0x0c, 0x5347); // subq.w #1,d7
    require_word(scheduler, 0x0e, 0x2028); // move.l $10(a0),d0
    if (big16(scheduler, 0x10) != 0x0010) {
        throw std::runtime_error("Unexpected Deuteros scheduler program-state offset");
    }
    require_word(scheduler, 0x16, 0x2240); // movea.l d0,a1
    require_word(scheduler, 0x18, 0x3028); // move.w $6(a0),d0
    if (big16(scheduler, 0x1a) != 0x0006) {
        throw std::runtime_error("Unexpected Deuteros scheduler selector offset");
    }
    // Four mutually exclusive selector checks lead to timer, audio-position,
    // stepped-position, and gated-input handling respectively. The selectors
    // and their state offsets are literal bytecode facts, not inferred modes.
    require_word(scheduler, 0x20, 0xb03c); // cmp.b #3,d0
    if (big16(scheduler, 0x22) != 3) throw std::runtime_error("Unexpected scheduler timer selector");
    require_word(scheduler, 0x38, 0xb03c); // cmp.b #5,d0
    if (big16(scheduler, 0x3a) != 5) throw std::runtime_error("Unexpected scheduler audio selector");
    require_word(scheduler, 0x62, 0xb03c); // cmp.b #6,d0
    if (big16(scheduler, 0x64) != 6) throw std::runtime_error("Unexpected scheduler step selector");
    require_word(scheduler, 0x8c, 0xb03c); // cmp.b #$14,d0
    if (big16(scheduler, 0x8e) != 0x14) throw std::runtime_error("Unexpected scheduler input selector");
    require_word(scheduler, 0xba, 0x0839); // btst #5,$dff01f
    if (big16(scheduler, 0xbc) != 5 || big32(scheduler, 0xbe) != 0xdff01f) {
        throw std::runtime_error("Unexpected Deuteros scheduler tail probe");
    }
    require_word(scheduler, 0xc2, 0x4eb9); // jsr $21698
    require_long(scheduler, 0xc4, 0x21698);
    // The gated input path at $2188e reaches $21982.  Preserve the exact
    // unsigned comparison/clamp route rather than inferring meanings for the
    // state word or its two downstream services.
    constexpr std::uint32_t input_dispatch_address = 0x21982;
    const auto input_dispatch = stage_bytes.subspan(main_offset(input_dispatch_address));
    require_word(input_dispatch, 0x00, 0x3039); // move.w $21704,d0
    require_long(input_dispatch, 0x02, 0x21704);
    require_word(input_dispatch, 0x06, 0xb03c); // cmp.w #2,d0
    if (big16(input_dispatch, 0x08) != 2) {
        throw std::runtime_error("Unexpected Deuteros input-dispatch compare value");
    }
    require_word(input_dispatch, 0x0a, 0x640c); // bcc.s $2199a
    require_word(input_dispatch, 0x0c, 0x33fc); // move.w #1,$21704
    if (big16(input_dispatch, 0x0e) != 1 || big32(input_dispatch, 0x10) != 0x21704) {
        throw std::runtime_error("Unexpected Deuteros input-dispatch clamp");
    }
    require_word(input_dispatch, 0x14, 0x6000); // bra.w $218cc
    if (big16(input_dispatch, 0x16) != 0xff34) {
        throw std::runtime_error("Unexpected Deuteros input-dispatch service branch");
    }
    require_word(input_dispatch, 0x18, 0x6700); // beq.w $218cc
    if (big16(input_dispatch, 0x1a) != 0xff30) {
        throw std::runtime_error("Unexpected Deuteros input-dispatch equality branch");
    }
    require_word(input_dispatch, 0x1c, 0x6000); // bra.w $2181c
    if (big16(input_dispatch, 0x1e) != 0xfe7c) {
        throw std::runtime_error("Unexpected Deuteros input-dispatch continue branch");
    }
    // Both <= two input paths enter $218cc.  Its fixed post-service tail
    // advances $21704 and takes $21a4c for result two or $219f8 for result
    // three.  Decode the two exits from the genuine raw stage so callers do
    // not have to simulate a named menu/main screen to report the handoff.
    constexpr std::uint32_t dispatch_service_address = 0x218cc;
    const auto dispatch_service = stage_bytes.subspan(main_offset(dispatch_service_address));
    require_word(dispatch_service, 0x38, 0x3039); // move.w $21704,d0
    require_long(dispatch_service, 0x3a, 0x21704);
    require_word(dispatch_service, 0x3e, 0x5240); // addq.w #1,d0
    require_word(dispatch_service, 0x40, 0xb07c); // cmp.w #2,d0
    if (big16(dispatch_service, 0x42) != 2) {
        throw std::runtime_error("Unexpected Deuteros input-service first compare value");
    }
    require_word(dispatch_service, 0x44, 0x6700); // beq.w $21a4c
    if (big16(dispatch_service, 0x46) != 0x013a) {
        throw std::runtime_error("Unexpected Deuteros input-service first exit branch");
    }
    require_word(dispatch_service, 0x48, 0xb07c); // cmp.w #3,d0
    if (big16(dispatch_service, 0x4a) != 3) {
        throw std::runtime_error("Unexpected Deuteros input-service second compare value");
    }
    require_word(dispatch_service, 0x4c, 0x6700); // beq.w $219f8
    if (big16(dispatch_service, 0x4e) != 0x00de) {
        throw std::runtime_error("Unexpected Deuteros input-service second exit branch");
    }
    constexpr std::uint32_t first_exit_address = 0x21a4c;
    const auto first_exit = stage_bytes.subspan(main_offset(first_exit_address));
    require_word(first_exit, 0x00, 0x23fc); // move.l #1,$219f4
    if (big32(first_exit, 0x02) != 1 || big32(first_exit, 0x06) != 0x219f4) {
        throw std::runtime_error("Unexpected Deuteros input-service first exit profile write");
    }
    // The shared tail preserves incoming controller A1 and carries the
    // selected longword into the bootstrap's two return slots.
    require_word(first_exit, 0x46, 0x2039); // move.l $20976,d0
    require_long(first_exit, 0x48, 0x20976);
    require_word(first_exit, 0x4c, 0x23c0); // move.l d0,$12ff8
    require_long(first_exit, 0x4e, 0x12ff8);
    require_word(first_exit, 0x52, 0x2039); // move.l $219f4,d0
    require_long(first_exit, 0x54, 0x219f4);
    require_word(first_exit, 0x58, 0x23c0); // move.l d0,$12ffc
    require_long(first_exit, 0x5a, 0x12ffc);
    require_word(first_exit, 0x5e, 0x4e75); // rts
    constexpr std::uint32_t second_exit_address = 0x219f8;
    const auto second_exit = stage_bytes.subspan(main_offset(second_exit_address));
    require_word(second_exit, 0x00, 0x23fc); // move.l #5,$219f4
    if (big32(second_exit, 0x02) != 5 || big32(second_exit, 0x06) != 0x219f4) {
        throw std::runtime_error("Unexpected Deuteros input-service second exit profile write");
    }
    require_word(second_exit, 0x0a, 0x4eb9); // jsr $20b42
    require_long(second_exit, 0x0c, 0x20b42);
    require_word(second_exit, 0x10, 0xb0bc); // cmp.l #$4452f018,d0
    if (big32(second_exit, 0x12) != 0x4452f018) {
        throw std::runtime_error("Unexpected Deuteros input-service second exit match value");
    }
    require_word(second_exit, 0x16, 0x6746); // beq.b $21a56
    // The resource path is a real data-flow boundary after re-entering the
    // main stage. It uses D0 as a four-byte table index, makes a 4-byte probe
    // at the selected original disk offset, then uses that recovered
    // longword as the exact body-transfer length. Keep the raw destination
    // cells and retry gate rather than giving a format or UI meaning to them.
    constexpr std::uint32_t resource_loader_address = 0x21932;
    constexpr std::uint32_t resource_table_address = 0x21708;
    constexpr std::uint32_t resource_probe_address = 0x2ad24;
    constexpr std::uint32_t resource_payload_address = 0x32a24;
    constexpr std::uint32_t resource_transfer_address = 0x20a90;
    constexpr std::uint32_t resource_retry_address = 0x2196e;
    const auto resource_loader = stage_bytes.subspan(main_offset(resource_loader_address));
    require_word(resource_loader, 0x00, 0x41f9); // lea $21708,a0
    require_long(resource_loader, 0x02, resource_table_address);
    require_word(resource_loader, 0x06, 0xe548); // lsl.w #2,d0
    require_word(resource_loader, 0x08, 0x2e30); // move.l 0(a0,d0.w),d7
    if (big16(resource_loader, 0x0a) != 0) {
        throw std::runtime_error("Unexpected Deuteros resource-table index displacement");
    }
    require_word(resource_loader, 0x0c, 0x2f07); // move.l d7,-(sp)
    require_word(resource_loader, 0x0e, 0x23fc); // move.l #0,$2ad24
    if (big32(resource_loader, 0x10) != 0 || big32(resource_loader, 0x14) != resource_probe_address) {
        throw std::runtime_error("Unexpected Deuteros resource probe clear");
    }
    require_word(resource_loader, 0x18, 0x223c); // move.l #$2ad24,d1
    require_long(resource_loader, 0x1a, resource_probe_address);
    require_word(resource_loader, 0x1e, 0x7004); // moveq #4,d0
    require_word(resource_loader, 0x20, 0x4eb9); // jsr $20a90
    require_long(resource_loader, 0x22, resource_transfer_address);
    require_word(resource_loader, 0x26, 0x2e1f); // move.l (sp)+,d7
    require_word(resource_loader, 0x28, 0x2039); // move.l $2ad24,d0
    require_long(resource_loader, 0x2a, resource_probe_address);
    require_word(resource_loader, 0x2e, 0x670c); // beq.b $2196e
    require_word(resource_loader, 0x30, 0x223c); // move.l #$32a24,d1
    require_long(resource_loader, 0x32, resource_payload_address);
    require_word(resource_loader, 0x36, 0x4eb9); // jsr $20a90
    require_long(resource_loader, 0x38, resource_transfer_address);
    require_word(resource_loader, 0x3c, 0x0839); // btst #10,$dff016
    if (big16(resource_loader, 0x3e) != 10 || big32(resource_loader, 0x40) != 0xdff016) {
        throw std::runtime_error("Unexpected Deuteros resource retry probe");
    }
    require_word(resource_loader, 0x44, 0x67f6); // beq.b $2196e
    require_word(resource_loader, 0x46, 0x4e75); // rts
    const auto transfer = stage_bytes.subspan(main_offset(resource_transfer_address));
    require_word(transfer, 0x00, 0x2407); // move.l d7,d2
    require_word(transfer, 0x02, 0xb0bc); // cmp.l #$1600,d0
    if (big32(transfer, 0x04) != 0x1600) {
        throw std::runtime_error("Unexpected Deuteros resource transfer chunk length");
    }

    // This helper is the first fully recovered direct consumer of the bytes
    // loaded to $32a24. It saves A4, uses that exact payload base, combines
    // two original state cells, and reads a word through the masked index.
    // Do not assign a resource type or gameplay label here: the validated
    // instructions establish only the data-flow and arithmetic.
    constexpr std::uint32_t resource_consumer_address = 0x2016a;
    constexpr std::uint32_t resource_seed_address = 0x20168;
    constexpr std::uint32_t resource_counter_address = 0x2079e;
    const auto resource_consumer = stage_bytes.subspan(main_offset(resource_consumer_address));
    require_word(resource_consumer, 0x00, 0x2f0c); // move.l a4,-(sp)
    require_word(resource_consumer, 0x02, 0x49f9); // lea $32a24,a4
    require_long(resource_consumer, 0x04, resource_payload_address);
    require_word(resource_consumer, 0x08, 0x3039); // move.w $20168,d0
    require_long(resource_consumer, 0x0a, resource_seed_address);
    require_word(resource_consumer, 0x0e, 0xd0b9); // add.l $2079e,d0
    require_long(resource_consumer, 0x10, resource_counter_address);
    require_word(resource_consumer, 0x14, 0x0240); // andi.w #$3ffe,d0
    if (big16(resource_consumer, 0x16) != 0x3ffe) {
        throw std::runtime_error("Unexpected Deuteros resource consumer index mask");
    }
    require_word(resource_consumer, 0x18, 0x3034); // move.w 0(a4,d0.w),d0
    if (big16(resource_consumer, 0x1a) != 0) {
        throw std::runtime_error("Unexpected Deuteros resource consumer word displacement");
    }
    require_word(resource_consumer, 0x1c, 0x0640); // addi.w #14,d0
    if (big16(resource_consumer, 0x1e) != 14) {
        throw std::runtime_error("Unexpected Deuteros resource consumer word addend");
    }
    require_word(resource_consumer, 0x20, 0xd179); // add.w d0,$20168
    require_long(resource_consumer, 0x22, resource_seed_address);
    require_word(resource_consumer, 0x26, 0x285f); // movea.l (sp)+,a4
    require_word(resource_consumer, 0x28, 0x4e75); // rts

    // The command dispatcher reaches that consumer through two independent
    // literal command arms. Preserve only the verified compare/call pairs.
    constexpr std::array<std::uint16_t, 2> resource_consumer_commands{0x000a, 0x0011};
    constexpr std::array<std::uint32_t, 2> resource_consumer_call_sites{0x2159c, 0x2163a};
    for (std::size_t index = 0; index < resource_consumer_call_sites.size(); ++index) {
        const auto call_site = stage_bytes.subspan(main_offset(resource_consumer_call_sites[index] - 6));
        require_word(call_site, 0x00, 0xb03c); // cmp.w #literal,d0
        if (big16(call_site, 0x02) != resource_consumer_commands[index]) {
            throw std::runtime_error("Unexpected Deuteros resource-consumer command literal");
        }
        require_word(call_site, 0x04, index == 0 ? 0x661a : 0x661c); // bne.s around call
        require_word(call_site, 0x06, 0x4eb9); // jsr absolute long
        require_long(call_site, 0x08, resource_consumer_address);
    }

    // The following render pass distinguishes the $0f-installed $fe state
    // from ordinary indexed bitmaps. It passes state +12 directly to $20580;
    // do not infer a host bitmap format for that separate byte-stream path.
    constexpr std::uint32_t renderer_pass_address = 0x21448;
    constexpr std::uint16_t alternate_renderer_selector = 0x00fe;
    constexpr std::uint16_t alternate_renderer_state_data_offset = 0x000c;
    constexpr std::uint32_t alternate_renderer_address = 0x20580;
    constexpr std::uint32_t regular_renderer_address = 0x20c8c;
    const auto renderer_pass = stage_bytes.subspan(main_offset(renderer_pass_address));
    require_word(renderer_pass, 0x00, 0x41f9); // lea $210f8,a0
    require_long(renderer_pass, 0x02, 0x210f8);
    require_word(renderer_pass, 0x06, 0x3e39); // move.w $21248,d7
    require_long(renderer_pass, 0x08, 0x21248);
    require_word(renderer_pass, 0x0c, 0x5347); // subq.w #1,d7
    require_word(renderer_pass, 0x10, 0x3028); // move.w 6(a0),d0
    if (big16(renderer_pass, 0x12) != 6) {
        throw std::runtime_error("Unexpected Deuteros renderer selector state offset");
    }
    require_word(renderer_pass, 0x1a, 0xb07c); // cmp.w #$ff,d0
    if (big16(renderer_pass, 0x1c) != 0x00ff) {
        throw std::runtime_error("Unexpected Deuteros renderer disabled selector");
    }
    require_word(renderer_pass, 0x20, 0xb07c); // cmp.w #$fe,d0
    if (big16(renderer_pass, 0x22) != alternate_renderer_selector) {
        throw std::runtime_error("Unexpected Deuteros alternate renderer selector");
    }
    require_word(renderer_pass, 0x26, 0x2868); // movea.l 12(a0),a4
    if (big16(renderer_pass, 0x28) != alternate_renderer_state_data_offset) {
        throw std::runtime_error("Unexpected Deuteros alternate renderer state-data offset");
    }
    require_word(renderer_pass, 0x30, 0x4eb9); // jsr $20580
    require_long(renderer_pass, 0x32, alternate_renderer_address);
    require_word(renderer_pass, 0x3e, 0x4eb9); // jsr $20c8c
    require_long(renderer_pass, 0x40, regular_renderer_address);
    // $2162a stores $ffff into $210f4. Once the scheduler returns, $21856
    // tests that same cell and $2185c branches to $21892 when it is nonzero.
    // This is a raw main-stage request edge, not a named gameplay handoff.
    constexpr std::uint32_t channel_request_cell_address = 0x210f4;
    constexpr std::uint16_t channel_request_value = 0xffff;
    constexpr std::uint32_t channel_request_loop_test_address = 0x21856;
    constexpr std::uint32_t channel_request_loop_branch_address = 0x2185c;
    constexpr std::uint32_t channel_request_continuation_address = 0x21892;
    const auto channel_request_arm = stage_bytes.subspan(main_offset(0x2162a));
    require_word(channel_request_arm, 0x00, 0x33fc);
    if (big16(channel_request_arm, 0x02) != channel_request_value
        || big32(channel_request_arm, 0x04) != channel_request_cell_address) {
        throw std::runtime_error("Unexpected Deuteros channel-request write");
    }
    const auto channel_request_loop = stage_bytes.subspan(main_offset(channel_request_loop_test_address));
    require_word(channel_request_loop, 0x00, 0x4a39);
    require_long(channel_request_loop, 0x02, channel_request_cell_address);
    require_word(channel_request_loop, 0x06, 0x6634);
    const DeuterosAmigaMainStageEntry main_stage_entry{entry, 0x20976, 0x21704,
        0x22296, 0x7fff0, initialization_calls, loop_address, 0x22a5a, 0x21380, 0x21720,
        0x2171e, 0x210f2, 1, 0xdff016, 10, 0xbfe001, 6, 0x210f8, 0x21248, 0x18,
        0x10, 0x06, 0x08, {3, 5, 6, 0x14}, 0xdff01f, 5, 0x21698, input_dispatch_address,
        0x21704, 2, 1, 0x218cc, 0x2181c, 0x21704, 2, 0x21a4c, 3, 0x219f8,
        0x219f4, 1, 0x12ff8, 0x12ffc, 0x219f4, 5, 0x20b42, 0x4452f018, 0x21a56,
        resource_loader_address, resource_table_address, 2, resource_probe_address,
        resource_payload_address, resource_transfer_address, 0x1600, 0xdff016, 10,
        resource_retry_address, resource_consumer_address, resource_payload_address,
        resource_seed_address, resource_counter_address, 0x3ffe, 14,
        resource_consumer_commands, resource_consumer_call_sites,
        renderer_pass_address, alternate_renderer_selector,
        alternate_renderer_state_data_offset, alternate_renderer_address,
        regular_renderer_address, channel_request_cell_address,
        channel_request_value, channel_request_loop_test_address,
        channel_request_loop_branch_address, channel_request_continuation_address};

    // The resource loader at $21932 indexes five longwords at $21708. Both
    // addresses reside in the verified main stage, so translate the table
    // back to its ADF position instead of duplicating its contents.
    if (resource_table_address < main_stage.destination
        || resource_table_address - main_stage.destination + 20 > main_stage.length) {
        throw std::runtime_error("Deuteros resource table outside main stage");
    }
    const auto resource_table = disk.bytes(main_stage.disk_offset
        + resource_table_address - main_stage.destination, 20);
    std::array<std::uint32_t, 5> resource_offsets{};
    for (std::size_t index = 0; index < resource_offsets.size(); ++index) {
        resource_offsets[index] = big32(resource_table, index * 4);
        static_cast<void>(disk.bytes(resource_offsets[index], 1));
    }

    // The selected title profile is not a data-only blob: its first longword
    // is the 68000 absolute-JMP vector.  Decode it from the original track,
    // and insist that it remains within the loaded memory interval.  This is
    // deliberately distinct from profile zero's main-stage check above.
    const auto title_bytes = disk.bytes(title_handoff_profile.disk_offset,
        title_handoff_profile.length);
    require_word(title_bytes, 0, 0x4ef9); // jmp absolute long
    const auto title_entry = big32(title_bytes, 2);
    if (title_entry < title_handoff_profile.destination
        || title_entry - title_handoff_profile.destination >= title_handoff_profile.length) {
        throw std::runtime_error("Deuteros title entry outside loaded title stage");
    }
    const AmigaLoadStage title_stage{title_handoff_profile.disk_offset,
        title_handoff_profile.length, title_handoff_profile.destination, title_entry};
    return {loader, main_stage, main_stage_entry, resource_offsets, title_handoff_profile, title_stage};
}

DeuterosAmigaChannelRequestContinuation
parse_deuteros_amiga_channel_request_continuation(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan) {
    constexpr std::uint32_t entry = 0x21892;
    constexpr std::size_t length = 58;
    constexpr std::array<std::uint8_t, length> expected{{
        0x20, 0x39, 0x00, 0x02, 0x12, 0x6a, 0x67, 0x08,
        0x61, 0x00, 0x0a, 0x00, 0x61, 0x00, 0x0c, 0x02,
        0x4e, 0xb9, 0x00, 0x02, 0x2a, 0x5a, 0x20, 0x39,
        0x00, 0x02, 0x07, 0x9e, 0x22, 0x39, 0x00, 0x02,
        0x07, 0x9e, 0xb0, 0x81, 0x67, 0xf6, 0x4e, 0xb9,
        0x00, 0x02, 0x08, 0xba, 0x08, 0x39, 0x00, 0x06,
        0x00, 0xbf, 0xe0, 0x01, 0x67, 0xf6, 0x60, 0x00,
        0xff, 0x2c,
    }};
    const auto& stage = plan.main_stage;
    if (plan.main_stage_entry.channel_request_continuation_address != entry
        || entry < stage.destination || entry - stage.destination > stage.length
        || length > stage.length - (entry - stage.destination)) {
        throw std::runtime_error("Unexpected Deuteros channel-request continuation placement");
    }
    const auto disk_offset = stage.disk_offset + entry - stage.destination;
    const auto bytes = disk.bytes(disk_offset, length);
    constexpr auto expected_hash = "120fba90e0b4fa9e96d8a6cf95fbac512d67d7daa42c3776ce0d3066b3f02ee9";
    if (!std::equal(expected.begin(), expected.end(), bytes.begin())
        || to_hex(sha256(bytes)) != expected_hash) {
        throw std::runtime_error("Unexpected Deuteros channel-request continuation");
    }
    return {entry, 0x2126a, 0x21898, 0x218a2,
        {0x2189a, 0x2189e}, {0x2229c, 0x224a2}, 0x218a2, 0x22a5a,
        0x2079e, 0x218b6, 0x218ae, 0x218b8, 0x208ba,
        0x218be, 6, 0x218c6, 0x218be, 0x218c8, 0x217f6, expected_hash};
}

DeuterosAmigaChannelRequestFirstCallee
parse_deuteros_amiga_channel_request_first_callee(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    const DeuterosAmigaChannelRequestContinuation& continuation) {
    constexpr std::uint32_t entry = 0x2229c;
    constexpr std::size_t length = 0x94;
    constexpr std::array<std::uint8_t, length> expected{{
        0x33, 0xfc, 0x01, 0x00, 0x00, 0x02, 0x22, 0x9a, 0x13, 0xfc, 0x00, 0x00,
        0x00, 0x02, 0x07, 0xea, 0x08, 0x39, 0x00, 0x05, 0x00, 0xdf, 0xf0, 0x1f,
        0x67, 0x76, 0x74, 0x0f, 0x49, 0xf9, 0x00, 0x01, 0x2e, 0xcc, 0x30, 0x14,
        0xb0, 0x7c, 0x01, 0x00, 0x65, 0x04, 0x04, 0x40, 0x01, 0x00, 0xb0, 0x3c,
        0x00, 0x10, 0x65, 0x04, 0x04, 0x00, 0x00, 0x10, 0x12, 0x00, 0x02, 0x01,
        0x00, 0x0f, 0x67, 0x02, 0x53, 0x00, 0x38, 0xc0, 0x51, 0xca, 0xff, 0xdc,
        0x41, 0xf9, 0x00, 0x01, 0x2e, 0x12, 0x43, 0xf9, 0x00, 0x01, 0x2e, 0xcc,
        0x2f, 0x09, 0x30, 0x3c, 0x00, 0x10, 0x2c, 0x79, 0x00, 0x01, 0x2f, 0xec,
        0x4e, 0xae, 0xff, 0x40, 0x22, 0x5f, 0x41, 0xf9, 0x00, 0x01, 0x2f, 0x12,
        0x30, 0x3c, 0x00, 0x10, 0x2c, 0x79, 0x00, 0x01, 0x2f, 0xec, 0x4e, 0xae,
        0xff, 0x40, 0x51, 0x79, 0x00, 0x02, 0x22, 0x9a, 0x66, 0x0e, 0x4e, 0xb9,
        0x00, 0x02, 0x16, 0x98, 0x4e, 0xb9, 0x00, 0x02, 0x16, 0x98, 0x4e, 0x75,
        0x60, 0x00, 0xff, 0x7e,
    }};
    const auto& stage = plan.main_stage;
    if (continuation.local_call_targets[0] != entry || entry < stage.destination
        || entry - stage.destination > stage.length
        || length > stage.length - (entry - stage.destination)) {
        throw std::runtime_error("Unexpected Deuteros channel-request first callee placement");
    }
    const auto bytes = disk.bytes(stage.disk_offset + entry - stage.destination, length);
    constexpr auto expected_hash = "d1a162af50f92b60d03b1da4ab186a547e46d145b0599cfbbeff7fb5af324ac1";
    if (!std::equal(expected.begin(), expected.end(), bytes.begin())
        || to_hex(sha256(bytes)) != expected_hash) {
        throw std::runtime_error("Unexpected Deuteros channel-request first callee");
    }
    return {entry, 0x2229a, 0x0100, 0x207ea, 0x222ac, 5, 0x222b4, 0x2232c,
        0x000f, 0x222e0, 0x222be, 0x12fec, {0x222fc, 0x22312},
        {0x12e12, 0x12f12}, 0x2229a, 8, 0x2231c, 0x2232c,
        {0x2231e, 0x22324}, 0x21698, 0x2232a, expected_hash};
}

DeuterosAmigaChannelRequestSecondCallee
parse_deuteros_amiga_channel_request_second_callee(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    const DeuterosAmigaChannelRequestContinuation& continuation) {
    constexpr std::uint32_t entry = 0x224a2;
    constexpr std::size_t length = 42;
    constexpr std::array<std::uint8_t, length> expected{{
        0x21, 0xf9, 0x00, 0x02, 0x24, 0xe6, 0x00, 0x6c,
        0x42, 0x79, 0x00, 0xdf, 0xf0, 0xa8, 0x42, 0x79,
        0x00, 0xdf, 0xf0, 0xb8, 0x42, 0x79, 0x00, 0xdf,
        0xf0, 0xc8, 0x42, 0x79, 0x00, 0xdf, 0xf0, 0xd8,
        0x33, 0xfc, 0x00, 0x0f, 0x00, 0xdf, 0xf0, 0x96,
        0x4e, 0x75,
    }};
    const auto& stage = plan.main_stage;
    if (continuation.local_call_targets[1] != entry || entry < stage.destination
        || entry - stage.destination > stage.length
        || length > stage.length - (entry - stage.destination)) {
        throw std::runtime_error("Unexpected Deuteros channel-request second callee placement");
    }
    const auto bytes = disk.bytes(stage.disk_offset + entry - stage.destination, length);
    constexpr auto expected_hash = "d4e9a1ee0065537a627cdd9ee8827f11d5fa28e0f860aacb21bbdc7e11784bd1";
    if (!std::equal(expected.begin(), expected.end(), bytes.begin())
        || to_hex(sha256(bytes)) != expected_hash) {
        throw std::runtime_error("Unexpected Deuteros channel-request second callee");
    }
    return {entry, 0x224e6, 0x006c, {0xdff0a8, 0xdff0b8, 0xdff0c8, 0xdff0d8},
        0x000f, 0xdff096, 0x224ca, expected_hash};
}

DeuterosAmigaChannelRequestFollowingService
parse_deuteros_amiga_channel_request_following_service(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    const DeuterosAmigaChannelRequestContinuation& continuation) {
    constexpr std::uint32_t entry = 0x22a5a;
    constexpr std::size_t length = 0x130;
    const auto& stage = plan.main_stage;
    if (continuation.following_call_target != entry || entry < stage.destination
        || entry - stage.destination > stage.length
        || length > stage.length - (entry - stage.destination)) {
        throw std::runtime_error("Unexpected Deuteros channel-request following service placement");
    }
    const auto bytes = disk.bytes(stage.disk_offset + entry - stage.destination, length);
    constexpr auto expected_hash = "d5fdbdacd004d2cf377ea0dbaefb9d8b308ba23b568cfb3785456622bde49d19";
    if (to_hex(sha256(bytes)) != expected_hash) {
        throw std::runtime_error("Unexpected Deuteros channel-request following service");
    }
    return {entry, 0x22a30, 0, 0x000f, 0x22ab8, 0x22a6a, 0x22a6e, 0x000e,
        0x22aaa, 0x32a24, 0x22a6c, {1, 2, 4, 8}, 0x22b88, expected_hash};
}

DeuterosAmigaChannelRequestAdjacentEntry
parse_deuteros_amiga_channel_request_adjacent_entry(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    const DeuterosAmigaChannelRequestFollowingService& service) {
    constexpr std::uint32_t entry = 0x22b8a;
    constexpr std::size_t length = 96;
    constexpr std::array<std::uint8_t, length> expected{{
        0x4a,0x39,0x00,0x02,0x2a,0x30,0x67,0x02,0x4e,0x75,0xc0,0xfc,0x00,0x0e,
        0x20,0x79,0x00,0x02,0x2a,0xa6,0xd0,0xc0,0x20,0x10,0x6a,0x04,0x20,0x68,
        0x00,0x04,0x43,0xf9,0x00,0x02,0x2a,0x6e,0xe2,0x09,0x64,0x06,0x23,0x68,
        0x00,0x0a,0x00,0x0a,0xd3,0xfc,0x00,0x00,0x00,0x0e,0xe2,0x09,0x64,0x06,
        0x23,0x68,0x00,0x0a,0x00,0x0a,0xd3,0xfc,0x00,0x00,0x00,0x0e,0xe2,0x09,
        0x64,0x06,0x23,0x68,0x00,0x0a,0x00,0x0a,0xd3,0xfc,0x00,0x00,0x00,0x0e,
        0xe2,0x09,0x64,0x06,0x23,0x68,0x00,0x0a,0x00,0x0a,0x4e,0x75,
    }};
    const auto& stage = plan.main_stage;
    if (service.return_address != 0x22b88 || entry < stage.destination
        || entry - stage.destination > stage.length
        || length > stage.length - (entry - stage.destination)) {
        throw std::runtime_error("Unexpected Deuteros channel-request adjacent entry placement");
    }
    const auto bytes = disk.bytes(stage.disk_offset + entry - stage.destination, length);
    constexpr auto expected_hash = "10ed8be15c107dbb56ca98eb8d17ffd2bce3910dd169d67ba058447c9031b1ff";
    if (!std::equal(expected.begin(), expected.end(), bytes.begin())
        || to_hex(sha256(bytes)) != expected_hash) {
        throw std::runtime_error("Unexpected Deuteros channel-request adjacent entry");
    }
    return {entry, 0x22a30, 0x22b90, 0x22b94, 0x22b92, 0x000e, 0x22aa6,
        0x22ba2, 0x22ba8, 0x22a6e, 0x000a, 0x000e,
        {0x22bae,0x22bbe,0x22bce,0x22bde}, {0x22bb0,0x22bc0,0x22bd0,0x22be0},
        {0x22bb8,0x22bc8,0x22bd8,0x22be8}, {0x22bb2,0x22bc2,0x22bd2,0x22be2},
        0x22be8, expected_hash};
}

std::optional<DeuterosAmigaMainResourceTransfer>
read_deuteros_amiga_main_resource(const AmigaAdf& disk,
    const DeuterosAmigaLoadPlan& plan, std::uint16_t resource_index) {
    if (resource_index >= plan.resource_disk_offsets.size()) {
        throw std::runtime_error("Deuteros main resource index outside original table");
    }
    const auto source_disk_offset = plan.resource_disk_offsets[resource_index];
    const auto probe = disk.bytes(source_disk_offset, 4);
    const auto payload_length = big32(probe, 0);
    if (payload_length == 0) return std::nullopt;

    // $21932 restores D7 before the second call, while $20a90 receives D0
    // unchanged. Thus the original body begins at the same raw disk offset
    // as the four-byte probe rather than immediately after it.
    const auto source = disk.bytes(source_disk_offset, payload_length);
    return DeuterosAmigaMainResourceTransfer{
        resource_index,
        source_disk_offset,
        plan.main_stage_entry.resource_probe_address,
        plan.main_stage_entry.resource_payload_address,
        payload_length,
        std::vector<std::uint8_t>(source.begin(), source.end()),
    };
}

DeuterosAmigaResourceConsumerSample
sample_deuteros_amiga_main_resource_consumer(
    const DeuterosAmigaMainResourceTransfer& transfer,
    const DeuterosAmigaMainStageEntry& entry,
    std::uint16_t seed, std::uint32_t counter) {
    if (transfer.payload_destination_address != entry.resource_consumer_base_address
        || transfer.payload_destination_address != entry.resource_payload_address) {
        throw std::runtime_error("Deuteros resource consumer payload destination mismatch");
    }
    if (transfer.payload.size() != transfer.payload_length || transfer.payload.size() < 4) {
        throw std::runtime_error("Malformed Deuteros resource consumer payload");
    }
    if (big32(transfer.payload, 0) != transfer.payload_length) {
        throw std::runtime_error("Deuteros resource consumer length word mismatch");
    }
    if (entry.resource_consumer_index_mask != 0x3ffe
        || entry.resource_consumer_word_addend != 14) {
        throw std::runtime_error("Unsupported Deuteros resource consumer layout");
    }
    const auto index = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(seed + static_cast<std::uint16_t>(counter))
        & entry.resource_consumer_index_mask);
    if (static_cast<std::size_t>(index) + 2 > transfer.payload.size()) {
        throw std::runtime_error("Deuteros resource consumer lookup outside payload");
    }
    const auto word = big16(transfer.payload, index);
    const auto addend_result = static_cast<std::uint16_t>(word + entry.resource_consumer_word_addend);
    return {seed, counter, index, word, addend_result,
        static_cast<std::uint16_t>(seed + addend_result)};
}

} // namespace eon
