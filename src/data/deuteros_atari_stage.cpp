#include "data/deuteros_atari_boot.hpp"
#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

namespace eon {
namespace {

std::uint16_t be16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::runtime_error("Truncated Deuteros Atari ST first-stage word");
    }
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8U)
        | bytes[offset + 1]);
}

std::uint32_t be32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw std::runtime_error("Truncated Deuteros Atari ST first-stage longword");
    }
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U)
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 2]) << 8U) | bytes[offset + 3];
}

void require_bytes(std::span<const std::uint8_t> bytes, std::size_t offset,
    std::span<const std::uint8_t> expected, const char* what) {
    if (offset > bytes.size() || expected.size() > bytes.size() - offset
        || !std::equal(expected.begin(), expected.end(),
            bytes.begin() + static_cast<std::ptrdiff_t>(offset))) {
        throw std::runtime_error(what);
    }
}

std::size_t relative_target(const std::size_t base, const std::int64_t displacement,
    const std::size_t extent, const char* what) {
    if (base > extent) throw std::runtime_error(what);
    if (displacement >= 0) {
        const auto forward = static_cast<std::uint64_t>(displacement);
        if (forward > extent - base) throw std::runtime_error(what);
        return base + static_cast<std::size_t>(forward);
    }
    // Avoid negating INT64_MIN even though current 68000 displacements are
    // 8/16-bit: this helper remains safe if a wider decoded field is added.
    const auto backward = static_cast<std::uint64_t>(-(displacement + 1)) + 1U;
    if (backward > base) throw std::runtime_error(what);
    return base - static_cast<std::size_t>(backward);
}

DeuterosAtariRawRangeLoadPlan build_raw_range_load_plan(
    const std::uint32_t destination, const std::uint32_t byte_count,
    const std::uint32_t source_offset, const DeuterosAtariSecondStageProfile& stage) {
    constexpr std::uint32_t bytes_per_sector = 512;
    constexpr std::uint32_t bytes_per_side = 0x1200;
    if (stage.raw_read_max_sector_count != 9 || byte_count == 0
        || byte_count % bytes_per_sector != 0 || source_offset % bytes_per_side != 0) {
        throw std::runtime_error("Unsupported Deuteros Atari ST raw range load plan");
    }
    DeuterosAtariRawRangeLoadPlan result{
        .destination = destination,
        .byte_count = byte_count,
        .source_linear_sector = source_offset / bytes_per_side,
        .source_offset = source_offset,
        .requests = {},
    };
    auto remaining = byte_count;
    auto current_source_offset = source_offset;
    while (remaining != 0) {
        const auto chunk_bytes = std::min(remaining, bytes_per_side);
        const auto linear_side = current_source_offset / bytes_per_side;
        result.requests.push_back({
            .track = static_cast<std::uint16_t>(linear_side / 2U),
            .side = static_cast<std::uint8_t>(linear_side % 2U),
            .first_sector = 1,
            .sector_count = static_cast<std::uint16_t>(chunk_bytes / bytes_per_sector),
            .source_offset = current_source_offset,
        });
        remaining -= chunk_bytes;
        current_source_offset += chunk_bytes;
    }
    return result;
}

} // namespace

DeuterosAtariFirstStageProfile parse_deuteros_atari_first_stage(
    std::span<const std::uint8_t> bytes) {
    if (bytes.size() != 0x1200U || be16(bytes, 0) != 0x6000U) {
        throw std::runtime_error("Unsupported Deuteros Atari ST first-stage header");
    }
    // 68000 word branch displacement is relative to the extension word's
    // address. The boot stage jumps directly to $09c4.
    const auto entry_offset = relative_target(2U, static_cast<std::int16_t>(be16(bytes, 2)),
        bytes.size(), "Deuteros Atari ST first-stage entry leaves stage");
    if (entry_offset != 0x9c4U || entry_offset >= bytes.size()) {
        throw std::runtime_error("Unexpected Deuteros Atari ST first-stage entry");
    }
    // The checksum loop scans $43c literal stage bytes from offset $6, with
    // add-byte/rotate-left-8 and this seed/comparison pair.
    constexpr std::array<std::uint8_t, 16> checksum_setup{{
        0x41, 0xfa, 0xf5, 0xe4, // lea $10006(pc),a0
        0x20, 0x3c, 0x00, 0x00, 0x04, 0x3b,
        0x22, 0x3c, 0x22, 0x22, 0x55, 0x55}};
    require_bytes(bytes, 0xa20, checksum_setup, "Unexpected Deuteros Atari ST checksum setup");
    constexpr std::array<std::uint8_t, 6> checksum_compare{{0x0c, 0x81, 0x7a, 0xe2, 0x6a, 0xf7}};
    require_bytes(bytes, 0xa3a, checksum_compare, "Unexpected Deuteros Atari ST checksum comparison");
    // After validation, the stage starts another literal Floprd call. Its
    // preceding register setup fixes physical track 2 and RAM $70000.
    constexpr std::array<std::uint8_t, 14> next_stage_setup{{
        0x7a, 0x00, 0x7c, 0x02, 0x7e, 0x00,
        0x4d, 0xf9, 0x00, 0x07, 0x00, 0x00,
        0x48, 0x56}};
    require_bytes(bytes, 0xa68, next_stage_setup, "Unexpected Deuteros Atari ST next-stage setup");
    constexpr std::array<std::uint8_t, 26> next_stage_floprd{{
        0x3f, 0x3c, 0x00, 0x09, 0x3f, 0x07, 0x3f, 0x06,
        0x3f, 0x3c, 0x00, 0x01, 0x42, 0x67, 0x42, 0xa7,
        0x48, 0x56, 0x3f, 0x3c, 0x00, 0x08, 0x4e, 0x4e,
        0x4f, 0xef}};
    require_bytes(bytes, 0xa9c, next_stage_floprd, "Unexpected Deuteros Atari ST next-stage Floprd");
    constexpr std::array<std::uint8_t, 20> copy_setup{{
        0x20, 0x5f, 0x43, 0xf8, 0x1e, 0x00,
        0x2f, 0x09, 0x20, 0x3c, 0x00, 0x00, 0x11, 0xff,
        0x12, 0xd8, 0x51, 0xc8, 0xff, 0xfc}};
    require_bytes(bytes, 0xac8, copy_setup, "Unexpected Deuteros Atari ST first-stage copy");

    return {.entry_offset = entry_offset,
        .checksum_start_offset = 6,
        // BRA enters DBRA first, which predecrements D0=$43b before the
        // first body iteration. The ADD.B/ROL.L body therefore runs exactly
        // $43b times over stage +$6 through +$440 inclusive.
        .checksum_byte_count = static_cast<std::size_t>(be32(bytes, 0xa26)),
        .checksum_seed = be32(bytes, 0xa2c),
        .checksum_expected = be32(bytes, 0xa3c),
        .next_track = static_cast<std::uint16_t>(be16(bytes, 0xa6a) & 0x00ffU),
        .next_side = 0,
        .next_sector = 1,
        .next_sector_count = be16(bytes, 0xa9e),
        .next_destination = be32(bytes, 0xa70),
        // +$a74 pushes A6 ($70000) before the callback chain; +$ac8 pops
        // that exact word into A0 and the following loop copies it to $1e00.
        .copy_source = be32(bytes, 0xa70),
        .copy_destination = be16(bytes, 0xacc),
        .copy_byte_count = static_cast<std::size_t>(be32(bytes, 0xad2)) + 1U};
}

std::uint32_t calculate_deuteros_atari_first_stage_checksum(
    const std::span<const std::uint8_t> bytes, const DeuterosAtariFirstStageProfile& profile) {
    if (profile.checksum_start_offset > bytes.size()
        || profile.checksum_byte_count > bytes.size() - profile.checksum_start_offset) {
        throw std::runtime_error("Deuteros Atari ST checksum range outside first stage");
    }
    auto checksum = profile.checksum_seed;
    for (const auto value : bytes.subspan(profile.checksum_start_offset, profile.checksum_byte_count)) {
        // The original is ADD.B (A0)+,D1, not a longword add: preserve
        // D1's upper 24 bits and deliberately discard the low-byte carry.
        checksum = (checksum & 0xffffff00U)
            | static_cast<std::uint32_t>((checksum + value) & 0xffU);
        checksum = (checksum << 8U) | (checksum >> 24U);
    }
    return checksum;
}

DeuterosAtariSecondStageProfile parse_deuteros_atari_second_stage(
    std::span<const std::uint8_t> bytes) {
    if (bytes.size() != 0x1200U) {
        throw std::runtime_error("Unexpected Deuteros Atari ST second-stage length");
    }
    // This is executable 68000 setup at its loaded RAM origin $70000, not a
    // resource header: supervisor/user stack setup then JMP $00001ec4.
    constexpr std::array<std::uint8_t, 36> entry{{
        0x40, 0xc0, 0x08, 0x80, 0x00, 0x0d, 0x67, 0x10,
        0x4f, 0xf9, 0x00, 0x07, 0xb0, 0x00,
        0x43, 0xf9, 0x00, 0x00, 0x24, 0x78,
        0x4e, 0x61, 0x46, 0xc0,
        0x4f, 0xf9, 0x00, 0x00, 0x24, 0x78,
        0x4e, 0xf9, 0x00, 0x00, 0x1e, 0xc4}};
    require_bytes(bytes, 0, entry, "Unexpected Deuteros Atari ST second-stage entry");
    // The following reusable raw-reader caps a request at 9 sectors and
    // converts linear tracks >= $50 to side 1 with a $50 subtraction before
    // calling XBIOS Floprd (function 8).
    constexpr std::array<std::uint8_t, 74> raw_reader{{
        0x74, 0x09, 0xb0, 0xbc, 0x00, 0x00, 0x12, 0x00,
        0x64, 0x08, 0xe0, 0x48, 0xe2, 0x48, 0x52, 0x40,
        0x34, 0x00, 0x28, 0x07, 0x76, 0x00, 0xb8, 0x7c,
        0x00, 0x50, 0x65, 0x06, 0x76, 0x01, 0x04, 0x44,
        0x00, 0x50, 0x3f, 0x02, 0x3f, 0x03, 0x3f, 0x04,
        0x3f, 0x3c, 0x00, 0x01, 0x3f, 0x3c, 0x00, 0x00,
        0x2f, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x2f, 0x01,
        0x3f, 0x3c, 0x00, 0x08, 0x4e, 0x4e, 0xdf, 0xfc,
        0x00, 0x00, 0x00, 0x14, 0x31, 0xc0, 0x1e, 0x28,
        0x4e, 0x75}};
    require_bytes(bytes, 0x60, raw_reader, "Unexpected Deuteros Atari ST raw-reader routine");
    // The copied entry at $1ec4 maps to track-2 offset $c4. It obtains a
    // runtime state word, indexes the vector table at $1eac, then forwards
    // the handler's D1/D2 result to the local raw reader at $70030.
    constexpr std::array<std::uint8_t, 86> copied_dispatch{{
        0x20, 0x38, 0x25, 0xfc, 0x31, 0xc0, 0x1e, 0xaa,
        0x4f, 0xf9, 0x00, 0x00, 0x24, 0x78,
        0x2f, 0x3c, 0x00, 0x00, 0x1f, 0xa6, 0x3f, 0x3c, 0x00, 0x26, 0x4e, 0x4e,
        0x20, 0x38, 0x25, 0xf4, 0xb0, 0xbc, 0x00, 0x07, 0x11, 0x00,
        0x67, 0x08, 0x61, 0x00, 0x07, 0x14, 0x61, 0x00, 0x10, 0x32,
        0x4f, 0xf9, 0x00, 0x00, 0x24, 0x78,
        0x43, 0xf8, 0x1e, 0xac, 0x30, 0x38, 0x1e, 0xaa,
        0xe5, 0x48, 0x22, 0x71, 0x00, 0x00, 0x4e, 0x91,
        0x2f, 0x01, 0xc4, 0xfc, 0x12, 0x00, 0x2e, 0x02,
        0x61, 0x00, 0xff, 0x1e, 0x30, 0x38, 0x1e, 0xaa, 0x4e, 0x75}};
    require_bytes(bytes, 0xc4, copied_dispatch, "Unexpected Deuteros Atari ST copied dispatcher");
    return {.supervisor_stack = be32(bytes, 0xa),
        .application_stack = be32(bytes, 0x10),
        .direct_entry = be32(bytes, 0x20),
        .direct_entry_source_offset = 0xc4,
        .dispatch_state_address = be16(bytes, 0xca),
        .dispatch_table_address = be16(bytes, 0xfa),
        .dispatch_raw_reader_address = 0x70030,
        .raw_read_routine_offset = 0x60,
        .raw_read_max_sector_count = 9,
        .side_switch_track = 0x50};
}

DeuterosAtariFirstStageCopyExecutionPrefix
execute_deuteros_atari_first_stage_copy_prefix(
    const std::span<const std::uint8_t> second_stage_bytes,
    const DeuterosAtariFirstStageProfile& first_stage,
    const DeuterosAtariSecondStageProfile& second_stage) {
    constexpr std::uint32_t expected_source = 0x70000;
    constexpr std::uint32_t expected_destination = 0x1e00;
    constexpr std::size_t expected_byte_count = 0x1200;
    constexpr std::size_t expected_entry_offset = 0xc4;
    constexpr std::string_view expected_sha256 =
        "2489256511e857a4a1b20d413b4f869edaae1f4df7f62ce869e324cad40e81d7";

    // Re-parse the copied stage to bind the direct entry to the same original
    // bytes. This preserves the conditional nature of the copy: this is a
    // verified in-memory image, not a claim that the preceding XBIOS call
    // returned or that its dispatcher can now run.
    const auto parsed = parse_deuteros_atari_second_stage(second_stage_bytes);
    if (first_stage.copy_source != expected_source
        || first_stage.copy_destination != expected_destination
        || first_stage.copy_byte_count != expected_byte_count
        || second_stage_bytes.size() != expected_byte_count
        || second_stage.direct_entry_source_offset != expected_entry_offset
        || second_stage.direct_entry != expected_destination + expected_entry_offset
        || parsed.direct_entry_source_offset != second_stage.direct_entry_source_offset
        || parsed.direct_entry != second_stage.direct_entry) {
        throw std::runtime_error("Unexpected Deuteros Atari ST first-stage copy topology");
    }
    const auto source_digest = to_hex(sha256(second_stage_bytes));
    if (source_digest != expected_sha256) {
        throw std::runtime_error("Unexpected Deuteros Atari ST first-stage copy source");
    }
    std::vector<std::uint8_t> relocated_bytes(second_stage_bytes.begin(), second_stage_bytes.end());
    const auto relocated_digest = to_hex(sha256(relocated_bytes));
    if (relocated_digest != source_digest) {
        throw std::runtime_error("Deuteros Atari ST first-stage relocation lost original bytes");
    }
    return {expected_source, expected_destination, expected_byte_count, source_digest,
        std::move(relocated_bytes), relocated_digest, expected_entry_offset,
        expected_destination + static_cast<std::uint32_t>(expected_entry_offset)};
}

DeuterosAtariSecondStageEntryExecutionPrefix
execute_deuteros_atari_second_stage_entry_prefix(
    const std::span<const std::uint8_t> bytes, const DeuterosAtariSecondStageProfile& stage) {
    // The entry's MOVE SR / BCLR / BEQ decision depends on the original
    // machine status register, which is not present in this media.  Both
    // paths rejoin at +$18, however, so this is an executable local suffix
    // rather than a guessed status-register branch.  Its JMP enters copied
    // stage +$c4; do not cross that boundary because the dispatcher first
    // reads runtime RAM and then reaches a callback/XBIOS service.
    constexpr std::size_t join_offset = 0x18;
    constexpr auto join_bytes = std::to_array<std::uint8_t>({
        0x4f, 0xf9, 0x00, 0x00, 0x24, 0x78,
        0x4e, 0xf9, 0x00, 0x00, 0x1e, 0xc4,
    });
    constexpr std::string_view join_sha256 =
        "b40da514f09891a46ce07d1def675f82f77b7752f8153beb7638bdf5aea973ee";
    constexpr std::size_t dispatcher_source_offset = 0xc4;
    // Re-parse the full entry, not just the common tail: that proves the
    // SR-dependent predecessors and their shared join still belong to this
    // exact raw stage even for direct API callers that did not construct a
    // bootstrap session first.
    const auto parsed = parse_deuteros_atari_second_stage(bytes);
    if (bytes.size() != 0x1200U || stage.application_stack != 0x2478U
        || stage.direct_entry != 0x1ec4U
        || stage.direct_entry_source_offset != dispatcher_source_offset
        || parsed.application_stack != stage.application_stack
        || parsed.direct_entry != stage.direct_entry
        || parsed.direct_entry_source_offset != stage.direct_entry_source_offset) {
        throw std::runtime_error("Unexpected Deuteros Atari ST entry-prefix topology");
    }
    require_bytes(bytes, join_offset, join_bytes,
        "Unexpected Deuteros Atari ST entry-prefix instructions");
    const auto window = bytes.subspan(join_offset, join_bytes.size());
    const auto digest = to_hex(sha256(window));
    if (digest != join_sha256) {
        throw std::runtime_error("Unexpected Deuteros Atari ST entry-prefix hash");
    }
    return {join_offset, join_bytes.size(), digest, be16(window, 0), be32(window, 2),
        be16(window, 6), be32(window, 8), dispatcher_source_offset};
}

DeuterosAtariDispatchProfile parse_deuteros_atari_dispatch(
    std::span<const std::uint8_t> bytes) {
    if (bytes.size() != 0x1200U) {
        throw std::runtime_error("Unexpected Deuteros Atari ST dispatch-stage length");
    }
    constexpr std::array<std::uint8_t, 24> vectors{{
        0x00, 0x00, 0x1f, 0x1a, 0x00, 0x00, 0x1f, 0x2e,
        0x00, 0x00, 0x1f, 0x50, 0x00, 0x00, 0x1f, 0x1a,
        0x00, 0x00, 0x1f, 0x1a, 0x00, 0x00, 0x1f, 0x52}};
    require_bytes(bytes, 0xac, vectors, "Unexpected Deuteros Atari ST dispatch vectors");
    constexpr std::array<std::uint8_t, 20> state0{{
        0x22, 0x3c, 0x00, 0x01, 0x32, 0x00,
        0x20, 0x3c, 0x00, 0x00, 0x48, 0x00,
        0x24, 0x3c, 0x00, 0x00, 0x00, 0x04, 0x4e, 0x75}};
    require_bytes(bytes, 0x11a, state0, "Unexpected Deuteros Atari ST dispatch state 0");
    constexpr std::array<std::uint8_t, 34> state1{{
        0x2f, 0x3c, 0x00, 0x00, 0x26, 0x30, 0x3f, 0x3c, 0x00, 0x26,
        0x4e, 0x4e, 0x5c, 0x8f,
        0x22, 0x3c, 0x00, 0x00, 0xb0, 0x00,
        0x20, 0x3c, 0x00, 0x05, 0xe4, 0x00,
        0x24, 0x3c, 0x00, 0x00, 0x00, 0x4c, 0x4e, 0x75}};
    require_bytes(bytes, 0x12e, state1, "Unexpected Deuteros Atari ST dispatch state 1");
    // Slot 2 enters a local BRA at $1f50; slots 3/4 already point at $1f1a.
    // All three resolve statically to the same state-0 raw argument routine.
    constexpr std::array<std::uint8_t, 2> state2_alias{{0x60, 0xc8}};
    require_bytes(bytes, 0x150, state2_alias, "Unexpected Deuteros Atari ST dispatch state 2 alias");
    // Vector 5 ($1f52) is the next wholly static loader branch. It invokes
    // $70030 twice around a literal byte copy; these are raw arguments, not
    // inferred file names, sectors, or game-state meanings.
    constexpr auto state5 = std::to_array<std::uint8_t>({
        0x20, 0x3c, 0x00, 0x00, 0xb4, 0x00,
        0x22, 0x3c, 0x00, 0x00, 0xb0, 0x00,
        0x2f, 0x01, 0x2e, 0x3c, 0x00, 0x00, 0x00, 0x4c,
        0xce, 0xfc, 0x12, 0x00, 0x2f, 0x07, 0x61, 0x00, 0xfe, 0xc2,
        0x41, 0xf9, 0x00, 0x05, 0x7a, 0x00,
        0x22, 0x7c, 0x00, 0x00, 0xb0, 0x06,
        0x30, 0x3c, 0x93, 0x92, 0x53, 0x40,
        0x12, 0xd8, 0x51, 0xc8, 0xff, 0xfc,
        0x2e, 0x1f, 0x22, 0x17,
        0x06, 0x87, 0x00, 0x00, 0xb4, 0x00,
        0x06, 0x81, 0x00, 0x00, 0xb4, 0x00,
        0x20, 0x3c, 0x00, 0x04, 0xc8, 0x00,
        0x61, 0x00, 0xfe, 0x90, 0x60, 0x00, 0xff, 0x70});
    require_bytes(bytes, 0x152, state5, "Unexpected Deuteros Atari ST dispatch state 5");
    DeuterosAtariDispatchProfile result;
    for (std::size_t index = 0; index < result.vector_addresses.size(); ++index) {
        result.vector_addresses[index] = be32(bytes, 0xac + index * 4U);
    }
    result.state0_alias_addresses = {result.vector_addresses[2], result.vector_addresses[3],
        result.vector_addresses[4]};
    result.state0_destination = be32(bytes, 0x11c);
    result.state0_byte_count = be32(bytes, 0x122);
    result.state0_linear_sector = be32(bytes, 0x128);
    result.state1_destination = be32(bytes, 0x13e);
    result.state1_byte_count = be32(bytes, 0x144);
    result.state1_linear_sector = be32(bytes, 0x14a);
    result.state5_first_destination = be32(bytes, 0x15a);
    result.state5_first_byte_count = be32(bytes, 0x154);
    result.state5_first_reader_argument = be32(bytes, 0x162) * 0x1200U;
    result.state5_copy_source = be32(bytes, 0x172);
    result.state5_copy_destination = be32(bytes, 0x178);
    result.state5_copy_byte_count = static_cast<std::uint32_t>(be16(bytes, 0x17e)) + 1U;
    result.state5_second_destination = result.state5_first_destination + 0xb400U;
    result.state5_second_byte_count = be32(bytes, 0x19a);
    result.state5_second_reader_argument = result.state5_first_reader_argument + 0xb400U;
    return result;
}

DeuterosAtariState1ServiceBoundary parse_deuteros_atari_state1_service_boundary(
    const std::span<const std::uint8_t> bytes, const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariDispatchProfile& dispatch) {
    // This is the complete second direct-vector body, from its literal XBIOS
    // arguments through RTS. The trap is an explicit ABI boundary, but the
    // following D1/D0/D2 loads are static, caller-connected evidence for the
    // existing state-1 raw-load plan.
    constexpr std::size_t callee_offset = 0x12e;
    constexpr auto callee_bytes = std::to_array<std::uint8_t>({
        0x2f, 0x3c, 0x00, 0x00, 0x26, 0x30,
        0x3f, 0x3c, 0x00, 0x26,
        0x4e, 0x4e, 0x5c, 0x8f,
        0x22, 0x3c, 0x00, 0x00, 0xb0, 0x00,
        0x20, 0x3c, 0x00, 0x05, 0xe4, 0x00,
        0x24, 0x3c, 0x00, 0x00, 0x00, 0x4c, 0x4e, 0x75,
    });
    constexpr std::string_view callee_sha256 =
        "0bc76b22089d008e4ce90d63216c75acbe0786b0a06127fbd66ef0dc252949ac";
    if (bytes.size() != 0x1200U || stage.direct_entry_source_offset != 0xc4
        || dispatch.vector_addresses[1] != 0x1f2eU
        || dispatch.state1_destination != 0xb000U || dispatch.state1_byte_count != 0x5e400U
        || dispatch.state1_linear_sector != 0x4cU) {
        throw std::runtime_error("Unexpected Deuteros Atari ST state-1 service topology");
    }
    require_bytes(bytes, callee_offset, callee_bytes,
        "Unexpected Deuteros Atari ST state-1 service boundary");
    const auto window = bytes.subspan(callee_offset, callee_bytes.size());
    if (to_hex(sha256(window)) != callee_sha256) {
        throw std::runtime_error("Unexpected Deuteros Atari ST state-1 service hash");
    }
    // The literal pushes occupy six bytes. ADDQ.L #6,A7 therefore restores
    // only that caller-side stack layout; it is not evidence of a TRAP result.
    return {callee_offset, callee_bytes.size(), std::string(callee_sha256),
        be16(window, 0), be32(window, 2), be16(window, 6), be16(window, 8),
        be16(window, 10), be16(window, 12), 6U, be16(window, 14), be32(window, 16),
        be16(window, 20), be32(window, 22), be16(window, 26), be32(window, 28),
        be16(window, 32)};
}

DeuterosAtariRawLoadPlan build_deuteros_atari_state0_raw_load_plan(
    const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariDispatchProfile& dispatch) {
    constexpr std::uint32_t bytes_per_track = 0x1200;
    if (stage.raw_read_max_sector_count != 9 || dispatch.state0_byte_count != 0x4800
        || dispatch.state0_linear_sector != 4
        || dispatch.state0_byte_count % bytes_per_track != 0) {
        throw std::runtime_error("Unsupported Deuteros Atari ST state-0 raw-load plan");
    }
    const auto request_count = dispatch.state0_byte_count / bytes_per_track;
    if (request_count != 4) {
        throw std::runtime_error("Unexpected Deuteros Atari ST state-0 raw-load request count");
    }
    DeuterosAtariRawLoadPlan result{
        .destination = dispatch.state0_destination,
        .byte_count = dispatch.state0_byte_count,
        .source_linear_sector = dispatch.state0_linear_sector,
        .source_offset = static_cast<std::size_t>(dispatch.state0_linear_sector) * bytes_per_track,
    };
    for (std::size_t index = 0; index < result.requests.size(); ++index) {
        // The raw reader's `$1200` units are one nine-sector *side* span.
        // Convert each source offset back through the disk's two-sided
        // geometry rather than treating it as a cylinder number.
        const auto linear_side = dispatch.state0_linear_sector + static_cast<std::uint32_t>(index);
        result.requests[index] = {
            .track = static_cast<std::uint16_t>(linear_side / 2U),
            .side = static_cast<std::uint8_t>(linear_side % 2U),
            .first_sector = 1,
            .sector_count = stage.raw_read_max_sector_count,
            .source_offset = result.source_offset + index * bytes_per_track,
        };
    }
    return result;
}

DeuterosAtariRawRangeLoadPlan build_deuteros_atari_state1_raw_load_plan(
    const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariDispatchProfile& dispatch) {
    constexpr std::uint32_t bytes_per_side = 0x1200;
    if (dispatch.state1_destination != 0xb000
        || dispatch.state1_byte_count != 0x5e400 || dispatch.state1_linear_sector != 0x4c
        || dispatch.state1_linear_sector > UINT32_MAX / bytes_per_side) {
        throw std::runtime_error("Unsupported Deuteros Atari ST state-1 raw-load plan");
    }
    return build_raw_range_load_plan(dispatch.state1_destination, dispatch.state1_byte_count,
        dispatch.state1_linear_sector * bytes_per_side, stage);
}

DeuterosAtariState5RawLoadPlan build_deuteros_atari_state5_raw_load_plan(
    const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariDispatchProfile& dispatch) {
    if (dispatch.state5_first_destination != 0xb000 || dispatch.state5_first_byte_count != 0xb400
        || dispatch.state5_first_reader_argument != 0x55800 || dispatch.state5_copy_source != 0x57a00
        || dispatch.state5_copy_destination != 0xb006 || dispatch.state5_copy_byte_count != 0x9393
        || dispatch.state5_second_destination != 0x16400 || dispatch.state5_second_byte_count != 0x4c800
        || dispatch.state5_second_reader_argument != 0x60c00) {
        throw std::runtime_error("Unsupported Deuteros Atari ST state-5 raw-load plan");
    }
    return {
        .first_read = build_raw_range_load_plan(dispatch.state5_first_destination,
            dispatch.state5_first_byte_count, dispatch.state5_first_reader_argument, stage),
        .copy_source = dispatch.state5_copy_source,
        .copy_destination = dispatch.state5_copy_destination,
        .copy_byte_count = dispatch.state5_copy_byte_count,
        .second_read = build_raw_range_load_plan(dispatch.state5_second_destination,
            dispatch.state5_second_byte_count, dispatch.state5_second_reader_argument, stage),
    };
}

DeuterosAtariState5State1Prefix validate_deuteros_atari_state5_state1_prefix(
    const DeuterosAtariRawRangeLoadPlan& state1,
    const DeuterosAtariState5RawLoadPlan& state5,
    const std::span<const std::uint8_t> state1_bytes,
    const std::span<const std::uint8_t> state5_bytes) {
    constexpr std::string_view state1_sha256 =
        "0d5ccb3a337fcbd4d34d34b3ad24f20c3bb2edca7e7b734b8abb14f6c0a30f47";
    constexpr std::string_view prefix_sha256 =
        "ed55ad2a893a87af9f11d269faa6358420c47ed6beb1fee7a177e9beaed1e77c";
    const auto first_end = state5.first_read.source_offset + state5.first_read.byte_count;
    const auto second_end = state5.second_read.source_offset + state5.second_read.byte_count;
    if (first_end < state5.first_read.source_offset || second_end < state5.second_read.source_offset
        || state1.source_offset != state5.first_read.source_offset
        || first_end != state5.second_read.source_offset
        || second_end > state1.source_offset + state1.byte_count
        || state1_bytes.size() != state1.byte_count
        || state5_bytes.size() != state5.first_read.byte_count + state5.second_read.byte_count
        || state5_bytes.size() > state1_bytes.size()
        || !std::equal(state5_bytes.begin(), state5_bytes.end(), state1_bytes.begin())
        || to_hex(sha256(state1_bytes)) != state1_sha256
        || to_hex(sha256(state5_bytes)) != prefix_sha256) {
        throw std::runtime_error("Unexpected Deuteros Atari ST state-5/state-1 raw prefix");
    }
    return {state1.source_offset, state5_bytes.size(), std::string(prefix_sha256)};
}

DeuterosAtariState5ReturnProfile parse_deuteros_atari_state5_return(
    std::span<const std::uint8_t> bytes, const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariDispatchProfile& dispatch) {
    // Vector 5 ends immediately after its second $70030 call with BRA.W
    // -$90. For a 68000 word branch the base is the extension word itself:
    // track-2 +$1a2 / copied RAM $1fa2 therefore resolves to +$114 / $1f14.
    constexpr std::size_t branch_offset = 0x1a2;
    constexpr std::array<std::uint8_t, 4> branch_bytes{0x60, 0x00, 0xff, 0x70};
    constexpr std::size_t tail_offset = 0x114;
    constexpr std::array<std::uint8_t, 6> tail_bytes{0x30, 0x38, 0x1e, 0xaa, 0x4e, 0x75};
    constexpr std::string_view branch_sha256 =
        "4d11113ca2040c3c0d8e9fe7fc7ef2b65175cc580b8a4b81466908ae7c537896";
    constexpr std::string_view tail_sha256 =
        "506215d03a2272be5f938a8926864075fc50a79d8c2fc23f22955d290fe0c98f";
    if (bytes.size() != 0x1200U || stage.direct_entry_source_offset != 0xc4
        || stage.dispatch_state_address != 0x1eaa || dispatch.vector_addresses[5] != 0x1f52
        || !std::equal(branch_bytes.begin(), branch_bytes.end(),
            bytes.begin() + static_cast<std::ptrdiff_t>(branch_offset))
        || !std::equal(tail_bytes.begin(), tail_bytes.end(),
            bytes.begin() + static_cast<std::ptrdiff_t>(tail_offset))) {
        throw std::runtime_error("Unexpected Deuteros Atari ST vector-5 return path");
    }
    const auto branch_hash = to_hex(sha256(bytes.subspan(branch_offset, branch_bytes.size())));
    const auto tail_hash = to_hex(sha256(bytes.subspan(tail_offset, tail_bytes.size())));
    if (branch_hash != branch_sha256 || tail_hash != tail_sha256) {
        throw std::runtime_error("Unexpected Deuteros Atari ST vector-5 return path");
    }
    return {branch_offset, static_cast<std::int16_t>(be16(bytes, branch_offset + 2U)), tail_offset,
        branch_hash, tail_offset, tail_hash, be16(bytes, tail_offset + 2U), be16(bytes, tail_offset),
        be16(bytes, tail_offset + 4U)};
}

DeuterosAtariSupervisorCallbackProfile parse_deuteros_atari_supervisor_callback(
    std::span<const std::uint8_t> bytes, const DeuterosAtariSecondStageProfile& stage) {
    // At copied dispatcher +$d2, PEA $1fa6 and selector $26 form an XBIOS
    // boundary. The callback's direct stack reshaping is literal code only;
    // the ABI's caller frame and all service effects remain outside this model.
    constexpr std::size_t callsite_offset = 0xd2;
    constexpr std::array<std::uint8_t, 10> callsite_bytes{
        0x2f, 0x3c, 0x00, 0x00, 0x1f, 0xa6, 0x3f, 0x3c, 0x00, 0x26};
    constexpr std::size_t trap_offset = callsite_offset + callsite_bytes.size();
    constexpr std::array<std::uint8_t, 2> trap_bytes{0x4e, 0x4e};
    constexpr std::size_t callback_offset = 0x1a6;
    constexpr std::array<std::uint8_t, 12> callback_bytes{
        0x20, 0x17, 0x4f, 0xf9, 0x00, 0x07, 0xb0, 0x00, 0x2f, 0x00, 0x4e, 0x75};
    constexpr std::string_view callsite_sha256 =
        "11b26d5900e614547617a9c95611515e8238184756a0a18c7ff18b1ec372657b";
    constexpr std::string_view callback_sha256 =
        "1f8bdb0e61454fef9acb0dc3abcf7bfed2621828937380b415ab85d4f57ef143";
    if (bytes.size() != 0x1200U || stage.direct_entry_source_offset != 0xc4
        || !std::equal(callsite_bytes.begin(), callsite_bytes.end(),
            bytes.begin() + static_cast<std::ptrdiff_t>(callsite_offset))
        || !std::equal(trap_bytes.begin(), trap_bytes.end(),
            bytes.begin() + static_cast<std::ptrdiff_t>(trap_offset))
        || !std::equal(callback_bytes.begin(), callback_bytes.end(),
            bytes.begin() + static_cast<std::ptrdiff_t>(callback_offset))) {
        throw std::runtime_error("Unexpected Deuteros Atari ST supervisor callback boundary");
    }
    const auto callsite_hash = to_hex(sha256(bytes.subspan(callsite_offset, callsite_bytes.size())));
    const auto callback_hash = to_hex(sha256(bytes.subspan(callback_offset, callback_bytes.size())));
    if (callsite_hash != callsite_sha256 || callback_hash != callback_sha256) {
        throw std::runtime_error("Unexpected Deuteros Atari ST supervisor callback boundary");
    }
    return {callsite_offset, callsite_bytes.size(), callsite_hash, 0x1fa6, callback_offset,
        callback_bytes.size(), callback_hash, be16(bytes, callsite_offset), be16(bytes, callsite_offset + 8U),
        be16(bytes, trap_offset), be16(bytes, callback_offset), be32(bytes, callback_offset + 4U),
        be16(bytes, callback_offset + 8U), be16(bytes, callback_offset + 10U)};
}

DeuterosAtariSupervisorCallbackContinuation
parse_deuteros_atari_supervisor_callback_continuation(
    const std::span<const std::uint8_t> bytes, const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariSupervisorCallbackProfile& callback) {
    // The service boundary at +$dc is intentionally not crossed: this exact
    // following code reads $25f4 rather than a documented service result.
    // BEQ.S $08 joins at +$f2; otherwise the two local BSR.W displacements
    // resolve from their extension words to +$800 and +$1122 respectively.
    constexpr std::size_t continuation_offset = 0xde;
    constexpr std::array<std::uint8_t, 20> continuation_bytes{
        0x20, 0x38, 0x25, 0xf4, 0xb0, 0xbc, 0x00, 0x07, 0x11, 0x00,
        0x67, 0x08, 0x61, 0x00, 0x07, 0x14, 0x61, 0x00, 0x10, 0x32};
    constexpr std::string_view continuation_sha256 =
        "ed326a1d22a28ce5646b242c947c5120cb0855d6d05080e35ce398d48d459f56";
    constexpr std::size_t branch_target_offset = 0xf2;
    constexpr std::size_t first_bsr_target_offset = 0x800;
    constexpr std::size_t second_bsr_target_offset = 0x1122;
    if (bytes.size() != 0x1200U || stage.direct_entry_source_offset != 0xc4
        || callback.callsite_offset != 0xd2 || callback.callsite_bytes != 10
        || callback.trap_opcode != 0x4e4e || continuation_offset != callback.callsite_offset + 12U
        || !std::equal(continuation_bytes.begin(), continuation_bytes.end(),
            bytes.begin() + static_cast<std::ptrdiff_t>(continuation_offset))) {
        throw std::runtime_error("Unexpected Deuteros Atari ST supervisor callback continuation");
    }
    const auto digest = to_hex(sha256(bytes.subspan(continuation_offset, continuation_bytes.size())));
    if (digest != continuation_sha256) {
        throw std::runtime_error("Unexpected Deuteros Atari ST supervisor callback continuation hash");
    }
    return {continuation_offset, continuation_bytes.size(), digest,
        be16(bytes, continuation_offset), be16(bytes, continuation_offset + 2U),
        be16(bytes, continuation_offset + 4U), be32(bytes, continuation_offset + 6U),
        be16(bytes, continuation_offset + 10U), static_cast<std::int8_t>(bytes[continuation_offset + 11U]),
        branch_target_offset, be16(bytes, continuation_offset + 12U),
        static_cast<std::int16_t>(be16(bytes, continuation_offset + 14U)), first_bsr_target_offset,
        be16(bytes, continuation_offset + 16U),
        static_cast<std::int16_t>(be16(bytes, continuation_offset + 18U)), second_bsr_target_offset};
}

DeuterosAtariPostCallbackCalleeProfiles parse_deuteros_atari_post_callback_callee_profiles(
    const std::span<const std::uint8_t> bytes, const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariSupervisorCallbackContinuation& continuation) {
    // The first BSR target sets $25f4 to $71100, pushes literal arguments,
    // then reaches TRAP #14. Its cleanup and BRA.W are preserved as layout
    // only: they lie after an external boundary, so reachability is unknown.
    constexpr std::size_t first_offset = 0x800;
    constexpr auto first_bytes = std::to_array<std::uint8_t>({
        0x20, 0x3c, 0x00, 0x07, 0x11, 0x00, 0x21, 0xc0, 0x25, 0xf4,
        0x3f, 0x3c, 0xff, 0xff, 0x2f, 0x00, 0x2f, 0x00, 0x3f, 0x3c,
        0x00, 0x05, 0x20, 0x40, 0x70, 0x00, 0x3e, 0x3c, 0x1f, 0x3f,
        0x20, 0xc0, 0x51, 0xcf, 0xff, 0xfc, 0x4e, 0x4e, 0xdf, 0xfc,
        0x00, 0x00, 0x00, 0x0c, 0x60, 0x00, 0x08, 0xe8,
    });
    constexpr std::string_view first_sha256 =
        "bb662ff9f02861d2bc40c9d3d2ca97a662abc494ec20a4037807a81b22ca95a6";
    constexpr std::size_t second_offset = 0x1122;
    constexpr auto second_bytes = std::to_array<std::uint8_t>({
        0x20, 0x3c, 0x00, 0x00, 0x7e, 0x00, 0x22, 0x3c, 0x00, 0x02,
        0x00, 0x00, 0x2e, 0x3c, 0x00, 0x00, 0x90, 0x00, 0x61, 0x00,
        0xee, 0xfa,
    });
    constexpr std::string_view second_sha256 =
        "c74fb6b1e03cf6a123698e0356f3c9dbc45e637d9ce2a9479fef37eec6cbfd8c";
    if (stage.direct_entry_source_offset != 0xc4 || stage.raw_read_routine_offset != 0x60
        || continuation.first_bsr_target_offset != first_offset
        || continuation.second_bsr_target_offset != second_offset
        || continuation.first_bsr_opcode != 0x6100 || continuation.second_bsr_opcode != 0x6100) {
        throw std::runtime_error("Unexpected Deuteros Atari ST post-callback callee topology");
    }
    require_bytes(bytes, first_offset, first_bytes,
        "Unexpected Deuteros Atari ST first post-callback callee");
    require_bytes(bytes, second_offset, second_bytes,
        "Unexpected Deuteros Atari ST second post-callback callee");
    const auto first_window = bytes.subspan(first_offset, first_bytes.size());
    const auto second_window = bytes.subspan(second_offset, second_bytes.size());
    if (to_hex(sha256(first_window)) != first_sha256 || to_hex(sha256(second_window)) != second_sha256) {
        throw std::runtime_error("Unexpected Deuteros Atari ST post-callback callee hash");
    }
    // 68000 BSR/BRA word displacements are relative to their extension word.
    const auto first_branch_target = relative_target(first_offset + 46U,
        static_cast<std::int16_t>(be16(first_window, 46)), bytes.size(),
        "Deuteros Atari ST first post-callback branch leaves stage");
    const auto second_bsr_target = relative_target(second_offset + 20U,
        static_cast<std::int16_t>(be16(second_window, 20)), bytes.size(),
        "Deuteros Atari ST second post-callback branch leaves stage");
    // The BSR reaches the local range wrapper at +$30; that wrapper in turn
    // calls the known XBIOS-facing routine at +$60. Do not collapse the two
    // boundaries or presume either one returns.
    if (first_branch_target != 0x1116U || second_bsr_target != 0x30U) {
        throw std::runtime_error("Unexpected Deuteros Atari ST post-callback callee target");
    }
    return {first_offset, first_bytes.size(), std::string(first_sha256), be32(first_window, 2),
        be16(first_window, 8), be16(first_window, 20), 36, be16(first_window, 36),
        be16(first_window, 38), be32(first_window, 40), 44,
        static_cast<std::int16_t>(be16(first_window, 46)), first_branch_target,
        second_offset, second_bytes.size(), std::string(second_sha256), be32(second_window, 2),
        be32(second_window, 8), be32(second_window, 14), be16(second_window, 18),
        static_cast<std::int16_t>(be16(second_window, 20)), second_bsr_target};
}

DeuterosAtariFirstCalleeContinuation parse_deuteros_atari_first_callee_continuation(
    const std::span<const std::uint8_t> bytes, const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariPostCallbackCalleeProfiles& callees) {
    // The first callee's XBIOS call at +$824 is an intentionally opaque
    // boundary. Its following BRA.W targets +$1116, where these literal bytes
    // lay out an immediate-to-RAM store and RTS. Do not assert that the trap
    // returns, that this target is reached, or that either RAM value has game
    // semantics.
    constexpr std::size_t continuation_offset = 0x1116;
    constexpr auto continuation_bytes = std::to_array<std::uint8_t>({
        0x20, 0x3c, 0x00, 0x00, 0xb0, 0x00, 0x21, 0xc0, 0x25, 0xf0, 0x4e, 0x75,
    });
    constexpr std::string_view continuation_sha256 =
        "8778c08ae16a5f66009dda8d60a0dacba267cca4d29211a11fd2e30c40a7796b";
    if (stage.direct_entry_source_offset != 0xc4 || callees.first_callee_offset != 0x800
        || callees.first_callee_post_trap_branch_offset != 44
        || callees.first_callee_post_trap_branch_target_offset != continuation_offset
        || callees.first_callee_trap_opcode != 0x4e4e) {
        throw std::runtime_error("Unexpected Deuteros Atari ST first-callee continuation topology");
    }
    require_bytes(bytes, continuation_offset, continuation_bytes,
        "Unexpected Deuteros Atari ST first-callee continuation");
    const auto window = bytes.subspan(continuation_offset, continuation_bytes.size());
    if (to_hex(sha256(window)) != continuation_sha256) {
        throw std::runtime_error("Unexpected Deuteros Atari ST first-callee continuation hash");
    }
    return {continuation_offset, continuation_bytes.size(), std::string(continuation_sha256),
        be16(window, 0), be32(window, 2), be16(window, 6), be16(window, 8), be16(window, 10)};
}

DeuterosAtariSecondCalleeContinuation parse_deuteros_atari_second_callee_continuation(
    const std::span<const std::uint8_t> bytes, const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariPostCallbackCalleeProfiles& callees) {
    // The second callee's BSR.W at +$1134 enters the known local wrapper at
    // +$30. Its return continuation begins at +$1138, but that return is not
    // assumed: the wrapper reaches the XBIOS-facing raw reader at +$60.
    constexpr std::size_t continuation_offset = 0x1138;
    constexpr auto continuation_bytes = std::to_array<std::uint8_t>({
        0x43, 0xf9, 0x00, 0x02, 0x00, 0x00, 0x2f, 0x09,
        0x3f, 0x3c, 0x00, 0x06, 0x4e, 0x4e, 0x5c, 0x8f,
        0x41, 0xf9, 0x00, 0x02, 0x00, 0x20, 0x22, 0x78,
        0x25, 0xf4, 0x3e, 0x3c, 0x1f, 0x3f, 0x22, 0xd8,
        0x51, 0xcf, 0xff, 0xfc, 0x4e, 0x75,
    });
    constexpr std::string_view continuation_sha256 =
        "5b1480495df8defe3e1264dd083ec1c91134c01e56d3d94e060c583ee9b54a89";
    if (stage.raw_read_routine_offset != 0x60 || callees.second_callee_offset != 0x1122
        || callees.second_callee_bsr_target_offset != 0x30
        || callees.second_callee_prefix_byte_count != 22) {
        throw std::runtime_error("Unexpected Deuteros Atari ST second-callee continuation topology");
    }
    require_bytes(bytes, continuation_offset, continuation_bytes,
        "Unexpected Deuteros Atari ST second-callee continuation");
    const auto window = bytes.subspan(continuation_offset, continuation_bytes.size());
    if (to_hex(sha256(window)) != continuation_sha256) {
        throw std::runtime_error("Unexpected Deuteros Atari ST second-callee continuation hash");
    }
    // DBF uses the extension-word address as its displacement base, so its
    // -4 target is the preceding MOVE.L (A0)+,(A1)+ at +$1156.
    const auto copy_loop_target = relative_target(continuation_offset + 34U,
        static_cast<std::int16_t>(be16(window, 34)), bytes.size(),
        "Deuteros Atari ST copy loop leaves stage");
    if (copy_loop_target != 0x1156U) {
        throw std::runtime_error("Unexpected Deuteros Atari ST second-callee copy loop");
    }
    return {continuation_offset, continuation_bytes.size(), std::string(continuation_sha256),
        be32(window, 2), be16(window, 10), continuation_offset + 12U, be16(window, 12),
        be16(window, 14), 6, be32(window, 18), be16(window, 24), be16(window, 28),
        be16(window, 30), be16(window, 32), static_cast<std::int16_t>(be16(window, 34)),
        copy_loop_target, be16(window, 36)};
}

DeuterosAtariRawReaderWrapperProfile parse_deuteros_atari_raw_reader_wrapper(
    const std::span<const std::uint8_t> bytes, const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariPostCallbackCalleeProfiles& callees) {
    // This is the shared static wrapper at +$30. It calls the raw reader at
    // +$60 before sampling the word written at $1e28, then uses literal
    // conditional branches and a loop back to its save/call sequence. The
    // word's provenance/value and every service return remain deliberately
    // outside the recovered model.
    constexpr std::size_t wrapper_offset = 0x30;
    constexpr auto wrapper_bytes = std::to_array<std::uint8_t>({
        0x8e, 0xfc, 0x12, 0x00, 0x2f, 0x00, 0x2f, 0x01,
        0x2f, 0x07, 0x61, 0x00, 0x00, 0x24, 0x2e, 0x1f,
        0x22, 0x1f, 0x20, 0x1f, 0x4a, 0x78, 0x1e, 0x28,
        0x66, 0xe0, 0x04, 0x80, 0x00, 0x00, 0x12, 0x00,
        0x67, 0x0c, 0x65, 0x0a, 0x06, 0x81, 0x00, 0x00,
        0x12, 0x00, 0x52, 0x87, 0x60, 0xd6, 0x60, 0xca,
    });
    constexpr std::string_view wrapper_sha256 =
        "132ce2473e3764453bba01308e1f5044dc748bbea8b01975b67a259aa57cea7e";
    constexpr std::size_t raw_reader_offset = 0x60;
    constexpr std::size_t return_helper_offset = 0x2a;
    constexpr std::array<std::uint8_t, 6> return_helper_bytes{
        0x3e, 0x38, 0x1e, 0x28, 0x4e, 0x75};
    if (stage.raw_read_routine_offset != raw_reader_offset
        || callees.second_callee_bsr_target_offset != wrapper_offset) {
        throw std::runtime_error("Unexpected Deuteros Atari ST raw-reader wrapper topology");
    }
    require_bytes(bytes, wrapper_offset, wrapper_bytes,
        "Unexpected Deuteros Atari ST raw-reader wrapper");
    require_bytes(bytes, return_helper_offset, return_helper_bytes,
        "Unexpected Deuteros Atari ST raw-reader wrapper return helper");
    const auto window = bytes.subspan(wrapper_offset, wrapper_bytes.size());
    const auto digest = to_hex(sha256(window));
    if (digest != wrapper_sha256) {
        throw std::runtime_error("Unexpected Deuteros Atari ST raw-reader wrapper hash");
    }
    // Word branches use their extension-word address; byte branches use the
    // address immediately after their opcode/displacement pair.
    const auto raw_reader_target = relative_target(wrapper_offset + 12U,
        static_cast<std::int16_t>(be16(window, 12)), bytes.size(), "Deuteros Atari ST raw-reader branch leaves stage");
    const auto nonzero_target = relative_target(wrapper_offset + 26U,
        static_cast<std::int8_t>(window[25]), bytes.size(), "Deuteros Atari ST nonzero branch leaves stage");
    const auto first_terminal_target = relative_target(wrapper_offset + 34U,
        static_cast<std::int8_t>(window[33]), bytes.size(), "Deuteros Atari ST terminal branch leaves stage");
    const auto second_terminal_target = relative_target(wrapper_offset + 36U,
        static_cast<std::int8_t>(window[35]), bytes.size(), "Deuteros Atari ST terminal branch leaves stage");
    const auto loop_target = relative_target(wrapper_offset + 46U,
        static_cast<std::int8_t>(window[45]), bytes.size(), "Deuteros Atari ST loop branch leaves stage");
    const auto return_helper_target = relative_target(wrapper_offset + 48U,
        static_cast<std::int8_t>(window[47]), bytes.size(), "Deuteros Atari ST return branch leaves stage");
    if (raw_reader_target != raw_reader_offset || nonzero_target != return_helper_offset
        || first_terminal_target != wrapper_offset + 46U
        || second_terminal_target != wrapper_offset + 46U
        || loop_target != wrapper_offset + 4U || return_helper_target != return_helper_offset) {
        throw std::runtime_error("Unexpected Deuteros Atari ST raw-reader wrapper branches");
    }
    return {wrapper_offset, wrapper_bytes.size(), std::string(wrapper_sha256),
        be16(window, 0), be16(window, 2), be16(window, 10),
        static_cast<std::int16_t>(be16(window, 12)), raw_reader_target,
        be16(window, 20), be16(window, 22), be16(window, 24),
        static_cast<std::int8_t>(window[25]), nonzero_target, be16(window, 26), be32(window, 28),
        be16(window, 32), static_cast<std::int8_t>(window[33]), first_terminal_target,
        be16(window, 34), static_cast<std::int8_t>(window[35]), second_terminal_target,
        be16(window, 36), be32(window, 38), be16(window, 42), be16(window, 44),
        static_cast<std::int8_t>(window[45]), loop_target, be16(window, 46),
        static_cast<std::int8_t>(window[47]), return_helper_target, raw_reader_offset};
}

DeuterosAtariRawReaderCallLayout parse_deuteros_atari_raw_reader_call_layout(
    const std::span<const std::uint8_t> bytes, const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariRawReaderWrapperProfile& wrapper) {
    // This is the wrapper's direct BSR target.  Keep the external ABI call as
    // a hard boundary: the byte profile records only the original setup and
    // following store/RTS encoding, never a service result or disk transfer.
    constexpr std::size_t routine_offset = 0x60;
    constexpr auto routine_bytes = std::to_array<std::uint8_t>({
        0x74, 0x09, 0xb0, 0xbc, 0x00, 0x00, 0x12, 0x00,
        0x64, 0x08, 0xe0, 0x48, 0xe2, 0x48, 0x52, 0x40,
        0x34, 0x00, 0x28, 0x07, 0x76, 0x00, 0xb8, 0x7c,
        0x00, 0x50, 0x65, 0x06, 0x76, 0x01, 0x04, 0x44,
        0x00, 0x50, 0x3f, 0x02, 0x3f, 0x03, 0x3f, 0x04,
        0x3f, 0x3c, 0x00, 0x01, 0x3f, 0x3c, 0x00, 0x00,
        0x2f, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x2f, 0x01,
        0x3f, 0x3c, 0x00, 0x08, 0x4e, 0x4e, 0xdf, 0xfc,
        0x00, 0x00, 0x00, 0x14, 0x31, 0xc0, 0x1e, 0x28,
        0x4e, 0x75,
    });
    constexpr std::string_view routine_sha256 =
        "a5bec9d04daa8ce600add594f6325030acd2ad8535910dee62497da90d572c90";
    constexpr std::size_t abi_call_relative_offset = 60;
    constexpr std::size_t post_call_relative_offset = 68;
    if (bytes.size() != 0x1200U || stage.raw_read_routine_offset != routine_offset
        || wrapper.raw_reader_entry_offset != routine_offset
        || wrapper.raw_reader_bsr_target_offset != routine_offset) {
        throw std::runtime_error("Unexpected Deuteros Atari ST raw-reader call topology");
    }
    require_bytes(bytes, routine_offset, routine_bytes,
        "Unexpected Deuteros Atari ST raw-reader call layout");
    const auto window = bytes.subspan(routine_offset, routine_bytes.size());
    const auto digest = to_hex(sha256(window));
    if (digest != routine_sha256) {
        throw std::runtime_error("Unexpected Deuteros Atari ST raw-reader call hash");
    }
    // Both byte branches remain simple layout facts.  The first skips the
    // two shifts/increment; the second skips the alternate-side adjustment.
    const auto count_branch_target = relative_target(routine_offset + 10U,
        static_cast<std::int8_t>(window[9]), bytes.size(), "Deuteros Atari ST count branch leaves stage");
    const auto side_branch_target = relative_target(routine_offset + 28U,
        static_cast<std::int8_t>(window[27]), bytes.size(), "Deuteros Atari ST side branch leaves stage");
    if (count_branch_target != routine_offset + 18U
        || side_branch_target != routine_offset + 34U) {
        throw std::runtime_error("Unexpected Deuteros Atari ST raw-reader call branches");
    }
    return {routine_offset, routine_bytes.size(), digest,
        be16(window, 0), window[1], be16(window, 2), be32(window, 4),
        be16(window, 8), static_cast<std::int8_t>(window[9]), count_branch_target,
        be16(window, 10), be16(window, 12), be16(window, 14), be16(window, 16),
        be16(window, 18), be16(window, 22), be16(window, 24), be16(window, 26),
        static_cast<std::int8_t>(window[27]), side_branch_target, be16(window, 28),
        be16(window, 30), be16(window, 32), routine_offset + abi_call_relative_offset,
        be16(window, 58), be16(window, abi_call_relative_offset),
        be16(window, 62), be32(window, 64), be16(window, post_call_relative_offset),
        be16(window, post_call_relative_offset + 2U), be16(window, post_call_relative_offset + 4U)};
}

DeuterosAtariDirectVectorCalleeProfiles parse_deuteros_atari_direct_vector_callees(
    const std::span<const std::uint8_t> bytes, const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariDispatchProfile& dispatch) {
    // The copied dispatcher reaches these bodies only through JSR (A1), so
    // their relationship is a table fact rather than a reachability claim.
    // Each byte interval is complete through its original RTS or BRA.W. No
    // register value, raw read, branch outcome, or game-state purpose is
    // inferred from the literal code.
    constexpr std::size_t table_offset = 0xac;
    constexpr std::array<std::size_t, 3> slots{{0, 1, 5}};
    constexpr std::array<std::size_t, 3> offsets{{0x11a, 0x12e, 0x152}};
    constexpr std::array<std::size_t, 3> counts{{20, 34, 84}};
    constexpr std::array<std::string_view, 3> hashes{{
        "04c8eba86a6259f8d0b175fa18792cc64263863db51e76f9de839eec5c79ce0f",
        "0bc76b22089d008e4ce90d63216c75acbe0786b0a06127fbd66ef0dc252949ac",
        "eaee587850078d67a72dcf0da4b45e672c89a1352b040db580bedc0ba3b20e97",
    }};
    constexpr std::array<std::uint16_t, 3> first_words{{0x223c, 0x2f3c, 0x203c}};
    constexpr std::array<std::uint16_t, 3> final_words{{0x4e75, 0x4e75, 0xff70}};
    constexpr std::size_t alias_offset = 0x150;
    if (bytes.size() != 0x1200U || stage.direct_entry_source_offset != 0xc4
        || dispatch.vector_addresses
            != std::array<std::uint32_t, 6>{{0x1f1a, 0x1f2e, 0x1f50, 0x1f1a, 0x1f1a, 0x1f52}}) {
        throw std::runtime_error("Unexpected Deuteros Atari ST direct-vector callee topology");
    }
    DeuterosAtariDirectVectorCalleeProfiles result;
    result.vector_table_offset = table_offset;
    for (std::size_t index = 0; index < slots.size(); ++index) {
        const auto window = bytes.subspan(offsets[index], counts[index]);
        const auto digest = to_hex(sha256(window));
        if (digest != hashes[index] || be16(window, 0) != first_words[index]
            || be16(window, counts[index] - 2U) != final_words[index]) {
            throw std::runtime_error("Unexpected Deuteros Atari ST direct-vector callee bytes");
        }
        const auto runtime_address = be32(bytes, table_offset + slots[index] * 4U);
        if (runtime_address != 0x1e00U + offsets[index]) {
            throw std::runtime_error("Unexpected Deuteros Atari ST direct-vector callee address");
        }
        result.distinct_callees[index] = {slots[index], runtime_address, offsets[index], counts[index],
            digest, be16(window, 0), be16(window, counts[index] - 2U)};
    }
    constexpr std::array<std::uint8_t, 2> alias_bytes{{0x60, 0xc8}};
    require_bytes(bytes, alias_offset, alias_bytes,
        "Unexpected Deuteros Atari ST direct-vector alias branch");
    const auto alias_target = relative_target(alias_offset + 2U,
        static_cast<std::int8_t>(bytes[alias_offset + 1U]), bytes.size(),
        "Deuteros Atari ST alias branch leaves stage");
    if (alias_target != offsets[0]) {
        throw std::runtime_error("Unexpected Deuteros Atari ST direct-vector alias target");
    }
    result.alias_branch_offset = alias_offset;
    result.alias_branch_opcode = be16(bytes, alias_offset);
    result.alias_branch_displacement = static_cast<std::int8_t>(bytes[alias_offset + 1U]);
    result.alias_branch_target_offset = alias_target;
    return result;
}

DeuterosAtariDirectVectorTransferLoopProfile parse_deuteros_atari_direct_vector_transfer_loop(
    const std::span<const std::uint8_t> bytes, const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariDirectVectorCalleeProfiles& callees) {
    // This interval is nested within the byte-verified third distinct table
    // body.  Its pointer literals and loop encoding are useful preservation
    // facts, while all dynamic questions stay at the explicit table/call and
    // raw-media boundaries.
    constexpr std::size_t loop_offset = 0x170;
    constexpr auto loop_bytes = std::to_array<std::uint8_t>({
        0x41, 0xf9, 0x00, 0x05, 0x7a, 0x00,
        0x22, 0x7c, 0x00, 0x00, 0xb0, 0x06,
        0x30, 0x3c, 0x93, 0x92,
        0x53, 0x40, 0x12, 0xd8, 0x51, 0xc8, 0xff, 0xfc,
        0x2e, 0x1f, 0x22, 0x17,
    });
    constexpr std::string_view loop_sha256 =
        "92cb6cf8a41c55df8459a9608c9626ff7cc831cceb69dd2b5531ac766b111552";
    constexpr std::size_t dbf_relative_offset = 20;
    constexpr std::size_t transfer_relative_offset = 18;
    if (bytes.size() != 0x1200U || stage.direct_entry_source_offset != 0xc4
        || callees.distinct_callees[2].vector_slot != 5
        || callees.distinct_callees[2].stage_offset != 0x152
        || callees.distinct_callees[2].byte_count != 84
        || callees.distinct_callees[2].sha256
            != "eaee587850078d67a72dcf0da4b45e672c89a1352b040db580bedc0ba3b20e97") {
        throw std::runtime_error("Unexpected Deuteros Atari ST direct-vector transfer topology");
    }
    require_bytes(bytes, loop_offset, loop_bytes,
        "Unexpected Deuteros Atari ST direct-vector transfer loop");
    const auto window = bytes.subspan(loop_offset, loop_bytes.size());
    const auto digest = to_hex(sha256(window));
    if (digest != loop_sha256) {
        throw std::runtime_error("Unexpected Deuteros Atari ST direct-vector transfer hash");
    }
    // On this 68000 DBF encoding the signed displacement is relative to the
    // extension word, so -4 returns to the preceding byte transfer.
    const auto target = relative_target(loop_offset + dbf_relative_offset + 2U,
        static_cast<std::int16_t>(be16(window, dbf_relative_offset + 2U)), bytes.size(),
        "Deuteros Atari ST transfer loop leaves stage");
    if (target != loop_offset + 18U) {
        throw std::runtime_error("Unexpected Deuteros Atari ST direct-vector transfer backedge");
    }
    return {loop_offset, loop_bytes.size(), digest, be16(window, 0), be32(window, 2),
        be16(window, 6), be32(window, 8), be16(window, 12), be16(window, 14),
        be16(window, transfer_relative_offset), be16(window, dbf_relative_offset),
        static_cast<std::int16_t>(be16(window, dbf_relative_offset + 2U)), target};
}

DeuterosAtariDirectVectorTransferTailProfile parse_deuteros_atari_direct_vector_transfer_tail(
    const std::span<const std::uint8_t> bytes, const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariDirectVectorCalleeProfiles& callees,
    const DeuterosAtariDirectVectorTransferLoopProfile& loop,
    const DeuterosAtariRawReaderWrapperProfile& wrapper,
    const DeuterosAtariState5ReturnProfile& state5_return) {
    // This is the remaining suffix of the complete third direct-vector body.
    // It joins already independent byte boundaries (the local range wrapper
    // and the vector-5 dispatcher return) without turning either static
    // branch into an execution or state-selection claim.
    constexpr std::size_t tail_offset = 0x18c;
    constexpr auto tail_bytes = std::to_array<std::uint8_t>({
        0x06, 0x87, 0x00, 0x00, 0xb4, 0x00,
        0x06, 0x81, 0x00, 0x00, 0xb4, 0x00,
        0x20, 0x3c, 0x00, 0x04, 0xc8, 0x00,
        0x61, 0x00, 0xfe, 0x90,
        0x60, 0x00, 0xff, 0x70,
    });
    constexpr std::string_view tail_sha256 =
        "45ac9d176b63fa93e16475543939d2f16b4e98cc839b44d2ce2ba9358e978083";
    constexpr std::size_t bsr_relative_offset = 18;
    constexpr std::size_t bra_relative_offset = 22;
    if (bytes.size() != 0x1200U || stage.direct_entry_source_offset != 0xc4
        || callees.distinct_callees[2].stage_offset != 0x152
        || callees.distinct_callees[2].byte_count != 84
        || loop.loop_block_offset != 0x170 || loop.loop_block_byte_count != 28
        || loop.loop_block_sha256
            != "92cb6cf8a41c55df8459a9608c9626ff7cc831cceb69dd2b5531ac766b111552"
        || wrapper.wrapper_offset != 0x30 || state5_return.branch_offset != 0x1a2
        || state5_return.branch_target_offset != 0x114) {
        throw std::runtime_error("Unexpected Deuteros Atari ST direct-vector transfer tail topology");
    }
    require_bytes(bytes, tail_offset, tail_bytes,
        "Unexpected Deuteros Atari ST direct-vector transfer tail");
    const auto window = bytes.subspan(tail_offset, tail_bytes.size());
    const auto digest = to_hex(sha256(window));
    if (digest != tail_sha256) {
        throw std::runtime_error("Unexpected Deuteros Atari ST direct-vector transfer tail hash");
    }
    const auto bsr_target = relative_target(tail_offset + bsr_relative_offset + 2U,
        static_cast<std::int16_t>(be16(window, bsr_relative_offset + 2U)), bytes.size(),
        "Deuteros Atari ST transfer call leaves stage");
    const auto bra_target = relative_target(tail_offset + bra_relative_offset + 2U,
        static_cast<std::int16_t>(be16(window, bra_relative_offset + 2U)), bytes.size(),
        "Deuteros Atari ST transfer branch leaves stage");
    if (bsr_target != wrapper.wrapper_offset || bra_target != state5_return.branch_target_offset) {
        throw std::runtime_error("Unexpected Deuteros Atari ST direct-vector transfer tail branches");
    }
    return {tail_offset, tail_bytes.size(), digest,
        be16(window, 0), be32(window, 2), be16(window, 6), be32(window, 8),
        be16(window, 12), be32(window, 14), be16(window, bsr_relative_offset),
        static_cast<std::int16_t>(be16(window, bsr_relative_offset + 2U)), bsr_target,
        be16(window, bra_relative_offset),
        static_cast<std::int16_t>(be16(window, bra_relative_offset + 2U)), bra_target};
}

DeuterosAtariStateSelectionLayout parse_deuteros_atari_state_selection_layout(
    const std::span<const std::uint8_t> bytes, const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariDispatchProfile& dispatch) {
    // The first pair of copied-dispatcher operations has already happened
    // before the XBIOS callback boundary: it copies a longword's low word to
    // the dispatch word. The lookup block is later in the same copied code,
    // after that boundary; retain it as a static layout without claiming that
    // the service returns or that its JSR selects a specific table vector.
    constexpr std::size_t input_capture_offset = 0xc4;
    constexpr auto input_capture_bytes = std::to_array<std::uint8_t>({
        0x20, 0x38, 0x25, 0xfc, 0x31, 0xc0, 0x1e, 0xaa, 0x4f, 0xf9, 0x00, 0x00,
    });
    constexpr std::string_view input_capture_sha256 =
        "03cf620d981a775fd1adabe55deea940e08760e3e49c62cd0643c22b5aa08082";
    constexpr std::size_t table_lookup_offset = 0xf2;
    constexpr auto table_lookup_bytes = std::to_array<std::uint8_t>({
        0x4f, 0xf9, 0x00, 0x00, 0x24, 0x78, 0x43, 0xf8, 0x1e, 0xac,
        0x30, 0x38, 0x1e, 0xaa, 0xe5, 0x48, 0x22, 0x71, 0x00, 0x00,
        0x4e, 0x91,
    });
    constexpr std::string_view table_lookup_sha256 =
        "8e8551a51a7b989e6d2b7d1535819dea658a4e3e64562737755125c13c8f0d3c";
    if (bytes.size() != 0x1200U || stage.direct_entry_source_offset != input_capture_offset
        || stage.dispatch_state_address != 0x1eaa || stage.dispatch_table_address != 0x1eac
        || dispatch.vector_addresses
            != std::array<std::uint32_t, 6>{{0x1f1a, 0x1f2e, 0x1f50, 0x1f1a, 0x1f1a, 0x1f52}}) {
        throw std::runtime_error("Unexpected Deuteros Atari ST state-selection topology");
    }
    require_bytes(bytes, input_capture_offset, input_capture_bytes,
        "Unexpected Deuteros Atari ST state-selection input capture");
    require_bytes(bytes, table_lookup_offset, table_lookup_bytes,
        "Unexpected Deuteros Atari ST state-selection table lookup");
    const auto input_window = bytes.subspan(input_capture_offset, input_capture_bytes.size());
    const auto lookup_window = bytes.subspan(table_lookup_offset, table_lookup_bytes.size());
    if (to_hex(sha256(input_window)) != input_capture_sha256
        || to_hex(sha256(lookup_window)) != table_lookup_sha256) {
        throw std::runtime_error("Unexpected Deuteros Atari ST state-selection layout hash");
    }
    return {input_capture_offset, input_capture_bytes.size(), std::string(input_capture_sha256),
        be16(input_window, 0), be16(input_window, 2), be16(input_window, 4),
        be16(input_window, 6), table_lookup_offset, table_lookup_bytes.size(),
        std::string(table_lookup_sha256), be16(lookup_window, 6), be16(lookup_window, 8),
        be16(lookup_window, 10), be16(lookup_window, 14), be16(lookup_window, 16),
        be16(lookup_window, 18), be16(lookup_window, 20)};
}

DeuterosAtariStateSelectionContinuation parse_deuteros_atari_state_selection_continuation(
    const std::span<const std::uint8_t> bytes, const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariStateSelectionLayout& layout,
    const DeuterosAtariRawReaderWrapperProfile& wrapper) {
    // This starts directly after JSR (A1), which remains an indirect-call
    // boundary. The original bytes preserve D1, advance D4 by one side span,
    // transfer D2 to D7, and BSR to the local range wrapper. They do not show
    // that the selected routine returns, what either result register means, or
    // that either later call executes.
    constexpr std::size_t continuation_offset = 0x108;
    constexpr auto continuation_bytes = std::to_array<std::uint8_t>({
        0x2f, 0x01, 0xc4, 0xfc, 0x12, 0x00, 0x2e, 0x02, 0x61, 0x00,
        0xff, 0x1e, 0x30, 0x38, 0x1e, 0xaa, 0x4e, 0x75,
    });
    constexpr std::string_view continuation_sha256 =
        "e9ae4bd51bb06c6cb57ac7f26e81497995f7639f99a12e2a149194a39589e16c";
    constexpr std::size_t indirect_call_offset = 0x106;
    constexpr std::size_t wrapper_bsr_offset = 0x110;
    constexpr std::size_t wrapper_offset = 0x30;
    if (bytes.size() != 0x1200U || stage.direct_entry_source_offset != 0xc4
        || layout.table_lookup_offset != 0xf2 || layout.indirect_call_opcode != 0x4e91
        || wrapper.wrapper_offset != wrapper_offset || wrapper.raw_reader_bsr_target_offset != 0x60) {
        throw std::runtime_error("Unexpected Deuteros Atari ST state-selection continuation topology");
    }
    require_bytes(bytes, indirect_call_offset, std::array<std::uint8_t, 2>{0x4e, 0x91},
        "Unexpected Deuteros Atari ST state-selection indirect call");
    require_bytes(bytes, continuation_offset, continuation_bytes,
        "Unexpected Deuteros Atari ST state-selection continuation");
    const auto window = bytes.subspan(continuation_offset, continuation_bytes.size());
    const auto digest = to_hex(sha256(window));
    if (digest != continuation_sha256) {
        throw std::runtime_error("Unexpected Deuteros Atari ST state-selection continuation hash");
    }
    const auto wrapper_target = relative_target(wrapper_bsr_offset + 2U,
        static_cast<std::int16_t>(be16(bytes, wrapper_bsr_offset + 2U)), bytes.size(),
        "Deuteros Atari ST state wrapper branch leaves stage");
    if (wrapper_target != wrapper_offset) {
        throw std::runtime_error("Unexpected Deuteros Atari ST state-selection wrapper branch");
    }
    return {continuation_offset, continuation_bytes.size(), digest, be16(bytes, indirect_call_offset),
        be16(window, 0), be16(window, 2), be16(window, 4), be16(window, 6), be16(window, 8),
        static_cast<std::int16_t>(be16(window, 10)), wrapper_target, be16(window, 12),
        be16(window, 14), be16(window, 16)};
}

DeuterosAtariState0DuplicateStagePrefix
parse_deuteros_atari_state0_duplicate_stage_prefix(const std::span<const std::uint8_t> state0_bytes,
    const std::span<const std::uint8_t> second_stage_bytes) {
    constexpr std::size_t stage_size = 0x1200;
    if (state0_bytes.size() < stage_size || second_stage_bytes.size() != stage_size
        || !std::equal(second_stage_bytes.begin(), second_stage_bytes.end(), state0_bytes.begin())) {
        throw std::runtime_error("Deuteros Atari ST state-0 prefix is not the verified second stage");
    }
    // Reuse the independently bounded profiles so identity is not mistaken
    // for generic code-like bytes. The result still says nothing about a
    // return path that would execute the duplicate at its new load address.
    const auto stage = parse_deuteros_atari_second_stage(second_stage_bytes);
    static_cast<void>(parse_deuteros_atari_dispatch(second_stage_bytes));
    return {stage_size, to_hex(sha256(second_stage_bytes)), 0, stage.direct_entry_source_offset};
}

DeuterosAtariState1SkippedAsciiBlock
parse_deuteros_atari_state1_skipped_ascii_block(
    const std::span<const std::uint8_t> state1_bytes,
    const DeuterosAtariRawRangeLoadPlan& state1) {
    constexpr std::size_t branch_relative_offset = 0x48000;
    constexpr std::array<std::uint8_t, 4> branch{{0x60, 0x00, 0x09, 0xc2}};
    constexpr std::size_t ascii_relative_offset = 0x4800a;
    constexpr std::size_t ascii_byte_count = 0x438;
    constexpr std::size_t presentation_marker_offset = 0;
    constexpr std::size_t marker_byte_count = 55;
    constexpr std::array<std::size_t, 2> game_name_marker_offsets{{0x3c, 0x78}};
    constexpr auto ascii_sha256 =
        "8dd46e7c760a38d07273b18a4cbd3c03eb44a6b57c8c401580dd47fa4646484e";
    constexpr auto presentation_marker_sha256 =
        "785ebbc9d234032ee38c1cb5444ac1b5d46db21151ffad08d7b1898d6e6ce52a";
    constexpr auto game_name_marker_sha256 =
        "f0eb99896cde59d36a075e624092cbf02de3ce0d201ca3c5050c13f9c65720dc";
    if (state1.source_offset != 0x55800 || state1.destination != 0xb000
        || state1.byte_count != 0x5e400 || state1_bytes.size() != state1.byte_count
        || ascii_relative_offset > state1_bytes.size()
        || ascii_byte_count > state1_bytes.size() - ascii_relative_offset) {
        throw std::runtime_error("Unexpected Deuteros Atari ST state-1 skipped ASCII placement");
    }
    require_bytes(state1_bytes, branch_relative_offset, branch,
        "Unexpected Deuteros Atari ST state-1 skip branch");
    const auto ascii = state1_bytes.subspan(ascii_relative_offset, ascii_byte_count);
    if (to_hex(sha256(ascii)) != ascii_sha256
        || to_hex(sha256(ascii.subspan(presentation_marker_offset, marker_byte_count)))
            != presentation_marker_sha256
        || to_hex(sha256(ascii.subspan(game_name_marker_offsets[0], marker_byte_count)))
            != game_name_marker_sha256
        || to_hex(sha256(ascii.subspan(game_name_marker_offsets[1], marker_byte_count)))
            != game_name_marker_sha256) {
        throw std::runtime_error("Unexpected Deuteros Atari ST state-1 skipped ASCII block");
    }
    return {branch_relative_offset, 0x09c2, ascii_relative_offset, ascii_byte_count, 18,
        presentation_marker_offset, marker_byte_count, std::string(presentation_marker_sha256),
        game_name_marker_offsets, marker_byte_count, std::string(game_name_marker_sha256),
        ascii_sha256};
}

DeuterosAtariState1DisplayServiceBoundary
parse_deuteros_atari_state1_display_service_boundary(
    const std::span<const std::uint8_t> state1_bytes,
    const DeuterosAtariRawRangeLoadPlan& state1) {
    // The observed emulator PC was relocated, but this parser deliberately
    // starts from the physical state-1 interval and retains no guessed RAM
    // relocation. A 68000 BRA.W displacement is relative to the word after
    // its extension, hence $48000 + 4 + $09c2 = $489c6.
    constexpr std::size_t branch_relative_offset = 0x48000;
    constexpr auto branch = std::to_array<std::uint8_t>({0x60, 0x00, 0x09, 0xc2});
    constexpr std::int16_t branch_displacement = 0x09c2;
    constexpr std::size_t branch_target_relative_offset = 0x489c6;
    constexpr auto branch_sha256 =
        "6321ea5a7fcf59fb3f07d02b6bd333a62b9c897be5a67b233a83b3c935a38bf6";
    constexpr auto service_setup = std::to_array<std::uint8_t>({
        0x2f, 0x3c, 0xff, 0xff, 0xff, 0xff, // move.l #-1,-(a7)
        0x2f, 0x17,                         // move.l (a7),-(a7)
        0x3f, 0x3c, 0x00, 0x05,             // move.w #5,-(a7)
        0x4e, 0x4e,                         // trap #14
        0x4f, 0xef, 0x00, 0x0c,             // lea 12(a7),a7
    });
    constexpr auto service_setup_sha256 =
        "a07c7766104d5bf581862d24de4e594b60414625824e8360b1677cf92e88c6f3";
    if (state1.source_offset != 0x55800 || state1.destination != 0xb000
        || state1.byte_count != 0x5e400 || state1_bytes.size() != state1.byte_count
        || branch_target_relative_offset > state1_bytes.size()
        || service_setup.size() > state1_bytes.size() - branch_target_relative_offset) {
        throw std::runtime_error("Unexpected Deuteros Atari ST state-1 display-service placement");
    }
    require_bytes(state1_bytes, branch_relative_offset, branch,
        "Unexpected Deuteros Atari ST state-1 display-service branch");
    const auto branch_bytes = state1_bytes.subspan(branch_relative_offset, branch.size());
    const auto service_bytes = state1_bytes.subspan(branch_target_relative_offset, service_setup.size());
    require_bytes(state1_bytes, branch_target_relative_offset, service_setup,
        "Unexpected Deuteros Atari ST state-1 display-service setup");
    if (to_hex(sha256(branch_bytes)) != branch_sha256) {
        throw std::runtime_error("Unexpected Deuteros Atari ST state-1 display-service hash");
    }
    if (to_hex(sha256(service_bytes)) != service_setup_sha256) {
        throw std::runtime_error("Unexpected Deuteros Atari ST state-1 display-service hash");
    }
    const auto computed_target = branch_relative_offset + branch.size()
        + static_cast<std::size_t>(branch_displacement);
    if (computed_target != branch_target_relative_offset) {
        throw std::runtime_error("Unexpected Deuteros Atari ST state-1 display-service branch target");
    }
    return {branch_relative_offset, branch_displacement, branch_target_relative_offset,
        std::string(branch_sha256), branch_target_relative_offset, service_setup.size(),
        std::string(service_setup_sha256), be16(service_bytes, 0), be32(service_bytes, 2),
        be16(service_bytes, 6), be16(service_bytes, 8), be16(service_bytes, 10),
        be16(service_bytes, 12), be16(service_bytes, 14), be16(service_bytes, 16)};
}

} // namespace eon
