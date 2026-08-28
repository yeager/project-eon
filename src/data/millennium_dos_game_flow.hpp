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
