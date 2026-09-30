#pragma once

#include "engine/millennium_dos_video_function_13_session.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace eon {

enum class MillenniumDosVideoFunction13InterruptState {
    awaiting_interrupt_request,
    awaiting_retrace_port_read,
    awaiting_mcga_postlude_byte,
    iret_boundary,
    callback_boundary,
    awaiting_mcga_callback_word,
    callback_local_boundary,
    returned,
};

struct MillenniumDosVideoFunction13InterruptBoundary {
    std::uint16_t instruction_address = 0;
    constexpr bool operator==(const MillenniumDosVideoFunction13InterruptBoundary&) const = default;
};

struct MillenniumDosVideoFunction13InterruptRequest {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0x0127;
    std::uint16_t ax = 0x0013;
    std::uint16_t return_ip = 0x0129;
    std::uint16_t return_cs = 0;
    std::uint16_t return_flags = 0;
};

struct MillenniumDosVideoFunction13McgaPostludeByte {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0x001a;
    std::uint16_t segment = 0;
    std::uint16_t offset = 0x01e5;
    std::uint8_t value = 0;
};

enum class MillenniumDosVideoFunction13McgaCallbackReadWidth : std::uint8_t {
    byte = 1,
    word = 2,
};

struct MillenniumDosVideoFunction13McgaCallbackRead {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    std::uint16_t segment = 0;
    std::uint16_t offset = 0;
    std::uint16_t value = 0;
    MillenniumDosVideoFunction13McgaCallbackReadWidth width =
        MillenniumDosVideoFunction13McgaCallbackReadWidth::byte;
    constexpr bool operator==(const MillenniumDosVideoFunction13McgaCallbackRead&) const = default;
};

struct MillenniumDosVideoFunction13DriverByteEffect {
    std::uint16_t instruction_address = 0;
    std::uint16_t segment = 0;
    std::uint16_t offset = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunction13DriverByteEffect&) const = default;
};

struct MillenniumDosVideoFunction13DriverWordEffect {
    std::uint16_t instruction_address = 0;
    std::uint16_t segment = 0;
    std::uint16_t offset = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunction13DriverWordEffect&) const = default;
};

struct MillenniumDosVideoFunction13InterruptOutcome {
    std::uint16_t ax = 0;
    std::uint16_t dx = 0x03da;
    std::uint16_t di = 0x0026;
    std::uint16_t ds = 0;
    std::uint16_t return_ip = 0;
    std::uint16_t return_cs = 0;
    std::uint16_t return_flags = 0;
    std::vector<MillenniumDosVideoFunction13PortRead> port_reads;
    std::vector<MillenniumDosVideoFunction13DriverByteEffect> memory_effects;
};

// Hash-bound standalone INT 91h -> function-$13 dispatch path. The supplied
// driver image is a scenario input only; this does not establish that the
// original IVT selected it or emulate DOS, hardware ports, or the callback.
class MillenniumDosVideoFunction13InterruptSession {
public:
    MillenniumDosVideoFunction13InterruptSession(
        std::span<const std::uint8_t> english_driver,
        MillenniumDosVideoDriverKind kind,
        std::uint16_t driver_segment);

    [[nodiscard]] MillenniumDosVideoFunction13InterruptState state() const { return state_; }
    [[nodiscard]] std::uint64_t next_sequence() const { return last_sequence_ + 1; }
    [[nodiscard]] std::optional<MillenniumDosVideoFunction13InterruptBoundary> boundary() const;
    [[nodiscard]] const std::optional<MillenniumDosVideoFunction13Outcome>& retrace_outcome() const {
        return retrace_outcome_;
    }
    [[nodiscard]] std::optional<MillenniumDosVideoFunction13InterruptOutcome> outcome() const {
        return outcome_;
    }
    [[nodiscard]] const std::vector<MillenniumDosVideoFunction13McgaCallbackRead>& callback_reads() const {
        return callback_reads_;
    }
    [[nodiscard]] const std::vector<MillenniumDosVideoFunction13DriverByteEffect>&
    callback_driver_effects() const { return callback_driver_effects_; }
    [[nodiscard]] const std::vector<MillenniumDosVideoFunction13DriverWordEffect>&
    callback_driver_word_effects() const { return callback_driver_word_effects_; }

    void observe_interrupt_request(const MillenniumDosVideoFunction13InterruptRequest& request);
    void observe_port_read(const MillenniumDosVideoFunction13PortRead& read);
    void observe_mcga_postlude_byte(const MillenniumDosVideoFunction13McgaPostludeByte& read);
    void observe_mcga_callback_read(const MillenniumDosVideoFunction13McgaCallbackRead& read);
    void observe_mcga_callback_counter(const MillenniumDosVideoFunction13McgaCallbackRead& read);
    void execute_iret(std::uint64_t sequence, std::uint16_t instruction_address);

private:
    MillenniumDosVideoDriverKind kind_;
    std::uint16_t driver_segment_;
    MillenniumDosVideoFunction13Session retrace_;
    MillenniumDosVideoFunction13InterruptState state_ =
        MillenniumDosVideoFunction13InterruptState::awaiting_interrupt_request;
    std::uint64_t last_sequence_ = 0;
    std::optional<MillenniumDosVideoFunction13InterruptRequest> request_;
    std::optional<MillenniumDosVideoFunction13Outcome> retrace_outcome_;
    std::optional<MillenniumDosVideoFunction13InterruptOutcome> outcome_;
    std::vector<MillenniumDosVideoFunction13McgaCallbackRead> callback_reads_;
    std::vector<MillenniumDosVideoFunction13DriverByteEffect> callback_driver_effects_;
    std::vector<MillenniumDosVideoFunction13DriverWordEffect> callback_driver_word_effects_;
    std::uint16_t callback_next_instruction_ = 0;
};

} // namespace eon
