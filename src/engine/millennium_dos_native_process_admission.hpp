#pragma once

#include "engine/millennium_dos_native_process.hpp"
#include "engine/millennium_dos_post_overlay_loop_session.hpp"
#include "engine/millennium_dos_function_dispatch_admission.hpp"
#include "engine/millennium_dos_seventh_function_session.hpp"
#include "engine/millennium_dos_sixth_function_session.hpp"
#include "engine/millennium_dos_eighth_function_session.hpp"
#include "engine/millennium_dos_ninth_function_session.hpp"
#include "engine/millennium_dos_tenth_function_session.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace eon {

enum class MillenniumDosNativeRecoveryEntry {
    startup,
    post_gx_loader,
};

// Copy-only diagnostic value for a prepared native continuation. It contains
// no source path, original bytes, mutable process, or claim of reachability.
struct MillenniumDosNativeProcessCheckpoint {
    MillenniumDosNativeRecoveryEntry recovery_entry =
        MillenniumDosNativeRecoveryEntry::startup;
    MillenniumDosNativeProcessState state =
        MillenniumDosNativeProcessState::startup_first_private_interrupt;
    MillenniumDosNativeBoundary boundary;
    std::string release_sha256;
    std::string game_executable_sha256;
    std::optional<std::string> gx_overlay_sha256;
    bool static_recovery_entry = true;
};

// Owns the private backing copies required by MillenniumDosNativeProcess's
// read-only spans. This object is intended to live inside the release runtime;
// its public surface is typed observations plus a value-only checkpoint.
// Construction requires the caller's exact English release identity and
// independently validates both leaf byte streams. Authentication of the
// outer release container remains the future coordinator's responsibility.
// It does not assert that title execution reached either recovery entry.
class MillenniumDosNativeProcessAdmission {
public:
    [[nodiscard]] static MillenniumDosNativeProcessAdmission startup(
        std::string_view release_sha256,
        std::span<const std::uint8_t> game_executable);
    [[nodiscard]] static MillenniumDosNativeProcessAdmission post_gx_loader(
        std::string_view release_sha256,
        std::span<const std::uint8_t> game_executable,
        std::span<const std::uint8_t> gx_overlay_executable);

    MillenniumDosNativeProcessAdmission() = default;
    MillenniumDosNativeProcessAdmission(MillenniumDosNativeProcessAdmission&&) = default;
    MillenniumDosNativeProcessAdmission& operator=(MillenniumDosNativeProcessAdmission&&) = default;
    MillenniumDosNativeProcessAdmission(const MillenniumDosNativeProcessAdmission&) = delete;
    MillenniumDosNativeProcessAdmission& operator=(
        const MillenniumDosNativeProcessAdmission&) = delete;
    ~MillenniumDosNativeProcessAdmission() = default;

    [[nodiscard]] bool admitted() const { return process_ != nullptr && error_.empty(); }
    [[nodiscard]] const std::string& error() const { return error_; }
    [[nodiscard]] std::optional<MillenniumDosNativeProcessCheckpoint> checkpoint() const;
    // Constructs a span-based continuation whose lifetime must remain nested
    // within this admission. The exact backing bytes never cross the API.
    [[nodiscard]] MillenniumDosPostOverlayLoopSession make_post_overlay_loop_session(
        std::uint8_t selected_mode_byte) const;
    [[nodiscard]] MillenniumDosTenthFunctionSession make_tenth_function_session() const;
    [[nodiscard]] MillenniumDosSeventhFunctionSession make_seventh_function_session() const;
    [[nodiscard]] MillenniumDosSixthFunctionSession make_sixth_function_session() const;
    [[nodiscard]] MillenniumDosFunctionDispatchAdmission admit_function_dispatch(
        const MillenniumDosPostOverlayLoopSession& loop,
        MillenniumDosFunctionDispatchObservation observation) const;
    [[nodiscard]] MillenniumDosEighthFunctionSession make_eighth_function_session() const;
    [[nodiscard]] MillenniumDosNinthFunctionSession make_ninth_function_session() const;

    void observe_private_interrupt_return(std::uint16_t address, std::uint16_t ax);
    void observe_runtime_byte(std::uint16_t instruction_address,
        std::uint16_t runtime_address, std::uint8_t value);
    void observe_native_call_return(std::uint16_t call_address,
        std::uint16_t return_address);

    // Revokes the process before releasing its backing source bytes. A stale
    // checkpoint remains a detached value and cannot advance the new state.
    void reset();

private:
    static constexpr std::string_view english_release_sha256 =
        "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123";
    static constexpr std::string_view game_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    static constexpr std::string_view gx_sha256 =
        "093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb";

    void require_admitted() const;

    // Declaration order is intentional: process_ is destroyed before spans'
    // backing buffers during ordinary destruction.
    std::vector<std::uint8_t> game_executable_;
    std::vector<std::uint8_t> gx_overlay_executable_;
    std::unique_ptr<MillenniumDosNativeProcess> process_;
    MillenniumDosNativeRecoveryEntry recovery_entry_ =
        MillenniumDosNativeRecoveryEntry::startup;
    std::string error_;
};

} // namespace eon
