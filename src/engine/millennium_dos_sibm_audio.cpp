#include "engine/millennium_dos_sibm_audio.hpp"

#include "data/sha256.hpp"

#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

namespace eon {
namespace {

constexpr std::string_view sibm_driver_sha256 =
    "f3224caa43c1149907f852fa98816ed68c489b70f1ba795592d684d4e51f31b1";
constexpr std::array<std::pair<std::size_t, std::size_t>, 3> function_e_spans{{
    {0x0acf, 0x0b04 - 0x0acf}, // dispatch target and loaded-voice check
    {0x0973, 0x09ae - 0x0973}, // voice-table lookup and playback setup
    {0x06fb, 0x08b3 - 0x06fb}, // original VOC sample output path
}};
constexpr std::array<std::string_view, 3> function_e_span_sha256{{
    "8c6ee76c30563e564fbe0aea485f1af5bafc7ee1345a34adf3c6b97cd3e38c76",
    "6031173a3297112f6defa3645906ef449b0d98f6fb236b33a0dce4e86cf8ec70",
    "3f3aeba729251f427893e23ba071fd4e19e4a9d61cf2cc7e0cd72764b8b7f97c",
}};

} // namespace

MillenniumDosSibmAudioPlayer::MillenniumDosSibmAudioPlayer(
    const MillenniumDosSibmNativeVoiceBank& bank,
    const MillenniumDosSibmStartupCheckpoint& startup,
    const std::span<const std::uint8_t> sibm_driver,
    const std::uint32_t output_sample_rate)
    : bank_(bank), output_sample_rate_(output_sample_rate) {
    if (output_sample_rate == 0 || bank.voices.size() != 14
        || bank.startup_profile.driver_sha256 != sibm_driver_sha256
        || sibm_driver.size() != 2'871
        || to_hex(sha256(sibm_driver)) != sibm_driver_sha256) {
        throw std::runtime_error("Unsupported Millennium SIBM function-$0e playback source");
    }
    for (std::size_t index = 0; index < function_e_spans.size(); ++index) {
        const auto [offset, size] = function_e_spans[index];
        if (offset > sibm_driver.size() || size > sibm_driver.size() - offset
            || to_hex(sha256(sibm_driver.subspan(offset, size)))
                != function_e_span_sha256[index]) {
            throw std::runtime_error("Unsupported Millennium SIBM function-$0e playback source");
        }
    }
    if (startup.state != MillenniumDosSibmStartupState::complete
        || startup.function_twelve_ax != 0 || startup.last_function_eleven_ax != 0
        || startup.loaded_voice_count != 14) {
        throw std::runtime_error("SIBM function-$0e playback requires all fourteen loaded voices");
    }
    for (std::size_t index = 0; index < bank.voices.size(); ++index) {
        const auto& voice = bank.voices[index];
        if (voice.driver_index != index + 1U || voice.sample_rate == 0
            || voice.unsigned_pcm.empty()
            || voice.original_filename != bank.startup_profile.function_eleven_filenames[index]) {
            throw std::runtime_error("SIBM function-$0e voice table is not source-ordered PCM");
        }
    }
}

MillenniumDosSibmAudioCheckpoint MillenniumDosSibmAudioPlayer::start_voice(
    const std::uint8_t driver_index) {
    // The original function stops an active voice before it checks the new
    // table's loaded flag, so an invalid replacement also silences the old one.
    active_voice_.reset();
    phase_ = 0;
    sample_cursor_ = 0;
    error_.clear();
    if (driver_index == 0 || driver_index > bank_.voices.size()) {
        error_ = "SIBM function-$0e index is outside the loaded voice table";
        return checkpoint();
    }
    const auto index = static_cast<std::size_t>(driver_index - 1U);
    if (bank_.voices[index].driver_index != driver_index) {
        error_ = "SIBM function-$0e loaded flag has no matching ordered voice";
        return checkpoint();
    }
    active_voice_ = index;
    return checkpoint();
}

MillenniumDosSibmAudioCheckpoint MillenniumDosSibmAudioPlayer::start_voice_from_dispatch(
    const MillenniumDosGameSoundDispatchResult& dispatch) {
    if (dispatch.state != MillenniumDosGameSoundDispatchState::sibm_function_0e
        || dispatch.private_function != 0x0e || dispatch.next_instruction_address != 0xcfc7
        || dispatch.voice_index > 0xffU) {
        MillenniumDosSibmAudioCheckpoint result;
        result.error = "Millennium game dispatch does not request SIBM function-$0e";
        return result;
    }
    return start_voice(static_cast<std::uint8_t>(dispatch.voice_index));
}

std::vector<float> MillenniumDosSibmAudioPlayer::render(const std::size_t frames) {
    std::vector<float> output;
    if (!active_voice_ || frames == 0) return output;
    const auto& voice = bank_.voices[*active_voice_];
    if (voice.sample_rate == 0 || output_sample_rate_ == 0) {
        active_voice_.reset();
        error_ = "SIBM PCM playback rate is zero";
        return output;
    }
    if (frames > output.max_size() / 2U) {
        throw std::length_error("SIBM audio output frame count is too large");
    }
    output.reserve(frames * 2U);
    while (output.size() / 2U < frames) {
        sample_cursor_ = static_cast<std::size_t>(phase_ / output_sample_rate_);
        if (sample_cursor_ >= voice.unsigned_pcm.size()) {
            active_voice_.reset();
            break;
        }
        const auto centered = static_cast<int>(voice.unsigned_pcm[sample_cursor_]) - 128;
        const auto sample = static_cast<float>(centered) / 128.0F;
        output.push_back(sample);
        output.push_back(sample);
        if (phase_ > std::numeric_limits<std::uint64_t>::max() - voice.sample_rate) {
            active_voice_.reset();
            error_ = "SIBM audio phase counter overflow";
            break;
        }
        phase_ += voice.sample_rate;
    }
    return output;
}

MillenniumDosSibmAudioCheckpoint MillenniumDosSibmAudioPlayer::checkpoint() const {
    MillenniumDosSibmAudioCheckpoint result;
    result.accepted = active_voice_.has_value();
    result.error = error_;
    if (active_voice_) {
        const auto& voice = bank_.voices[*active_voice_];
        result.active_driver_index = voice.driver_index;
        result.source_sample_rate = voice.sample_rate;
        result.source_sample_count = voice.unsigned_pcm.size();
        result.source_sample_cursor = sample_cursor_;
    }
    return result;
}

} // namespace eon
