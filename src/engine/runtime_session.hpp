#pragma once

#include "launcher.hpp"

#include <cstdint>
#include <optional>
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
    // Profile five completed its split title reload and returned to the
    // owned $13000 JMP stub. No title code runs until the explicit local
    // program-entry advance validates that reconstructed runtime image.
    deuteros_amiga_title_program_entry,
    // The authentic boot-track graphics prefix completed its typed Eon
    // OpenLibrary compatibility call and stopped at AvailMem. No title stage
    // or display allocator may run until that boundary is resolved.
    deuteros_amiga_bootstrap_graphics_boundary,
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

// Host input is narrowed to exact per-session observations: two DOS startup
// values, the Amiga opening's held signal, or a typed keyboard/mouse event
// admitted only by the hash-verified Deuteros title callback contract.
enum class RuntimeInputObservationKind {
    ascii_character, character_available, opening_input_held,
    millennium_dos_console_character, millennium_dos_function_key,
    millennium_dos_console_flush,
    deuteros_amiga_raw_key_event, deuteros_amiga_raw_mouse_button_event,
    deuteros_amiga_raw_mouse_motion_event
};

// SDL text is UTF-8, while this English DOS poll consumes one byte in AL.
// Accept only a single printable ASCII byte, which has the same value in
// the DOS-compatible ASCII range; do not truncate or guess a code-page map.
[[nodiscard]] inline std::optional<std::uint8_t>
millennium_dos_ascii_console_byte(const std::string_view text) noexcept {
    if (text.size() != 1U) return std::nullopt;
    const auto byte = static_cast<unsigned char>(text.front());
    if (byte < 0x20U || byte > 0x7eU) return std::nullopt;
    return static_cast<std::uint8_t>(byte);
}

struct RuntimeInputObservation {
    RuntimeInputObservationKind kind = RuntimeInputObservationKind::ascii_character;
    char ascii_character = '\0';
    std::uint16_t raw_key_code = 0;
    std::uint16_t qualifier = 0;
    std::uint8_t raw_mouse_code = 0;
    std::uint8_t dos_scan_code = 0;
    std::int16_t raw_mouse_x = 0;
    std::int16_t raw_mouse_y = 0;
    [[nodiscard]] static RuntimeInputObservation ascii(const char value) {
        return {RuntimeInputObservationKind::ascii_character, value};
    }
    [[nodiscard]] static RuntimeInputObservation available_character() {
        return {RuntimeInputObservationKind::character_available, '\0'};
    }
    [[nodiscard]] static RuntimeInputObservation millennium_dos_character(
        const std::uint8_t value) {
        RuntimeInputObservation observation;
        observation.kind = RuntimeInputObservationKind::millennium_dos_console_character;
        observation.ascii_character = static_cast<char>(value);
        return observation;
    }
    [[nodiscard]] static RuntimeInputObservation millennium_dos_function_key(
        const std::uint8_t scan_code) {
        RuntimeInputObservation observation;
        observation.kind = RuntimeInputObservationKind::millennium_dos_function_key;
        observation.dos_scan_code = scan_code;
        return observation;
    }
    [[nodiscard]] static RuntimeInputObservation millennium_dos_console_flush() {
        RuntimeInputObservation observation;
        observation.kind = RuntimeInputObservationKind::millennium_dos_console_flush;
        return observation;
    }
    [[nodiscard]] static RuntimeInputObservation opening_input_held(const bool held) {
        return {RuntimeInputObservationKind::opening_input_held, held ? '\1' : '\0'};
    }
    [[nodiscard]] static RuntimeInputObservation deuteros_amiga_raw_key(
        const std::uint16_t code, const std::uint16_t qualifier = 0) {
        RuntimeInputObservation observation;
        observation.kind = RuntimeInputObservationKind::deuteros_amiga_raw_key_event;
        observation.raw_key_code = code;
        observation.qualifier = qualifier;
        return observation;
    }
    [[nodiscard]] static RuntimeInputObservation deuteros_amiga_raw_mouse_button(
        const std::uint8_t code, const std::uint16_t qualifier = 0) {
        RuntimeInputObservation observation;
        observation.kind = RuntimeInputObservationKind::deuteros_amiga_raw_mouse_button_event;
        observation.raw_mouse_code = code;
        observation.qualifier = qualifier;
        return observation;
    }
    [[nodiscard]] static RuntimeInputObservation deuteros_amiga_raw_mouse_motion(
        const std::int16_t x, const std::int16_t y,
        const std::uint16_t qualifier = 0) {
        RuntimeInputObservation observation;
        observation.kind = RuntimeInputObservationKind::deuteros_amiga_raw_mouse_motion_event;
        observation.qualifier = qualifier;
        observation.raw_mouse_code = 0xff;
        observation.raw_mouse_x = x;
        observation.raw_mouse_y = y;
        return observation;
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
    // Physical console bytes enter only the authenticated post-overlay
    // AH=06h poll; extended scan codes use DOS's two-read prefix sequence.
    millennium_dos_post_overlay_console_queue,
    // The Amiga opening consumes one physical held signal; its meaning beyond
    // the recovered finite opening is intentionally unspecified.
    deuteros_amiga_opening_held_signal,
    // The hash-bound English Amiga title callback consumes raw InputEvent
    // keyboard, button, and motion events through Eon's input.device compatibility service.
    deuteros_amiga_title_raw_key,
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
    // Presentation/audio flags describe decoded in-memory observations.
    // admitted_input means a typed per-session input contract exists; it does
    // not imply a broad controller map, generic simulation, or save writes.
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
