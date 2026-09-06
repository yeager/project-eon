#pragma once

#include "data/deuteros_amiga_audio.hpp"
#include "data/deuteros_amiga_channel_vm.hpp"
#include "engine/deuteros_amiga_owned_commands.hpp"
#include "engine/native_runtime_memory.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <optional>
#include <string>

namespace eon {

// The narrow, evidence-backed part of the Deuteros $22ab8/$22bea Paula
// path. An opcode-$0b event stages software descriptors at $22a6e; the later
// $22bea consumer transfers them to AUDx registers. This candidate mixer
// starts directly from the decoded event and reproduces that first
// DMA pass exactly from the original signed 8-bit bytes.  The following
// control-word program is deliberately not guessed: $22bea advances it on a
// hardware-service cadence that has not yet been recovered.
class DeuterosAmigaPaulaMixer {
public:
    // PAL Paula clock / two. A channel advances one signed PCM byte after
    // this many colour-clock ticks, as selected by AUDxPER.
    static constexpr std::uint32_t pal_sample_clock_hz = 3'546'895;

    struct ChannelState {
        const DeuterosAmigaSound* sound = nullptr;
        std::size_t sample_index = 0;
        std::uint64_t phase = 0;
    };

    explicit DeuterosAmigaPaulaMixer(const DeuterosAmigaSoundBank& bank,
        std::uint32_t output_sample_rate = 48'000);

    // Returns false for the special sound-zero descriptor or an out-of-range
    // table index. $22ab8 handles sound zero through an internal descriptor,
    // whose bytes are not game-media PCM; it must not be invented here.
    [[nodiscard]] bool submit(const DeuterosAmigaSoundEvent& event);

    // Interleaved stereo float frames. This is a lossless numerical view of
    // original signed PCM plus the AUDxVOL scale; no generated waveform,
    // filtering, looping, clipping, or end-of-DMA padding is introduced. The
    // returned frame count may therefore be smaller than `frames`.
    [[nodiscard]] std::vector<float> render(std::size_t frames);
    [[nodiscard]] const std::array<ChannelState, 4>& channels() const { return channels_; }
    [[nodiscard]] std::uint32_t output_sample_rate() const { return output_sample_rate_; }
    [[nodiscard]] bool has_active_channels() const;

private:
    const DeuterosAmigaSoundBank& bank_;
    std::uint32_t output_sample_rate_;
    std::array<ChannelState, 4> channels_{};
};

// Metadata-only receipt for an explicitly executed native $22bea consumer.
// It records register writes and hashes, never source PCM bytes.
struct DeuterosAmigaNativeAudioChannelCheckpoint {
    std::uint8_t channel=0;
    std::uint32_t pointer=0;
    std::uint16_t length_words=0,period=0,volume=0;
    bool pointer_written=false,length_written=false,period_written=false,volume_written=false;
    bool enabled_by_final_dma_intent=false;
    std::string pcm_sha256;
};
struct DeuterosAmigaNativeAudioInvocationCheckpoint {
    std::array<std::uint16_t,2> dma_writes{};
    std::array<DeuterosAmigaNativeAudioChannelCheckpoint,4> channels{};
};
struct DeuterosAmigaNativeAudioCheckpoint {
    std::uint64_t generation=0,trace_sequence=0,runtime_memory_checksum=0;
    std::array<DeuterosAmigaNativeAudioInvocationCheckpoint,2> invocations{};
    std::size_t audible_voice_count=0;
};

// One-shot renderer for exact, already-owned signed PCM spans selected by an
// admitted native consumer. It does not model Paula state, callbacks, loops,
// modulation, interrupts, filters, or an autonomous service cadence.
class DeuterosAmigaNativeAudioMixer {
public:
    struct Voice { std::uint8_t channel=0; std::uint16_t period=0,volume=0;
        std::vector<std::uint8_t> pcm; std::size_t index=0; std::uint64_t phase=0; };
    explicit DeuterosAmigaNativeAudioMixer(std::uint32_t output_sample_rate=48'000);
    [[nodiscard]] bool install(std::vector<Voice> voices);
    [[nodiscard]] std::vector<float> render(std::size_t frames);
    [[nodiscard]] bool audible() const;
private:
    std::uint32_t output_sample_rate_;
    std::vector<Voice> voices_;
};

struct DeuterosAmigaNativeAudioPreparation {
    bool accepted=false;
    std::string error;
    DeuterosAmigaNativeAudioCheckpoint checkpoint;
    std::vector<DeuterosAmigaNativeAudioMixer::Voice> voices;
};
[[nodiscard]] DeuterosAmigaNativeAudioPreparation prepare_deuteros_amiga_native_audio(
    const std::array<DeuterosAmigaOwnedAudioResult,2>& results,
    const NativeRuntimeMemory& memory,std::uint64_t generation,std::uint64_t trace_sequence);

} // namespace eon
