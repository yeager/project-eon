#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace eon {

// A longword in the combined TEXT+DATA image which GEMDOS asks the loader to
// adjust by its actual load base. The parser retains the value present on the
// original disk; it deliberately does not choose or apply a load address.
struct AtariStRelocation {
    std::uint32_t offset = 0;
    std::uint32_t original_value = 0;
};

// Layout recovered from an Atari ST GEMDOS executable (the 0x601a PRG
// header).  Values are retained as source-media facts; no load address or
// relocation result is invented by the parser.
struct AtariStPrg {
    std::uint32_t text_bytes = 0;
    std::uint32_t data_bytes = 0;
    std::uint32_t bss_bytes = 0;
    std::uint32_t symbol_bytes = 0;
    std::uint32_t flags = 0;
    std::uint16_t absolute_flag = 0;
    std::size_t relocation_count = 0;
    std::uint32_t first_relocation_offset = 0;
    std::uint32_t last_relocation_offset = 0;
    std::vector<AtariStRelocation> relocations;
};

// The first executable control transfer in the verified Equinox Millennium
// PRG. GEMDOS has already performed its ordinary relocation before execution;
// this retains original offsets and never selects a host load address.
struct MillenniumAtariBootstrap {
    std::uint32_t entry_offset = 0;
    std::uint32_t branch_target_offset = 0;
    std::uint32_t stage_source_offset = 0;
    std::uint32_t stage_last_longword_offset = 0;
    std::uint32_t stage_destination_offset = 0;
    std::uint32_t stage_bytes = 0;
};

// The first instruction sequence in the small bootstrap block after it has
// been copied into BSS. These are original absolute machine addresses, not
// host addresses selected by Project Eon.
struct MillenniumAtariBssEntry {
    std::uint32_t entry_offset = 0;
    std::uint32_t copy_source_address = 0;
    std::uint32_t copy_destination_address = 0;
    std::uint16_t initial_d0 = 0;
    std::uint32_t copied_words = 0;
    std::uint32_t jump_address = 0;
};

// Strictly parses a genuine Atari ST PRG image, including its compact
// relocation byte stream.  It rejects malformed offsets rather than treating
// a different file as a compatible game executable.
[[nodiscard]] AtariStPrg parse_atari_st_prg(std::span<const std::uint8_t> bytes);

// Validates the exact earliest bootstrap path in Millennium's verified Atari
// ST executable. It never emits a relocated or unpacked image.
[[nodiscard]] MillenniumAtariBootstrap parse_millennium_atari_bootstrap(
    std::span<const std::uint8_t> bytes, const AtariStPrg& prg);

// Validates the next exact BSS-resident stub reached by the verified
// bootstrap. It reports literal MOVE/DBF/JMP facts only; it never creates its
// input buffer, applies PRG relocation, or executes 68000 instructions.
[[nodiscard]] MillenniumAtariBssEntry parse_millennium_atari_bss_entry(
    std::span<const std::uint8_t> bytes, const AtariStPrg& prg,
    const MillenniumAtariBootstrap& bootstrap);

} // namespace eon
