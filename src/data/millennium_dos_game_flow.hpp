#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace eon {

// Exact, non-semantic trace of the first record in 2200AD.EXE's F-key table.
// These fields are code/data addresses in the flat image, not host pointers.
// The handler only changes its own emulated runtime image; Project Eon keeps
// the supplied executable and save media immutable.
struct MillenniumDosFirstFunctionKeyTrace {
    std::uint16_t handler_address = 0;
    std::uint16_t selector_address = 0;
    std::uint8_t selector_value = 0;
    std::uint16_t record_pointer_table_address = 0;
    std::uint16_t selected_record_address = 0;
    std::uint16_t screen_descriptor_address = 0;
    std::uint8_t screen_descriptor_mode = 0;
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
// native call in that routine.
struct MillenniumDosFourthFunctionKeyTrace {
    std::uint16_t handler_address = 0;
    std::uint16_t initialization_guard_address = 0;
    std::uint8_t transfer_al_value = 0;
    std::uint16_t common_routine_address = 0;
    std::uint16_t first_call_address = 0;
    std::uint16_t first_runtime_byte_address = 0;
    std::uint8_t first_runtime_byte_value = 0;
    std::uint16_t second_call_address = 0;
    std::uint16_t second_runtime_byte_address = 0;
    std::uint8_t second_runtime_byte_value = 0;
    std::uint16_t third_runtime_byte_address = 0;
    std::uint8_t third_runtime_byte_value = 0;
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
};

[[nodiscard]] MillenniumDosGameFlow parse_millennium_dos_game_flow(
    std::span<const std::uint8_t> game_executable);

} // namespace eon
