#include "engine/runtime_host.hpp"

namespace eon {

void RuntimeHost::begin_source_revocation() {
    if (state() == NativeSessionState::returning_to_menu) return;
    ++generation_;
    begin_return_to_menu();
}

void RuntimeHost::finish_source_revocation() {
    finish_return_to_menu();
}

RuntimeHostAdvance RuntimeHost::advance(const std::uint64_t monotonic_tick) {
    RuntimeHostAdvance result;
    if (state() != NativeSessionState::deuteros_amiga_opening) return result;
    if (!deuteros_amiga_opening_scheduler_active()) {
        result.opening_started = start_deuteros_amiga_opening_scheduler(monotonic_tick);
    }
    if (deuteros_amiga_opening_scheduler_active()) {
        result.opening = advance_deuteros_amiga_opening_scheduler(monotonic_tick);
    }
    result.opening_active = deuteros_amiga_opening_scheduler_active();
    return result;
}

RuntimeHostSnapshot RuntimeHost::snapshot() const {
    RuntimeHostSnapshot result;
    result.generation = generation_;
    result.revoking = revoking();
    result.input_suppressed = input_suppressed_;
    result.admission = admission();
    result.rejection = rejection();
    result.state = state();
    // A revocation interval is specifically the point at which SDL releases
    // its previous-generation borrows. Do not offer an old session/value to a
    // newly scheduled UI task during that interval.
    if (result.revoking) return result;
    result.session = session_snapshot();
    if (const auto presentation = presentation_snapshot()) {
        result.presentation = {presentation->kind, presentation->boundary,
            presentation->capabilities, presentation->input_contract};
    }
    return result;
}

void RuntimeHost::set_input_suppressed(const bool suppressed) {
    if (suppressed == input_suppressed_) return;
    if (suppressed) {
        // This is a host lifecycle cancellation, not a recovered input poll.
        // It must reach the coordinator before the gate closes so a prior
        // held value cannot affect a later native opening tick.
        static_cast<void>(NativeSessionController::observe_input(
            RuntimeInputObservation::opening_input_held(false)));
    }
    input_suppressed_ = suppressed;
}

RuntimeInputDisposition RuntimeHost::observe_input(const RuntimeInputObservation& observation) {
    if (input_suppressed_) return RuntimeInputDisposition::rejected;
    return NativeSessionController::observe_input(observation);
}

bool RuntimeHost::revoking() const {
    return state() == NativeSessionState::returning_to_menu;
}

} // namespace eon
