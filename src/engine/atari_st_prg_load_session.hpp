#pragma once

#include "data/atari_st_prg.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace eon {

struct AtariStPrgRelocationEffect {
    std::uint32_t image_offset = 0;
    std::uint32_t runtime_address = 0;
    std::uint32_t source_value = 0;
    std::uint32_t relocated_value = 0;
    constexpr bool operator==(const AtariStPrgRelocationEffect&) const = default;
};

// A native, in-memory equivalent of GEMDOS's deterministic PRG image setup.
// It owns TEXT+DATA followed by zeroed BSS and applies only the relocation
// table already admitted by parse_atari_st_prg(). It does not model a
// basepage, GEMDOS, TOS, CPU state, program arguments, or execution.
struct AtariStPrgLoadCheckpoint {
    std::uint32_t load_base = 0;
    std::uint32_t entry_address = 0;
    std::uint32_t text_bytes = 0;
    std::uint32_t data_bytes = 0;
    std::uint32_t bss_bytes = 0;
    std::string source_sha256;
    std::string loadable_source_sha256;
    std::string materialized_image_sha256;
    std::vector<std::uint8_t> image;
    std::vector<AtariStPrgRelocationEffect> relocation_effects;
};

[[nodiscard]] AtariStPrgLoadCheckpoint materialize_atari_st_prg_load(
    std::span<const std::uint8_t> bytes, const AtariStPrg& prg,
    std::uint32_t load_base, std::uint32_t address_limit_exclusive = 0x01000000U);

class MillenniumAtariPrgLoadSession {
public:
    // Eon's native address map deliberately owns this address. It is not a
    // claim about the address chosen by an original TOS installation.
    static constexpr std::uint32_t native_load_base = 0x00010000U;

    explicit MillenniumAtariPrgLoadSession(std::span<const std::uint8_t> program);

    [[nodiscard]] const AtariStPrgLoadCheckpoint& checkpoint() const noexcept {
        return checkpoint_;
    }

private:
    AtariStPrgLoadCheckpoint checkpoint_;
};

} // namespace eon
