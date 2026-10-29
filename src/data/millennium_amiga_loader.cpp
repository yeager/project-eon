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

template <std::size_t Size>
struct ExecutableByteAnchor {
    std::string_view sha256;
    static constexpr std::size_t size() { return Size; }
    bool matches(const std::span<const std::uint8_t> bytes) const {
        return bytes.size() == Size && to_hex(eon::sha256(bytes)) == sha256;
    }
};

template <std::size_t Size>
std::size_t find_anchor_offset(const std::span<const std::uint8_t> bytes,
                               const ExecutableByteAnchor<Size>& anchor) {
    for (std::size_t offset = 0; offset + Size <= bytes.size(); ++offset) {
        if (anchor.matches(bytes.subspan(offset, Size))) return offset;
    }
    return bytes.size();
}

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
    constexpr ExecutableByteAnchor<14> first_prefix{"9edcf914ca28b87d29e6c6a8d4d2e57fcbb28008fe3081802e6a6d59c6300dba"};
    const auto loader = disk.bytes(bootstrap_disk_offset, bootstrap_length);
    const auto first_offset = find_anchor_offset(loader, first_prefix);
    if (first_offset == loader.size()) throw std::runtime_error("Millennium Amiga first-stage read not found");
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
    constexpr ExecutableByteAnchor<14> resident_prefix{"c3439bfa2141982ad0212ad347e34e68144aac20238f2c70495f0689edba36ba"};
    const auto resident_offset = find_anchor_offset(loader, resident_prefix);
    if (resident_offset == loader.size()) throw std::runtime_error("Millennium Amiga resident-stage read not found");
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
    constexpr std::size_t expected_size = 132;
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
        || expected_size > plan.bootstrap_loader.disk_offset + plan.bootstrap_loader.length - raw_disk_offset) {
        throw std::runtime_error("Millennium Amiga opaque invocation boundary outside bootstrap loader");
    }
    const auto bytes = disk.bytes(raw_disk_offset, expected_size);
    const auto hash = to_hex(sha256(bytes));
    if (hash != expected_hash) {
        throw std::runtime_error("Unexpected Millennium Amiga opaque invocation boundary");
    }
    return {entry_address, raw_disk_offset, expected_size, hash, first_invocation,
        plan.first_stage.destination, static_post_first, resident_jump,
        plan.resident_stage.destination};
}

MillenniumAmigaFirstStageEntryBoundary
parse_millennium_amiga_first_stage_entry_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan) {
    constexpr std::size_t source_offset = 0x6e000;
    constexpr std::size_t source_size = 0x24200;
    constexpr std::uint32_t destination = 0x41000;
    constexpr std::size_t entry_size = 0x151e;
    constexpr std::string_view source_hash =
        "df97c7f6cd622b16b9ffb57bc562906e349c18c56ed8abeb564c6f411e64891c";
    constexpr std::string_view entry_hash =
        "7fdf3bd5f9e142e18de37258d45ef8ba836703cdd2aafd16781567fe9992f76e";
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
    constexpr std::size_t expected_size = 66;
    constexpr std::string_view expected_hash =
        "341e6cff049ff9cda953ad0c91f9a064ed2d2cdc1782b417f27ecad7c9b279b4";
    if (plan.bootstrap_loader.disk_offset != raw_disk_offset
        || plan.bootstrap_loader.length != 0x400
        || plan.bootstrap_loader.destination != entry_address) {
        throw std::runtime_error("Unexpected Millennium Amiga bootstrap relocation plan");
    }
    const auto bytes = disk.bytes(raw_disk_offset, expected_size);
    const auto digest = to_hex(sha256(bytes));
    if (digest != expected_hash) {
        throw std::runtime_error("Unexpected Millennium Amiga bootstrap relocator");
    }
    if (copy_source + copy_count - 1U != copy_end
        || copy_end != loaded_end
        || copy_source + (relocated_continuation - copy_destination) != raw_continuation) {
        throw std::runtime_error("Invalid Millennium Amiga bootstrap relocation boundary");
    }
    return {entry_address, entry_address, loaded_end, copy_source, copy_destination,
        copy_count, copy_end, relocated_continuation, raw_continuation, raw_disk_offset,
        static_cast<std::uint32_t>(expected_size), digest};
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
            || !std::equal(text.begin(), text.end(), source.begin() + static_cast<std::ptrdiff_t>(offset))
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

    constexpr ExecutableByteAnchor<20> entry_prefix{"6e78c50f48753ebe5a7295e338a1bca09e07d9cfcadef9f6ab4014c958140460"};
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset, entry_prefix.size() + 2U);
    if (!entry_prefix.matches(std::span<const std::uint8_t>(bytes.begin(), entry_prefix.size()))
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
    constexpr ExecutableByteAnchor<16> prefix{"3a36f1b9fc98e239ea8361f6a53bf39fcfd7f6f74a599d553d623a33f32d3abe"};
    constexpr ExecutableByteAnchor<10> next_word{"3313fc7eba84dd309387045ce2cc4a1d1cd7e5ae1a175e76439566210b3e3139"};
    constexpr std::array<std::uint32_t, 3> magnitude_addresses{{0x7b764, 0x7b766, 0x7b768}};
    constexpr std::array<std::uint32_t, 3> sign_addresses{{0x7b776, 0x7b777, 0x7b778}};
    constexpr ExecutableByteAnchor<26> tail{"abce757d1ab25758858aea67dce36732c9f07909e02aa84eb1fd11e195328ccc"};
    constexpr std::size_t stores_size = 12;
    constexpr std::size_t routine_length = prefix.size() + stores_size
        + 2 * (next_word.size() + stores_size) + tail.size();
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + routine_offset, routine_length);
    if (!prefix.matches(std::span<const std::uint8_t>(bytes.begin(), prefix.size()))) {
        throw std::runtime_error("Unexpected Millennium Amiga resident word-splitter prefix");
    }

    std::size_t offset = prefix.size();
    for (std::size_t index = 0; index < magnitude_addresses.size(); ++index) {
        if (index != 0) {
            if (!next_word.matches(std::span<const std::uint8_t>(bytes.begin() + static_cast<std::ptrdiff_t>(offset), next_word.size()))) {
                throw std::runtime_error("Unexpected Millennium Amiga resident word-splitter input");
            }
            offset += next_word.size();
        }
        if (big32(bytes, offset) != (0x33c00000U | (magnitude_addresses[index] >> 16U))
            || big32(bytes, offset + 2U) != magnitude_addresses[index]
            || big32(bytes, offset + 6U) != (0x13c30000U | (sign_addresses[index] >> 16U))
            || big32(bytes, offset + 8U) != sign_addresses[index]) {
            throw std::runtime_error("Unexpected Millennium Amiga resident word-splitter store");
        }
        offset += 12U;
    }
    if (!tail.matches(std::span<const std::uint8_t>(bytes.begin() + static_cast<std::ptrdiff_t>(offset), tail.size()))) {
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
    constexpr ExecutableByteAnchor<32> expected_prefix{"eb11f5c5dfda4234b0214599bffec09402deff2435c58d57db1f7ab84c07c434"};
    if (relative > plan.resident_stage.length
        || expected_prefix.size() > plan.resident_stage.length - relative) {
        throw std::runtime_error("Millennium Amiga helper is outside resident raw range");
    }
    const auto raw_disk_offset = plan.resident_stage.disk_offset + relative;
    const auto source = disk.bytes(raw_disk_offset, expected_prefix.size());
    if (!expected_prefix.matches(std::span<const std::uint8_t>(source.begin(), expected_prefix.size()))) {
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
    constexpr ExecutableByteAnchor<32> expected_prefix{"a695fd5ead90e07075256b1347220afde1a4439dd804cf1a9d445da4411cb52a"};
    if (relative > plan.resident_stage.length
        || expected_prefix.size() > plan.resident_stage.length - relative) {
        throw std::runtime_error("Millennium Amiga setup helper is outside resident raw range");
    }
    const auto raw_disk_offset = plan.resident_stage.disk_offset + relative;
    const auto source = disk.bytes(raw_disk_offset, expected_prefix.size());
    if (!expected_prefix.matches(std::span<const std::uint8_t>(source.begin(), expected_prefix.size()))) {
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
    constexpr ExecutableByteAnchor<6> word_copies_bytes{"52153e0b0a91bd56037e7abbab765100111f889e0977492ee01f56567d1cea07"};
    constexpr ExecutableByteAnchor<6> sign_copies_bytes{"a957eeb88252ad32df7faee8a2c5166e745968afc7689f9fef641afa0ec4f876"};

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
        constexpr ExecutableByteAnchor<6> magnitude_prefix{"7498a7d6afb071f6ea05d7cf10544065b075203b4649b1ddafc2d02ccb3d47e3"};
        constexpr ExecutableByteAnchor<6> sign_prefix{"2e56d8aa0b32b4254300b3c2411bc20a157b1c77c5780e2f6bd85b021d00758e"};
        constexpr ExecutableByteAnchor<20> suffix{"3b76c33c82cd5170597f2f2bcab583b66687596b884af4bdcdb529b7aa8d5512"};
        if (big32(bytes, 0) != (0x287c0000U | (sources[index] >> 16U))
            || big32(bytes, 2U) != sources[index]) {
            throw std::runtime_error("Unexpected Millennium Amiga helper staging source");
        }
        if (!magnitude_prefix.matches(std::span<const std::uint8_t>(bytes.begin() + prefix_size, magnitude_prefix.size()))
            || !word_copies_bytes.matches(std::span<const std::uint8_t>(
                bytes.begin() + static_cast<std::ptrdiff_t>(prefix_size + magnitude_prefix.size()),
                word_copies_bytes.size()))) {
            throw std::runtime_error("Unexpected Millennium Amiga helper word staging");
        }
        if (!sign_prefix.matches(std::span<const std::uint8_t>(bytes.begin() + prefix_size + magnitude_prefix.size() + word_copies, sign_prefix.size()))
            || !sign_copies_bytes.matches(std::span<const std::uint8_t>(
                bytes.begin() + static_cast<std::ptrdiff_t>(prefix_size + magnitude_prefix.size()
                    + word_copies + sign_prefix.size()), sign_copies_bytes.size()))) {
            throw std::runtime_error("Unexpected Millennium Amiga helper sign staging");
        }
        if (!suffix.matches(std::span<const std::uint8_t>(bytes.begin() + size - suffix.size(), suffix.size()))) {
            throw std::runtime_error("Unexpected Millennium Amiga helper staging tail");
        }
        if (big32(bytes, size) != (0x2a7c0000U | (post_helper_sources[index] >> 16U))
            || big32(bytes, size + 2U) != post_helper_sources[index]
            || big32(bytes, size + 6U) != 0x287c0007U
            || big32(bytes, size + 8U) != 0x0007b764U) {
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
    constexpr ExecutableByteAnchor<12> tail_calls{"2bf7727a4a03c2ddd5f1e6ae7649af0f4e9f8214a7fdfb469496003876260f34"};
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
        || !tail_calls.matches(std::span<const std::uint8_t>(bytes.end() - tail_calls.size(), tail_calls.size()))) {
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
    constexpr ExecutableByteAnchor<6> tail_call{"ae0e1b15817215c290633c1b06ea5cd94794cac951be33a3ca642aceb7534e1a"};
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
        || !tail_call.matches(std::span<const std::uint8_t>(bytes.end() - tail_call.size(), tail_call.size()))) {
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
    constexpr ExecutableByteAnchor<14> gate{"c65e808b661416a61463f76e18e434653d5651c3fb34d22bee898561a9ebc131"};
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
    if (!gate.matches(std::span<const std::uint8_t>(gate_bytes.begin(), gate.size()))) {
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
    constexpr ExecutableByteAnchor<24> bytes_expected{"acf42abab396b2454375f4908ad5d29bee84b2ba1d5d4ca2ac2953aceaa949c5"};
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
    if (!bytes_expected.matches(std::span<const std::uint8_t>(bytes.begin(), bytes_expected.size()))) {
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
    constexpr ExecutableByteAnchor<10> expected{"ec4d9ed2cdc11925c4a3e63a0e8382e7c6c2a42601e2f9dc5f64e267a58f3803"};
    if (zero_path.selector_not_equal_target != entry || entry < plan.resident_stage.destination) {
        throw std::runtime_error("Unexpected Millennium Amiga predicate not-equal path placement");
    }
    const auto relative = entry - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length || expected.size() > plan.resident_stage.length - relative) {
        throw std::runtime_error("Millennium Amiga predicate not-equal path is outside raw range");
    }
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    if (!expected.matches(std::span<const std::uint8_t>(bytes.begin(), expected.size()))) {
        throw std::runtime_error("Unexpected Millennium Amiga predicate not-equal path");
    }
    return {entry, 0, 2, entry + 4, 0x7b90a};
}

MillenniumAmigaResidentIndependentEntryGate
parse_millennium_amiga_resident_independent_entry_gate(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan) {
    validate_range(plan.resident_stage);
    constexpr std::uint32_t entry = 0x68508;
    constexpr ExecutableByteAnchor<18> expected{"96aae276f68f7b4a3548394db0d93d0435053ab5632463dc77936ff583b30a78"};
    if (entry < plan.resident_stage.destination) {
        throw std::runtime_error("Millennium Amiga independent entry precedes raw range");
    }
    const auto relative = entry - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length || expected.size() > plan.resident_stage.length - relative) {
        throw std::runtime_error("Millennium Amiga independent entry is outside raw range");
    }
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    if (!expected.matches(std::span<const std::uint8_t>(bytes.begin(), expected.size()))) {
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
    constexpr ExecutableByteAnchor<10> expected{"5b120eaef941ac336d22e4f76adaeefd8c1d6795d105685f048074edd49c3a6c"};
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
        || !expected.matches(std::span<const std::uint8_t>(bytes.begin(), expected.size()))) {
        throw std::runtime_error("Unexpected Millennium Amiga negative-D3 terminal");
    }
    return {entry, 0x2800, entry + 4, 0x2800, entry + 8};
}

MillenniumAmigaResidentPostNegativeD3Terminal
parse_millennium_amiga_resident_post_negative_d3_terminal(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentNegativeD3Terminal& terminal) {
    constexpr std::uint32_t entry = 0x685fe;
    constexpr ExecutableByteAnchor<28> expected{"a45ff5eca6e3594574b464574fa0aae3027bd2ea11472770708c96f4d21b56cc"};
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
    if (!expected.matches(std::span<const std::uint8_t>(bytes.begin(), expected.size()))
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
    constexpr ExecutableByteAnchor<54> expected{"d3f6b63090429e11fb3a77e4573817649e2bb7996d06811ea2751078794534ce"};
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
    if (!expected.matches(std::span<const std::uint8_t>(bytes.begin(), expected.size())) || observed_hash != expected_hash) {
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
    constexpr ExecutableByteAnchor<6> expected{"6234de114f74ff307c623a4c84a6c099493f16b4c5ab26afdf4f0626e7b6ac64"};
    if (gate.flag_zero_target != entry || entry < plan.resident_stage.destination) {
        throw std::runtime_error("Unexpected Millennium Amiga independent zero target placement");
    }
    const auto relative = entry - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length || expected.size() > plan.resident_stage.length - relative) {
        throw std::runtime_error("Millennium Amiga independent zero target is outside raw range");
    }
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    if (!expected.matches(std::span<const std::uint8_t>(bytes.begin(), expected.size()))) {
        throw std::runtime_error("Unexpected Millennium Amiga independent zero target");
    }
    return {entry, 0x0120, entry + 4, entry + 6 + 0x12};
}

MillenniumAmigaResidentIndependentCompareTargetBoundary
parse_millennium_amiga_resident_independent_compare_target_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentIndependentZeroTargetBoundary& boundary) {
    constexpr std::uint32_t entry = 0x68562;
    constexpr ExecutableByteAnchor<10> expected{"4ae7f4ee5d5200a93789893ef956848baac28e152357884504ea04ec3e006b7e"};
    if (boundary.conditional_branch_target != entry || entry < plan.resident_stage.destination) {
        throw std::runtime_error("Unexpected Millennium Amiga independent compare target placement");
    }
    const auto relative = entry - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length || expected.size() > plan.resident_stage.length - relative) {
        throw std::runtime_error("Millennium Amiga independent compare target is outside raw range");
    }
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    if (!expected.matches(std::span<const std::uint8_t>(bytes.begin(), expected.size()))) {
        throw std::runtime_error("Unexpected Millennium Amiga independent compare target");
    }
    return {entry, entry + 8, entry + 10 + 0x0e};
}

MillenniumAmigaResidentIndependentBranchTargetBoundary
parse_millennium_amiga_resident_independent_branch_target_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentIndependentCompareTargetBoundary& boundary) {
    constexpr std::uint32_t entry = 0x6857a;
    constexpr ExecutableByteAnchor<8> expected{"cfbba9bd1550c6c204a312027c8e99f30c3f4cfde9dcc90173423d113a6ef1ee"};
    if (boundary.conditional_branch_target != entry || entry < plan.resident_stage.destination) throw std::runtime_error("Unexpected Millennium Amiga independent branch target placement");
    const auto relative = entry - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length || expected.size() > plan.resident_stage.length - relative) throw std::runtime_error("Millennium Amiga independent branch target is outside raw range");
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    if (!expected.matches(std::span<const std::uint8_t>(bytes.begin(), expected.size()))) throw std::runtime_error("Unexpected Millennium Amiga independent branch target");
    return {entry, entry + 6, entry + 8 + 4};
}

MillenniumAmigaResidentIndependentBranchPreparationBoundary
parse_millennium_amiga_resident_independent_branch_preparation_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentIndependentBranchTargetBoundary& boundary) {
    constexpr std::uint32_t entry = 0x68586;
    constexpr ExecutableByteAnchor<16> expected{"bd42dd2b1469da14944dab6bd33f0016f20581d168ecbf3972c1186bd3363d3e"};
    if (boundary.conditional_branch_target != entry || entry < plan.resident_stage.destination) throw std::runtime_error("Unexpected Millennium Amiga independent branch preparation placement");
    const auto relative = entry - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length || expected.size() > plan.resident_stage.length - relative) throw std::runtime_error("Millennium Amiga independent branch preparation is outside raw range");
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    if (!expected.matches(std::span<const std::uint8_t>(bytes.begin(), expected.size()))) throw std::runtime_error("Unexpected Millennium Amiga independent branch preparation");
    return {entry, entry + 10, 0x7b26a};
}

MillenniumAmigaResidentIndependentPostCallTailBoundary
parse_millennium_amiga_resident_independent_post_call_tail_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentIndependentBranchPreparationBoundary& boundary) {
    // This starts at the return PC of JSR $7b26a. That return is not
    // presumed: hash-lock its call-free caller-side tail as source evidence.
    constexpr std::uint32_t entry = 0x68596;
    constexpr std::size_t expected_size = 104;
    constexpr std::string_view expected_hash =
        "eeed978d0afd278cc48868c0d2b76205304ddfa80b174d2aac95dc50b80dd551";
    if (boundary.unknown_call_address != 0x68590 || boundary.unknown_call_target != 0x7b26a
        || entry < plan.resident_stage.destination) {
        throw std::runtime_error("Unexpected Millennium Amiga independent post-call tail placement");
    }
    const auto relative = entry - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length || expected_size > plan.resident_stage.length - relative) {
        throw std::runtime_error("Millennium Amiga independent post-call tail is outside raw range");
    }
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected_size);
    const auto hash = to_hex(sha256(bytes));
    if (hash != expected_hash) {
        throw std::runtime_error("Unexpected Millennium Amiga independent post-call tail");
    }
    return {entry, plan.resident_stage.disk_offset + relative, expected_size, hash,
        {0x7b3b0, 0x7b3b1, 0x7b3b4, 0x7b3ba, 0x7b3bb, 0x7b3bc},
        0x685ee, 0x7bcf8, 0x685f4, 0x685fc, 0x685fe};
}

MillenniumAmigaResidentSeparateEntryGate
parse_millennium_amiga_resident_separate_entry_gate(const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan) {
    constexpr std::uint32_t entry = 0x68d50;
    constexpr ExecutableByteAnchor<12> expected{"5f3fbe8ef5e5f5e35bbe09b2ce0a959ce3e4c0a8e2b65f1df6851b0bc446e145"};
    if (entry < plan.resident_stage.destination) throw std::runtime_error("Millennium Amiga separate entry precedes raw range");
    const auto relative = entry - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length || expected.size() > plan.resident_stage.length - relative) throw std::runtime_error("Millennium Amiga separate entry is outside raw range");
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    if (!expected.matches(std::span<const std::uint8_t>(bytes.begin(), expected.size()))) throw std::runtime_error("Unexpected Millennium Amiga separate entry gate");
    return {entry, entry + 8, entry + 12 + 6};
}

MillenniumAmigaResidentSeparateBranchBoundary
parse_millennium_amiga_resident_separate_branch_boundary(
    const AmigaAdf& disk,
    const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentSeparateEntryGate& gate) {
    constexpr std::uint32_t entry = 0x68d62;
    constexpr ExecutableByteAnchor<32> expected{"750ed87d455d69514071db3b06f452c3a1a6cf93f8f769338b9e590010921a44"};
    if (gate.branch_target != entry || entry < plan.resident_stage.destination) {
        throw std::runtime_error("Unexpected Millennium Amiga separate branch placement");
    }
    const auto relative = entry - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length
        || expected.size() > plan.resident_stage.length - relative) {
        throw std::runtime_error("Millennium Amiga separate branch outside raw range");
    }
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size());
    if (!expected.matches(std::span<const std::uint8_t>(bytes.begin(), expected.size()))) {
        throw std::runtime_error("Unexpected Millennium Amiga separate branch");
    }
    return {entry, entry + 12, entry + 16 + 6, entry + 26, 0x778f0};
}

MillenniumAmigaResidentSeparatePostCallBoundary parse_millennium_amiga_resident_separate_post_call_boundary(const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan, const MillenniumAmigaResidentSeparateBranchBoundary& branch) {
    constexpr std::uint32_t entry = 0x68d82, next_target = 0x7b342;
    constexpr ExecutableByteAnchor<26> expected{"e49e750f78946956c22d4cd80206139d38808d4ecb3b1579906aeaede0db7b77"};
    constexpr std::string_view expected_hash = "e49e750f78946956c22d4cd80206139d38808d4ecb3b1579906aeaede0db7b77";
    constexpr std::string_view target_hash = "731d016983d29dcb23abad28f3f0f225bd3708073e8c0c8481a97a50b460cdcf";
    if (branch.unknown_call_address != 0x68d7c || branch.unknown_call_target != 0x778f0 || entry < plan.resident_stage.destination || next_target < plan.resident_stage.destination) throw std::runtime_error("Unexpected Millennium Amiga separate post-call placement");
    const auto relative = entry - plan.resident_stage.destination, target_relative = next_target - plan.resident_stage.destination;
    if (relative > plan.resident_stage.length || expected.size() > plan.resident_stage.length - relative || target_relative > plan.resident_stage.length || 32U > plan.resident_stage.length - target_relative) throw std::runtime_error("Millennium Amiga separate post-call is outside raw range");
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset + relative, expected.size()), target = disk.bytes(plan.resident_stage.disk_offset + target_relative, 32);
    const auto hash = to_hex(sha256(bytes)), target_prefix_hash = to_hex(sha256(target));
    if (!expected.matches(std::span<const std::uint8_t>(bytes.begin(), expected.size())) || hash != expected_hash || target_prefix_hash != target_hash) throw std::runtime_error("Unexpected Millennium Amiga separate post-call boundary");
    return {entry, plan.resident_stage.disk_offset + relative, hash, 0x2208, 0x6934e, 0x7c256, entry + 20, next_target, plan.resident_stage.disk_offset + target_relative, target_prefix_hash};
}

MillenniumAmigaResidentSeparatePostCallTailBoundary
parse_millennium_amiga_resident_separate_post_call_tail_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentSeparatePostCallBoundary& boundary) {
    constexpr std::uint32_t entry = 0x68d9c;
    constexpr ExecutableByteAnchor<36> expected{"08c660de1ed6d0b0f535e451c84450397383a923a1808fa9678d3ae85a8cc17b"};
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
    if (!expected.matches(std::span<const std::uint8_t>(bytes.begin(), expected.size())) || hash != expected_hash) {
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
    constexpr ExecutableByteAnchor<14> expected{"ef2fe6161118a1b0ac6cee838be9a4dc2b0483ba274a213d3ac653ea6f334e3b"};
    constexpr ExecutableByteAnchor<32> target_prefix{"13ed782f5463fd93bbd4376777a1c01d8fd636018de8aef52f5710eb0da11a2b"};
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
    if (!expected.matches(std::span<const std::uint8_t>(bytes.begin(), expected.size()))
        || !target_prefix.matches(std::span<const std::uint8_t>(prefix.begin(), target_prefix.size()))
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
    constexpr ExecutableByteAnchor<4> preceding_branch{"7d57d5a4e829aa10607ae6294aefedd2b49fe94d529c8d13568cb2e93c22de6d"};
    constexpr ExecutableByteAnchor<36> expected{"8cb29601f0c76406930e37d44b29853501857c36f3cb833ccdd32e78418597d4"};
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
    if (!expected.matches(std::span<const std::uint8_t>(source.begin(), expected.size()))
        || !preceding_branch.matches(std::span<const std::uint8_t>(preceding.begin(), preceding_branch.size()))
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
    constexpr ExecutableByteAnchor<34> expected{"f4a047914e83ab873a037ea16a4f5aaa9a402c38f48a525efc69d9e49cca15a8"};
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
    if (!expected.matches(std::span<const std::uint8_t>(bytes.begin(), expected.size())) || hash != expected_hash || target_digest != target_hash || fallthrough_digest != fallthrough_hash) throw std::runtime_error("Unexpected Millennium Amiga byte gate");
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
    constexpr ExecutableByteAnchor<30> expected{"b2d2c6cadc50725eb8b4f0b680c325586ed457b29232481b503f3e337d589341"};
    constexpr ExecutableByteAnchor<32> convergence_prefix{"93b0d20954d235c624406450161a359968e4f1baefcbaeb47ede08fda0cd1e71"};
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
    if (!expected.matches(std::span<const std::uint8_t>(bytes.begin(), expected.size()))
        || !convergence_prefix.matches(std::span<const std::uint8_t>(next.begin(), convergence_prefix.size()))
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
    constexpr ExecutableByteAnchor<34> expected{"d63b2de78fbc18f2a4213206d1f05947a604dafc5b23fea56f87b624cb7549ab"};
    constexpr ExecutableByteAnchor<32> target_prefix{"ba2a0127999eb628ef05008867728fd31952c6d4b268bdb38f35130bab9973ae"};
    constexpr ExecutableByteAnchor<32> fallthrough_prefix{"5b3ae299a769dcca25b96b3b588ab65b1c44843abf0ef1288a1a74741dec9993"};
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
    if (!expected.matches(std::span<const std::uint8_t>(bytes.begin(), expected.size()))
        || !target_prefix.matches(std::span<const std::uint8_t>(target_bytes.begin(), target_prefix.size()))
        || !fallthrough_prefix.matches(std::span<const std::uint8_t>(fallthrough_bytes.begin(), fallthrough_prefix.size()))
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
    constexpr ExecutableByteAnchor<36> expected{"a7f4be625a6a39615f0ace12a1a8e013b781575625858b4f0c257d171b0947f3"};
    constexpr ExecutableByteAnchor<18> external_prefix{"dde319f5e57db52df300956d4e3e59dc6dc7967f0ff582674d502109fcfa2f69"};
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
    if (!expected.matches(std::span<const std::uint8_t>(bytes.begin(), expected.size()))
        || !external_prefix.matches(std::span<const std::uint8_t>(call_prefix.begin(), external_prefix.size()))
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
    constexpr ExecutableByteAnchor<24> expected{"4a50d1c5f71ada9a3571e09b00437c51037c3949ff8e57a4b153ea032828d061"};
    constexpr ExecutableByteAnchor<12> other_path_bytes{"fc1fca692a8fc07b5fd7c502ae2d772eeff63c0c3d33d298f9c4fac414f337da"};
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
    if (!expected.matches(std::span<const std::uint8_t>(bytes.begin(), expected.size()))
        || !other_path_bytes.matches(std::span<const std::uint8_t>(other.begin(), other_path_bytes.size()))
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
    constexpr ExecutableByteAnchor<42> expected{"3220d65f197163401c649a36d756ecf3005d2f342b81de5a7d4528f9a45da851"};
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
    if (!expected.matches(std::span<const std::uint8_t>(bytes.begin(), expected.size())) || hash != expected_hash
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
