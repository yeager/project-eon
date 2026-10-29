#pragma once

#include "data/deuteros_amiga_title_stage.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace eon {

enum class DeuterosAmigaInputHandlerOrigin : std::uint8_t {
    native_compatibility,
};

struct DeuterosAmigaNativeInputHandlerRegistration {
    std::uint64_t runtime_generation = 0;
    std::uint64_t service_sequence = 0;
    std::uint32_t open_device_call_address = 0;
    std::uint32_t add_handler_call_address = 0;
    std::int16_t add_handler_vector = 0;
    std::uint32_t callback_address = 0;
    DeuterosAmigaInputHandlerOrigin origin =
        DeuterosAmigaInputHandlerOrigin::native_compatibility;
};

// Eon-owned substitute for the exact input.device registration contract
// parsed from the English Deuteros title stage. This records a local
// compatibility service; it is not an observation of the original Exec.
class DeuterosAmigaInputDeviceCompatibilitySession {
public:
    [[nodiscard]] std::optional<DeuterosAmigaNativeInputHandlerRegistration>
    register_title_handler_native(
        const DeuterosAmigaTitleCallbackRegistrationProfile& profile,
        const std::uint64_t runtime_generation) noexcept {
        if (runtime_generation == 0 || runtime_generation_ != 0
            || !register_title_handler(profile))
            return std::nullopt;
        runtime_generation_ = runtime_generation;
        return DeuterosAmigaNativeInputHandlerRegistration{
            runtime_generation, 3, profile.open_device_call_address,
            profile.registration_entry_address + 0xdaU,
            profile.exec_vector, profile.callback_address,
            DeuterosAmigaInputHandlerOrigin::native_compatibility};
    }

    [[nodiscard]] bool register_title_handler(
        const DeuterosAmigaTitleCallbackRegistrationProfile& profile) noexcept {
        constexpr std::string_view registration_hash =
            "f571a8e5e48c29fe3d6f493e503e2a3a0b3328ac4cafb425808eff48804c4f27";
        constexpr std::string_view callback_hash =
            "ff4b055b2d5128465c891debcad00ff4e53cbf661de47b9ee3d6278f33d5e5f8";
        if (profile.registration_entry_address != 0x1ef74
            || profile.registration_sha256 != registration_hash
            || profile.callback_sha256 != callback_hash
            || profile.open_device_request_address != 0x1eefa
            || profile.open_device_name_address != 0x1edfc
            || profile.open_device_call_address != 0x1efb0
            || profile.open_device_exec_vector != -0x1bc
            || profile.open_device_unit != 0 || profile.open_device_flags != 0
            || profile.request_command_value != 9
            || profile.request_descriptor_offset != 0x28
            || profile.descriptor_address != 0x1ef48
            || profile.descriptor_callback_offset != 0x12
            || profile.callback_address != 0x1f056
            || profile.exec_base_address != 4 || profile.exec_vector != -0x1ce) {
            return false;
        }
        opened_ = true;
        handler_added_ = true;
        return true;
    }

    void reset() noexcept {
        opened_ = false;
        handler_added_ = false;
        runtime_generation_ = 0;
    }
    [[nodiscard]] bool opened() const noexcept { return opened_; }
    [[nodiscard]] bool handler_added() const noexcept { return handler_added_; }
    [[nodiscard]] std::uint64_t runtime_generation() const noexcept {
        return runtime_generation_;
    }

private:
    bool opened_ = false;
    bool handler_added_ = false;
    std::uint64_t runtime_generation_ = 0;
};

} // namespace eon
