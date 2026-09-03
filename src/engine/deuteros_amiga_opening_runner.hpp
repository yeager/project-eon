#pragma once

#include "engine/deuteros_amiga_opening.hpp"

#include <cstdint>
#include <functional>
#include <optional>
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

    // The scheduler deliberately owns only a narrow tick callback.  The
    // native session controller supplies it, keeping the runner independent
    // of SDL and preventing this scheduling helper from gaining a mutable
    // media/session reference of its own.
    using TickSource = std::function<std::optional<DeuterosAmigaVmEvents>()>;

    DeuterosAmigaOpeningRunner(TickSource tick_source, std::uint64_t initial_tick);
    [[nodiscard]] DeuterosAmigaOpeningAdvance advance(std::uint64_t now);
    [[nodiscard]] std::uint64_t scheduled_tick() const { return scheduled_tick_; }
    [[nodiscard]] bool stopped() const { return stopped_; }

private:
    TickSource tick_source_;
    std::uint64_t scheduled_tick_;
    bool stopped_ = false;
};

} // namespace eon
