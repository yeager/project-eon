#include "engine/deuteros_amiga_opening_runner.hpp"

namespace eon {

DeuterosAmigaOpeningRunner::DeuterosAmigaOpeningRunner(NativeSessionController& controller,
    const std::uint64_t initial_tick)
    : controller_(controller), scheduled_tick_(initial_tick) {}

DeuterosAmigaOpeningAdvance DeuterosAmigaOpeningRunner::advance(const std::uint64_t now) {
    DeuterosAmigaOpeningAdvance result;
    if (stopped_ || now < scheduled_tick_) return result;
    while (now - scheduled_tick_ >= scheduler_period_ms
        && result.events.size() < maximum_catch_up_ticks && !stopped_) {
        const auto events = controller_.tick_deuteros_amiga_opening();
        if (!events) {
            stopped_ = true;
            break;
        }
        result.title_handoff = result.title_handoff || events->title_handoff;
        result.events.push_back(*events);
        scheduled_tick_ += scheduler_period_ms;
        if (result.title_handoff) stopped_ = true;
    }
    if (result.events.size() == maximum_catch_up_ticks
        && now - scheduled_tick_ >= scheduler_period_ms) {
        scheduled_tick_ = now;
        result.resynchronized = true;
    }
    return result;
}

} // namespace eon
