#include "engine/millennium_dos_title_to_game_session.hpp"

#include "data/millennium_dos_title_exit.hpp"
#include "data/millennium_dos_title_flow.hpp"
#include "data/sha256.hpp"

#include <stdexcept>

namespace eon {

MillenniumDosTitleToGameSession::MillenniumDosTitleToGameSession(
    const std::span<const std::uint8_t> mill_launcher,
    const std::span<const std::uint8_t> titles_executable) {
    constexpr auto mill_sha =
        "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e";
    constexpr auto titles_sha =
        "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6";
    if (to_hex(sha256(mill_launcher)) != mill_sha
        || to_hex(sha256(titles_executable)) != titles_sha) {
        throw std::runtime_error("Unsupported Millennium DOS title-to-game media");
    }
    const auto flow = parse_millennium_dos_title_flow(titles_executable, mill_launcher);
    const auto exit = parse_millennium_dos_title_exit_closure(titles_executable);
    if (flow.launcher_title_call_address != 0x0240
        || flow.launcher_game_call_address != 0x024c
        || flow.launcher_common_call_target != 0x031c
        || flow.launcher_game_program_address != 0x069a
        || flow.launcher_exec_interrupt_site != 0x0337
        || flow.launcher_exec_result_interrupt_site != 0x0348
        || exit.nonzero_entry_address != 0x1c54
        || exit.exit_interrupt != 0x21 || exit.exit_service != 0x4c) {
        throw std::runtime_error("Unsupported Millennium DOS title-to-game connection");
    }
}

MillenniumDosTitleToGameBoundary MillenniumDosTitleToGameSession::boundary() const {
    switch (state_) {
    case MillenniumDosTitleToGameState::awaiting_title_cleanup_return:
        return {MillenniumDosTitleToGameBoundaryKind::call_return,0x1c54,0x1c57,0,0};
    case MillenniumDosTitleToGameState::awaiting_title_post_driver_return:
        return {MillenniumDosTitleToGameBoundaryKind::call_return,0x1c57,0x1c5a,0,0};
    case MillenniumDosTitleToGameState::awaiting_stack_word:
        return {MillenniumDosTitleToGameBoundaryKind::runtime_word,0x1c60,0x1aa0,0,0};
    case MillenniumDosTitleToGameState::awaiting_title_final_local_return:
        return {MillenniumDosTitleToGameBoundaryKind::call_return,0x1c64,0x1c67,0,0};
    case MillenniumDosTitleToGameState::awaiting_exit_stub_call_return:
        return {MillenniumDosTitleToGameBoundaryKind::call_return,0x1a0f,0x1a12,0,0};
    case MillenniumDosTitleToGameState::awaiting_title_termination:
        return {MillenniumDosTitleToGameBoundaryKind::dos_interrupt_return,0x1a18,0,0,0x4c00};
    case MillenniumDosTitleToGameState::awaiting_parent_exec_return:
        return {MillenniumDosTitleToGameBoundaryKind::dos_interrupt_return,0x0337,0x0339,0,0x4b00};
    case MillenniumDosTitleToGameState::awaiting_child_status:
        return {MillenniumDosTitleToGameBoundaryKind::dos_interrupt_return,0x0348,0x034a,0,0x4d00};
    case MillenniumDosTitleToGameState::game_exec_boundary:
        return {MillenniumDosTitleToGameBoundaryKind::dos_exec,0x024c,0x031c,0x069a,0};
    }
    throw std::runtime_error("Invalid Millennium DOS title-to-game state");
}

void MillenniumDosTitleToGameSession::observe_call_return(
    const std::uint16_t call_address, const std::uint16_t return_address) {
    const auto b = boundary();
    if (b.kind != MillenniumDosTitleToGameBoundaryKind::call_return
        || call_address != b.instruction_address || return_address != b.target_or_return) {
        throw std::runtime_error("Detached Millennium DOS title-exit call return");
    }
    switch (state_) {
    case MillenniumDosTitleToGameState::awaiting_title_cleanup_return:
        state_ = MillenniumDosTitleToGameState::awaiting_title_post_driver_return; break;
    case MillenniumDosTitleToGameState::awaiting_title_post_driver_return:
        effects_.push_back({0x1c5a,0x1a0e,0});
        state_ = MillenniumDosTitleToGameState::awaiting_stack_word; break;
    case MillenniumDosTitleToGameState::awaiting_title_final_local_return:
        state_ = MillenniumDosTitleToGameState::awaiting_exit_stub_call_return; break;
    case MillenniumDosTitleToGameState::awaiting_exit_stub_call_return:
        state_ = MillenniumDosTitleToGameState::awaiting_title_termination; break;
    default: throw std::runtime_error("Unsupported Millennium DOS title-exit call state");
    }
}

void MillenniumDosTitleToGameSession::observe_stack_word(const std::uint16_t instruction_address,
    const std::uint16_t address, const std::uint16_t value) {
    if (state_ != MillenniumDosTitleToGameState::awaiting_stack_word
        || instruction_address != 0x1c60 || address != 0x1aa0) {
        throw std::runtime_error("Detached Millennium DOS title stack observation");
    }
    restored_sp_ = value;
    state_ = MillenniumDosTitleToGameState::awaiting_title_final_local_return;
}

void MillenniumDosTitleToGameSession::observe_title_termination(
    const std::uint16_t interrupt_address, const std::uint16_t ax) {
    if (state_ != MillenniumDosTitleToGameState::awaiting_title_termination
        || interrupt_address != 0x1a18 || ax != 0x4c00) {
        throw std::runtime_error("Detached Millennium DOS title termination");
    }
    state_ = MillenniumDosTitleToGameState::awaiting_parent_exec_return;
}

void MillenniumDosTitleToGameSession::observe_parent_exec_return(
    const std::uint16_t interrupt_address, const bool carry) {
    if (state_ != MillenniumDosTitleToGameState::awaiting_parent_exec_return
        || interrupt_address != 0x0337 || carry) {
        throw std::runtime_error("Millennium DOS title EXEC did not take the proven noncarry route");
    }
    state_ = MillenniumDosTitleToGameState::awaiting_child_status;
}

void MillenniumDosTitleToGameSession::observe_child_status(const std::uint16_t interrupt_address,
    const std::uint8_t al, const bool carry) {
    if (state_ != MillenniumDosTitleToGameState::awaiting_child_status
        || interrupt_address != 0x0348 || carry || al != 0) {
        throw std::runtime_error("Millennium DOS title status does not select game request");
    }
    child_status_al_ = al;
    state_ = MillenniumDosTitleToGameState::game_exec_boundary;
}

} // namespace eon
