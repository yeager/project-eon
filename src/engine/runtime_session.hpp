#pragma once

#include "launcher.hpp"

#include <string>
#include <string_view>

namespace eon {

// This is deliberately an envelope around an already admitted platform
// adapter, rather than a new generic emulator or game-state abstraction. It
// gives SDL, CLI diagnostics and future platform front ends one safe answer
// to what is live without exposing source paths, archive members, media bytes
// or a fabricated common input model.
enum class RuntimeSessionKind {
    millennium_dos_title,
    // The English launcher accepted one literal sound-driver selection but
    // stopped before that original driver's ABI is known.
    millennium_dos_sound_driver_boundary,
    // TITLES.EXE took its local nonzero-console path but its process/launcher
    // return and 2200ad.exe request remain unobserved.
    millennium_dos_title_handoff_boundary,
    // One exact ten-record reference trace has advanced the call-free GX
    // startup suffix through its second private-INT boundary. No interrupt,
    // local callee, device result, input, frame, or game state is modeled.
    millennium_dos_gx_startup_boundary,
    // Explicit observations advanced the recovered post-overlay loop from
    // the second INT 91h return. It remains a no-capability typed boundary.
    millennium_dos_post_overlay_loop,
    // Explicit scaled dispatch index 6 was observed resolving to $7521.
    // The typed handler is still a no-capability preservation boundary.
    millennium_dos_seventh_function,
    millennium_dos_sixth_function,
    millennium_dos_eighth_function,
    millennium_dos_ninth_function,
    millennium_dos_ninth_function_handoff,
    millennium_dos_fourth_function,
    millennium_dos_fifth_function,
    millennium_dos_third_function,
    millennium_dos_first_function,
    millennium_dos_second_function,
    millennium_dos_second_function_callback,
    // The scaled dispatcher was explicitly observed resolving index 9 to the
    // exact English $7384 handler. The handler remains observation-driven and
    // grants no host presentation, audio, or input capability.
    millennium_dos_tenth_function,
    millennium_amiga_bootstrap,
    millennium_atari_bootstrap,
    deuteros_amiga_opening,
    // The verified opening handed one exact original title stage to its first
    // unresolved Exec boundary. It is not a rendered title or input session.
    deuteros_amiga_title_stage,
    // A v4/v5 trace was revalidated at consumption time. This remains an
    // immutable provenance checkpoint with no renderer, audio or input right.
    deuteros_amiga_title_display_trace_boundary,
    deuteros_atari_bootstrap,
};

enum class RuntimeSessionBoundary {
    // A session is admitted, but only its published recovered presentation
    // observations may cross into host SDL. This is never a parity claim.
    recovered_presentation_boundary,
    // A raw/bootstrap session has no admitted host frame, input or device
    // contract. SDL must present its explicit boundary rather than infer one.
    bootstrap_boundary,
};

// Physical-key remapping is intentionally absent. These observations model
// only two independently evidenced DOS values and are rejected for every
// other active adapter: an ASCII byte for MILL.COM's literal sound chooser,
// or the nonzero availability result of TITLES.EXE's DOS console poll.
enum class RuntimeInputObservationKind { ascii_character, character_available, opening_input_held };

struct RuntimeInputObservation {
    RuntimeInputObservationKind kind = RuntimeInputObservationKind::ascii_character;
    char ascii_character = '\0';
    [[nodiscard]] static RuntimeInputObservation ascii(const char value) {
        return {RuntimeInputObservationKind::ascii_character, value};
    }
    [[nodiscard]] static RuntimeInputObservation available_character() {
        return {RuntimeInputObservationKind::character_available, '\0'};
    }
    [[nodiscard]] static RuntimeInputObservation opening_input_held(const bool held) {
        return {RuntimeInputObservationKind::opening_input_held, held ? '\1' : '\0'};
    }
};

enum class RuntimeInputDisposition { rejected, ignored, observed, boundary_reached };

// This is a declarative statement of the *currently recovered* host-input
// envelope. It is deliberately narrower than a controller map or a gameplay
// action list, so it cannot infer unknown title or gameplay controls.
enum class RuntimeInputContract {
    none,
    // MILL.COM/TITLES.EXE accept only their separately recovered source
    // observations: a literal chooser byte or a nonzero console-poll result.
    millennium_dos_startup_observation,
    // The Amiga opening consumes one physical held signal; its meaning beyond
    // the recovered finite opening is intentionally unspecified.
    deuteros_amiga_opening_held_signal,
};

[[nodiscard]] std::string_view runtime_session_kind_label(RuntimeSessionKind kind);
[[nodiscard]] std::string_view runtime_session_boundary_label(RuntimeSessionBoundary boundary);
[[nodiscard]] RuntimeInputContract runtime_input_contract_for_session(RuntimeSessionKind kind);
// Stable machine identifier for CLI/F10 diagnostics; not localized game text.
[[nodiscard]] std::string_view runtime_input_contract_identifier(RuntimeInputContract contract);
[[nodiscard]] bool runtime_input_contract_admits_host_observation(RuntimeInputContract contract);
// This gate is deliberately about observed value shape, not physical keys,
// controller bindings, or game actions. The coordinator remains responsible
// for the release-specific parser/session check after this gate succeeds.
[[nodiscard]] bool runtime_input_contract_accepts_observation(RuntimeInputContract contract,
    RuntimeInputObservationKind observation);

struct RuntimeSessionCapabilities {
    // These flags describe only an already decoded in-memory presentation or
    // audio observation. They do not permit a new media read, texture cache,
    // host audio device, input mapping, simulation transition or save write.
    bool decoded_presentation = false;
    bool audio_observations = false;
    // This must exactly match whether the immutable input_contract admits at
    // least one typed observation. It never by itself grants a controller map.
    bool admitted_input = false;
    constexpr bool operator==(const RuntimeSessionCapabilities&) const = default;
};

struct RuntimeSessionSnapshot {
    Game game = Game::millennium;
    Platform platform = Platform::dos;
    std::string language;
    std::string release_sha256;
    RuntimeSessionKind kind = RuntimeSessionKind::millennium_dos_title;
    RuntimeSessionBoundary boundary = RuntimeSessionBoundary::bootstrap_boundary;
    RuntimeInputContract input_contract = RuntimeInputContract::none;
    RuntimeSessionCapabilities capabilities;
    constexpr bool operator==(const RuntimeSessionSnapshot&) const = default;
};

// Validate the declaration that a release-capability table makes about a
// typed native session. This is independent of media and does not construct a
// session; it prevents a table row from broadening SDL-facing rights beyond
// the implementation's exact recovered envelope.
[[nodiscard]] bool runtime_session_declaration_is_valid(
    RuntimeSessionKind kind, RuntimeSessionBoundary boundary,
    RuntimeSessionCapabilities capabilities);

[[nodiscard]] RuntimeSessionSnapshot make_runtime_session_snapshot(
    const ResolvedLaunchRequest& launch, RuntimeSessionKind kind);

} // namespace eon
