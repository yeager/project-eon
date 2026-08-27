#include "engine/deuteros_amiga_paula.hpp"

#include <stdexcept>

namespace eon {

DeuterosAmigaPaulaMixer::DeuterosAmigaPaulaMixer(const DeuterosAmigaSoundBank& bank,
    std::uint32_t output_sample_rate)
    : bank_(bank), output_sample_rate_(output_sample_rate) {
    if (output_sample_rate_ == 0) throw std::runtime_error("Invalid Paula output sample rate");
}

bool DeuterosAmigaPaulaMixer::submit(const DeuterosAmigaSoundEvent& event) {
    // $22ab8 tests D0 as a byte and routes zero to the private $22aaa
    // descriptor instead of indexing the bundle sound table.
    if (event.sound == 0 || event.sound >= bank_.sounds.size()) return false;
    const auto& sound = bank_.sounds[event.sound];
    const auto channel_mask = static_cast<std::uint16_t>(event.channels & 0x000fU);
    for (std::size_t channel = 0; channel < channels_.size(); ++channel) {
        if ((channel_mask & (static_cast<std::uint16_t>(1U) << channel)) == 0) continue;
        // The original copies the descriptor anew to each selected AUDx.
        channels_[channel] = {&sound, 0, 0};
    }
    return channel_mask != 0;
}

std::vector<float> DeuterosAmigaPaulaMixer::render(std::size_t frames) {
    std::vector<float> result(frames * 2U, 0.0F);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        for (std::size_t channel = 0; channel < channels_.size(); ++channel) {
            auto& state = channels_[channel];
            if (!state.sound || state.sample_index >= state.sound->pcm.size()) continue;
            const auto encoded = state.sound->pcm[state.sample_index];
            // Paula DMA bytes are signed; spell out the two's-complement
            // interpretation rather than relying on host char signedness.
            const auto raw = encoded < 0x80U ? static_cast<std::int16_t>(encoded)
                                             : static_cast<std::int16_t>(encoded) - 256;
            const auto value = static_cast<float>(raw) / 128.0F
                * static_cast<float>(state.sound->volume) / 64.0F;
            // Amiga's conventional hardware stereo wiring: AUD0/AUD3 left,
            // AUD1/AUD2 right. The original channel mask maps directly to
            // DMAEN bits 0..3 in $22ab8.
            result[frame * 2U + ((channel == 0 || channel == 3) ? 0U : 1U)] += value;

            // Keep an integer clock accumulator so the host rate cannot
            // introduce a rounding drift into Paula's AUDxPER cadence.
            state.phase += pal_sample_clock_hz;
            const auto threshold = static_cast<std::uint64_t>(state.sound->period)
                * output_sample_rate_;
            while (state.phase >= threshold && state.sample_index < state.sound->pcm.size()) {
                state.phase -= threshold;
                ++state.sample_index;
            }
        }
    }
    return result;
}

bool DeuterosAmigaPaulaMixer::has_active_channels() const {
    for (const auto& state : channels_) {
        if (state.sound && state.sample_index < state.sound->pcm.size()) return true;
    }
    return false;
}

} // namespace eon
