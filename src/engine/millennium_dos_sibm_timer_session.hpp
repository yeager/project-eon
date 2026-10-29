#pragma once

#include "engine/millennium_dos_sibm_function_zero_session.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace eon {

enum class MillenniumDosSibmTimerState : std::uint8_t {
    awaiting_native_activation,
    running,
    sample_dispatch_boundary,
    rejected,
};

enum class MillenniumDosSibmTimerAction : std::uint8_t {
    completed_without_legacy_chain,
    request_host_clock_chain,
    stop_before_sample_callback,
};

struct MillenniumDosSibmTimerCheckpoint {
    MillenniumDosSibmTimerState state =
        MillenniumDosSibmTimerState::awaiting_native_activation;
    std::uint64_t callback_count = 0;
    std::uint8_t driver_flags = 0;
    std::uint16_t software_current_divisor = 0;
    std::uint16_t countdown = 0;
    std::uint16_t timer_accumulator = 0;
    std::uint16_t timer_divisor = 0;
    std::uint8_t sample_cursor_flag = 0;
    std::uint16_t next_instruction_address = 0x034d;
    std::optional<MillenniumDosSibmTimerAction> last_action;
    std::string error;
};

// Source-bounded native translation of SIBM's timer handler. It owns only
// software timer state. A caller must explicitly activate it as an Eon timer
// callback; no DOS vector, PIT, PIC, or speaker port is installed or touched.
// The recovered handler's saved INT $08 target is intentionally not guessed:
// when its 16-bit accumulator overflows, the result requests a host clock
// chain and leaves that service to the surrounding native runtime.
class MillenniumDosSibmTimerSession {
public:
    MillenniumDosSibmTimerSession(std::span<const std::uint8_t> sibm_driver,
        const MillenniumDosSibmFunctionZeroCheckpoint& function_zero);

    [[nodiscard]] MillenniumDosSibmTimerCheckpoint activate_native_callback();
    [[nodiscard]] MillenniumDosSibmTimerCheckpoint advance_callback();
    [[nodiscard]] MillenniumDosSibmTimerCheckpoint checkpoint() const;

private:
    void reject(std::string message);
    [[nodiscard]] bool advance_handler_prefix();
    void advance_timer_tail();

    MillenniumDosSibmTimerCheckpoint checkpoint_;
    std::uint16_t initial_driver_flags_ = 0;
    std::uint16_t current_divisor_ = 0;
};

} // namespace eon
