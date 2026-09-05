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
    }
    throw std::runtime_error("Invalid Millennium Amiga bootstrap relocator state");
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
    const std::uint32_t instruction_address, const std::uint32_t target_address) {
    if (state_ != MillenniumAmigaBootstrapRelocatorState::awaiting_first_read_return
        || instruction_address != 0x662cc || target_address != 0x661da) {
        throw std::runtime_error("Detached Millennium Amiga first-read return");
    }
    state_ = MillenniumAmigaBootstrapRelocatorState::awaiting_opaque_first_stage;
}

} // namespace eon
