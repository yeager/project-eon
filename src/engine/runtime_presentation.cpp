#include "engine/runtime_presentation.hpp"
#include "engine/native_session_controller.hpp"

namespace eon {
namespace {

std::optional<RuntimePresentationKind> presentation_kind_for(const RuntimeSessionKind kind) {
    switch (kind) {
    case RuntimeSessionKind::millennium_dos_title:
        return RuntimePresentationKind::millennium_dos_title;
    case RuntimeSessionKind::millennium_dos_sound_driver_boundary:
        return RuntimePresentationKind::millennium_dos_sound_driver_boundary;
    case RuntimeSessionKind::millennium_dos_title_handoff_boundary:
        return RuntimePresentationKind::millennium_dos_title_handoff_boundary;
    case RuntimeSessionKind::millennium_dos_gx_startup_boundary:
        return RuntimePresentationKind::millennium_dos_gx_startup_boundary;
    case RuntimeSessionKind::millennium_dos_post_overlay_loop:
        return RuntimePresentationKind::millennium_dos_post_overlay_loop;
    case RuntimeSessionKind::millennium_dos_seventh_function:
        return RuntimePresentationKind::millennium_dos_seventh_function;
    case RuntimeSessionKind::millennium_dos_sixth_function:
        return RuntimePresentationKind::millennium_dos_sixth_function;
    case RuntimeSessionKind::millennium_dos_eighth_function:
        return RuntimePresentationKind::millennium_dos_eighth_function;
    case RuntimeSessionKind::millennium_dos_ninth_function:
        return RuntimePresentationKind::millennium_dos_ninth_function;
    case RuntimeSessionKind::millennium_dos_fourth_function:return RuntimePresentationKind::millennium_dos_fourth_function;
    case RuntimeSessionKind::millennium_dos_fifth_function:return RuntimePresentationKind::millennium_dos_fifth_function;
    case RuntimeSessionKind::millennium_dos_third_function:return RuntimePresentationKind::millennium_dos_third_function;
    case RuntimeSessionKind::millennium_dos_first_function:return RuntimePresentationKind::millennium_dos_first_function;
    case RuntimeSessionKind::millennium_dos_tenth_function:
        return RuntimePresentationKind::millennium_dos_tenth_function;
    case RuntimeSessionKind::millennium_amiga_bootstrap:
        return RuntimePresentationKind::millennium_amiga_bootstrap;
    case RuntimeSessionKind::millennium_atari_bootstrap:
        return RuntimePresentationKind::millennium_atari_bootstrap;
    case RuntimeSessionKind::deuteros_amiga_opening:
        return RuntimePresentationKind::deuteros_amiga_opening;
    case RuntimeSessionKind::deuteros_amiga_title_stage:
        return RuntimePresentationKind::deuteros_amiga_title_stage_boundary;
    case RuntimeSessionKind::deuteros_amiga_title_display_trace_boundary:
        return RuntimePresentationKind::deuteros_amiga_title_display_trace_boundary;
    case RuntimeSessionKind::deuteros_atari_bootstrap:
        return RuntimePresentationKind::deuteros_atari_bootstrap;
    }
    return std::nullopt;
}

bool state_matches_kind(const NativeSessionState state, const RuntimeSessionKind kind) {
    RuntimeSessionSnapshot snapshot;
    snapshot.kind = kind;
    return native_session_state_for(snapshot, ReleaseRuntimeAdmission::active) == state;
}

} // namespace

std::string_view runtime_presentation_kind_label(const RuntimePresentationKind kind) {
    switch (kind) {
    case RuntimePresentationKind::millennium_dos_title: return "MILLENNIUM DOS TITLE";
    case RuntimePresentationKind::millennium_dos_sound_driver_boundary:
        return "MILLENNIUM DOS SOUND DRIVER BOUNDARY";
    case RuntimePresentationKind::millennium_dos_title_handoff_boundary:
        return "MILLENNIUM DOS TITLE HANDOFF BOUNDARY";
    case RuntimePresentationKind::millennium_dos_gx_startup_boundary:
        return "MILLENNIUM DOS GX STARTUP BOUNDARY";
    case RuntimePresentationKind::millennium_dos_post_overlay_loop:
        return "MILLENNIUM DOS POST-OVERLAY LOOP";
    case RuntimePresentationKind::millennium_dos_seventh_function:
    case RuntimePresentationKind::millennium_dos_sixth_function:
    case RuntimePresentationKind::millennium_dos_eighth_function:
    case RuntimePresentationKind::millennium_dos_ninth_function:
    case RuntimePresentationKind::millennium_dos_fourth_function:
    case RuntimePresentationKind::millennium_dos_fifth_function:
    case RuntimePresentationKind::millennium_dos_third_function:
    case RuntimePresentationKind::millennium_dos_first_function:
        return "MILLENNIUM DOS SEVENTH-FUNCTION HANDLER";
    case RuntimePresentationKind::millennium_dos_tenth_function:
        return "MILLENNIUM DOS TENTH-FUNCTION HANDLER";
    case RuntimePresentationKind::millennium_amiga_bootstrap: return "MILLENNIUM AMIGA BOOTSTRAP";
    case RuntimePresentationKind::millennium_atari_bootstrap: return "MILLENNIUM ATARI ST BOOTSTRAP";
    case RuntimePresentationKind::deuteros_amiga_opening: return "DEUTEROS AMIGA OPENING";
    case RuntimePresentationKind::deuteros_amiga_title_stage_boundary:
        return "DEUTEROS AMIGA TITLE STAGE BOUNDARY";
    case RuntimePresentationKind::deuteros_amiga_title_display_trace_boundary:
        return "DEUTEROS AMIGA TITLE DISPLAY TRACE BOUNDARY";
    case RuntimePresentationKind::deuteros_atari_bootstrap: return "DEUTEROS ATARI ST BOOTSTRAP";
    }
    return "UNAVAILABLE";
}

std::optional<RuntimePresentationSnapshot> runtime_presentation_for(
    const NativeSessionState state, const ReleaseRuntimeAdmission admission,
    const std::optional<RuntimeSessionSnapshot>& session) {
    if (admission != ReleaseRuntimeAdmission::active || !session
        || !runtime_session_declaration_is_valid(session->kind, session->boundary,
            session->capabilities)
        || session->input_contract != runtime_input_contract_for_session(session->kind)
        || !state_matches_kind(state, session->kind)) {
        return std::nullopt;
    }
    const auto kind = presentation_kind_for(session->kind);
    if (!kind) return std::nullopt;
    return RuntimePresentationSnapshot{*kind, state, session->boundary, session->capabilities,
        session->input_contract, native_session_state_label(state),
        runtime_session_boundary_label(session->boundary)};
}

} // namespace eon
