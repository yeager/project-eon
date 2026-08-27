#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>

namespace eon {
namespace {

constexpr std::array<std::uint32_t, 64> constants{{
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2,
}};

constexpr std::uint32_t rotate(std::uint32_t value, unsigned bits) {
    return (value >> bits) | (value << (32U - bits));
}

class Sha256 {
public:
    void update(std::span<const std::uint8_t> input) {
        bit_count_ += static_cast<std::uint64_t>(input.size()) * 8U;
        for (const auto value : input) {
            buffer_[buffer_size_++] = value;
            if (buffer_size_ == buffer_.size()) {
                transform(buffer_);
                buffer_size_ = 0;
            }
        }
    }

    Sha256Digest finish() {
        buffer_[buffer_size_++] = 0x80;
        if (buffer_size_ > 56) {
            std::fill(buffer_.begin() + static_cast<std::ptrdiff_t>(buffer_size_), buffer_.end(), 0);
            transform(buffer_);
            buffer_size_ = 0;
        }
        std::fill(buffer_.begin() + static_cast<std::ptrdiff_t>(buffer_size_), buffer_.begin() + 56, 0);
        for (unsigned index = 0; index < 8; ++index) {
            buffer_[63U - index] = static_cast<std::uint8_t>(bit_count_ >> (index * 8U));
        }
        transform(buffer_);
        Sha256Digest digest{};
        for (std::size_t index = 0; index < state_.size(); ++index) {
            digest[index * 4] = static_cast<std::uint8_t>(state_[index] >> 24U);
            digest[index * 4 + 1] = static_cast<std::uint8_t>(state_[index] >> 16U);
            digest[index * 4 + 2] = static_cast<std::uint8_t>(state_[index] >> 8U);
            digest[index * 4 + 3] = static_cast<std::uint8_t>(state_[index]);
        }
        return digest;
    }

private:
    void transform(const std::array<std::uint8_t, 64>& block) {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t index = 0; index < 16; ++index) {
            words[index] = (static_cast<std::uint32_t>(block[index * 4]) << 24U)
                | (static_cast<std::uint32_t>(block[index * 4 + 1]) << 16U)
                | (static_cast<std::uint32_t>(block[index * 4 + 2]) << 8U)
                | block[index * 4 + 3];
        }
        for (std::size_t index = 16; index < words.size(); ++index) {
            const auto s0 = rotate(words[index - 15], 7) ^ rotate(words[index - 15], 18)
                ^ (words[index - 15] >> 3U);
            const auto s1 = rotate(words[index - 2], 17) ^ rotate(words[index - 2], 19)
                ^ (words[index - 2] >> 10U);
            words[index] = words[index - 16] + s0 + words[index - 7] + s1;
        }
        auto [a,b,c,d,e,f,g,h] = state_;
        for (std::size_t index = 0; index < words.size(); ++index) {
            const auto big1 = rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25);
            const auto choose = (e & f) ^ (~e & g);
            const auto first = h + big1 + choose + constants[index] + words[index];
            const auto big0 = rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22);
            const auto majority = (a & b) ^ (a & c) ^ (b & c);
            const auto second = big0 + majority;
            h = g; g = f; f = e; e = d + first; d = c; c = b; b = a; a = first + second;
        }
        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
        state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
    }

    std::array<std::uint32_t, 8> state_{{
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19,
    }};
    std::uint64_t bit_count_ = 0;
    std::array<std::uint8_t, 64> buffer_{};
    std::size_t buffer_size_ = 0;
};

} // namespace

Sha256Digest sha256(std::span<const std::uint8_t> data) {
    Sha256 hasher;
    hasher.update(data);
    return hasher.finish();
}

Sha256Digest sha256_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Unable to open " + path.string());
    Sha256 hasher;
    std::array<std::uint8_t, 64 * 1024> buffer{};
    while (stream) {
        stream.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        const auto count = stream.gcount();
        if (count > 0) hasher.update(std::span(buffer.data(), static_cast<std::size_t>(count)));
    }
    return hasher.finish();
}

std::string to_hex(const Sha256Digest& digest) {
    constexpr char alphabet[] = "0123456789abcdef";
    std::string result(64, '0');
    for (std::size_t index = 0; index < digest.size(); ++index) {
        result[index * 2] = alphabet[digest[index] >> 4U];
        result[index * 2 + 1] = alphabet[digest[index] & 0x0fU];
    }
    return result;
}

} // namespace eon
