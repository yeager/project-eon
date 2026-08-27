#include "data/deuteros_amiga_channel_vm.hpp"

#include <stdexcept>

namespace eon {
namespace {

// $21612 adds this exact base before storing the result at channel-state
// offset +12. The bundle transferred by $21932 occupies this address in the
// original main-stage memory map; the VM retains only the resulting address,
// never a decoded or copied resource.
constexpr std::uint32_t channel_resource_base_address = 0x32a24;

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

std::uint16_t DeuterosAmigaRandom::next() {
    if (transfer_ != nullptr || entry_ != nullptr) {
        if (transfer_ == nullptr || entry_ == nullptr) {
            throw std::runtime_error("Incomplete Deuteros transferred-resource random source");
        }
        const auto sample = sample_deuteros_amiga_main_resource_consumer(
            *transfer_, *entry_, seed_, vblank_counter_);
        seed_ = sample.seed_after;
        return sample.addend_result;
    }
    if (disk_ == nullptr || bundle_ == nullptr) {
        throw std::runtime_error("Incomplete Deuteros raw random source");
    }
    const auto index = static_cast<std::uint16_t>(seed_ + static_cast<std::uint16_t>(vblank_counter_))
        & 0x3ffeU;
    if (static_cast<std::uint32_t>(index) + 2 > bundle_->length) {
        throw std::runtime_error("Deuteros random lookup outside bundle");
    }
    const auto bytes = disk_->bytes(bundle_->disk_offset + index, 2);
    const auto source = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(bytes[0]) << 8U) | bytes[1]);
    const auto result = static_cast<std::uint16_t>(source + 14U);
    seed_ = static_cast<std::uint16_t>(seed_ + result);
    return result;
}

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
        constexpr std::size_t maximum_commands_per_tick = 4096;
        std::size_t commands = 0;
        while (state.active) {
            if (state.wait_mode == 3) {
                if (state.timer != 0) {
                    --state.timer;
                    break;
                }
            } else if (state.wait_mode == 5) {
                if (static_cast<std::uint16_t>(inputs.audio_position - 1) != state.timer
                    || state.parameter >= inputs.audio_limit) break;
            } else if (state.wait_mode == 6) {
                auto low = static_cast<std::uint16_t>(state.mode_data);
                if ((low & 0xffU) != 0) {
                    --low;
                    state.mode_data = (state.mode_data & 0xffff0000U) | low;
                    break;
                }
                low = static_cast<std::uint16_t>(state.mode_data >> 16U);
                state.mode_data = (state.mode_data & 0xffff0000U) | low;
                state.y = static_cast<std::int16_t>(state.y + signed_word(state.parameter));
                if (state.timer != 0) {
                    --state.timer;
                    break;
                }
            } else if (state.wait_mode == 0) {
                state.active = false;
                break;
            } else if (state.wait_mode == 0x14) {
                // input_pressed represents the prior polled frame, matching
                // $2141a before the new poll at $21442.
                if (!input_gate_ || !inputs.input_pressed) break;
            } else {
                // $2140c falls through to $2142a for every unrecognised mode.
                state.active = false;
                break;
            }

            bool yielded = false;
            while (state.active && !yielded) {
                if (++commands > maximum_commands_per_tick) {
                    throw std::runtime_error("Deuteros channel command limit exceeded");
                }
                yielded = execute(state, events, inputs);
            }
            // Opcode $00 (and the verified $10 tail) returns after clearing
            // selector +6. The original program pointer remains installed
            // until the *next* scheduler pass observes selector zero and
            // clears it. Do not collapse that one-VBL intermediate state.
            if (state.wait_mode == 0) break;
            // The original RTS returns to $213ac/$213dc/$21422 and branches
            // straight back to this same scheduler in the current tick.
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
        // $214ae writes D0 (the decoded zero opcode) to state +6 and RTS.
        // It deliberately does not clear state +16 in this pass. On the next
        // scheduler visit selector zero reaches $2142a and clears that
        // program pointer. Keep the original one-VBL intermediate state.
        state.wait_mode = 0;
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
        mode_byte_ = static_cast<std::uint8_t>(command.operands[0]);
        break;
    case 0x0f:
        if (command.operands[0] >= bundle_.length) throw std::runtime_error("Deuteros alternate resource outside bundle");
        state.alternate_resource = command.operands[0];
        state.mode_data = channel_resource_base_address + command.operands[0];
        state.bitmap_selector = 0xfe;
        events.alternate_resources.push_back(command.operands[0]);
        break;
    case 0x10:
        // $2162a writes $ffff to raw main-loop cell $210f4. This event is
        // that exact request observation, not an inferred title transition.
        transition_requested_ = true;
        events.transition_requested = true;
        // $2162a sets the global request then reaches $2167a. With D0 still
        // equal to $10 it ultimately reaches $214ae, clearing selector +6;
        // the next scheduler pass clears the program pointer at +16.
        state.wait_mode = 0;
        return true;
    case 0x11: {
        auto choice = static_cast<std::uint8_t>(random_word(inputs) & 0x0fU);
        const auto threshold = static_cast<std::uint8_t>(command.operands[0]);
        if (choice >= threshold) choice = static_cast<std::uint8_t>(choice - threshold);
        const auto product = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(choice) * static_cast<std::uint16_t>(command.operands[1]));
        const auto advance = static_cast<std::int16_t>(product);
        const auto target = static_cast<std::int64_t>(state.stream_offset) + advance;
        if (target < 0 || target >= bundle_.length) {
            throw std::runtime_error("Deuteros random branch outside bundle");
        }
        state.stream_offset = static_cast<std::uint32_t>(target);
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
