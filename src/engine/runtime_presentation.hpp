#pragma once

#include "engine/runtime_session.hpp"

#include <optional>
#include <string_view>

namespace eon {

enum class NativeSessionState;
enum class ReleaseRuntimeAdmission;

// SDL-free description of what the native runtime is permitted to present.
// It deliberately carries no decoded pixels, audio samples, media paths,
// archive handles, or mutable input/session objects. The SDL layer can use it
// to select a view while the coordinator remains the sole owner of original
// media and recovered execution state.
enum class RuntimePresentationKind {
    millennium_dos_title,
    millennium_dos_sound_driver_boundary,
    millennium_dos_title_handoff_boundary,
    millennium_dos_gx_startup_boundary,
    millennium_dos_post_overlay_loop,
    millennium_dos_seventh_function,
    millennium_dos_sixth_function,
    millennium_dos_eighth_function,
    millennium_dos_ninth_function,
    millennium_dos_fourth_function,
    millennium_dos_fifth_function,
    millennium_dos_third_function,
    millennium_dos_first_function,
    millennium_dos_second_function,
    millennium_dos_tenth_function,
    millennium_amiga_bootstrap,
    millennium_atari_bootstrap,
    deuteros_amiga_opening,
    deuteros_amiga_title_stage_boundary,
    deuteros_amiga_title_display_trace_boundary,
    deuteros_atari_bootstrap,
};

[[nodiscard]] std::string_view runtime_presentation_kind_label(RuntimePresentationKind kind);

struct RuntimePresentationSnapshot {
    RuntimePresentationKind kind;
    NativeSessionState state;
    RuntimeSessionBoundary boundary;
    RuntimeSessionCapabilities capabilities;
    RuntimeInputContract input_contract;
    std::string_view state_label;
    std::string_view boundary_label;
};

// Convert only a live, internally consistent release-bound runtime snapshot.
// Menu, rejected, transitional, stale, or mismatched values fail closed.
[[nodiscard]] std::optional<RuntimePresentationSnapshot> runtime_presentation_for(
    NativeSessionState state, ReleaseRuntimeAdmission admission,
    const std::optional<RuntimeSessionSnapshot>& session);

} // namespace eon
