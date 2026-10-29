#pragma once

#include "engine/bounded_memory_transfer.hpp"
#include "engine/deuteros_amiga_owned_commands.hpp"
#include "engine/deuteros_amiga_title_custom_chip_boundary_session.hpp"
#include "data/m68k_executor.hpp"
#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace eon {

struct DeuterosAmigaOwnedTitleCallbackResult {
    std::uint8_t event_class = 0;
    std::uint16_t raw_key_code = 0;
    std::uint16_t pending_count_before = 0;
    std::uint16_t pending_count_after = 0;
    std::optional<std::uint8_t> enqueued_byte;
    std::uint32_t stop_address = 0;
    bool move_sprite_call_reached = false;
    std::uint16_t pointer_x = 0;
    std::uint16_t pointer_y = 0;
};

// Native C++ port of the exact hash-locked callback's bounded local effects.
// It does not execute original M68k instructions, invoke input.device, or
// cross the external MoveSprite vector at $201b6.
[[nodiscard]] inline DeuterosAmigaOwnedTitleCallbackResult
execute_deuteros_amiga_owned_title_raw_key_callback(
    const std::span<const std::uint8_t> original_stage,
    const std::uint32_t stage_address,
    const std::span<std::uint8_t> runtime_stage,
    const DeuterosAmigaTitleInputEventPrefix& event,
    const std::span<const std::uint8_t> callback_code) {
    constexpr std::uint32_t callback_address = 0x1f056;
    constexpr std::uint32_t callback_return = 0x00fe6228;
    constexpr std::uint32_t queue_address = 0x1eec0;
    constexpr std::uint32_t pending_address = 0x1eed6;
    constexpr std::uint32_t latch_address = 0x1ee0c;
    constexpr std::uint32_t qualifier_address = 0x1ee0e;
    constexpr std::uint32_t mirror_address = 0x1ef2e;
    constexpr std::uint32_t source_table_address = 0x1ee20;
    constexpr std::uint32_t pointer_x_address = 0x1ffc8;
    constexpr std::uint32_t pointer_y_address = 0x1ffce;
    constexpr std::uint32_t minimum_x_address = 0x1ffca;
    constexpr std::uint32_t maximum_x_address = 0x1ffcc;
    constexpr std::uint32_t minimum_y_address = 0x1ffd0;
    constexpr std::uint32_t maximum_y_address = 0x1ffd2;
    constexpr std::uint32_t button_state_address = 0x1ffd4;
    constexpr std::uint32_t motion_gate_address = 0x1ee16;
    constexpr std::uint32_t motion_delta_x_address = 0x1ee10;
    constexpr std::uint32_t motion_delta_y_address = 0x1ee12;
    constexpr std::string_view stage_hash =
        "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03";
    constexpr std::string_view callback_hash =
        "ff4b055b2d5128465c891debcad00ff4e53cbf661de47b9ee3d6278f33d5e5f8";
    constexpr std::string_view source_table_hash =
        "2f00ffdf05ab28379e97e91e98fa764e45769d7ea55363846543becf7552e265";
    if (original_stage.empty() || original_stage.size() != runtime_stage.size()
        || to_hex(sha256(original_stage)) != stage_hash
        || to_hex(sha256(callback_code)) != callback_hash
        || callback_code.size() != 0xfaU
        || callback_address < stage_address
        || callback_address - stage_address > original_stage.size()
        || callback_code.size() > original_stage.size()-(callback_address-stage_address)
        || !std::equal(callback_code.begin(), callback_code.end(),
            original_stage.begin() + static_cast<std::ptrdiff_t>(callback_address-stage_address))
        || source_table_address < stage_address
        || source_table_address-stage_address > original_stage.size()
        || source_table_address-stage_address+0xa0U > original_stage.size()
        || to_hex(sha256(original_stage.subspan(source_table_address-stage_address,0xa0U)))
            != source_table_hash
        || (event.event_class != 1 && event.event_class != 2)
        || event.subclass != 0
        || (event.event_class == 2
            && event.code != 0x68 && event.code != 0x69
            && event.code != 0xe8 && event.code != 0xe9
            && event.code != 0x00ff)
        || queue_address < stage_address || pending_address < stage_address
        || latch_address < stage_address || qualifier_address < stage_address
        || mirror_address < stage_address || source_table_address < stage_address
        || pointer_x_address < stage_address || pointer_y_address < stage_address
        || minimum_x_address < stage_address || maximum_x_address < stage_address
        || minimum_y_address < stage_address || maximum_y_address < stage_address
        || button_state_address < stage_address || motion_gate_address < stage_address
        || motion_delta_x_address < stage_address || motion_delta_y_address < stage_address
        || queue_address-stage_address+21U > runtime_stage.size()
        || pending_address-stage_address+2U > runtime_stage.size()
        || latch_address-stage_address+2U > runtime_stage.size()
        || qualifier_address-stage_address+2U > runtime_stage.size()
        || mirror_address-stage_address+1U > runtime_stage.size()
        || pointer_x_address-stage_address+12U > runtime_stage.size()
        || pointer_y_address-stage_address+2U > runtime_stage.size()
        || minimum_x_address-stage_address+2U > runtime_stage.size()
        || maximum_x_address-stage_address+2U > runtime_stage.size()
        || minimum_y_address-stage_address+2U > runtime_stage.size()
        || maximum_y_address-stage_address+2U > runtime_stage.size()
        || button_state_address-stage_address+2U > runtime_stage.size()
        || motion_gate_address-stage_address+1U > runtime_stage.size()) {
        throw std::runtime_error("Deuteros title callback requires the exact owned stage and admitted event");
    }
    if (motion_delta_x_address-stage_address+2U > runtime_stage.size()
        || motion_delta_y_address-stage_address+2U > runtime_stage.size()) {
        throw std::runtime_error("Deuteros mouse deltas escape owned title memory");
    }
    const auto offset=[stage_address](const std::uint32_t address) {
        return static_cast<std::size_t>(address-stage_address);
    };
    const auto read_word=[&](const std::uint32_t address) {
        const auto at=offset(address);
        return static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(runtime_stage[at])<<8U)
            |runtime_stage[at+1U]);
    };
    const auto write_word=[&](const std::uint32_t address, const std::uint16_t value) {
        const auto at=offset(address);
        runtime_stage[at]=static_cast<std::uint8_t>(value>>8U);
        runtime_stage[at+1U]=static_cast<std::uint8_t>(value);
    };
    const auto signed_word=[](const std::uint16_t value) -> std::int32_t {
        return (value&0x8000U)!=0
            ? static_cast<std::int32_t>(value)-0x10000
            : static_cast<std::int32_t>(value);
    };
    const auto pending_before=read_word(pending_address);
    // The original queue has twenty usable entries. Refuse noncanonical
    // counts rather than allowing a corrupt high byte to select beyond it.
    if (event.event_class == 1 && pending_before > 0x14U) {
        throw std::runtime_error("Deuteros raw-key queue count is outside its proven range");
    }
    std::optional<std::uint8_t> queued;
    bool move_sprite_call_reached=false;
    auto stop_address=callback_return;
    std::uint16_t output_x=0,output_y=0;

    runtime_stage[offset(mirror_address)]=event.event_class;
    if(event.event_class==1) {
        write_word(qualifier_address,event.qualifier);
        const auto code=event.code;
        const auto last_code=read_word(latch_address);
        if(code!=last_code) {
            write_word(latch_address,code);
            if(code<0x50U&&pending_before<0x14U) {
                const auto table_index=static_cast<std::size_t>(code
                    +((event.qualifier&7U)!=0?0x50U:0U));
                if(table_index>=0xa0U)
                    throw std::runtime_error("Deuteros callback selector escapes original source table");
                queued=original_stage[offset(source_table_address)+table_index];
                runtime_stage[offset(queue_address)+pending_before]=*queued;
                write_word(pending_address,static_cast<std::uint16_t>(pending_before+1U));
            }
        }
    } else if(runtime_stage[offset(motion_gate_address)]!=0) {
        // The original class-two handler exits while its initialization gate is set.
    } else if(event.code==0x00ffU) {
        write_word(0x1ee10U,static_cast<std::uint16_t>(event.x));
        write_word(0x1ee12U,static_cast<std::uint16_t>(event.y));
        const auto clamp_axis=[&](const std::uint32_t coordinate_address,
            const std::uint32_t delta_address,const std::uint32_t minimum_address,
            const std::uint32_t maximum_address) {
            const auto current=read_word(coordinate_address);
            const auto delta=signed_word(read_word(delta_address));
            const auto sum=static_cast<std::uint16_t>(current
                +static_cast<std::uint16_t>(delta));
            if(delta<0) {
                if(signed_word(sum)<0 || sum<read_word(minimum_address))
                    return read_word(minimum_address);
            } else if(sum>=read_word(maximum_address)) {
                return read_word(maximum_address);
            }
            return sum;
        };
        output_x=clamp_axis(pointer_x_address,0x1ee10U,0x1ffcaU,0x1ffccU);
        output_y=clamp_axis(pointer_y_address,0x1ee12U,0x1ffd0U,0x1ffd2U);
        write_word(pointer_x_address,output_x);
        write_word(pointer_y_address,output_y);
        move_sprite_call_reached=true;
        stop_address=0x201b6;
    } else {
        const auto code=static_cast<std::uint16_t>(event.code&0x7fU);
        if(code==0x68U||code==0x69U) {
            const auto button_state=static_cast<std::uint16_t>(
                ((event.qualifier&0x4000U)!=0?1U:0U)
                |((event.qualifier&0x2000U)!=0?2U:0U));
            write_word(button_state_address,button_state);
        }
    }
    return {event.event_class,event.code,pending_before,read_word(pending_address),queued,
        stop_address,move_sprite_call_reached,output_x,output_y};
}

struct DeuterosAmigaPostTransitionHelperPrefix {
    std::array<std::uint32_t, 2> write_addresses{};
    std::array<std::uint32_t, 2> write_values{};
    std::uint32_t d0 = 0;
    std::uint32_t d1 = 0;
    std::uint32_t tail_call_address = 0;
};

// Native C++ port of the fully owned local prefix at $3f7a8. The original
// tail-branches to $3fbf8, so the returned register arguments mark that next
// boundary; this function does not execute the target or resume its caller.
[[nodiscard]] inline DeuterosAmigaPostTransitionHelperPrefix
execute_deuteros_amiga_post_transition_helper_prefix(
    const std::span<const std::uint8_t> original_stage,
    const std::uint32_t stage_address,
    const std::span<std::uint8_t> runtime_stage) {
    constexpr std::uint32_t entry_address = 0x3f7a8;
    constexpr std::uint32_t tail_call_address = 0x3fbf8;
    constexpr std::uint32_t word_address = 0x3f77e;
    constexpr std::uint32_t byte_address = 0x3f752;
    constexpr std::size_t code_size = 20;
    constexpr std::string_view stage_hash =
        "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03";
    constexpr std::string_view code_hash =
        "2ed49faf9d9a87986b075890b247f699e89a8e9e1b2970eca7d9830bd952a9ed";
    if (original_stage.empty() || original_stage.size() != runtime_stage.size()
        || to_hex(sha256(original_stage)) != stage_hash
        || entry_address < stage_address
        || entry_address-stage_address > original_stage.size()
        || code_size > original_stage.size()-(entry_address-stage_address)
        || to_hex(sha256(original_stage.subspan(entry_address-stage_address,code_size)))
            != code_hash
        || word_address < stage_address || byte_address < stage_address
        || word_address-stage_address+2U > runtime_stage.size()
        || byte_address-stage_address+1U > runtime_stage.size()) {
        throw std::runtime_error("Deuteros post-transition helper requires the exact owned title stage");
    }

    // The validated instructions are MOVEQ #0,D0; MOVE.W D0,$3f77e;
    // MOVE.B D0,$3f752; MOVEQ #$f,D1; BRA.W $3fbf8.
    runtime_stage[word_address-stage_address] = 0;
    runtime_stage[word_address-stage_address+1U] = 0;
    runtime_stage[byte_address-stage_address] = 0;
    return {{{word_address, byte_address}}, {{0, 0}}, 0, 0x0f, tail_call_address};
}

// At the initial scheduler poll the runtime records the original instruction
// in `next_instruction_address`; recurring hardware reads use the dedicated
// pending-read field. The SDL adapter still applies its recovered-address
// whitelist after selecting this candidate.
[[nodiscard]] constexpr std::uint32_t deuteros_amiga_outer_input_candidate(
    const std::uint32_t pending_read_instruction,
    const std::uint32_t next_instruction_address) noexcept {
    return pending_read_instruction != 0
        ? pending_read_instruction : next_instruction_address;
}

// Encode only the input the host owns for CIA-A: Port-1 fire is active low
// on bit 6. Other hardware lines are outside this SDL sample.
[[nodiscard]] constexpr std::uint8_t deuteros_amiga_ciaa_fire_sample(
    const bool fire_button_held) {
    return fire_button_held ? std::uint8_t{0x00} : std::uint8_t{0x40};
}

// POTINP's right-mouse button is also active low. The hardware bit is bit 10
// in the register, which the byte-wide custom-chip boundary exposes as bit 2.
[[nodiscard]] constexpr std::uint8_t deuteros_amiga_potinp_right_mouse_sample(
    const bool right_button_held) noexcept {
    return right_button_held ? std::uint8_t{0x00} : std::uint8_t{0x04};
}

struct DeuterosAmigaOwnedTitleResponseCycle {
    std::uint8_t response = 0;
    std::uint16_t pending_count_before = 0;
    std::uint16_t pending_count_after = 0;
    std::uint32_t stop_address = 0;
};

// Native C++ port of the hash-locked $1f238 response-queue helper. An empty
// queue is an external polling boundary and is never fabricated as a result.
[[nodiscard]] inline DeuterosAmigaOwnedTitleResponseCycle
consume_deuteros_amiga_owned_title_response_queue_once(
    const std::span<const std::uint8_t> original_stage,
    const std::uint32_t stage_address,
    const std::span<std::uint8_t> runtime_stage) {
    constexpr std::uint32_t response_queue = 0x1eec0;
    constexpr std::uint32_t pending_address = 0x1eed6;
    constexpr std::uint32_t helper_entry = 0x1f238;
    constexpr std::uint32_t helper_return = 0x1f258;
    constexpr std::size_t helper_size = helper_return-helper_entry;
    constexpr std::string_view stage_hash =
        "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03";
    if (original_stage.empty() || original_stage.size() != runtime_stage.size()
        || to_hex(sha256(original_stage)) != stage_hash
        || response_queue < stage_address
        || pending_address < stage_address
        || helper_entry < stage_address
        || helper_entry-stage_address > original_stage.size()
        || helper_size > original_stage.size()-(helper_entry-stage_address)
        || response_queue-stage_address+21U > runtime_stage.size()
        || pending_address-stage_address+2U > runtime_stage.size()) {
        throw std::runtime_error("Deuteros response cycle requires the exact owned title stage");
    }
    const auto offset = [stage_address](const std::uint32_t address) {
        return static_cast<std::size_t>(address - stage_address);
    };
    const auto read_word = [&](const std::uint32_t address) {
        const auto at = offset(address);
        return static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(runtime_stage[at]) << 8U)
            | runtime_stage[at + 1U]);
    };
    constexpr std::size_t shift_count=20;
    const auto pending_before = read_word(pending_address);
    if (pending_before == 0) {
        throw std::runtime_error("Deuteros response cycle has no queued event");
    }
    const auto response=runtime_stage[offset(response_queue)];
    for(std::size_t index=0;index<shift_count;++index)
        runtime_stage[offset(response_queue)+index]
            =runtime_stage[offset(response_queue)+index+1U];
    const auto pending_after=static_cast<std::uint16_t>(pending_before-1U);
    const auto at=offset(pending_address);
    runtime_stage[at]=static_cast<std::uint8_t>(pending_after>>8U);
    runtime_stage[at+1U]=static_cast<std::uint8_t>(pending_after);
    return {response,pending_before,pending_after,helper_return};
}

struct DeuterosAmigaOwnedResponseLoopBoundary {
    std::uint64_t trace_sequence = 0;
    std::string_view release_sha256;
    std::uint32_t program_counter = 0;
    std::uint32_t stack_pointer = 0;
    std::uint32_t caller_return_address = 0;
};

struct DeuterosAmigaOwnedResponseLoopStep {
    std::uint64_t trace_sequence = 0;
    std::uint8_t response = 0;
    std::uint16_t pending_count_before = 0;
    std::uint16_t pending_count_after = 0;
    std::uint16_t control_word_before = 0;
    std::uint16_t control_word_after = 0;
    bool control_word_written = false;
    std::uint32_t next_program_counter = 0;
    std::uint32_t next_stack_pointer = 0;
};

// Native equivalent of one owned call/response/tail pass. It is entered only
// from the hash-locked scheduler call at $407b4 and reads its caller return
// from the owned stack. Empty queues remain an explicit wait boundary.
[[nodiscard]] inline DeuterosAmigaOwnedResponseLoopStep
execute_deuteros_amiga_owned_title_response_loop_once(
    const std::span<const std::uint8_t> original_stage,
    const std::uint32_t stage_address,
    const std::span<std::uint8_t> runtime_stage,
    const DeuterosAmigaOwnedResponseLoopBoundary& boundary) {
    constexpr std::uint32_t call_address = 0x407b4;
    constexpr std::uint32_t helper_address = 0x1f238;
    constexpr std::size_t helper_size = 0x20;
    constexpr std::uint32_t tail_address = 0x407ba;
    constexpr std::size_t tail_size = 44;
    constexpr std::uint32_t control_address = 0x407e6;
    constexpr std::string_view release_sha256 =
        "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04";
    constexpr std::string_view stage_sha256 =
        "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03";
    constexpr std::string_view call_sha256 =
        "88e2b3531aa5cb582d1ed1a672f9a524c89cbdf572c7a7d77c8cc7f4e6db695d";
    constexpr std::string_view helper_sha256 =
        "6e6de830717a9396a0be3b17eb2f8cc50fe669491339b1600a5b64052e129387";
    constexpr std::string_view tail_sha256 =
        "b4212844a9f0fb4008caad00950e613b70581a5552cacabc253ea0966ed16df3";
    if (boundary.trace_sequence == 0 || boundary.release_sha256 != release_sha256
        || boundary.program_counter != call_address || stage_address != 0x13000
        || original_stage.size() != runtime_stage.size()
        || to_hex(sha256(original_stage)) != stage_sha256
        || tail_address < stage_address || call_address < stage_address
        || helper_address < stage_address || control_address < stage_address
        || call_address-stage_address > original_stage.size()
        || 6U > original_stage.size()-(call_address-stage_address)
        || helper_address-stage_address > original_stage.size()
        || helper_size > original_stage.size()-(helper_address-stage_address)
        || tail_address-stage_address > original_stage.size()
        || tail_size > original_stage.size()-(tail_address-stage_address)
        || control_address-stage_address+2U > runtime_stage.size()
        || boundary.stack_pointer < stage_address
        || boundary.stack_pointer-stage_address > runtime_stage.size()
        || 4U > runtime_stage.size()-(boundary.stack_pointer-stage_address)
        || boundary.caller_return_address < stage_address
        || boundary.caller_return_address >= stage_address+runtime_stage.size()
        || (boundary.caller_return_address & 1U) != 0U) {
        throw std::runtime_error("Deuteros owned response loop boundary is outside the exact title stage");
    }
    const auto code_matches = [&](const std::uint32_t address, const std::size_t length,
        const std::string_view expected_hash) {
        const auto offset = static_cast<std::size_t>(address-stage_address);
        const auto source = original_stage.subspan(offset,length);
        const auto live = std::span<const std::uint8_t>(runtime_stage).subspan(offset,length);
        return to_hex(sha256(source)) == expected_hash
            && std::equal(source.begin(),source.end(),live.begin());
    };
    if (!code_matches(call_address,6,call_sha256)
        || runtime_stage[call_address-stage_address] != 0x4e
        || runtime_stage[call_address-stage_address+1U] != 0xb9)
        throw std::runtime_error("Deuteros response loop caller differs from the admitted title image");
    if (!code_matches(helper_address,helper_size,helper_sha256)) {
        const auto offset = static_cast<std::size_t>(helper_address-stage_address);
        throw std::runtime_error("Deuteros response queue helper differs: source="
            + to_hex(sha256(original_stage.subspan(offset,helper_size)))
            + " live=" + to_hex(sha256(std::span<const std::uint8_t>(runtime_stage)
                .subspan(offset,helper_size))));
    }
    if (!code_matches(tail_address,tail_size,tail_sha256))
        throw std::runtime_error("Deuteros response loop tail differs from the admitted title image");
    const auto stack_offset = static_cast<std::size_t>(boundary.stack_pointer-stage_address);
    const auto read_word = [&](const std::uint32_t address) {
        const auto at = static_cast<std::size_t>(address-stage_address);
        return static_cast<std::uint16_t>((std::uint16_t{runtime_stage[at]} << 8U)
            | runtime_stage[at+1U]);
    };
    const auto stack_return = (std::uint32_t{runtime_stage[stack_offset]} << 24U)
        | (std::uint32_t{runtime_stage[stack_offset+1U]} << 16U)
        | (std::uint32_t{runtime_stage[stack_offset+2U]} << 8U)
        | runtime_stage[stack_offset+3U];
    if (stack_return != boundary.caller_return_address) {
        throw std::runtime_error("Deuteros response loop stack return differs from its boundary receipt");
    }
    constexpr std::uint32_t pending_address = 0x1eed6;
    if (pending_address < stage_address
        || pending_address-stage_address+2U > runtime_stage.size()) {
        throw std::runtime_error("Deuteros response queue is outside owned title memory");
    }
    const auto pending = read_word(pending_address);
    if (pending == 0 || pending > 0x14U) {
        throw std::runtime_error("Deuteros response loop requires a nonempty canonical queue");
    }

    auto pending_stage = std::vector<std::uint8_t>(runtime_stage.begin(),runtime_stage.end());
    const auto cycle = consume_deuteros_amiga_owned_title_response_queue_once(
        original_stage,stage_address,pending_stage);
    auto control_before = read_word(control_address);
    auto control_after = control_before;
    bool wrote = false;
    std::uint32_t next_pc = call_address;
    std::uint32_t next_sp = boundary.stack_pointer;
    if (cycle.response == 0x1bU) {
        next_pc = boundary.caller_return_address;
        next_sp += 4U;
    } else if (cycle.response == 0x2eU || cycle.response == 0x2cU) {
        auto low = static_cast<std::uint8_t>(control_after & 0xffU);
        low = cycle.response == 0x2eU
            ? static_cast<std::uint8_t>(low+1U)
            : static_cast<std::uint8_t>(low-1U);
        control_after = static_cast<std::uint16_t>((control_after & 0xff00U) | low);
        const auto at = static_cast<std::size_t>(control_address-stage_address);
        pending_stage[at] = static_cast<std::uint8_t>(control_after >> 8U);
        pending_stage[at+1U] = static_cast<std::uint8_t>(control_after);
        wrote = true;
    }
    std::copy(pending_stage.begin(),pending_stage.end(),runtime_stage.begin());
    return {boundary.trace_sequence,cycle.response,cycle.pending_count_before,
        cycle.pending_count_after,control_before,control_after,wrote,next_pc,next_sp};
}

struct DeuterosAmigaOuterInputRoute {
    std::uint32_t next_instruction=0;
    std::uint32_t d0=0;
    std::uint32_t caller_return=0;
    std::uint32_t fade_return=0;
    std::uint32_t library=0;
    std::uint16_t status_register=0;
    std::uint64_t instructions_executed=0;
    std::vector<m68k::BranchCheckpoint> branch_checkpoints;

    DeuterosAmigaOuterInputRoute() = default;
    DeuterosAmigaOuterInputRoute(std::uint32_t next, std::uint32_t register_zero,
        std::uint32_t caller=0, std::uint32_t fade=0, std::uint32_t library_base=0,
        std::uint16_t status=0, std::uint64_t instruction_count=0,
        std::vector<m68k::BranchCheckpoint> branches={})
        : next_instruction(next), d0(register_zero), caller_return(caller),
          fade_return(fade), library(library_base), status_register(status),
          instructions_executed(instruction_count), branch_checkpoints(std::move(branches)) {}
};

template<class Read,class Write>
DeuterosAmigaOuterInputRoute execute_deuteros_amiga_owned_outer_transition(
    std::uint32_t instruction,std::uint32_t d0,Read read,Write write);

template<class Read, class Write>
DeuterosAmigaOuterInputRoute execute_deuteros_amiga_owned_secondary_input(
    const std::span<const std::uint8_t> code, const std::uint8_t port_value,
    const std::uint32_t initial_d0, Read read, Write write) {
    constexpr std::uint32_t entry = 0x2185e;
    constexpr std::uint32_t scheduler = 0x2181c;
    constexpr std::uint32_t transition = 0x21982;
    if (code.size() != 0x34U
        || to_hex(sha256(code))
            != "82eaf3c25827471bcc4005c155722c63bd6913af294677ab592162b13904fd86")
        throw std::runtime_error("Deuteros secondary input source span is unavailable");

    std::vector<m68k::BranchCheckpoint> branches;
    std::uint64_t instructions = 0;
    std::uint32_t pc = entry;
    std::uint32_t d0 = initial_d0;
    std::uint16_t sr = 0;
    const auto set_zero_flag = [&sr](const bool zero) {
        sr = static_cast<std::uint16_t>(sr & 0xfffbU);
        if (zero) sr = static_cast<std::uint16_t>(sr | 0x0004U);
    };
    const auto set_test_flags = [&sr](const std::uint8_t value) {
        sr = static_cast<std::uint16_t>(sr & 0xfff0U);
        if ((value & 0x80U) != 0) sr = static_cast<std::uint16_t>(sr | 0x0008U);
        if (value == 0) sr = static_cast<std::uint16_t>(sr | 0x0004U);
    };
    const auto branch = [&](const std::uint32_t address, const std::uint32_t target,
                            const bool taken) {
        branches.push_back({address, sr, target, taken});
        pc = taken ? target : address + 2U;
        ++instructions;
    };

    // BTST #6,$bfe001; BNE $21870.  The only host-supplied byte is the
    // explicitly sampled CIA port value at this input boundary.
    set_zero_flag((port_value & 0x40U) == 0);
    ++instructions;
    branch(0x21866U, 0x21870U, (sr & 0x0004U) == 0);
    const bool latch_store_executed = pc != 0x21870U;
    if (latch_store_executed) {
        ++instructions;
        pc = 0x21870U;
    }

    // TST.B $2171e; BNE $21880.  Do not read the later latch/counter cells
    // on paths whose original branches skip them.
    set_test_flags(static_cast<std::uint8_t>(read(0x2171e, 1)));
    ++instructions;
    branch(0x21876U, 0x21880U, (sr & 0x0004U) == 0);
    if (pc == 0x21880U) {
        branch(0x21880U, scheduler, true); // BRA.B $2181c
    } else {
        const auto latch_value = latch_store_executed
            ? std::uint8_t{1}
            : static_cast<std::uint8_t>(read(0x21720, 1));
        set_test_flags(latch_value);
        ++instructions;
        branch(0x2187eU, 0x21882U, (sr & 0x0004U) == 0);
        if (pc == 0x21880U) {
            branch(0x21880U, scheduler, true); // BRA.B $2181c
        } else {
            d0 = (d0 & 0xffff0000U) | read(0x21696, 2); // MOVE.W counter,D0
            ++instructions;
            set_zero_flag((d0 & 1U) == 0); // BTST #0,D0
            ++instructions;
            branch(0x2188cU, scheduler, (sr & 0x0004U) == 0);
            if (pc != scheduler)
                branch(0x2188eU, transition, true); // BRA.W $21982
        }
    }
    if (pc != scheduler && pc != transition)
        throw std::runtime_error("Deuteros secondary input stopped at an unsupported instruction");

    const auto commit_input_writes = [&]() {
        write(0xbfe001, MemoryTransferElementWidth::byte, port_value);
        if (latch_store_executed)
            write(0x21720, MemoryTransferElementWidth::byte, 1);
    };

    if(pc==transition){
        auto route=execute_deuteros_amiga_owned_outer_transition(
            transition,d0,read,write);
        commit_input_writes();
        route.status_register=sr;
        route.instructions_executed=instructions;
        route.branch_checkpoints=std::move(branches);
        return route;
    }
    commit_input_writes();
    DeuterosAmigaOuterInputRoute route;
    route.next_instruction = pc;
    route.d0 = d0;
    route.status_register = sr;
    route.instructions_executed = instructions;
    route.branch_checkpoints = std::move(branches);
    return route;
}

template<class Read,class Write>
std::uint32_t restart_deuteros_amiga_owned_loop(Read read,Write write){
    using Width=MemoryTransferElementWidth;
    std::uint32_t d0=0;
    write(0x22a30,Width::byte,0);
    std::uint16_t channels=15;
    stage_deuteros_amiga_owned_sound(d0,channels,read,write);
    write(0x21720,Width::word,0);
    write(0x2171e,Width::word,0);
    write(0x210f2,Width::word,1);
    return d0;
}

// $224a2 restores the owned vector operand and emits raw audio-control
// writes; $22a5a then resets the software descriptors. These writes alone
// are not a host-audio acknowledgement or a hardware readback model.
template<class Read,class Write>
std::uint32_t execute_deuteros_amiga_owned_cleanup(Read read,Write write){
    using Width=MemoryTransferElementWidth;
    write(0x6c,Width::longword,read(0x224e6,4));
    for(std::uint32_t channel=0;channel<4;++channel)
        write(0xdff0a8+channel*16,Width::word,0);
    write(0xdff096,Width::word,15);
    write(0x22a30,Width::byte,0);
    std::uint32_t d0=0;
    std::uint16_t channels=15;
    stage_deuteros_amiga_owned_sound(d0,channels,read,write);
    return d0;
}

// Follow the local cleanup prefix, retaining the primary BSR's return site.
// Stop before fade service calls or the asynchronous counter read.
template<class Read,class Write>
DeuterosAmigaOuterInputRoute execute_deuteros_amiga_owned_outer_transition(
    std::uint32_t instruction,std::uint32_t d0,Read read,Write write){
    using Width=MemoryTransferElementWidth;
    const auto caller_return=instruction==0x21850?0x21854U:0U;
    if(instruction==0x21850)instruction=0x218cc;
    if(instruction==0x21982){
        d0=(d0&0xffff0000U)|read(0x21704,2);
        const auto selector=d0&0xffU; // Original CMP.B, not CMP.W.
        if(selector>2)return {0x21380,d0,0};
        if(selector<2)write(0x21704,Width::word,1);
        instruction=0x218cc;
    }
    if(instruction!=0x21892&&instruction!=0x218cc)
        throw std::runtime_error("Unsupported Deuteros outer transition instruction");
    const bool restart=instruction==0x21892;
    d0=read(0x2126a,4);
    if(d0!=0){
        write(0x2229a,Width::word,0x100);
        write(0x207ea,Width::byte,0);
        return {0x222ac,d0,caller_return,restart?0x2189eU:0x218d8U};
    }
    // $22a5a tail-calls the existing native $22ab8 software staging routine.
    d0=0;
    write(0x22a30,Width::byte,0);
    std::uint16_t channels=15;
    stage_deuteros_amiga_owned_sound(d0,channels,read,write);
    return {restart?0x218a8U:0x218e2U,d0,caller_return};
}

// The caller validates the ordered port observation before publishing this
// private transaction. Values are raw port bytes, not host button meanings.
template<class Read,class Write>
DeuterosAmigaOuterInputRoute execute_deuteros_amiga_owned_outer_input(
    std::uint32_t instruction,std::uint8_t value,std::uint32_t d0,Read read,Write write){
    using Width=MemoryTransferElementWidth;
    if(instruction==0x217e4){
        write(0xbfe001,Width::byte,value|2U);
        d0=(d0&0xffff0000U)|read(0x21704,2);
        return {0x21926,d0,0x217f6};
    }
    if(instruction==0x218be){
        write(0xbfe001,Width::byte,value);
        if((value&0x40U)==0)return {0x218be,d0};
        // $217f6 re-enters the loop without reloading the original resource.
        d0=restart_deuteros_amiga_owned_loop(read,write);
        return {0x21816,d0};
    }
    if(instruction==0x222ac){
        write(0xdff01f,Width::byte,value);
        if((value&0x20U)==0)return {0x222ac,d0};
        for(std::uint32_t index=0;index<16;++index){
            auto color=read(0x12ecc+index*2,2);
            if(color>=0x100U)color-=0x100U;
            if((color&0xffU)>=0x10U)color-=0x10U;
            if((color&15U)!=0)--color;
            write(0x12ecc+index*2,Width::word,color);
        }
        return {0x222fc,(d0&0xffff0000U)|16U,0,0,read(0x12fec,4)};
    }
    const auto counter_word=[&](){
        const auto counter=read(0x21696,2);
        d0=(d0&0xffff0000U)|counter;
        return counter;
    };
    if(instruction==0x21822){
        write(0xdff016,Width::byte,value);
        // BTST #10 against memory tests bit 10 modulo 8 in the byte.
        if((value&4U)==0)write(0x21721,Width::byte,1);
        if(read(0x2171e,1)==0&&read(0x21721,1)!=0&&(counter_word()&1U)==0)
            return execute_deuteros_amiga_owned_outer_transition(0x21850,d0,read,write);
        if(read(0x210f4,1)!=0)
            return execute_deuteros_amiga_owned_outer_transition(0x21892,d0,read,write);
        return {0x2185e,d0};
    }
    if(instruction==0x2185e){
        throw std::runtime_error("Deuteros secondary input requires its hash-bound instruction span");
    }
    throw std::runtime_error("Unsupported Deuteros outer input instruction");
}

template<class Read,class Write>
DeuterosAmigaOuterInputRoute execute_deuteros_amiga_owned_outer_input(
    std::uint32_t instruction,std::uint8_t value,std::uint32_t d0,Read read,Write write,
    const std::span<const std::uint8_t> secondary_input_code){
    if(instruction==0x2185e)
        return execute_deuteros_amiga_owned_secondary_input(secondary_input_code,value,d0,
            read,write);
    return execute_deuteros_amiga_owned_outer_input(instruction,value,d0,read,write);
}

} // namespace eon
