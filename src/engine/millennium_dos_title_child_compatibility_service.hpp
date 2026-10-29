#pragma once

#include "engine/millennium_dos_paragraph_arena.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace eon {

struct MillenniumDosTitleChildImageByte {
    std::uint16_t offset = 0;
    std::uint8_t value = 0;
};

struct MillenniumDosTitleChildCompatibilityCheckpoint {
    MillenniumDosParagraphAllocation allocation;
    std::size_t admitted_image_byte_count = 0;
    std::uint16_t image_load_offset = 0x0100;
    std::uint16_t entry_ip = 0x0100;
    std::uint16_t recovered_prefix_destination = 0x1b80;
    bool exact_leaf_admitted = false;
    bool psp_modeled = false;
    bool environment_modeled = false;
    bool parent_exec_return_observed = false;
};

// A deliberately narrow native replacement for the deterministic part of
// DOS child loading. It admits one exact TITLES.EXE leaf into an isolated
// compatibility-arena segment. It does not create a PSP/environment or claim
// anything about the parent's INT 21h/4b00 return.
class MillenniumDosTitleChildCompatibilityService {
public:
    MillenniumDosTitleChildCompatibilityService(
        std::span<const std::uint8_t> titles_executable,
        MillenniumDosParagraphAllocation allocation);
    [[nodiscard]] const std::vector<MillenniumDosTitleChildImageByte>&
    image_effects() const noexcept { return effects_; }
    [[nodiscard]] MillenniumDosTitleChildCompatibilityCheckpoint checkpoint() const;

    [[nodiscard]] static constexpr std::uint16_t required_paragraphs() {
        return 455; // ceil(($0100 + 7022) / 16)
    }

private:
    MillenniumDosParagraphAllocation allocation_;
    std::vector<MillenniumDosTitleChildImageByte> effects_;
};

} // namespace eon
