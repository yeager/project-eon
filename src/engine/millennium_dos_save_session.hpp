#pragma once

#include "data/millennium_dos_game_data.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace eon {

// A read-only view of one original English DOS save serialization.  This is
// intentionally not a save-game implementation: it has no setters, export,
// or simulation rules.  It preserves the input bytes in memory so that known
// table fields and still-opaque ranges can be inspected together.
class MillenniumDosSaveSession {
public:
    explicit MillenniumDosSaveSession(std::span<const std::uint8_t> serialized);

    [[nodiscard]] const MillenniumDosSaveLayout& layout() const { return layout_; }
    [[nodiscard]] const MillenniumDosStateTableEntry& state_record(std::size_t index) const;

    // Original file bytes, retained verbatim in memory.  No archive or save
    // file is unpacked, written, or modified by this class.
    [[nodiscard]] std::span<const std::uint8_t> serialized_bytes() const { return serialized_; }
    [[nodiscard]] std::span<const std::uint8_t> opaque_bytes(
        std::size_t offset, std::size_t length) const;
    [[nodiscard]] const std::string& sha256() const { return sha256_; }

private:
    std::vector<std::uint8_t> serialized_;
    MillenniumDosSaveLayout layout_;
    std::string sha256_;
};

} // namespace eon
