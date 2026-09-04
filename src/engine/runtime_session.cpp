#include "engine/runtime_session.hpp"

namespace eon {

std::string_view runtime_session_kind_label(const RuntimeSessionKind kind) {
    switch (kind) {
    case RuntimeSessionKind::millennium_dos_title: return "MILLENNIUM DOS TITLE";
    case RuntimeSessionKind::millennium_dos_sound_driver_boundary:
        return "MILLENNIUM DOS SOUND DRIVER BOUNDARY";
    case RuntimeSessionKind::millennium_dos_title_handoff_boundary:
        return "MILLENNIUM DOS TITLE HANDOFF BOUNDARY";
    case RuntimeSessionKind::millennium_dos_gx_startup_boundary:
        return "MILLENNIUM DOS GX STARTUP BOUNDARY";
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

RuntimeInputContract runtime_input_contract_for_session(const RuntimeSessionKind kind) {
    switch (kind) {
    case RuntimeSessionKind::millennium_dos_title:
        return RuntimeInputContract::millennium_dos_startup_observation;
    case RuntimeSessionKind::deuteros_amiga_opening:
        return RuntimeInputContract::deuteros_amiga_opening_held_signal;
    case RuntimeSessionKind::millennium_dos_sound_driver_boundary:
    case RuntimeSessionKind::millennium_dos_title_handoff_boundary:
    case RuntimeSessionKind::millennium_dos_gx_startup_boundary:
    case RuntimeSessionKind::millennium_amiga_bootstrap:
    case RuntimeSessionKind::millennium_atari_bootstrap:
    case RuntimeSessionKind::deuteros_amiga_title_stage:
    case RuntimeSessionKind::deuteros_atari_bootstrap:
        return RuntimeInputContract::none;
    }
    return RuntimeInputContract::none;
}

std::string_view runtime_input_contract_identifier(const RuntimeInputContract contract) {
    switch (contract) {
    case RuntimeInputContract::none: return "none";
    case RuntimeInputContract::millennium_dos_startup_observation:
        return "millennium-dos-startup-observation";
    case RuntimeInputContract::deuteros_amiga_opening_held_signal:
        return "deuteros-amiga-opening-held-signal";
    }
    return "none";
}

bool runtime_input_contract_admits_host_observation(const RuntimeInputContract contract) {
    return contract != RuntimeInputContract::none;
}

bool runtime_input_contract_accepts_observation(const RuntimeInputContract contract,
    const RuntimeInputObservationKind observation) {
    switch (contract) {
    case RuntimeInputContract::millennium_dos_startup_observation:
        return observation == RuntimeInputObservationKind::ascii_character
            || observation == RuntimeInputObservationKind::character_available;
    case RuntimeInputContract::deuteros_amiga_opening_held_signal:
        return observation == RuntimeInputObservationKind::opening_input_held;
    case RuntimeInputContract::none:
        return false;
    }
    return false;
}

bool runtime_session_declaration_is_valid(const RuntimeSessionKind kind,
    const RuntimeSessionBoundary boundary, const RuntimeSessionCapabilities capabilities) {
    RuntimeSessionBoundary expected_boundary = RuntimeSessionBoundary::bootstrap_boundary;
    RuntimeSessionCapabilities expected_capabilities;
    switch (kind) {
    case RuntimeSessionKind::millennium_dos_title:
        expected_boundary = RuntimeSessionBoundary::recovered_presentation_boundary;
        expected_capabilities = {true, false, true};
        break;
    case RuntimeSessionKind::deuteros_amiga_opening:
        expected_boundary = RuntimeSessionBoundary::recovered_presentation_boundary;
        expected_capabilities = {true, true, true};
        break;
    case RuntimeSessionKind::millennium_dos_sound_driver_boundary:
    case RuntimeSessionKind::millennium_dos_title_handoff_boundary:
    case RuntimeSessionKind::millennium_dos_gx_startup_boundary:
    case RuntimeSessionKind::millennium_amiga_bootstrap:
    case RuntimeSessionKind::millennium_atari_bootstrap:
    case RuntimeSessionKind::deuteros_amiga_title_stage:
    case RuntimeSessionKind::deuteros_atari_bootstrap:
        break;
    }
    return boundary == expected_boundary && capabilities == expected_capabilities
        && capabilities.admitted_input
            == runtime_input_contract_admits_host_observation(runtime_input_contract_for_session(kind));
}

RuntimeSessionSnapshot make_runtime_session_snapshot(const ResolvedLaunchRequest& launch,
    const RuntimeSessionKind kind) {
    RuntimeSessionSnapshot snapshot;
    snapshot.game = launch.release.game;
    snapshot.platform = launch.release.platform;
    snapshot.language = launch.release.language;
    snapshot.release_sha256 = launch.release.sha256;
    snapshot.kind = kind;
    snapshot.input_contract = runtime_input_contract_for_session(kind);
    switch (kind) {
    case RuntimeSessionKind::millennium_dos_title:
        snapshot.boundary = RuntimeSessionBoundary::recovered_presentation_boundary;
        snapshot.capabilities.decoded_presentation = true;
        // This is not a generic controller map: the coordinator accepts only
        // the two exact observations described by RuntimeInputObservation.
        snapshot.capabilities.admitted_input = true;
        break;
    case RuntimeSessionKind::millennium_dos_sound_driver_boundary:
    case RuntimeSessionKind::millennium_dos_title_handoff_boundary:
    case RuntimeSessionKind::millennium_dos_gx_startup_boundary:
        // Neither original transition has a recovered return/ABI contract.
        // Preserve its terminal observation without forwarding another host
        // input byte, rendering a successor screen, or changing game state.
        snapshot.boundary = RuntimeSessionBoundary::bootstrap_boundary;
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
