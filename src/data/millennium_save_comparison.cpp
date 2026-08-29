#include "data/millennium_save_comparison.hpp"

#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace eon {
namespace {

struct KnownSave {
    MillenniumSavePlatform platform;
    std::string_view slot_name;
    std::size_t size;
    std::string_view sha256;
};

constexpr std::array<KnownSave, 5> known_saves{{
    {MillenniumSavePlatform::dos, "2200SAVE.I", 9'538,
        "a9b3d77534d3d575012f9553bfed9520edf92a83af408c977e7f0fd226a470e7"},
    {MillenniumSavePlatform::atari_st, "2200SAVE.I", 7'313,
        "b0b91572a7cc8ca0b7b112a8ce09bcf0c6645c6b32df836ae8c2eb27d86c333a"},
    {MillenniumSavePlatform::atari_st, "2200SAVE.II", 7'313,
        "fa11ee72b3ca009d8a5d6cece8ff3f95b01b29ed53106e2d3730c9a545400065"},
    {MillenniumSavePlatform::atari_st, "2200SAVE.III", 7'313,
        "54519e0eebfe3f3a38b04e4b372caf67476148c135dafbfe8d0a4bcae601eae2"},
    {MillenniumSavePlatform::atari_st, "2200SAVE.IV", 7'313,
        "8c1709bb7aba3adc2e6538867383229c4d6a285d29a78fb431970d0d926ffbd2"},
}};

const KnownSave& known_save(const MillenniumSavePlatform platform, const std::string_view slot_name) {
    for (const auto& candidate : known_saves) {
        if (candidate.platform == platform && candidate.slot_name == slot_name) return candidate;
    }
    throw std::runtime_error("Unsupported Millennium save identity");
}

} // namespace

MillenniumAuthenticatedSave authenticate_millennium_save(const MillenniumSavePlatform platform,
    const std::string_view slot_name, const std::span<const std::uint8_t> bytes) {
    const auto& expected = known_save(platform, slot_name);
    if (bytes.size() != expected.size || to_hex(sha256(bytes)) != expected.sha256) {
        throw std::runtime_error("Unsupported Millennium save bytes");
    }
    return {platform, std::string(expected.slot_name), std::string(expected.sha256),
        {bytes.begin(), bytes.end()}};
}

MillenniumSaveByteComparison compare_millennium_saves(const MillenniumAuthenticatedSave& left,
    const MillenniumAuthenticatedSave& right) {
    // Re-authenticate our retained bytes. This makes a hand-constructed
    // `MillenniumAuthenticatedSave` unable to bypass the exact media gate.
    const auto checked_left = authenticate_millennium_save(left.platform, left.slot_name, left.bytes);
    const auto checked_right = authenticate_millennium_save(right.platform, right.slot_name, right.bytes);
    const auto shared = std::min(checked_left.bytes.size(), checked_right.bytes.size());
    MillenniumSaveByteComparison result{.left_sha256 = checked_left.sha256,
        .right_sha256 = checked_right.sha256, .shared_bytes = shared,
        .left_only_bytes = checked_left.bytes.size() - shared,
        .right_only_bytes = checked_right.bytes.size() - shared};
    while (result.common_prefix_bytes < shared
        && checked_left.bytes[result.common_prefix_bytes]
            == checked_right.bytes[result.common_prefix_bytes]) {
        ++result.common_prefix_bytes;
    }
    for (std::size_t index = 0; index < shared; ++index) {
        if (checked_left.bytes[index] == checked_right.bytes[index]) ++result.equal_positions;
    }
    result.different_positions = shared - result.equal_positions;
    while (result.common_suffix_bytes < shared - result.common_prefix_bytes
        && checked_left.bytes[checked_left.bytes.size() - 1U - result.common_suffix_bytes]
            == checked_right.bytes[checked_right.bytes.size() - 1U - result.common_suffix_bytes]) {
        ++result.common_suffix_bytes;
    }
    return result;
}

} // namespace eon
