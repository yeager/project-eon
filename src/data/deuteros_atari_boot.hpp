#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace eon {

// The supplied Deuteros ST dumps are raw protected disks, despite carrying a
// DOS-style BPB.  In particular their FAT root region is executable/data, not
// a GEMDOS directory.  This reader deliberately models only the evidence in
// their boot sectors and raw sectors; it never treats those bytes as files.
struct DeuterosAtariBootProfile {
    std::uint16_t bytes_per_sector = 0;
    std::uint8_t sectors_per_cluster = 0;
    std::uint16_t total_sectors = 0;
    std::uint16_t sectors_per_track = 0;
    std::uint16_t heads = 0;
    std::uint16_t boot_checksum = 0;
    std::uint16_t boot_branch_target = 0;
    bool killer_boot_signature = false;

    // The supplied unlabelled Disk 2 has a distinct KILLER_BOOT routine.
    // These fields record only its literal vector-table copy and absolute
    // jump; no game-stage meaning is inferred from the protection code.
    bool has_killer_boot_vector_setup = false;
    std::size_t killer_boot_entry_offset = 0;
    std::size_t killer_boot_vector_source_offset = 0;
    std::uint32_t killer_boot_vector_destination = 0;
    std::size_t killer_boot_vector_longword_count = 0;
    std::uint32_t killer_boot_continuation = 0;

    // Recovered from the Replicants Disk 1 boot code's XBIOS Floprd call.
    // This is a raw 9-sector first stage, not a packed archive or a FAT file.
    bool has_recovered_first_stage = false;
    std::uint16_t first_stage_track = 0;
    std::uint8_t first_stage_side = 0;
    std::uint8_t first_stage_sector = 0; // Atari sectors are numbered from 1.
    std::uint16_t first_stage_sector_count = 0;
    std::size_t first_stage_offset = 0;
    std::size_t first_stage_length = 0;
};

// This is the control-flow boundary within the verified Disk 1 raw stage.
// Field names describe instructions and physical media only; it is not yet a
// claim about the original game's title or simulation.
struct DeuterosAtariFirstStageProfile {
    std::size_t entry_offset = 0;
    std::size_t checksum_start_offset = 0;
    std::size_t checksum_byte_count = 0;
    std::uint32_t checksum_seed = 0;
    std::uint32_t checksum_expected = 0;
    std::uint16_t next_track = 0;
    std::uint8_t next_side = 0;
    std::uint8_t next_sector = 0;
    std::uint16_t next_sector_count = 0;
    std::uint32_t next_destination = 0;
    std::uint32_t copy_source = 0;
    std::uint32_t copy_destination = 0;
    std::size_t copy_byte_count = 0;
};

[[nodiscard]] DeuterosAtariFirstStageProfile parse_deuteros_atari_first_stage(
    std::span<const std::uint8_t> bytes);

struct DeuterosAtariSecondStageProfile {
    std::uint32_t supervisor_stack = 0;
    std::uint32_t application_stack = 0;
    std::uint32_t direct_entry = 0;
    std::size_t direct_entry_source_offset = 0;
    std::uint32_t dispatch_state_address = 0;
    std::uint32_t dispatch_table_address = 0;
    std::uint32_t dispatch_raw_reader_address = 0;
    std::size_t raw_read_routine_offset = 0;
    std::uint16_t raw_read_max_sector_count = 0;
    std::uint16_t side_switch_track = 0;
};

// Parses the raw track-2 stage loaded by the first-stage profile. It has no
// embedded title resource: its proven direct hand-off is an absolute RAM jump.
[[nodiscard]] DeuterosAtariSecondStageProfile parse_deuteros_atari_second_stage(
    std::span<const std::uint8_t> bytes);

// Static values returned by the first two table vectors. They are raw-loader
// arguments only; their game semantics and state-selection source are unknown.
struct DeuterosAtariDispatchProfile {
    std::array<std::uint32_t, 6> vector_addresses{};
    std::uint32_t state0_destination = 0;
    std::uint32_t state0_byte_count = 0;
    std::uint32_t state0_linear_sector = 0;
    std::uint32_t state1_destination = 0;
    std::uint32_t state1_byte_count = 0;
    std::uint32_t state1_linear_sector = 0;
};

[[nodiscard]] DeuterosAtariDispatchProfile parse_deuteros_atari_dispatch(
    std::span<const std::uint8_t> bytes);

class DeuterosAtariDisk {
public:
    static constexpr std::size_t standard_size = 737'280;

    explicit DeuterosAtariDisk(std::vector<std::uint8_t> image);

    [[nodiscard]] const DeuterosAtariBootProfile& boot_profile() const { return profile_; }
    [[nodiscard]] std::vector<std::uint8_t> read_sectors(
        std::uint16_t track, std::uint8_t side, std::uint8_t first_sector,
        std::uint16_t sector_count) const;

private:
    std::vector<std::uint8_t> image_;
    DeuterosAtariBootProfile profile_;
};

} // namespace eon
