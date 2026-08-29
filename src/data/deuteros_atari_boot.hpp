#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
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

    // The relocated 40-byte continuation starts at RAM $12. It clears
    // successive 32-byte blocks forever; this describes its literal loop
    // shape without executing it or assigning it a game-purpose.
    bool has_killer_boot_continuation_profile = false;
    std::size_t killer_boot_relocated_byte_count = 0;
    std::string killer_boot_relocated_sha256;
    std::uint32_t killer_boot_clear_start = 0;
    std::uint32_t killer_boot_clear_stride = 0;
    std::size_t killer_boot_clear_longword_count = 0;

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

// Reproduces only the first-stage's literal ADD.B (A0)+,D1 / ROL.L #8,D1
// checksum. ADD.B changes D1's low byte without propagating carry into its
// upper bytes. This is a gate on the original raw stage, not an Atari CPU or
// protection emulator.
[[nodiscard]] std::uint32_t calculate_deuteros_atari_first_stage_checksum(
    std::span<const std::uint8_t> bytes, const DeuterosAtariFirstStageProfile& profile);

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
    // Table slots 2, 3 and 4 statically resolve to the state-0 vector.
    std::array<std::uint32_t, 3> state0_alias_addresses{};
    std::uint32_t state0_destination = 0;
    std::uint32_t state0_byte_count = 0;
    std::uint32_t state0_linear_sector = 0;
    std::uint32_t state1_destination = 0;
    std::uint32_t state1_byte_count = 0;
    std::uint32_t state1_linear_sector = 0;
    std::uint32_t state5_first_destination = 0;
    std::uint32_t state5_first_byte_count = 0;
    std::uint32_t state5_first_reader_argument = 0;
    std::uint32_t state5_copy_source = 0;
    std::uint32_t state5_copy_destination = 0;
    std::uint32_t state5_copy_byte_count = 0;
    std::uint32_t state5_second_destination = 0;
    std::uint32_t state5_second_byte_count = 0;
    std::uint32_t state5_second_reader_argument = 0;
};

[[nodiscard]] DeuterosAtariDispatchProfile parse_deuteros_atari_dispatch(
    std::span<const std::uint8_t> bytes);

// The first dispatch vector provides a wholly static raw-load request. This
// plan preserves that request as four original nine-sector reads; it neither
// selects a runtime state nor interprets the resulting bytes as a title/game
// screen.
struct DeuterosAtariRawReadRequest {
    std::uint16_t track = 0;
    std::uint8_t side = 0;
    std::uint8_t first_sector = 1;
    std::uint16_t sector_count = 0;
    std::size_t source_offset = 0;
};

struct DeuterosAtariRawLoadPlan {
    std::uint32_t destination = 0;
    std::uint32_t byte_count = 0;
    std::uint32_t source_linear_sector = 0;
    std::size_t source_offset = 0;
    std::array<DeuterosAtariRawReadRequest, 4> requests{};
};

[[nodiscard]] DeuterosAtariRawLoadPlan build_deuteros_atari_state0_raw_load_plan(
    const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariDispatchProfile& dispatch);

// State 1 has equally static raw-reader arguments, but spans more than the
// fixed four side-sized reads used by state 0. This records the original
// requests, including the final short request, without selecting the state or
// assigning meaning to the destination bytes.
struct DeuterosAtariRawRangeLoadPlan {
    std::uint32_t destination = 0;
    std::uint32_t byte_count = 0;
    std::uint32_t source_linear_sector = 0;
    std::size_t source_offset = 0;
    std::vector<DeuterosAtariRawReadRequest> requests;
};

[[nodiscard]] DeuterosAtariRawRangeLoadPlan build_deuteros_atari_state1_raw_load_plan(
    const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariDispatchProfile& dispatch);

// The sixth static dispatch vector makes two raw reads around a literal copy.
// This is a data-flow record only; it does not select vector 5 or interpret
// either loaded interval as a game resource.
struct DeuterosAtariState5RawLoadPlan {
    DeuterosAtariRawRangeLoadPlan first_read;
    std::uint32_t copy_source = 0;
    std::uint32_t copy_destination = 0;
    std::uint32_t copy_byte_count = 0;
    DeuterosAtariRawRangeLoadPlan second_read;
};

[[nodiscard]] DeuterosAtariState5RawLoadPlan build_deuteros_atari_state5_raw_load_plan(
    const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariDispatchProfile& dispatch);

// Both reads in vector 5 cover one contiguous prefix of state 1's separately
// recovered raw interval. This verifies only that physical-media relationship:
// it neither selects either runtime vector nor attaches a resource/title/game
// meaning to the shared bytes.
struct DeuterosAtariState5State1Prefix {
    std::size_t source_offset = 0;
    std::size_t byte_count = 0;
    std::string sha256;
};

[[nodiscard]] DeuterosAtariState5State1Prefix
validate_deuteros_atari_state5_state1_prefix(
    const DeuterosAtariRawRangeLoadPlan& state1,
    const DeuterosAtariState5RawLoadPlan& state5,
    std::span<const std::uint8_t> state1_bytes,
    std::span<const std::uint8_t> state5_bytes);

// The sixth vector's second raw-reader return falls through a literal BRA.W
// to the copied dispatcher's local return tail. This preserves only the
// original branch and its final MOVE.W/RTS bytes; it does not select vector 5,
// perform either raw read, or attach a game meaning to the runtime word.
struct DeuterosAtariState5ReturnProfile {
    std::size_t branch_offset = 0;
    std::int16_t branch_displacement = 0;
    std::size_t branch_target_offset = 0;
    std::string branch_sha256;
    std::size_t dispatcher_tail_offset = 0;
    std::string dispatcher_tail_sha256;
    std::uint16_t state_word_address = 0;
    std::uint16_t move_word_opcode = 0;
    std::uint16_t return_opcode = 0;
};

[[nodiscard]] DeuterosAtariState5ReturnProfile parse_deuteros_atari_state5_return(
    std::span<const std::uint8_t> bytes, const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariDispatchProfile& dispatch);

// A separate copied-dispatcher route passes a literal callback to XBIOS
// selector $26. The callback's stack operations are byte-proven, but its ABI
// return provenance is intentionally not emulated or inferred.
struct DeuterosAtariSupervisorCallbackProfile {
    std::size_t callsite_offset = 0;
    std::size_t callsite_bytes = 0;
    std::string callsite_sha256;
    std::uint32_t callback_address = 0;
    std::size_t callback_offset = 0;
    std::size_t callback_bytes = 0;
    std::string callback_sha256;
    std::uint16_t callback_push_opcode = 0;
    std::uint16_t xbios_selector = 0;
    std::uint16_t trap_opcode = 0;
    std::uint16_t callback_return_address_load_opcode = 0;
    std::uint32_t callback_stack_address = 0;
    std::uint16_t callback_stack_move_opcode = 0;
    std::uint16_t callback_return_opcode = 0;
};

[[nodiscard]] DeuterosAtariSupervisorCallbackProfile parse_deuteros_atari_supervisor_callback(
    std::span<const std::uint8_t> bytes, const DeuterosAtariSecondStageProfile& stage);

// The copied dispatcher has a byte-proven continuation immediately following
// the callback's TRAP #14. It reads a distinct RAM longword, compares it to a
// literal, and conditionally skips two local BSR.Ws. This does not establish
// a TRAP result, callback frame, or either subroutine's effect.
struct DeuterosAtariSupervisorCallbackContinuation {
    std::size_t continuation_offset = 0;
    std::size_t continuation_bytes = 0;
    std::string continuation_sha256;
    std::uint16_t ram_read_opcode = 0;
    std::uint16_t ram_word_address = 0;
    std::uint16_t compare_opcode = 0;
    std::uint32_t compare_immediate = 0;
    std::uint16_t branch_opcode = 0;
    std::int8_t branch_displacement = 0;
    std::size_t branch_target_offset = 0;
    std::uint16_t first_bsr_opcode = 0;
    std::int16_t first_bsr_displacement = 0;
    std::size_t first_bsr_target_offset = 0;
    std::uint16_t second_bsr_opcode = 0;
    std::int16_t second_bsr_displacement = 0;
    std::size_t second_bsr_target_offset = 0;
};

[[nodiscard]] DeuterosAtariSupervisorCallbackContinuation
parse_deuteros_atari_supervisor_callback_continuation(
    std::span<const std::uint8_t> bytes, const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariSupervisorCallbackProfile& callback);

// The not-equal path in the post-callback comparison has two local BSR.W
// targets. These profiles preserve each target's literal setup through its
// first external/raw-reader-wrapper boundary. Neither profile claims that the
// comparison takes this path, that either BSR returns, or that XBIOS succeeds.
struct DeuterosAtariPostCallbackCalleeProfiles {
    std::size_t first_callee_offset = 0;
    std::size_t first_callee_byte_count = 0;
    std::string first_callee_sha256;
    std::uint32_t first_callee_literal = 0;
    std::uint16_t first_callee_ram_address = 0;
    std::uint16_t first_callee_trap_selector = 0;
    std::size_t first_callee_trap_offset = 0;
    std::uint16_t first_callee_trap_opcode = 0;
    std::uint16_t first_callee_stack_cleanup_opcode = 0;
    std::uint32_t first_callee_stack_cleanup_bytes = 0;
    std::size_t first_callee_post_trap_branch_offset = 0;
    std::int16_t first_callee_post_trap_branch_displacement = 0;
    std::size_t first_callee_post_trap_branch_target_offset = 0;
    std::size_t second_callee_offset = 0;
    std::size_t second_callee_prefix_byte_count = 0;
    std::string second_callee_prefix_sha256;
    std::uint32_t second_callee_byte_count = 0;
    std::uint32_t second_callee_destination = 0;
    std::uint32_t second_callee_raw_reader_argument = 0;
    std::uint16_t second_callee_bsr_opcode = 0;
    std::int16_t second_callee_bsr_displacement = 0;
    std::size_t second_callee_bsr_target_offset = 0;
};

[[nodiscard]] DeuterosAtariPostCallbackCalleeProfiles
parse_deuteros_atari_post_callback_callee_profiles(
    std::span<const std::uint8_t> bytes, const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariSupervisorCallbackContinuation& continuation);

// The state-0 load begins with a byte-identical duplicate of the recovered
// second boot stage. This records identity only: no original return path is
// known to enter the duplicate at its `$13200` load address.
struct DeuterosAtariState0DuplicateStagePrefix {
    std::size_t byte_count = 0;
    std::string sha256;
    std::size_t direct_entry_offset = 0;
    std::size_t dispatcher_offset = 0;
};

[[nodiscard]] DeuterosAtariState0DuplicateStagePrefix
parse_deuteros_atari_state0_duplicate_stage_prefix(std::span<const std::uint8_t> state0_bytes,
    std::span<const std::uint8_t> second_stage_bytes);

// A byte-bounded printable block in the state-1 raw media is preceded by an
// unconditional BRA.W encoding. It is preservation metadata, not original UI
// text: it is never rendered, translated, or assigned a runtime consumer.
struct DeuterosAtariState1SkippedAsciiBlock {
    std::size_t branch_relative_offset = 0;
    std::int16_t branch_displacement = 0;
    std::size_t ascii_relative_offset = 0;
    std::size_t ascii_byte_count = 0;
    std::size_t printable_run_count = 0;
    std::string ascii_sha256;
};

[[nodiscard]] DeuterosAtariState1SkippedAsciiBlock
parse_deuteros_atari_state1_skipped_ascii_block(
    std::span<const std::uint8_t> state1_bytes, const DeuterosAtariRawRangeLoadPlan& state1);

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
