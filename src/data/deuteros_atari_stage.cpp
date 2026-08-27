#include "data/deuteros_atari_boot.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

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

} // namespace

DeuterosAtariFirstStageProfile parse_deuteros_atari_first_stage(
    std::span<const std::uint8_t> bytes) {
    if (bytes.size() != 0x1200U || be16(bytes, 0) != 0x6000U) {
        throw std::runtime_error("Unsupported Deuteros Atari ST first-stage header");
    }
    // 68000 word branch displacement is relative to the extension word's
    // address. The boot stage jumps directly to $09c4.
    const auto entry_offset = static_cast<std::size_t>(2U + be16(bytes, 2));
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
        .checksum_byte_count = static_cast<std::size_t>(be32(bytes, 0xa26)) + 1U,
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
        checksum += value;
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
        const auto linear_track = dispatch.state0_linear_sector + static_cast<std::uint32_t>(index);
        result.requests[index] = {
            .track = static_cast<std::uint16_t>(linear_track % 0x50U),
            .side = static_cast<std::uint8_t>(linear_track / 0x50U),
            .first_sector = 1,
            .sector_count = stage.raw_read_max_sector_count,
            .source_offset = result.source_offset + index * bytes_per_track,
        };
    }
    return result;
}

} // namespace eon
