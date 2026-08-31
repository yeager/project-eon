#include "engine/runtime_session.hpp"

namespace eon {

std::string_view runtime_session_kind_label(const RuntimeSessionKind kind) {
    switch (kind) {
    case RuntimeSessionKind::millennium_dos_title: return "MILLENNIUM DOS TITLE";
    case RuntimeSessionKind::millennium_amiga_bootstrap: return "MILLENNIUM AMIGA BOOTSTRAP";
    case RuntimeSessionKind::millennium_atari_bootstrap: return "MILLENNIUM ATARI ST BOOTSTRAP";
    case RuntimeSessionKind::deuteros_amiga_opening: return "DEUTEROS AMIGA OPENING";
    case RuntimeSessionKind::deuteros_amiga_title_stage: return "DEUTEROS AMIGA TITLE STAGE";
    case RuntimeSessionKind::deuteros_atari_bootstrap: return "DEUTEROS ATARI ST BOOTSTRAP";
    }
    return "UNKNOWN";
}

std::string_view runtime_session_boundary_label(const RuntimeSessionBoundary boundary) {
    switch (boundary) {
    case RuntimeSessionBoundary::recovered_presentation_boundary:
        return "RECOVERED PRESENTATION BOUNDARY";
    case RuntimeSessionBoundary::bootstrap_boundary: return "BOOTSTRAP BOUNDARY";
    }
    return "BOOTSTRAP BOUNDARY";
}

RuntimeSessionSnapshot make_runtime_session_snapshot(const ResolvedLaunchRequest& launch,
    const RuntimeSessionKind kind) {
    RuntimeSessionSnapshot snapshot;
    snapshot.game = launch.release.game;
    snapshot.platform = launch.release.platform;
    snapshot.language = launch.release.language;
    snapshot.release_sha256 = launch.release.sha256;
    snapshot.kind = kind;
    switch (kind) {
    case RuntimeSessionKind::millennium_dos_title:
        snapshot.boundary = RuntimeSessionBoundary::recovered_presentation_boundary;
        snapshot.capabilities.decoded_presentation = true;
        // This is not a generic controller map: the coordinator accepts only
        // the two exact observations described by RuntimeInputObservation.
        snapshot.capabilities.admitted_input = true;
        break;
    case RuntimeSessionKind::deuteros_amiga_opening:
        snapshot.boundary = RuntimeSessionBoundary::recovered_presentation_boundary;
        snapshot.capabilities.decoded_presentation = true;
        snapshot.capabilities.audio_observations = true;
        snapshot.capabilities.admitted_input = true;
        break;
    case RuntimeSessionKind::deuteros_amiga_title_stage:
        // The title handoff proves only the loaded original interval and its
        // local pre-Exec writes. Do not retain opening presentation/audio or
        // route another host input signal into the unresolved title ABI.
        snapshot.boundary = RuntimeSessionBoundary::bootstrap_boundary;
        break;
    case RuntimeSessionKind::millennium_amiga_bootstrap:
    case RuntimeSessionKind::millennium_atari_bootstrap:
    case RuntimeSessionKind::deuteros_atari_bootstrap:
        snapshot.boundary = RuntimeSessionBoundary::bootstrap_boundary;
        break;
    }
    return snapshot;
}

} // namespace eon
