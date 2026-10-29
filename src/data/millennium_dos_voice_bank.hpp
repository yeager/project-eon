#pragma once

#include "data/creative_voice.hpp"
#include "data/millennium_dos_game_flow.hpp"
#include "data/millennium_dos_sound_driver.hpp"
#include "platform/game_data.hpp"

#include <cstddef>
#include <cstdint>
#include <array>
#include <span>
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

// Native PCM payloads for the exact VOC files that SIBM function $0b loads
// through $08b3. These are decoded in memory from VerifiedReleaseMedia; no
// DOS file handle, emulator, extracted file, or host playback policy is kept.
struct MillenniumDosSibmNativeVoice {
    std::uint8_t driver_index = 0;
    std::uint16_t original_name_address = 0;
    std::string original_filename;
    std::string sha256;
    std::uint32_t sample_rate = 0;
    std::array<std::uint8_t, 26> original_header{};
    std::array<std::uint8_t, 6> first_block_header{};
    std::vector<std::uint8_t> unsigned_pcm;
};

struct MillenniumDosSibmNativeVoiceBank {
    MillenniumDosSibmStartupProfile startup_profile;
    std::vector<MillenniumDosSibmNativeVoice> voices;
    std::size_t total_unsigned_pcm_sample_count = 0;
};

[[nodiscard]] MillenniumDosVoiceBankEvidence parse_millennium_dos_voice_bank(
    const VerifiedReleaseMedia& media);

[[nodiscard]] MillenniumDosSibmNativeVoiceBank load_millennium_dos_sibm_native_voice_bank(
    std::span<const std::uint8_t> game_executable,
    std::span<const std::uint8_t> sibm_driver,
    const VerifiedReleaseMedia& media);

} // namespace eon
