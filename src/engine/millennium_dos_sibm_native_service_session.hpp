#pragma once

#include "engine/millennium_dos_native_process_admission.hpp"
#include "engine/millennium_dos_sibm_startup_session.hpp"
#include "engine/millennium_dos_sibm_timer_session.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace eon {

enum class MillenniumDosSibmNativeServiceState : std::uint8_t {
    complete,
    rejected,
};

struct MillenniumDosSibmNativeServiceCheckpoint {
    MillenniumDosSibmNativeServiceState state =
        MillenniumDosSibmNativeServiceState::rejected;
    std::uint64_t generation = 0;
    std::uint64_t source_process_generation = 0;
    MillenniumDosNativeProcessState source_process_state =
        MillenniumDosNativeProcessState::startup_sibm_timer_vector_get_boundary;
    MillenniumDosNativeBoundary source_process_boundary;
    std::uint16_t native_function_zero_ax = 0;
    bool native_timer_callback_activated = false;
    bool dos_timer_vector_result_observed = false;
    bool guest_process_advanced = false;
    bool guest_timer_vector_installed = false;
    std::uint16_t native_driver_segment = 0;
    std::uint64_t allocation_arena_generation = 0;
    std::size_t loaded_voice_count = 0;
    MillenniumDosSibmTimerCheckpoint timer;
    MillenniumDosSibmStartupCheckpoint startup;
    std::string error;
};

struct MillenniumDosSibmNativeMemoryCommitResult {
    bool accepted = false;
    std::string error;
};

[[nodiscard]] MillenniumDosSibmNativeMemoryCommitResult
commit_millennium_dos_sibm_native_memory(NativeRuntimeMemory& target,
    const MillenniumDosSibmNativeServiceCheckpoint& service,
    std::span<const std::uint8_t> source_driver);

// Native replacement for the SIBM startup after its exact AH=$35 boundary.
// It deliberately keeps the recovered guest process at that boundary while
// an Eon-owned callback and voice-loader session prepare native assets.
class MillenniumDosSibmNativeServiceSession {
public:
    MillenniumDosSibmNativeServiceSession(std::uint64_t requested_generation,
        std::uint64_t active_process_generation,
        const MillenniumDosNativeProcessCheckpoint& source_process,
        std::span<const std::uint8_t> sibm_driver,
        const MillenniumDosSibmNativeVoiceBank& bank,
        std::uint16_t native_driver_segment = 0xe000,
        std::uint16_t arena_begin_segment = 0xe100,
        std::uint16_t arena_end_segment_exclusive = 0xffff,
        std::uint64_t arena_generation = 0);

    [[nodiscard]] MillenniumDosSibmNativeServiceCheckpoint checkpoint() const;

private:
    MillenniumDosSibmNativeServiceCheckpoint checkpoint_;
    std::optional<MillenniumDosSibmTimerSession> timer_;
    std::optional<MillenniumDosSibmStartupSession> startup_;
};

} // namespace eon
