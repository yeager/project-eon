#pragma once

#include "data/deuteros_amiga_audio.hpp"
#include "data/deuteros_amiga_channel_vm.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace eon {

// The narrow, evidence-backed part of the Deuteros $22ab8/$22bea Paula
// path.  An opcode-$0b event copies the raw DMA address, length, period and
// volume to every selected AUDx register.  This mixer reproduces that first
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

} // namespace eon
