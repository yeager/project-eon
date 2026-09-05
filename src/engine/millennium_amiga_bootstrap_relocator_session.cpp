#include "engine/millennium_amiga_bootstrap_relocator_session.hpp"

#include "data/amiga_adf.hpp"
#include "data/millennium_amiga_loader.hpp"
#include "data/sha256.hpp"

#include <stdexcept>

namespace eon {

MillenniumAmigaBootstrapRelocatorSession::MillenniumAmigaBootstrapRelocatorSession(
    const std::span<const std::uint8_t> disk_image) {
    constexpr auto disk_sha =
        "8263e19b431b61c3c34363bb282703476145a45259c94132be82b529ec13b53c";
    constexpr auto bootstrap_sha =
        "c31e59f83d6825a2da7a6fd5e3297a322993b0483105794fca449d97d3861e06";
    if (disk_image.size() != AmigaAdf::standard_size
        || to_hex(sha256(disk_image)) != disk_sha
        || to_hex(sha256(disk_image.subspan(0x400, 0x400))) != bootstrap_sha) {
        throw std::runtime_error("Unsupported Millennium Amiga bootstrap relocator media");
    }
    const AmigaAdf disk(std::vector<std::uint8_t>(disk_image.begin(), disk_image.end()));
    const auto plan = parse_millennium_amiga_load_plan(disk);
    const auto boundary = parse_millennium_amiga_bootstrap_relocation_boundary(disk, plan);
    if (boundary.copy_source_address != 0x70032
        || boundary.copy_destination_address != 0x66032
        || boundary.copy_byte_count != 0x3cf
        || boundary.copy_source_end_inclusive != 0x70400
        || boundary.relocated_continuation_address != 0x6629e) {
        throw std::runtime_error("Unsupported Millennium Amiga bootstrap relocation contract");
    }
    constexpr std::size_t first_stage_disk_offset = 0x6e000;
    constexpr std::size_t first_stage_byte_count = 0x24200;
    constexpr auto first_stage_sha =
        "df97c7f6cd622b16b9ffb57bc562906e349c18c56ed8abeb564c6f411e64891c";
    const auto first_stage = disk_image.subspan(first_stage_disk_offset,
        first_stage_byte_count);
    first_stage_sha256_ = to_hex(sha256(first_stage));
    if (first_stage_sha256_ != first_stage_sha
        || first_stage[0] != 0x60 || first_stage[1] != 0x00
        || first_stage[2] != 0x00 || first_stage[3] != 0xba) {
        throw std::runtime_error("Unsupported Millennium Amiga opaque first stage");
    }
    first_stage_bytes_.assign(first_stage.begin(), first_stage.end());

    custom_chip_effect_ = {0x70000, 0xdff104, 0x0024};
    copy_effects_.reserve(boundary.copy_byte_count);
    // $70032..$703ff is entirely inside the exact disk +$400 load. The final
    // $70400 byte is intentionally not read from the following disk byte.
    for (std::uint32_t i = 0; i < boundary.copy_byte_count - 1; ++i) {
        copy_effects_.push_back({0x70036, boundary.copy_source_address + i,
            boundary.copy_destination_address + i,
            disk_image[0x432 + i]});
    }
    final_a3_ = 0x66400;
    final_a5_ = 0x70400;
    final_d1_ = 0;
}

MillenniumAmigaBootstrapRelocatorBoundary
MillenniumAmigaBootstrapRelocatorSession::boundary() const {
    switch (state_) {
    case MillenniumAmigaBootstrapRelocatorState::awaiting_overread_byte:
        return {0x70036, 0x70400, 0x66400};
    case MillenniumAmigaBootstrapRelocatorState::awaiting_terminal_jump:
        return {0x7003c, 0, 0x6629e};
    case MillenniumAmigaBootstrapRelocatorState::awaiting_setup_call_return:
        return {0x662b2, 0, 0x66128};
    case MillenniumAmigaBootstrapRelocatorState::awaiting_first_read_return:
        return {0x662cc, 0, 0x661da};
    case MillenniumAmigaBootstrapRelocatorState::awaiting_opaque_first_stage:
        return {0x662e4, 0, 0x41000};
    case MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_illegal_exception:
        return {0x410de, 0x10, 0};
    }
    throw std::runtime_error("Invalid Millennium Amiga bootstrap relocator state");
}

MillenniumAmigaFirstStageEntryExecution
MillenniumAmigaBootstrapRelocatorSession::execute_first_stage_entry(
    const MillenniumAmigaFirstStageRegisterObservation& observation) {
    if (state_ != MillenniumAmigaBootstrapRelocatorState::awaiting_opaque_first_stage
        || observation.instruction_address != 0x41000
        || observation.stack_pointer < 4
        || (observation.stack_pointer - 4 < 0x14)
        || (observation.stack_pointer - 4 < 0x4108a
            && observation.stack_pointer > 0x4104a)
        || first_stage_bytes_.size() != 0x24200
        || first_stage_bytes_[0] != 0x60 || first_stage_bytes_[1] != 0x00
        || first_stage_bytes_[2] != 0x00 || first_stage_bytes_[3] != 0xba
        || first_stage_bytes_[0xbc] != 0x2f || first_stage_bytes_[0xbd] != 0x0e
        || first_stage_bytes_[0xde] != 0x4a || first_stage_bytes_[0xdf] != 0xfc) {
        throw std::runtime_error("Detached Millennium Amiga first-stage entry");
    }
    MillenniumAmigaFirstStageEntryExecution result;
    result.branch_target = 0x410bc;
    result.snapshot_address = 0x4104a;
    for (std::size_t i = 0; i < observation.data.size(); ++i) result.snapshot[i] = observation.data[i];
    for (std::size_t i = 0; i < observation.address.size(); ++i) result.snapshot[8+i] = observation.address[i];
    // MOVEM first stores the temporary A6=$4104a, then MOVE.L (A7)+,-8(A6)
    // restores the original A6 into that exact saved-register slot.
    result.snapshot[14] = observation.address[6];
    result.snapshot[15] = observation.stack_pointer - 4;
    result.transient_stack_address = observation.stack_pointer - 4;
    result.original_a6 = observation.address[6];
    result.installed_vector_address = 0x10;
    result.installed_vector_value = 0x410e0;
    result.resulting_d0 = observation.exception_vector_10;
    result.resulting_a6 = 0x4108a;
    result.resulting_stack_pointer = observation.stack_pointer;
    result.illegal_instruction_address = 0x410de;
    first_stage_entry_execution_ = result;
    state_ = MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_illegal_exception;
    return result;
}

void MillenniumAmigaBootstrapRelocatorSession::observe_overread_byte(
    const std::uint32_t instruction_address, const std::uint32_t source_address,
    const std::uint8_t value) {
    if (state_ != MillenniumAmigaBootstrapRelocatorState::awaiting_overread_byte
        || instruction_address != 0x70036 || source_address != 0x70400) {
        throw std::runtime_error("Detached Millennium Amiga bootstrap over-read");
    }
    copy_effects_.push_back({0x70036, 0x70400, 0x66400, value});
    final_a3_ = 0x66401;
    final_a5_ = 0x70401;
    final_d1_ = 0xffff;
    state_ = MillenniumAmigaBootstrapRelocatorState::awaiting_terminal_jump;
}

void MillenniumAmigaBootstrapRelocatorSession::observe_terminal_jump(
    const std::uint32_t instruction_address, const std::uint32_t target_address) {
    if (state_ != MillenniumAmigaBootstrapRelocatorState::awaiting_terminal_jump
        || instruction_address != 0x7003c || target_address != 0x6629e) {
        throw std::runtime_error("Detached Millennium Amiga bootstrap terminal jump");
    }
    state_ = MillenniumAmigaBootstrapRelocatorState::awaiting_setup_call_return;
}

void MillenniumAmigaBootstrapRelocatorSession::observe_setup_call_return(
    const std::uint32_t instruction_address, const std::uint32_t target_address) {
    if (state_ != MillenniumAmigaBootstrapRelocatorState::awaiting_setup_call_return
        || instruction_address != 0x662b2 || target_address != 0x66128) {
        throw std::runtime_error("Detached Millennium Amiga setup-call return");
    }
    state_ = MillenniumAmigaBootstrapRelocatorState::awaiting_first_read_return;
}

void MillenniumAmigaBootstrapRelocatorSession::observe_first_read_return(
    const std::uint32_t instruction_address, const std::uint32_t target_address,
    const std::uint8_t io_error) {
    if (state_ != MillenniumAmigaBootstrapRelocatorState::awaiting_first_read_return
        || instruction_address != 0x662cc || target_address != 0x661da
        || io_error != 0) {
        throw std::runtime_error("Detached Millennium Amiga first-read return");
    }
    state_ = MillenniumAmigaBootstrapRelocatorState::awaiting_opaque_first_stage;
}

} // namespace eon
