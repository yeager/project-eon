#include "data/deuteros_atari_boot.hpp"
#include "data/sha256.hpp"

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
    constexpr auto ascii_sha256 =
        "8dd46e7c760a38d07273b18a4cbd3c03eb44a6b57c8c401580dd47fa4646484e";
    if (state1.source_offset != 0x55800 || state1.destination != 0xb000
        || state1.byte_count != 0x5e400 || state1_bytes.size() != state1.byte_count
        || ascii_relative_offset > state1_bytes.size()
        || ascii_byte_count > state1_bytes.size() - ascii_relative_offset) {
        throw std::runtime_error("Unexpected Deuteros Atari ST state-1 skipped ASCII placement");
    }
    require_bytes(state1_bytes, branch_relative_offset, branch,
        "Unexpected Deuteros Atari ST state-1 skip branch");
    const auto ascii = state1_bytes.subspan(ascii_relative_offset, ascii_byte_count);
    if (to_hex(sha256(ascii)) != ascii_sha256) {
        throw std::runtime_error("Unexpected Deuteros Atari ST state-1 skipped ASCII block");
    }
    return {branch_relative_offset, 0x09c2, ascii_relative_offset, ascii_byte_count, 18,
        ascii_sha256};
}

} // namespace eon
