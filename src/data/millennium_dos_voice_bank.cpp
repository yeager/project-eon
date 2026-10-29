#include "data/millennium_dos_voice_bank.hpp"

#include "data/creative_voice.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace eon {

MillenniumDosVoiceBankEvidence parse_millennium_dos_voice_bank(
    const VerifiedReleaseMedia& media) {
    const auto& release = media.release();
    if (release.game != Game::millennium || release.platform != Platform::dos
        || release.language != "en") {
        throw std::runtime_error("Millennium DOS VOC bank is unavailable for this release");
    }
    constexpr std::string_view executable_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    constexpr std::array<std::string_view, 14> voice_sha256{{
        "5f796a7fe8bcf5113a65087f76853061f8d96065f9a3cbe66b6c61303b677a88",
        "7da8ec44f635e5968a7d909a63c2539991a720718ef325a65231ebbe52d3aed3",
        "95d2029cd015023057a3b911e2683b6612cf13aaad232e701dddd1713aa0126f",
        "3d814191f3d91bb7f9ba8788782c1772e4327310b057a97ee96ce8971b030a66",
        "3e87ca7997f0f8001ab575203c0878222eabfcdca7582c6fbd9c83ea3a85c11b",
        "834bc620d70aea606164c17758bb98aafc7eb43f3f12d62e5e4fea96b02771ed",
        "934d6607b2b042cad283dfd347efae1756fdeecb046a267b70ffe5f8605fd7e0",
        "073576f03b20a037469b7e40b380b9f592218db9edfd3ad5e0fa6dcc9398bf7d",
        "14561107b7d2cdb45ec3a5664654a9ba16f4b77ad821ae42d470b82f350b5e63",
        "5e3aab196c2199de9e045610cf1e5094e04619e78e2f27166d0b10a28a2b65c3",
        "e534ebc67c441b9eb6a944e8ddcb10042e22703847bc869bc2946a76f1157234",
        "a0b87f1e6e8038505cb67d3b0a730fe8e1307921ff816ebce2c92acc462fdca6",
        "004b74bfa6569f2bb86bd63f2c6e6bd97f692af8358591506c1745f174cf66f1",
        "a7f1984ff031b451262eb9f201080dd340a8fe0896f3bd57440c2e1c1c839ea4",
    }};
    const auto executable = media.borrow(executable_sha256);
    if (!executable) throw std::runtime_error("Millennium DOS VOC name source is unavailable");
    MillenniumDosVoiceBankEvidence result;
    result.name_table = parse_millennium_dos_sound_effect_name_table_evidence(*executable);
    result.voices.reserve(voice_sha256.size());
    for (std::size_t index = 0; index < voice_sha256.size(); ++index) {
        const auto bytes = media.borrow(voice_sha256[index]);
        if (!bytes) throw std::runtime_error("Millennium DOS VOC leaf is unavailable");
        const auto voice = decode_creative_voice(*bytes);
        if (voice.unsigned_pcm.size()
            > std::numeric_limits<std::size_t>::max() - result.total_unsigned_pcm_sample_count) {
            throw std::runtime_error("Millennium DOS VOC sample count overflows");
        }
        result.total_unsigned_pcm_sample_count += voice.unsigned_pcm.size();
        if (std::find(result.sample_rates.begin(), result.sample_rates.end(), voice.sample_rate)
            == result.sample_rates.end()) result.sample_rates.push_back(voice.sample_rate);
        result.voices.push_back({std::string(result.name_table.filenames[index]),
            std::string(voice_sha256[index]), voice.sample_rate, voice.unsigned_pcm.size()});
    }
    return result;
}

MillenniumDosSibmNativeVoiceBank load_millennium_dos_sibm_native_voice_bank(
    const std::span<const std::uint8_t> game_executable,
    const std::span<const std::uint8_t> sibm_driver,
    const VerifiedReleaseMedia& media) {
    constexpr std::string_view game_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    constexpr std::string_view sibm_sha256 =
        "f3224caa43c1149907f852fa98816ed68c489b70f1ba795592d684d4e51f31b1";
    const auto& release = media.release();
    if (release.game != Game::millennium || release.platform != Platform::dos
        || release.language != "en") {
        throw std::runtime_error("SIBM native voices require the exact English DOS release");
    }
    const auto owned_executable = media.borrow(game_sha256);
    const auto owned_sibm = media.borrow(sibm_sha256);
    if (!owned_executable || !owned_sibm
        || !std::equal(game_executable.begin(), game_executable.end(), owned_executable->begin(),
            owned_executable->end())
        || !std::equal(sibm_driver.begin(), sibm_driver.end(), owned_sibm->begin(),
            owned_sibm->end())) {
        throw std::runtime_error("SIBM native voice sources are detached from verified release media");
    }

    MillenniumDosSibmNativeVoiceBank result;
    result.startup_profile = parse_millennium_dos_sibm_startup_profile(
        game_executable, sibm_driver);
    const auto catalogue = parse_millennium_dos_voice_bank(media);
    if (catalogue.voices.size() != result.startup_profile.function_eleven_filenames.size()) {
        throw std::runtime_error("SIBM function-$0b media table and VOC catalogue differ in size");
    }
    result.voices.reserve(catalogue.voices.size());
    for (std::size_t index = 0; index < catalogue.voices.size(); ++index) {
        const auto& entry = catalogue.voices[index];
        if (entry.original_filename != result.startup_profile.function_eleven_filenames[index]) {
            throw std::runtime_error("SIBM function-$0b name table differs from verified VOC catalogue");
        }
        const auto bytes = media.borrow(entry.sha256);
        if (!bytes || bytes->size() < 33) {
            throw std::runtime_error("SIBM function-$0b VOC leaf is unavailable or truncated");
        }
        constexpr std::size_t header_size = 26;
        const auto data_offset = static_cast<std::size_t>(
            static_cast<std::uint16_t>((*bytes)[20])
            | (static_cast<std::uint16_t>((*bytes)[21]) << 8U));
        if (data_offset != header_size || (*bytes)[header_size] != 1) {
            throw std::runtime_error("SIBM $08b3 only admits the anchored first VOC data block");
        }
        const auto block_length = static_cast<std::size_t>((*bytes)[27])
            | (static_cast<std::size_t>((*bytes)[28]) << 8U)
            | (static_cast<std::size_t>((*bytes)[29]) << 16U);
        if (block_length < 2U || (*bytes)[29] != 0
            || block_length > bytes->size() - header_size - 5U
            || header_size + 4U + block_length + 1U != bytes->size()
            || (*bytes)[header_size + 4U + block_length] != 0
            || (*bytes)[header_size + 5U] != 0) {
            throw std::runtime_error("SIBM $08b3 VOC block length differs from its exact read sequence");
        }
        const auto decoded = decode_creative_voice(*bytes);
        const auto sample_length = block_length - 2U;
        const auto sample_offset = header_size + 6U;
        if (decoded.unsigned_pcm.size() != sample_length
            || !std::equal(decoded.unsigned_pcm.begin(), decoded.unsigned_pcm.end(),
                bytes->begin() + static_cast<std::ptrdiff_t>(sample_offset))) {
            throw std::runtime_error("SIBM $08b3 PCM payload differs from the verified native VOC decode");
        }
        if (decoded.unsigned_pcm.size()
            > std::numeric_limits<std::size_t>::max() - result.total_unsigned_pcm_sample_count) {
            throw std::runtime_error("SIBM native voice sample count overflows");
        }
        result.total_unsigned_pcm_sample_count += decoded.unsigned_pcm.size();
        std::array<std::uint8_t, 26> original_header{};
        std::array<std::uint8_t, 6> first_block_header{};
        std::copy_n(bytes->begin(), original_header.size(), original_header.begin());
        std::copy_n(bytes->begin() + static_cast<std::ptrdiff_t>(header_size),
            first_block_header.size(), first_block_header.begin());
        result.voices.push_back({
            static_cast<std::uint8_t>(index + 1U),
            result.startup_profile.function_eleven_name_addresses[index],
            entry.original_filename, entry.sha256, decoded.sample_rate,
            original_header, first_block_header, decoded.unsigned_pcm});
    }
    return result;
}

} // namespace eon
