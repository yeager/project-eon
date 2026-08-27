#pragma once

#include "data/deuteros_amiga_bundle.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace eon {

struct DeuterosAmigaChannelState {
    std::uint16_t bitmap_selector = 0;
    std::int16_t x = 0;
    std::int16_t y = 0;
    std::uint16_t wait_mode = 0;
    std::uint16_t timer = 0;
    std::uint16_t parameter = 0;
    std::uint32_t mode_data = 0;
    std::optional<std::uint32_t> alternate_resource;
    std::uint32_t stream_offset = 0;
    std::optional<std::uint32_t> return_offset;
    bool active = true;
};

struct DeuterosAmigaSoundEvent {
    std::uint16_t sound = 0;
    std::uint16_t channels = 0;
};

struct DeuterosAmigaVmInputs {
    std::uint32_t audio_position = 0;
    std::uint16_t audio_limit = 0;
    bool input_pressed = false;
    // Mirrors $2016a. Callers can reproduce the original timing-dependent
    // source; tests may supply a fixed value when random commands are reached.
    std::function<std::uint16_t()> random_word;
};

struct DeuterosAmigaVmEvents {
    std::optional<std::uint16_t> palette;
    std::vector<DeuterosAmigaSoundEvent> sounds;
    bool transition_requested = false;
};

class DeuterosAmigaChannelVm {
public:
    DeuterosAmigaChannelVm(const AmigaAdf& disk, const DeuterosAmigaBundle& bundle);

    [[nodiscard]] const std::vector<DeuterosAmigaChannelState>& channels() const { return channels_; }
    [[nodiscard]] std::uint16_t palette_index() const { return palette_index_; }
    [[nodiscard]] bool input_gate() const { return input_gate_; }
    [[nodiscard]] std::uint8_t mode_byte() const { return mode_byte_; }
    [[nodiscard]] bool transition_requested() const { return transition_requested_; }
    [[nodiscard]] DeuterosAmigaVmEvents tick(const DeuterosAmigaVmInputs& inputs = {});

private:
    bool execute(DeuterosAmigaChannelState& state, DeuterosAmigaVmEvents& events,
        const DeuterosAmigaVmInputs& inputs);

    const AmigaAdf& disk_;
    const DeuterosAmigaBundle& bundle_;
    std::vector<DeuterosAmigaChannelState> channels_;
    std::uint16_t palette_index_ = 0;
    bool input_gate_ = false;
    std::uint8_t mode_byte_ = 0;
    bool transition_requested_ = false;
};

} // namespace eon
