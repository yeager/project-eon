#include "engine/millennium_dos_native_process.hpp"

#include "data/sha256.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace eon {
namespace {
constexpr std::uint16_t private_interrupt_site = 0x0129;
constexpr std::uint16_t gx_adapter_call_site = 0xd373;
constexpr std::uint16_t gx_adapter_return_site = 0xd376;
constexpr std::uint16_t gx_mode_read_site = 0xd349;
constexpr std::uint16_t gx_post_mode_read_site = 0xd388;
constexpr std::uint16_t gx_mode_byte_address = 0xda05;
constexpr std::uint16_t allocator_second_request_site = 0xd208;
constexpr std::uint16_t allocator_free_request_site = 0xd213;
constexpr std::uint16_t allocator_third_request_site = 0xd21a;
constexpr std::uint16_t allocator_file_open_site = 0xd1d4;
constexpr std::uint16_t allocator_file_seek_site = 0xd1e4;
constexpr std::uint16_t allocator_file_close_site = 0xd1ed;
constexpr std::uint16_t allocator_fourth_request_site = 0xd232;
constexpr std::uint16_t allocator_failure_site = 0xd2ac;
constexpr std::uint16_t allocator_file_error_site = 0x05a7;
}

MillenniumDosNativeProcess::MillenniumDosNativeProcess(
    const std::span<const std::uint8_t> game_executable)
    : game_executable_(game_executable) {
    startup_prefix_ = evaluate_millennium_dos_english_startup_prefix(game_executable_);
    if (startup_prefix_->outcome
            != MillenniumDosEnglishStartupPrefixOutcome::first_private_interrupt_boundary
        || startup_prefix_->boundary_address != private_interrupt_site) {
        throw std::runtime_error("Unsupported Millennium DOS native startup entry");
    }
}

MillenniumDosNativeProcess::MillenniumDosNativeProcess(
    const std::span<const std::uint8_t> game_executable,
    const std::span<const std::uint8_t> gx_overlay_executable)
    : game_executable_(game_executable),
      state_(MillenniumDosNativeProcessState::gx_private_interrupt),
      gx_session_(std::make_unique<MillenniumDosGxStartupSession>(
          game_executable, gx_overlay_executable)) {}

MillenniumDosNativeProcess MillenniumDosNativeProcess::startup(
    const std::span<const std::uint8_t> game_executable) {
    return MillenniumDosNativeProcess(game_executable);
}

MillenniumDosNativeProcess MillenniumDosNativeProcess::post_gx_loader(
    const std::span<const std::uint8_t> game_executable,
    const std::span<const std::uint8_t> gx_overlay_executable) {
    return MillenniumDosNativeProcess(game_executable, gx_overlay_executable);
}

MillenniumDosNativeBoundary MillenniumDosNativeProcess::boundary() const {
    switch (state_) {
    case MillenniumDosNativeProcessState::startup_first_private_interrupt:
    case MillenniumDosNativeProcessState::startup_selected_private_interrupt:
    case MillenniumDosNativeProcessState::gx_private_interrupt:
    case MillenniumDosNativeProcessState::gx_post_overlay_private_interrupt:
        return {MillenniumDosNativeBoundaryKind::private_interrupt,
            private_interrupt_site, std::uint8_t{0x91}, std::nullopt, 0, std::nullopt};
    case MillenniumDosNativeProcessState::startup_local_return_boundary:
        return {MillenniumDosNativeBoundaryKind::local_return,
            startup_prefix_->boundary_address, std::nullopt, std::nullopt, 0, std::nullopt};
    case MillenniumDosNativeProcessState::startup_palette_loop_return_boundary:
        return {MillenniumDosNativeBoundaryKind::local_return,
            0x047c, std::nullopt, std::nullopt, 0, std::nullopt};
    case MillenniumDosNativeProcessState::startup_ega_caller_return_boundary:
        return {MillenniumDosNativeBoundaryKind::local_return,
            0xd1d0, std::nullopt, std::nullopt, 0, std::nullopt};
    case MillenniumDosNativeProcessState::startup_palette_interrupt_boundary:
        return {MillenniumDosNativeBoundaryKind::bios_interrupt,
            startup_prefix_->boundary_address, std::uint8_t{0x10}, std::nullopt, 0, std::nullopt};
    case MillenniumDosNativeProcessState::startup_palette_interrupt_return_observed:
        return {MillenniumDosNativeBoundaryKind::observed_bios_interrupt_return,
            startup_prefix_->boundary_address, std::uint8_t{0x10}, std::nullopt, 0, std::nullopt};
    case MillenniumDosNativeProcessState::startup_allocation_interrupt_boundary:
        return {MillenniumDosNativeBoundaryKind::dos_interrupt, 0xd201,
            std::uint8_t{0x21}, std::nullopt, 0, std::uint8_t{0x4a}};
    case MillenniumDosNativeProcessState::startup_allocator_second_request_boundary:
        return {MillenniumDosNativeBoundaryKind::dos_interrupt,
            allocator_second_request_site, std::uint8_t{0x21}, std::nullopt, 0,
            std::uint8_t{0x48}};
    case MillenniumDosNativeProcessState::startup_allocator_free_request_boundary:
        return {MillenniumDosNativeBoundaryKind::dos_interrupt,
            allocator_free_request_site, std::uint8_t{0x21}, std::nullopt, 0,
            std::uint8_t{0x49}};
    case MillenniumDosNativeProcessState::startup_allocator_third_request_boundary:
        return {MillenniumDosNativeBoundaryKind::dos_interrupt,
            allocator_third_request_site, std::uint8_t{0x21}, std::nullopt, 0,
            std::uint8_t{0x48}};
    case MillenniumDosNativeProcessState::startup_allocator_file_open_request_boundary:
        return {MillenniumDosNativeBoundaryKind::dos_interrupt,
            allocator_file_open_site, std::uint8_t{0x21}, std::nullopt, 0,
            std::uint8_t{0x3d}};
    case MillenniumDosNativeProcessState::startup_allocator_file_seek_request_boundary:
        return {MillenniumDosNativeBoundaryKind::dos_interrupt,
            allocator_file_seek_site, std::uint8_t{0x21}, std::nullopt, 0,
            std::uint8_t{0x42}};
    case MillenniumDosNativeProcessState::startup_allocator_file_close_request_boundary:
        return {MillenniumDosNativeBoundaryKind::dos_interrupt,
            allocator_file_close_site, std::uint8_t{0x21}, std::nullopt, 0,
            std::uint8_t{0x3e}};
    case MillenniumDosNativeProcessState::startup_allocator_fourth_request_boundary:
        return {MillenniumDosNativeBoundaryKind::dos_interrupt,
            allocator_fourth_request_site, std::uint8_t{0x21}, std::nullopt, 0,
            std::uint8_t{0x48}};
    case MillenniumDosNativeProcessState::startup_allocator_fifth_request_boundary:
        return {MillenniumDosNativeBoundaryKind::dos_interrupt,
            0xd23e, std::uint8_t{0x21}, std::nullopt, 0, std::uint8_t{0x48}};
    case MillenniumDosNativeProcessState::startup_allocator_sixth_request_boundary:
        return {MillenniumDosNativeBoundaryKind::dos_interrupt,
            0xd251, std::uint8_t{0x21}, std::nullopt, 0, std::uint8_t{0x48}};
    case MillenniumDosNativeProcessState::startup_allocator_seventh_request_boundary:
        return {MillenniumDosNativeBoundaryKind::dos_interrupt,
            0xd25d, std::uint8_t{0x21}, std::nullopt, 0, std::uint8_t{0x48}};
    case MillenniumDosNativeProcessState::startup_allocator_eighth_request_boundary:
        return {MillenniumDosNativeBoundaryKind::dos_interrupt,
            0xd26d, std::uint8_t{0x21}, std::nullopt, 0, std::uint8_t{0x48}};
    case MillenniumDosNativeProcessState::startup_allocator_ninth_request_boundary:
        return {MillenniumDosNativeBoundaryKind::dos_interrupt,
            0xd27d, std::uint8_t{0x21}, std::nullopt, 0, std::uint8_t{0x48}};
    case MillenniumDosNativeProcessState::startup_allocator_tenth_request_boundary:
        return {MillenniumDosNativeBoundaryKind::dos_interrupt,
            0xd294, std::uint8_t{0x21}, std::nullopt, 0, std::uint8_t{0x48}};
    case MillenniumDosNativeProcessState::startup_allocator_eleventh_request_boundary:
        return {MillenniumDosNativeBoundaryKind::dos_interrupt,
            0xd2a4, std::uint8_t{0x21}, std::nullopt, 0, std::uint8_t{0x48}};
    case MillenniumDosNativeProcessState::startup_allocator_local_call_boundary:
        return {MillenniumDosNativeBoundaryKind::local_instruction,
            0xd2f5, std::nullopt, std::nullopt, 0, std::nullopt};
    case MillenniumDosNativeProcessState::startup_zero_path_file_open_request_boundary:
        return {MillenniumDosNativeBoundaryKind::dos_interrupt,
            0x054d, std::uint8_t{0x21}, std::nullopt, 0, std::uint8_t{0x3d}};
    case MillenniumDosNativeProcessState::startup_zero_path_file_read_request_boundary:
        return {MillenniumDosNativeBoundaryKind::dos_interrupt,
            0x0580, std::uint8_t{0x21}, std::nullopt, 0, std::uint8_t{0x3f}};
    case MillenniumDosNativeProcessState::startup_zero_path_file_close_request_boundary:
        return {MillenniumDosNativeBoundaryKind::dos_interrupt,
            0x05a2, std::uint8_t{0x21}, std::nullopt, 0, std::uint8_t{0x3e}};
    case MillenniumDosNativeProcessState::startup_zero_path_vector_get_request_boundary:
        return {MillenniumDosNativeBoundaryKind::dos_interrupt,
            0x4260, std::uint8_t{0x21}, std::nullopt, 0, std::uint8_t{0x35}};
    case MillenniumDosNativeProcessState::startup_zero_path_vector_set_request_boundary:
        return {MillenniumDosNativeBoundaryKind::dos_interrupt,
            0x4271, std::uint8_t{0x21}, std::nullopt, 0, std::uint8_t{0x25}};
    case MillenniumDosNativeProcessState::startup_zero_path_vector_set_return_observed:
        return {MillenniumDosNativeBoundaryKind::observed_dos_interrupt_return,
            0x4271, std::uint8_t{0x21}, std::nullopt, 0, std::uint8_t{0x25}};
    case MillenniumDosNativeProcessState::startup_zero_path_continuation_byte_boundary:
        return {MillenniumDosNativeBoundaryKind::runtime_byte,
            0xd2fd, std::nullopt, std::uint16_t{0x0082}, 0, std::nullopt};
    case MillenniumDosNativeProcessState::startup_zero_path_selector_call_boundary:
        return {MillenniumDosNativeBoundaryKind::local_instruction,
            0xd305, std::nullopt, std::nullopt, 0, std::nullopt};
    case MillenniumDosNativeProcessState::startup_sibm_timer_vector_get_boundary:
        return {MillenniumDosNativeBoundaryKind::dos_interrupt,
            0x0233, std::uint8_t{0x21}, std::nullopt, 0, std::uint8_t{0x35}};
    case MillenniumDosNativeProcessState::startup_sibm_timer_vector_set_boundary:
        return {MillenniumDosNativeBoundaryKind::dos_interrupt,
            0x0246, std::uint8_t{0x21}, std::nullopt, 0, std::uint8_t{0x25}};
    case MillenniumDosNativeProcessState::startup_sibm_timer_vector_set_return_observed:
        return {MillenniumDosNativeBoundaryKind::observed_dos_interrupt_return,
            0x0246, std::uint8_t{0x21}, std::nullopt, 0, std::uint8_t{0x25}};
    case MillenniumDosNativeProcessState::startup_allocator_failure_branch_boundary:
        return {MillenniumDosNativeBoundaryKind::local_instruction,
            allocator_failure_site, std::nullopt, std::nullopt, 0, std::nullopt};
    case MillenniumDosNativeProcessState::startup_allocator_file_error_boundary:
        return {MillenniumDosNativeBoundaryKind::local_instruction,
            allocator_file_error_site, std::nullopt, std::nullopt, 0, std::nullopt};
    case MillenniumDosNativeProcessState::gx_mode_byte:
        return {MillenniumDosNativeBoundaryKind::runtime_byte,
            gx_mode_read_site, std::nullopt, gx_mode_byte_address, 0, std::nullopt};
    case MillenniumDosNativeProcessState::gx_adapter_return:
        return {MillenniumDosNativeBoundaryKind::native_call_return,
            gx_adapter_call_site, std::nullopt, std::nullopt, 0, std::nullopt};
    case MillenniumDosNativeProcessState::gx_post_overlay_call_return: {
        const auto count = gx_session_->observed_post_overlay_call_return_count();
        return {MillenniumDosNativeBoundaryKind::native_call_return,
            static_cast<std::uint16_t>(gx_adapter_return_site + count * 3U),
            std::nullopt, std::nullopt, count, std::nullopt};
    }
    case MillenniumDosNativeProcessState::gx_post_overlay_mode_byte:
        return {MillenniumDosNativeBoundaryKind::runtime_byte,
            gx_post_mode_read_site, std::nullopt, gx_mode_byte_address, 1, std::nullopt};
    }
    throw std::runtime_error("Unsupported Millennium DOS native process state");
}

void MillenniumDosNativeProcess::observe_startup_allocation_request(
    const std::uint16_t interrupt_address) {
    if (state_ != MillenniumDosNativeProcessState::startup_local_return_boundary
        || startup_prefix_->outcome != MillenniumDosEnglishStartupPrefixOutcome::equal_return
        || startup_prefix_->boundary_address != 0x0455 || interrupt_address != 0xd201) {
        throw std::runtime_error("Detached startup DOS allocation request boundary");
    }
    state_ = MillenniumDosNativeProcessState::startup_allocation_interrupt_boundary;
}

void MillenniumDosNativeProcess::continue_startup_allocator_zero_path() {
    if (state_ != MillenniumDosNativeProcessState::startup_allocator_local_call_boundary) {
        throw std::runtime_error("Millennium zero-path continuation is detached from allocator return");
    }
    constexpr std::uint16_t load_bias = 0x100;
    constexpr std::array<std::uint8_t, 3> call_selector{0xe8,0x69,0x3e};
    if (game_executable_.size() != 54'391
        || !std::equal(call_selector.begin(), call_selector.end(),
            game_executable_.begin() + static_cast<std::ptrdiff_t>(0xd2f5 - load_bias))) {
        throw std::runtime_error("Millennium selector call source span changed");
    }
    const auto evidence = parse_millennium_dos_startup_zero_path_boundary(game_executable_);
    const auto mode = runtime_byte(evidence.selector_mode_byte_address);
    if (!mode) {
        throw std::runtime_error("Millennium DOS selector mode was not established by startup");
    }
    const auto selected_name = *mode == evidence.selector_matching_values[0]
        ? evidence.selector_name_addresses[0]
        : *mode == evidence.selector_matching_values[1]
            ? evidence.selector_name_addresses[1]
            : *mode == evidence.selector_matching_values[2]
                ? evidence.selector_name_addresses[2]
                : evidence.selector_name_addresses[3];
    // `$1161` preserves the selected DX through `$053a`; that entry's short
    // jump skips its SECURITY.HID override and reaches `$054d` with DS=CS,
    // AL=2 and AH=3Dh. Stop at INT $21; its outcome remains external.
    startup_zero_path_file_open_request_ = MillenniumDosNativeStartupFileOpenRequest{
        evidence.first_external_interrupt_site,
        static_cast<std::uint16_t>(evidence.first_external_interrupt_site + 2U),
        evidence.first_external_interrupt, evidence.first_external_service,
        static_cast<std::uint16_t>((static_cast<std::uint16_t>(evidence.first_external_service) << 8U)
            | evidence.first_external_access_mode),
        true, selected_name};
    state_ = MillenniumDosNativeProcessState::startup_zero_path_file_open_request_boundary;
}

void MillenniumDosNativeProcess::observe_dos_interrupt_return(
    const MillenniumDosNativeDosInterruptReturnObservation& observation) {
    const auto expected = boundary();
    if (state_ == MillenniumDosNativeProcessState::startup_zero_path_file_open_request_boundary) {
        if (observation.interrupt_instruction_address != 0x054d
            || observation.interrupt_number != 0x21 || observation.service != 0x3d
            || observation.return_instruction_pointer != 0x054f) {
            throw std::runtime_error("DOS return is detached from Millennium startup text open");
        }
        constexpr std::uint16_t load_bias = 0x100;
        constexpr std::array<std::uint8_t, 5> selector_return{0x73,0x03,0xe9,0x23,0xf4};
        constexpr std::array<std::uint8_t, 15> read_helper{
            0x1e,0x06,0x1e,0x0e,0x1f,0x8b,0x1e,0x00,0xda,0x1f,0xb4,0x3f,0xcd,0x21,0xe9};
        const auto hash_span = [&](const std::uint16_t address, const std::size_t size) {
            return to_hex(sha256(game_executable_.subspan(address - load_bias, size)));
        };
        if (game_executable_.size() != 54'391
            || hash_span(0x054f, 8) != "5a8be199cf08143cbaf8396f5e76c4e621b27a2d93ce4645963ff59fcb03bfe7"
            || hash_span(0x117f, selector_return.size())
                != "91ff7e5d3139966833ffe09d7c9ac5ded1f6ce3898036462a1c7d392166f7646"
            || hash_span(0x0574, read_helper.size())
                != "8898a6fec2ad72ebb0b5dfdf5b393632e421875f42bb0dce172553e3d9be10ad"
            || !std::equal(selector_return.begin(), selector_return.end(),
                game_executable_.begin() + static_cast<std::ptrdiff_t>(0x117f - load_bias))
            || !std::equal(read_helper.begin(), read_helper.end(),
                game_executable_.begin() + static_cast<std::ptrdiff_t>(0x0574 - load_bias))) {
            throw std::runtime_error("Millennium startup text open continuation changed");
        }
        startup_zero_path_file_open_dos_return_ = observation;
        runtime_bytes_[0xda00] = static_cast<std::uint8_t>(observation.ax);
        runtime_bytes_[0xda01] = static_cast<std::uint8_t>(observation.ax >> 8U);
        if ((observation.flags & 1U) != 0) {
            state_ = MillenniumDosNativeProcessState::startup_allocator_file_error_boundary;
            return;
        }
        const auto handle = observation.ax;
        const auto destination_segment_low = runtime_byte(0x011c);
        const auto destination_segment_high = runtime_byte(0x011d);
        if (!destination_segment_low || !destination_segment_high
            || game_executable_[0x011a - load_bias] != 0
            || game_executable_[0x011b - load_bias] != 0) {
            throw std::runtime_error("Millennium startup text destination is not established");
        }
        const auto destination_segment = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(*destination_segment_low)
            | (static_cast<std::uint16_t>(*destination_segment_high) << 8U));
        startup_zero_path_file_read_request_ = MillenniumDosNativeStartupFileRequest{
            0x0580, 0x0582, 0x21, 0x3f,
            static_cast<std::uint16_t>(0x3f00U | (handle & 0x00ffU)),
            handle, 0xffff, 0, destination_segment};
        state_ = MillenniumDosNativeProcessState::startup_zero_path_file_read_request_boundary;
        return;
    }
    if (state_ == MillenniumDosNativeProcessState::startup_zero_path_file_read_request_boundary) {
        if (observation.interrupt_instruction_address != 0x0580
            || observation.interrupt_number != 0x21 || observation.service != 0x3f
            || observation.return_instruction_pointer != 0x0582) {
            throw std::runtime_error("DOS return is detached from Millennium startup text read");
        }
        constexpr std::uint16_t load_bias = 0x100;
        constexpr std::array<std::uint8_t, 3> read_return{0xe9,0x1f,0x00};
        constexpr std::array<std::uint8_t, 5> read_status_branch{0x73,0x03,0xe9,0x13,0xf4};
        constexpr std::array<std::uint8_t, 15> close_helper{
            0x1e,0x06,0x1e,0x0e,0x1f,0x8b,0x1e,0x00,0xda,0x1f,0xb4,0x3e,0xcd,0x21,0x07};
        const auto hash_span = [&](const std::uint16_t address, const std::size_t size) {
            return to_hex(sha256(game_executable_.subspan(address - load_bias, size)));
        };
        if (game_executable_.size() != 54'391
            || hash_span(0x0582, read_return.size())
                != "8224e80c61df7b8377e8f7b2de471601641538aa0b17cddf3a0b7470ccc97b5b"
            || hash_span(0x118f, read_status_branch.size())
                != "b63ac75d62f92c1b4dc014fc948026e8e681c5a49bf2bd0110ab582fcd2ad7df"
            || hash_span(0x0596, close_helper.size())
                != "6b625f7ba16cc8701f5fad620944f85b59065b33309d0338bd8ef40f1c50959a"
            || !std::equal(read_return.begin(), read_return.end(),
                game_executable_.begin() + static_cast<std::ptrdiff_t>(0x0582 - load_bias))
            || !std::equal(read_status_branch.begin(), read_status_branch.end(),
                game_executable_.begin() + static_cast<std::ptrdiff_t>(0x118f - load_bias))) {
            throw std::runtime_error("Millennium startup text read continuation changed");
        }
        startup_zero_path_file_read_dos_return_ = observation;
        if ((observation.flags & 1U) != 0) {
            state_ = MillenniumDosNativeProcessState::startup_allocator_file_error_boundary;
            return;
        }
        const auto handle_low = runtime_byte(0xda00);
        const auto handle_high = runtime_byte(0xda01);
        if (!handle_low || !handle_high) {
            throw std::runtime_error("Millennium startup file handle is not established");
        }
        const auto handle = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(*handle_low)
            | (static_cast<std::uint16_t>(*handle_high) << 8U));
        startup_zero_path_file_close_request_ = MillenniumDosNativeStartupFileRequest{
            0x05a2, 0x05a4, 0x21, 0x3e,
            static_cast<std::uint16_t>(0x3e00U | (observation.ax & 0x00ffU)),
            handle, observation.cx, observation.dx, observation.return_code_segment};
        state_ = MillenniumDosNativeProcessState::startup_zero_path_file_close_request_boundary;
        return;
    }
    if (state_ == MillenniumDosNativeProcessState::startup_zero_path_file_close_request_boundary) {
        if (observation.interrupt_instruction_address != 0x05a2
            || observation.interrupt_number != 0x21 || observation.service != 0x3e
            || observation.return_instruction_pointer != 0x05a4) {
            throw std::runtime_error("DOS return is detached from Millennium startup text close");
        }
        constexpr std::uint16_t load_bias = 0x100;
        constexpr std::array<std::uint8_t, 5> close_return{0xcd,0x21,0x07,0x1f,0xc3};
        constexpr std::array<std::uint8_t, 12> close_status_branch{
            0x0e,0x1f,0x73,0x03,0xe9,0x09,0xf4,0x0e,0x1f,0xe9,0xb6,0x30};
        constexpr std::array<std::uint8_t, 26> vector_setup{
            0x06,0x57,0x1e,0x56,0xb8,0x93,0x35,0xcd,0x21,0x89,0x1e,0x55,0x42,
            0x8c,0x06,0x57,0x42,0xc5,0x16,0x1a,0x01,0xb8,0x93,0x25,0xcd,0x21};
        const auto hash_span = [&](const std::uint16_t address, const std::size_t size) {
            return to_hex(sha256(game_executable_.subspan(address - load_bias, size)));
        };
        if (game_executable_.size() != 54'391
            || hash_span(0x05a2, close_return.size())
                != "b1355acb1623a8dc2c264c4f3a43b0055dfd3e33a39d17488d57cba330aef049"
            || hash_span(0x1197, close_status_branch.size())
                != "c5e960e2300c3692fd5aa1faa5d08a3c210199393e679b214ed5d8c2e3474686"
            || hash_span(0x4259, vector_setup.size())
                != "f67c06c92b7d8e2624049b2631ebafb731c240788ece0a3a56fa2c9b4b632e5b"
            || !std::equal(close_return.begin(), close_return.end(),
                game_executable_.begin() + static_cast<std::ptrdiff_t>(0x05a2 - load_bias))
            || !std::equal(close_status_branch.begin(), close_status_branch.end(),
                game_executable_.begin() + static_cast<std::ptrdiff_t>(0x1197 - load_bias))
            || !std::equal(vector_setup.begin(), vector_setup.end(),
                game_executable_.begin() + static_cast<std::ptrdiff_t>(0x4259 - load_bias))) {
            throw std::runtime_error("Millennium startup text close continuation changed");
        }
        startup_zero_path_file_close_dos_return_ = observation;
        if ((observation.flags & 1U) != 0) {
            state_ = MillenniumDosNativeProcessState::startup_allocator_file_error_boundary;
            return;
        }
        startup_zero_path_vector_get_request_ = MillenniumDosNativeStartupFileRequest{
            0x4260, 0x4262, 0x21, 0x35, 0x3593,
            observation.bx, observation.cx, observation.dx, observation.return_code_segment};
        state_ = MillenniumDosNativeProcessState::startup_zero_path_vector_get_request_boundary;
        return;
    }
    if (state_ == MillenniumDosNativeProcessState::startup_zero_path_vector_get_request_boundary) {
        if (observation.interrupt_instruction_address != 0x4260
            || observation.interrupt_number != 0x21 || observation.service != 0x35
            || observation.return_instruction_pointer != 0x4262) {
            throw std::runtime_error("DOS return is detached from Millennium INT 93h vector query");
        }
        constexpr std::uint16_t load_bias = 0x100;
        constexpr std::array<std::uint8_t, 17> vector_return{
            0x89,0x1e,0x55,0x42,0x8c,0x06,0x57,0x42,0xc5,0x16,0x1a,0x01,
            0xb8,0x93,0x25,0xcd,0x21};
        if (game_executable_.size() != 54'391
            || to_hex(sha256(game_executable_.subspan(0x4262 - load_bias,
                vector_return.size())))
                != "e595b01819c1dbf8cb0c9f537385a79a581df889510253aca6fb21fa7b7eb405"
            || !std::equal(vector_return.begin(), vector_return.end(),
                game_executable_.begin() + static_cast<std::ptrdiff_t>(0x4262 - load_bias))) {
            throw std::runtime_error("Millennium INT 93h vector-query continuation changed");
        }
        const auto destination_segment_low = runtime_byte(0x011c);
        const auto destination_segment_high = runtime_byte(0x011d);
        if (!destination_segment_low || !destination_segment_high
            || game_executable_[0x011a - load_bias] != 0
            || game_executable_[0x011b - load_bias] != 0) {
            throw std::runtime_error("Millennium INT 93h driver vector target is not established");
        }
        startup_zero_path_vector_get_dos_return_ = observation;
        runtime_bytes_[0x4255] = static_cast<std::uint8_t>(observation.bx);
        runtime_bytes_[0x4256] = static_cast<std::uint8_t>(observation.bx >> 8U);
        runtime_bytes_[0x4257] = static_cast<std::uint8_t>(observation.es);
        runtime_bytes_[0x4258] = static_cast<std::uint8_t>(observation.es >> 8U);
        const auto destination_segment = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(*destination_segment_low)
            | (static_cast<std::uint16_t>(*destination_segment_high) << 8U));
        startup_zero_path_vector_set_request_ = MillenniumDosNativeStartupFileRequest{
            0x4271, 0x4273, 0x21, 0x25, 0x2593,
            observation.bx, observation.cx, 0, destination_segment};
        state_ = MillenniumDosNativeProcessState::startup_zero_path_vector_set_request_boundary;
        return;
    }
    if (state_ == MillenniumDosNativeProcessState::startup_zero_path_vector_set_request_boundary) {
        if (observation.interrupt_instruction_address != 0x4271
            || observation.interrupt_number != 0x21 || observation.service != 0x25
            || observation.return_instruction_pointer != 0x4273) {
            throw std::runtime_error("DOS return is detached from Millennium INT 93h vector installation");
        }
        constexpr std::uint16_t load_bias = 0x100;
        constexpr std::array<std::uint8_t, 8> vector_set_return{
            0x5e,0x1f,0x5f,0x07,0xc3,0xe9,0x02,0x6d};
        if (game_executable_.size() != 54'391
            || to_hex(sha256(game_executable_.subspan(0x4273 - load_bias,
                vector_set_return.size())))
                != "4e56863d015f9cf472c6d632265064924f22b2bab01ff8999bc39042053bb238"
            || !std::equal(vector_set_return.begin(), vector_set_return.end(),
                game_executable_.begin() + static_cast<std::ptrdiff_t>(0x4273 - load_bias))) {
            throw std::runtime_error("Millennium INT 93h vector-install continuation changed");
        }
        startup_zero_path_vector_set_dos_return_ = observation;
        const auto continuation = parse_millennium_dos_startup_zero_continuation_boundary(
            game_executable_);
        if (continuation.continuation_entry_address != 0xd2f8
            || continuation.first_local_call_address != 0xd305
            || game_executable_[0xd2f5 - load_bias] != 0xe8
            || game_executable_[0xd2f6 - load_bias] != 0x69
            || game_executable_[0xd2f7 - load_bias] != 0x3e) {
            throw std::runtime_error("Millennium startup zero-path caller return changed");
        }
        state_ = MillenniumDosNativeProcessState::startup_zero_path_continuation_byte_boundary;
        return;
    }
    if (state_ == MillenniumDosNativeProcessState::startup_allocator_second_request_boundary) {
        if (expected.kind != MillenniumDosNativeBoundaryKind::dos_interrupt
            || expected.address != observation.interrupt_instruction_address
            || expected.interrupt != observation.interrupt_number
            || expected.service != observation.service
            || observation.return_instruction_pointer != 0xd20a) {
            throw std::runtime_error("Millennium DOS return is detached from allocator INT $21/AH=$48");
        }
        constexpr std::string_view executable_sha256 =
            "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
        constexpr std::array<std::uint8_t, 11> continuation{
            0x2e,0x89,0x1e,0x2e,0xd1,0x8e,0xc0,0xb4,0x49,0xcd,0x21};
        constexpr std::uint16_t load_bias = 0x100;
        constexpr std::size_t offset = 0xd20a - load_bias;
        if (game_executable_.size() != 54'391
            || to_hex(sha256(game_executable_)) != executable_sha256
            || !std::equal(continuation.begin(), continuation.end(),
                game_executable_.begin() + static_cast<std::ptrdiff_t>(offset))
            || to_hex(sha256(game_executable_.subspan(offset, continuation.size())))
                != "9a09d46b4f907a56e959a680774053057eeb843f944f3e01a5ed614d9201e21f") {
            throw std::runtime_error("Millennium allocator AH=$48 return continuation changed");
        }
        startup_allocator_second_dos_return_ = observation;
        startup_allocator_stored_bx_ = observation.bx;
        startup_allocator_es_segment_ = observation.ax;
        runtime_bytes_[0xd12e] = static_cast<std::uint8_t>(observation.bx);
        runtime_bytes_[0xd12f] = static_cast<std::uint8_t>(observation.bx >> 8U);
        state_ = MillenniumDosNativeProcessState::startup_allocator_free_request_boundary;
        return;
    }
    if (state_ == MillenniumDosNativeProcessState::startup_allocator_free_request_boundary) {
        if (expected.kind != MillenniumDosNativeBoundaryKind::dos_interrupt
            || expected.address != observation.interrupt_instruction_address
            || expected.interrupt != observation.interrupt_number
            || expected.service != observation.service
            || observation.return_instruction_pointer != 0xd215) {
            throw std::runtime_error("Millennium DOS return is detached from allocator INT $21/AH=$49");
        }
        constexpr std::string_view executable_sha256 =
            "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
        constexpr std::array<std::uint8_t, 7> continuation{
            0xbb,0xa6,0x00,0xb4,0x48,0xcd,0x21};
        constexpr std::uint16_t load_bias = 0x100;
        constexpr std::size_t offset = 0xd215 - load_bias;
        if (game_executable_.size() != 54'391
            || to_hex(sha256(game_executable_)) != executable_sha256
            || !std::equal(continuation.begin(), continuation.end(),
                game_executable_.begin() + static_cast<std::ptrdiff_t>(offset))
            || to_hex(sha256(game_executable_.subspan(offset, continuation.size())))
                != "05c669cf64c063a1b1bd193c23baa231616d54d6539abdb371f144dbdfba2585") {
            throw std::runtime_error("Millennium allocator AH=$49 return continuation changed");
        }
        startup_allocator_free_dos_return_ = observation;
        state_ = MillenniumDosNativeProcessState::startup_allocator_third_request_boundary;
        return;
    }
    if (state_ == MillenniumDosNativeProcessState::startup_allocator_third_request_boundary) {
        if (expected.kind != MillenniumDosNativeBoundaryKind::dos_interrupt
            || expected.address != observation.interrupt_instruction_address
            || expected.interrupt != observation.interrupt_number
            || expected.service != observation.service
            || observation.return_instruction_pointer != 0xd21c) {
            throw std::runtime_error("Millennium DOS return is detached from allocator INT $21/AH=$48 third request");
        }
        constexpr std::string_view executable_sha256 =
            "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
        constexpr std::array<std::uint8_t, 20> continuation{
            0xbb,0x12,0x01,0x2e,0x89,0x47,0x02,0x73,0x03,0xe9,
            0x84,0x00,0x0e,0x1f,0xba,0x0d,0x10,0xe8,0xa1,0xff};
        constexpr std::uint16_t load_bias = 0x100;
        constexpr std::size_t offset = 0xd21c - load_bias;
        if (game_executable_.size() != 54'391
            || to_hex(sha256(game_executable_)) != executable_sha256
            || !std::equal(continuation.begin(), continuation.end(),
                game_executable_.begin() + static_cast<std::ptrdiff_t>(offset))
            || to_hex(sha256(game_executable_.subspan(offset, continuation.size())))
                != "cb53aa38a52efd09e2a30a53da2706b3c7c89855bfe75a968b86201602cc756d"
            || to_hex(sha256(game_executable_.subspan(0xd1d1 - load_bias, 5U)))
                != "06a31ffeae96544b136159050eabb961328023c749e073cd9e9e0b752a905884") {
            throw std::runtime_error("Millennium allocator third AH=$48 return continuation changed");
        }
        startup_allocator_third_dos_return_ = observation;
        runtime_bytes_[0xd114] = static_cast<std::uint8_t>(observation.ax);
        runtime_bytes_[0xd115] = static_cast<std::uint8_t>(observation.ax >> 8U);
        if ((observation.flags & 1U) != 0) {
            state_ = MillenniumDosNativeProcessState::startup_allocator_failure_branch_boundary;
            return;
        }
        constexpr std::array<std::uint8_t, 5> open_request{0xb8,0x00,0x3d,0xcd,0x21};
        if (!std::equal(open_request.begin(), open_request.end(),
                game_executable_.begin() + static_cast<std::ptrdiff_t>(0xd1d1 - load_bias))) {
            throw std::runtime_error("Millennium allocator file-open request source span changed");
        }
        startup_file_open_request_ = MillenniumDosNativeStartupFileOpenRequest{
            0xd1d4, 0xd1d6, 0x21, 0x3d, 0x3d00, true, 0x100d};
        state_ = MillenniumDosNativeProcessState::startup_allocator_file_open_request_boundary;
        return;
    }
    constexpr std::uint16_t load_bias = 0x100;
    constexpr std::string_view file_helper_sha256 =
        "53e07981c3bc29e486661becbefa47ac6e9b3d84c6744f6f779a2156c2c27ba6";
    if (state_ == MillenniumDosNativeProcessState::startup_allocator_file_open_request_boundary) {
        if (observation.interrupt_instruction_address != allocator_file_open_site
            || observation.interrupt_number != 0x21 || observation.service != 0x3d
            || observation.return_code_segment == 0
            || observation.return_instruction_pointer != 0xd1d6
            || to_hex(sha256(game_executable_.subspan(0xd1d1 - load_bias, 0x20U)))
                != file_helper_sha256) {
            throw std::runtime_error("Millennium allocator file-open return is detached from its exact helper");
        }
        if ((observation.flags & 1U) != 0) {
            state_ = MillenniumDosNativeProcessState::startup_allocator_file_error_boundary;
            return;
        }
        startup_file_handle_ = observation.ax;
        startup_file_seek_request_ = MillenniumDosNativeStartupFileRequest{
            0xd1e4, 0xd1e6, 0x21, 0x42, 0x4202, observation.ax, 0, 0};
        state_ = MillenniumDosNativeProcessState::startup_allocator_file_seek_request_boundary;
        return;
    }
    if (state_ == MillenniumDosNativeProcessState::startup_allocator_file_seek_request_boundary) {
        if (observation.interrupt_instruction_address != allocator_file_seek_site
            || observation.interrupt_number != 0x21 || observation.service != 0x42
            || observation.return_instruction_pointer != 0xd1e6
            || !startup_file_handle_
            || to_hex(sha256(game_executable_.subspan(0xd1d1 - load_bias, 0x20U)))
                != file_helper_sha256) {
            throw std::runtime_error("Millennium allocator file-seek return is detached from its exact helper");
        }
        if ((observation.flags & 1U) != 0) {
            state_ = MillenniumDosNativeProcessState::startup_allocator_file_error_boundary;
            return;
        }
        startup_file_size_ = observation.ax;
        startup_file_close_request_ = MillenniumDosNativeStartupFileRequest{
            0xd1ed, 0xd1ef, 0x21, 0x3e, 0x3e00, *startup_file_handle_, 0, 0};
        state_ = MillenniumDosNativeProcessState::startup_allocator_file_close_request_boundary;
        return;
    }
    if (state_ == MillenniumDosNativeProcessState::startup_allocator_file_close_request_boundary) {
        if (observation.interrupt_instruction_address != allocator_file_close_site
            || observation.interrupt_number != 0x21 || observation.service != 0x3e
            || observation.return_instruction_pointer != 0xd1ef
            || !startup_file_handle_ || !startup_file_size_
            || to_hex(sha256(game_executable_.subspan(0xd1d1 - load_bias, 0x20U)))
                != file_helper_sha256) {
            throw std::runtime_error("Millennium allocator file-close return is detached from its exact helper");
        }
        const auto rounded_file_size = static_cast<std::uint16_t>(
            *startup_file_size_ + std::uint16_t{0x000f});
        const auto paragraphs = static_cast<std::uint16_t>(rounded_file_size >> 4U);
        constexpr std::array<std::uint8_t, 5> next_request{0xb4,0x48,0xcd,0x21,0xa3};
        if (startup_file_helper_invocation_ == 0) {
            if (!std::equal(next_request.begin(), next_request.end(),
                    game_executable_.begin() + static_cast<std::ptrdiff_t>(0xd230 - load_bias))) {
                throw std::runtime_error("Millennium allocator fourth request source bytes changed");
            }
            startup_allocator_fourth_request_ = MillenniumDosNativeStartupFileRequest{
                0xd232, 0xd234, 0x21, 0x48,
                static_cast<std::uint16_t>(0x4800U | (paragraphs & 0x00ffU)),
                paragraphs, 0, 0};
            state_ = MillenniumDosNativeProcessState::startup_allocator_fourth_request_boundary;
        } else {
            if (!std::equal(next_request.begin(), next_request.end(),
                    game_executable_.begin() + static_cast<std::ptrdiff_t>(0xd24f - load_bias))) {
                throw std::runtime_error("Millennium allocator sixth request source bytes changed");
            }
            startup_allocator_fifth_request_ = MillenniumDosNativeStartupFileRequest{
                0xd251, 0xd253, 0x21, 0x48,
                static_cast<std::uint16_t>(0x4800U | (paragraphs & 0x00ffU)),
                paragraphs, 0, 0};
            state_ = MillenniumDosNativeProcessState::startup_allocator_sixth_request_boundary;
        }
        ++startup_file_helper_invocation_;
        return;
    }
    if (state_ == MillenniumDosNativeProcessState::startup_allocator_fourth_request_boundary) {
        if (observation.interrupt_instruction_address != allocator_fourth_request_site
            || observation.interrupt_number != 0x21 || observation.service != 0x48
            || observation.return_instruction_pointer != 0xd234) {
            throw std::runtime_error("Millennium DOS return is detached from allocator fourth INT $21/AH=$48 request");
        }
        startup_allocator_fourth_dos_return_ = observation;
        startup_allocator_fourth_segment_ = observation.ax;
        runtime_bytes_[0x0116] = static_cast<std::uint8_t>(observation.ax);
        runtime_bytes_[0x0117] = static_cast<std::uint8_t>(observation.ax >> 8U);
        if ((observation.flags & 1U) != 0) {
            state_ = MillenniumDosNativeProcessState::startup_allocator_failure_branch_boundary;
            return;
        }
        constexpr std::array<std::uint8_t, 7> fifth_request{
            0xbb,0xc0,0x00,0xb4,0x48,0xcd,0x21};
        if (!std::equal(fifth_request.begin(), fifth_request.end(),
                game_executable_.begin() + static_cast<std::ptrdiff_t>(0xd239 - load_bias))) {
            throw std::runtime_error("Millennium allocator fifth request source span changed");
        }
        startup_allocator_fifth_request_ = MillenniumDosNativeStartupFileRequest{
            0xd23e, 0xd240, 0x21, 0x48,
            static_cast<std::uint16_t>(0x4800U | (observation.ax & 0x00ffU)),
            0x00c0, 0, 0};
        state_ = MillenniumDosNativeProcessState::startup_allocator_fifth_request_boundary;
        return;
    }
    if (state_ == MillenniumDosNativeProcessState::startup_allocator_fifth_request_boundary) {
        if (observation.interrupt_instruction_address != 0xd23e
            || observation.interrupt_number != 0x21 || observation.service != 0x48
            || observation.return_instruction_pointer != 0xd240) {
            throw std::runtime_error("Millennium DOS return is detached from allocator fifth INT $21/AH=$48 request");
        }
        startup_allocator_fifth_dos_return_ = observation;
        runtime_bytes_[0x011c] = static_cast<std::uint8_t>(observation.ax);
        runtime_bytes_[0x011d] = static_cast<std::uint8_t>(observation.ax >> 8U);
        if ((observation.flags & 1U) != 0) {
            state_ = MillenniumDosNativeProcessState::startup_allocator_failure_branch_boundary;
            return;
        }
        startup_file_handle_.reset();
        startup_file_size_.reset();
        startup_file_open_request_ = MillenniumDosNativeStartupFileOpenRequest{
            0xd1d4, 0xd1d6, 0x21, 0x3d, 0x3d00, true, 0x11c2};
        state_ = MillenniumDosNativeProcessState::startup_allocator_file_open_request_boundary;
        return;
    }
    if (state_ == MillenniumDosNativeProcessState::startup_allocator_sixth_request_boundary) {
        if (observation.interrupt_instruction_address != 0xd251
            || observation.interrupt_number != 0x21 || observation.service != 0x48
            || observation.return_instruction_pointer != 0xd253) {
            throw std::runtime_error("Millennium DOS return is detached from allocator sixth INT $21/AH=$48 request");
        }
        startup_allocator_sixth_dos_return_ = observation;
        runtime_bytes_[0x0118] = static_cast<std::uint8_t>(observation.ax);
        runtime_bytes_[0x0119] = static_cast<std::uint8_t>(observation.ax >> 8U);
        if ((observation.flags & 1U) != 0) {
            state_ = MillenniumDosNativeProcessState::startup_allocator_failure_branch_boundary;
            return;
        }
        constexpr std::array<std::uint8_t, 7> seventh_request{
            0xbb,0x00,0x10,0xb4,0x48,0xcd,0x21};
        if (!std::equal(seventh_request.begin(), seventh_request.end(),
                game_executable_.begin() + static_cast<std::ptrdiff_t>(0xd258 - load_bias))) {
            throw std::runtime_error("Millennium allocator seventh request source span changed");
        }
        startup_allocator_seventh_request_ = MillenniumDosNativeStartupFileRequest{
            0xd25d, 0xd25f, 0x21, 0x48,
            static_cast<std::uint16_t>(0x4800U | (observation.ax & 0x00ffU)),
            0x1000, 0, 0};
        state_ = MillenniumDosNativeProcessState::startup_allocator_seventh_request_boundary;
        return;
    }
    if (state_ == MillenniumDosNativeProcessState::startup_allocator_seventh_request_boundary) {
        if (observation.interrupt_instruction_address != 0xd25d
            || observation.interrupt_number != 0x21 || observation.service != 0x48
            || observation.return_instruction_pointer != 0xd25f) {
            throw std::runtime_error("Millennium DOS return is detached from allocator seventh INT $21/AH=$48 request");
        }
        startup_allocator_seventh_dos_return_ = observation;
        runtime_bytes_[0x010c] = static_cast<std::uint8_t>(observation.ax);
        runtime_bytes_[0x010d] = static_cast<std::uint8_t>(observation.ax >> 8U);
        if ((observation.flags & 1U) != 0) {
            state_ = MillenniumDosNativeProcessState::startup_allocator_failure_branch_boundary;
            return;
        }
        constexpr std::array<std::uint8_t, 7> eighth_request{
            0xbb,0xa1,0x0f,0xb4,0x48,0xcd,0x21};
        if (!std::equal(eighth_request.begin(), eighth_request.end(),
                game_executable_.begin() + static_cast<std::ptrdiff_t>(0xd268 - load_bias))) {
            throw std::runtime_error("Millennium allocator eighth request source span changed");
        }
        startup_allocator_eighth_request_ = MillenniumDosNativeStartupFileRequest{
            0xd26d, 0xd26f, 0x21, 0x48,
            static_cast<std::uint16_t>(0x4800U | (observation.ax & 0x00ffU)),
            0x0fa1, 0, 0};
        state_ = MillenniumDosNativeProcessState::startup_allocator_eighth_request_boundary;
        return;
    }
    if (state_ == MillenniumDosNativeProcessState::startup_allocator_eighth_request_boundary) {
        if (observation.interrupt_instruction_address != 0xd26d
            || observation.interrupt_number != 0x21 || observation.service != 0x48
            || observation.return_instruction_pointer != 0xd26f) {
            throw std::runtime_error("Millennium DOS return is detached from allocator eighth INT $21/AH=$48 request");
        }
        startup_allocator_eighth_dos_return_ = observation;
        runtime_bytes_[0x0110] = static_cast<std::uint8_t>(observation.ax);
        runtime_bytes_[0x0111] = static_cast<std::uint8_t>(observation.ax >> 8U);
        if ((observation.flags & 1U) != 0) {
            state_ = MillenniumDosNativeProcessState::startup_allocator_failure_branch_boundary;
            return;
        }
        constexpr std::array<std::uint8_t, 7> ninth_request{
            0xbb,0xa1,0x0f,0xb4,0x48,0xcd,0x21};
        if (!std::equal(ninth_request.begin(), ninth_request.end(),
                game_executable_.begin() + static_cast<std::ptrdiff_t>(0xd278 - load_bias))) {
            throw std::runtime_error("Millennium allocator ninth request source span changed");
        }
        startup_allocator_ninth_request_ = MillenniumDosNativeStartupFileRequest{
            0xd27d, 0xd27f, 0x21, 0x48,
            static_cast<std::uint16_t>(0x4800U | (observation.ax & 0x00ffU)),
            0x0fa1, 0, 0};
        state_ = MillenniumDosNativeProcessState::startup_allocator_ninth_request_boundary;
        return;
    }
    if (state_ == MillenniumDosNativeProcessState::startup_allocator_ninth_request_boundary) {
        if (observation.interrupt_instruction_address != 0xd27d
            || observation.interrupt_number != 0x21 || observation.service != 0x48
            || observation.return_instruction_pointer != 0xd27f) {
            throw std::runtime_error("Millennium DOS return is detached from allocator ninth INT $21/AH=$48 request");
        }
        startup_allocator_ninth_dos_return_ = observation;
        if ((observation.flags & 1U) != 0) {
            state_ = MillenniumDosNativeProcessState::startup_allocator_failure_branch_boundary;
            return;
        }
        constexpr std::string_view continuation_sha256 =
            "8a8344c213b4afd3e076188b6883c8ae5568d6e448af3da65415c0debd20f783";
        constexpr std::array<std::uint8_t, 23> continuation{
            0x72,0x2b,0xbb,0x42,0x10,0x2e,0x89,0x47,0x02,0xbb,0x46,0x10,
            0x2e,0x89,0x47,0x02,0xbb,0x00,0x10,0xb4,0x48,0xcd,0x21};
        if (to_hex(sha256(game_executable_.subspan(0xd27f - load_bias,
                    continuation.size()))) != continuation_sha256
            || !std::equal(continuation.begin(), continuation.end(),
                game_executable_.begin() + static_cast<std::ptrdiff_t>(0xd27f - load_bias))) {
            throw std::runtime_error("Millennium allocator ninth-return continuation changed");
        }
        for (const auto address : {std::uint16_t{0x1044}, std::uint16_t{0x1048}}) {
            runtime_bytes_[address] = static_cast<std::uint8_t>(observation.ax);
            runtime_bytes_[static_cast<std::uint16_t>(address + 1U)] =
                static_cast<std::uint8_t>(observation.ax >> 8U);
        }
        startup_allocator_tenth_request_ = MillenniumDosNativeStartupFileRequest{
            0xd294, 0xd296, 0x21, 0x48,
            static_cast<std::uint16_t>(0x4800U | (observation.ax & 0x00ffU)),
            0x1000, 0, 0};
        state_ = MillenniumDosNativeProcessState::startup_allocator_tenth_request_boundary;
        return;
    }
    if (state_ == MillenniumDosNativeProcessState::startup_allocator_tenth_request_boundary) {
        if (observation.interrupt_instruction_address != 0xd294
            || observation.interrupt_number != 0x21 || observation.service != 0x48
            || observation.return_instruction_pointer != 0xd296) {
            throw std::runtime_error("Millennium DOS return is detached from allocator tenth INT $21/AH=$48 request");
        }
        startup_allocator_tenth_dos_return_ = observation;
        if ((observation.flags & 1U) != 0) {
            state_ = MillenniumDosNativeProcessState::startup_allocator_failure_branch_boundary;
            return;
        }
        constexpr std::string_view continuation_sha256 =
            "6c2f0dd3862ed0e00d1cdaea5086562f5b6e3594041ac4080778a0c7565715b9";
        constexpr std::array<std::uint8_t, 16> continuation{
            0x72,0x14,0xbb,0x58,0x10,0x2e,0x89,0x47,0x02,0xbb,0x01,0x00,
            0xb4,0x48,0xcd,0x21};
        if (to_hex(sha256(game_executable_.subspan(0xd296 - load_bias,
                    continuation.size()))) != continuation_sha256
            || !std::equal(continuation.begin(), continuation.end(),
                game_executable_.begin() + static_cast<std::ptrdiff_t>(0xd296 - load_bias))) {
            throw std::runtime_error("Millennium allocator tenth-return continuation changed");
        }
        runtime_bytes_[0x105a] = static_cast<std::uint8_t>(observation.ax);
        runtime_bytes_[0x105b] = static_cast<std::uint8_t>(observation.ax >> 8U);
        startup_allocator_eleventh_request_ = MillenniumDosNativeStartupFileRequest{
            0xd2a4, 0xd2a6, 0x21, 0x48,
            static_cast<std::uint16_t>(0x4800U | (observation.ax & 0x00ffU)),
            1, 0, 0};
        state_ = MillenniumDosNativeProcessState::startup_allocator_eleventh_request_boundary;
        return;
    }
    if (state_ == MillenniumDosNativeProcessState::startup_allocator_eleventh_request_boundary) {
        if (observation.interrupt_instruction_address != 0xd2a4
            || observation.interrupt_number != 0x21 || observation.service != 0x48
            || observation.return_instruction_pointer != 0xd2a6) {
            throw std::runtime_error("Millennium DOS return is detached from allocator eleventh INT $21/AH=$48 request");
        }
        startup_allocator_eleventh_dos_return_ = observation;
        constexpr std::array<std::uint8_t, 6> allocator_return{0xa3,0x2a,0xd1,0x33,0xd2,0xc3};
        constexpr std::array<std::uint8_t, 7> caller_test{0xa3,0x28,0xd1,0x23,0xd2,0x74,0x03};
        if (!std::equal(allocator_return.begin(), allocator_return.end(),
                game_executable_.begin() + static_cast<std::ptrdiff_t>(0xd2a6 - load_bias))
            || to_hex(sha256(game_executable_.subspan(0xd2a6 - load_bias,
                    allocator_return.size())))
                != "ad34ec4b8d669ed0f1e6fd712e9e3c74079f081a1aa8f6da2dd4b03e588b6825"
            || !std::equal(caller_test.begin(), caller_test.end(),
                game_executable_.begin() + static_cast<std::ptrdiff_t>(0xd2eb - load_bias))
            || to_hex(sha256(game_executable_.subspan(0xd2eb - load_bias,
                    caller_test.size())))
                != "0d00e0a5a8b84c84cf2093275c5e956b283898d9156c1f31b451c58db3cfb352") {
            throw std::runtime_error("Millennium allocator return-to-caller source bytes changed");
        }
        startup_allocator_return_ax_ = observation.ax;
        runtime_bytes_[0xd12a] = static_cast<std::uint8_t>(observation.ax);
        runtime_bytes_[0xd12b] = static_cast<std::uint8_t>(observation.ax >> 8U);
        runtime_bytes_[0xd128] = static_cast<std::uint8_t>(observation.ax);
        runtime_bytes_[0xd129] = static_cast<std::uint8_t>(observation.ax >> 8U);
        state_ = MillenniumDosNativeProcessState::startup_allocator_local_call_boundary;
        continue_startup_allocator_zero_path();
        return;
    }
    if (state_ != MillenniumDosNativeProcessState::startup_allocation_interrupt_boundary
        || expected.kind != MillenniumDosNativeBoundaryKind::dos_interrupt
        || expected.address != observation.interrupt_instruction_address
        || expected.interrupt != observation.interrupt_number
        || expected.service != observation.service
        || observation.return_instruction_pointer
            != static_cast<std::uint16_t>(expected.address + 2U)) {
        throw std::runtime_error("Millennium DOS return is detached from allocation INT $21/AH=$4a");
    }
    constexpr std::string_view executable_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    constexpr std::array<std::uint8_t, 7> next_request_bytes{
        0xbb,0x00,0xfa,0xb4,0x48,0xcd,0x21};
    constexpr std::uint16_t next_request_setup_address = 0xd203;
    const auto next_request_offset = static_cast<std::size_t>(next_request_setup_address - load_bias);
    if (game_executable_.size() != 54'391
        || to_hex(sha256(game_executable_)) != executable_sha256
        || !std::equal(next_request_bytes.begin(), next_request_bytes.end(),
            game_executable_.begin() + static_cast<std::ptrdiff_t>(next_request_offset))
        || to_hex(sha256(game_executable_.subspan(next_request_offset, next_request_bytes.size())))
            != "745590d9713233f5c556ef8e355b38030363e524166bbd770f19624e3d2a0847") {
        throw std::runtime_error("Millennium allocator next request source span changed");
    }
    startup_dos_return_ = observation;
    state_ = MillenniumDosNativeProcessState::startup_allocator_second_request_boundary;
}

void MillenniumDosNativeProcess::apply_startup_writes(
    const MillenniumDosEnglishStartupPrefix& prefix) {
    for (const auto& write : prefix.local_writes) {
        if (write.width != 1 && write.width != 2) {
            throw std::runtime_error("Unsupported Millennium DOS native write width");
        }
        runtime_bytes_[write.address] = static_cast<std::uint8_t>(write.value & 0xffU);
        if (write.width == 2) {
            runtime_bytes_[static_cast<std::uint16_t>(write.address + 1U)] =
                static_cast<std::uint8_t>(write.value >> 8U);
        }
    }
}

void MillenniumDosNativeProcess::observe_private_interrupt_return(
    const std::uint16_t address, const std::uint16_t ax) {
    if (address != private_interrupt_site) {
        throw std::runtime_error("Millennium DOS private return is detached from its INT site");
    }
    if (state_ == MillenniumDosNativeProcessState::startup_first_private_interrupt) {
        startup_first_ax_ = ax;
        startup_prefix_ = evaluate_millennium_dos_english_startup_prefix(
            game_executable_, startup_first_ax_);
        apply_startup_writes(*startup_prefix_);
        state_ = MillenniumDosNativeProcessState::startup_selected_private_interrupt;
        return;
    }
    if (state_ == MillenniumDosNativeProcessState::startup_selected_private_interrupt) {
        startup_prefix_ = evaluate_millennium_dos_english_startup_prefix(
            game_executable_, startup_first_ax_, ax);
        apply_startup_writes(*startup_prefix_);
        state_ = startup_prefix_->outcome == MillenniumDosEnglishStartupPrefixOutcome::equal_return
            ? MillenniumDosNativeProcessState::startup_local_return_boundary
            : MillenniumDosNativeProcessState::startup_palette_interrupt_boundary;
        return;
    }
    if (state_ == MillenniumDosNativeProcessState::gx_private_interrupt) {
        gx_session_->observe_private_return(ax);
        state_ = MillenniumDosNativeProcessState::gx_mode_byte;
        return;
    }
    throw std::runtime_error("Out-of-order Millennium DOS private return observation");
}

void MillenniumDosNativeProcess::observe_bios_interrupt_return(
    const MillenniumDosNativeBiosInterruptReturnObservation& observation) {
    const auto expected = boundary();
    if (state_ != MillenniumDosNativeProcessState::startup_palette_interrupt_boundary
        || expected.kind != MillenniumDosNativeBoundaryKind::bios_interrupt
        || expected.address != observation.interrupt_instruction_address
        || expected.interrupt != observation.interrupt_number
        || observation.return_instruction_pointer
            != static_cast<std::uint16_t>(expected.address + 2U)
        || !startup_prefix_->first_palette_request) {
        throw std::runtime_error(
            "Millennium DOS BIOS return is detached from the startup palette INT $10 boundary");
    }
    if (startup_ega_palette_session_) {
        auto palette = *startup_ega_palette_session_;
        const auto returned = palette.observe_write_return({observation.sequence,
            observation.interrupt_instruction_address, observation.interrupt_number,
            observation.ax, observation.bx});
        if (!returned.accepted) {
            throw std::runtime_error(returned.error);
        }
        startup_bios_return_ = observation;
        startup_ega_palette_session_ = std::move(palette);
        state_ = startup_ega_palette_session_->complete()
            ? MillenniumDosNativeProcessState::startup_palette_loop_return_boundary
            : MillenniumDosNativeProcessState::startup_palette_interrupt_boundary;
        return;
    }
    startup_bios_return_ = observation;
    state_ = MillenniumDosNativeProcessState::startup_palette_interrupt_return_observed;
}

void MillenniumDosNativeProcess::begin_startup_ega_palette(
    const MillenniumDosGameFunctionFourDispatchResult& function_four) {
    if (state_ != MillenniumDosNativeProcessState::startup_palette_interrupt_boundary
        || !startup_prefix_ || !startup_prefix_->first_palette_request
        || startup_ega_palette_session_) {
        throw std::runtime_error("EGA palette session is detached from the startup BIOS boundary");
    }
    startup_ega_palette_session_.emplace(game_executable_, function_four);
}

void MillenniumDosNativeProcess::continue_startup_ega_caller(
    const std::uint8_t selector_value, const std::uint16_t first_return_word,
    const std::uint16_t outer_return_word) {
    constexpr std::uint16_t load_bias = 0x100;
    constexpr std::array<std::uint8_t, 14> caller_bytes{
        0xa0,0x05,0xda,0x3c,0x02,0x75,0x06,0xb8,0x00,0xb8,0xa3,0x07,0x01,0xc3};
    constexpr std::string_view executable_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    constexpr std::string_view caller_sha256 =
        "0ab3b7a8bbd26341b2b8e7a475159a649e3277cb9090ab45a3a1cb59b59b2ad6";
    constexpr std::size_t caller_offset = 0xd1c3U - load_bias;
    const auto owned_selector = runtime_byte(0xda05);
    if (state_ != MillenniumDosNativeProcessState::startup_palette_loop_return_boundary
        || !startup_ega_palette_session_ || !startup_ega_palette_session_->complete()
        || game_executable_.size() != 54'391U
        || to_hex(sha256(game_executable_)) != executable_sha256
        || to_hex(sha256(game_executable_.subspan(caller_offset, caller_bytes.size())))
            != caller_sha256
        || !std::equal(caller_bytes.begin(), caller_bytes.end(),
            game_executable_.begin() + static_cast<std::ptrdiff_t>(caller_offset))
        || first_return_word != 0xd1c3 || outer_return_word != 0xd2e5
        || !owned_selector || selector_value != *owned_selector) {
        throw std::runtime_error("EGA caller continuation lacks its exact source or owned inputs");
    }
    if (selector_value == 2) {
        runtime_bytes_[0x0107] = 0x00;
        runtime_bytes_[0x0108] = 0xb8;
    }
    // The caller's RET consumes $d2e5. We stop at that return target because
    // the common allocator path still needs its independently owned stack.
    state_ = MillenniumDosNativeProcessState::startup_ega_caller_return_boundary;
}

void MillenniumDosNativeProcess::continue_startup_ega_allocator_request(
    const std::uint16_t outer_return_word) {
    constexpr std::uint16_t load_bias = 0x100;
    constexpr std::array<std::uint8_t, 19> continuation_bytes{
        0x52,0x0e,0x1f,0xe8,0x0f,0xff,0xa3,0x28,0xd1,0x23,0xd2,
        0x74,0x03,0xe9,0x56,0x01,0xe8,0x69,0x3e};
    constexpr std::uint16_t continuation_address = 0xd2e5;
    constexpr std::size_t continuation_offset = continuation_address - load_bias;
    constexpr std::string_view executable_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    constexpr std::string_view continuation_sha256 =
        "9623d493ddfa9339d3137799c133a99df86425db2fb0d674a81b2e09555692b6";
    if (state_ != MillenniumDosNativeProcessState::startup_ega_caller_return_boundary
        || outer_return_word != continuation_address
        || game_executable_.size() != 54'391U
        || to_hex(sha256(game_executable_)) != executable_sha256
        || to_hex(sha256(game_executable_.subspan(
            continuation_offset, continuation_bytes.size()))) != continuation_sha256
        || !std::equal(continuation_bytes.begin(), continuation_bytes.end(),
            game_executable_.begin() + static_cast<std::ptrdiff_t>(continuation_offset))) {
        throw std::runtime_error("EGA allocator continuation lacks its exact caller return or source");
    }
    const auto allocation = parse_millennium_dos_startup_allocation_boundary(game_executable_);
    if (allocation.continuation_entry_address != continuation_address
        || allocation.allocator_call_address != 0xd2e8
        || allocation.allocator_entry_address != 0xd1fa
        || allocation.allocator_first_external_interrupt_site != 0xd201
        || allocation.allocator_first_external_interrupt != 0x21
        || allocation.allocator_first_external_service != 0x4a) {
        throw std::runtime_error("EGA allocator request profile changed");
    }
    state_ = MillenniumDosNativeProcessState::startup_allocation_interrupt_boundary;
}

void MillenniumDosNativeProcess::observe_runtime_byte(
    const std::uint16_t instruction_address, const std::uint16_t runtime_address,
    const std::uint8_t value, const std::uint64_t sequence) {
    const auto expected = boundary();
    if (expected.kind != MillenniumDosNativeBoundaryKind::runtime_byte
        || expected.address != instruction_address
        || expected.runtime_byte_address != runtime_address) {
        throw std::runtime_error("Millennium DOS runtime-byte observation is detached from its read");
    }
    if (state_ == MillenniumDosNativeProcessState::gx_mode_byte) {
        gx_session_->observe_mode_byte(value);
        state_ = MillenniumDosNativeProcessState::gx_adapter_return;
        return;
    }
    if (state_ == MillenniumDosNativeProcessState::gx_post_overlay_mode_byte) {
        gx_session_->observe_post_overlay_mode_byte(value);
        state_ = MillenniumDosNativeProcessState::gx_post_overlay_private_interrupt;
        return;
    }
    if (state_ == MillenniumDosNativeProcessState::startup_zero_path_continuation_byte_boundary) {
        if (instruction_address != 0xd2fd || runtime_address != 0x0082 || sequence == 0) {
            throw std::runtime_error("Millennium startup selector byte is detached from CS:$0082");
        }
        const auto continuation = parse_millennium_dos_startup_zero_continuation_boundary(
            game_executable_);
        if (continuation.continuation_entry_address != 0xd2f8
            || continuation.decoded_byte_storage_address != 0x0122
            || continuation.source_byte_subtract_immediate != 0x30) {
            throw std::runtime_error("Millennium startup selector continuation changed");
        }
        startup_zero_path_continuation_byte_ = MillenniumDosNativeRuntimeByteObservation{
            sequence, instruction_address, runtime_address, value};
        runtime_bytes_[0x0122] = static_cast<std::uint8_t>(value - 0x30U);
        state_ = MillenniumDosNativeProcessState::startup_zero_path_selector_call_boundary;
        return;
    }
    throw std::runtime_error("Out-of-order Millennium DOS runtime-byte observation");
}

void MillenniumDosNativeProcess::observe_startup_sibm_function_zero_prefix(
    const MillenniumDosSibmFunctionZeroCheckpoint& result) {
    if (state_ != MillenniumDosNativeProcessState::startup_zero_path_selector_call_boundary
        || !startup_zero_path_continuation_byte_
        || result.state != MillenniumDosSibmFunctionZeroState::awaiting_timer_vector_get
        || result.next_instruction_address != 0x0233 || result.dos_ax != 0x3508
        || !result.function_input) {
        throw std::runtime_error(
            "SIBM function-zero result is detached from the observed startup selector call");
    }
    startup_sibm_function_zero_ = result;
    state_ = MillenniumDosNativeProcessState::startup_sibm_timer_vector_get_boundary;
}

void MillenniumDosNativeProcess::observe_startup_sibm_function_zero_vector_get(
    const MillenniumDosSibmFunctionZeroCheckpoint& result) {
    if (state_ != MillenniumDosNativeProcessState::startup_sibm_timer_vector_get_boundary
        || !startup_sibm_function_zero_
        || startup_sibm_function_zero_->state
            != MillenniumDosSibmFunctionZeroState::awaiting_timer_vector_get
        || result.state != MillenniumDosSibmFunctionZeroState::awaiting_timer_vector_set
        || result.dos_interrupt_instruction != 0x0246 || result.dos_ax != 0x2508
        || result.next_instruction_address != 0x0246
        || !result.timer_vector_offset || !result.timer_vector_segment
        || !result.timer_vector_return_ax || result.last_observation_sequence == 0) {
        throw std::runtime_error(
            "SIBM vector-query result is detached from its active AH=$35 boundary");
    }
    startup_sibm_function_zero_ = result;
    state_ = MillenniumDosNativeProcessState::startup_sibm_timer_vector_set_boundary;
}

void MillenniumDosNativeProcess::observe_startup_sibm_function_zero_vector_set_return(
    const MillenniumDosSibmFunctionZeroCheckpoint& result) {
    if (state_ != MillenniumDosNativeProcessState::startup_sibm_timer_vector_set_boundary
        || !startup_sibm_function_zero_
        || startup_sibm_function_zero_->state
            != MillenniumDosSibmFunctionZeroState::awaiting_timer_vector_set
        || result.state
            != MillenniumDosSibmFunctionZeroState::timer_vector_set_return_observed
        || result.dos_interrupt_instruction != 0x0246 || result.dos_ax != 0x2508
        || result.next_instruction_address != 0x0248
        || result.last_observation_sequence
            <= startup_sibm_function_zero_->last_observation_sequence
        || !result.timer_vector_offset || !result.timer_vector_segment
        || !result.timer_vector_return_ax || !result.timer_vector_set_return_ax
        || !result.timer_vector_set_return_bx || !result.timer_vector_set_return_cx
        || !result.timer_vector_set_return_dx || !result.timer_vector_set_return_flags
        || !result.timer_vector_set_return_es) {
        throw std::runtime_error(
            "SIBM vector-set return is detached from its active AH=$25 boundary");
    }
    startup_sibm_function_zero_ = result;
    state_ = MillenniumDosNativeProcessState::startup_sibm_timer_vector_set_return_observed;
}

void MillenniumDosNativeProcess::observe_native_call_return(
    const std::uint16_t call_address, const std::uint16_t return_address) {
    const auto expected = boundary();
    if (expected.kind != MillenniumDosNativeBoundaryKind::native_call_return
        || expected.address != call_address
        || return_address != static_cast<std::uint16_t>(call_address + 3U)) {
        throw std::runtime_error("Millennium DOS call return is detached from its call site");
    }
    if (state_ == MillenniumDosNativeProcessState::gx_adapter_return) {
        gx_session_->observe_adapter_return();
        state_ = MillenniumDosNativeProcessState::gx_post_overlay_call_return;
        return;
    }
    if (state_ == MillenniumDosNativeProcessState::gx_post_overlay_call_return) {
        gx_session_->observe_post_overlay_call_return();
        state_ = gx_session_->observed_post_overlay_call_return_count() == 6
            ? MillenniumDosNativeProcessState::gx_post_overlay_mode_byte
            : MillenniumDosNativeProcessState::gx_post_overlay_call_return;
        return;
    }
    throw std::runtime_error("Out-of-order Millennium DOS call-return observation");
}

std::optional<std::uint8_t> MillenniumDosNativeProcess::runtime_byte(
    const std::uint16_t address) const {
    const auto found = runtime_bytes_.find(address);
    return found == runtime_bytes_.end() ? std::nullopt
                                        : std::optional<std::uint8_t>(found->second);
}

std::optional<std::uint8_t> MillenniumDosNativeProcess::gx_overlay_byte(
    const std::uint16_t offset) const {
    return gx_session_ ? gx_session_->overlay_byte(offset) : std::nullopt;
}

} // namespace eon
