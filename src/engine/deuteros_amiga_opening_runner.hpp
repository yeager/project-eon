#pragma once

#include "engine/native_session_controller.hpp"

#include <cstdint>
#include <vector>

namespace eon {

// Deterministic host scheduler for the already recovered 50 Hz Amiga opening.
// It owns no SDL object, media path, save, audio device or renderer. VM events
// remain the coordinator's exact, release-bound output for the SDL layer to
// present or mix.
struct DeuterosAmigaOpeningAdvance {
    std::vector<DeuterosAmigaVmEvents> events;
    bool resynchronized = false;
    bool title_handoff = false;
};

class DeuterosAmigaOpeningRunner {
public:
    static constexpr std::uint64_t scheduler_period_ms = 20;
    static constexpr std::size_t maximum_catch_up_ticks = 4;

    DeuterosAmigaOpeningRunner(NativeSessionController& controller, std::uint64_t initial_tick);
    [[nodiscard]] DeuterosAmigaOpeningAdvance advance(std::uint64_t now);
    [[nodiscard]] std::uint64_t scheduled_tick() const { return scheduled_tick_; }
    [[nodiscard]] bool stopped() const { return stopped_; }

private:
    NativeSessionController& controller_;
    std::uint64_t scheduled_tick_;
    bool stopped_ = false;
};

} // namespace eon
