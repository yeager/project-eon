#pragma once

#include "data/millennium_dos_bitmap.hpp"
#include "data/millennium_dos_lib.hpp"
#include "data/millennium_dos_title_flow.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace eon {

// A decoded P01..P25 resource selected by TITLES.EXE's static transition
// loop. It is a patch record, not a reconstructed display frame.
struct MillenniumDosTitleTransitionPatch {
    std::uint16_t resource_index = 0;
    std::string resource_name;
    // These identify the unmodified record inside TITLE.LIB. They are
    // provenance, rather than a claim that the record is a complete frame.
    std::uint32_t source_offset = 0;
    std::uint32_t source_size = 0;
    std::string source_sha256;
    MillenniumDosBitmap bitmap;
    std::vector<std::uint8_t> mode_two_logical_to_dac;
};

struct MillenniumDosTitleTransitionSequence {
    std::uint16_t original_step_stride = 0;
    // TITLES.EXE advances through one contiguous P01..P25 record bank. Its
    // hash gives preservation tools an immutable identity without composing
    // the patches into an unproven animation.
    std::uint32_t source_bank_offset = 0;
    std::uint32_t source_bank_size = 0;
    std::string source_bank_sha256;
    std::vector<MillenniumDosTitleTransitionPatch> patches;
};

// Validates and decodes the named sequence without assembling it into a host
// animation: destination buffers, composition and cadence are unrecovered.
[[nodiscard]] MillenniumDosTitleTransitionSequence parse_millennium_dos_title_transition(
    const MillenniumDosLib& title_library, const MillenniumDosTitleFlow& flow);

} // namespace eon
