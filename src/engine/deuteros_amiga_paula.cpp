#include "engine/deuteros_amiga_paula.hpp"

#include "data/sha256.hpp"

#include <stdexcept>

namespace eon {

DeuterosAmigaPaulaMixer::DeuterosAmigaPaulaMixer(const DeuterosAmigaSoundBank& bank,
    std::uint32_t output_sample_rate)
    : bank_(bank), output_sample_rate_(output_sample_rate) {
    if (output_sample_rate_ == 0) throw std::runtime_error("Invalid Paula output sample rate");
}

bool DeuterosAmigaPaulaMixer::submit(const DeuterosAmigaSoundEvent& event) {
    // $22ab8 tests D0 as a byte and routes zero to the private $22aaa
    // descriptor instead of indexing the bundle sound table.
    if (event.sound == 0 || event.sound >= bank_.sounds.size()) return false;
    const auto& sound = bank_.sounds[event.sound];
    const auto channel_mask = static_cast<std::uint16_t>(event.channels & 0x000fU);
    for (std::size_t channel = 0; channel < channels_.size(); ++channel) {
        if ((channel_mask & (static_cast<std::uint16_t>(1U) << channel)) == 0) continue;
        // The original stages the descriptor anew for each selected channel.
        channels_[channel] = {&sound, 0, 0};
    }
    return channel_mask != 0;
}

std::vector<float> DeuterosAmigaPaulaMixer::render(std::size_t frames) {
    if (!has_active_channels()) return {};
    std::vector<float> result(frames * 2U, 0.0F);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        for (std::size_t channel = 0; channel < channels_.size(); ++channel) {
            auto& state = channels_[channel];
            if (!state.sound || state.sample_index >= state.sound->pcm.size()) continue;
            const auto encoded = state.sound->pcm[state.sample_index];
            // Paula DMA bytes are signed; spell out the two's-complement
            // interpretation rather than relying on host char signedness.
            const auto raw = encoded < 0x80U ? static_cast<std::int16_t>(encoded)
                                             : static_cast<std::int16_t>(encoded) - 256;
            const auto value = static_cast<float>(raw) / 128.0F
                * static_cast<float>(state.sound->volume) / 64.0F;
            // Amiga's conventional hardware stereo wiring: AUD0/AUD3 left,
            // AUD1/AUD2 right. The original channel mask maps directly to
            // software request bits 0..3 staged by $22ab8.
            result[frame * 2U + ((channel == 0 || channel == 3) ? 0U : 1U)] += value;

            // Keep an integer clock accumulator so the host rate cannot
            // introduce a rounding drift into Paula's AUDxPER cadence.
            state.phase += pal_sample_clock_hz;
            const auto threshold = static_cast<std::uint64_t>(state.sound->period)
                * output_sample_rate_;
            while (state.phase >= threshold && state.sample_index < state.sound->pcm.size()) {
                state.phase -= threshold;
                ++state.sample_index;
            }
        }

        // Do not turn the end of an original DMA pass into host-generated
        // silence. The sample just written is the final hardware-held PCM
        // value; subsequent frames have no active AUDx DMA and belong to the
        // unrecovered driver/service boundary.
        if (!has_active_channels()) {
            result.resize((frame + 1U) * 2U);
            break;
        }
    }
    return result;
}

bool DeuterosAmigaPaulaMixer::has_active_channels() const {
    for (const auto& state : channels_) {
        if (state.sound && state.sample_index < state.sound->pcm.size()) return true;
    }
    return false;
}

DeuterosAmigaNativeAudioMixer::DeuterosAmigaNativeAudioMixer(
    const std::uint32_t output_sample_rate):output_sample_rate_(output_sample_rate){
    if(output_sample_rate_==0)throw std::runtime_error("Invalid native audio output sample rate");
}
bool DeuterosAmigaNativeAudioMixer::install(std::vector<Voice> voices){
    voices_=std::move(voices);return audible();
}
bool DeuterosAmigaNativeAudioMixer::audible()const{
    for(const auto& voice:voices_)if(voice.index<voice.pcm.size())return true;
    return false;
}
std::vector<float> DeuterosAmigaNativeAudioMixer::render(const std::size_t frames){
    if(!audible())return {};
    std::vector<float> output(frames*2U,0.0F);
    for(std::size_t frame=0;frame<frames;++frame){
        for(auto& voice:voices_){
            if(voice.index>=voice.pcm.size())continue;
            const auto encoded=voice.pcm[voice.index];
            const auto signed_sample=encoded<0x80U?static_cast<std::int16_t>(encoded)
                :static_cast<std::int16_t>(encoded)-256;
            output[frame*2U+((voice.channel==0||voice.channel==3)?0U:1U)]+=
                static_cast<float>(signed_sample)/128.0F*static_cast<float>(voice.volume)/64.0F;
            voice.phase+=DeuterosAmigaPaulaMixer::pal_sample_clock_hz;
            const auto threshold=static_cast<std::uint64_t>(voice.period)*output_sample_rate_;
            while(voice.phase>=threshold&&voice.index<voice.pcm.size()){
                voice.phase-=threshold;++voice.index;
            }
        }
        if(!audible()){output.resize((frame+1U)*2U);break;}
    }
    return output;
}

DeuterosAmigaNativeAudioPreparation prepare_deuteros_amiga_native_audio(
    const std::array<DeuterosAmigaOwnedAudioResult,2>& results,
    const NativeRuntimeMemory& memory,const std::uint64_t generation,
    const std::uint64_t trace_sequence){
    DeuterosAmigaNativeAudioPreparation prepared;
    prepared.checkpoint.generation=generation;
    prepared.checkpoint.trace_sequence=trace_sequence;
    prepared.checkpoint.runtime_memory_checksum=memory.checkpoint().checksum;
    for(std::size_t invocation=0;invocation<results.size();++invocation){
        const auto& source=results[invocation];auto& target=prepared.checkpoint.invocations[invocation];
        target.dma_writes=source.dma_writes;
        for(std::size_t channel=0;channel<source.channel_registers.size();++channel){
            const auto& registers=source.channel_registers[channel];auto& receipt=target.channels[channel];
            receipt.channel=registers.channel;receipt.pointer=registers.pointer;
            receipt.length_words=registers.length_words;receipt.period=registers.period;
            receipt.volume=registers.volume;receipt.pointer_written=registers.pointer_written;
            receipt.length_written=registers.length_written;receipt.period_written=registers.period_written;
            receipt.volume_written=registers.volume_written;
            receipt.enabled_by_final_dma_intent=(source.dma_writes[1]&0x8000U)!=0
                &&(source.dma_writes[1]&(1U<<channel))!=0;
            if(!receipt.pointer_written||!receipt.length_written||receipt.length_words==0)continue;
            const auto byte_count=static_cast<std::size_t>(receipt.length_words)*2U;
            const auto pcm=memory.read_linear_range(receipt.pointer,byte_count);
            if(!pcm){prepared.error="Deuteros native audio range is not owned";return prepared;}
            receipt.pcm_sha256=to_hex(sha256(*pcm));
            if(!receipt.enabled_by_final_dma_intent||!receipt.period_written
                ||!receipt.volume_written||receipt.period==0||receipt.volume==0)continue;
            if(receipt.volume>64){prepared.error="Deuteros native audio volume is invalid";return prepared;}
            prepared.voices.push_back({receipt.channel,receipt.period,receipt.volume,*pcm,0,0});
        }
    }
    prepared.checkpoint.audible_voice_count=prepared.voices.size();prepared.accepted=true;
    return prepared;
}

} // namespace eon
