#include "engine/native_session_controller.hpp"

namespace eon {

std::string_view native_session_state_label(const NativeSessionState state) {
    switch (state) {
    case NativeSessionState::menu: return "MENU";
    case NativeSessionState::admission_rejected: return "ADMISSION REJECTED";
    case NativeSessionState::millennium_dos_title: return "MILLENNIUM DOS TITLE";
    case NativeSessionState::millennium_dos_sound_driver_boundary:
        return "MILLENNIUM DOS SOUND DRIVER BOUNDARY";
    case NativeSessionState::millennium_dos_title_handoff_boundary:
        return "MILLENNIUM DOS TITLE HANDOFF BOUNDARY";
    case NativeSessionState::millennium_amiga_bootstrap: return "MILLENNIUM AMIGA BOOTSTRAP";
    case NativeSessionState::millennium_atari_bootstrap: return "MILLENNIUM ATARI ST BOOTSTRAP";
    case NativeSessionState::deuteros_amiga_opening: return "DEUTEROS AMIGA OPENING";
    case NativeSessionState::deuteros_amiga_title_stage_boundary:
        return "DEUTEROS AMIGA TITLE STAGE BOUNDARY";
    case NativeSessionState::deuteros_atari_bootstrap: return "DEUTEROS ATARI ST BOOTSTRAP";
    case NativeSessionState::returning_to_menu: return "RETURNING TO MENU";
    }
    return "ADMISSION REJECTED";
}

NativeSessionState native_session_state_for(const std::optional<RuntimeSessionSnapshot>& snapshot,
    const ReleaseRuntimeAdmission admission) {
    if (!snapshot) {
        return admission == ReleaseRuntimeAdmission::unselected
            ? NativeSessionState::menu : NativeSessionState::admission_rejected;
    }
    if (admission != ReleaseRuntimeAdmission::active) return NativeSessionState::admission_rejected;
    switch (snapshot->kind) {
    case RuntimeSessionKind::millennium_dos_title: return NativeSessionState::millennium_dos_title;
    case RuntimeSessionKind::millennium_dos_sound_driver_boundary:
        return NativeSessionState::millennium_dos_sound_driver_boundary;
    case RuntimeSessionKind::millennium_dos_title_handoff_boundary:
        return NativeSessionState::millennium_dos_title_handoff_boundary;
    case RuntimeSessionKind::millennium_amiga_bootstrap: return NativeSessionState::millennium_amiga_bootstrap;
    case RuntimeSessionKind::millennium_atari_bootstrap: return NativeSessionState::millennium_atari_bootstrap;
    case RuntimeSessionKind::deuteros_amiga_opening: return NativeSessionState::deuteros_amiga_opening;
    case RuntimeSessionKind::deuteros_amiga_title_stage:
        return NativeSessionState::deuteros_amiga_title_stage_boundary;
    case RuntimeSessionKind::deuteros_atari_bootstrap: return NativeSessionState::deuteros_atari_bootstrap;
    }
    return NativeSessionState::admission_rejected;
}

RuntimeCandidateLaunchResult NativeSessionController::launch_direct(const LaunchRequest& candidate,
    const std::vector<ReleaseArchive>& releases) {
    if (state_ == NativeSessionState::returning_to_menu) {
        return {runtime_.admission(), std::nullopt};
    }
    const auto result = runtime_.launch_direct(candidate, releases);
    synchronize_after_runtime_change();
    return result;
}

RuntimeCandidateLaunchResult NativeSessionController::launch_menu(const LauncherSessionState& session,
    const LaunchRequest& base, const std::vector<ReleaseArchive>& releases) {
    if (state_ == NativeSessionState::returning_to_menu) {
        return {runtime_.admission(), std::nullopt};
    }
    const auto result = runtime_.launch_menu(session, base, releases);
    synchronize_after_runtime_change();
    return result;
}

RuntimeInputDisposition NativeSessionController::observe_input(const RuntimeInputObservation& observation) {
    if (state_ == NativeSessionState::returning_to_menu) {
        return RuntimeInputDisposition::rejected;
    }
    const auto result = runtime_.coordinator().observe_input(observation);
    synchronize_after_runtime_change();
    return result;
}

std::optional<DeuterosAmigaVmEvents> NativeSessionController::tick_deuteros_amiga_opening() {
    if (state_ == NativeSessionState::returning_to_menu) return std::nullopt;
    const auto events = runtime_.coordinator().tick_deuteros_amiga_opening();
    synchronize_after_runtime_change();
    return events;
}

void NativeSessionController::begin_return_to_menu() {
    state_ = NativeSessionState::returning_to_menu;
}

void NativeSessionController::finish_return_to_menu() {
    if (state_ != NativeSessionState::returning_to_menu) return;
    runtime_.reset();
    state_ = NativeSessionState::menu;
}

void NativeSessionController::reset() {
    begin_return_to_menu();
    finish_return_to_menu();
}

void NativeSessionController::synchronize() {
    synchronize_after_runtime_change();
}

bool NativeSessionController::is_live() const {
    return state_ != NativeSessionState::menu && state_ != NativeSessionState::admission_rejected
        && state_ != NativeSessionState::returning_to_menu;
}

bool NativeSessionController::requires_revocation_for(const LauncherSourceIdentity& source) const {
    return runtime_.requires_revocation_for(source);
}

void NativeSessionController::synchronize_after_runtime_change() {
    if (state_ == NativeSessionState::returning_to_menu) return;
    state_ = native_session_state_for(runtime_.coordinator().session_snapshot(), runtime_.admission());
}

} // namespace eon
