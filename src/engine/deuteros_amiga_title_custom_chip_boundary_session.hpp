#pragma once

#include "data/deuteros_amiga_title_stage.hpp"
#include "engine/deuteros_amiga_input_device_compatibility.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>

namespace eon {

// The recovered callback reads only InputEvent bytes +$4..+$d. Keep this as
// a bounded prefix, not a claim that the complete Amiga InputEvent/timestamp
// or host-to-Amiga key mapping is implicit. USB HID physical usages are
// translated separately below; the game's keymap/character interpretation is
// still outside this byte-layout helper.
struct DeuterosAmigaTitleInputEventPrefix {
    std::uint8_t event_class = 0;
    std::uint8_t subclass = 0;
    std::uint16_t code = 0;
    std::uint16_t qualifier = 0;
    std::int16_t x = 0;
    std::int16_t y = 0;
};

// The title callback accepts the Amiga Input Device's left/right mouse
// button codes. Bit 7 is the matching button-up form.
[[nodiscard]] constexpr std::optional<std::uint8_t>
deuteros_amiga_raw_mouse_button_code(const std::uint8_t sdl_button,
    const bool button_up = false) noexcept {
    std::uint8_t code = 0;
    if (sdl_button == 1) code = 0x68;       // SDL_BUTTON_LEFT / IECODE_LBUTTON
    else if (sdl_button == 3) code = 0x69;  // SDL_BUTTON_RIGHT / IECODE_RBUTTON
    else return std::nullopt;
    if (button_up) code = static_cast<std::uint8_t>(code | 0x80U);
    return code;
}

// Convert a render-space delta over the game's presentation rectangle into
// the raw coordinate units expected by the recovered title callback. The
// local MoveSprite caller halves X after subtracting its hotspot offset, so
// its X state uses two units per 320-wide low-resolution pixel; Y remains
// one unit per 200-line pixel.
[[nodiscard]] inline std::optional<std::int16_t>
deuteros_amiga_raw_mouse_delta_from_renderer(const float render_delta,
    const float viewport_extent, const std::uint16_t source_extent,
    double& fractional_remainder) noexcept {
    if (!std::isfinite(render_delta) || !std::isfinite(viewport_extent)
        || viewport_extent <= 0.0F || source_extent == 0) {
        return std::nullopt;
    }
    const double accumulated = static_cast<double>(render_delta)
        * static_cast<double>(source_extent) / static_cast<double>(viewport_extent)
        + fractional_remainder;
    const double rounded = std::round(accumulated);
    if (!std::isfinite(rounded)
        || rounded < static_cast<double>(std::numeric_limits<std::int16_t>::min())
        || rounded > static_cast<double>(std::numeric_limits<std::int16_t>::max())) {
        return std::nullopt;
    }
    fractional_remainder = accumulated - rounded;
    return static_cast<std::int16_t>(rounded);
}

// Map USB HID keyboard usages (the physical usages used by SDL scancodes) to
// the classic Amiga keyboard matrix. Key-up is encoded with bit 7 as specified
// by the Amiga keyboard device. This maps physical positions, not characters
// or locale-specific keymaps.
[[nodiscard]] constexpr std::optional<std::uint8_t>
deuteros_amiga_raw_key_code_from_usb_usage(
    const std::uint16_t usage, const bool key_up = false) noexcept {
    constexpr std::array<std::uint8_t, 26> letter_codes{
        0x20, 0x35, 0x33, 0x22, 0x12, 0x23, 0x24, 0x25, 0x17, 0x26,
        0x27, 0x28, 0x37, 0x36, 0x18, 0x19, 0x10, 0x13, 0x21, 0x14,
        0x16, 0x34, 0x11, 0x32, 0x15, 0x31};
    std::uint8_t code = 0;
    if (usage >= 0x04 && usage <= 0x1d) {
        code = letter_codes[usage - 0x04];
    } else if (usage >= 0x1e && usage <= 0x26) {
        code = static_cast<std::uint8_t>(usage - 0x1d); // 1..9
    } else if (usage == 0x27) {
        code = 0x0a; // 0
    } else {
        switch (usage) {
        case 0x28: code = 0x44; break; // Return
        case 0x29: code = 0x45; break; // Escape
        case 0x2a: code = 0x41; break; // Backspace
        case 0x2b: code = 0x42; break; // Tab
        case 0x2c: code = 0x40; break; // Space
        case 0x2d: code = 0x0b; break; // - _
        case 0x2e: code = 0x0c; break; // = +
        case 0x2f: code = 0x1a; break; // [ {
        case 0x30: code = 0x1b; break; // ] }
        case 0x31: code = 0x0d; break; // backslash / pipe
        case 0x32: code = 0x2b; break; // non-US key near Return
        case 0x33: code = 0x29; break; // ; :
        case 0x34: code = 0x2a; break; // ' "
        case 0x35: code = 0x00; break; // grave / tilde
        case 0x36: code = 0x38; break; // , <
        case 0x37: code = 0x39; break; // . >
        case 0x38: code = 0x3a; break; // / ?
        case 0x3a: case 0x3b: case 0x3c: case 0x3d: case 0x3e:
        case 0x3f: case 0x40: case 0x41: case 0x42: case 0x43:
            code = static_cast<std::uint8_t>(0x50 + usage - 0x3a); break; // F1..F10
        case 0x4c: code = 0x46; break; // Delete
        case 0x4f: code = 0x4e; break; // Right
        case 0x50: code = 0x4f; break; // Left
        case 0x51: code = 0x4d; break; // Down
        case 0x52: code = 0x4c; break; // Up
        case 0x54: code = 0x5c; break; // Keypad /
        case 0x55: code = 0x5d; break; // Keypad *
        case 0x56: code = 0x4a; break; // Keypad -
        case 0x57: code = 0x5e; break; // Keypad +
        case 0x58: code = 0x43; break; // Keypad Enter
        case 0x59: case 0x5a: case 0x5b:
            code = static_cast<std::uint8_t>(0x1d + usage - 0x59); break; // Keypad 1..3
        case 0x5c: case 0x5d: case 0x5e:
            code = static_cast<std::uint8_t>(0x2d + usage - 0x5c); break; // Keypad 4..6
        case 0x5f: case 0x60: case 0x61:
            code = static_cast<std::uint8_t>(0x3d + usage - 0x5f); break; // Keypad 7..9
        case 0x62: code = 0x0f; break; // Keypad 0
        case 0x63: code = 0x3c; break; // Keypad .
        case 0x64: code = 0x30; break; // non-US key near left Shift
        default: return std::nullopt;
        }
    }
    if (key_up) code = static_cast<std::uint8_t>(code | 0x80U);
    return code;
}

[[nodiscard]] constexpr std::array<std::uint8_t, 14>
encode_deuteros_amiga_title_input_event_prefix(
    const DeuterosAmigaTitleInputEventPrefix event,
    const std::uint32_t next_event = 0) noexcept {
    const auto word = [](const std::uint16_t value) constexpr {
        return std::array<std::uint8_t, 2>{
            static_cast<std::uint8_t>(value >> 8U),
            static_cast<std::uint8_t>(value)};
    };
    const auto longword = [](const std::uint32_t value) constexpr {
        return std::array<std::uint8_t, 4>{
            static_cast<std::uint8_t>(value >> 24U),
            static_cast<std::uint8_t>(value >> 16U),
            static_cast<std::uint8_t>(value >> 8U),
            static_cast<std::uint8_t>(value)};
    };
    std::array<std::uint8_t, 14> bytes{};
    const auto next = longword(next_event);
    for (std::size_t i = 0; i < next.size(); ++i) bytes[i] = next[i];
    bytes[4] = event.event_class;
    bytes[5] = event.subclass;
    const auto code = word(event.code);
    const auto qualifier = word(event.qualifier);
    const auto x = word(static_cast<std::uint16_t>(event.x));
    const auto y = word(static_cast<std::uint16_t>(event.y));
    for (std::size_t i = 0; i < 2; ++i) {
        bytes[6 + i] = code[i];
        bytes[8 + i] = qualifier[i];
        bytes[10 + i] = x[i];
        bytes[12 + i] = y[i];
    }
    return bytes;
}

struct DeuterosAmigaObservedCustomChipWrite {
    std::uint64_t trace_sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t base_address = 0;
    std::uint16_t register_offset = 0;
    std::uint16_t value = 0;
};

struct DeuterosAmigaDeferredCallbackExecCall {
    std::uint32_t exec_base_read_address = 0;
    std::uint32_t exec_base_source_address = 0;
    std::uint32_t call_address = 0;
    std::int16_t vector = 0;
    std::uint32_t return_address = 0;
};

struct DeuterosAmigaTitleCallbackRegistrationLocalPlan {
    std::array<DeuterosAmigaObservedCustomChipWrite, 4> observed_writes{};
    std::uint32_t call_address = 0;
    std::uint32_t call_target = 0;
    std::uint32_t descriptor_address = 0;
    std::uint16_t descriptor_owner_offset = 0;
    std::uint32_t descriptor_owner_value = 0;
    std::uint16_t descriptor_callback_offset = 0;
    std::uint32_t descriptor_callback_value = 0;
    std::uint32_t request_address = 0;
    std::uint16_t request_command_offset = 0;
    std::uint16_t request_command_value = 0;
    std::uint16_t request_descriptor_offset = 0;
    std::uint32_t request_descriptor_value = 0;
    DeuterosAmigaDeferredCallbackExecCall deferred_exec_call;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaNativeDisplayInitialization {
    std::uint64_t runtime_generation = 0;
    std::uint32_t base_address = 0;
    std::uint32_t byte_count = 0;
    std::array<std::uint16_t, 4> custom_register_offsets{};
    std::array<std::uint16_t, 4> custom_register_values{};
    std::uint32_t source_entry_address = 0;
    DeuterosAmigaTitleCallbackRegistrationLocalPlan callback_registration;
};

struct DeuterosAmigaObservedCallbackExecReturn {
    std::uint64_t trace_sequence = 0;
    std::uint32_t exec_base_source_address = 0;
    std::uint32_t call_address = 0;
    std::int16_t vector = 0;
    std::uint32_t return_address = 0;
    std::uint32_t result_d0 = 0;
    std::uint16_t result_sr = 0;
};

struct DeuterosAmigaTitlePostCallbackRegistrationAdvance {
    DeuterosAmigaObservedCallbackExecReturn observed_return;
    std::uint32_t registration_rts_address = 0;
    std::uint32_t caller_return_address = 0;
    std::uint32_t next_call_address = 0;
    std::uint32_t next_call_target = 0;
    std::uint32_t stop_before_address = 0;
};

// One observed callback byte-one producer invocation. The input fields are
// original callback-frame and RAM values; they do not identify a host key or
// assign gameplay meaning. Entry/return samples and before/after memory are
// supplied together so the runtime can admit this producer atomically.
struct DeuterosAmigaObservedTitleCallbackProducer {
    std::uint64_t entry_trace_sequence = 0;
    std::uint64_t return_trace_sequence = 0;
    std::uint32_t entry_address = 0;
    std::uint32_t return_sample_address = 0;
    std::uint8_t caller_event_byte = 0;
    std::uint16_t caller_word_at_6 = 0;
    std::uint16_t caller_word_at_8 = 0;
    std::uint8_t mirrored_event_before = 0;
    std::uint8_t mirrored_event_after = 0;
    std::uint16_t selector_word_before = 0;
    std::uint16_t selector_word_after = 0;
    std::array<std::uint8_t, 21> queue_before{};
    std::array<std::uint8_t, 21> queue_after{};
    std::uint16_t pending_count_before = 0;
    std::uint16_t pending_count_after = 0;
    bool operator==(const DeuterosAmigaObservedTitleCallbackProducer&) const = default;
};

struct DeuterosAmigaTitleCallbackProducerRuntimePlan {
    DeuterosAmigaObservedTitleCallbackProducer observation;
    DeuterosAmigaTitleCallbackProducerResult producer;
    bool operator==(const DeuterosAmigaTitleCallbackProducerRuntimePlan&) const = default;
};

// Input-linked v33 samples for the callback-owned $20118 -> MoveSprite route.
// The register values are captured at the instruction sites; they carry no
// host-action or graphics-library interpretation by themselves.
struct DeuterosAmigaObservedTitleMoveSpriteCall {
    std::uint64_t sequence = 0;
    std::array<std::uint64_t, 5> sample_cycles{};
    std::array<std::uint32_t, 5> instruction_addresses{};
    std::uint16_t input_ordinal = 0;
    std::int32_t input_frame = 0;
    std::uint32_t caller_return_address = 0;
    DeuterosAmigaTitleMoveSpriteInput input;
    std::uint32_t d0 = 0;
    std::uint32_t d1 = 0;
    std::uint32_t a0 = 0;
    std::uint32_t a1 = 0;
    std::uint32_t a6 = 0;
    bool operator==(const DeuterosAmigaObservedTitleMoveSpriteCall&) const = default;
};

struct DeuterosAmigaTitleMoveSpriteRuntimePlan {
    DeuterosAmigaObservedTitleMoveSpriteCall observation;
    DeuterosAmigaTitleMoveSpriteResult expected;
    bool arguments_match = false;
    bool operator==(const DeuterosAmigaTitleMoveSpriteRuntimePlan&) const = default;
};

// One complete, host-input-linked observation of the original response-queue
// caller, dequeue helper, and caller continuation. The snapshots preserve
// the captured values verbatim; they do not assign a meaning to the returned
// byte or to any host input.
struct DeuterosAmigaObservedTitleResponseQueue {
    std::uint64_t sequence = 0;
    std::array<std::uint64_t, 6> sample_cycles{};
    std::array<std::uint32_t, 6> instruction_addresses{};
    std::array<std::uint16_t, 6> input_ordinals{};
    std::array<std::uint32_t, 3> stack_return_addresses{};
    std::array<std::uint16_t, 2> d0_values{};
    std::uint16_t second_read_sr = 0;
    std::array<std::array<std::uint8_t, 21>, 6> queue_snapshots{};
    std::array<std::uint16_t, 6> pending_counts{};
    bool operator==(const DeuterosAmigaObservedTitleResponseQueue&) const = default;
};

struct DeuterosAmigaTitleResponseQueueRuntimePlan {
    DeuterosAmigaObservedTitleResponseQueue observation;
    DeuterosAmigaTitleResponseQueueResult dequeue;
    bool operator==(const DeuterosAmigaTitleResponseQueueRuntimePlan&) const = default;
};

class DeuterosAmigaTitleCustomChipBoundarySession {
public:
    DeuterosAmigaTitleCustomChipBoundarySession(std::uint64_t preceding_trace_sequence,
        const DeuterosAmigaTitleStageProfile& stage,
        const DeuterosAmigaTitleCallbackRegistrationProfile& callback)
        : preceding_trace_sequence_(preceding_trace_sequence), callback_(callback) {
        if (preceding_trace_sequence == 0
            || stage.initialization_custom_base_address != 0xdff000
            || stage.initialization_custom_offsets != expected_offsets_
            || stage.initialization_custom_values != expected_values_
            || stage.initialization_internal_calls[3] != 0x1ef74
            || stage.initialization_internal_calls[4] != 0x206d4
            || callback.registration_entry_address != 0x1ef74
            || callback.descriptor_address != 0x1ef48
            || callback.descriptor_callback_offset != 0x12
            || callback.callback_address != 0x1f056
            || callback.request_address != 0x1eefa
            || callback.request_command_offset != 0x1c
            || callback.request_command_value != 9
            || callback.request_descriptor_offset != 0x28
            || callback.exec_base_address != 4 || callback.exec_vector != -0x1ce
            || callback.registration_return_address != 0x1f052
            || callback.registration_sha256
                != "f571a8e5e48c29fe3d6f493e503e2a3a0b3328ac4cafb425808eff48804c4f27") {
            throw std::runtime_error("Invalid Deuteros custom-chip boundary provenance");
        }
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleCallbackRegistrationLocalPlan>
    observe_write(const DeuterosAmigaObservedCustomChipWrite& observation) {
        if (complete_) return std::nullopt;
        constexpr std::array<std::uint32_t, 4> sites{0x40498, 0x4049e, 0x404a4, 0x404aa};
        const auto previous = count_ == 0
            ? preceding_trace_sequence_ : observations_[count_ - 1].trace_sequence;
        if (count_ >= sites.size() || observation.trace_sequence <= previous
            || observation.instruction_address != sites[count_]
            || observation.base_address != 0xdff000
            || observation.register_offset != expected_offsets_[count_]
            || observation.value != expected_values_[count_]) {
            throw std::runtime_error("Deuteros custom-chip observation does not match boundary");
        }
        observations_[count_++] = observation;
        if (count_ != observations_.size()) return std::nullopt;
        complete_ = true;
        return DeuterosAmigaTitleCallbackRegistrationLocalPlan{observations_,
            0x404b0, callback_.registration_entry_address, callback_.descriptor_address,
            0x0e, 0x1ef40, callback_.descriptor_callback_offset, callback_.callback_address,
            callback_.request_address, callback_.request_command_offset,
            callback_.request_command_value, callback_.request_descriptor_offset,
            callback_.descriptor_address,
            {0x1f04a, callback_.exec_base_address, 0x1f04e,
                callback_.exec_vector, callback_.registration_return_address},
            0x1f04a};
    }

    [[nodiscard]] std::optional<DeuterosAmigaNativeDisplayInitialization>
    initialize_native_display(const std::uint64_t runtime_generation,
        const std::uint32_t base_address, const std::uint32_t byte_count) {
        if (complete_ || native_display_initialization_
            || runtime_generation == 0 || base_address < 0x00e00000
            || base_address > 0x00f00000 - 0x7d00 || byte_count != 0x7d00
            || (base_address & 0xfffU) != 0 || count_ != 0) {
            return std::nullopt;
        }
        native_display_initialization_ = DeuterosAmigaNativeDisplayInitialization{
            runtime_generation, base_address, byte_count, expected_offsets_,
            expected_values_, 0x40498,
            DeuterosAmigaTitleCallbackRegistrationLocalPlan{{},
                0x404b0, callback_.registration_entry_address,
                callback_.descriptor_address, 0x0e, 0x1ef40,
                callback_.descriptor_callback_offset, callback_.callback_address,
                callback_.request_address, callback_.request_command_offset,
                callback_.request_command_value, callback_.request_descriptor_offset,
                callback_.descriptor_address,
                {0x1f04a, callback_.exec_base_address, 0x1f04e,
                    callback_.exec_vector, callback_.registration_return_address},
                0x1f04a}};
        complete_ = true;
        return native_display_initialization_;
    }

    [[nodiscard]] bool accept_native_input_handler_registration(
        const DeuterosAmigaNativeInputHandlerRegistration& registration) noexcept {
        if (!native_display_initialization_ || native_input_handler_registration_
            || registration.runtime_generation == 0
            || registration.runtime_generation
                != native_display_initialization_->runtime_generation
            || registration.service_sequence != 3
            || registration.open_device_call_address != 0x1efb0
            || registration.add_handler_call_address != 0x1f04e
            || registration.add_handler_vector != -0x1ce
            || registration.callback_address != callback_.callback_address
            || registration.origin
                != DeuterosAmigaInputHandlerOrigin::native_compatibility) {
            return false;
        }
        native_input_handler_registration_ = registration;
        return true;
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCallbackRegistrationAdvance>
    observe_exec_return(const DeuterosAmigaObservedCallbackExecReturn& observation) {
        if (!complete_ || observed_exec_return_) return std::nullopt;
        if (observation.trace_sequence <= observations_.back().trace_sequence
            || observation.exec_base_source_address != 4
            || observation.call_address != 0x1f04e || observation.vector != -0x1ce
            || observation.return_address != 0x1f052) {
            throw std::runtime_error("Deuteros callback Exec return does not match boundary");
        }
        observed_exec_return_ = observation;
        return DeuterosAmigaTitlePostCallbackRegistrationAdvance{
            observation, 0x1f052, 0x404b6, 0x404b6, 0x206d4, 0x404b6};
    }

    [[nodiscard]] std::size_t observed_write_count() const noexcept { return count_; }
    [[nodiscard]] bool complete() const noexcept { return complete_; }
    [[nodiscard]] const std::optional<DeuterosAmigaNativeDisplayInitialization>&
    native_display_initialization() const noexcept {
        return native_display_initialization_;
    }
    [[nodiscard]] const std::optional<DeuterosAmigaNativeInputHandlerRegistration>&
    native_input_handler_registration() const noexcept {
        return native_input_handler_registration_;
    }
    [[nodiscard]] const std::optional<DeuterosAmigaObservedCallbackExecReturn>&
    observed_exec_return() const noexcept { return observed_exec_return_; }
    [[nodiscard]] std::uint32_t stop_before_address() const noexcept {
        return complete_ ? 0x1f04a : std::array<std::uint32_t, 4>{
            0x40498, 0x4049e, 0x404a4, 0x404aa}[count_];
    }

private:
    static constexpr std::array<std::uint16_t, 4> expected_offsets_{
        0x40, 0x42, 0x9a, 0x96};
    static constexpr std::array<std::uint16_t, 4> expected_values_{
        0x7fff, 0x7fff, 0xc000, 0x87ff};
    std::uint64_t preceding_trace_sequence_ = 0;
    DeuterosAmigaTitleCallbackRegistrationProfile callback_;
    std::array<DeuterosAmigaObservedCustomChipWrite, 4> observations_{};
    std::size_t count_ = 0;
    bool complete_ = false;
    std::optional<DeuterosAmigaNativeDisplayInitialization>
        native_display_initialization_;
    std::optional<DeuterosAmigaNativeInputHandlerRegistration>
        native_input_handler_registration_;
    std::optional<DeuterosAmigaObservedCallbackExecReturn> observed_exec_return_;
};

} // namespace eon
