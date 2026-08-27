#include "data/millennium_amiga_loader.hpp"
#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

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
        throw std::runtime_error("Invalid Millennium Amiga first-stage length");
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
    // add.l d7,d7 doubles the $1600 I/O chunk before the resident request;
    // the original code then supplies the total length in d0 to the same loop.
    if (resident_chunk == 0 || resident_chunk > UINT32_MAX / 2U) {
        throw std::runtime_error("Invalid Millennium Amiga resident-stage length");
    }
    const auto resident_length = resident_chunk * 2U * 0x10U;
    const auto magic_offset = resident_offset + 54;
    if (big32(loader, magic_offset) != 0xa8d398fb) {
        throw std::runtime_error("Millennium Amiga loader handoff marker not found");
    }

    MillenniumAmigaLoadPlan plan{
        make_stage(disk, bootstrap_disk_offset, bootstrap_length, bootstrap_destination),
        make_stage(disk, 0x24200, first_chunk * multiplier, 0x41000),
        make_stage(disk, 0x16400, resident_length, 0x68000),
        0x68000,
        big32(loader, magic_offset),
    };
    validate_range(plan.bootstrap_loader);
    validate_range(plan.first_stage);
    validate_range(plan.resident_stage);
    return plan;
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
            if (!std::equal(next_word.begin(), next_word.end(), bytes.begin() + offset)) {
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
        if (!std::equal(expected_store.begin(), expected_store.end(), bytes.begin() + offset)) {
            throw std::runtime_error("Unexpected Millennium Amiga resident word-splitter store");
        }
        offset += expected_store.size();
    }
    if (!std::equal(tail.begin(), tail.end(), bytes.begin() + offset)) {
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
        const auto opcode = static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset]) << 8U)
            | bytes[offset + 1U];
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
                const auto register_index = static_cast<std::uint16_t>((opcode >> 9U) & 7U);
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
        0x6b, 0x00, 0x00, 0x86,             // bmi.w $68594
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

} // namespace eon
