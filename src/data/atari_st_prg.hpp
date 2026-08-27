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

// Strictly parses a genuine Atari ST PRG image, including its compact
// relocation byte stream.  It rejects malformed offsets rather than treating
// a different file as a compatible game executable.
[[nodiscard]] AtariStPrg parse_atari_st_prg(std::span<const std::uint8_t> bytes);

} // namespace eon
