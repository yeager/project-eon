#include "data/creative_voice.hpp"

#include <array>
#include <limits>
#include <stdexcept>

namespace eon {
namespace {

constexpr std::array<std::uint8_t, 20> signature{{
    'C', 'r', 'e', 'a', 't', 'i', 'v', 'e', ' ', 'V', 'o', 'i', 'c', 'e', ' ', 'F', 'i', 'l', 'e', 0x1a,
}};

std::uint16_t le16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset + 2 > bytes.size()) throw std::runtime_error("Truncated VOC 16-bit field");
    return static_cast<std::uint16_t>(bytes[offset])
        | (static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
}

std::uint32_t le24(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset + 3 > bytes.size()) throw std::runtime_error("Truncated VOC 24-bit field");
    return static_cast<std::uint32_t>(bytes[offset])
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U)
        | (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U);
}

void append(std::vector<std::uint8_t>& destination, std::span<const std::uint8_t> source) {
    if (source.size() > destination.max_size() - destination.size()) {
        throw std::runtime_error("VOC PCM output is too large");
    }
    destination.insert(destination.end(), source.begin(), source.end());
}

} // namespace

CreativeVoice decode_creative_voice(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 26 || !std::equal(signature.begin(), signature.end(), bytes.begin())) {
        throw std::runtime_error("Invalid Creative Voice File signature");
    }
    const auto data_offset = le16(bytes, 20);
    const auto version = le16(bytes, 22);
    const auto check = le16(bytes, 24);
    if (data_offset < 26 || data_offset > bytes.size() || static_cast<std::uint16_t>(version + check) != 0x1233U) {
        throw std::runtime_error("Invalid Creative Voice File header");
    }

    CreativeVoice result;
    std::size_t offset = data_offset;
    bool terminated = false;
    while (offset < bytes.size()) {
        const auto type = bytes[offset++];
        if (type == 0) {
            terminated = true;
            break;
        }
        const auto length = le24(bytes, offset);
        offset += 3;
        if (length > bytes.size() - offset) throw std::runtime_error("Truncated VOC block");
        const auto block = bytes.subspan(offset, length);
        offset += length;

        switch (type) {
        case 1: {
            if (block.size() < 2 || block[1] != 0) {
                throw std::runtime_error("Unsupported VOC sound-data encoding");
            }
            const auto divisor = 256U - static_cast<unsigned>(block[0]);
            if (divisor == 0) throw std::runtime_error("Invalid VOC time constant");
            const auto rate = 1'000'000U / divisor;
            if (result.sample_rate != 0 && result.sample_rate != rate) {
                throw std::runtime_error("VOC changes PCM sample rate mid-stream");
            }
            result.sample_rate = rate;
            append(result.unsigned_pcm, block.subspan(2));
            break;
        }
        case 2:
            if (result.sample_rate == 0) throw std::runtime_error("VOC continuation before sound data");
            append(result.unsigned_pcm, block);
            break;
        case 3: {
            if (block.size() != 3) throw std::runtime_error("Invalid VOC silence block");
            const auto divisor = 256U - static_cast<unsigned>(block[2]);
            if (divisor == 0) throw std::runtime_error("Invalid VOC silence time constant");
            const auto rate = 1'000'000U / divisor;
            if (result.sample_rate != 0 && result.sample_rate != rate) {
                throw std::runtime_error("VOC changes PCM sample rate mid-stream");
            }
            result.sample_rate = rate;
            const auto samples = static_cast<std::size_t>(le16(block, 0)) + 1;
            if (samples > result.unsigned_pcm.max_size() - result.unsigned_pcm.size()) {
                throw std::runtime_error("VOC silence output is too large");
            }
            result.unsigned_pcm.insert(result.unsigned_pcm.end(), samples, 0x80);
            break;
        }
        default:
            // Loops, markers and extensions affect playback control but not
            // byte-for-byte PCM decoding. They need explicit execution-model
            // evidence before Project Eon can claim to reproduce them.
            throw std::runtime_error("Unsupported Creative Voice File block");
        }
    }
    if (!terminated || result.sample_rate == 0 || result.unsigned_pcm.empty()) {
        throw std::runtime_error("Creative Voice File has no playable PCM data");
    }
    return result;
}

} // namespace eon
