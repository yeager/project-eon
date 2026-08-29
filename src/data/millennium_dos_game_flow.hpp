#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace eon {

// SDL has no BIOS interrupt surface. This is the narrow host-adapter payload
// for BIOS INT 10h / AH=10h / AL=00h (set one EGA/VGA palette register): the
// original BL register selects the register and BH supplies its raw value.
// It describes requests encoded by original bytes, not a current host palette.
struct MillenniumDosEgaPaletteRegisterWrite {
    std::uint8_t register_index = 0;
    std::uint8_t color_value = 0;

    constexpr bool operator==(const MillenniumDosEgaPaletteRegisterWrite&) const = default;
};

// Exact, non-semantic trace of the first record in 2200AD.EXE's F-key table.
// These fields are code/data addresses in the flat image, not host pointers.
// The handler first calls the common display selector, so none of the setup
// stores below is a host-side effect: their reachability depends on that
// native call returning. Project Eon keeps the supplied executable and save
// media immutable.
struct MillenniumDosFirstFunctionKeyTrace {
    std::uint16_t handler_address = 0;
    std::uint16_t display_selector_call_address = 0;
    std::uint16_t setup_entry_address = 0;
    std::uint16_t selector_address = 0;
    std::uint8_t selector_value = 0;
    std::uint16_t record_pointer_table_address = 0;
    std::uint16_t selected_record_address = 0;
    std::uint16_t selected_record_storage_address = 0;
    std::uint16_t screen_descriptor_address = 0;
    std::uint8_t screen_descriptor_mode = 0;
    std::uint16_t screen_selector_storage_address = 0;
    std::uint16_t screen_descriptor_storage_address = 0;
    std::uint16_t setup_first_call_address = 0;
    std::uint8_t selected_record_byte_2 = 0;
    std::uint8_t selected_record_byte_36 = 0;
};

// Exact, non-semantic trace of the second record in 2200AD.EXE's F-key
// table. Unlike F1, the handler's initial branch is conditional on a byte
// populated at runtime, so this describes both the original gate and the
// setup reached only when the gate admits execution.
struct MillenniumDosSecondFunctionKeyTrace {
    std::uint16_t handler_address = 0;
    std::uint16_t availability_address = 0;
    std::uint8_t minimum_availability = 0;
    std::uint16_t wait_call_address = 0;
    std::uint16_t callback_slot_address = 0;
    std::uint16_t callback_address = 0;
    std::uint16_t first_record_address = 0;
    std::uint16_t record_stride = 0;
    std::uint16_t record_list_address = 0;
    std::uint16_t list_mode_address = 0;
    std::uint8_t list_mode_value = 0;
};

// Exact, non-semantic trace of table record two (raw F3 / $3d). This handler
// is guarded by two independently populated runtime words; the host never
// supplies either value.
struct MillenniumDosThirdFunctionKeyTrace {
    std::uint16_t handler_address = 0;
    std::uint16_t initialization_guard_address = 0;
    std::uint16_t availability_address = 0;
    std::uint16_t wait_call_address = 0;
    std::uint16_t callback_slot_address = 0;
    std::uint16_t callback_address = 0;
    std::uint16_t list_mode_address = 0;
    std::uint8_t list_mode_value = 0;
    std::uint16_t source_far_pointer_address = 0;
    std::uint16_t list_address = 0;
};

// Exact, non-semantic trace of table record three (raw F4 / $3e).  The
// handler declines to proceed while its runtime guard is nonzero; its admitted
// path sets AL to $02 and transfers to a short common routine.  Project Eon
// reports the code-observed writes but does not execute or emulate either
// native call in that routine. Crucially, there is no runtime write before
// the first call. The first literal write is reachable only if $4d2c returns;
// the two trailing literal writes are reachable only if $9dd5 returns. Those
// call-return conditions are part of the preservation boundary, rather than
// a host-side assumption about the native runtime.
struct MillenniumDosFourthFunctionKeyTrace {
    std::uint16_t handler_address = 0;
    std::uint16_t initialization_guard_address = 0;
    // Original code here writes zero to initialization_guard_address. Reaching
    // it still depends on prior native runtime values, so it is provenance,
    // not permission to manufacture a guard value.
    std::uint16_t initialization_guard_clear_address = 0;
    std::uint8_t transfer_al_value = 0;
    std::uint16_t common_routine_address = 0;
    std::uint16_t first_call_address = 0;
    std::uint16_t first_write_instruction_address = 0;
    std::uint16_t first_runtime_byte_address = 0;
    std::uint8_t first_runtime_byte_value = 0;
    std::uint16_t second_call_address = 0;
    std::uint16_t second_write_instruction_address = 0;
    std::uint16_t second_runtime_byte_address = 0;
    std::uint8_t second_runtime_byte_value = 0;
    std::uint16_t third_runtime_byte_address = 0;
    std::uint8_t third_runtime_byte_value = 0;
    std::uint16_t common_return_instruction_address = 0;
};

// Exact, non-semantic trace of table record four (raw F5 / $3f). Its handler
// immediately loads AL=$02 and calls four in-image routines before returning.
// F5 itself has no store before its first call. The first callee immediately
// enters another routine, so no post-call state is safe to reconstruct until
// native execution/return behavior is understood. Project Eon records that
// exact boundary without executing calls or manufacturing runtime state.
struct MillenniumDosFifthFunctionKeyTrace {
    std::uint16_t handler_address = 0;
    std::uint8_t transfer_al_value = 0;
    std::uint16_t first_call_address = 0;
    // The first instruction at first_call_address is itself a near call.
    // It is the preservation boundary: no F5-owned store precedes it.
    std::uint16_t first_call_initial_nested_call_address = 0;
    std::uint16_t second_call_address = 0;
    // The second callee begins by comparing this native mode byte to one,
    // then may wait on native input/hardware state. This is provenance only.
    std::uint16_t second_call_mode_address = 0;
    std::uint8_t second_call_mode_value = 0;
    std::uint16_t third_call_address = 0;
    std::uint16_t third_call_initial_nested_call_address = 0;
    std::uint16_t fourth_call_address = 0;
};

// Exact, non-semantic trace of table record five (raw F6 / $40).  Its native
// handler is guarded by the same runtime word as F3/F4.  On admission it
// snapshots three runtime cells, changes them after two native calls, enters
// the original $09fa polling loop, and returns. A separately verified
// follow-up routine restores those snapshots before making its own first
// native call. The route that selects that follow-up remains unproven, so
// Project Eon records the bytes but never manufactures the guard, executes
// calls, or applies either the temporary writes or restoration.
struct MillenniumDosSixthFunctionKeyTrace {
    std::uint16_t handler_address = 0;
    std::uint16_t initialization_guard_address = 0;
    std::uint16_t display_selector_call_address = 0;
    std::uint16_t command_value = 0;
    std::uint16_t first_call_address = 0;
    std::uint16_t second_call_address = 0;
    std::uint16_t saved_first_byte_address = 0;
    std::uint16_t first_byte_address = 0;
    std::uint16_t saved_second_byte_address = 0;
    std::uint16_t second_byte_address = 0;
    std::uint16_t saved_word_address = 0;
    std::uint16_t word_address = 0;
    std::uint8_t first_byte_value = 0;
    std::uint8_t second_byte_value = 0;
    std::uint16_t callback_word_value = 0;
    std::uint16_t callback_word_address = 0;
    std::uint16_t wait_call_address = 0;
    std::uint16_t restoration_handler_address = 0;
    std::uint16_t restoration_first_source_address = 0;
    std::uint16_t restoration_first_destination_address = 0;
    std::uint16_t restoration_word_source_address = 0;
    std::uint16_t restoration_word_destination_address = 0;
    std::uint16_t restoration_second_source_address = 0;
    std::uint16_t restoration_second_destination_address = 0;
    std::uint16_t restoration_first_call_address = 0;
};

// Exact, non-semantic trace of table record six (raw F7 / $41). Its native
// handler has the same $a19e admission gate as F3/F4/F6. The admitted path
// reads several native runtime words and routes them through in-image helper
// calls. Project Eon exposes only the verified control-flow literals and
// addresses; it neither supplies those values nor executes the helpers.
struct MillenniumDosSeventhFunctionKeyTrace {
    std::uint16_t handler_address = 0;
    std::uint16_t initialization_guard_address = 0;
    std::uint8_t initial_al_value = 0;
    std::uint16_t first_call_address = 0;
    std::uint16_t first_command_value = 0;
    std::uint16_t first_command_call_address = 0;
    std::uint16_t second_command_value = 0;
    std::uint16_t second_command_call_address = 0;
    std::uint16_t first_runtime_word_address = 0;
    std::uint16_t second_runtime_word_address = 0;
    std::uint16_t third_runtime_word_address = 0;
    std::uint16_t fourth_runtime_word_address = 0;
    std::uint16_t fifth_runtime_word_address = 0;
    std::uint16_t sixth_runtime_word_address = 0;
    std::uint16_t helper_a_address = 0;
    std::uint16_t helper_b_address = 0;
    std::uint16_t helper_c_address = 0;
    std::uint16_t literal_al_value = 0;
    std::uint16_t terminal_call_address = 0;
};

// Exact, non-semantic trace of table record seven (raw F8 / $42). The native
// handler resets a byte in its own runtime image, enters a local preflight
// routine, then repeatedly calls an in-image routine according to the carry
// flag produced by SHR BL,1. Project Eon does not provide BL or execute any
// of these native routines.
struct MillenniumDosEighthFunctionKeyTrace {
    std::uint16_t handler_address = 0;
    std::uint16_t reset_runtime_byte_address = 0;
    std::uint8_t reset_runtime_byte_value = 0;
    std::uint8_t initial_al_value = 0;
    std::uint16_t local_preflight_address = 0;
    std::uint16_t preflight_runtime_byte_address = 0;
    std::uint16_t preflight_enabled_call_address = 0;
    std::uint16_t decrement_runtime_byte_address = 0;
    std::uint16_t depleted_jump_address = 0;
    std::uint16_t repeated_call_address = 0;
    // Intel's ModR/M register code for BL in the verified SHR BL,1 opcode.
    std::uint16_t repeat_shift_register = 0;
};

// A hash-locked evaluation of F8's small loop after the original preflight
// routine has returned. The $09fa helper remains an ABI boundary, so callers
// must provide its observed BL returns explicitly. This never calls native
// code, writes executable/save media, or invents preflight state.
struct MillenniumDosEighthFunctionKeyRepeatLoop {
    std::uint16_t call_address = 0;
    std::uint16_t helper_address = 0;
    std::uint16_t shift_address = 0;
    std::uint16_t return_address = 0;
    std::vector<std::uint8_t> shifted_bl_values;
    std::uint8_t final_bl = 0;
};

enum class MillenniumDosEighthFunctionKeyPreflightOutcome {
    helper_boundary,
    returns,
    table_jump_boundary,
};

// The literal $731a preflight before F8's repeat loop. Both runtime bytes are
// explicit inputs; the table at $db4b and every call target remain original
// memory/code boundaries, never reconstructed host state.
struct MillenniumDosEighthFunctionKeyPreflight {
    std::uint16_t entry_address = 0;
    std::uint16_t enabled_byte_address = 0;
    std::uint8_t enabled_byte_value = 0;
    std::uint16_t helper_address = 0;
    std::uint16_t counter_byte_address = 0;
    std::uint8_t initial_counter_byte = 0;
    std::optional<std::uint8_t> decremented_counter_byte;
    std::uint16_t translation_table_address = 0;
    std::optional<std::uint8_t> translation_index;
    std::uint16_t table_jump_address = 0;
    std::uint16_t return_address = 0;
    MillenniumDosEighthFunctionKeyPreflightOutcome outcome =
        MillenniumDosEighthFunctionKeyPreflightOutcome::returns;
};

// The local prefix reached after F8's external XLAT boundary. The translated
// AL byte is supplied explicitly because $db4b is outside the COM image. This
// validates only the in-image selector table and the two original runtime-byte
// writes before the following pointer-controlled interpreter.
struct MillenniumDosEighthFunctionKeyTableJumpPrefix {
    std::uint16_t entry_address = 0;
    std::uint8_t translated_al = 0;
    std::uint16_t reset_runtime_byte_address = 0;
    std::uint8_t reset_runtime_byte_value = 0;
    std::uint16_t selected_runtime_byte_address = 0;
    std::uint8_t selected_runtime_byte_value = 0;
    std::uint16_t selector_table_address = 0;
    std::uint16_t selected_pointer = 0;
    std::uint16_t next_gate_runtime_byte_address = 0;
    std::uint16_t nonzero_gate_address = 0;
    std::uint16_t zero_gate_address = 0;
};

// The pointer-controlled portion of F8's $7948 routine has one safe local
// branch before it touches a selected record.  A nonzero $6e2f returns; a
// zero value reads the selected record and reaches the first native helper.
// The supplied gate value is an observation, not reconstructed runtime
// state.  No helper is called and no native byte is written by this trace.
enum class MillenniumDosEighthFunctionKeySelectedRecordOutcome {
    returns_without_record,
    first_helper_boundary,
};

struct MillenniumDosEighthFunctionKeySelectedRecordGate {
    std::uint16_t entry_address = 0;
    std::uint8_t translated_al = 0;
    std::uint16_t selector_table_address = 0;
    std::uint16_t selected_pointer = 0;
    std::uint16_t gate_runtime_byte_address = 0;
    std::uint8_t gate_runtime_byte_value = 0;
    std::uint16_t zero_gate_address = 0;
    std::uint16_t nonzero_gate_address = 0;
    std::uint16_t return_address = 0;
    // These fields remain absent when the nonzero gate returns before the
    // selected original record is dereferenced.
    // Positional bytes only; their higher-level record format is unrecovered.
    std::optional<std::uint8_t> record_byte_0;
    std::optional<std::uint16_t> record_word_1;
    std::optional<std::uint8_t> record_byte_3;
    std::optional<std::uint8_t> record_byte_4;
    std::optional<std::uint16_t> first_helper_call_address;
    std::optional<std::uint16_t> first_helper_address;
    MillenniumDosEighthFunctionKeySelectedRecordOutcome outcome =
        MillenniumDosEighthFunctionKeySelectedRecordOutcome::returns_without_record;
};

// The English DOS main loop routes action $0b to a short local prefix that
// toggles one observed runtime byte before its first unresolved helper. The
// input is caller-observed native state; the result describes that exact
// prefix and must not be treated as an emulated helper result. Spanish has
// matching handler bytes but an unproven dispatch route, so it is rejected.
struct MillenniumDosFirstSpecialActionPrefix {
    std::uint8_t action = 0;
    std::uint16_t dispatch_branch_address = 0;
    std::uint16_t dispatch_call_address = 0;
    std::uint16_t handler_address = 0;
    std::uint16_t runtime_byte_address = 0;
    std::uint8_t observed_runtime_byte = 0;
    std::uint8_t toggled_runtime_byte = 0;
    std::uint16_t selected_ax_value = 0;
    std::uint16_t helper_call_address = 0;
    std::uint16_t helper_address = 0;
};

// English-DOS-only static prefix shared by action $0b and an F7-owned call.
// The supplied AX is caller evidence. The native segment/table value and
// first helper return remain deliberately unresolved.
struct MillenniumDosSharedHelperPrefix {
    std::uint16_t entry_address = 0;
    std::uint16_t caller_ax = 0;
    std::uint16_t source_segment_cell_address = 0;
    std::uint16_t scratch_byte_address = 0;
    std::uint8_t scratch_byte_value = 0;
    std::uint16_t shifted_ax = 0;
    std::uint16_t lodsw_address = 0;
    std::uint16_t first_helper_call_address = 0;
    std::uint16_t first_helper_address = 0;
    std::string raw_sha256;
};

enum class MillenniumDosSecondSpecialActionOutcome {
    blocked_by_runtime_byte,
    helper_boundary,
};

// English DOS action $0c is admitted only when observed byte $da3a is zero.
// The admitted path sets AX then stops at its first native helper; neither
// branch invents runtime state or executes original code.
struct MillenniumDosSecondSpecialActionPrefix {
    std::uint8_t action = 0;
    std::uint16_t runtime_byte_address = 0;
    std::uint8_t observed_runtime_byte = 0;
    std::uint16_t blocked_loop_address = 0;
    std::uint16_t handler_address = 0;
    std::uint16_t selected_ax_value = 0;
    std::uint16_t helper_call_address = 0;
    std::uint16_t helper_address = 0;
    MillenniumDosSecondSpecialActionOutcome outcome =
        MillenniumDosSecondSpecialActionOutcome::blocked_by_runtime_byte;
};

// Exact, non-semantic trace of table record eight (raw F9 / $43). Its native
// handler has the established $a19e admission gate, clears two native bytes,
// selects the observed local mode, and can cycle through the F8 preflight
// routine while the value at $da06 is below nine. Project Eon records these
// executable facts only; it never supplies or mutates native runtime state.
struct MillenniumDosNinthFunctionKeyTrace {
    std::uint16_t handler_address = 0;
    std::uint16_t initialization_guard_address = 0;
    std::uint16_t display_selector_call_address = 0;
    std::uint16_t first_reset_runtime_byte_address = 0;
    std::uint8_t first_reset_runtime_byte_value = 0;
    std::uint8_t initial_al_value = 0;
    std::uint16_t local_mode_address = 0;
    std::uint8_t local_mode_value = 0;
    std::uint16_t second_reset_runtime_byte_address = 0;
    std::uint8_t second_reset_runtime_byte_value = 0;
    std::uint16_t enabled_runtime_byte_address = 0;
    std::uint16_t enabled_call_address = 0;
    std::uint16_t limit_runtime_byte_address = 0;
    std::uint8_t limit_value = 0;
    std::uint16_t local_preflight_address = 0;
    std::uint32_t terminal_call_address = 0;
};

// Exact, non-semantic trace of table record nine (raw F10 / $44). Its native
// handler is admitted only when the same runtime word used by F3/F4/F6/F7/F9
// is zero. The admitted path clears native bytes, uses a code-local mode byte,
// and repeats an original poll according to native flags. Project Eon exposes
// these verified operands and control-flow targets only; it never supplies the
// runtime values, executes the calls, or applies the native writes.
struct MillenniumDosTenthFunctionKeyTrace {
    std::uint16_t handler_address = 0;
    std::uint16_t initialization_guard_address = 0;
    std::uint16_t display_selector_call_address = 0;
    std::uint16_t first_reset_runtime_byte_address = 0;
    std::uint8_t first_reset_runtime_byte_value = 0;
    std::uint8_t initial_al_value = 0;
    std::uint16_t second_reset_runtime_byte_address = 0;
    std::uint8_t second_reset_runtime_byte_value = 0;
    std::uint16_t local_mode_address = 0;
    std::uint8_t local_mode_value = 0;
    std::uint16_t enabled_runtime_byte_address = 0;
    std::uint16_t enabled_call_address = 0;
    std::uint16_t limit_runtime_byte_address = 0;
    std::uint8_t limit_value = 0;
    std::uint16_t local_preflight_address = 0;
    std::uint8_t local_mode_reset_value = 0;
    std::uint16_t conditional_runtime_byte_address = 0;
    std::uint16_t conditional_call_address = 0;
    std::uint32_t first_terminal_call_address = 0;
    std::uint16_t second_terminal_call_address = 0;
    std::uint32_t third_terminal_call_address = 0;
    std::uint16_t wait_runtime_byte_address = 0;
    std::uint16_t wait_call_address = 0;
    std::uint16_t repeat_shift_register = 0;
    std::uint32_t final_call_address = 0;
};

// Narrow, code-validated facts from the English DOS 2200AD.EXE main loop.
// They describe dispatch mechanics only.  In particular, the meanings of the
// eight-byte table entries and the routines they reach have not been inferred.
struct MillenniumDosGameFlow {
    std::uint16_t entry_address = 0;
    // Static startup facts immediately after the flat COM entry's segment
    // setup. They do not claim that any called native routine returns.
    std::uint16_t startup_address = 0;
    std::uint16_t startup_stack_pointer = 0;
    std::uint16_t startup_first_call_address = 0;
    std::uint8_t startup_first_call_interrupt = 0;
    std::uint16_t startup_first_call_return_address = 0;
    // If the external private-interrupt wrapper returns, its caller stores
    // AX, then AH twice, and snapshots SP before comparing AL. These are
    // instruction-level destination facts only, not host-side state writes.
    std::uint16_t startup_first_call_return_site = 0;
    std::uint16_t startup_result_word_address = 0;
    std::uint16_t startup_result_high_byte_first_address = 0;
    std::uint16_t startup_result_high_byte_second_address = 0;
    std::uint16_t startup_stack_snapshot_address = 0;
    std::uint16_t startup_mode_compare_address = 0;
    std::uint16_t startup_mode_byte_address = 0;
    std::uint8_t startup_mode_equal_value = 0;
    std::uint32_t startup_equal_call_address = 0;
    std::uint32_t startup_other_call_address = 0;
    // The two static selector targets both prepare AX=$0004, ES=CS and
    // BX=$d19f before their own direct call back to $0124.  Their following
    // call targets are only reachable if that private-interrupt wrapper
    // returns; these remain byte-level control-flow facts, not call effects.
    std::uint16_t startup_equal_path_private_call_site = 0;
    std::uint16_t startup_equal_path_next_call_address = 0;
    std::uint16_t startup_equal_followup_write_address = 0;
    std::uint8_t startup_equal_followup_write_value = 0;
    std::uint16_t startup_other_path_private_call_site = 0;
    std::uint16_t startup_other_path_next_call_address = 0;
    std::uint16_t startup_other_followup_table_address = 0;
    std::uint8_t startup_other_followup_table_size = 0;
    std::array<std::uint8_t, 16> startup_other_followup_table_values{};
    std::uint16_t startup_other_followup_interrupt_site = 0;
    std::uint8_t startup_other_followup_interrupt_number = 0;
    std::uint8_t startup_other_followup_video_function = 0;
    std::uint8_t startup_other_followup_video_subfunction = 0;
    std::uint32_t startup_nonzero_dx_branch_address = 0;
    std::uint16_t main_loop_address = 0;
    std::uint32_t action_poll_address = 0;
    std::uint8_t special_action_0 = 0;
    std::uint8_t special_action_1 = 0;
    std::uint8_t function_key_first_action = 0;
    std::size_t function_key_count = 0;
    std::uint16_t function_key_table_address = 0;
    std::size_t function_key_table_stride = 0;
    std::uint32_t function_key_dispatch_address = 0;
    MillenniumDosFirstFunctionKeyTrace first_function_key;
    MillenniumDosSecondFunctionKeyTrace second_function_key;
    MillenniumDosThirdFunctionKeyTrace third_function_key;
    MillenniumDosFourthFunctionKeyTrace fourth_function_key;
    MillenniumDosFifthFunctionKeyTrace fifth_function_key;
    MillenniumDosSixthFunctionKeyTrace sixth_function_key;
    MillenniumDosSeventhFunctionKeyTrace seventh_function_key;
    MillenniumDosEighthFunctionKeyTrace eighth_function_key;
    MillenniumDosNinthFunctionKeyTrace ninth_function_key;
    MillenniumDosTenthFunctionKeyTrace tenth_function_key;
};

[[nodiscard]] MillenniumDosGameFlow parse_millennium_dos_game_flow(
    std::span<const std::uint8_t> game_executable);

// The caller-side continuation after the English startup mode selector. It is
// reachable only if the selected private-$0124 path returns. The local callee
// begins with an INT 21h boundary, so this profile stops there and records
// neither DOS effects nor an assumed return. The later DX split is likewise a
// static branch fact, not a host-side startup decision.
struct MillenniumDosStartupAllocationBoundary {
    std::string executable_sha256;
    std::uint16_t continuation_entry_address = 0;
    std::uint16_t allocator_call_address = 0;
    std::uint16_t allocator_entry_address = 0;
    std::uint16_t allocator_first_external_interrupt_site = 0;
    std::uint8_t allocator_first_external_interrupt = 0;
    std::uint8_t allocator_first_external_service = 0;
    std::uint16_t post_allocator_result_storage_address = 0;
    std::uint16_t dx_test_address = 0;
    std::uint16_t dx_zero_branch_address = 0;
    std::uint16_t dx_zero_branch_target = 0;
    std::uint16_t dx_nonzero_jump_address = 0;
    std::uint16_t dx_nonzero_jump_target = 0;
    std::uint16_t dx_zero_path_first_call_address = 0;
    std::uint16_t dx_zero_path_first_call_target = 0;
    std::string continuation_sha256;
    std::string allocator_prefix_sha256;
};

[[nodiscard]] MillenniumDosStartupAllocationBoundary
parse_millennium_dos_startup_allocation_boundary(
    std::span<const std::uint8_t> game_executable);

// The DX-zero successor of the allocation boundary has a separately bounded
// local route.  It selects one of four in-image names using the already
// native-populated $da05 byte, then calls a second local routine.  That
// routine replaces DX with the hash-locked SECURITY.HID name before reaching
// DOS INT 21h/AH=3Dh.  This is strictly instruction/data provenance: neither
// a DX value, a file-open result, carry, nor any subsequent code is supplied
// or executed by Project Eon.
struct MillenniumDosStartupZeroPathBoundary {
    std::string executable_sha256;
    std::uint16_t zero_path_entry_address = 0;
    std::uint16_t selector_entry_address = 0;
    std::uint16_t selector_mode_byte_address = 0;
    std::array<std::uint8_t, 3> selector_matching_values{};
    std::array<std::uint16_t, 4> selector_name_addresses{};
    std::uint16_t selector_call_address = 0;
    std::uint16_t selector_call_target = 0;
    std::uint16_t security_name_address = 0;
    std::uint16_t first_external_interrupt_site = 0;
    std::uint8_t first_external_interrupt = 0;
    std::uint8_t first_external_service = 0;
    std::uint8_t first_external_access_mode = 0;
    std::string zero_path_sha256;
    std::string selector_sha256;
    std::string security_loader_prefix_sha256;
    std::string selector_names_sha256;
    std::string security_name_sha256;
};

[[nodiscard]] MillenniumDosStartupZeroPathBoundary
parse_millennium_dos_startup_zero_path_boundary(
    std::span<const std::uint8_t> game_executable);

// This continuation is reachable only if the DX-zero selector and its
// original DOS-facing loader return. It records the next encoded local call
// and allocation request, stopping at that new DOS boundary. It never
// supplies a selector value, file-open result, return value, or allocation.
struct MillenniumDosStartupZeroContinuationBoundary {
    std::string executable_sha256;
    std::uint16_t continuation_entry_address = 0;
    std::size_t continuation_byte_count = 0;
    std::string continuation_sha256;
    std::uint16_t source_byte_address = 0;
    std::uint8_t source_byte_subtract_immediate = 0;
    std::uint16_t decoded_byte_storage_address = 0;
    std::uint16_t first_local_call_address = 0;
    std::uint16_t first_local_call_target = 0;
    std::uint16_t first_external_interrupt_site = 0;
    std::uint8_t first_external_interrupt = 0;
    std::uint8_t first_external_service = 0;
    std::uint16_t allocation_request_paragraphs = 0;
};

[[nodiscard]] MillenniumDosStartupZeroContinuationBoundary
parse_millennium_dos_startup_zero_continuation_boundary(
    std::span<const std::uint8_t> game_executable);

// The encoded successor of the conditional AH=$48 boundary.  It is reachable
// only if the preceding local helper and DOS interrupt both return.  The
// bytes store BX through a CS override, copy AX into ES, and then stop at the
// following DOS interrupt.  This exposes no return value, allocation, segment
// meaning, or DOS effect.
struct MillenniumDosStartupPostAllocationBoundary {
    std::string executable_sha256;
    std::uint16_t entry_address = 0;
    std::size_t byte_count = 0;
    std::uint16_t cs_override_store_address = 0;
    std::uint16_t cs_override_store_target_address = 0;
    std::uint16_t es_from_ax_address = 0;
    std::uint16_t first_external_interrupt_site = 0;
    std::uint8_t first_external_interrupt = 0;
    std::uint8_t first_external_service = 0;
    std::string boundary_sha256;
};

[[nodiscard]] MillenniumDosStartupPostAllocationBoundary
parse_millennium_dos_startup_post_allocation_boundary(
    std::span<const std::uint8_t> game_executable);

// The caller-side continuation immediately after the preceding AH=$49 DOS
// boundary. It is conditional on that boundary returning, so this is a
// hash-locked code trace only. The literal instructions restore the saved DX
// register, load the same far pointer into DX and SI, then make three local
// calls. The third is the already independently recorded GX loader; none of
// the calls, pointer values, DOS result, or returns are interpreted.
struct MillenniumDosStartupPostReleaseContinuation {
    std::string executable_sha256;
    std::uint16_t entry_address = 0;
    std::size_t byte_count = 0;
    std::uint16_t restore_dx_address = 0;
    std::uint16_t first_far_pointer_load_address = 0;
    std::uint16_t far_pointer_address = 0;
    std::uint16_t second_far_pointer_load_address = 0;
    std::uint16_t first_call_address = 0;
    std::uint16_t first_call_target = 0;
    std::uint16_t static_data_call_address = 0;
    std::uint16_t static_data_call_target = 0;
    std::uint16_t gx_loader_call_address = 0;
    std::uint16_t gx_loader_call_target = 0;
    std::string continuation_sha256;
};

[[nodiscard]] MillenniumDosStartupPostReleaseContinuation
parse_millennium_dos_startup_post_release_continuation(
    std::span<const std::uint8_t> game_executable);

// This is the immediate caller continuation following the independently
// bounded GX loader call.  It reinstates ES from CS, supplies two literal
// register operands, and enters the already recorded private INT $91 wrapper.
// Reaching it requires the prior loader to return; neither its parameters nor
// the wrapper's result, interrupt effect, or return are assigned a host-side
// meaning.
struct MillenniumDosStartupPostGxLoaderBoundary {
    std::string executable_sha256;
    std::uint16_t entry_address = 0;
    std::size_t byte_count = 0;
    std::uint16_t push_cs_address = 0;
    std::uint16_t pop_es_address = 0;
    std::uint16_t bx_literal = 0;
    std::uint16_t ax_literal = 0;
    std::uint16_t private_call_address = 0;
    std::uint16_t private_call_target = 0;
    std::uint8_t private_interrupt = 0;
    std::string boundary_sha256;
};

[[nodiscard]] MillenniumDosStartupPostGxLoaderBoundary
parse_millennium_dos_startup_post_gx_loader_boundary(
    std::span<const std::uint8_t> game_executable);

// The `$0124` routine is the exact target of the post-GX-loader call above.
// It is preserved as a raw, in-image wrapper: its stack operations, INT $91,
// and RET are encoded instruction facts only.  In particular, this does not
// assign an ABI, dispatch behavior, register effect, or return behavior to
// the private interrupt.
struct MillenniumDosPrivateInt91Wrapper {
    std::string executable_sha256;
    std::uint16_t entry_address = 0;
    std::size_t byte_count = 0;
    std::uint16_t caller_call_address = 0;
    std::uint16_t caller_call_target = 0;
    std::uint16_t push_ds_address = 0;
    std::uint16_t push_si_address = 0;
    std::uint16_t push_di_address = 0;
    std::uint16_t push_bp_address = 0;
    std::uint16_t push_es_address = 0;
    std::uint16_t private_interrupt_site = 0;
    std::uint8_t private_interrupt = 0;
    std::uint16_t pop_es_address = 0;
    std::uint16_t pop_bp_address = 0;
    std::uint16_t pop_di_address = 0;
    std::uint16_t pop_si_address = 0;
    std::uint16_t pop_ds_address = 0;
    std::uint16_t return_address = 0;
    std::string wrapper_sha256;
};

[[nodiscard]] MillenniumDosPrivateInt91Wrapper
parse_millennium_dos_private_int91_wrapper(
    std::span<const std::uint8_t> game_executable);

// The caller's encoded return site after the private INT $91 wrapper.  This
// static prefix reads one original runtime byte, selects among four literal
// DX/AX pairs, stores the selected DX word with a CS override, then calls an
// in-image routine. It is conditional on the wrapper returning. Neither the
// source byte, the selected registers' meaning, the store's runtime effect,
// nor the callee/interrupt behavior is reconstructed by Project Eon.
struct MillenniumDosPostInt91CallerSelector {
    std::string executable_sha256;
    std::uint16_t return_site_address = 0;
    std::size_t byte_count = 0;
    std::uint16_t source_byte_address = 0;
    std::uint16_t first_compare_address = 0;
    std::uint8_t first_compare_value = 0;
    std::uint16_t second_compare_address = 0;
    std::uint8_t second_compare_value = 0;
    std::uint16_t third_compare_address = 0;
    std::uint8_t third_compare_value = 0;
    std::uint16_t shared_store_address = 0;
    std::uint16_t shared_store_target_address = 0;
    std::uint16_t first_call_address = 0;
    std::uint16_t first_call_target = 0;
    std::string selector_sha256;
};

[[nodiscard]] MillenniumDosPostInt91CallerSelector
parse_millennium_dos_post_int91_caller_selector(
    std::span<const std::uint8_t> game_executable);

// This is the encoded caller continuation after the selector's overlay
// adapter CALL. It is retained only as a conditional static code span: six
// direct near CALLs precede a native-byte comparison, whose two locally
// encoded routes converge on repeated CS-to-DS/ES setup. No call return,
// comparison result, register state, or target behaviour is reconstructed.
struct MillenniumDosPostOverlayAdapterContinuation {
    std::string executable_sha256;
    std::uint16_t return_site_address = 0;
    std::size_t byte_count = 0;
    std::array<std::uint16_t, 6> initial_call_addresses{};
    std::array<std::uint16_t, 6> initial_call_targets{};
    std::uint16_t mode_compare_address = 0;
    std::uint16_t mode_byte_address = 0;
    std::uint8_t mode_equal_value = 0;
    std::uint16_t equal_branch_address = 0;
    std::uint16_t equal_branch_target = 0;
    std::uint16_t other_call_address = 0;
    std::uint16_t other_call_target = 0;
    std::uint16_t other_jump_address = 0;
    std::uint16_t convergence_address = 0;
    std::uint16_t equal_call_address = 0;
    std::uint16_t equal_call_target = 0;
    std::uint16_t first_push_cs_address = 0;
    std::uint16_t first_pop_ds_address = 0;
    std::uint16_t first_pop_es_address = 0;
    std::uint16_t second_pop_es_address = 0;
    std::string continuation_sha256;
};

[[nodiscard]] MillenniumDosPostOverlayAdapterContinuation
parse_millennium_dos_post_overlay_adapter_continuation(
    std::span<const std::uint8_t> game_executable);

// This is the following encoded caller span after the preceding segment
// setup. It contains fifteen local near CALL encodings, two AL tests, a
// conditional branch, and an encoded one-byte read/flip/store sequence. Its
// final branch targets the existing function-key dispatcher at $d3e2. This
// is static code provenance only: no preceding return, byte value, branch,
// register state, call return, or target effect is inferred by Project Eon.
struct MillenniumDosPostOverlayAdapterLoop {
    std::string executable_sha256;
    std::uint16_t entry_address = 0;
    std::size_t byte_count = 0;
    std::array<std::uint16_t, 15> call_addresses{};
    std::array<std::uint16_t, 15> call_targets{};
    std::uint16_t first_al_test_address = 0;
    std::uint16_t first_nonzero_branch_address = 0;
    std::uint16_t first_nonzero_branch_target = 0;
    std::uint16_t native_byte_load_address = 0;
    std::uint16_t native_byte_address = 0;
    std::uint16_t native_byte_xor_address = 0;
    std::uint8_t native_byte_xor_literal = 0;
    std::uint16_t native_byte_store_address = 0;
    std::uint16_t loop_al_test_address = 0;
    std::uint16_t loop_zero_branch_address = 0;
    std::uint16_t loop_zero_branch_target = 0;
    std::uint16_t following_dispatch_address = 0;
    std::string loop_sha256;
};

[[nodiscard]] MillenniumDosPostOverlayAdapterLoop
parse_millennium_dos_post_overlay_adapter_loop(
    std::span<const std::uint8_t> game_executable);

// The hash-identified main-loop dispatch prefix directly follows the
// post-overlay loop's fall-through address. It records only the encoded
// action comparisons, guard read, table normalization, and direct calls.
// Native AL/CL values, branch choices, indirect-table contents, and all
// callee behaviour remain outside the recovered runtime.
struct MillenniumDosPostOverlayDispatchPrefix {
    std::string executable_sha256;
    std::uint16_t entry_address = 0;
    std::size_t byte_count = 0;
    std::uint16_t first_action_compare_address = 0;
    std::uint8_t first_action_value = 0;
    std::uint16_t first_action_branch_address = 0;
    std::uint16_t first_action_branch_target = 0;
    std::uint16_t guard_load_address = 0;
    std::uint16_t guard_byte_address = 0;
    std::uint16_t guard_test_address = 0;
    std::uint16_t guard_nonzero_branch_address = 0;
    std::uint16_t guard_nonzero_branch_target = 0;
    std::uint16_t second_action_compare_address = 0;
    std::uint8_t second_action_value = 0;
    std::uint16_t second_action_unequal_branch_address = 0;
    std::uint16_t second_action_unequal_branch_target = 0;
    std::uint16_t second_action_call_address = 0;
    std::uint16_t second_action_call_target = 0;
    std::uint16_t action_base_subtract_address = 0;
    std::uint8_t action_base_value = 0;
    std::uint16_t action_limit_compare_address = 0;
    std::uint8_t action_limit_value = 0;
    std::uint16_t action_limit_branch_address = 0;
    std::uint16_t action_limit_branch_target = 0;
    std::uint16_t table_base_load_address = 0;
    std::uint16_t table_base_address = 0;
    std::uint16_t scaled_call_address = 0;
    std::uint16_t scaled_call_target = 0;
    std::uint16_t function_key_loop_jump_address = 0;
    std::uint16_t function_key_loop_jump_target = 0;
    std::uint16_t first_action_call_address = 0;
    std::uint16_t first_action_call_target = 0;
    std::uint16_t first_action_loop_jump_address = 0;
    std::uint16_t first_action_loop_jump_target = 0;
    std::string prefix_sha256;
};

[[nodiscard]] MillenniumDosPostOverlayDispatchPrefix
parse_millennium_dos_post_overlay_dispatch_prefix(
    std::span<const std::uint8_t> game_executable);

// The DX-nonzero successor is a separate, hash-locked static route.  It
// loads an immediate byte and short-jumps into an in-image continuation which
// stores that byte with a CS override, restores SP from an original cell, and
// calls a local routine.  That routine immediately reaches INT 33h.  This
// records encoded operands only: the predecessor's DX, the value in AH, the
// local call return, and all mouse-interrupt behaviour remain native and are
// not supplied or executed by Project Eon.
struct MillenniumDosStartupNonzeroPathBoundary {
    std::string executable_sha256;
    std::uint16_t nonzero_entry_address = 0;
    std::uint8_t immediate_al_value = 0;
    std::uint16_t short_jump_address = 0;
    std::uint16_t continuation_entry_address = 0;
    std::uint16_t continuation_byte_storage_address = 0;
    std::uint16_t continuation_stack_source_address = 0;
    std::uint16_t continuation_first_call_address = 0;
    std::uint16_t continuation_first_call_target = 0;
    std::uint16_t first_external_interrupt_site = 0;
    std::uint8_t first_external_interrupt = 0;
    std::uint8_t first_external_service = 0;
    std::string nonzero_entry_sha256;
    std::string continuation_sha256;
    std::string local_callee_prefix_sha256;
};

[[nodiscard]] MillenniumDosStartupNonzeroPathBoundary
parse_millennium_dos_startup_nonzero_path_boundary(
    std::span<const std::uint8_t> game_executable);

// Hash-locked raw evidence for 2200AD.EXE loading the original 2200GX.EXE
// overlay. Both executables are immutable inputs. These are only encoded
// loader/transfer facts: no DOS service, segment value, call return, overlay
// routine, screen, or resource ordering is modelled.
struct MillenniumDosGxOverlayLoadEvidence {
    std::string game_sha256;
    std::string overlay_sha256;
    std::uint16_t source_name_address = 0;
    std::uint16_t loader_entry_address = 0;
    std::uint16_t loader_segment_cell_address = 0;
    std::uint16_t first_call_address = 0;
    std::uint16_t first_call_target = 0;
    std::uint16_t second_call_address = 0;
    std::uint16_t second_call_target = 0;
    std::uint16_t third_call_address = 0;
    std::uint16_t third_call_target = 0;
    std::uint16_t loader_return_address = 0;
    std::uint16_t caller_call_address = 0;
    std::uint16_t caller_target = 0;
    std::string loader_sha256;
};

// Hash-locked static request for 2200AD4.BIN during the same original DOS
// startup. This records only encoded open/read/close control flow: the DOS
// handle/result, DS source buffer and call returns remain external boundaries.
struct MillenniumDosStaticDataLoadEvidence {
    std::uint16_t source_name_address = 0;
    std::uint16_t loader_entry_address = 0;
    std::uint16_t caller_call_address = 0;
    std::uint16_t caller_target = 0;
    std::uint16_t open_call_address = 0;
    std::uint16_t read_call_address = 0;
    std::uint16_t close_call_address = 0;
    std::uint16_t loader_return_address = 0;
    std::string caller_sha256;
    std::string loader_sha256;
    std::string source_name_sha256;
};

struct MillenniumDosGxOverlayAdapterEvidence {
    std::uint16_t entry_address = 0;
    std::uint16_t overlay_segment_cell_address = 0;
    std::uint16_t overlay_entry_offset = 0;
    std::uint16_t far_transfer_address = 0;
    std::uint16_t continuation_address = 0;
    std::uint16_t return_address = 0;
    std::string raw_sha256;
};

struct MillenniumDosGxOverlayDispatcherEvidence {
    std::uint16_t entry_offset = 0;
    std::uint16_t far_return_offset = 0;
    std::uint16_t table_offset = 0;
    std::array<std::uint16_t, 21> observed_selector_targets{};
    std::string dispatch_sha256;
    std::string table_sha256;
};

struct MillenniumDosGxOverlaySelectorEvidence {
    std::uint16_t caller_entry_address = 0;
    std::uint16_t selector_source_address = 0;
    std::array<std::uint8_t, 3> matching_selector_values{};
    std::array<std::uint16_t, 4> dx_values{};
    std::array<std::uint16_t, 4> overlay_targets{};
    std::array<std::uint16_t, 4> overlay_record_offsets{};
    std::uint16_t dx_storage_address = 0;
    std::uint16_t adapter_call_address = 0;
    std::uint16_t adapter_target = 0;
    std::string caller_sha256;
    std::string overlay_prefix_sha256;
};

// The four original GX overlay entry targets selected by 2200AD's startup
// selector converge on one compact record-copying suffix. This is raw
// executable provenance: each source record is eight original bytes, and the
// destination/state cells are instruction operands rather than a recovered
// screen, layout, or host-side renderer contract.
struct MillenniumDosGxOverlayStartupRecordEvidence {
    std::string overlay_sha256;
    std::uint16_t first_entry_offset = 0;
    std::size_t entry_span_byte_count = 0;
    std::array<std::uint16_t, 4> entry_offsets{};
    std::array<std::uint16_t, 4> source_record_offsets{};
    std::array<std::array<std::uint8_t, 8>, 4> source_records{};
    std::uint16_t shared_copy_entry_offset = 0;
    std::uint16_t copy_destination_offset = 0;
    std::uint8_t copy_word_count = 0;
    std::uint16_t copied_last_byte_storage_offset = 0;
    std::array<std::uint16_t, 3> state_word_storage_offsets{};
    std::uint16_t terminal_word_storage_offset = 0;
    std::uint16_t terminal_word_value = 0;
    std::string entry_span_sha256;
    std::string record_bank_sha256;
};

// Hash-locked continuation of dispatcher table slot 13 (+$08d0) in the
// original GX overlay. The dispatcher selects this raw target only after its
// native selector calculation. This describes the following literal setup,
// conditional return, and local back edge; it does not assign meaning to the
// reset cells, choose a selector, or execute a native callee.
struct MillenniumDosGxOverlayDispatch13Evidence {
    std::string overlay_sha256;
    std::uint16_t entry_offset = 0;
    std::size_t byte_count = 0;
    std::array<std::uint16_t, 7> call_offsets{};
    std::array<std::uint16_t, 7> call_targets{};
    std::array<std::uint16_t, 3> zeroed_word_storage_offsets{};
    std::uint16_t first_result_compare_offset = 0;
    std::uint8_t first_result_compare_value = 0;
    std::uint16_t first_result_equal_branch_offset = 0;
    std::uint16_t first_result_equal_target_offset = 0;
    std::uint16_t state_flag_test_offset = 0;
    std::uint16_t conditional_return_branch_offset = 0;
    std::uint16_t conditional_return_target_offset = 0;
    std::uint16_t state_index_increment_offset = 0;
    std::uint16_t state_index_mask_literal = 0;
    std::uint16_t state_index_stride_literal = 0;
    std::uint16_t local_back_edge_offset = 0;
    std::uint16_t local_back_edge_target_offset = 0;
    std::string span_sha256;
};

[[nodiscard]] MillenniumDosGxOverlayLoadEvidence
parse_millennium_dos_gx_overlay_load_evidence(
    std::span<const std::uint8_t> game_executable,
    std::span<const std::uint8_t> gx_overlay_executable);
[[nodiscard]] MillenniumDosStaticDataLoadEvidence
parse_millennium_dos_static_data_load_evidence(
    std::span<const std::uint8_t> game_executable);
[[nodiscard]] MillenniumDosGxOverlayAdapterEvidence
parse_millennium_dos_gx_overlay_adapter_evidence(
    std::span<const std::uint8_t> game_executable,
    const MillenniumDosGxOverlayLoadEvidence& loader);
[[nodiscard]] MillenniumDosGxOverlayDispatcherEvidence
parse_millennium_dos_gx_overlay_dispatcher_evidence(
    std::span<const std::uint8_t> gx_overlay_executable,
    const MillenniumDosGxOverlayAdapterEvidence& adapter);
[[nodiscard]] MillenniumDosGxOverlaySelectorEvidence
parse_millennium_dos_gx_overlay_selector_evidence(
    std::span<const std::uint8_t> game_executable,
    std::span<const std::uint8_t> gx_overlay_executable,
    const MillenniumDosGxOverlayAdapterEvidence& adapter,
    const MillenniumDosGxOverlayDispatcherEvidence& dispatcher);
[[nodiscard]] MillenniumDosGxOverlayStartupRecordEvidence
parse_millennium_dos_gx_overlay_startup_record_evidence(
    std::span<const std::uint8_t> gx_overlay_executable,
    const MillenniumDosGxOverlaySelectorEvidence& selector);
[[nodiscard]] MillenniumDosGxOverlayDispatch13Evidence
parse_millennium_dos_gx_overlay_dispatch13_evidence(
    std::span<const std::uint8_t> gx_overlay_executable,
    const MillenniumDosGxOverlayDispatcherEvidence& dispatcher);

// The English 2200AD.EXE startup's two local selector callees are bounded
// independently from the broad main-loop profile.  Both retain the private
// INT $91 wrapper call and their following local call as encoded targets
// only; this parser never chooses a selector result, supplies either call's
// return, or interprets a native byte read by the other route.
struct MillenniumDosEnglishGameStartupCallees {
    std::string executable_sha256;
    std::uint16_t equal_entry_address = 0;
    std::size_t equal_byte_count = 0;
    std::string equal_sha256;
    std::uint16_t equal_private_function = 0;
    std::uint16_t equal_private_record_address = 0;
    std::uint16_t equal_private_call_address = 0;
    std::uint16_t equal_private_target_address = 0;
    std::uint16_t equal_followup_call_address = 0;
    std::uint16_t equal_followup_target_address = 0;
    std::uint8_t equal_result_value = 0;
    std::uint16_t equal_result_storage_address = 0;
    std::uint16_t equal_return_address = 0;
    std::uint16_t other_entry_address = 0;
    std::size_t other_byte_count = 0;
    std::string other_sha256;
    std::uint16_t other_private_function = 0;
    std::uint16_t other_private_record_address = 0;
    std::uint16_t other_private_call_address = 0;
    std::uint16_t other_private_target_address = 0;
    std::uint16_t other_followup_call_address = 0;
    std::uint16_t other_followup_target_address = 0;
    std::uint16_t other_result_source_address = 0;
    std::uint8_t other_compare_value = 0;
    std::uint16_t other_equal_store_address = 0;
    std::uint16_t other_return_address = 0;
};

[[nodiscard]] MillenniumDosEnglishGameStartupCallees
parse_millennium_dos_english_game_startup_callees(
    std::span<const std::uint8_t> game_executable);

// The immediate English local follow-ups are separately hash-locked.  The
// palette routine stops at its first BIOS interrupt; its initial CX literal
// and loop are instruction facts, not a claimed number of executions.
struct MillenniumDosEnglishGameStartupFollowups {
    std::string executable_sha256;
    std::uint16_t equal_entry_address = 0;
    std::size_t equal_byte_count = 0;
    std::string equal_sha256;
    std::uint8_t equal_literal_value = 0;
    std::uint16_t equal_storage_address = 0;
    std::uint16_t equal_return_address = 0;
    std::uint16_t palette_entry_address = 0;
    std::size_t palette_byte_count = 0;
    std::string palette_sha256;
    std::uint16_t palette_table_address = 0;
    std::array<std::uint8_t, 16> palette_table_values{};
    std::string palette_table_sha256;
    std::uint16_t palette_initial_cx = 0;
    std::uint16_t bios_interrupt = 0;
    std::uint16_t bios_ax = 0;
    std::uint16_t palette_return_address = 0;
};

[[nodiscard]] MillenniumDosEnglishGameStartupFollowups
parse_millennium_dos_english_game_startup_followups(
    std::span<const std::uint8_t> game_executable,
    const MillenniumDosEnglishGameStartupCallees& callees);

// The Spanish FAT12 edition reaches its own TITLES.EXE and 2200AD.EXE through
// IBM.COM, not the English MILL.COM path. This records only original names,
// hashes and direct local control flow.  The hash-locked local callee also
// exposes the literal DOS EXEC register setup, but execution, carry/AL
// results and either target program's ABI remain unmodelled.
struct MillenniumDosSpanishIbmHandoffEvidence {
    std::string ibm_sha256;
    std::string titles_sha256;
    std::string game_sha256;
    std::uint16_t caller_entry_address = 0;
    std::uint16_t title_name_address = 0;
    std::uint16_t game_name_address = 0;
    std::uint16_t first_call_address = 0;
    std::uint16_t second_call_address = 0;
    std::uint16_t callee_address = 0;
    std::uint16_t first_nonzero_branch_address = 0;
    std::uint16_t second_nonzero_branch_address = 0;
    std::uint16_t callee_return_address = 0;
    std::uint16_t exec_parameter_block_address = 0;
    std::uint16_t exec_ax = 0;
    std::uint8_t exec_interrupt = 0;
    std::uint16_t carry_branch_address = 0;
    std::uint16_t carry_branch_target_address = 0;
    std::uint8_t child_status_ah = 0;
    std::uint8_t child_status_interrupt = 0;
    std::string caller_sha256;
    std::string callee_sha256;
};

[[nodiscard]] MillenniumDosSpanishIbmHandoffEvidence
parse_millennium_dos_spanish_ibm_handoff_evidence(
    std::span<const std::uint8_t> ibm_executable,
    std::span<const std::uint8_t> titles_executable,
    std::span<const std::uint8_t> game_executable);

// The Spanish 2200AD.EXE COM entry has its own hash-locked startup prefix.
// It reaches the private wrapper at $0124, then branches on a native-returned
// AL value. This profile intentionally records no return value or game state.
struct MillenniumDosSpanishGameStartupEvidence {
    std::string executable_sha256;
    std::uint16_t entry_jump_target_address = 0;
    std::uint16_t startup_entry_address = 0;
    std::size_t startup_byte_count = 0;
    std::string startup_sha256;
    std::uint16_t stack_pointer = 0;
    std::uint16_t private_function = 0;
    std::uint16_t private_record_address = 0;
    std::uint16_t private_call_address = 0;
    std::uint16_t private_wrapper_address = 0;
    std::uint16_t private_result_word_address = 0;
    std::uint16_t result_compare_address = 0;
    std::uint8_t compared_al_value = 0;
    std::uint16_t equal_call_address = 0;
    std::uint16_t equal_call_target_address = 0;
    std::uint16_t other_call_address = 0;
    std::uint16_t other_call_target_address = 0;
};

[[nodiscard]] MillenniumDosSpanishGameStartupEvidence
parse_millennium_dos_spanish_game_startup_evidence(std::span<const std::uint8_t> game_executable);

// The two local callees selected after the Spanish startup's private $0124
// result are fully bounded until their next private/follow-up call boundaries.
// The parser does not choose either caller branch or model any callee return.
struct MillenniumDosSpanishGameStartupCallees {
    std::uint16_t equal_entry_address = 0;
    std::size_t equal_byte_count = 0;
    std::string equal_sha256;
    std::uint16_t equal_private_function = 0;
    std::uint16_t equal_private_record_address = 0;
    std::uint16_t equal_private_call_address = 0;
    std::uint16_t equal_private_target_address = 0;
    std::uint16_t equal_followup_call_address = 0;
    std::uint16_t equal_followup_target_address = 0;
    std::uint8_t equal_result_value = 0;
    std::uint16_t equal_result_storage_address = 0;
    std::uint16_t equal_return_address = 0;
    std::uint16_t other_entry_address = 0;
    std::size_t other_byte_count = 0;
    std::string other_sha256;
    std::uint16_t other_private_function = 0;
    std::uint16_t other_private_record_address = 0;
    std::uint16_t other_private_call_address = 0;
    std::uint16_t other_private_target_address = 0;
    std::uint16_t other_followup_call_address = 0;
    std::uint16_t other_followup_target_address = 0;
    std::uint16_t other_result_source_address = 0;
    std::uint8_t other_compare_value = 0;
    std::uint16_t other_equal_store_address = 0;
    std::uint16_t other_return_address = 0;
};

[[nodiscard]] MillenniumDosSpanishGameStartupCallees
parse_millennium_dos_spanish_game_startup_callees(
    std::span<const std::uint8_t> game_executable,
    const MillenniumDosSpanishGameStartupEvidence& startup);

// The two direct local follow-ups reached after the Spanish startup callees.
// One writes a literal byte and returns; the other initializes CX to 16 and
// contains a BIOS palette-request loop. The external interrupt's register
// effects are unrecovered, so the initial count is not a claimed number of
// runtime iterations. Neither path is selected or executed here.
struct MillenniumDosSpanishGameStartupFollowups {
    std::uint16_t equal_entry_address = 0;
    std::size_t equal_byte_count = 0;
    std::string equal_sha256;
    std::uint8_t equal_literal_value = 0;
    std::uint16_t equal_storage_address = 0;
    std::uint16_t equal_return_address = 0;
    std::uint16_t palette_entry_address = 0;
    std::size_t palette_byte_count = 0;
    std::string palette_sha256;
    std::uint16_t palette_table_address = 0;
    std::array<std::uint8_t, 16> palette_table_values{};
    std::string palette_table_sha256;
    std::uint16_t palette_initial_cx = 0;
    std::uint16_t bios_interrupt = 0;
    std::uint16_t bios_ax = 0;
    std::uint16_t palette_return_address = 0;
};

[[nodiscard]] MillenniumDosSpanishGameStartupFollowups
parse_millennium_dos_spanish_game_startup_followups(
    std::span<const std::uint8_t> game_executable,
    const MillenniumDosSpanishGameStartupCallees& callees);

[[nodiscard]] MillenniumDosEighthFunctionKeyRepeatLoop
evaluate_millennium_dos_eighth_function_key_repeat_loop(
    std::span<const std::uint8_t> game_executable,
    std::span<const std::uint8_t> helper_return_bl_values);

[[nodiscard]] MillenniumDosEighthFunctionKeyPreflight
evaluate_millennium_dos_eighth_function_key_preflight(
    std::span<const std::uint8_t> game_executable,
    std::uint8_t enabled_byte, std::uint8_t counter_byte);

[[nodiscard]] MillenniumDosEighthFunctionKeyTableJumpPrefix
evaluate_millennium_dos_eighth_function_key_table_jump_prefix(
    std::span<const std::uint8_t> game_executable, std::uint8_t translated_al);

[[nodiscard]] MillenniumDosEighthFunctionKeySelectedRecordGate
evaluate_millennium_dos_eighth_function_key_selected_record_gate(
    std::span<const std::uint8_t> game_executable, std::uint8_t translated_al,
    std::uint8_t gate_runtime_byte);

// Validates only the English DOS action-$0b dispatch and handler prefix. It
// stops at $0666 and never invokes the original helper or persists the
// caller-observed/toggled byte to executable or save media.
[[nodiscard]] MillenniumDosFirstSpecialActionPrefix
evaluate_millennium_dos_first_special_action_prefix(
    std::span<const std::uint8_t> game_executable,
    std::uint8_t observed_runtime_byte);

// Validates the shared helper through its first internal call. It accepts only
// the hash-identified English executable and never dereferences its native
// segment/table, invokes the helper, or writes caller state.
[[nodiscard]] MillenniumDosSharedHelperPrefix
evaluate_millennium_dos_shared_helper_prefix(
    std::span<const std::uint8_t> game_executable, std::uint16_t caller_ax);

[[nodiscard]] MillenniumDosSecondSpecialActionPrefix
evaluate_millennium_dos_second_special_action_prefix(
    std::span<const std::uint8_t> game_executable,
    std::uint8_t observed_runtime_byte);

// Projects the verified, finite sequence of original BIOS palette-register
// requests into the SDL-facing adapter payload. Callers must still establish
// the conditional native path themselves; this function neither invokes BIOS
// nor applies a palette to SDL or any reconstructed runtime state.
[[nodiscard]] std::array<MillenniumDosEgaPaletteRegisterWrite, 16>
millennium_dos_startup_ega_palette_register_writes(const MillenniumDosGameFlow& flow);

} // namespace eon
