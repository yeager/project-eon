#include "data/deuteros_atari_boot.hpp"
#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace eon {
namespace {

std::uint16_t be16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::runtime_error("Truncated Deuteros Atari ST boot field");
    }
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8U)
        | bytes[offset + 1]);
}

std::uint32_t be32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw std::runtime_error("Truncated Deuteros Atari ST longword");
    }
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U)
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 2]) << 8U)
        | static_cast<std::uint32_t>(bytes[offset + 3]);
}

std::uint16_t le16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::runtime_error("Truncated Deuteros Atari ST BPB field");
    }
    return static_cast<std::uint16_t>(bytes[offset]
        | (static_cast<std::uint16_t>(bytes[offset + 1]) << 8U));
}

bool starts_with(std::span<const std::uint8_t> bytes, std::size_t offset,
    std::span<const std::uint8_t> expected) {
    return offset <= bytes.size() && expected.size() <= bytes.size() - offset
        && std::equal(expected.begin(), expected.end(), bytes.begin()
            + static_cast<std::ptrdiff_t>(offset));
}

} // namespace

DeuterosAtariDisk::DeuterosAtariDisk(std::vector<std::uint8_t> image) : image_(std::move(image)) {
    if (image_.size() != standard_size) {
        throw std::runtime_error("Unsupported Deuteros Atari ST disk geometry");
    }
    const std::span bytes(image_);
    if ((be16(bytes, 0) & 0xff00U) != 0x6000U) {
        throw std::runtime_error("Deuteros Atari ST disk has no 68000 boot branch");
    }
    profile_.boot_branch_target = static_cast<std::uint16_t>(2U + (be16(bytes, 0) & 0x00ffU));
    if (profile_.boot_branch_target >= 512U) {
        throw std::runtime_error("Deuteros Atari ST boot branch leaves boot sector");
    }
    profile_.bytes_per_sector = le16(bytes, 11);
    profile_.sectors_per_cluster = bytes[13];
    profile_.total_sectors = le16(bytes, 19);
    profile_.sectors_per_track = le16(bytes, 24);
    profile_.heads = le16(bytes, 26);
    if (profile_.bytes_per_sector != 512U || profile_.sectors_per_cluster != 2U
        || profile_.total_sectors != 1440U || profile_.sectors_per_track != 9U
        || profile_.heads != 2U) {
        throw std::runtime_error("Unexpected Deuteros Atari ST BPB geometry");
    }
    std::uint32_t checksum = 0;
    for (std::size_t offset = 0; offset < 512; offset += 2) checksum += be16(bytes, offset);
    profile_.boot_checksum = static_cast<std::uint16_t>(checksum);
    if (profile_.boot_checksum != 0x1234U) {
        throw std::runtime_error("Invalid Deuteros Atari ST boot checksum");
    }
    constexpr std::array<std::uint8_t, 12> killer_boot{{
        'K', 'I', 'L', 'L', 'E', 'R', '_', 'B', 'O', 'O', 'T', 0}};
    profile_.killer_boot_signature = starts_with(bytes, 0x24, killer_boot);

    // Disk 2's post-BPB branch at $22 enters a KILLER_BOOT-specific setup.
    // It enters supervisor mode, copies ten literal longwords from the boot
    // bytes at $f0 to absolute RAM $8, then jumps to absolute address $12.
    // The LEA extension word is at $e2, so its PC-relative `$000e` resolves
    // to $f0 rather than the instruction's own offset.
    // Keep this as a protected-media trace: the copied words and destination
    // are not classified as a game executable or resource.
    constexpr std::array<std::uint8_t, 24> killer_vector_setup{{
        0x46, 0xfc, 0x27, 0x00, // move.w #$2700,sr
        0x43, 0xf8, 0x00, 0x08, // lea $8.w,a1
        0x41, 0xfa, 0x00, 0x0e, // lea $ee(pc),a0
        0x7e, 0x09,             // moveq #9,d7
        0x22, 0xd8,             // move.l (a0)+,(a1)+
        0x51, 0xcf, 0xff, 0xfc, // dbf d7,$e4
        0x4e, 0xf8, 0x00, 0x12  // jmp $12.w
    }};
    if (profile_.killer_boot_signature && profile_.boot_branch_target == 0x22U
        && starts_with(bytes, 0xd8, killer_vector_setup)) {
        profile_.has_killer_boot_vector_setup = true;
        profile_.killer_boot_entry_offset = 0x30;
        profile_.killer_boot_vector_source_offset = 0xf0;
        profile_.killer_boot_vector_destination = 0x8;
        profile_.killer_boot_vector_longword_count = 10;
        profile_.killer_boot_continuation = 0x12;
        constexpr std::size_t relocated_byte_count = 40;
        constexpr auto expected_relocated_sha256 =
            "21a5d61e2289fe2f2141d3710fad31faf42e96f59c5fba768819380e8f595a8d";
        constexpr std::array<std::uint8_t, relocated_byte_count> relocated_code{{
            0x00, 0x00, 0x00, 0x0c, 0x20, 0x78, 0x00, 0x04,
            0x4e, 0xd0, 0x41, 0xfa, 0x00, 0x1c, 0x70, 0x00,
            0x22, 0x00, 0x24, 0x00, 0x26, 0x00, 0x28, 0x00,
            0x2a, 0x00, 0x2c, 0x00, 0x2e, 0x00, 0x48, 0xd0,
            0x00, 0xff, 0xd0, 0xfc, 0x00, 0x20, 0x60, 0xf6,
        }};
        const auto relocated = bytes.subspan(profile_.killer_boot_vector_source_offset,
            relocated_byte_count);
        if (starts_with(bytes, profile_.killer_boot_vector_source_offset, relocated_code)
            && to_hex(sha256(relocated)) == expected_relocated_sha256) {
            profile_.has_killer_boot_continuation_profile = true;
            profile_.killer_boot_relocated_byte_count = relocated_byte_count;
            profile_.killer_boot_relocated_sha256 = expected_relocated_sha256;
            profile_.killer_boot_clear_start = 0x30;
            profile_.killer_boot_clear_stride = 0x20;
            profile_.killer_boot_clear_longword_count = 8;
        }
    }

    // At $50 the supplied Replicants Disk 1 starts a literal Floprd argument
    // sequence. It reads 9 sectors at track 70 / side 0 / sector 1 into A6.
    // Do not generalize this to the other crack boot sectors.
    constexpr std::array<std::uint8_t, 26> replicants_floprd{{
        0x3f, 0x3c, 0x00, 0x09, 0x3f, 0x07, 0x3f, 0x06,
        0x3f, 0x3c, 0x00, 0x01, 0x42, 0x67, 0x42, 0xa7,
        0x48, 0x56, 0x3f, 0x3c, 0x00, 0x08, 0x4e, 0x4e,
        0x4f, 0xef}};
    if (starts_with(bytes, 0x50, replicants_floprd)) {
        profile_.has_recovered_first_stage = true;
        profile_.first_stage_track = 70;
        profile_.first_stage_side = 0;
        profile_.first_stage_sector = 1;
        profile_.first_stage_sector_count = 9;
        profile_.first_stage_offset = static_cast<std::size_t>(70) * 2U * 9U * 512U;
        profile_.first_stage_length = 9U * 512U;
    }
}

DeuterosAtariKillerBootHandoff parse_deuteros_atari_killer_boot_handoff(
    const std::span<const std::uint8_t> boot_sector, const DeuterosAtariBootProfile& profile) {
    // This is the source instruction block which makes the copy visible to
    // the machine: the DBF executes ten MOVE.L transfers, then JMP $12.w.
    // The target is inside that copied $f0..$117 source span once relocated
    // to RAM $8.  It is intentionally not treated as a host reset or game
    // bootstrap; the preceding copied JMP (A0) instead depends on RAM $4.
    constexpr std::size_t setup_offset = 0xd8;
    constexpr auto setup_bytes = std::to_array<std::uint8_t>({
        0x46, 0xfc, 0x27, 0x00, 0x43, 0xf8, 0x00, 0x08,
        0x41, 0xfa, 0x00, 0x0e, 0x7e, 0x09, 0x22, 0xd8,
        0x51, 0xcf, 0xff, 0xfc, 0x4e, 0xf8, 0x00, 0x12,
    });
    constexpr std::string_view setup_sha256 =
        "1ce81773d11374cac65ce69742a475e0731cbc8798f7c7bd374c04a2d2a7d150";
    constexpr std::size_t source_offset = 0xf0;
    constexpr std::size_t byte_count = 40;
    constexpr std::string_view relocated_sha256 =
        "21a5d61e2289fe2f2141d3710fad31faf42e96f59c5fba768819380e8f595a8d";
    constexpr std::uint32_t destination = 0x8;
    constexpr std::uint32_t continuation_address = 0x12;
    constexpr std::size_t continuation_relocated_offset = 10;
    constexpr std::size_t vector_jump_relocated_offset = 8;
    if (boot_sector.size() != 512U || !profile.killer_boot_signature
        || !profile.has_killer_boot_vector_setup || !profile.has_killer_boot_continuation_profile
        || profile.boot_branch_target != 0x22U || profile.killer_boot_entry_offset != 0x30U
        || profile.killer_boot_vector_source_offset != source_offset
        || profile.killer_boot_vector_destination != destination
        || profile.killer_boot_vector_longword_count != 10U
        || profile.killer_boot_continuation != continuation_address
        || profile.killer_boot_relocated_byte_count != byte_count
        || profile.killer_boot_relocated_sha256 != relocated_sha256) {
        throw std::runtime_error("Unexpected Deuteros Atari ST KILLER_BOOT handoff topology");
    }
    if (!starts_with(boot_sector, setup_offset, setup_bytes)) {
        throw std::runtime_error("Unexpected Deuteros Atari ST KILLER_BOOT handoff setup");
    }
    const auto setup = boot_sector.subspan(setup_offset, setup_bytes.size());
    const auto relocated = boot_sector.subspan(source_offset, byte_count);
    if (to_hex(sha256(setup)) != setup_sha256 || to_hex(sha256(relocated)) != relocated_sha256
        || be16(relocated, continuation_relocated_offset) != 0x41faU
        || be16(relocated, vector_jump_relocated_offset) != 0x4ed0U
        || be16(relocated, vector_jump_relocated_offset - 2U) != 0x0004U) {
        throw std::runtime_error("Unexpected Deuteros Atari ST KILLER_BOOT handoff bytes");
    }
    return {setup_offset, setup_bytes.size(), std::string(setup_sha256), source_offset, byte_count,
        destination, std::string(relocated_sha256), continuation_address,
        continuation_relocated_offset, be16(relocated, continuation_relocated_offset),
        vector_jump_relocated_offset, be16(relocated, vector_jump_relocated_offset),
        be16(relocated, vector_jump_relocated_offset - 2U)};
}

DeuterosAtariKillerBootExecutionPrefix execute_deuteros_atari_killer_boot_prefix(
    const std::span<const std::uint8_t> boot_sector, const DeuterosAtariBootProfile& profile) {
    // Bind the caller-connected DBF copy and its direct relocated entry. The
    // separate JMP (A0) vector-cell path at relocated +$8 is not followed.
    const auto handoff = parse_deuteros_atari_killer_boot_handoff(boot_sector, profile);
    constexpr std::array<std::uint8_t, 30> continuation_bytes{{
        0x41, 0xfa, 0x00, 0x1c, 0x70, 0x00,
        0x22, 0x00, 0x24, 0x00, 0x26, 0x00, 0x28, 0x00,
        0x2a, 0x00, 0x2c, 0x00, 0x2e, 0x00,
        0x48, 0xd0, 0x00, 0xff, 0xd0, 0xfc, 0x00, 0x20,
        0x60, 0xf6,
    }};
    constexpr std::uint32_t first_clear_address = 0x32;
    constexpr std::uint32_t loop_target_address = 0x30;
    const auto relocated = boot_sector.subspan(handoff.source_offset, handoff.byte_count);
    if (handoff.destination != 0x8U || handoff.continuation_address != 0x12U
        || handoff.continuation_relocated_offset != 10U
        || continuation_bytes.size() != relocated.size() - handoff.continuation_relocated_offset
        || !std::equal(continuation_bytes.begin(), continuation_bytes.end(),
            relocated.begin() + static_cast<std::ptrdiff_t>(handoff.continuation_relocated_offset))) {
        throw std::runtime_error("Unexpected Deuteros Atari ST KILLER_BOOT local continuation");
    }

    DeuterosAtariKillerBootExecutionPrefix result;
    result.relocation_destination = handoff.destination;
    result.relocated_bytes.resize(relocated.size());
    for (std::size_t index = 0; index < result.relocated_longwords.size(); ++index) {
        const auto offset = index * 4U;
        std::copy_n(relocated.begin() + static_cast<std::ptrdiff_t>(offset), 4,
            result.relocated_bytes.begin() + static_cast<std::ptrdiff_t>(offset));
        result.relocated_longwords[index] = be32(relocated, offset);
    }
    if (!std::equal(result.relocated_bytes.begin(), result.relocated_bytes.end(), relocated.begin())) {
        throw std::runtime_error("Deuteros Atari ST KILLER_BOOT relocation lost original bytes");
    }
    result.continuation_address = handoff.continuation_address;
    result.first_clear_address = first_clear_address;
    for (std::size_t index = 0; index < result.cleared_longword_addresses.size(); ++index) {
        result.cleared_longword_addresses[index] = first_clear_address
            + static_cast<std::uint32_t>(index * 4U);
    }
    result.next_clear_address = first_clear_address + 0x20U;
    result.loop_target_address = loop_target_address;
    return result;
}

std::vector<std::uint8_t> DeuterosAtariDisk::read_sectors(
    std::uint16_t track, std::uint8_t side, std::uint8_t first_sector,
    std::uint16_t sector_count) const {
    if (side >= profile_.heads || first_sector == 0 || first_sector > profile_.sectors_per_track
        || sector_count == 0 || sector_count > profile_.sectors_per_track - first_sector + 1U) {
        throw std::runtime_error("Deuteros Atari ST raw sector range is invalid");
    }
    const auto sectors_per_cylinder = static_cast<std::size_t>(profile_.heads)
        * profile_.sectors_per_track;
    const auto first_logical_sector = static_cast<std::size_t>(track) * sectors_per_cylinder
        + static_cast<std::size_t>(side) * profile_.sectors_per_track + first_sector - 1U;
    if (first_logical_sector >= profile_.total_sectors
        || sector_count > profile_.total_sectors - first_logical_sector) {
        throw std::runtime_error("Deuteros Atari ST raw sector range is outside disk");
    }
    const auto offset = first_logical_sector * profile_.bytes_per_sector;
    const auto length = static_cast<std::size_t>(sector_count) * profile_.bytes_per_sector;
    if (offset > image_.size() || length > image_.size() - offset) {
        throw std::runtime_error("Deuteros Atari ST raw sector bytes are outside disk");
    }
    return {image_.begin() + static_cast<std::ptrdiff_t>(offset),
        image_.begin() + static_cast<std::ptrdiff_t>(offset + length)};
}

} // namespace eon
