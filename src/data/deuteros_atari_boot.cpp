#include "data/deuteros_atari_boot.hpp"

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
    // bytes at $ee to absolute RAM $8, then jumps to absolute address $12.
    // Keep this as a protected-media trace: the copied words and destination
    // are not classified as a game executable or resource.
    constexpr std::array<std::uint8_t, 28> killer_vector_setup{{
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
        profile_.killer_boot_vector_source_offset = 0xee;
        profile_.killer_boot_vector_destination = 0x8;
        profile_.killer_boot_vector_longword_count = 10;
        profile_.killer_boot_continuation = 0x12;
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
