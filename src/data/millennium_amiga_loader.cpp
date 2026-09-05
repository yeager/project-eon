#include "data/millennium_amiga_loader.hpp"
#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <stdexcept>
#include <string_view>

namespace eon {
namespace {

constexpr std::uint32_t bootstrap_disk_offset = 0x400;
constexpr std::uint32_t bootstrap_length = 0x400;
constexpr std::uint32_t bootstrap_destination = 0x70000;

std::uint32_t big32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw std::runtime_error("Truncated Millennium Amiga loader field");
    }
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U)
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 2]) << 8U)
        | bytes[offset + 3];
}

void validate_range(const MillenniumAmigaLoadStage& stage) {
    if (stage.length == 0 || stage.disk_offset > AmigaAdf::standard_size
        || stage.length > AmigaAdf::standard_size - stage.disk_offset) {
        throw std::runtime_error("Millennium Amiga loader requests data outside ADF");
    }
}

MillenniumAmigaLoadStage make_stage(const AmigaAdf& disk, std::uint32_t disk_offset,
                                    std::uint32_t length, std::uint32_t destination) {
    MillenniumAmigaLoadStage stage{disk_offset, length, destination, {}};
    validate_range(stage);
    stage.raw_sha256 = to_hex(sha256(disk.bytes(stage.disk_offset, stage.length)));
    return stage;
}

} // namespace

MillenniumAmigaLoadPlan parse_millennium_amiga_load_plan(const AmigaAdf& disk) {
    if (disk.kind() != AmigaDiskKind::dos || !disk.boot_checksum_valid()) {
        throw std::runtime_error("Millennium Amiga loader requires a checksummed DOS ADF");
    }

    // These instructions are the recovered first raw read in the stage loaded
    // from disk offset $400. They establish the primary game stage at $41000.
    constexpr std::array<std::uint8_t, 14> first_prefix{{
        0x22, 0x3c, 0x00, 0x04, 0x10, 0x00, // move.l #$41000,d1
        0x20, 0x3c, 0x00, 0x02, 0x42, 0x00, // move.l #$24200,d0
        0x2e, 0x3c,                    // move.l #...,d7
    }};
    const auto loader = disk.bytes(bootstrap_disk_offset, bootstrap_length);
    const auto first = std::search(loader.begin(), loader.end(), first_prefix.begin(), first_prefix.end());
    if (first == loader.end()) throw std::runtime_error("Millennium Amiga first-stage read not found");
    const auto first_offset = static_cast<std::size_t>(first - loader.begin());
    if (first_offset + 34 > loader.size()) throw std::runtime_error("Truncated Millennium Amiga first-stage request");
    const auto first_chunk = big32(loader, first_offset + 14);
    const auto multiplier = static_cast<std::uint32_t>(loader[first_offset + 20]) << 8U
        | loader[first_offset + 21];
    if (loader[first_offset + 18] != 0xce || loader[first_offset + 19] != 0xfc
        || loader[first_offset + 22] != 0x4e || loader[first_offset + 23] != 0xb9
        || big32(loader, first_offset + 24) != 0x000661da) {
        throw std::runtime_error("Unexpected Millennium Amiga first-stage loader sequence");
    }
    if (first_chunk == 0 || multiplier == 0 || first_chunk > UINT32_MAX / multiplier) {
        throw std::runtime_error("Invalid Millennium Amiga first-stage disk offset");
    }

    // The following request is issued after calling the first loaded stage.
    constexpr std::array<std::uint8_t, 14> resident_prefix{{
        0x22, 0x3c, 0x00, 0x06, 0x80, 0x00, // move.l #$68000,d1
        0x20, 0x3c, 0x00, 0x01, 0x64, 0x00, // move.l #$16400,d0
        0x2e, 0x3c,                    // move.l #...,d7
    }};
    const auto resident = std::search(loader.begin(), loader.end(), resident_prefix.begin(), resident_prefix.end());
    if (resident == loader.end()) throw std::runtime_error("Millennium Amiga resident-stage read not found");
    const auto resident_offset = static_cast<std::size_t>(resident - loader.begin());
    if (resident_offset + 50 > loader.size()) throw std::runtime_error("Truncated Millennium Amiga resident-stage request");
    const auto resident_chunk = big32(loader, resident_offset + 14);
    if (loader[resident_offset + 18] != 0xde || loader[resident_offset + 19] != 0x87
        || loader[resident_offset + 20] != 0x4e || loader[resident_offset + 21] != 0xb9
        || big32(loader, resident_offset + 22) != 0x000661da) {
        throw std::runtime_error("Unexpected Millennium Amiga resident-stage loader sequence");
    }
    // $661da copies D7 to D2 before chunking D0. $66216 then stores D0 at
    // IORequest+$24 (io_Length), D1 at +$28 (io_Data), and D2 at +$2c
    // (io_Offset). Thus the multiplied D7 value is the disk offset, not the
    // transfer length. This distinction is essential: reversing the fields
    // would make the first read overwrite its still-running loader.
    if (resident_chunk == 0 || resident_chunk > UINT32_MAX / 2U) {
        throw std::runtime_error("Invalid Millennium Amiga resident-stage length");
    }
    const auto resident_disk_offset = resident_chunk * 2U * 0x10U;
    const auto magic_offset = resident_offset + 54;
    if (big32(loader, magic_offset) != 0xa8d398fb) {
        throw std::runtime_error("Millennium Amiga loader handoff marker not found");
    }

    MillenniumAmigaLoadPlan plan{
        make_stage(disk, bootstrap_disk_offset, bootstrap_length, bootstrap_destination),
        make_stage(disk, first_chunk * multiplier, 0x24200, 0x41000),
        make_stage(disk, resident_disk_offset, 0x16400, 0x68000),
        0x68000,
        big32(loader, magic_offset),
    };
    validate_range(plan.bootstrap_loader);
    validate_range(plan.first_stage);
    validate_range(plan.resident_stage);
    return plan;
}

MillenniumAmigaBootstrapOpaqueInvocationBoundary
parse_millennium_amiga_bootstrap_opaque_invocation_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan) {
    // This is the bootstrap-loader's static continuation after its setup
    // call. It issues the already-validated first raw read, calls that loaded
    // address indirectly through A3, then issues the resident read and jumps
    // to its A3 value. The first invocation is deliberately an opaque stop:
    // the following source bytes are not evidence that it returns at runtime.
    constexpr std::uint32_t entry_address = 0x7029e;
    constexpr std::size_t raw_disk_offset = 0x69e;
    constexpr std::array<std::uint8_t, 132> expected{{
        0x23,0xc9,0x00,0x06,0x60,0x42,0x20,0x3c,0x00,0x06,0x1a,0x80,
        0x53,0x80,0x66,0xfc,0x4e,0xb9,0x00,0x06,0x61,0x28,0x22,0x3c,
        0x00,0x04,0x10,0x00,0x20,0x3c,0x00,0x02,0x42,0x00,0x2e,0x3c,
        0x00,0x00,0x16,0x00,0xce,0xfc,0x00,0x50,0x4e,0xb9,0x00,0x06,
        0x61,0xda,0x70,0x00,0x2e,0x79,0x00,0x07,0xff,0x00,0x26,0x7c,
        0x00,0x04,0x10,0x00,0x22,0x79,0x00,0x06,0x60,0x42,0x4e,0x93,
        0x22,0x3c,0x00,0x06,0x80,0x00,0x20,0x3c,0x00,0x01,0x64,0x00,
        0x2e,0x3c,0x00,0x00,0x16,0x00,0xde,0x87,0x4e,0xb9,0x00,0x06,
        0x61,0xda,0x70,0x00,0x2e,0x79,0x00,0x07,0xff,0x00,0x26,0x7c,
        0x00,0x06,0x80,0x00,0x22,0x79,0x00,0x06,0x60,0x42,0x60,0x04,
        0x20,0x4b,0x4e,0x75,0x2c,0x3c,0xa8,0xd3,0x98,0xfb,0x4e,0xd3,
    }};
    constexpr std::string_view expected_hash =
        "b8ca18e61e5372ba4387abd69f6796435671465ddaf48cd3a3e4b41e2528efdc";
    constexpr std::uint32_t first_invocation = 0x702e4;
    constexpr std::uint32_t static_post_first = 0x702e6;
    constexpr std::uint32_t resident_jump = 0x70320;
    if (plan.bootstrap_loader.disk_offset != bootstrap_disk_offset
        || plan.bootstrap_loader.destination != bootstrap_destination
        || plan.first_stage.disk_offset != 0x6e000 || plan.first_stage.length != 0x24200
        || plan.first_stage.destination != 0x41000 || plan.resident_stage.disk_offset != 0x2c000
        || plan.resident_stage.length != 0x16400 || plan.resident_stage.destination != 0x68000
        || plan.resident_entry != plan.resident_stage.destination) {
        throw std::runtime_error("Unexpected Millennium Amiga opaque invocation plan");
    }
    if (raw_disk_offset < plan.bootstrap_loader.disk_offset
        || raw_disk_offset > plan.bootstrap_loader.disk_offset + plan.bootstrap_loader.length
        || expected.size() > plan.bootstrap_loader.disk_offset + plan.bootstrap_loader.length - raw_disk_offset) {
        throw std::runtime_error("Millennium Amiga opaque invocation boundary outside bootstrap loader");
    }
    const auto bytes = disk.bytes(raw_disk_offset, expected.size());
    const auto hash = to_hex(sha256(bytes));
    if (!std::equal(expected.begin(), expected.end(), bytes.begin()) || hash != expected_hash) {
        throw std::runtime_error("Unexpected Millennium Amiga opaque invocation boundary");
    }
    return {entry_address, raw_disk_offset, expected.size(), hash, first_invocation,
        plan.first_stage.destination, static_post_first, resident_jump,
        plan.resident_stage.destination};
}

MillenniumAmigaFirstStageEntryBoundary
parse_millennium_amiga_first_stage_entry_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan) {
    constexpr std::size_t source_offset = 0x6e000;
    constexpr std::size_t source_size = 0x24200;
    constexpr std::uint32_t destination = 0x41000;
    constexpr std::size_t entry_size = 0x1d8;
    constexpr std::string_view source_hash =
        "df97c7f6cd622b16b9ffb57bc562906e349c18c56ed8abeb564c6f411e64891c";
    constexpr std::string_view entry_hash =
        "0bac96c92bd1639976b8e4f57c60aca022e170490f9ea0703a96bd99cae965bd";
    if (plan.first_stage.disk_offset != source_offset
        || plan.first_stage.length != source_size
        || plan.first_stage.destination != destination
        || plan.first_stage.raw_sha256 != source_hash) {
        throw std::runtime_error("Unexpected Millennium Amiga first-stage transfer");
    }
    const auto source = disk.bytes(source_offset, source_size);
    const auto entry = source.first(entry_size);
    if (to_hex(sha256(source)) != source_hash
        || to_hex(sha256(entry)) != entry_hash
        || entry[0] != 0x60 || entry[1] != 0x00
        || entry[2] != 0x00 || entry[3] != 0xba
        || entry[0xbc] != 0x2f || entry[0xbd] != 0x0e
        || entry[0xde] != 0x4a || entry[0xdf] != 0xfc
        || entry[0xe0] != 0x23 || entry[0xe1] != 0xc0
        || entry[0xfc] != 0x4a || entry[0xfd] != 0xfc
        || entry[0x172] != 0x48 || entry[0x173] != 0xe7
        || entry[0x1d6] != 0x4e || entry[0x1d7] != 0x73) {
        throw std::runtime_error("Unexpected Millennium Amiga first-stage entry");
    }
    return {source_offset, source_size, destination, std::string(source_hash),
        entry_size, std::string(entry_hash), 0x410bc, 0x410de, 0x10,
        0x410e0, 0x410fc, 0x41172, 0x410fe, 0x41110};
}

MillenniumAmigaBootstrapRelocationBoundary
parse_millennium_amiga_bootstrap_relocation_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan) {
    // The $70000 bootstrap was read as exactly $400 bytes from disk +$400.
    // Its local DBRA relocator starts at source $70032 and executes D1+1
    // copies after deriving D1 = $66400 - $66032 = $3ce. That last source
    // byte is $70400, one byte beyond the established half-open I/O range.
    // Preserve this instead of silently taking the following ADF byte.
    constexpr std::uint32_t entry_address = 0x70000;
    constexpr std::uint32_t loaded_end = 0x70400;
    constexpr std::uint32_t copy_source = 0x70032;
    constexpr std::uint32_t copy_destination = 0x66032;
    constexpr std::uint32_t copy_count = 0x3cf;
    constexpr std::uint32_t copy_end = 0x70400;
    constexpr std::uint32_t relocated_continuation = 0x6629e;
    constexpr std::uint32_t raw_continuation = 0x7029e;
    constexpr std::size_t raw_disk_offset = 0x400;
    constexpr std::array<std::uint8_t, 66> expected{{
        0x33,0xfc,0x00,0x24,0x00,0xdf,0xf1,0x04,0x4e,0x71,0x4e,0x71,
        0x26,0x7c,0x00,0x06,0x60,0x32,0x22,0x3c,0x00,0x06,0x64,0x00,
        0x04,0x81,0x00,0x06,0x60,0x32,0x23,0xcf,0x00,0x07,0xff,0x00,
        0x2e,0x7c,0x00,0x07,0xfe,0x00,0x48,0xe7,0x00,0xfe,0x48,0x7a,
        0x00,0x00,0x2a,0x5f,0x54,0x8d,0x16,0xdd,0x51,0xc9,0xff,0xfc,
        0x4e,0xf9,0x00,0x06,0x62,0x9e,
    }};
    constexpr std::string_view expected_hash =
        "341e6cff049ff9cda953ad0c91f9a064ed2d2cdc1782b417f27ecad7c9b279b4";
    if (plan.bootstrap_loader.disk_offset != raw_disk_offset
        || plan.bootstrap_loader.length != 0x400
        || plan.bootstrap_loader.destination != entry_address) {
        throw std::runtime_error("Unexpected Millennium Amiga bootstrap relocation plan");
    }
    const auto bytes = disk.bytes(raw_disk_offset, expected.size());
    const auto digest = to_hex(sha256(bytes));
    if (!std::equal(expected.begin(), expected.end(), bytes.begin()) || digest != expected_hash) {
        throw std::runtime_error("Unexpected Millennium Amiga bootstrap relocator");
    }
    if (copy_source + copy_count - 1U != copy_end
        || copy_end != loaded_end
        || copy_source + (relocated_continuation - copy_destination) != raw_continuation) {
        throw std::runtime_error("Invalid Millennium Amiga bootstrap relocation boundary");
    }
    return {entry_address, entry_address, loaded_end, copy_source, copy_destination,
        copy_count, copy_end, relocated_continuation, raw_continuation, raw_disk_offset,
        static_cast<std::uint32_t>(expected.size()), digest};
}

MillenniumAmigaFirstStageSourceAnchorBoundary
parse_millennium_amiga_first_stage_source_anchor_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan) {
    // The first stage is invoked indirectly and its output representation is
    // unknown. These are only byte-exact anchors in the original raw input.
    constexpr std::size_t source_offset = 0x24200;
    constexpr std::size_t source_size = 0x6e000;
    constexpr std::string_view source_hash =
        "5ed30d5fe99c0dfc905bbe639d626be558f022514c83bc5ff287ad91014ccf7a";
    constexpr std::array<std::uint32_t, 3> anchor_offsets{{0x4a3dc, 0x4a648, 0x4a936}};
    constexpr std::array<std::string_view, 3> anchors{{
        "exec.library", "graphics.library", "input.device",
    }};
    constexpr std::array<std::size_t, 2> window_offsets{{0x4a5b0, 0x4a900}};
    constexpr std::array<std::size_t, 2> window_sizes{{0x160, 0x220}};
    constexpr std::array<std::string_view, 2> window_hashes{{
        "97bb8cbe026ac3bba2c19cc296bc7cef00fbd0c8095c678f4cc303761b8b8309",
        "ee84336cbf4665bcd2bc48d054c024a20e4c5faaaf26cd5fdcc78e6b8f3931c9",
    }};
    if (plan.first_stage.disk_offset != source_offset || plan.first_stage.length != source_size
        || plan.first_stage.destination != 0x41000) {
        throw std::runtime_error("Unexpected Millennium Amiga first-stage source plan");
    }
    const auto source = disk.bytes(source_offset, source_size);
    const auto hash = to_hex(sha256(source));
    if (hash != source_hash) {
        throw std::runtime_error("Unexpected Millennium Amiga first-stage source range");
    }
    for (std::size_t index = 0; index < anchors.size(); ++index) {
        const auto offset = anchor_offsets[index];
        const auto text = anchors[index];
        if (offset > source.size() || text.size() + 1U > source.size() - offset
            || !std::equal(text.begin(), text.end(), source.begin() + offset)
            || source[offset + text.size()] != 0) {
            throw std::runtime_error("Unexpected Millennium Amiga first-stage source anchor");
        }
    }
    MillenniumAmigaFirstStageSourceAnchorBoundary result{
        source_offset, source_size, hash, anchor_offsets, window_offsets, window_sizes, {},
    };
    for (std::size_t index = 0; index < window_offsets.size(); ++index) {
        const auto offset = window_offsets[index];
        const auto size = window_sizes[index];
        if (offset > source.size() || size > source.size() - offset) {
            throw std::runtime_error("Millennium Amiga first-stage source window outside range");
        }
        const auto window_hash = to_hex(sha256(source.subspan(offset, size)));
        if (window_hash != window_hashes[index]) {
            throw std::runtime_error("Unexpected Millennium Amiga first-stage source window");
        }
        result.window_sha256[index] = window_hash;
    }
    return result;
}

MillenniumAmigaSharedResidentLayout parse_millennium_amiga_shared_resident_layout(
    const std::span<const std::uint8_t> image) {
    constexpr std::uint32_t disk_offset = 0x16400;
    constexpr std::uint32_t length = 0x2c000;
    constexpr std::uint32_t destination = 0x68000;
    constexpr std::string_view expected_sha256 =
        "d144abc05f891710dc99b30d87f020bd6e2ff7796ef86a847f07b8d97d55d18e";
    if (image.size() < static_cast<std::size_t>(disk_offset) + length) {
        throw std::runtime_error("Millennium Amiga image truncates shared resident range");
    }
    const auto resident = image.subspan(disk_offset, length);
    const auto digest = to_hex(sha256(resident));
    if (digest != expected_sha256) {
        throw std::runtime_error("Unexpected Millennium Amiga shared resident range");
    }
    return {disk_offset, length, destination, digest};
}

MillenniumAmigaResidentEntry parse_millennium_amiga_resident_entry(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan) {
    validate_range(plan.first_stage);
    validate_range(plan.resident_stage);
    if (plan.resident_entry != plan.resident_stage.destination) {
        throw std::runtime_error("Millennium Amiga resident entry is outside its loaded range");
    }

    constexpr std::array<std::uint8_t, 20> entry_prefix{{
        0x4e, 0xb9, 0x00, 0x07, 0x87, 0xd4, // jsr $787d4
        0x4a, 0x03,                         // tst.b d3
        0x67, 0x04,                         // beq.s past OR
        0x00, 0x40, 0x01, 0x00,             // ori.w #$0100,d0
        0x33, 0xc0, 0x00, 0x07, 0xb7, 0x5a, // move.w d0,$7b75a
    }};
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset, entry_prefix.size() + 2U);
    if (!std::equal(entry_prefix.begin(), entry_prefix.end(), bytes.begin())
        || bytes[entry_prefix.size()] != 0x4e || bytes[entry_prefix.size() + 1U] != 0x75) {
        throw std::runtime_error("Unexpected Millennium Amiga resident entry gate");
    }

    constexpr std::uint32_t initializer_address = 0x787d4;
    const auto first_end = static_cast<std::uint64_t>(plan.first_stage.destination)
        + plan.first_stage.length;
    if (initializer_address < plan.first_stage.destination || initializer_address >= first_end) {
        throw std::runtime_error("Millennium Amiga resident initializer is outside first stage RAM");
    }
    return {plan.resident_entry, initializer_address, 0x7b75a, 0x0100};
}

MillenniumAmigaResidentWordSplitter parse_millennium_amiga_resident_word_splitter(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan) {
    validate_range(plan.resident_stage);
    if (plan.resident_entry != plan.resident_stage.destination) {
        throw std::runtime_error("Millennium Amiga resident entry is outside its loaded range");
    }

    // The preceding gate is 22 bytes. This routine deliberately has no
    // inferred call edge from that gate: it is merely the next complete raw
    // subroutine, profiled because all of its bytes and RAM operands are
    // directly present in the resident disk range.
    constexpr std::size_t routine_offset = 0x16;
    constexpr std::array<std::uint8_t, 16> prefix{{
        0x20, 0x49,                         // movea.l a1,a0
        0xd0, 0xfc, 0x00, 0x36,             // adda.w #$36,a0
        0x30, 0x18,                         // move.w (a0)+,d0
        0x42, 0x03,                         // clr.b d3
        0xe3, 0x48,                         // lsl.w #1,d0
        0xe3, 0x13,                         // roxl.b #1,d3
        0xe2, 0x48,                         // lsr.w #1,d0
    }};
    constexpr std::array<std::uint8_t, 10> next_word{{
        0x30, 0x18, 0x42, 0x03, 0xe3, 0x48, 0xe3, 0x13, 0xe2, 0x48,
    }};
    constexpr std::array<std::uint32_t, 3> magnitude_addresses{{0x7b764, 0x7b766, 0x7b768}};
    constexpr std::array<std::uint32_t, 3> sign_addresses{{0x7b776, 0x7b777, 0x7b778}};
    constexpr std::array<std::uint8_t, 26> tail{{
        0x4e, 0xb9, 0x00, 0x07, 0xba, 0x12, // jsr $7ba12
        0x30, 0x39, 0x00, 0x07, 0xb7, 0x68, // move.w $7b768,d0
        0x16, 0x39, 0x00, 0x07, 0xb7, 0x78, // move.b $7b778,d3
        0x4a, 0x03, 0x67, 0x02, 0x44, 0x40, 0x4e, 0x75, // sign branch; rts
    }};
    constexpr std::size_t stores_size = 12;
    constexpr std::size_t routine_length = prefix.size() + stores_size
        + 2 * (next_word.size() + stores_size) + tail.size();
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + routine_offset, routine_length);
    if (!std::equal(prefix.begin(), prefix.end(), bytes.begin())) {
        throw std::runtime_error("Unexpected Millennium Amiga resident word-splitter prefix");
    }

    std::size_t offset = prefix.size();
    for (std::size_t index = 0; index < magnitude_addresses.size(); ++index) {
        if (index != 0) {
            if (!std::equal(next_word.begin(), next_word.end(),
                    bytes.begin() + static_cast<std::ptrdiff_t>(offset))) {
                throw std::runtime_error("Unexpected Millennium Amiga resident word-splitter input");
            }
            offset += next_word.size();
        }
        const std::array<std::uint8_t, 12> expected_store{{
            0x33, 0xc0,
            static_cast<std::uint8_t>(magnitude_addresses[index] >> 24U),
            static_cast<std::uint8_t>(magnitude_addresses[index] >> 16U),
            static_cast<std::uint8_t>(magnitude_addresses[index] >> 8U),
            static_cast<std::uint8_t>(magnitude_addresses[index]),
            0x13, 0xc3,
            static_cast<std::uint8_t>(sign_addresses[index] >> 24U),
            static_cast<std::uint8_t>(sign_addresses[index] >> 16U),
            static_cast<std::uint8_t>(sign_addresses[index] >> 8U),
            static_cast<std::uint8_t>(sign_addresses[index]),
        }};
        if (!std::equal(expected_store.begin(), expected_store.end(),
                bytes.begin() + static_cast<std::ptrdiff_t>(offset))) {
            throw std::runtime_error("Unexpected Millennium Amiga resident word-splitter store");
        }
        offset += expected_store.size();
    }
    if (!std::equal(tail.begin(), tail.end(),
            bytes.begin() + static_cast<std::ptrdiff_t>(offset))) {
        throw std::runtime_error("Unexpected Millennium Amiga resident word-splitter tail");
    }
    return {plan.resident_entry + static_cast<std::uint32_t>(routine_offset), 0x36,
        magnitude_addresses, sign_addresses, 0x7ba12, 0x7b768, 0x7b778};
}

MillenniumAmigaResidentWordSplitterPreHelperState
split_millennium_amiga_resident_words_pre_helper(
    const std::array<std::uint16_t, 3>& source_words) {
    MillenniumAmigaResidentWordSplitterPreHelperState state;
    for (std::size_t index = 0; index < source_words.size(); ++index) {
        // LSL.W #1 sends original bit 15 to X; ROXL.B #1 rotates that X
        // into cleared D3 bit 0; LSR.W #1 restores bits 14..0.
        state.magnitude_words[index] = static_cast<std::uint16_t>(source_words[index] & 0x7fffU);
        state.sign_bytes[index] = static_cast<std::uint8_t>(source_words[index] >> 15U);
    }
    return state;
}

MillenniumAmigaResidentHelperRawBoundary
parse_millennium_amiga_resident_helper_raw_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentWordSplitter& splitter) {
    validate_range(plan.resident_stage);
    if (splitter.helper_address < plan.resident_stage.destination) {
        throw std::runtime_error("Millennium Amiga helper precedes resident raw range");
    }
    const auto relative = splitter.helper_address - plan.resident_stage.destination;
    constexpr std::array<std::uint8_t, 32> expected_prefix{{
        0x00, 0x01, 0x20, 0x00, 0x80, 0xac, 0x00, 0x00,
        0x01, 0x00, 0x08, 0x80, 0x42, 0x00, 0x00, 0x01,
        0x01, 0x00, 0x80, 0xac, 0x00, 0x00, 0x01, 0x00,
        0x20, 0x80, 0x42, 0x00, 0x00, 0x01, 0x00, 0x10,
    }};
    if (relative > plan.resident_stage.length
        || expected_prefix.size() > plan.resident_stage.length - relative) {
        throw std::runtime_error("Millennium Amiga helper is outside resident raw range");
    }
    const auto raw_disk_offset = plan.resident_stage.disk_offset + relative;
    const auto source = disk.bytes(raw_disk_offset, expected_prefix.size());
    if (!std::equal(expected_prefix.begin(), expected_prefix.end(), source.begin())) {
        throw std::runtime_error("Unexpected Millennium Amiga helper raw boundary");
    }
    MillenniumAmigaResidentHelperRawBoundary result;
    result.helper_address = splitter.helper_address;
    result.raw_disk_offset = raw_disk_offset;
    std::copy(source.begin(), source.end(), result.raw_prefix.begin());
    result.raw_prefix_sha256 = to_hex(sha256(source));
    return result;
}

MillenniumAmigaResidentSetupHelperRawBoundary
parse_millennium_amiga_resident_setup_helper_raw_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan) {
    validate_range(plan.resident_stage);
    constexpr std::uint32_t helper_address = 0x7b77e;
    if (helper_address < plan.resident_stage.destination) {
        throw std::runtime_error("Millennium Amiga setup helper precedes resident raw range");
    }
    const auto relative = helper_address - plan.resident_stage.destination;
    constexpr std::array<std::uint8_t, 32> expected_prefix{{
        0x04, 0x00, 0x6e, 0x00, 0xc2, 0x00, 0x04, 0x4a,
        0x00, 0xc2, 0x40, 0x00, 0x7a, 0x00, 0xc2, 0x00,
        0x10, 0x52, 0x00, 0xc2, 0x01, 0x00, 0x52, 0x00,
        0xc2, 0x00, 0x01, 0x4a, 0x00, 0xc2, 0x08, 0x00,
    }};
    if (relative > plan.resident_stage.length
        || expected_prefix.size() > plan.resident_stage.length - relative) {
        throw std::runtime_error("Millennium Amiga setup helper is outside resident raw range");
    }
    const auto raw_disk_offset = plan.resident_stage.disk_offset + relative;
    const auto source = disk.bytes(raw_disk_offset, expected_prefix.size());
    if (!std::equal(expected_prefix.begin(), expected_prefix.end(), source.begin())) {
        throw std::runtime_error("Unexpected Millennium Amiga setup helper raw boundary");
    }
    MillenniumAmigaResidentSetupHelperRawBoundary result;
    result.helper_address = helper_address;
    result.raw_disk_offset = raw_disk_offset;
    std::copy(source.begin(), source.end(), result.raw_prefix.begin());
    result.raw_prefix_sha256 = to_hex(sha256(source));
    return result;
}

std::array<MillenniumAmigaResidentHelperStagingCallsite, 2>
parse_millennium_amiga_resident_helper_staging_callsites(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentWordSplitter& splitter) {
    validate_range(plan.resident_stage);
    constexpr std::uint32_t setup_helper = 0x7b77e;
    constexpr std::uint32_t magnitude_destination = 0x7b764;
    constexpr std::uint32_t sign_destination = 0x7b776;
    constexpr std::uint32_t clear_byte = 0x7b14e;
    constexpr std::array<std::uint32_t, 2> entries{{0x69624, 0x69b88}};
    constexpr std::array<std::uint32_t, 2> sources{{0x7cc3c, 0x7cc68}};
    constexpr std::array<std::uint32_t, 2> post_helper_sources{{0x7cc46, 0x7cc72}};
    std::array<MillenniumAmigaResidentHelperStagingCallsite, 2> callsites{};
    constexpr std::array<std::uint8_t, 6> word_copies_bytes{{0x3a, 0xdc, 0x3a, 0xdc, 0x3a, 0xdc}};
    constexpr std::array<std::uint8_t, 6> sign_copies_bytes{{0x1a, 0xdc, 0x1a, 0xdc, 0x1a, 0xdc}};

    for (std::size_t index = 0; index < callsites.size(); ++index) {
        if (entries[index] < plan.resident_stage.destination) {
            throw std::runtime_error("Millennium Amiga helper staging callsite precedes resident range");
        }
        const auto relative = entries[index] - plan.resident_stage.destination;
        constexpr std::size_t prefix_size = 6;
        constexpr std::size_t word_copies = 3 * 2;
        constexpr std::size_t sign_copies = 3 * 2;
        constexpr std::size_t suffix_size = 6 + 8 + 6;
        constexpr std::size_t size = prefix_size + 6 + word_copies + 6 + sign_copies + suffix_size;
        constexpr std::size_t post_helper_prefix_size = 12;
        if (relative > plan.resident_stage.length
            || size + post_helper_prefix_size > plan.resident_stage.length - relative) {
            throw std::runtime_error("Millennium Amiga helper staging callsite is outside resident range");
        }
        const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, size + post_helper_prefix_size);
        const std::array<std::uint8_t, 6> source_prefix{{
            0x28, 0x7c, static_cast<std::uint8_t>(sources[index] >> 24U),
            static_cast<std::uint8_t>(sources[index] >> 16U),
            static_cast<std::uint8_t>(sources[index] >> 8U), static_cast<std::uint8_t>(sources[index]),
        }};
        constexpr std::array<std::uint8_t, 6> magnitude_prefix{{0x2a, 0x7c, 0x00, 0x07, 0xb7, 0x64}};
        constexpr std::array<std::uint8_t, 6> sign_prefix{{0x2a, 0x7c, 0x00, 0x07, 0xb7, 0x76}};
        constexpr std::array<std::uint8_t, 20> suffix{{
            0x4e, 0xb9, 0x00, 0x07, 0xb7, 0x7e,
            0x13, 0xfc, 0x00, 0x00, 0x00, 0x07, 0xb1, 0x4e,
            0x4e, 0xb9, 0x00, 0x07, 0xba, 0x12,
        }};
        if (!std::equal(source_prefix.begin(), source_prefix.end(), bytes.begin())) {
            throw std::runtime_error("Unexpected Millennium Amiga helper staging source");
        }
        if (!std::equal(magnitude_prefix.begin(), magnitude_prefix.end(), bytes.begin() + prefix_size)
            || !std::equal(bytes.begin() + prefix_size + magnitude_prefix.size(),
                bytes.begin() + prefix_size + magnitude_prefix.size() + word_copies,
                word_copies_bytes.begin())) {
            throw std::runtime_error("Unexpected Millennium Amiga helper word staging");
        }
        if (!std::equal(sign_prefix.begin(), sign_prefix.end(), bytes.begin() + prefix_size + magnitude_prefix.size() + word_copies)
            || !std::equal(bytes.begin() + prefix_size + magnitude_prefix.size() + word_copies + sign_prefix.size(),
                bytes.begin() + prefix_size + magnitude_prefix.size() + word_copies + sign_prefix.size() + sign_copies,
                sign_copies_bytes.begin())) {
            throw std::runtime_error("Unexpected Millennium Amiga helper sign staging");
        }
        if (!std::equal(suffix.begin(), suffix.end(), bytes.begin() + size - suffix.size())) {
            throw std::runtime_error("Unexpected Millennium Amiga helper staging tail");
        }
        const std::array<std::uint8_t, 12> post_helper_prefix{{
            0x2a, 0x7c,
            static_cast<std::uint8_t>(post_helper_sources[index] >> 24U),
            static_cast<std::uint8_t>(post_helper_sources[index] >> 16U),
            static_cast<std::uint8_t>(post_helper_sources[index] >> 8U),
            static_cast<std::uint8_t>(post_helper_sources[index]),
            0x28, 0x7c, 0x00, 0x07, 0xb7, 0x64,
        }};
        if (!std::equal(post_helper_prefix.begin(), post_helper_prefix.end(), bytes.begin() + size)) {
            throw std::runtime_error("Unexpected Millennium Amiga post-helper static boundary");
        }
        callsites[index] = {entries[index], sources[index], magnitude_destination, sign_destination,
            setup_helper, clear_byte, splitter.helper_address,
            entries[index] + static_cast<std::uint32_t>(size),
            post_helper_sources[index], magnitude_destination};
    }
    return callsites;
}

MillenniumAmigaResidentHelperStagingPreSetupState
stage_millennium_amiga_resident_helper_pre_setup(
    const std::array<std::uint16_t, 3>& source_words,
    const std::array<std::uint8_t, 3>& source_sign_bytes) {
    // Each verified callsite contains three MOVE.W (A4)+,(A5)+ followed by
    // three MOVE.B (A4)+,(A5)+.  No later call edge is included here.
    return {source_words, source_sign_bytes};
}

MillenniumAmigaResidentFirstPostHelperStaticChain
parse_millennium_amiga_resident_first_post_helper_static_chain(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentHelperStagingCallsite& callsite) {
    // This is a static post-JSR byte anchor only. The first caller's next
    // 86 raw bytes contain two literal JSR encodings at +0x4a and +0x50; the
    // entire range is hash-bound so a similar-looking patched sequence fails.
    validate_range(plan.resident_stage);
    constexpr std::uint32_t entry_address = 0x69624;
    constexpr std::uint32_t static_start_address = 0x69656;
    constexpr std::uint32_t next_setup_call_address = 0x696a0;
    constexpr std::uint32_t next_setup_target = 0x7b77e;
    constexpr std::uint32_t following_call_address = 0x696a6;
    constexpr std::uint32_t following_target = 0x7c802;
    constexpr std::size_t byte_count = 86;
    constexpr std::array<std::uint8_t, 12> tail_calls{{
        0x4e, 0xb9, 0x00, 0x07, 0xb7, 0x7e, 0x4e, 0xb9, 0x00, 0x07, 0xc8, 0x02,
    }};
    if (callsite.entry_address != entry_address || callsite.post_helper_return_address != static_start_address
        || static_start_address < plan.resident_stage.destination) {
        throw std::runtime_error("Unexpected Millennium Amiga first post-helper static chain caller");
    }
    const auto relative = static_start_address - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length || byte_count > plan.resident_stage.length - relative) {
        throw std::runtime_error("Millennium Amiga first post-helper static chain outside resident range");
    }
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, byte_count);
    const auto hash = to_hex(sha256(bytes));
    if (hash != "5f42f9d3078d374f8b4a70fcc59c618abb9381d6b33ef25b3f2967876f0afe7b"
        || !std::equal(tail_calls.begin(), tail_calls.end(), bytes.end() - tail_calls.size())) {
        throw std::runtime_error("Unexpected Millennium Amiga first post-helper static chain");
    }
    return {entry_address, static_start_address, plan.resident_stage.disk_offset + relative,
        static_cast<std::uint32_t>(byte_count), hash, next_setup_call_address, next_setup_target,
        following_call_address, following_target};
}

MillenniumAmigaResidentSecondPostHelperStaticChain
parse_millennium_amiga_resident_second_post_helper_static_chain(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentHelperStagingCallsite& callsite) {
    // The second caller has its own raw continuation form. Preserve only its
    // 44-byte prefix and final literal JSR; do not reuse or infer the first
    // caller's path and do not assert any runtime return from $7ba12.
    validate_range(plan.resident_stage);
    constexpr std::uint32_t entry_address = 0x69b88;
    constexpr std::uint32_t static_start_address = 0x69bba;
    constexpr std::uint32_t static_call_address = 0x69be0;
    constexpr std::uint32_t static_call_target = 0x68d50;
    constexpr std::size_t byte_count = 44;
    constexpr std::array<std::uint8_t, 6> tail_call{{0x4e, 0xb9, 0x00, 0x06, 0x8d, 0x50}};
    if (callsite.entry_address != entry_address || callsite.post_helper_return_address != static_start_address
        || static_start_address < plan.resident_stage.destination) {
        throw std::runtime_error("Unexpected Millennium Amiga second post-helper static chain caller");
    }
    const auto relative = static_start_address - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length || byte_count > plan.resident_stage.length - relative) {
        throw std::runtime_error("Millennium Amiga second post-helper static chain outside resident range");
    }
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, byte_count);
    const auto hash = to_hex(sha256(bytes));
    if (hash != "5616f19900cb96ebc81edf90d0d17a9cde1644be07657801e243514b05e6ee23"
        || !std::equal(tail_call.begin(), tail_call.end(), bytes.end() - tail_call.size())) {
        throw std::runtime_error("Unexpected Millennium Amiga second post-helper static chain");
    }
    return {entry_address, static_start_address, plan.resident_stage.disk_offset + relative,
        static_cast<std::uint32_t>(byte_count), hash, static_call_address, static_call_target};
}

MillenniumAmigaResidentStagingDirectReachabilityBoundary
parse_millennium_amiga_resident_staging_direct_reachability_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const std::array<MillenniumAmigaResidentHelperStagingCallsite, 2>& callsites) {
    // Search raw absolute JSR/JMP.L, statically resolvable BSR.W, and only
    // fully local MOVEA.L #address,An + JSR/JMP (An) pairs. This deliberately
    // stops short of arbitrary disassembly or wider register tracking.
    validate_range(plan.resident_stage);
    constexpr std::array<std::uint32_t, 2> entries{{0x69624, 0x69b88}};
    if (callsites[0].entry_address != entries[0] || callsites[1].entry_address != entries[1]) {
        throw std::runtime_error("Unexpected Millennium Amiga staging entries for reachability scan");
    }
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset, plan.resident_stage.length);
    MillenniumAmigaResidentStagingDirectReachabilityBoundary result;
    result.staging_entry_addresses = entries;
    result.scanned_raw_disk_offset = plan.resident_stage.disk_offset;
    result.scanned_byte_count = plan.resident_stage.length;
    for (std::size_t offset = 0; offset + 2U <= bytes.size(); ++offset) {
        const auto opcode = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(bytes[offset]) << 8U)
            | bytes[offset + 1U]);
        if (offset + 6U <= bytes.size()) {
            const auto target = big32(bytes, offset + 2U);
            for (std::size_t index = 0; index < entries.size(); ++index) {
                if (target != entries[index]) continue;
                if (opcode == 0x4eb9U) ++result.absolute_jsr_counts[index];
                if (opcode == 0x4ef9U) ++result.absolute_jmp_counts[index];
            }
        }
        if (opcode == 0x6100U && offset + 4U <= bytes.size()) {
            const auto displacement = static_cast<std::int16_t>(
                static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 2U]) << 8U)
                | bytes[offset + 3U]);
            // 68000 BSR.W computes its target from the extension-word PC.
            const auto target = static_cast<std::uint32_t>(
                static_cast<std::int64_t>(plan.resident_stage.destination)
                + static_cast<std::int64_t>(offset) + 2 + displacement);
            for (std::size_t index = 0; index < entries.size(); ++index) {
                if (target == entries[index]) ++result.pc_relative_bsr_word_counts[index];
            }
        }
        if (offset + 8U <= bytes.size() && (opcode & 0xf1ffU) == 0x207cU) {
            // MOVEA.L #imm,An encodes as 0x20/22/.../2e 0x7c. Restrict to
            // that exact immediate form and an immediately following (An)
            // control transfer so the address-register value is fully local.
            if (bytes[offset + 1U] == 0x7cU) {
                const auto register_index = static_cast<std::uint16_t>(
                    (static_cast<std::uint32_t>(opcode) >> 9U) & 7U);
                const auto immediate_target = big32(bytes, offset + 2U);
                const auto transfer = static_cast<std::uint16_t>(
                    (static_cast<std::uint16_t>(bytes[offset + 6U]) << 8U) | bytes[offset + 7U]);
                for (std::size_t index = 0; index < entries.size(); ++index) {
                    if (immediate_target != entries[index]) continue;
                    if (transfer == static_cast<std::uint16_t>(0x4e90U + register_index)) {
                        ++result.local_immediate_register_jsr_counts[index];
                    }
                    if (transfer == static_cast<std::uint16_t>(0x4ed0U + register_index)) {
                        ++result.local_immediate_register_jmp_counts[index];
                    }
                }
            }
        }
    }
    if (result.absolute_jsr_counts != std::array<std::uint32_t, 2>{}
        || result.absolute_jmp_counts != std::array<std::uint32_t, 2>{}
        || result.pc_relative_bsr_word_counts != std::array<std::uint32_t, 2>{}
        || result.local_immediate_register_jsr_counts != std::array<std::uint32_t, 2>{}
        || result.local_immediate_register_jmp_counts != std::array<std::uint32_t, 2>{}) {
        throw std::runtime_error("Millennium Amiga staging entries gained direct absolute reachability");
    }
    return result;
}

MillenniumAmigaResidentPredicateGate
parse_millennium_amiga_resident_predicate_gate(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentWordSplitter& splitter) {
    validate_range(plan.resident_stage);
    constexpr std::uint32_t entry = 0x68078;
    constexpr std::uint32_t predicate = 0x7b816;
    constexpr std::array<std::uint8_t, 14> gate{{
        0x4e, 0xb9, 0x00, 0x07, 0xb8, 0x16, // jsr $7b816
        0x4a, 0x03,                         // tst.b d3
        0x67, 0x02,                         // beq.s continuation
        0x4e, 0x75,                         // rts
        0x2f, 0x09,                         // first continuation instruction
    }};
    if (splitter.entry_address != 0x68016 || entry < plan.resident_stage.destination
        || predicate < plan.resident_stage.destination) {
        throw std::runtime_error("Unexpected Millennium Amiga resident predicate gate placement");
    }
    const auto entry_relative = entry - plan.resident_stage.destination;
    const auto predicate_relative = predicate - plan.resident_stage.destination;
    constexpr std::size_t prefix_size = 32;
    if (entry_relative > plan.resident_stage.length || gate.size() > plan.resident_stage.length - entry_relative
        || predicate_relative > plan.resident_stage.length
        || prefix_size > plan.resident_stage.length - predicate_relative) {
        throw std::runtime_error("Millennium Amiga resident predicate gate is outside raw range");
    }
    const auto gate_bytes = disk.bytes(plan.resident_stage.disk_offset + entry_relative, gate.size());
    if (!std::equal(gate.begin(), gate.end(), gate_bytes.begin())) {
        throw std::runtime_error("Unexpected Millennium Amiga resident predicate gate");
    }
    const auto raw_prefix = disk.bytes(plan.resident_stage.disk_offset + predicate_relative, prefix_size);
    MillenniumAmigaResidentPredicateGate result;
    result.entry_address = entry;
    result.predicate_address = predicate;
    result.nonzero_return_address = entry + 10;
    result.zero_continue_address = entry + 12;
    result.predicate_raw_disk_offset = plan.resident_stage.disk_offset + predicate_relative;
    std::copy(raw_prefix.begin(), raw_prefix.end(), result.predicate_raw_prefix.begin());
    result.predicate_raw_prefix_sha256 = to_hex(sha256(raw_prefix));
    return result;
}

MillenniumAmigaResidentPredicateZeroPathBoundary
parse_millennium_amiga_resident_predicate_zero_path_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentPredicateGate& gate) {
    validate_range(plan.resident_stage);
    constexpr std::uint32_t entry = 0x68084;
    constexpr std::uint32_t branch_address = 0x6808e;
    constexpr std::uint32_t branch_target = 0x680ca;
    constexpr std::uint32_t call_address = 0x68096;
    constexpr std::uint32_t call_target = 0x7b90a;
    constexpr std::array<std::uint8_t, 24> bytes_expected{{
        0x2f, 0x09,                         // move.l a1,-(sp)
        0x34, 0x29, 0x00, 0x12,             // move.w $12(a1),d2
        0xb4, 0x3c, 0x00, 0x01,             // cmp.w #1,d2
        0x66, 0x3a,                         // bne.s $680ca
        0x34, 0x29, 0x00, 0x14,             // move.w $14(a1),d2
        0x2f, 0x02,                         // move.l d2,-(sp)
        0x4e, 0xb9, 0x00, 0x07, 0xb9, 0x0a, // jsr $7b90a
    }};
    if (gate.zero_continue_address != entry || entry < plan.resident_stage.destination
        || call_target < plan.resident_stage.destination) {
        throw std::runtime_error("Unexpected Millennium Amiga predicate zero path placement");
    }
    const auto entry_relative = entry - plan.resident_stage.destination;
    const auto call_relative = call_target - plan.resident_stage.destination;
    constexpr std::size_t prefix_size = 32;
    if (entry_relative > plan.resident_stage.length
        || bytes_expected.size() > plan.resident_stage.length - entry_relative
        || call_relative > plan.resident_stage.length
        || prefix_size > plan.resident_stage.length - call_relative) {
        throw std::runtime_error("Millennium Amiga predicate zero path is outside raw range");
    }
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + entry_relative, bytes_expected.size());
    if (!std::equal(bytes_expected.begin(), bytes_expected.end(), bytes.begin())) {
        throw std::runtime_error("Unexpected Millennium Amiga predicate zero path");
    }
    const auto raw_prefix = disk.bytes(plan.resident_stage.disk_offset + call_relative, prefix_size);
    MillenniumAmigaResidentPredicateZeroPathBoundary result;
    result.entry_address = entry;
    result.selector_a1_offset = 0x12;
    result.selector_compare_value = 1;
    result.selector_not_equal_branch_address = branch_address;
    result.selector_not_equal_target = branch_target;
    result.equal_path_argument_a1_offset = 0x14;
    result.unknown_call_address = call_address;
    result.unknown_call_target = call_target;
    result.unknown_call_raw_disk_offset = plan.resident_stage.disk_offset + call_relative;
    std::copy(raw_prefix.begin(), raw_prefix.end(), result.unknown_call_raw_prefix.begin());
    result.unknown_call_raw_prefix_sha256 = to_hex(sha256(raw_prefix));
    return result;
}

MillenniumAmigaResidentPredicateNotEqualPathBoundary
parse_millennium_amiga_resident_predicate_not_equal_path_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentPredicateZeroPathBoundary& zero_path) {
    validate_range(plan.resident_stage);
    constexpr std::uint32_t entry = 0x680ca;
    constexpr std::array<std::uint8_t, 10> expected{{
        0x2f, 0x00,                         // move.l d0,-(sp)
        0x2f, 0x02,                         // move.l d2,-(sp)
        0x4e, 0xb9, 0x00, 0x07, 0xb9, 0x0a, // jsr $7b90a
    }};
    if (zero_path.selector_not_equal_target != entry || entry < plan.resident_stage.destination) {
        throw std::runtime_error("Unexpected Millennium Amiga predicate not-equal path placement");
    }
    const auto relative = entry - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length || expected.size() > plan.resident_stage.length - relative) {
        throw std::runtime_error("Millennium Amiga predicate not-equal path is outside raw range");
    }
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    if (!std::equal(expected.begin(), expected.end(), bytes.begin())) {
        throw std::runtime_error("Unexpected Millennium Amiga predicate not-equal path");
    }
    return {entry, 0, 2, entry + 4, 0x7b90a};
}

MillenniumAmigaResidentIndependentEntryGate
parse_millennium_amiga_resident_independent_entry_gate(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan) {
    validate_range(plan.resident_stage);
    constexpr std::uint32_t entry = 0x68508;
    constexpr std::array<std::uint8_t, 18> expected{{
        0x48, 0xe7, 0xf0, 0x04,             // movem.l d0-d3/a5,-(sp)
        0x4a, 0x43,                         // tst.w d3
        0x6b, 0x00, 0x00, 0x86,             // bmi.w $68598
        0x4a, 0x39, 0x00, 0x07, 0xb1, 0x42, // tst.b $7b142
        0x67, 0x30,                         // beq.s $68546
    }};
    if (entry < plan.resident_stage.destination) {
        throw std::runtime_error("Millennium Amiga independent entry precedes raw range");
    }
    const auto relative = entry - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length || expected.size() > plan.resident_stage.length - relative) {
        throw std::runtime_error("Millennium Amiga independent entry is outside raw range");
    }
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    if (!std::equal(expected.begin(), expected.end(), bytes.begin())) {
        throw std::runtime_error("Unexpected Millennium Amiga independent entry gate");
    }
    return {entry, 0x6850e, 0x68598, 0x68512, 0x7b142, 0x68518, 0x6854a};
}

MillenniumAmigaResidentNegativeD3Continuation
parse_millennium_amiga_resident_negative_d3_continuation(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentIndependentEntryGate& gate) {
    constexpr std::uint32_t entry = 0x68598;
    constexpr std::size_t size = 100;
    if (gate.negative_d3_target != entry || entry < plan.resident_stage.destination) {
        throw std::runtime_error("Unexpected Millennium Amiga negative-D3 continuation placement");
    }
    const auto relative = entry - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length || size > plan.resident_stage.length - relative) {
        throw std::runtime_error("Millennium Amiga negative-D3 continuation is outside raw range");
    }
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, size);
    if (to_hex(sha256(bytes)) != "716e8bf1db5d7cad89a0074cf6fe7cc6a0a66d73379814bac181a5f6c4a9e500") {
        throw std::runtime_error("Unexpected Millennium Amiga negative-D3 continuation");
    }
    return {entry, 0x685ee, 0x7bcf8, 0x685fc};
}

MillenniumAmigaResidentNegativeD3Terminal
parse_millennium_amiga_resident_negative_d3_terminal(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentNegativeD3Continuation& continuation) {
    constexpr std::uint32_t entry = 0x685f4;
    constexpr std::array<std::uint8_t, 10> expected{{
        0x06, 0x42, 0x28, 0x00, // first encoded immediate word instruction
        0x06, 0x43, 0x28, 0x00, // second encoded immediate word instruction
        0x4e, 0x75,             // terminal rts
    }};
    if (continuation.entry_address != 0x68598 || continuation.return_address != entry + 8
        || entry < plan.resident_stage.destination) {
        throw std::runtime_error("Unexpected Millennium Amiga negative-D3 terminal placement");
    }
    const auto relative = entry - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length || expected.size() > plan.resident_stage.length - relative) {
        throw std::runtime_error("Millennium Amiga negative-D3 terminal is outside raw range");
    }
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    if (to_hex(sha256(bytes)) != "5b120eaef941ac336d22e4f76adaeefd8c1d6795d105685f048074edd49c3a6c"
        || !std::equal(expected.begin(), expected.end(), bytes.begin())) {
        throw std::runtime_error("Unexpected Millennium Amiga negative-D3 terminal");
    }
    return {entry, 0x2800, entry + 4, 0x2800, entry + 8};
}

MillenniumAmigaResidentPostNegativeD3Terminal
parse_millennium_amiga_resident_post_negative_d3_terminal(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentNegativeD3Terminal& terminal) {
    constexpr std::uint32_t entry = 0x685fe;
    constexpr std::array<std::uint8_t, 28> expected{{
        0x42, 0x40, 0x13, 0xc0, 0x00, 0x07, 0xb3, 0xb5,
        0x13, 0xc0, 0x00, 0x07, 0xb3, 0xbc, 0x30, 0x01,
        0x32, 0x02, 0x4a, 0x40, 0x66, 0x02, 0x4e, 0x75,
        0x6a, 0x02, 0x4e, 0x75,
    }};
    constexpr auto expected_hash = "a45ff5eca6e3594574b464574fa0aae3027bd2ea11472770708c96f4d21b56cc";
    if (terminal.entry_address != 0x685f4 || terminal.return_address != entry - 2
        || entry < plan.resident_stage.destination) {
        throw std::runtime_error("Unexpected Millennium Amiga post-negative-D3 terminal placement");
    }
    const auto relative = entry - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length || expected.size() > plan.resident_stage.length - relative) {
        throw std::runtime_error("Millennium Amiga post-negative-D3 terminal is outside raw range");
    }
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    if (!std::equal(expected.begin(), expected.end(), bytes.begin())
        || to_hex(sha256(bytes)) != expected_hash) {
        throw std::runtime_error("Unexpected Millennium Amiga post-negative-D3 terminal");
    }
    return {entry, {0x7b3b5, 0x7b3bc}, entry + 14, entry + 16, entry + 18,
        entry + 20, entry + 24, entry + 22, entry + 24, entry + 28, entry + 26,
        expected_hash};
}

MillenniumAmigaResidentPostNegativeD3TerminalExecution
execute_millennium_amiga_resident_post_negative_d3_terminal_prefix(
    const MillenniumAmigaResidentPostNegativeD3Terminal& terminal,
    const MillenniumAmigaResidentPostNegativeD3TerminalInput input) {
    // $685fe..$68619 is entirely local once its already hash-checked bytes
    // have been identified.  Model only its observable register/absolute-byte
    // effects; do not invent a caller, stack, or execution beyond $6861a.
    constexpr std::uint32_t entry = 0x685fe;
    if (terminal.entry_address != entry
        || terminal.absolute_byte_store_addresses != std::array<std::uint32_t, 2>{0x7b3b5, 0x7b3bc}
        || terminal.copied_d1_address != entry + 14
        || terminal.copied_d2_address != entry + 16
        || terminal.d0_test_address != entry + 18
        || terminal.nonzero_branch_address != entry + 20
        || terminal.zero_return_address != entry + 22
        || terminal.nonnegative_branch_address != entry + 24
        || terminal.nonnegative_branch_target != entry + 28
        || terminal.negative_return_address != entry + 26
        || terminal.raw_sha256
            != "a45ff5eca6e3594574b464574fa0aae3027bd2ea11472770708c96f4d21b56cc") {
        throw std::runtime_error("Detached Millennium Amiga local terminal evidence");
    }

    MillenniumAmigaResidentPostNegativeD3TerminalExecution result;
    // CLR.W D0; MOVE.B D0,$7b3b5; MOVE.B D0,$7b3bc.
    result.d0 = input.d0 & 0xffff0000U;
    result.absolute_byte_writes = {0, 0};
    // MOVE.W D1,D0; MOVE.W D2,D1.  The high words remain untouched.
    result.d0 = (result.d0 & 0xffff0000U) | (input.d1 & 0xffffU);
    result.d1 = (input.d1 & 0xffff0000U) | (input.d2 & 0xffffU);
    result.d2 = input.d2;

    const auto tested_word = static_cast<std::uint16_t>(input.d1);
    if (tested_word == 0) {
        result.stop = MillenniumAmigaResidentPostNegativeD3TerminalStop::zero_return;
        result.next_address = terminal.zero_return_address;
    } else if ((tested_word & 0x8000U) != 0) {
        result.stop = MillenniumAmigaResidentPostNegativeD3TerminalStop::negative_return;
        result.next_address = terminal.negative_return_address;
    } else {
        result.stop = MillenniumAmigaResidentPostNegativeD3TerminalStop::nonnegative_continuation_boundary;
        result.next_address = terminal.nonnegative_branch_target;
    }
    return result;
}

MillenniumAmigaResidentPostNegativeD3ContinuationBoundary
parse_millennium_amiga_resident_post_negative_d3_continuation_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentPostNegativeD3Terminal& terminal) {
    constexpr std::uint32_t entry = 0x6861a;
    constexpr std::array<std::uint8_t, 54> expected{{
        0x04, 0x42, 0x28, 0x00,             // addi.w #$2800,d2
        0x04, 0x43, 0x28, 0x00,             // addi.w #$2800,d3
        0x04, 0x41, 0x28, 0x00,             // addi.w #$2800,d1
        0x48, 0xe7, 0xf0, 0x04,             // movem.l d0-d3/a5,-(sp)
        0x3c, 0x3c, 0x7d, 0x00,             // move.w #$7d00,d6
        0x3e, 0x06,                         // move.w d6,d7
        0xdc, 0x41,                         // add.w d1,d6
        0xde, 0x42,                         // add.w d2,d7
        0xbc, 0x47,                         // cmp.w d7,d6
        0x64, 0x02,                         // bcc.s $6863a
        0xc3, 0x42,                         // exg d1,d2
        0x92, 0x42,                         // sub.w d2,d1
        0x52, 0x41,                         // addq.w #1,d1
        0xb6, 0x7c, 0x00, 0x62,             // cmp.w #$62,d3
        0x65, 0x0c,                         // bcs.s $68650
        0x6b, 0x4e,                         // bmi.s $68694
        0x4c, 0xdf, 0x20, 0x0f,             // movem.l (sp)+,d0-d3/a5
        0x4e, 0xf9, 0x00, 0x07, 0xbe, 0xf0, // jmp $7bef0
    }};
    constexpr std::string_view expected_hash =
        "d3f6b63090429e11fb3a77e4573817649e2bb7996d06811ea2751078794534ce";
    if (terminal.entry_address != 0x685fe || terminal.nonnegative_branch_target != entry
        || entry < plan.resident_stage.destination) {
        throw std::runtime_error("Unexpected Millennium Amiga post-negative-D3 continuation placement");
    }
    const auto relative = entry - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length || expected.size() > plan.resident_stage.length - relative) {
        throw std::runtime_error("Millennium Amiga post-negative-D3 continuation is outside raw range");
    }
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    const auto observed_hash = to_hex(sha256(bytes));
    if (!std::equal(expected.begin(), expected.end(), bytes.begin()) || observed_hash != expected_hash) {
        throw std::runtime_error("Unexpected Millennium Amiga post-negative-D3 continuation");
    }
    return {entry, {0x2800, 0x2800, 0x2800}, 0x7d00,
        entry + 28, entry + 32,
        entry + 40, entry + 54,
        entry + 42, entry + 122,
        entry + 48, 0x7bef0,
        plan.resident_stage.disk_offset + relative,
        static_cast<std::uint32_t>(expected.size()), observed_hash};
}

MillenniumAmigaResidentPostNegativeD3ContinuationExecution
execute_millennium_amiga_resident_post_negative_d3_continuation_prefix(
    const MillenniumAmigaResidentPostNegativeD3ContinuationBoundary& boundary,
    const MillenniumAmigaResidentPostNegativeD3ContinuationInput input) {
    constexpr auto expected_hash =
        "d3f6b63090429e11fb3a77e4573817649e2bb7996d06811ea2751078794534ce";
    if (boundary.entry_address != 0x6861a
        || boundary.add_immediates != std::array<std::uint16_t, 3>{0x2800, 0x2800, 0x2800}
        || boundary.range_base_immediate != 0x7d00
        || boundary.compare_branch_address != 0x68636
        || boundary.compare_branch_target != 0x6863a
        || boundary.low_range_branch_address != 0x68642
        || boundary.low_range_branch_target != 0x68650
        || boundary.negative_range_branch_address != 0x68644
        || boundary.negative_range_branch_target != 0x68694
        || boundary.terminal_jump_address != 0x6864a
        || boundary.terminal_jump_target != 0x7bef0
        || boundary.raw_disk_offset != 0x16a1a || boundary.byte_count != 54
        || boundary.raw_sha256 != expected_hash) {
        throw std::runtime_error("Detached Millennium Amiga post-negative-D3 continuation evidence");
    }

    const auto with_low_word = [](std::uint32_t value, std::uint16_t low) {
        return (value & 0xffff0000U) | low;
    };
    const auto add_word = [&](std::uint32_t value, std::uint16_t addend) {
        return with_low_word(value, static_cast<std::uint16_t>(value + addend));
    };

    MillenniumAmigaResidentPostNegativeD3ContinuationExecution result;
    result.d0 = input.d0;
    result.d1 = add_word(input.d1, 0x2800);
    result.d2 = add_word(input.d2, 0x2800);
    result.d3 = add_word(input.d3, 0x2800);
    result.a5 = input.a5;

    // MOVEM saves D0-D3/A5, then the original executes only word-width
    // arithmetic.  Keep upper halves exactly as supplied rather than making
    // a fabricated full-register interpretation.
    result.d6 = with_low_word(input.d6, 0x7d00);
    result.d7 = with_low_word(input.d7, static_cast<std::uint16_t>(result.d6));
    result.d6 = add_word(result.d6, static_cast<std::uint16_t>(result.d1));
    result.d7 = add_word(result.d7, static_cast<std::uint16_t>(result.d2));

    // CMP.W D7,D6 / BCC.S: unsigned low-word D6 >= D7 takes the branch,
    // skipping the EXG/SUB/ADDQ sequence but staying inside this exact span.
    if (static_cast<std::uint16_t>(result.d6) < static_cast<std::uint16_t>(result.d7)) {
        std::swap(result.d1, result.d2); // EXG D1,D2
        result.d1 = with_low_word(result.d1,
            static_cast<std::uint16_t>(result.d1 - result.d2));
        result.d1 = add_word(result.d1, 1); // ADDQ.W #1,D1
    }

    const auto d3_low = static_cast<std::uint16_t>(result.d3);
    if (d3_low < 0x0062U) {
        result.stop = MillenniumAmigaResidentPostNegativeD3ContinuationStop::low_range_branch_boundary;
        result.next_address = boundary.low_range_branch_target;
        return result;
    }
    if ((d3_low & 0x8000U) != 0) {
        result.stop = MillenniumAmigaResidentPostNegativeD3ContinuationStop::negative_range_branch_boundary;
        result.next_address = boundary.negative_range_branch_target;
        return result;
    }

    // The only route to the external JMP executes MOVEM.L (SP)+,D0-D3/A5.
    // Expose that exact restored image, but do not represent SP or enter the
    // external target.
    result.restored_registers = {input.d0, input.d1, input.d2, input.d3, input.a5};
    result.stop = MillenniumAmigaResidentPostNegativeD3ContinuationStop::external_jump_boundary;
    result.next_address = boundary.terminal_jump_target;
    return result;
}

MillenniumAmigaResidentIndependentZeroTargetBoundary
parse_millennium_amiga_resident_independent_zero_target_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentIndependentEntryGate& gate) {
    validate_range(plan.resident_stage);
    constexpr std::uint32_t entry = 0x6854a;
    constexpr std::array<std::uint8_t, 6> expected{{0xb4, 0x7c, 0x01, 0x20, 0x65, 0x12}};
    if (gate.flag_zero_target != entry || entry < plan.resident_stage.destination) {
        throw std::runtime_error("Unexpected Millennium Amiga independent zero target placement");
    }
    const auto relative = entry - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length || expected.size() > plan.resident_stage.length - relative) {
        throw std::runtime_error("Millennium Amiga independent zero target is outside raw range");
    }
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    if (!std::equal(expected.begin(), expected.end(), bytes.begin())) {
        throw std::runtime_error("Unexpected Millennium Amiga independent zero target");
    }
    return {entry, 0x0120, entry + 4, entry + 6 + 0x12};
}

MillenniumAmigaResidentIndependentCompareTargetBoundary
parse_millennium_amiga_resident_independent_compare_target_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentIndependentZeroTargetBoundary& boundary) {
    constexpr std::uint32_t entry = 0x68562;
    constexpr std::array<std::uint8_t, 10> expected{{0x3e, 0x02, 0xde, 0x41, 0xbe, 0x7c, 0x01, 0x20, 0x65, 0x0e}};
    if (boundary.conditional_branch_target != entry || entry < plan.resident_stage.destination) {
        throw std::runtime_error("Unexpected Millennium Amiga independent compare target placement");
    }
    const auto relative = entry - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length || expected.size() > plan.resident_stage.length - relative) {
        throw std::runtime_error("Millennium Amiga independent compare target is outside raw range");
    }
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    if (!std::equal(expected.begin(), expected.end(), bytes.begin())) {
        throw std::runtime_error("Unexpected Millennium Amiga independent compare target");
    }
    return {entry, entry + 8, entry + 10 + 0x0e};
}

MillenniumAmigaResidentIndependentBranchTargetBoundary
parse_millennium_amiga_resident_independent_branch_target_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentIndependentCompareTargetBoundary& boundary) {
    constexpr std::uint32_t entry = 0x6857a;
    constexpr std::array<std::uint8_t, 8> expected{{0x4a, 0x39, 0x00, 0x07, 0xb1, 0x42, 0x67, 0x04}};
    if (boundary.conditional_branch_target != entry || entry < plan.resident_stage.destination) throw std::runtime_error("Unexpected Millennium Amiga independent branch target placement");
    const auto relative = entry - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length || expected.size() > plan.resident_stage.length - relative) throw std::runtime_error("Millennium Amiga independent branch target is outside raw range");
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    if (!std::equal(expected.begin(), expected.end(), bytes.begin())) throw std::runtime_error("Unexpected Millennium Amiga independent branch target");
    return {entry, entry + 6, entry + 8 + 4};
}

MillenniumAmigaResidentIndependentBranchPreparationBoundary
parse_millennium_amiga_resident_independent_branch_preparation_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentIndependentBranchTargetBoundary& boundary) {
    constexpr std::uint32_t entry = 0x68586;
    constexpr std::array<std::uint8_t, 16> expected{{0x3e,0x02,0x02,0x47,0xff,0xf0,0xe2,0x4f,0xda,0xc7,0x4e,0xb9,0x00,0x07,0xb2,0x6a}};
    if (boundary.conditional_branch_target != entry || entry < plan.resident_stage.destination) throw std::runtime_error("Unexpected Millennium Amiga independent branch preparation placement");
    const auto relative = entry - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length || expected.size() > plan.resident_stage.length - relative) throw std::runtime_error("Millennium Amiga independent branch preparation is outside raw range");
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    if (!std::equal(expected.begin(), expected.end(), bytes.begin())) throw std::runtime_error("Unexpected Millennium Amiga independent branch preparation");
    return {entry, entry + 10, 0x7b26a};
}

MillenniumAmigaResidentIndependentPostCallTailBoundary
parse_millennium_amiga_resident_independent_post_call_tail_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentIndependentBranchPreparationBoundary& boundary) {
    // This starts at the return PC of JSR $7b26a. That return is not
    // presumed: hash-lock its call-free caller-side tail as source evidence.
    constexpr std::uint32_t entry = 0x68596;
    constexpr std::array<std::uint8_t, 104> expected{{
        0x4c,0xdf,0x20,0x0f,0x42,0x46,0x1c,0x39,0x00,0x07,0xb3,0xb0,
        0x1e,0x39,0x00,0x07,0xb3,0xb1,0xdf,0x39,0x00,0x07,0xb3,0xb4,
        0x64,0x02,0x52,0x06,0x4a,0x79,0x00,0x07,0xb3,0xb6,0x6b,0x02,
        0x44,0x46,0xd4,0x46,0x52,0x43,0xda,0xfc,0x00,0x90,0x42,0x46,
        0x1c,0x39,0x00,0x07,0xb3,0xba,0x1e,0x39,0x00,0x07,0xb3,0xbb,
        0xdf,0x39,0x00,0x07,0xb3,0xbc,0x64,0x02,0x52,0x06,0x4a,0x39,
        0x00,0x07,0xb3,0xbd,0x67,0x02,0x44,0x46,0xd2,0x46,0x6b,0x0a,
        0x53,0x40,0x67,0x06,0x4e,0xf9,0x00,0x07,0xbc,0xf8,0x06,0x42,
        0x28,0x00,0x06,0x43,0x28,0x00,0x4e,0x75,
    }};
    constexpr std::string_view expected_hash =
        "eeed978d0afd278cc48868c0d2b76205304ddfa80b174d2aac95dc50b80dd551";
    if (boundary.unknown_call_address != 0x68590 || boundary.unknown_call_target != 0x7b26a
        || entry < plan.resident_stage.destination) {
        throw std::runtime_error("Unexpected Millennium Amiga independent post-call tail placement");
    }
    const auto relative = entry - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length || expected.size() > plan.resident_stage.length - relative) {
        throw std::runtime_error("Millennium Amiga independent post-call tail is outside raw range");
    }
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    const auto hash = to_hex(sha256(bytes));
    if (!std::equal(expected.begin(), expected.end(), bytes.begin()) || hash != expected_hash) {
        throw std::runtime_error("Unexpected Millennium Amiga independent post-call tail");
    }
    return {entry, plan.resident_stage.disk_offset + relative, expected.size(), hash,
        {0x7b3b0, 0x7b3b1, 0x7b3b4, 0x7b3ba, 0x7b3bb, 0x7b3bc},
        0x685ee, 0x7bcf8, 0x685f4, 0x685fc, 0x685fe};
}

MillenniumAmigaResidentSeparateEntryGate
parse_millennium_amiga_resident_separate_entry_gate(const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan) {
    constexpr std::uint32_t entry = 0x68d50;
    constexpr std::array<std::uint8_t, 12> expected{{0x10,0x39,0x00,0x07,0xc2,0x4f,0x4a,0x00,0x6a,0x00,0x00,0x06}};
    if (entry < plan.resident_stage.destination) throw std::runtime_error("Millennium Amiga separate entry precedes raw range");
    const auto relative = entry - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length || expected.size() > plan.resident_stage.length - relative) throw std::runtime_error("Millennium Amiga separate entry is outside raw range");
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    if (!std::equal(expected.begin(), expected.end(), bytes.begin())) throw std::runtime_error("Unexpected Millennium Amiga separate entry gate");
    return {entry, entry + 8, entry + 12 + 6};
}

MillenniumAmigaResidentSeparateBranchBoundary
parse_millennium_amiga_resident_separate_branch_boundary(
    const AmigaAdf& disk,
    const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentSeparateEntryGate& gate) {
    constexpr std::uint32_t entry = 0x68d62;
    constexpr std::array<std::uint8_t, 32> expected{{
        0x42, 0x42, 0x14, 0x39, 0x00, 0x07, 0xc2, 0x4e,
        0x44, 0x02, 0x4a, 0x02, 0x6a, 0x00, 0x00, 0x06,
        0x14, 0x3c, 0x00, 0x02, 0xd0, 0x42, 0x06, 0x40,
        0x01, 0x90, 0x4e, 0xb9, 0x00, 0x07, 0x78, 0xf0,
    }};
    if (gate.branch_target != entry || entry < plan.resident_stage.destination) {
        throw std::runtime_error("Unexpected Millennium Amiga separate branch placement");
    }
    const auto relative = entry - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length
        || expected.size() > plan.resident_stage.length - relative) {
        throw std::runtime_error("Millennium Amiga separate branch outside raw range");
    }
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    if (!std::equal(expected.begin(), expected.end(), bytes.begin())) {
        throw std::runtime_error("Unexpected Millennium Amiga separate branch");
    }
    return {entry, entry + 12, entry + 16 + 6, entry + 26, 0x778f0};
}

MillenniumAmigaResidentSeparatePostCallBoundary parse_millennium_amiga_resident_separate_post_call_boundary(const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan, const MillenniumAmigaResidentSeparateBranchBoundary& branch) {
    constexpr std::uint32_t entry = 0x68d82, next_target = 0x7b342;
    constexpr std::array<std::uint8_t, 26> expected{{0x30,0x3c,0x22,0x08,0x2a,0x79,0x00,0x06,0x93,0x4e,0xda,0xc0,0x20,0x15,0x23,0xc0,0x00,0x07,0xc2,0x56,0x4e,0xb9,0x00,0x07,0xb3,0x42}};
    constexpr std::string_view expected_hash = "e49e750f78946956c22d4cd80206139d38808d4ecb3b1579906aeaede0db7b77";
    constexpr std::string_view target_hash = "731d016983d29dcb23abad28f3f0f225bd3708073e8c0c8481a97a50b460cdcf";
    if (branch.unknown_call_address != 0x68d7c || branch.unknown_call_target != 0x778f0 || entry < plan.resident_stage.destination || next_target < plan.resident_stage.destination) throw std::runtime_error("Unexpected Millennium Amiga separate post-call placement");
    const auto relative = entry - plan.resident_stage.destination, target_relative = next_target - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length || expected.size() > plan.resident_stage.length - relative || target_relative > plan.resident_stage.length || 32U > plan.resident_stage.length - target_relative) throw std::runtime_error("Millennium Amiga separate post-call is outside raw range");
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size()), target = disk.bytes(plan.resident_stage.disk_offset + target_relative, 32);
    const auto hash = to_hex(sha256(bytes)), target_prefix_hash = to_hex(sha256(target));
    if (!std::equal(expected.begin(), expected.end(), bytes.begin()) || hash != expected_hash || target_prefix_hash != target_hash) throw std::runtime_error("Unexpected Millennium Amiga separate post-call boundary");
    return {entry, plan.resident_stage.disk_offset + relative, hash, 0x2208, 0x6934e, 0x7c256, entry + 20, next_target, plan.resident_stage.disk_offset + target_relative, target_prefix_hash};
}

MillenniumAmigaResidentSeparatePostCallTailBoundary
parse_millennium_amiga_resident_separate_post_call_tail_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentSeparatePostCallBoundary& boundary) {
    constexpr std::uint32_t entry = 0x68d9c;
    constexpr std::array<std::uint8_t, 36> expected{{
        0x4e, 0xb9, 0x00, 0x07, 0xdb, 0xa8,
        0x4e, 0xb9, 0x00, 0x07, 0xd8, 0xa8,
        0x4e, 0xb9, 0x00, 0x07, 0xd4, 0x80,
        0x4e, 0xb9, 0x00, 0x07, 0xb5, 0x94,
        0x4e, 0xb9, 0x00, 0x07, 0xd5, 0xc8,
        0x4e, 0xb9, 0x00, 0x07, 0xb3, 0x6c,
    }};
    constexpr std::array<std::uint32_t, 6> targets{{
        0x7dba8, 0x7d8a8, 0x7d480, 0x7b594, 0x7d5c8, 0x7b36c,
    }};
    constexpr std::array<std::string_view, 6> target_hashes{{
        "b388a3622caeeccac01d793650e63e192de821abc789ca334b6ba00a1475ca34",
        "819055da14479352b3f672e6db10424bdebb90230350b0e8088eb0cb0acbd087",
        "dbb41359b827129e186a7cf2f4d79c7f45f11f4cbe53e964a0633b7ee7070df5",
        "e9aa8c8f766b3486163339990968f9829d29b69c3c991ed2a7fc71c483d16846",
        "de1fdcc69a46a7f661c191fa69cd64a693053f4026708400ca4bc6defe224c79",
        "cbe69ef816a594b6e9c0e8a27d5cacc660920df3a0aebe9a31849c113a3f909f",
    }};
    constexpr std::string_view expected_hash =
        "08c660de1ed6d0b0f535e451c84450397383a923a1808fa9678d3ae85a8cc17b";
    if (boundary.following_call_address != 0x68d96
        || boundary.following_call_target != 0x7b342
        || entry < plan.resident_stage.destination) {
        throw std::runtime_error("Unexpected Millennium Amiga separate post-call tail placement");
    }
    const auto relative = entry - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length || expected.size() > plan.resident_stage.length - relative) {
        throw std::runtime_error("Millennium Amiga separate post-call tail is outside raw range");
    }
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    const auto hash = to_hex(sha256(bytes));
    if (!std::equal(expected.begin(), expected.end(), bytes.begin()) || hash != expected_hash) {
        throw std::runtime_error("Unexpected Millennium Amiga separate post-call tail");
    }
    MillenniumAmigaResidentSeparatePostCallTailBoundary result{
        .entry_address = entry,
        .raw_disk_offset = plan.resident_stage.disk_offset + relative,
        .byte_count = expected.size(),
        .sha256 = hash,
        .call_addresses = {entry, entry + 6, entry + 12, entry + 18, entry + 24, entry + 30},
        .call_targets = targets,
    };
    for (std::size_t index = 0; index < targets.size(); ++index) {
        if (targets[index] < plan.resident_stage.destination) {
            throw std::runtime_error("Millennium Amiga separate post-call tail target precedes raw range");
        }
        const auto target_relative = targets[index] - plan.resident_stage.destination;
        if (target_relative > plan.resident_stage.length
            || 32U > plan.resident_stage.length - target_relative) {
            throw std::runtime_error("Millennium Amiga separate post-call tail target is outside raw range");
        }
        const auto prefix = disk.bytes(plan.resident_stage.disk_offset + target_relative, 32);
        const auto prefix_hash = to_hex(sha256(prefix));
        if (prefix_hash != target_hashes[index]) {
            throw std::runtime_error("Unexpected Millennium Amiga separate post-call tail target");
        }
        result.target_raw_disk_offsets[index] = plan.resident_stage.disk_offset + target_relative;
        result.target_prefix_sha256[index] = prefix_hash;
    }
    return result;
}

MillenniumAmigaResidentSeparatePostCallTailBranchBoundary
parse_millennium_amiga_resident_separate_post_call_tail_branch_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentSeparatePostCallTailBoundary& tail) {
    constexpr std::uint32_t entry = 0x68dc0, target = 0x68dec;
    constexpr std::array<std::uint8_t, 14> expected{{
        0x10, 0x39, 0x00, 0x07, 0xc2, 0x55, 0xb0, 0x3c, 0x00, 0x0c,
        0x65, 0x00, 0x00, 0x20,
    }};
    constexpr std::array<std::uint8_t, 32> target_prefix{{
        0x2a, 0x7c, 0x00, 0x07, 0xc2, 0x5c, 0x54, 0x8d,
        0x30, 0x3c, 0x00, 0x02, 0x4a, 0x2d, 0x00, 0x0d,
        0x66, 0x00, 0x00, 0x08, 0x4e, 0xf9, 0x00, 0x07,
        0xc4, 0x68, 0x4e, 0xb9, 0x00, 0x07, 0xd3, 0xe0,
    }};
    constexpr std::string_view expected_hash =
        "ef2fe6161118a1b0ac6cee838be9a4dc2b0483ba274a213d3ac653ea6f334e3b";
    constexpr std::string_view target_hash =
        "13ed782f5463fd93bbd4376777a1c01d8fd636018de8aef52f5710eb0da11a2b";
    if (tail.entry_address != 0x68d9c || tail.byte_count != 36
        || entry < plan.resident_stage.destination || target < plan.resident_stage.destination) {
        throw std::runtime_error("Unexpected Millennium Amiga separate post-call tail branch placement");
    }
    const auto relative = entry - plan.resident_stage.destination;
    const auto target_relative = target - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length || expected.size() > plan.resident_stage.length - relative
        || target_relative > plan.resident_stage.length
        || target_prefix.size() > plan.resident_stage.length - target_relative) {
        throw std::runtime_error("Millennium Amiga separate post-call tail branch is outside raw range");
    }
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    const auto prefix = disk.bytes(plan.resident_stage.disk_offset + target_relative, target_prefix.size());
    const auto hash = to_hex(sha256(bytes));
    const auto prefix_hash = to_hex(sha256(prefix));
    if (!std::equal(expected.begin(), expected.end(), bytes.begin())
        || !std::equal(target_prefix.begin(), target_prefix.end(), prefix.begin())
        || hash != expected_hash || prefix_hash != target_hash) {
        throw std::runtime_error("Unexpected Millennium Amiga separate post-call tail branch");
    }
    // BCS.W has its displacement base at the extension word: $68dcc + $20.
    return {entry, plan.resident_stage.disk_offset + relative, hash, 0x7c255, 0x0c,
        entry + 10, target, plan.resident_stage.disk_offset + target_relative, prefix_hash};
}

MillenniumAmigaResidentSeparateComparisonBoundary
parse_millennium_amiga_resident_separate_comparison_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentSeparatePostCallTailBranchBoundary& boundary) {
    constexpr std::uint32_t entry = 0x68e6c, continuation = 0x68e90;
    constexpr std::array<std::uint8_t, 4> preceding_branch{{0x66, 0x00, 0x00, 0x5e}};
    constexpr std::array<std::uint8_t, 36> expected{{
        0x42, 0x40, 0x42, 0x41, 0xb6, 0x3c, 0x00, 0x08,
        0x67, 0x00, 0x00, 0x0a, 0x65, 0x00, 0x00, 0x04,
        0x55, 0x01, 0x52, 0x01, 0xb4, 0x3c, 0x00, 0x08,
        0x67, 0x00, 0x00, 0x0a, 0x65, 0x00, 0x00, 0x04,
        0x55, 0x00, 0x52, 0x00,
    }};
    constexpr std::string_view expected_hash =
        "8cb29601f0c76406930e37d44b29853501857c36f3cb833ccdd32e78418597d4";
    constexpr std::string_view continuation_hash =
        "8a81ad1a39efe0442addd9302b3b0e5e0c0bd72ecaf5904d2fa5e1c2834cd964";
    if (boundary.conditional_branch_target != 0x68dec || entry < plan.resident_stage.destination
        || continuation < plan.resident_stage.destination) {
        throw std::runtime_error("Unexpected Millennium Amiga comparison boundary placement");
    }
    constexpr std::uint32_t preceding_address = 0x68e0c;
    const auto relative = entry - plan.resident_stage.destination;
    const auto preceding_relative = preceding_address - plan.resident_stage.destination;
    const auto continuation_relative = continuation - plan.resident_stage.destination;
    if (expected.size() > plan.resident_stage.length - relative
        || preceding_branch.size() > plan.resident_stage.length - preceding_relative
        || 32U > plan.resident_stage.length - continuation_relative) {
        throw std::runtime_error("Millennium Amiga comparison boundary is outside raw range");
    }
    const auto source = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    const auto preceding = disk.bytes(plan.resident_stage.disk_offset + preceding_relative,
        preceding_branch.size());
    const auto prefix = disk.bytes(plan.resident_stage.disk_offset + continuation_relative, 32);
    const auto hash = to_hex(sha256(source));
    const auto prefix_hash = to_hex(sha256(prefix));
    if (!std::equal(expected.begin(), expected.end(), source.begin())
        || !std::equal(preceding_branch.begin(), preceding_branch.end(), preceding.begin())
        || hash != expected_hash || prefix_hash != continuation_hash) {
        throw std::runtime_error("Unexpected Millennium Amiga comparison boundary");
    }
    // The preceding BNE.W displacement is based at extension word $68e0e.
    return {entry, plan.resident_stage.disk_offset + relative, hash, preceding_address, entry,
        {entry + 8, entry + 12, entry + 24, entry + 28},
        {0x68e80, 0x68e7e, 0x68e90, 0x68e8e},
        plan.resident_stage.disk_offset + continuation_relative, prefix_hash};
}

MillenniumAmigaResidentSeparateByteGateBoundary
parse_millennium_amiga_resident_separate_byte_gate_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentSeparateComparisonBoundary& boundary) {
    constexpr std::uint32_t entry = 0x68e90, target = 0x68ed6, fallthrough = 0x68eb2;
    constexpr std::array<std::uint8_t, 34> expected{{
        0x2f,0x00,0x2f,0x01,0x36,0x3c,0x00,0x08,0x34,0x3c,0x00,0x08,
        0x22,0x1f,0x20,0x1f,0x1e,0x39,0x00,0x07,0xc2,0x4e,0x13,0xc0,
        0x00,0x07,0xc2,0x4e,0xbe,0x00,0x67,0x00,0x00,0x26,
    }};
    constexpr std::string_view expected_hash = "f4a047914e83ab873a037ea16a4f5aaa9a402c38f48a525efc69d9e49cca15a8";
    constexpr std::string_view target_hash = "79871297097662cd29a3659d5399a17c847a8c46d6753e1d968cb27b83c5210b";
    constexpr std::string_view fallthrough_hash = "cd83cab5400642c141e3252fd28302a94e7169d1f5bc7a6021cbe78c5daacd02";
    if (boundary.continuation_raw_disk_offset != 0x17290) throw std::runtime_error("Unexpected Millennium Amiga byte gate placement");
    const auto relative = entry - plan.resident_stage.destination;
    const auto target_relative = target - plan.resident_stage.destination;
    const auto fallthrough_relative = fallthrough - plan.resident_stage.destination;
    if (expected.size() > plan.resident_stage.length - relative || 32U > plan.resident_stage.length - target_relative || 32U > plan.resident_stage.length - fallthrough_relative) throw std::runtime_error("Millennium Amiga byte gate outside raw range");
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    const auto target_bytes = disk.bytes(plan.resident_stage.disk_offset + target_relative, 32);
    const auto fallthrough_bytes = disk.bytes(plan.resident_stage.disk_offset + fallthrough_relative, 32);
    const auto hash = to_hex(sha256(bytes));
    const auto target_digest = to_hex(sha256(target_bytes));
    const auto fallthrough_digest = to_hex(sha256(fallthrough_bytes));
    if (!std::equal(expected.begin(), expected.end(), bytes.begin()) || hash != expected_hash || target_digest != target_hash || fallthrough_digest != fallthrough_hash) throw std::runtime_error("Unexpected Millennium Amiga byte gate");
    return {entry, plan.resident_stage.disk_offset + relative, hash, 0x7c24e, 0x68eae, target,
        plan.resident_stage.disk_offset + target_relative, target_digest,
        plan.resident_stage.disk_offset + fallthrough_relative, fallthrough_digest};
}

MillenniumAmigaResidentSeparateByteGateTargetBoundary
parse_millennium_amiga_resident_separate_byte_gate_target_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentSeparateByteGateBoundary& boundary) {
    constexpr std::uint32_t entry = 0x68ed6;
    constexpr std::uint32_t convergence = 0x68ef4;
    constexpr std::array<std::uint8_t, 30> expected{{
        0x06, 0x39, 0x00, 0x80, 0x00, 0x07, 0xc2, 0x53, 0x64, 0x00,
        0x00, 0x14, 0x0c, 0x39, 0x00, 0x18, 0x00, 0x07, 0xc2, 0x51,
        0x64, 0x00, 0x00, 0x08, 0x58, 0x39, 0x00, 0x07, 0xc2, 0x51,
    }};
    constexpr std::array<std::uint8_t, 32> convergence_prefix{{
        0x1e, 0x39, 0x00, 0x07, 0xc2, 0x4f, 0x13, 0xc1,
        0x00, 0x07, 0xc2, 0x4f, 0xbe, 0x01, 0x67, 0x00,
        0x00, 0x26, 0x13, 0xfc, 0x00, 0x02, 0x00, 0x07,
        0xc2, 0x50, 0x13, 0xfc, 0x00, 0x00, 0x00, 0x07,
    }};
    constexpr std::string_view expected_hash =
        "b2d2c6cadc50725eb8b4f0b680c325586ed457b29232481b503f3e337d589341";
    constexpr std::string_view convergence_hash =
        "93b0d20954d235c624406450161a359968e4f1baefcbaeb47ede08fda0cd1e71";
    if (boundary.conditional_branch_target != entry || entry < plan.resident_stage.destination
        || convergence < plan.resident_stage.destination) {
        throw std::runtime_error("Unexpected Millennium Amiga byte gate target placement");
    }
    const auto relative = entry - plan.resident_stage.destination;
    const auto convergence_relative = convergence - plan.resident_stage.destination;
    if (expected.size() > plan.resident_stage.length - relative
        || convergence_prefix.size() > plan.resident_stage.length - convergence_relative) {
        throw std::runtime_error("Millennium Amiga byte gate target is outside raw range");
    }
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    const auto next = disk.bytes(plan.resident_stage.disk_offset + convergence_relative,
        convergence_prefix.size());
    const auto hash = to_hex(sha256(bytes));
    const auto next_hash = to_hex(sha256(next));
    if (!std::equal(expected.begin(), expected.end(), bytes.begin())
        || !std::equal(convergence_prefix.begin(), convergence_prefix.end(), next.begin())
        || hash != expected_hash || next_hash != convergence_hash) {
        throw std::runtime_error("Unexpected Millennium Amiga byte gate target");
    }
    // Both BCC.W displacements are based at their extension words ($68ee0
    // and $68eec); the ADDQ.B falls straight through to the same address.
    return {entry, plan.resident_stage.disk_offset + relative, hash,
        {0x68ede, 0x68eea}, {convergence, convergence}, convergence,
        plan.resident_stage.disk_offset + convergence_relative, next_hash};
}

MillenniumAmigaResidentSeparateByteGateConvergenceBoundary
parse_millennium_amiga_resident_separate_byte_gate_convergence_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentSeparateByteGateTargetBoundary& boundary) {
    constexpr std::uint32_t entry = 0x68ef4;
    constexpr std::uint32_t target = 0x68f2a;
    constexpr std::uint32_t fallthrough = 0x68f06;
    constexpr std::array<std::uint8_t, 34> expected{{
        0x1e, 0x39, 0x00, 0x07, 0xc2, 0x4f, 0x13, 0xc1, 0x00, 0x07,
        0xc2, 0x4f, 0xbe, 0x01, 0x67, 0x00, 0x00, 0x26, 0x13, 0xfc,
        0x00, 0x02, 0x00, 0x07, 0xc2, 0x50, 0x13, 0xfc, 0x00, 0x00,
        0x00, 0x07, 0xc2, 0x52,
    }};
    constexpr std::array<std::uint8_t, 32> target_prefix{{
        0x06, 0x39, 0x00, 0x60, 0x00, 0x07, 0xc2, 0x52,
        0x64, 0x00, 0x00, 0x14, 0x0c, 0x39, 0x00, 0x10,
        0x00, 0x07, 0xc2, 0x50, 0x64, 0x00, 0x00, 0x08,
        0x54, 0x39, 0x00, 0x07, 0xc2, 0x50, 0x4e, 0xb9,
    }};
    constexpr std::array<std::uint8_t, 32> fallthrough_prefix{{
        0x13, 0xfc, 0x00, 0x02, 0x00, 0x07, 0xc2, 0x50,
        0x13, 0xfc, 0x00, 0x00, 0x00, 0x07, 0xc2, 0x52,
        0xbe, 0x3c, 0x00, 0x00, 0x67, 0x00, 0x00, 0x2c,
        0x13, 0xfc, 0x00, 0x00, 0x00, 0x07, 0xc2, 0x4f,
    }};
    constexpr std::string_view expected_hash =
        "d63b2de78fbc18f2a4213206d1f05947a604dafc5b23fea56f87b624cb7549ab";
    constexpr std::string_view target_hash =
        "ba2a0127999eb628ef05008867728fd31952c6d4b268bdb38f35130bab9973ae";
    constexpr std::string_view fallthrough_hash =
        "5b3ae299a769dcca25b96b3b588ab65b1c44843abf0ef1288a1a74741dec9993";
    if (boundary.convergence_address != entry || entry < plan.resident_stage.destination
        || target < plan.resident_stage.destination || fallthrough < plan.resident_stage.destination) {
        throw std::runtime_error("Unexpected Millennium Amiga byte gate convergence placement");
    }
    const auto relative = entry - plan.resident_stage.destination;
    const auto target_relative = target - plan.resident_stage.destination;
    const auto fallthrough_relative = fallthrough - plan.resident_stage.destination;
    if (expected.size() > plan.resident_stage.length - relative
        || target_prefix.size() > plan.resident_stage.length - target_relative
        || fallthrough_prefix.size() > plan.resident_stage.length - fallthrough_relative) {
        throw std::runtime_error("Millennium Amiga byte gate convergence is outside raw range");
    }
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    const auto target_bytes = disk.bytes(plan.resident_stage.disk_offset + target_relative,
        target_prefix.size());
    const auto fallthrough_bytes = disk.bytes(plan.resident_stage.disk_offset + fallthrough_relative,
        fallthrough_prefix.size());
    const auto hash = to_hex(sha256(bytes));
    const auto target_digest = to_hex(sha256(target_bytes));
    const auto fallthrough_digest = to_hex(sha256(fallthrough_bytes));
    if (!std::equal(expected.begin(), expected.end(), bytes.begin())
        || !std::equal(target_prefix.begin(), target_prefix.end(), target_bytes.begin())
        || !std::equal(fallthrough_prefix.begin(), fallthrough_prefix.end(), fallthrough_bytes.begin())
        || hash != expected_hash || target_digest != target_hash || fallthrough_digest != fallthrough_hash) {
        throw std::runtime_error("Unexpected Millennium Amiga byte gate convergence");
    }
    // BEQ.W's displacement base is the extension word at $68f04.
    return {entry, plan.resident_stage.disk_offset + relative, hash, 0x68f02, target,
        plan.resident_stage.disk_offset + target_relative, target_digest,
        plan.resident_stage.disk_offset + fallthrough_relative, fallthrough_digest};
}

MillenniumAmigaResidentSeparateByteGateTakenBranchBoundary
parse_millennium_amiga_resident_separate_byte_gate_taken_branch_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentSeparateByteGateConvergenceBoundary& boundary) {
    constexpr std::uint32_t entry = 0x68f2a;
    constexpr std::uint32_t convergence = 0x68f48;
    constexpr std::uint32_t external_call_target = 0x7caa6;
    constexpr std::array<std::uint8_t, 36> expected{{
        0x06, 0x39, 0x00, 0x60, 0x00, 0x07, 0xc2, 0x52,
        0x64, 0x00, 0x00, 0x14, 0x0c, 0x39, 0x00, 0x10,
        0x00, 0x07, 0xc2, 0x50, 0x64, 0x00, 0x00, 0x08,
        0x54, 0x39, 0x00, 0x07, 0xc2, 0x50, 0x4e, 0xb9,
        0x00, 0x07, 0xca, 0xa6,
    }};
    constexpr std::array<std::uint8_t, 18> external_prefix{{
        0x4e, 0xb9, 0x00, 0x07, 0xca, 0xa6, 0x4e, 0xb9,
        0x00, 0x07, 0xd6, 0xd2, 0x28, 0x7c, 0x00, 0x07,
        0xc2, 0x1b,
    }};
    constexpr std::string_view expected_hash =
        "a7f4be625a6a39615f0ace12a1a8e013b781575625858b4f0c257d171b0947f3";
    constexpr std::string_view external_prefix_hash =
        "dde319f5e57db52df300956d4e3e59dc6dc7967f0ff582674d502109fcfa2f69";
    if (boundary.conditional_branch_target != entry || entry < plan.resident_stage.destination
        || convergence < plan.resident_stage.destination) {
        throw std::runtime_error("Unexpected Millennium Amiga taken branch placement");
    }
    const auto relative = entry - plan.resident_stage.destination;
    const auto convergence_relative = convergence - plan.resident_stage.destination;
    if (expected.size() > plan.resident_stage.length - relative
        || external_prefix.size() > plan.resident_stage.length - convergence_relative) {
        throw std::runtime_error("Millennium Amiga taken branch is outside raw range");
    }
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    const auto call_prefix = disk.bytes(plan.resident_stage.disk_offset + convergence_relative,
        external_prefix.size());
    const auto hash = to_hex(sha256(bytes));
    const auto call_prefix_hash = to_hex(sha256(call_prefix));
    if (!std::equal(expected.begin(), expected.end(), bytes.begin())
        || !std::equal(external_prefix.begin(), external_prefix.end(), call_prefix.begin())
        || hash != expected_hash || call_prefix_hash != external_prefix_hash) {
        throw std::runtime_error("Unexpected Millennium Amiga taken branch");
    }
    // Each BCC.W displacement is relative to its extension word; the final
    // JSR is recorded as a raw external boundary and is not invoked.
    return {entry, plan.resident_stage.disk_offset + relative, hash,
        {0x68f32, 0x68f3e}, {convergence, convergence}, convergence,
        convergence, external_call_target, plan.resident_stage.disk_offset + convergence_relative,
        call_prefix_hash};
}

MillenniumAmigaResidentSeparateByteGateFallthroughBoundary
parse_millennium_amiga_resident_separate_byte_gate_fallthrough_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentSeparateByteGateConvergenceBoundary& boundary) {
    constexpr std::uint32_t entry = 0x68f06;
    constexpr std::uint32_t other_path = 0x68f1e;
    constexpr std::uint32_t convergence = 0x68f48;
    constexpr std::array<std::uint8_t, 24> expected{{
        0x13, 0xfc, 0x00, 0x02, 0x00, 0x07, 0xc2, 0x50,
        0x13, 0xfc, 0x00, 0x00, 0x00, 0x07, 0xc2, 0x52,
        0xbe, 0x3c, 0x00, 0x00, 0x67, 0x00, 0x00, 0x2c,
    }};
    constexpr std::array<std::uint8_t, 12> other_path_bytes{{
        0x13, 0xfc, 0x00, 0x00, 0x00, 0x07, 0xc2, 0x4f,
        0x60, 0x00, 0x00, 0x20,
    }};
    constexpr std::string_view expected_hash =
        "4a50d1c5f71ada9a3571e09b00437c51037c3949ff8e57a4b153ea032828d061";
    constexpr std::string_view other_path_hash =
        "fc1fca692a8fc07b5fd7c502ae2d772eeff63c0c3d33d298f9c4fac414f337da";
    if (boundary.fallthrough_raw_disk_offset != 0x17306
        || entry < plan.resident_stage.destination || other_path < plan.resident_stage.destination) {
        throw std::runtime_error("Unexpected Millennium Amiga byte gate fallthrough placement");
    }
    const auto relative = entry - plan.resident_stage.destination;
    const auto other_relative = other_path - plan.resident_stage.destination;
    if (expected.size() > plan.resident_stage.length - relative
        || other_path_bytes.size() > plan.resident_stage.length - other_relative) {
        throw std::runtime_error("Millennium Amiga byte gate fallthrough is outside raw range");
    }
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    const auto other = disk.bytes(plan.resident_stage.disk_offset + other_relative,
        other_path_bytes.size());
    const auto hash = to_hex(sha256(bytes));
    const auto other_hash = to_hex(sha256(other));
    if (!std::equal(expected.begin(), expected.end(), bytes.begin())
        || !std::equal(other_path_bytes.begin(), other_path_bytes.end(), other.begin())
        || hash != expected_hash || other_hash != other_path_hash) {
        throw std::runtime_error("Unexpected Millennium Amiga byte gate fallthrough");
    }
    // BEQ.W and BRA.W both take their displacement from their extension words.
    return {entry, plan.resident_stage.disk_offset + relative, hash, 0x68f1a, convergence,
        other_path, other_hash, 0x68f26, convergence, convergence};
}

MillenniumAmigaResidentSeparatePostExternalCallBoundary
parse_millennium_amiga_resident_separate_post_external_call_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentSeparateByteGateTakenBranchBoundary& boundary) {
    constexpr std::uint32_t entry = 0x68f4e;
    constexpr std::array<std::uint8_t, 42> expected{{
        0x4e, 0xb9, 0x00, 0x07, 0xd6, 0xd2, // jsr $7d6d2
        0x28, 0x7c, 0x00, 0x07, 0xc2, 0x1b, // movea.l #$7c21b,a4
        0x4e, 0xb9, 0x00, 0x07, 0x78, 0x0a, // jsr $7780a
        0x2a, 0x7c, 0x00, 0x07, 0xc2, 0x5c, // movea.l #$7c25c,a5
        0x54, 0x8d,                         // addq.l #2,(a5)
        0x30, 0x2d, 0x00, 0x10,             // move.w $10(a5),d0
        0x4e, 0xb9, 0x00, 0x07, 0x7b, 0x34, // jsr $77b34
        0x4e, 0xf9, 0x00, 0x07, 0xc5, 0x4e, // jmp $7c54e
    }};
    constexpr std::array<std::uint32_t, 3> call_targets{{0x7d6d2, 0x7780a, 0x77b34}};
    constexpr std::array<std::uint32_t, 3> call_addresses{{0x68f4e, 0x68f5a, 0x68f6c}};
    constexpr std::array<std::size_t, 3> target_offsets{{0x2bad2, 0x25c0a, 0x25f34}};
    constexpr std::array<std::string_view, 3> target_hashes{{
        "4e2f8f40d56a7d2a46f654be0fe5df4edaf4ca6d3d0864cc2c6d41355fa8c5b4",
        "dc67f3a81c04fbfb92bfdf7a8b88679dc07e3f61e90708198467ce3877ab5beb",
        "cfe704f22abb52092c496fdd49802da1d0a461f95474889a35c259cd47ca42c8",
    }};
    constexpr std::uint32_t terminal_target = 0x7c54e;
    constexpr std::size_t terminal_target_offset = 0x2a94e;
    constexpr std::string_view expected_hash =
        "3220d65f197163401c649a36d756ecf3005d2f342b81de5a7d4528f9a45da851";
    constexpr std::string_view terminal_target_hash =
        "502069bdbda2f35899d16237fd1d2aa477be20f0c950231fb71f32583f23de14";
    if (boundary.external_call_address != 0x68f48 || boundary.external_call_target != 0x7caa6
        || entry < plan.resident_stage.destination) {
        throw std::runtime_error("Unexpected Millennium Amiga post-external-call placement");
    }
    const auto relative = entry - plan.resident_stage.destination;
    if (expected.size() > plan.resident_stage.length - relative) {
        throw std::runtime_error("Millennium Amiga post-external-call boundary is outside raw range");
    }
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    const auto hash = to_hex(sha256(bytes));
    std::array<std::string, 3> observed_hashes{};
    for (std::size_t index = 0; index < target_offsets.size(); ++index) {
        if (target_offsets[index] > AmigaAdf::standard_size
            || 32U > AmigaAdf::standard_size - target_offsets[index]) {
            throw std::runtime_error("Millennium Amiga post-external-call target is outside ADF");
        }
        observed_hashes[index] = to_hex(sha256(disk.bytes(target_offsets[index], 32)));
        if (observed_hashes[index] != target_hashes[index]) {
            throw std::runtime_error("Unexpected Millennium Amiga post-external-call target");
        }
    }
    const auto observed_terminal_hash = to_hex(sha256(disk.bytes(terminal_target_offset, 32)));
    if (!std::equal(expected.begin(), expected.end(), bytes.begin()) || hash != expected_hash
        || observed_terminal_hash != terminal_target_hash) {
        throw std::runtime_error("Unexpected Millennium Amiga post-external-call boundary");
    }
    return {entry, plan.resident_stage.disk_offset + relative, expected.size(), hash,
        call_addresses, call_targets, target_offsets, observed_hashes, {0x7c21b, 0x7c25c},
        0x68f72, terminal_target, terminal_target_offset, observed_terminal_hash};
}

MillenniumAmigaResidentSeparateTerminalJumpRawTargetBoundary
parse_millennium_amiga_resident_separate_terminal_jump_raw_target_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentSeparatePostExternalCallBoundary& boundary) {
    // The preceding code only conditionally reaches this JMP after unknown
    // calls.  Its target lies in the source resident range, but that range is
    // loader-transformed before any runtime interpretation.  Preserve a
    // larger source fingerprint without treating it as decoded code.
    constexpr std::uint32_t jump_address = 0x68f72;
    constexpr std::uint32_t target_address = 0x7c54e;
    constexpr std::size_t raw_disk_offset = 0x2a94e;
    constexpr std::size_t byte_count = 256;
    constexpr std::string_view expected_hash =
        "0149a457e657e18805ff61675e80741fa78d25f201f120498193315804b87eea";
    if (boundary.terminal_jump_address != jump_address
        || boundary.terminal_jump_target != target_address
        || boundary.terminal_jump_target_raw_disk_offset != raw_disk_offset
        || raw_disk_offset < plan.resident_stage.disk_offset
        || raw_disk_offset > plan.resident_stage.disk_offset + plan.resident_stage.length
        || byte_count > plan.resident_stage.disk_offset + plan.resident_stage.length - raw_disk_offset) {
        throw std::runtime_error("Unexpected Millennium Amiga terminal jump raw target placement");
    }
    const auto source = disk.bytes(raw_disk_offset, byte_count);
    const auto hash = to_hex(sha256(source));
    if (hash != expected_hash) {
        throw std::runtime_error("Unexpected Millennium Amiga terminal jump raw target");
    }
    return {jump_address, target_address, raw_disk_offset, byte_count, hash};
}

} // namespace eon
