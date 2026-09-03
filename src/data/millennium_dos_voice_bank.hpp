#pragma once

#include "data/millennium_dos_game_flow.hpp"
#include "platform/game_data.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace eon {

// Read-only identity and decoded-format facts for one original VOC leaf. No
// row maps a game event to a sound or authorizes driver/host playback.
struct MillenniumDosVoiceBankEntry {
    std::string original_filename;
    std::string sha256;
    std::uint32_t sample_rate = 0;
    std::size_t unsigned_pcm_sample_count = 0;
};

// The exact SFX family named by the English 2200AD.EXE table. The bank is a
// preservation catalogue, not a mixer: it retains no PCM bytes and cannot
// select an effect, initialize a DOS driver, or emit audio.
struct MillenniumDosVoiceBankEvidence {
    MillenniumDosSoundEffectNameTableEvidence name_table;
    std::vector<MillenniumDosVoiceBankEntry> voices;
    std::size_t total_unsigned_pcm_sample_count = 0;
    std::vector<std::uint32_t> sample_rates;
};

[[nodiscard]] MillenniumDosVoiceBankEvidence parse_millennium_dos_voice_bank(
    const VerifiedReleaseMedia& media);

} // namespace eon
