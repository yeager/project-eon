#pragma once

#include "data/millennium_dos_video_driver.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace eon {

struct MillenniumDosGameFunctionFourStackWord {
    std::uint16_t offset = 0;
    // Empty means the original write occurred, but its value is not known.
    std::optional<std::uint16_t> value;
};

struct MillenniumDosGameFunctionFourDriverWrite {
    std::uint16_t offset = 0;
    std::uint8_t before = 0;
    std::uint8_t after = 0;
};

struct MillenniumDosGameFunctionFourDispatchResult {
    bool accepted = false;
    std::string error;
    MillenniumDosVideoDriverKind kind = MillenniumDosVideoDriverKind::mcga;
    std::uint16_t entry_address = 0;
    std::uint16_t wrapper_call_address = 0;
    std::uint16_t wrapper_return_address = 0;
    std::uint16_t branch_call_return_address = 0;
    std::uint16_t continuation_address = 0;
    std::uint16_t handler_function_address = 0;
    std::uint8_t record_byte = 0;
    std::uint8_t masked_record_byte = 0;
    std::uint8_t driver_state_before = 0;
    std::uint8_t driver_state_after = 0;
    std::uint16_t returned_ax = 0;
    // The process advances through the hash-bound caller's next local prefix:
    // before its RET for the equal path or before INT $10 for the other path.
    std::uint16_t final_stack_pointer = 0;
    std::vector<MillenniumDosGameFunctionFourDriverWrite> driver_writes;
    std::vector<MillenniumDosGameFunctionFourStackWord> stack_words;
};

struct MillenniumDosStartupAllocationRequestBoundary {
    bool accepted = false;
    std::string error;
    std::uint16_t instruction_address = 0;
    std::uint8_t interrupt_number = 0;
    std::uint8_t ah = 0;
    std::uint16_t bx = 0;
    std::uint16_t ds = 0;
    std::uint16_t code_segment = 0;
    std::uint16_t stack_pointer = 0;
    std::vector<MillenniumDosGameFunctionFourStackWord> stack_words;
};

// Reconstruct the second English 2200AD startup INT $91 function-$04 call.
// This is limited to the hash-admitted child image and EGA640/MCGA handlers.
// `first_function_ax` is the result of the previously source-executed
// function-$1f leaf; the call branch is selected from its exact AH value.
// Unknown saved SI/DI/BP and FLAGS remain absent in the stack snapshot.
[[nodiscard]] MillenniumDosGameFunctionFourDispatchResult
execute_millennium_dos_game_function_four_dispatch(
    std::span<const std::uint8_t> exact_game_executable,
    std::uint16_t first_function_ax,
    std::span<const std::uint8_t> exact_driver_image,
    std::span<const std::uint8_t> owned_loaded_driver_image,
    MillenniumDosVideoDriverKind kind,
    std::uint16_t child_code_segment,
    std::uint16_t loaded_driver_segment,
    std::uint8_t vector_interrupt,
    std::uint16_t vector_offset,
    std::uint16_t vector_segment);

// Continue only the exact MCGA equal-return prefix from $0455 through the
// caller's RET, common setup, and allocator callee prologue. Stops at INT
// $21/AH=$4a without modeling the interrupt frame or DOS result. DX and its
// saved stack word remain unknown.
[[nodiscard]] MillenniumDosStartupAllocationRequestBoundary
advance_millennium_dos_startup_to_allocation_request(
    std::span<const std::uint8_t> exact_game_executable,
    const MillenniumDosGameFunctionFourDispatchResult& function_four,
    std::uint16_t child_code_segment);

} // namespace eon
