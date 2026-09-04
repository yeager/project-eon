#pragma once

#include "data/millennium_dos_game_flow.hpp"
#include "engine/millennium_dos_gx_startup_session.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>

namespace eon {

// One manually recompiled, hash-bound continuation of the English DOS game.
// It never decodes arbitrary x86 and cannot cross a native call by itself.
enum class MillenniumDosNativeProcessState {
    startup_first_private_interrupt,
    startup_selected_private_interrupt,
    startup_local_return_boundary,
    startup_palette_interrupt_boundary,
    gx_private_interrupt,
    gx_mode_byte,
    gx_adapter_return,
    gx_post_overlay_call_return,
    gx_post_overlay_mode_byte,
    gx_post_overlay_private_interrupt,
};

enum class MillenniumDosNativeBoundaryKind {
    private_interrupt,
    bios_interrupt,
    runtime_byte,
    native_call_return,
    local_return,
};

struct MillenniumDosNativeBoundary {
    MillenniumDosNativeBoundaryKind kind = MillenniumDosNativeBoundaryKind::private_interrupt;
    std::uint16_t address = 0;
    std::optional<std::uint8_t> interrupt;
    std::optional<std::uint16_t> runtime_byte_address;
    std::size_t ordinal = 0;
    constexpr bool operator==(const MillenniumDosNativeBoundary&) const = default;
};

// `startup()` begins only at 2200AD.EXE's independently verified entry.
// `post_gx_loader()` is a separate recovery entry and deliberately does not
// claim that the title/startup path reached it. The caller must keep both
// authenticated source spans alive for the lifetime of this object.
class MillenniumDosNativeProcess {
public:
    [[nodiscard]] static MillenniumDosNativeProcess startup(
        std::span<const std::uint8_t> game_executable);
    [[nodiscard]] static MillenniumDosNativeProcess post_gx_loader(
        std::span<const std::uint8_t> game_executable,
        std::span<const std::uint8_t> gx_overlay_executable);

    [[nodiscard]] MillenniumDosNativeProcessState state() const { return state_; }
    [[nodiscard]] MillenniumDosNativeBoundary boundary() const;

    // Observations are accepted only at the exact currently published
    // boundary. Address arguments prevent a result from being replayed at a
    // different INT, call, or native byte read.
    void observe_private_interrupt_return(std::uint16_t address, std::uint16_t ax);
    void observe_runtime_byte(std::uint16_t instruction_address,
        std::uint16_t runtime_address, std::uint8_t value);
    void observe_native_call_return(std::uint16_t call_address,
        std::uint16_t return_address);

    [[nodiscard]] std::optional<std::uint8_t> runtime_byte(std::uint16_t address) const;
    [[nodiscard]] std::optional<std::uint8_t> gx_overlay_byte(std::uint16_t offset) const;

private:
    explicit MillenniumDosNativeProcess(std::span<const std::uint8_t> game_executable);
    MillenniumDosNativeProcess(std::span<const std::uint8_t> game_executable,
        std::span<const std::uint8_t> gx_overlay_executable);
    void apply_startup_writes(const MillenniumDosEnglishStartupPrefix& prefix);

    std::span<const std::uint8_t> game_executable_;
    MillenniumDosNativeProcessState state_ =
        MillenniumDosNativeProcessState::startup_first_private_interrupt;
    std::optional<std::uint16_t> startup_first_ax_;
    std::optional<MillenniumDosEnglishStartupPrefix> startup_prefix_;
    std::unique_ptr<MillenniumDosGxStartupSession> gx_session_;
    std::map<std::uint16_t, std::uint8_t> runtime_bytes_;
};

} // namespace eon
