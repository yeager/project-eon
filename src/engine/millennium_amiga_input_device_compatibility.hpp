#pragma once

#include "data/millennium_amiga_loader.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace eon {

enum class MillenniumAmigaInputDeviceCompatibilityOrigin : std::uint8_t {
    eon_native_compatibility,
};

// A caller-owned, explicitly Eon-native result for the three source-identified
// device calls. These fields are not observed Kickstart/Exec return values.
struct MillenniumAmigaInputDeviceRegistrationOutcome {
    std::uint64_t runtime_generation = 0;
    std::uint32_t open_device_call_address = 0;
    std::int16_t open_device_vector = 0;
    std::uint32_t open_device_return_address = 0;
    bool open_device_succeeded = false;
    std::uint32_t reset_doio_call_address = 0;
    std::int16_t reset_doio_vector = 0;
    std::uint32_t reset_doio_return_address = 0;
    std::uint16_t reset_doio_command = 0;
    bool reset_doio_succeeded = false;
    std::uint32_t add_handler_call_address = 0;
    std::int16_t add_handler_vector = 0;
    std::uint32_t add_handler_return_address = 0;
    std::uint16_t add_handler_command = 0;
    std::uint32_t handler_structure_address = 0;
    bool add_handler_succeeded = false;
    MillenniumAmigaInputDeviceCompatibilityOrigin origin =
        MillenniumAmigaInputDeviceCompatibilityOrigin::eon_native_compatibility;
};

// Register/queue model derived only from the English Defjam source spans
// admitted by MillenniumAmigaInputDeviceOpenBoundary. This is a native
// compatibility implementation of that one callback's data effects; it does
// not execute 68k code, model input.device internals, or infer game actions.
class MillenniumAmigaInputDeviceCompatibilitySession {
public:
    struct RawInputEvent {
        std::uint32_t event_list_address = 0;
        std::uint8_t event_class = 0;
        std::uint16_t code = 0;
        std::uint16_t qualifier = 0;
    };

    struct DispatchResult {
        bool accepted = false;
        bool consumed = false;
        bool queued_character = false;
        std::uint8_t translated_character = 0;
        std::uint32_t returned_event_list_address = 0;
        std::uint8_t last_event_class = 0;
        std::uint8_t event_flag = 0;
        std::uint16_t queued_count = 0;
    };

    // The session borrows the original key-table span from `disk`; the ADF
    // storage must outlive this compatibility session. It never copies or
    // writes original table bytes.
    MillenniumAmigaInputDeviceCompatibilitySession(
        const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
        std::uint64_t runtime_generation);

    [[nodiscard]] bool register_native_compatibility_handler(
        const MillenniumAmigaInputDeviceRegistrationOutcome& outcome) noexcept;

    // This accepts already translated host input as the Amiga RAWKEY code and
    // qualifier. SDL key-to-raw-code conversion belongs at the runtime edge.
    [[nodiscard]] DispatchResult dispatch(const RawInputEvent& event,
        std::uint64_t runtime_generation) noexcept;

    // Matches the hash-bound queue consumer: return byte zero, shift the next
    // 20 queue bytes down, and decrement the count. An empty queue returns no
    // value rather than synthesizing input.
    [[nodiscard]] std::optional<std::uint8_t> take_next_character(
        std::uint64_t runtime_generation) noexcept;

    [[nodiscard]] bool handler_registered() const noexcept { return registered_; }
    [[nodiscard]] std::uint16_t queued_count() const noexcept { return queued_count_; }
    [[nodiscard]] std::uint16_t previous_raw_key_code() const noexcept {
        return previous_raw_key_code_;
    }
    [[nodiscard]] std::uint16_t previous_qualifier() const noexcept {
        return previous_qualifier_;
    }
    [[nodiscard]] std::uint8_t event_flag() const noexcept { return event_flag_; }
    [[nodiscard]] const MillenniumAmigaInputDeviceOpenBoundary& source_boundary()
        const noexcept { return source_boundary_; }

private:
    std::uint64_t runtime_generation_ = 0;
    MillenniumAmigaInputDeviceOpenBoundary source_boundary_;
    std::span<const std::uint8_t> key_translation_table_;
    std::array<std::uint8_t, 0x15> input_queue_{};
    std::uint16_t previous_raw_key_code_ = 0;
    std::uint16_t previous_qualifier_ = 0;
    std::uint16_t queued_count_ = 0;
    std::uint8_t last_event_class_ = 0;
    std::uint8_t event_flag_ = 0;
    bool registered_ = false;
};

} // namespace eon
