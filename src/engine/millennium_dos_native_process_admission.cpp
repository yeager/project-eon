#include "engine/millennium_dos_native_process_admission.hpp"

#include "data/sha256.hpp"

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
MillenniumDosFourthFunctionSession MillenniumDosNativeProcessAdmission::make_fourth_function_session() const { require_admitted(); return MillenniumDosFourthFunctionSession(game_executable_); }
MillenniumDosFifthFunctionSession MillenniumDosNativeProcessAdmission::make_fifth_function_session() const { require_admitted(); return MillenniumDosFifthFunctionSession(game_executable_); }
MillenniumDosThirdFunctionSession MillenniumDosNativeProcessAdmission::make_third_function_session() const { require_admitted(); return MillenniumDosThirdFunctionSession(game_executable_); }

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
}

} // namespace eon
