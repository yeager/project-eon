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

// Disk 2's KILLER_BOOT setup contains a complete, direct relocation edge:
// it copies forty original boot bytes to RAM $8, then jumps to the relocated
// continuation at $12.  The copied span also contains a separate indirect
// JMP through the original reset-vector cell at $4.  This profile records
// those literal, caller-connected facts without reading that cell, choosing
// an entry point, or assigning a game/protection meaning to either path.
struct DeuterosAtariKillerBootHandoff {
    std::size_t setup_offset = 0;
    std::size_t setup_byte_count = 0;
    std::string setup_sha256;
    std::size_t source_offset = 0;
    std::size_t byte_count = 0;
    std::uint32_t destination = 0;
    std::string relocated_sha256;
    std::uint32_t continuation_address = 0;
    std::size_t continuation_relocated_offset = 0;
    std::uint16_t continuation_first_opcode = 0;
    std::size_t vector_jump_relocated_offset = 0;
    std::uint16_t vector_jump_opcode = 0;
    std::uint16_t vector_jump_pointer_address = 0;
};

[[nodiscard]] DeuterosAtariKillerBootHandoff parse_deuteros_atari_killer_boot_handoff(
    std::span<const std::uint8_t> boot_sector, const DeuterosAtariBootProfile& profile);

// The KILLER_BOOT continuation is the only supplied Disk 2 path whose next
// instructions are wholly local after its literal relocation. This records
// its ten original longword copies and one iteration of its eight-longword
// clear loop as isolated emulated-RAM facts. It never reads the reset vector
// at $4, invokes a trap, touches host memory, or treats the loop as a game
// bootstrap.
struct DeuterosAtariKillerBootExecutionPrefix {
    std::uint32_t relocation_destination = 0;
    std::vector<std::uint8_t> relocated_bytes;
    std::array<std::uint32_t, 10> relocated_longwords{};
    std::uint32_t continuation_address = 0;
    std::uint32_t first_clear_address = 0;
    std::array<std::uint32_t, 8> cleared_longword_addresses{};
    std::uint32_t next_clear_address = 0;
    std::uint32_t loop_target_address = 0;
};

[[nodiscard]] DeuterosAtariKillerBootExecutionPrefix
execute_deuteros_atari_killer_boot_prefix(
    std::span<const std::uint8_t> boot_sector, const DeuterosAtariBootProfile& profile);

// The Disk 2 boot entry has one static caller-connected route to a local
// bytewise decoder.  The routine mutates bytes in its original boot buffer
// and then crosses GEMDOS via TRAP #1, so Project Eon records and evaluates
// only the bounded XOR transform in a separate vector.  It never invokes the
// service, displays the decoded bytes, or writes them back into game media.
struct DeuterosAtariKillerBootDecoderBoundary {
    std::size_t caller_offset = 0;
    std::size_t caller_byte_count = 0;
    std::string caller_sha256;
    std::size_t decoder_offset = 0;
    std::size_t decoder_byte_count = 0;
    std::string decoder_sha256;
    std::uint32_t source_address = 0;
    std::size_t source_offset = 0;
    std::size_t encoded_byte_count = 0;
    std::string encoded_sha256;
    std::uint8_t xor_immediate = 0;
    std::uint16_t gemdos_selector = 0;
    std::uint16_t trap_opcode = 0;
};

[[nodiscard]] DeuterosAtariKillerBootDecoderBoundary
parse_deuteros_atari_killer_boot_decoder_boundary(
    std::span<const std::uint8_t> boot_sector, const DeuterosAtariBootProfile& profile);

[[nodiscard]] std::vector<std::uint8_t> decode_deuteros_atari_killer_boot_message(
    std::span<const std::uint8_t> boot_sector,
    const DeuterosAtariKillerBootDecoderBoundary& boundary);

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

// The two possible SR-dependent paths in the track-2 entry join at +$18.
// From that join the original code unconditionally installs the application
// stack and jumps into the copied dispatcher.  This execution prefix models
// only those shared local instructions; it deliberately neither reads SR nor
// executes the dispatcher, whose first instruction consumes runtime RAM and
// later reaches an XBIOS/callback boundary.
struct DeuterosAtariSecondStageEntryExecutionPrefix {
    std::size_t join_offset = 0;
    std::size_t executed_byte_count = 0;
    std::string sha256;
    std::uint16_t stack_load_opcode = 0;
    std::uint32_t application_stack = 0;
    std::uint16_t jump_opcode = 0;
    std::uint32_t dispatcher_entry = 0;
    std::size_t stop_before_dispatcher_source_offset = 0;
};

[[nodiscard]] DeuterosAtariSecondStageEntryExecutionPrefix
execute_deuteros_atari_second_stage_entry_prefix(
    std::span<const std::uint8_t> bytes, const DeuterosAtariSecondStageProfile& stage);

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

// The second direct dispatch vector has a local, fully encoded continuation:
// it makes one XBIOS TRAP #14 request, cleans its six argument bytes, then
// returns the raw-reader arguments used by the separately bounded state-1
// plan. This records caller-connected machine-code facts only. It does not
// invoke XBIOS, choose state 1, or interpret the loaded interval.
struct DeuterosAtariState1ServiceBoundary {
    std::size_t callee_offset = 0;
    std::size_t callee_byte_count = 0;
    std::string callee_sha256;
    std::uint16_t longword_push_opcode = 0;
    std::uint32_t longword_argument = 0;
    std::uint16_t selector_push_opcode = 0;
    std::uint16_t xbios_selector = 0;
    std::uint16_t trap_opcode = 0;
    std::uint16_t stack_cleanup_opcode = 0;
    std::uint8_t stack_cleanup_bytes = 0;
    std::uint16_t destination_load_opcode = 0;
    std::uint32_t destination = 0;
    std::uint16_t byte_count_load_opcode = 0;
    std::uint32_t byte_count = 0;
    std::uint16_t linear_sector_load_opcode = 0;
    std::uint32_t linear_sector = 0;
    std::uint16_t return_opcode = 0;
};

[[nodiscard]] DeuterosAtariState1ServiceBoundary parse_deuteros_atari_state1_service_boundary(
    std::span<const std::uint8_t> bytes, const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariDispatchProfile& dispatch);

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

// The first post-callback callee has a literal post-service branch. Its
// target is recorded independently from the XBIOS boundary: the branch proves
// byte layout, not that the service returns or that the target executes.
struct DeuterosAtariFirstCalleeContinuation {
    std::size_t continuation_offset = 0;
    std::size_t continuation_byte_count = 0;
    std::string continuation_sha256;
    std::uint16_t immediate_load_opcode = 0;
    std::uint32_t immediate_value = 0;
    std::uint16_t absolute_store_opcode = 0;
    std::uint16_t absolute_store_address = 0;
    std::uint16_t return_opcode = 0;
};

[[nodiscard]] DeuterosAtariFirstCalleeContinuation
parse_deuteros_atari_first_callee_continuation(
    std::span<const std::uint8_t> bytes, const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariPostCallbackCalleeProfiles& callees);

// The second post-callback callee reaches the local raw-reader wrapper first.
// If that unknown external/raw-reader path returns, its next 38 bytes perform
// another literal TRAP #14 setup, then copy a fixed longword range and RTS.
// This records the post-boundary byte layout only; it does not infer that the
// wrapper or either service returns, what selector $6 means, or that the copy
// takes place.
struct DeuterosAtariSecondCalleeContinuation {
    std::size_t continuation_offset = 0;
    std::size_t continuation_byte_count = 0;
    std::string continuation_sha256;
    std::uint32_t trap_argument_address = 0;
    std::uint16_t trap_selector = 0;
    std::size_t trap_offset = 0;
    std::uint16_t trap_opcode = 0;
    std::uint16_t stack_cleanup_opcode = 0;
    std::uint32_t stack_cleanup_bytes = 0;
    std::uint32_t copy_source = 0;
    std::uint16_t copy_destination_pointer_address = 0;
    std::uint16_t copy_loop_counter = 0;
    std::uint16_t copy_move_opcode = 0;
    std::uint16_t copy_dbf_opcode = 0;
    std::int16_t copy_dbf_displacement = 0;
    std::size_t copy_loop_target_offset = 0;
    std::uint16_t return_opcode = 0;
};

[[nodiscard]] DeuterosAtariSecondCalleeContinuation
parse_deuteros_atari_second_callee_continuation(
    std::span<const std::uint8_t> bytes, const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariPostCallbackCalleeProfiles& callees);

// The local range wrapper at track-2 +$30 is the direct static target of the
// copied dispatcher's final BSR.W and of the second post-callback callee. It
// divides the caller's literal range unit, preserves its inputs, calls the
// raw reader at +$60, and has literal status/loop branches. This is a byte
// layout profile only: it does not assign a meaning or
// value to that RAM word, infer a raw-reader/XBIOS result, or claim that a
// caller reaches or returns from this wrapper.
struct DeuterosAtariRawReaderWrapperProfile {
    std::size_t wrapper_offset = 0;
    std::size_t wrapper_byte_count = 0;
    std::string wrapper_sha256;
    std::uint16_t divisor_opcode = 0;
    std::uint16_t divisor = 0;
    std::uint16_t raw_reader_bsr_opcode = 0;
    std::int16_t raw_reader_bsr_displacement = 0;
    std::size_t raw_reader_bsr_target_offset = 0;
    std::uint16_t status_test_opcode = 0;
    std::uint16_t status_word_address = 0;
    std::uint16_t nonzero_branch_opcode = 0;
    std::int8_t nonzero_branch_displacement = 0;
    std::size_t nonzero_branch_target_offset = 0;
    std::uint16_t chunk_subtract_opcode = 0;
    std::uint32_t chunk_bytes = 0;
    std::uint16_t first_terminal_branch_opcode = 0;
    std::int8_t first_terminal_branch_displacement = 0;
    std::size_t first_terminal_branch_target_offset = 0;
    std::uint16_t second_terminal_branch_opcode = 0;
    std::int8_t second_terminal_branch_displacement = 0;
    std::size_t second_terminal_branch_target_offset = 0;
    std::uint16_t destination_advance_opcode = 0;
    std::uint32_t destination_advance_bytes = 0;
    std::uint16_t unit_advance_opcode = 0;
    std::uint16_t loop_branch_opcode = 0;
    std::int8_t loop_branch_displacement = 0;
    std::size_t loop_branch_target_offset = 0;
    std::uint16_t return_helper_branch_opcode = 0;
    std::int8_t return_helper_branch_displacement = 0;
    std::size_t return_helper_target_offset = 0;
    std::size_t raw_reader_entry_offset = 0;
};

[[nodiscard]] DeuterosAtariRawReaderWrapperProfile
parse_deuteros_atari_raw_reader_wrapper(std::span<const std::uint8_t> bytes,
    const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariPostCallbackCalleeProfiles& callees);

// The wrapper's direct BSR.W target is a complete, fixed-size machine-code
// block at track-2 +$60.  This profile preserves its literal arithmetic,
// stack layout, ABI opcode, and post-ABI store/RTS bytes without assigning a
// result to the external call or treating it as a performed disk operation.
struct DeuterosAtariRawReaderCallLayout {
    std::size_t routine_offset = 0;
    std::size_t routine_byte_count = 0;
    std::string routine_sha256;
    std::uint16_t initial_count_opcode = 0;
    std::uint16_t initial_count_immediate = 0;
    std::uint16_t count_compare_opcode = 0;
    std::uint32_t count_compare_immediate = 0;
    std::uint16_t count_branch_opcode = 0;
    std::int8_t count_branch_displacement = 0;
    std::size_t count_branch_target_offset = 0;
    std::uint16_t first_word_shift_opcode = 0;
    std::uint16_t second_word_shift_opcode = 0;
    std::uint16_t word_increment_opcode = 0;
    std::uint16_t count_register_transfer_opcode = 0;
    std::uint16_t source_register_transfer_opcode = 0;
    std::uint16_t side_compare_opcode = 0;
    std::uint16_t side_compare_immediate = 0;
    std::uint16_t side_branch_opcode = 0;
    std::int8_t side_branch_displacement = 0;
    std::size_t side_branch_target_offset = 0;
    std::uint16_t alternate_side_opcode = 0;
    std::uint16_t side_adjust_opcode = 0;
    std::uint16_t side_adjust_immediate = 0;
    std::size_t abi_call_offset = 0;
    std::uint16_t abi_selector = 0;
    std::uint16_t abi_call_opcode = 0;
    std::uint16_t stack_cleanup_opcode = 0;
    std::uint32_t stack_cleanup_bytes = 0;
    std::uint16_t post_call_store_opcode = 0;
    std::uint16_t post_call_store_address = 0;
    std::uint16_t return_opcode = 0;
};

[[nodiscard]] DeuterosAtariRawReaderCallLayout
parse_deuteros_atari_raw_reader_call_layout(std::span<const std::uint8_t> bytes,
    const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariRawReaderWrapperProfile& wrapper);

// The copied dispatch table's three distinct direct targets are complete
// fixed-size blocks in the recovered second stage.  This profiles the table
// linkage and original bytes only.  It neither supplies a dispatch index nor
// claims that an indirect JSR reaches, executes, or returns from any target.
struct DeuterosAtariDirectVectorCalleeProfile {
    std::size_t vector_slot = 0;
    std::uint32_t runtime_address = 0;
    std::size_t stage_offset = 0;
    std::size_t byte_count = 0;
    std::string sha256;
    std::uint16_t first_opcode = 0;
    std::uint16_t final_word = 0;
};

struct DeuterosAtariDirectVectorCalleeProfiles {
    std::size_t vector_table_offset = 0;
    std::array<DeuterosAtariDirectVectorCalleeProfile, 3> distinct_callees{};
    std::size_t alias_branch_offset = 0;
    std::uint16_t alias_branch_opcode = 0;
    std::int8_t alias_branch_displacement = 0;
    std::size_t alias_branch_target_offset = 0;
};

[[nodiscard]] DeuterosAtariDirectVectorCalleeProfiles
parse_deuteros_atari_direct_vector_callees(std::span<const std::uint8_t> bytes,
    const DeuterosAtariSecondStageProfile& stage, const DeuterosAtariDispatchProfile& dispatch);

// The third distinct table body contains one self-contained literal-pointer
// transfer loop.  This parser records its original encoding and DBF backedge
// only.  It neither selects a vector nor claims that the surrounding body,
// transfer, later branch, or any raw-media operation executes.
struct DeuterosAtariDirectVectorTransferLoopProfile {
    std::size_t loop_block_offset = 0;
    std::size_t loop_block_byte_count = 0;
    std::string loop_block_sha256;
    std::uint16_t destination_pointer_load_opcode = 0;
    std::uint32_t destination_pointer = 0;
    std::uint16_t source_pointer_load_opcode = 0;
    std::uint32_t source_pointer = 0;
    std::uint16_t counter_load_opcode = 0;
    std::uint16_t counter_initial_value = 0;
    std::uint16_t transfer_opcode = 0;
    std::uint16_t dbf_opcode = 0;
    std::int16_t dbf_displacement = 0;
    std::size_t dbf_target_offset = 0;
};

[[nodiscard]] DeuterosAtariDirectVectorTransferLoopProfile
parse_deuteros_atari_direct_vector_transfer_loop(std::span<const std::uint8_t> bytes,
    const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariDirectVectorCalleeProfiles& callees);

// The bytes directly after the third direct vector's literal-pointer loop
// contain two immediate adjustments, a local BSR.W to the already bounded
// range wrapper, and a BRA.W to the separately bounded dispatcher return.
// This preserves their literal instruction layout only.  It does not select
// the vector, execute either branch, assign register meanings, or perform a
// raw-media operation.
struct DeuterosAtariDirectVectorTransferTailProfile {
    std::size_t tail_offset = 0;
    std::size_t tail_byte_count = 0;
    std::string tail_sha256;
    std::uint16_t first_immediate_adjust_opcode = 0;
    std::uint32_t first_immediate_adjust_value = 0;
    std::uint16_t second_immediate_adjust_opcode = 0;
    std::uint32_t second_immediate_adjust_value = 0;
    std::uint16_t literal_load_opcode = 0;
    std::uint32_t literal_load_value = 0;
    std::uint16_t range_wrapper_bsr_opcode = 0;
    std::int16_t range_wrapper_bsr_displacement = 0;
    std::size_t range_wrapper_bsr_target_offset = 0;
    std::uint16_t dispatcher_return_bra_opcode = 0;
    std::int16_t dispatcher_return_bra_displacement = 0;
    std::size_t dispatcher_return_bra_target_offset = 0;
};

[[nodiscard]] DeuterosAtariDirectVectorTransferTailProfile
parse_deuteros_atari_direct_vector_transfer_tail(std::span<const std::uint8_t> bytes,
    const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariDirectVectorCalleeProfiles& callees,
    const DeuterosAtariDirectVectorTransferLoopProfile& loop,
    const DeuterosAtariRawReaderWrapperProfile& wrapper,
    const DeuterosAtariState5ReturnProfile& state5_return);

// The copied dispatcher captures the low word of a RAM longword before its
// supervisor-service boundary, and separately contains a literal table lookup
// followed by JSR (A1).  These byte-proven layouts identify the input and
// lookup mechanics, but do not establish an initial RAM value, a service
// return, bounds checking, a selected vector, or any game meaning.
struct DeuterosAtariStateSelectionLayout {
    std::size_t input_capture_offset = 0;
    std::size_t input_capture_byte_count = 0;
    std::string input_capture_sha256;
    std::uint16_t source_longword_load_opcode = 0;
    std::uint16_t source_longword_address = 0;
    std::uint16_t state_word_store_opcode = 0;
    std::uint16_t state_word_address = 0;
    std::size_t table_lookup_offset = 0;
    std::size_t table_lookup_byte_count = 0;
    std::string table_lookup_sha256;
    std::uint16_t table_base_load_opcode = 0;
    std::uint16_t table_base_address = 0;
    std::uint16_t state_word_load_opcode = 0;
    std::uint16_t index_shift_opcode = 0;
    std::uint16_t indexed_vector_load_opcode = 0;
    std::uint16_t indexed_vector_displacement = 0;
    std::uint16_t indirect_call_opcode = 0;
};

[[nodiscard]] DeuterosAtariStateSelectionLayout
parse_deuteros_atari_state_selection_layout(std::span<const std::uint8_t> bytes,
    const DeuterosAtariSecondStageProfile& stage, const DeuterosAtariDispatchProfile& dispatch);

// The bytes immediately following the copied dispatcher's JSR (A1) retain
// the selected routine's D1/D2 convention, advance the raw-reader argument,
// and call the local range wrapper. This is only a post-indirect-call layout:
// it does not establish that a table entry is selected, that it returns, or
// that the wrapper/raw reader is reached or returns.
struct DeuterosAtariStateSelectionContinuation {
    std::size_t continuation_offset = 0;
    std::size_t continuation_byte_count = 0;
    std::string continuation_sha256;
    std::uint16_t indirect_call_opcode = 0;
    std::uint16_t d1_stack_save_opcode = 0;
    std::uint16_t raw_reader_argument_advance_opcode = 0;
    std::uint16_t raw_reader_argument_advance_bytes = 0;
    std::uint16_t d2_to_d7_opcode = 0;
    std::uint16_t raw_reader_wrapper_bsr_opcode = 0;
    std::int16_t raw_reader_wrapper_bsr_displacement = 0;
    std::size_t raw_reader_wrapper_target_offset = 0;
    std::uint16_t state_word_return_opcode = 0;
    std::uint16_t state_word_address = 0;
    std::uint16_t return_opcode = 0;
};

[[nodiscard]] DeuterosAtariStateSelectionContinuation
parse_deuteros_atari_state_selection_continuation(std::span<const std::uint8_t> bytes,
    const DeuterosAtariSecondStageProfile& stage,
    const DeuterosAtariStateSelectionLayout& layout,
    const DeuterosAtariRawReaderWrapperProfile& wrapper);

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
    // These runs identify the supplied Replicants presentation block as
    // non-game provenance.  They are hashes and offsets only: their text is
    // never returned to the UI, translated, or used as title data.
    std::size_t presentation_marker_offset = 0;
    std::size_t presentation_marker_byte_count = 0;
    std::string presentation_marker_sha256;
    std::array<std::size_t, 2> game_name_marker_offsets{};
    std::size_t game_name_marker_byte_count = 0;
    std::string game_name_marker_sha256;
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

// A release-neutral, read-only classification of one supplied Atari ST leaf.
// It deliberately recognises only the protected-media boot envelope; it does
// not interpret root bytes as files, execute the branch, or make an XBIOS
// request. This lets the launcher diagnose every verified variant without
// substituting the one Replicants chain used for deeper recovery.
struct DeuterosAtariMediaEvidence {
    enum class BootEnvelopeStatus {
        nonstandard_geometry,
        invalid_branch,
        invalid_bpb,
        invalid_checksum,
        valid,
    };

    std::size_t image_size = 0;
    BootEnvelopeStatus boot_envelope_status = BootEnvelopeStatus::nonstandard_geometry;
    bool standard_protected_geometry = false;
    bool valid_boot_profile = false;
    std::uint16_t boot_checksum = 0;
    std::uint16_t boot_branch_target = 0;
    bool recovered_replicants_first_stage = false;
    bool killer_boot_signature = false;
};

[[nodiscard]] DeuterosAtariMediaEvidence inspect_deuteros_atari_media(
    std::span<const std::uint8_t> image);

} // namespace eon
