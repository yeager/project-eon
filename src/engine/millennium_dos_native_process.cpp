#include "engine/millennium_dos_native_process.hpp"

#include <stdexcept>

namespace eon {
namespace {
constexpr std::uint16_t private_interrupt_site = 0x0129;
constexpr std::uint16_t gx_adapter_call_site = 0xd373;
constexpr std::uint16_t gx_adapter_return_site = 0xd376;
constexpr std::uint16_t gx_mode_read_site = 0xd349;
constexpr std::uint16_t gx_post_mode_read_site = 0xd388;
constexpr std::uint16_t gx_mode_byte_address = 0xda05;
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
            private_interrupt_site, std::uint8_t{0x91}, std::nullopt, 0};
    case MillenniumDosNativeProcessState::startup_local_return_boundary:
        return {MillenniumDosNativeBoundaryKind::local_return,
            startup_prefix_->boundary_address, std::nullopt, std::nullopt, 0};
    case MillenniumDosNativeProcessState::startup_palette_interrupt_boundary:
        return {MillenniumDosNativeBoundaryKind::bios_interrupt,
            startup_prefix_->boundary_address, std::uint8_t{0x10}, std::nullopt, 0};
    case MillenniumDosNativeProcessState::gx_mode_byte:
        return {MillenniumDosNativeBoundaryKind::runtime_byte,
            gx_mode_read_site, std::nullopt, gx_mode_byte_address, 0};
    case MillenniumDosNativeProcessState::gx_adapter_return:
        return {MillenniumDosNativeBoundaryKind::native_call_return,
            gx_adapter_call_site, std::nullopt, std::nullopt, 0};
    case MillenniumDosNativeProcessState::gx_post_overlay_call_return: {
        const auto count = gx_session_->observed_post_overlay_call_return_count();
        return {MillenniumDosNativeBoundaryKind::native_call_return,
            static_cast<std::uint16_t>(gx_adapter_return_site + count * 3U),
            std::nullopt, std::nullopt, count};
    }
    case MillenniumDosNativeProcessState::gx_post_overlay_mode_byte:
        return {MillenniumDosNativeBoundaryKind::runtime_byte,
            gx_post_mode_read_site, std::nullopt, gx_mode_byte_address, 1};
    }
    throw std::runtime_error("Unsupported Millennium DOS native process state");
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

void MillenniumDosNativeProcess::observe_runtime_byte(
    const std::uint16_t instruction_address, const std::uint16_t runtime_address,
    const std::uint8_t value) {
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
    throw std::runtime_error("Out-of-order Millennium DOS runtime-byte observation");
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
