#pragma once

#include "data/millennium_dos_voice_bank.hpp"
#include "engine/millennium_dos_game_sound_dispatch.hpp"
#include "engine/millennium_dos_sibm_startup_session.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace eon {

struct MillenniumDosSibmAudioCheckpoint {
    bool accepted = false;
    std::optional<std::uint8_t> active_driver_index;
    std::uint32_t source_sample_rate = 0;
    std::size_t source_sample_count = 0;
    std::size_t source_sample_cursor = 0;
    std::string error;
};

// Native host playback for the game's SIBM function $0e voice path. The
// source routine selects one loaded voice-table entry, passes its original
// VOC PCM span and sample time constant to the PC-speaker output routine, and
// replaces any active voice. Eon sends the same decoded VOC sample bytes to
// SDL's audio clock instead of reproducing the PIT-driven speaker waveform.
class MillenniumDosSibmAudioPlayer {
public:
    MillenniumDosSibmAudioPlayer(const MillenniumDosSibmNativeVoiceBank& bank,
        const MillenniumDosSibmStartupCheckpoint& startup,
        std::span<const std::uint8_t> sibm_driver,
        std::uint32_t output_sample_rate = 48'000);

    // driver_index is the original function-$0e [BP+4] table index (1..14).
    [[nodiscard]] MillenniumDosSibmAudioCheckpoint start_voice(
        std::uint8_t driver_index);
    // Consumes only a source-validated $d0c9 function-$0e route. Function-$10,
    // gated, pending, rejected, or forged-index outcomes do not start PCM.
    [[nodiscard]] MillenniumDosSibmAudioCheckpoint start_voice_from_dispatch(
        const MillenniumDosGameSoundDispatchResult& dispatch);
    // Returns interleaved stereo float frames at output_sample_rate(). The
    // result ends exactly with the original PCM sample; it does not loop or
    // invent trailing source samples.
    [[nodiscard]] std::vector<float> render(std::size_t frames);
    [[nodiscard]] MillenniumDosSibmAudioCheckpoint checkpoint() const;
    [[nodiscard]] std::uint32_t output_sample_rate() const { return output_sample_rate_; }

private:
    const MillenniumDosSibmNativeVoiceBank& bank_;
    std::uint32_t output_sample_rate_ = 0;
    std::optional<std::size_t> active_voice_;
    std::uint64_t phase_ = 0;
    std::size_t sample_cursor_ = 0;
    std::string error_;
};

} // namespace eon
