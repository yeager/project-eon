#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace eon {

// Creative Voice File (VOC) audio decoded from the original DOS release.
// Samples remain the original unsigned 8-bit PCM stream; the parser does not
// resynthesise effects or substitute an arbitrary sample rate.
struct CreativeVoice {
    std::uint32_t sample_rate = 0;
    std::vector<std::uint8_t> unsigned_pcm;
};

[[nodiscard]] CreativeVoice decode_creative_voice(std::span<const std::uint8_t> bytes);

} // namespace eon
