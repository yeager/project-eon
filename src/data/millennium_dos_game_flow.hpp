#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace eon {

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

} // namespace eon
