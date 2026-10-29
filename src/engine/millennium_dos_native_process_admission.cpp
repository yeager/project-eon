#include "engine/millennium_dos_native_process_admission.hpp"

#include "data/sha256.hpp"
#include "data/millennium_dos_video_driver.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace eon {
namespace {
bool has_identity(const std::span<const std::uint8_t> bytes,
    const std::string_view expected_sha256) {
    return to_hex(sha256(bytes)) == expected_sha256;
}
}

MillenniumDosNativeProcessAdmission MillenniumDosNativeProcessAdmission::startup(
    const std::string_view release_sha256,
    const std::span<const std::uint8_t> game_executable) {
    MillenniumDosNativeProcessAdmission result;
    if (release_sha256 != english_release_sha256
        || !has_identity(game_executable, game_sha256)) {
        result.error_ = "Millennium DOS native startup source identity rejected";
        return result;
    }
    try {
        result.game_executable_.assign(game_executable.begin(), game_executable.end());
        result.process_ = std::make_unique<MillenniumDosNativeProcess>(
            MillenniumDosNativeProcess::startup(result.game_executable_));
    } catch (const std::exception& exception) {
        result.reset();
        result.error_ = std::string("Millennium DOS native startup rejected: ") + exception.what();
    }
    return result;
}

MillenniumDosNativeProcessAdmission MillenniumDosNativeProcessAdmission::post_gx_loader(
    const std::string_view release_sha256,
    const std::span<const std::uint8_t> game_executable,
    const std::span<const std::uint8_t> gx_overlay_executable) {
    MillenniumDosNativeProcessAdmission result;
    result.recovery_entry_ = MillenniumDosNativeRecoveryEntry::post_gx_loader;
    if (release_sha256 != english_release_sha256
        || !has_identity(game_executable, game_sha256)
        || !has_identity(gx_overlay_executable, gx_sha256)) {
        result.error_ = "Millennium DOS post-GX native source identity rejected";
        return result;
    }
    try {
        result.game_executable_.assign(game_executable.begin(), game_executable.end());
        result.gx_overlay_executable_.assign(
            gx_overlay_executable.begin(), gx_overlay_executable.end());
        result.process_ = std::make_unique<MillenniumDosNativeProcess>(
            MillenniumDosNativeProcess::post_gx_loader(
                result.game_executable_, result.gx_overlay_executable_));
    } catch (const std::exception& exception) {
        result.reset();
        result.error_ = std::string("Millennium DOS post-GX native source rejected: ")
            + exception.what();
    }
    return result;
}

std::optional<MillenniumDosNativeProcessCheckpoint>
MillenniumDosNativeProcessAdmission::checkpoint() const {
    if (!admitted()) return std::nullopt;
    return MillenniumDosNativeProcessCheckpoint{
        .recovery_entry = recovery_entry_,
        .state = process_->state(),
        .boundary = process_->boundary(),
        .release_sha256 = std::string(english_release_sha256),
        .game_executable_sha256 = std::string(game_sha256),
        .gx_overlay_sha256 = recovery_entry_ == MillenniumDosNativeRecoveryEntry::post_gx_loader
            ? std::optional<std::string>{std::string(gx_sha256)} : std::nullopt,
        .static_recovery_entry = true,
        .observed_child_process_entry = observed_child_process_entry_,
        .child_entry_sequence = child_entry_sequence_,
        .child_code_segment = child_code_segment_,
        .last_observation_sequence = last_observation_sequence_,
        .startup_bios_return = process_->startup_bios_return(),
        .startup_ega_palette_write_count = process_->startup_ega_palette_session()
            ? process_->startup_ega_palette_session()->completed_write_count() : 0,
        .startup_dos_return = process_->startup_dos_return(),
        .startup_allocator_second_dos_return = process_->startup_allocator_second_dos_return(),
        .startup_allocator_free_dos_return = process_->startup_allocator_free_dos_return(),
        .startup_allocator_third_dos_return = process_->startup_allocator_third_dos_return(),
        .startup_file_open_request = process_->startup_file_open_request(),
        .startup_zero_path_file_open_request =
            process_->startup_zero_path_file_open_request(),
        .startup_zero_path_file_open_dos_return =
            process_->startup_zero_path_file_open_dos_return(),
        .startup_zero_path_file_read_request =
            process_->startup_zero_path_file_read_request(),
        .startup_zero_path_file_read_dos_return =
            process_->startup_zero_path_file_read_dos_return(),
        .startup_zero_path_file_close_request =
            process_->startup_zero_path_file_close_request(),
        .startup_zero_path_file_close_dos_return =
            process_->startup_zero_path_file_close_dos_return(),
        .startup_zero_path_vector_get_request =
            process_->startup_zero_path_vector_get_request(),
        .startup_zero_path_vector_get_dos_return =
            process_->startup_zero_path_vector_get_dos_return(),
        .startup_zero_path_vector_set_request =
            process_->startup_zero_path_vector_set_request(),
        .startup_zero_path_vector_set_dos_return =
            process_->startup_zero_path_vector_set_dos_return(),
        .startup_zero_path_continuation_byte =
            process_->startup_zero_path_continuation_byte(),
        .startup_zero_path_selector_value = process_->startup_zero_path_continuation_byte()
            ? std::optional<std::uint8_t>{static_cast<std::uint8_t>(
                process_->startup_zero_path_continuation_byte()->value - 0x30U)}
            : std::nullopt,
        .startup_sibm_function_zero = process_->startup_sibm_function_zero(),
        .startup_file_seek_request = process_->startup_file_seek_request(),
        .startup_file_close_request = process_->startup_file_close_request(),
        .startup_allocator_fourth_request = process_->startup_allocator_fourth_request(),
        .startup_allocator_fifth_request = process_->startup_allocator_fifth_request(),
        .startup_allocator_seventh_request = process_->startup_allocator_seventh_request(),
        .startup_allocator_eighth_request = process_->startup_allocator_eighth_request(),
        .startup_allocator_ninth_request = process_->startup_allocator_ninth_request(),
        .startup_allocator_tenth_request = process_->startup_allocator_tenth_request(),
        .startup_allocator_eleventh_request = process_->startup_allocator_eleventh_request(),
        .startup_file_handle = process_->startup_file_handle(),
        .startup_file_size = process_->startup_file_size(),
        .startup_allocator_fourth_dos_return = process_->startup_allocator_fourth_dos_return(),
        .startup_allocator_fourth_segment = process_->startup_allocator_fourth_segment(),
        .startup_allocator_fifth_dos_return = process_->startup_allocator_fifth_dos_return(),
        .startup_allocator_sixth_dos_return = process_->startup_allocator_sixth_dos_return(),
        .startup_allocator_seventh_dos_return = process_->startup_allocator_seventh_dos_return(),
        .startup_allocator_eighth_dos_return = process_->startup_allocator_eighth_dos_return(),
        .startup_allocator_ninth_dos_return = process_->startup_allocator_ninth_dos_return(),
        .startup_allocator_tenth_dos_return = process_->startup_allocator_tenth_dos_return(),
        .startup_allocator_eleventh_dos_return = process_->startup_allocator_eleventh_dos_return(),
        .startup_allocator_return_ax = process_->startup_allocator_return_ax(),
        .startup_allocator_stored_bx = process_->startup_allocator_stored_bx(),
        .startup_allocator_es_segment = process_->startup_allocator_es_segment(),
    };
}

MillenniumDosPostOverlayLoopSession
MillenniumDosNativeProcessAdmission::make_post_overlay_loop_session(
    const std::uint8_t selected_mode_byte) const {
    require_admitted();
    return MillenniumDosPostOverlayLoopSession(game_executable_, selected_mode_byte);
}

MillenniumDosTenthFunctionSession
MillenniumDosNativeProcessAdmission::make_tenth_function_session() const {
    require_admitted();
    return MillenniumDosTenthFunctionSession(game_executable_);
}

MillenniumDosSeventhFunctionSession
MillenniumDosNativeProcessAdmission::make_seventh_function_session() const {
    require_admitted();
    return MillenniumDosSeventhFunctionSession(game_executable_);
}

MillenniumDosSixthFunctionSession
MillenniumDosNativeProcessAdmission::make_sixth_function_session() const {
    require_admitted();
    return MillenniumDosSixthFunctionSession(game_executable_);
}

MillenniumDosFunctionDispatchAdmission
MillenniumDosNativeProcessAdmission::admit_function_dispatch(
    const MillenniumDosPostOverlayLoopSession& loop,
    const MillenniumDosFunctionDispatchObservation observation) const {
    require_admitted();
    return admit_millennium_dos_function_dispatch(game_executable_, loop, observation);
}

MillenniumDosEighthFunctionSession
MillenniumDosNativeProcessAdmission::make_eighth_function_session() const {
    require_admitted();
    return MillenniumDosEighthFunctionSession(game_executable_);
}
MillenniumDosNinthFunctionSession MillenniumDosNativeProcessAdmission::make_ninth_function_session() const {
    require_admitted(); return MillenniumDosNinthFunctionSession(game_executable_);
}
MillenniumDosNinthFunctionHandoffSession MillenniumDosNativeProcessAdmission::make_ninth_function_handoff_session() const {
    require_admitted(); return MillenniumDosNinthFunctionHandoffSession(game_executable_);
}
MillenniumDosFourthFunctionSession MillenniumDosNativeProcessAdmission::make_fourth_function_session() const { require_admitted(); return MillenniumDosFourthFunctionSession(game_executable_); }
MillenniumDosFifthFunctionSession MillenniumDosNativeProcessAdmission::make_fifth_function_session() const { require_admitted(); return MillenniumDosFifthFunctionSession(game_executable_); }
MillenniumDosThirdFunctionSession MillenniumDosNativeProcessAdmission::make_third_function_session() const { require_admitted(); return MillenniumDosThirdFunctionSession(game_executable_); }
MillenniumDosFirstFunctionSession MillenniumDosNativeProcessAdmission::make_first_function_session() const { require_admitted(); return MillenniumDosFirstFunctionSession(game_executable_); }
MillenniumDosSecondFunctionSession MillenniumDosNativeProcessAdmission::make_second_function_session() const { require_admitted(); return MillenniumDosSecondFunctionSession(game_executable_); }
MillenniumDosSecondFunctionCallbackSession MillenniumDosNativeProcessAdmission::make_second_function_callback_session() const { require_admitted(); return MillenniumDosSecondFunctionCallbackSession(game_executable_); }
MillenniumDosBdfServiceSession MillenniumDosNativeProcessAdmission::make_bdf_service_session() const { require_admitted(); return MillenniumDosBdfServiceSession(game_executable_); }
MillenniumDosBdfModeTwoSession MillenniumDosNativeProcessAdmission::make_bdf_mode_two_session(const std::uint16_t entry_di) const { require_admitted(); return MillenniumDosBdfModeTwoSession(game_executable_,entry_di); }
MillenniumDosBdfOtherModeSession MillenniumDosNativeProcessAdmission::make_bdf_other_mode_session(const std::uint8_t entry_dl,const std::uint16_t entry_di) const { require_admitted(); return MillenniumDosBdfOtherModeSession(game_executable_,entry_dl,entry_di); }
MillenniumDosSharedHelperSession MillenniumDosNativeProcessAdmission::make_shared_helper_session(const std::uint16_t caller_ax) const { require_admitted(); return MillenniumDosSharedHelperSession(game_executable_,caller_ax); }
MillenniumDosGameSession MillenniumDosNativeProcessAdmission::make_game_session() const { require_admitted(); return MillenniumDosGameSession(parse_millennium_dos_game_flow(game_executable_),game_executable_); }
MillenniumDosGxOverlayAdapterSession MillenniumDosNativeProcessAdmission::make_gx_overlay_adapter_session(
    const std::uint16_t caller_ax, const std::uint16_t caller_return,
    const std::uint16_t code_segment) const {
    require_admitted();
    return MillenniumDosGxOverlayAdapterSession(
        game_executable_, caller_ax, caller_return, code_segment);
}

void MillenniumDosNativeProcessAdmission::require_admitted() const {
    if (!admitted()) {
        throw std::runtime_error("Millennium DOS native process is not admitted");
    }
}

void MillenniumDosNativeProcessAdmission::observe_private_interrupt_return(
    const std::uint16_t address, const std::uint16_t ax) {
    require_admitted();
    process_->observe_private_interrupt_return(address, ax);
}

void MillenniumDosNativeProcessAdmission::begin_startup_ega_palette(
    const MillenniumDosGameFunctionFourDispatchResult& function_four) {
    require_admitted();
    if (recovery_entry_ != MillenniumDosNativeRecoveryEntry::startup
        || !observed_child_process_entry_ || !process_) {
        throw std::runtime_error("EGA palette session is detached from the observed DOS child");
    }
    process_->begin_startup_ega_palette(function_four);
}

void MillenniumDosNativeProcessAdmission::observe_startup_allocation_request(
    const std::uint16_t interrupt_address) {
    require_admitted();
    if (recovery_entry_ != MillenniumDosNativeRecoveryEntry::startup
        || !observed_child_process_entry_) {
        throw std::runtime_error("Millennium DOS allocation request is detached from the observed child");
    }
    process_->observe_startup_allocation_request(interrupt_address);
}

void MillenniumDosNativeProcessAdmission::observe_bios_interrupt_return(
    const MillenniumDosNativeBiosInterruptReturnObservation& observation) {
    require_admitted();
    const auto boundary = process_->boundary();
    if (recovery_entry_ != MillenniumDosNativeRecoveryEntry::startup
        || !observed_child_process_entry_
        || observation.sequence <= last_observation_sequence_
        || observation.sequence <= child_entry_sequence_
        || boundary.kind != MillenniumDosNativeBoundaryKind::bios_interrupt
        || observation.interrupt_instruction_address != boundary.address
        || boundary.interrupt != observation.interrupt_number
        || observation.return_code_segment != child_code_segment_
        || observation.return_instruction_pointer
            != static_cast<std::uint16_t>(boundary.address + 2U)) {
        throw std::runtime_error(
            "Millennium DOS BIOS return is detached from the observed startup continuation");
    }
    process_->observe_bios_interrupt_return(observation);
    last_observation_sequence_ = observation.sequence;
}

void MillenniumDosNativeProcessAdmission::continue_startup_ega_caller(
    const std::uint8_t selector_value, const std::uint16_t first_return_word,
    const std::uint16_t outer_return_word) {
    require_admitted();
    if (recovery_entry_ != MillenniumDosNativeRecoveryEntry::startup
        || !observed_child_process_entry_ || !process_->startup_ega_palette_session()
        || !process_->startup_ega_palette_session()->complete()) {
        throw std::runtime_error("EGA caller continuation is detached from completed startup palette");
    }
    process_->continue_startup_ega_caller(
        selector_value, first_return_word, outer_return_word);
}

void MillenniumDosNativeProcessAdmission::continue_startup_ega_allocator_request(
    const std::uint16_t outer_return_word) {
    require_admitted();
    if (recovery_entry_ != MillenniumDosNativeRecoveryEntry::startup
        || !observed_child_process_entry_
        || process_->state()
            != MillenniumDosNativeProcessState::startup_ega_caller_return_boundary) {
        throw std::runtime_error("EGA allocator request is detached from the returned caller");
    }
    process_->continue_startup_ega_allocator_request(outer_return_word);
}

void MillenniumDosNativeProcessAdmission::observe_dos_interrupt_return(
    const MillenniumDosNativeDosInterruptReturnObservation& observation) {
    require_admitted();
    const auto boundary = process_->boundary();
    if (recovery_entry_ != MillenniumDosNativeRecoveryEntry::startup
        || !observed_child_process_entry_
        || observation.sequence <= last_observation_sequence_
        || observation.sequence <= child_entry_sequence_
        || boundary.kind != MillenniumDosNativeBoundaryKind::dos_interrupt
        || boundary.address != observation.interrupt_instruction_address
        || boundary.interrupt != observation.interrupt_number
        || boundary.service != observation.service
        || observation.return_code_segment != child_code_segment_
        || observation.return_instruction_pointer
            != static_cast<std::uint16_t>(boundary.address + 2U)) {
        throw std::runtime_error(
            "Millennium DOS return is detached from the observed startup INT $21 boundary");
    }
    process_->observe_dos_interrupt_return(observation);
    last_observation_sequence_ = observation.sequence;
}

void MillenniumDosNativeProcessAdmission::observe_runtime_byte(
    const std::uint64_t sequence, const std::uint16_t instruction_address,
    const std::uint16_t runtime_address, const std::uint8_t value) {
    require_admitted();
    const auto boundary = process_->boundary();
    if (recovery_entry_ != MillenniumDosNativeRecoveryEntry::startup
        || !observed_child_process_entry_ || sequence <= last_observation_sequence_
        || sequence <= child_entry_sequence_
        || boundary.kind != MillenniumDosNativeBoundaryKind::runtime_byte
        || boundary.address != instruction_address
        || boundary.runtime_byte_address != runtime_address) {
        throw std::runtime_error(
            "Millennium DOS runtime byte is detached from the observed startup continuation");
    }
    process_->observe_runtime_byte(instruction_address, runtime_address, value, sequence);
    last_observation_sequence_ = sequence;
}

void MillenniumDosNativeProcessAdmission::observe_startup_sibm_function_zero_prefix(
    const MillenniumDosSibmFunctionZeroCheckpoint& result) {
    require_admitted();
    if (recovery_entry_ != MillenniumDosNativeRecoveryEntry::startup
        || !observed_child_process_entry_
        || !process_->startup_zero_path_continuation_byte()) {
        throw std::runtime_error(
            "SIBM function zero requires the observed native DOS startup continuation");
    }
    process_->observe_startup_sibm_function_zero_prefix(result);
}

void MillenniumDosNativeProcessAdmission::observe_startup_sibm_timer_vector_get_return(
    const MillenniumDosNativeDosInterruptReturnObservation& observation,
    const MillenniumDosSibmFunctionZeroCheckpoint& result,
    const std::uint16_t loaded_driver_segment) {
    require_admitted();
    const auto boundary = process_->boundary();
    if (recovery_entry_ != MillenniumDosNativeRecoveryEntry::startup
        || !observed_child_process_entry_ || loaded_driver_segment == 0
        || observation.sequence <= last_observation_sequence_
        || observation.sequence <= child_entry_sequence_
        || process_->state()
            != MillenniumDosNativeProcessState::startup_sibm_timer_vector_get_boundary
        || boundary.address != 0x0233 || boundary.interrupt != std::uint8_t{0x21}
        || boundary.service != std::uint8_t{0x35}
        || observation.interrupt_instruction_address != 0x0233
        || observation.interrupt_number != 0x21 || observation.service != 0x35
        || observation.return_code_segment != loaded_driver_segment
        || observation.return_instruction_pointer != 0x0235
        || result.state != MillenniumDosSibmFunctionZeroState::awaiting_timer_vector_set
        || result.last_observation_sequence != observation.sequence) {
        throw std::runtime_error(
            "SIBM AH=$35 return is detached from the loaded driver or active game handoff");
    }
    process_->observe_startup_sibm_function_zero_vector_get(result);
    last_observation_sequence_ = observation.sequence;
}

void MillenniumDosNativeProcessAdmission::observe_startup_sibm_timer_vector_set_return(
    const MillenniumDosNativeDosInterruptReturnObservation& observation,
    const MillenniumDosSibmFunctionZeroCheckpoint& result,
    const std::uint16_t loaded_driver_segment) {
    require_admitted();
    const auto boundary = process_->boundary();
    if (recovery_entry_ != MillenniumDosNativeRecoveryEntry::startup
        || !observed_child_process_entry_ || loaded_driver_segment == 0
        || observation.sequence <= last_observation_sequence_
        || observation.sequence <= child_entry_sequence_
        || process_->state()
            != MillenniumDosNativeProcessState::startup_sibm_timer_vector_set_boundary
        || boundary.address != 0x0246 || boundary.interrupt != std::uint8_t{0x21}
        || boundary.service != std::uint8_t{0x25}
        || observation.interrupt_instruction_address != 0x0246
        || observation.interrupt_number != 0x21 || observation.service != 0x25
        || observation.return_code_segment != loaded_driver_segment
        || observation.return_instruction_pointer != 0x0248
        || result.state
            != MillenniumDosSibmFunctionZeroState::timer_vector_set_return_observed
        || result.last_observation_sequence != observation.sequence
        || result.timer_vector_set_return_ax != observation.ax
        || result.timer_vector_set_return_bx != observation.bx
        || result.timer_vector_set_return_cx != observation.cx
        || result.timer_vector_set_return_dx != observation.dx
        || result.timer_vector_set_return_flags != observation.flags
        || result.timer_vector_set_return_es != observation.es) {
        throw std::runtime_error(
            "SIBM AH=$25 return is detached from the loaded driver or active game handoff");
    }
    process_->observe_startup_sibm_function_zero_vector_set_return(result);
    last_observation_sequence_ = observation.sequence;
}

void MillenniumDosNativeProcessAdmission::observe_private_interrupt_return(
    const MillenniumDosNativePrivateInterruptReturnObservation& observation) {
    require_admitted();
    const auto boundary = process_->boundary();
    if (!observed_child_process_entry_ || observation.sequence <= last_observation_sequence_
        || observation.sequence <= child_entry_sequence_
        || boundary.kind != MillenniumDosNativeBoundaryKind::private_interrupt
        || observation.interrupt_return_address != boundary.address) {
        throw std::runtime_error("Millennium DOS private return is detached from the observed child continuation");
    }
    process_->observe_private_interrupt_return(observation.interrupt_return_address,
        observation.ax);
    last_observation_sequence_ = observation.sequence;
}

void MillenniumDosNativeProcessAdmission::observe_child_process_entry(
    const std::uint64_t sequence, const std::uint16_t code_segment) {
    require_admitted();
    if (recovery_entry_ != MillenniumDosNativeRecoveryEntry::startup
        || observed_child_process_entry_ || sequence == 0 || code_segment == 0
        || process_->state() != MillenniumDosNativeProcessState::startup_first_private_interrupt) {
        throw std::runtime_error("Millennium DOS native startup entry is detached from the first private interrupt");
    }
    observed_child_process_entry_ = true;
    child_entry_sequence_ = sequence;
    child_code_segment_ = code_segment;
    last_observation_sequence_ = sequence;
}

MillenniumDosNativeFunction31DispatchResult
MillenniumDosNativeProcessAdmission::execute_startup_function_31_return(
    const std::uint64_t sequence,
    const std::span<const std::uint8_t> exact_driver_image,
    const std::span<const std::uint8_t> owned_loaded_driver_image,
    const MillenniumDosVideoDriverKind kind,
    const std::uint16_t loaded_driver_segment,
    const std::uint8_t vector_interrupt,
    const std::uint16_t vector_offset,
    const std::uint16_t vector_segment) {
    MillenniumDosNativeFunction31DispatchResult result;
    result.sequence = sequence;
    try {
        require_admitted();
        constexpr std::uint16_t int91_return_site = 0x0129;
        constexpr std::uint16_t int91_interrupt = 0x91;
        constexpr std::uint16_t com_load_bias = 0x0100;
        constexpr std::uint16_t startup_request_address = 0xd2b4;
        constexpr std::array<std::uint8_t, 20> startup_request{
            0x8c,0xc8,0x8e,0xd0,0xb8,0x00,0xda,0x89,0xc4,0xb8,
            0x1f,0x00,0x0e,0x07,0xbb,0x9e,0xd1,0xe8,0x5c,0x2e};
        const auto startup_offset = static_cast<std::size_t>(
            startup_request_address - com_load_bias);
        if (recovery_entry_ != MillenniumDosNativeRecoveryEntry::startup
            || !observed_child_process_entry_
            || process_->state() != MillenniumDosNativeProcessState::startup_first_private_interrupt
            || process_->boundary().kind != MillenniumDosNativeBoundaryKind::private_interrupt
            || process_->boundary().address != int91_return_site
            || sequence <= last_observation_sequence_
            || sequence <= child_entry_sequence_
            || startup_offset > game_executable_.size()
            || game_executable_.size() - startup_offset < startup_request.size()
            || !std::equal(startup_request.begin(), startup_request.end(),
                game_executable_.begin() + static_cast<std::ptrdiff_t>(startup_offset))) {
            throw std::runtime_error(
                "Startup is not at the exact first function-$1f INT $91h boundary");
        }
        const auto profile = parse_millennium_dos_video_driver(exact_driver_image, kind);
        if (loaded_driver_segment == 0 || vector_interrupt != int91_interrupt
            || vector_offset != 0 || vector_segment != loaded_driver_segment
            || owned_loaded_driver_image.size() != exact_driver_image.size()) {
            throw std::runtime_error("The active INT $91h vector or loaded image is detached");
        }
        // Only these four bytes are writable data in the covered English
        // function-zero/function-four/handler routes. Every instruction,
        // dispatch-table entry, and other driver byte must still be original.
        const std::array<std::uint16_t,4> mutable_cells{
            profile.function_thirty_one_state_address,
            profile.function_zero_cached_mode_address,
            profile.function_four_state_address,
            0x01e4};
        for (std::size_t offset = 0; offset < exact_driver_image.size(); ++offset) {
            const auto mutable_cell = std::ranges::find(mutable_cells,
                static_cast<std::uint16_t>(offset)) != mutable_cells.end();
            if (!mutable_cell && owned_loaded_driver_image[offset] != exact_driver_image[offset]) {
                throw std::runtime_error("Loaded driver has an unverified modified byte");
            }
        }
        const auto dispatch_prefix_sha256 = kind == MillenniumDosVideoDriverKind::ega640
            ? "a7d507882cf6bfe5b51db64b2dfb3ea8dd66f9d032b52622af2edac0a5ba81aa"
            : "2892839bc9f8ed84ce14e315addef727247431c808236b413356094d2392f35c";
        if (exact_driver_image.size() < 0x13
            || to_hex(sha256(exact_driver_image.first(0x13))) != dispatch_prefix_sha256
            || !std::equal(exact_driver_image.begin(), exact_driver_image.begin() + 0x13,
                owned_loaded_driver_image.begin())) {
            throw std::runtime_error("The private-handler DS setup differs from its source image");
        }
        result.target_address = profile.function_thirty_one_address;
        result.local_state_address = profile.function_thirty_one_state_address;
        result.local_state_value = owned_loaded_driver_image[result.local_state_address];
        MillenniumDosVideoFunction31Session function31(exact_driver_image, kind);
        function31.observe_local_state_read({sequence,profile.function_thirty_one_address,
            loaded_driver_segment,profile.function_thirty_one_state_address,
            result.local_state_value});
        const auto outcome = function31.outcome();
        if (!outcome || function31.state() != MillenniumDosVideoFunction31State::ret_boundary
            || outcome->ret_instruction_address != profile.function_thirty_one_address + 5U) {
            throw std::runtime_error("Private function-$1f did not reach its verified RET");
        }
        result.returned_ax = outcome->ax;

        // The 2200AD wrapper restores its saved registers and returns through
        // the source-established COM call frame. Function $1f changes AX from
        // the local byte and the profile's literal AH; FLAGS remain external.
        process_->observe_private_interrupt_return(int91_return_site,result.returned_ax);
        last_observation_sequence_ = sequence;
        result.accepted = true;
    } catch (const std::exception& exception) {
        result.error = exception.what();
    }
    return result;
}

void MillenniumDosNativeProcessAdmission::observe_runtime_byte(
    const std::uint16_t instruction_address, const std::uint16_t runtime_address,
    const std::uint8_t value) {
    require_admitted();
    process_->observe_runtime_byte(instruction_address, runtime_address, value);
}

void MillenniumDosNativeProcessAdmission::observe_native_call_return(
    const std::uint16_t call_address, const std::uint16_t return_address) {
    require_admitted();
    process_->observe_native_call_return(call_address, return_address);
}

void MillenniumDosNativeProcessAdmission::reset() {
    process_.reset();
    gx_overlay_executable_.clear();
    game_executable_.clear();
    observed_child_process_entry_ = false;
    child_entry_sequence_ = 0;
    child_code_segment_ = 0;
    last_observation_sequence_ = 0;
}

} // namespace eon
