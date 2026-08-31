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
    millennium_amiga_bootstrap,
    millennium_atari_bootstrap,
    deuteros_amiga_opening,
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

[[nodiscard]] std::string_view runtime_session_kind_label(RuntimeSessionKind kind);
[[nodiscard]] std::string_view runtime_session_boundary_label(RuntimeSessionBoundary boundary);

struct RuntimeSessionCapabilities {
    // These flags describe only an already decoded in-memory presentation or
    // audio observation. They do not permit a new media read, texture cache,
    // host audio device, input mapping, simulation transition or save write.
    bool decoded_presentation = false;
    bool audio_observations = false;
    // No current release has a cross-platform inferred input contract. Keep
    // this explicit so a caller must add evidence before forwarding input.
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
    RuntimeSessionCapabilities capabilities;
    constexpr bool operator==(const RuntimeSessionSnapshot&) const = default;
};

[[nodiscard]] RuntimeSessionSnapshot make_runtime_session_snapshot(
    const ResolvedLaunchRequest& launch, RuntimeSessionKind kind);

} // namespace eon
