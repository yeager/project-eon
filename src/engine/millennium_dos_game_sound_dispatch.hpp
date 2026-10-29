#pragma once

#include "data/sha256.hpp"

#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace eon {

enum class MillenniumDosGameSoundDispatchState {
    rejected,
    selector_gate_blocked,
    awaiting_video_function_return,
    sibm_function_10,
    sibm_function_0e,
};

struct MillenniumDosGameSoundDispatchObservation {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    std::uint16_t entry_ax = 0;
    std::uint16_t entry_bx = 0;
    std::uint8_t selector_byte = 0;
    std::uint16_t video_function_return_address = 0;
};

struct MillenniumDosGameSoundDispatchResult {
    MillenniumDosGameSoundDispatchState state =
        MillenniumDosGameSoundDispatchState::rejected;
    std::uint64_t sequence = 0;
    std::uint16_t next_instruction_address = 0;
    std::uint16_t private_function = 0;
    std::uint16_t voice_index = 0;
    std::uint16_t secondary_argument = 0;
    bool caller_bh_discarded = false;
    std::string error;
};

// Derives only the source-visible SIBM call that follows an explicitly
// observed $d0c9 call. It does not perform INT $95 or play a voice. `game` is
// the exact authenticated 2200AD.EXE byte view; selector and call-return
// values must come from that same live native boundary.
[[nodiscard]] inline MillenniumDosGameSoundDispatchResult
evaluate_millennium_dos_game_sound_dispatch(
    const std::span<const std::uint8_t> game,
    const MillenniumDosGameSoundDispatchObservation& observation) {
    constexpr std::string_view game_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    constexpr std::string_view dispatcher_sha256 =
        "a224289d59fa43556581ff79941ab31935c66719d2bdb44f88d2d2d33932e565";
    constexpr std::string_view function_e_wrapper_sha256 =
        "ecdd74bba58df274c2a9e8638dcc0143803e53209cca68ca7968b2db99a6411e";
    constexpr std::string_view function_ten_wrapper_sha256 =
        "39706f64c8976677198b154f30cf43d5e35ef1974211ce3676f4909d4d716885";
    constexpr std::string_view int95_wrapper_sha256 =
        "e5c033ac945f574e791bfff61eae9ea686bc81c99ab7842374290759a02b7f2c";
    constexpr std::size_t dispatcher_offset = 0xcfc9; // load bias $0100
    constexpr std::size_t function_e_wrapper_offset = 0xcec7;
    constexpr std::size_t function_ten_wrapper_offset = 0xced4;
    constexpr std::size_t int95_wrapper_offset = 0xce8f;
    constexpr std::size_t dispatcher_size = 0x3b;
    constexpr std::size_t function_e_wrapper_size = 0x0d;
    constexpr std::size_t function_ten_wrapper_size = 0x09;
    constexpr std::size_t int95_wrapper_size = 0x10;

    const auto rejected = [&observation](std::string error) {
        MillenniumDosGameSoundDispatchResult result;
        result.sequence = observation.sequence;
        result.error = std::move(error);
        return result;
    };
    const auto matches = [game](const std::size_t offset, const std::size_t size,
                                const std::string_view digest) {
        return offset <= game.size() && size <= game.size() - offset
            && to_hex(sha256(game.subspan(offset, size))) == digest;
    };
    if (game.size() != 54'391 || to_hex(sha256(game)) != game_sha256
        || !matches(dispatcher_offset, dispatcher_size, dispatcher_sha256)
        || !matches(function_e_wrapper_offset, function_e_wrapper_size,
            function_e_wrapper_sha256)
        || !matches(function_ten_wrapper_offset, function_ten_wrapper_size,
            function_ten_wrapper_sha256)
        || !matches(int95_wrapper_offset, int95_wrapper_size, int95_wrapper_sha256)) {
        return rejected("Millennium $d0c9 source or wrapper hash mismatch");
    }
    if (observation.sequence == 0 || observation.instruction_address != 0xd0c9) {
        return rejected("Millennium sound dispatch is detached from the $d0c9 boundary");
    }

    MillenniumDosGameSoundDispatchResult result;
    result.sequence = observation.sequence;
    if (observation.selector_byte != 0) {
        result.state = MillenniumDosGameSoundDispatchState::selector_gate_blocked;
        result.next_instruction_address = 0xd101;
        return result;
    }
    if (observation.video_function_return_address == 0) {
        result.state = MillenniumDosGameSoundDispatchState::awaiting_video_function_return;
        result.next_instruction_address = 0xd0e7;
        return result;
    }
    if (observation.video_function_return_address != 0xd0e7) {
        return rejected("Millennium function-$13 return is detached from $d0e7");
    }

    const auto caller_al = static_cast<std::uint8_t>(observation.entry_ax);
    if (caller_al == 0) {
        result.state = MillenniumDosGameSoundDispatchState::sibm_function_10;
        result.private_function = 0x10;
        result.next_instruction_address = 0xcfd4;
        return result;
    }

    // $d0ef clears both BX bytes; $d0f1 copies caller AH into BL; $d0f3
    // copies the resulting zero BH into AH. In the $cfc7 wrapper, AX is the
    // first stack argument at the handler's BP+4, which SIBM indexes.
    result.state = MillenniumDosGameSoundDispatchState::sibm_function_0e;
    result.private_function = 0x0e;
    result.next_instruction_address = 0xcfc7;
    result.voice_index = caller_al;
    result.secondary_argument = static_cast<std::uint8_t>(observation.entry_ax >> 8U);
    result.caller_bh_discarded = true;
    return result;
}

} // namespace eon
