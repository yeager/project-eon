#include "engine/millennium_dos_video_driver_load_session.hpp"

#include "data/millennium_dos_title_flow.hpp"

#include <stdexcept>

namespace eon {

MillenniumDosVideoDriverLoadSession::MillenniumDosVideoDriverLoadSession(
    const std::span<const std::uint8_t> titles_executable,
    const std::span<const std::uint8_t> mill_com,
    const std::span<const std::uint8_t> selected_driver,
    const MillenniumDosVideoDriverKind kind,
    const MillenniumDosVideoSelectorSource selector_source,
    const std::uint8_t selector, const char observed_command,
    const std::uint8_t raw_detector_result)
    : selector_source_(selector_source), selector_(selector),
      observed_command_(observed_command), raw_detector_result_(raw_detector_result),
      driver_(parse_millennium_dos_video_driver(selected_driver, kind)),
      driver_bytes_(selected_driver.begin(), selected_driver.end()) {
    if ((kind != MillenniumDosVideoDriverKind::ega640 && kind != MillenniumDosVideoDriverKind::mcga)
        || (selector_source != MillenniumDosVideoSelectorSource::command_tail
            && selector_source != MillenniumDosVideoSelectorSource::hardware_detector)
        || (selector != 1 && selector != 2)
        || (kind == MillenniumDosVideoDriverKind::ega640) != (selector == 1)
        || (selector_source == MillenniumDosVideoSelectorSource::command_tail
            && ((observed_command != 'e' && observed_command != 'E'
                    && observed_command != 'm' && observed_command != 'M')
                || (observed_command == 'e' || observed_command == 'E') != (selector == 1)
                || raw_detector_result != 0))
        || (selector_source == MillenniumDosVideoSelectorSource::hardware_detector
            && (observed_command != 0 || raw_detector_result > 2
                || ((raw_detector_result == 1) != (selector == 1)))) ) {
        throw std::runtime_error("Millennium DOS video selector and driver identity disagree");
    }
    const auto flow = parse_millennium_dos_title_flow(titles_executable, mill_com);
    if (flow.launcher_private_interrupt_loader_call_address != 0x0204
        || flow.launcher_private_interrupt_loader_call_target != 0x02cf
        || flow.launcher_private_interrupt_handler_loader_entry != 0x02cf
        || flow.launcher_private_interrupt_handler_open_service != 0x3d
        || flow.launcher_private_interrupt_handler_seek_end_service != 0x42
        || flow.launcher_private_interrupt_handler_allocate_service != 0x48
        || flow.launcher_private_interrupt_handler_rewind_service != 0x42
        || flow.launcher_private_interrupt_handler_read_service != 0x3f
        || flow.launcher_private_interrupt_handler_close_service != 0x3e
        || flow.launcher_private_interrupt_number != 0x91
        || flow.launcher_private_interrupt_handler_offset != 0
        || flow.launcher_private_interrupt_install_address != 0x0209) {
        throw std::runtime_error("Unsupported Millennium DOS video-driver loader path");
    }
    if (kind == MillenniumDosVideoDriverKind::ega640) {
        filename_ = "ega640.bin";
        filename_address_ = flow.launcher_private_interrupt_handler_first_program_address;
        if (flow.launcher_private_interrupt_handler_first_program != filename_
            || flow.launcher_private_interrupt_handler_first_selector != 1) {
            throw std::runtime_error("Unsupported Millennium DOS EGA driver selection");
        }
    } else {
        filename_ = "mcga.bin";
        filename_address_ = flow.launcher_private_interrupt_handler_other_program_address;
        if (flow.launcher_private_interrupt_handler_other_program != filename_
            || flow.launcher_private_interrupt_handler_other_selector != 2) {
            throw std::runtime_error("Unsupported Millennium DOS MCGA driver selection");
        }
    }
}

MillenniumDosVideoDriverLoadBoundary MillenniumDosVideoDriverLoadSession::boundary() const {
    switch (state_) {
    case MillenniumDosVideoDriverLoadState::awaiting_open_result:
        return {MillenniumDosVideoDriverLoadBoundaryKind::dos_result,0x02d2,0x3d00,filename_address_,0,0x21,false,false};
    case MillenniumDosVideoDriverLoadState::awaiting_seek_end_result:
        return {MillenniumDosVideoDriverLoadBoundaryKind::dos_result,0x02eb,0x4202,0,0,0x21,false,false};
    case MillenniumDosVideoDriverLoadState::awaiting_allocation_result:
        return {MillenniumDosVideoDriverLoadBoundaryKind::dos_result,0x02fa,0x4800,0,
            static_cast<std::uint16_t>((driver_bytes_.size()+15)/16),0x21,false,false};
    case MillenniumDosVideoDriverLoadState::awaiting_seek_start_result:
        return {MillenniumDosVideoDriverLoadBoundaryKind::dos_result,0x0309,0x4200,0,0,0x21,false,false};
    case MillenniumDosVideoDriverLoadState::awaiting_read_result:
        return {MillenniumDosVideoDriverLoadBoundaryKind::dos_result,0x0313,0x3f00,0,
            static_cast<std::uint16_t>(driver_bytes_.size()),0x21,false,false};
    case MillenniumDosVideoDriverLoadState::awaiting_close_result:
        return {MillenniumDosVideoDriverLoadBoundaryKind::dos_result,0x0319,0x3e00,0,0,0x21,false,false};
    case MillenniumDosVideoDriverLoadState::set_vector_request_boundary:
        return {MillenniumDosVideoDriverLoadBoundaryKind::interrupt_request,0x020c,0x2591,0,0,0x21,false,false};
    case MillenniumDosVideoDriverLoadState::set_vector_result_boundary:
    case MillenniumDosVideoDriverLoadState::set_vector_result_observed:
        return {MillenniumDosVideoDriverLoadBoundaryKind::dos_result_observation,0x020c,
            set_vector_ax_,0,0,0x21,set_vector_carry_,
            state_ == MillenniumDosVideoDriverLoadState::set_vector_result_observed};
    }
    throw std::runtime_error("Invalid Millennium DOS video-driver load state");
}

void MillenniumDosVideoDriverLoadSession::observe_open_result(
    const std::uint16_t instruction, const bool carry, const std::uint16_t ax) {
    if (state_ != MillenniumDosVideoDriverLoadState::awaiting_open_result
        || instruction != 0x02d2 || carry) {
        throw std::runtime_error("Millennium DOS video-driver open failed or detached");
    }
    file_handle_ = ax;
    state_ = MillenniumDosVideoDriverLoadState::awaiting_seek_end_result;
}

void MillenniumDosVideoDriverLoadSession::observe_seek_end_result(
    const std::uint16_t instruction, const bool carry, const std::uint16_t bx,
    const std::uint16_t ax, const std::uint16_t dx) {
    if (state_ != MillenniumDosVideoDriverLoadState::awaiting_seek_end_result
        || instruction != 0x02eb || carry || bx != file_handle_ || dx != 0
        || ax != driver_bytes_.size()) {
        throw std::runtime_error("Millennium DOS video-driver length differs from leaf");
    }
    state_ = MillenniumDosVideoDriverLoadState::awaiting_allocation_result;
}

void MillenniumDosVideoDriverLoadSession::observe_allocation_result(
    const std::uint16_t instruction, const bool carry, const std::uint16_t ax) {
    if (state_ != MillenniumDosVideoDriverLoadState::awaiting_allocation_result
        || instruction != 0x02fa || carry || ax == 0) {
        throw std::runtime_error("Millennium DOS video-driver allocation failed or detached");
    }
    load_segment_ = ax;
    state_ = MillenniumDosVideoDriverLoadState::awaiting_seek_start_result;
}

void MillenniumDosVideoDriverLoadSession::observe_seek_start_result(
    const std::uint16_t instruction, const bool carry, const std::uint16_t bx,
    const std::uint16_t ax, const std::uint16_t dx) {
    if (state_ != MillenniumDosVideoDriverLoadState::awaiting_seek_start_result
        || instruction != 0x0309 || carry || bx != file_handle_ || ax != 0 || dx != 0) {
        throw std::runtime_error("Millennium DOS video-driver rewind failed or detached");
    }
    state_ = MillenniumDosVideoDriverLoadState::awaiting_read_result;
}

void MillenniumDosVideoDriverLoadSession::observe_read_result(
    const std::uint16_t instruction, const bool carry, const std::uint16_t bx,
    const std::uint16_t ax) {
    if (state_ != MillenniumDosVideoDriverLoadState::awaiting_read_result
        || instruction != 0x0313 || carry || bx != file_handle_ || ax != driver_bytes_.size()) {
        throw std::runtime_error("Millennium DOS video-driver read was incomplete or detached");
    }
    memory_effects_.reserve(driver_bytes_.size());
    for (std::size_t n = 0; n < driver_bytes_.size(); ++n) {
        memory_effects_.push_back({0x0313,load_segment_,static_cast<std::uint16_t>(n),driver_bytes_[n]});
    }
    state_ = MillenniumDosVideoDriverLoadState::awaiting_close_result;
}

void MillenniumDosVideoDriverLoadSession::observe_close_result(
    const std::uint16_t instruction, const bool carry, const std::uint16_t bx) {
    if (state_ != MillenniumDosVideoDriverLoadState::awaiting_close_result
        || instruction != 0x0319 || carry || bx != file_handle_) {
        throw std::runtime_error("Millennium DOS video-driver close failed or detached");
    }
    state_ = MillenniumDosVideoDriverLoadState::set_vector_request_boundary;
}

void MillenniumDosVideoDriverLoadSession::observe_set_vector_request(
    const std::uint16_t instruction, const std::uint8_t interrupt,
    const std::uint16_t ax, const std::uint16_t dx) {
    if (state_ != MillenniumDosVideoDriverLoadState::set_vector_request_boundary
        || instruction != 0x020c || interrupt != 0x21 || ax != 0x2591 || dx != 0) {
        throw std::runtime_error("Detached Millennium DOS INT 21h SetVect request");
    }
    state_ = MillenniumDosVideoDriverLoadState::set_vector_result_boundary;
}

void MillenniumDosVideoDriverLoadSession::observe_set_vector_dos_result(
    const std::uint16_t instruction, const bool carry, const std::uint16_t ax) {
    if (state_ != MillenniumDosVideoDriverLoadState::set_vector_result_boundary
        || instruction != 0x020c) {
        throw std::runtime_error("Detached Millennium DOS SetVect result observation");
    }
    set_vector_carry_ = carry;
    set_vector_ax_ = ax;
    state_ = MillenniumDosVideoDriverLoadState::set_vector_result_observed;
    // This is only the externally observed DOS result boundary. No vector
    // contents, handler dispatch, or private-interrupt behavior is inferred.
}

} // namespace eon
