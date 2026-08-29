#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace eon {

// Byte-locked English TITLES.EXE nonzero-input closure. It exposes only local
// control-flow operands; calls, returns, and DOS termination stay native.
struct MillenniumDosTitleExitClosure {
    std::string executable_sha256;
    std::uint16_t nonzero_entry_address = 0;
    std::size_t nonzero_byte_count = 0;
    std::string nonzero_sha256;
    std::uint16_t private_driver_call_address = 0;
    std::uint16_t private_driver_target_address = 0;
    std::uint16_t post_driver_call_address = 0;
    std::uint16_t post_driver_target_address = 0;
    std::uint16_t status_clear_instruction_address = 0;
    std::uint16_t status_storage_address = 0;
    std::uint8_t status_clear_value = 0;
    std::uint16_t stack_restore_instruction_address = 0;
    std::uint16_t stack_restore_source_address = 0;
    std::uint16_t final_local_call_address = 0;
    std::uint16_t final_local_call_target_address = 0;
    std::uint16_t jump_to_exit_address = 0;
    std::uint16_t exit_stub_address = 0;
    std::uint16_t exit_stub_preceding_call_address = 0;
    std::uint16_t exit_stub_preceding_call_target_address = 0;
    std::uint8_t exit_interrupt = 0;
    std::uint8_t exit_service = 0;
    std::string exit_tail_sha256;
};

[[nodiscard]] MillenniumDosTitleExitClosure parse_millennium_dos_title_exit_closure(
    std::span<const std::uint8_t> titles_executable);

} // namespace eon
