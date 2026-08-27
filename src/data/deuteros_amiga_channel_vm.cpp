#include "data/deuteros_amiga_channel_vm.hpp"

#include <limits>
#include <stdexcept>

namespace eon {
namespace {

std::int16_t signed_word(std::uint32_t value) {
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

std::uint16_t random_word(const DeuterosAmigaVmInputs& inputs) {
    if (!inputs.random_word) {
        throw std::runtime_error("Deuteros channel requires original-compatible random source");
    }
    return inputs.random_word();
}

} // namespace

DeuterosAmigaChannelVm::DeuterosAmigaChannelVm(
    const AmigaAdf& disk, const DeuterosAmigaBundle& bundle)
    : disk_(disk), bundle_(bundle) {
    for (const auto& source : parse_deuteros_amiga_channels(disk, bundle)) {
        channels_.push_back({
            static_cast<std::uint16_t>(source.initial_state_0 >> 16U),
            static_cast<std::int16_t>(source.initial_state_0 & 0xffffU),
            static_cast<std::int16_t>(source.initial_state_4 >> 16U),
            static_cast<std::uint16_t>(source.initial_state_4 & 0xffffU),
            source.initial_state_8, 0, 0, std::nullopt,
            source.stream_relative_offset, std::nullopt, true,
        });
    }
}

DeuterosAmigaVmEvents DeuterosAmigaChannelVm::tick(const DeuterosAmigaVmInputs& inputs) {
    DeuterosAmigaVmEvents events;
    for (auto& state : channels_) {
        if (!state.active) continue;
        if (state.wait_mode == 3) {
            if (state.timer != 0) {
                --state.timer;
                continue;
            }
        } else if (state.wait_mode == 5) {
            if (static_cast<std::uint16_t>(inputs.audio_position - 1) != state.timer
                || state.parameter >= inputs.audio_limit) continue;
        } else if (state.wait_mode == 6) {
            auto low = static_cast<std::uint16_t>(state.mode_data);
            if ((low & 0xffU) != 0) {
                --low;
                state.mode_data = (state.mode_data & 0xffff0000U) | low;
                continue;
            }
            low = static_cast<std::uint16_t>(state.mode_data >> 16U);
            state.mode_data = (state.mode_data & 0xffff0000U) | low;
            state.y = static_cast<std::int16_t>(state.y + signed_word(state.parameter));
            if (state.timer != 0) {
                --state.timer;
                continue;
            }
        } else if (state.wait_mode == 0) {
            state.active = false;
            continue;
        } else if (state.wait_mode == 0x14) {
            if (!input_gate_ || !inputs.input_pressed) continue;
        } else {
            --state.wait_mode;
            if (state.wait_mode != 0) continue;
        }

        constexpr std::size_t maximum_commands_per_tick = 4096;
        bool yielded = false;
        for (std::size_t command = 0; command < maximum_commands_per_tick && !yielded; ++command) {
            yielded = execute(state, events, inputs);
            if (!state.active) break;
            if (command + 1 == maximum_commands_per_tick) {
                throw std::runtime_error("Deuteros channel command limit exceeded");
            }
        }
    }
    return events;
}

bool DeuterosAmigaChannelVm::execute(DeuterosAmigaChannelState& state,
    DeuterosAmigaVmEvents& events, const DeuterosAmigaVmInputs& inputs) {
    const auto start = state.stream_offset;
    const auto command = decode_deuteros_amiga_channel_command(disk_, bundle_, start);
    state.stream_offset += command.encoded_size;
    switch (command.opcode) {
    case 0:
        state.wait_mode = 0;
        state.active = false;
        return true;
    case 1: state.bitmap_selector = static_cast<std::uint16_t>(command.operands[0]); break;
    case 2:
        state.x = signed_word(command.operands[0] >> 16U);
        state.y = signed_word(command.operands[0]);
        break;
    case 3:
        state.wait_mode = 3;
        state.timer = static_cast<std::uint16_t>(command.operands[0]);
        return true;
    case 4:
        palette_index_ = static_cast<std::uint16_t>(command.operands[0]);
        events.palette = palette_index_;
        break;
    case 5:
        state.wait_mode = 5;
        state.timer = static_cast<std::uint16_t>(command.operands[0] >> 16U);
        state.parameter = static_cast<std::uint16_t>(command.operands[0]);
        return true;
    case 6:
        state.wait_mode = 6;
        state.timer = static_cast<std::uint16_t>(command.operands[0] >> 16U);
        state.parameter = static_cast<std::uint16_t>(command.operands[0]);
        state.mode_data = command.operands[1];
        return true;
    case 7: state.x = static_cast<std::int16_t>(state.x + signed_word(command.operands[0])); break;
    case 8: state.y = static_cast<std::int16_t>(state.y + signed_word(command.operands[0])); break;
    case 9: {
        const auto operand_address = start + 2;
        if (command.operands[0] > operand_address) throw std::runtime_error("Deuteros channel jump before bundle");
        state.stream_offset = operand_address - command.operands[0];
        break;
    }
    case 0x0a:
        state.wait_mode = 3;
        state.timer = static_cast<std::uint16_t>(random_word(inputs) & command.operands[0]);
        return true;
    case 0x0b:
        events.sounds.push_back({static_cast<std::uint16_t>(command.operands[0]),
            static_cast<std::uint16_t>(command.operands[1])});
        break;
    case 0x0c:
        state.return_offset = state.stream_offset;
        if (command.operands[0] >= bundle_.length) throw std::runtime_error("Deuteros channel call outside bundle");
        state.stream_offset = command.operands[0];
        break;
    case 0x0d:
        if (!state.return_offset) throw std::runtime_error("Deuteros channel return without call");
        state.stream_offset = *state.return_offset;
        break;
    case 0x0e:
        // Original writes the low byte to $207ea; retain it as the bundle mode.
        break;
    case 0x0f:
        if (command.operands[0] >= bundle_.length) throw std::runtime_error("Deuteros alternate resource outside bundle");
        state.alternate_resource = command.operands[0];
        state.bitmap_selector = 0xfe;
        break;
    case 0x10:
        transition_requested_ = true;
        events.transition_requested = true;
        state.wait_mode = 0;
        state.active = false;
        return true;
    case 0x11: {
        auto choice = static_cast<std::uint16_t>(random_word(inputs) & 0x0fU);
        const auto threshold = static_cast<std::uint16_t>(command.operands[0]);
        if (choice >= threshold) choice = static_cast<std::uint16_t>(choice - threshold);
        const auto advance = static_cast<std::uint64_t>(choice) * command.operands[1];
        if (advance > std::numeric_limits<std::uint32_t>::max() - state.stream_offset
            || state.stream_offset + advance >= bundle_.length) {
            throw std::runtime_error("Deuteros random branch outside bundle");
        }
        state.stream_offset += static_cast<std::uint32_t>(advance);
        break;
    }
    case 0x12: input_gate_ = false; break;
    case 0x13: input_gate_ = true; break;
    case 0x14:
        state.wait_mode = 0x14;
        state.timer = static_cast<std::uint16_t>(command.operands[0]);
        return true;
    default: throw std::runtime_error("Unsupported Deuteros channel command");
    }
    return false;
}

} // namespace eon
