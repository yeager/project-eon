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

} // namespace eon
