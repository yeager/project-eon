#include "engine/millennium_dos_title_to_game_session.hpp"

#include "data/millennium_dos_title_exit.hpp"
#include "data/millennium_dos_title_flow.hpp"
#include "data/millennium_dos_game_flow.hpp"
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
    case MillenniumDosTitleToGameState::awaiting_game_process_entry:
        return {MillenniumDosTitleToGameBoundaryKind::game_process_entry,0x0100,0,0x069a,0x4b00};
    case MillenniumDosTitleToGameState::game_process_entry_boundary:
        return {MillenniumDosTitleToGameBoundaryKind::game_process_entry,0x0100,
            child_code_segment_,0x069a,0x4b00};
    case MillenniumDosTitleToGameState::game_entry_prefix_executed:
        return {MillenniumDosTitleToGameBoundaryKind::game_entry_prefix,0xd2b0,0xd2b0,0,0};
    case MillenniumDosTitleToGameState::game_startup_call_boundary:
        return {MillenniumDosTitleToGameBoundaryKind::game_native_call,0xd2c5,0x0124,0,0x001f,0xd19e};
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

void MillenniumDosTitleToGameSession::observe_game_exec_request(
    const MillenniumDosTitleToGameExecRequest& observation) {
    if (state_ != MillenniumDosTitleToGameState::game_exec_boundary
        || game_exec_requested_ || observation.sequence == 0
        || observation.call_instruction != 0x024c || observation.helper_target != 0x031c
        || observation.program_address != 0x069a || observation.parameter_block != 0x067a) {
        throw std::runtime_error("Detached Millennium DOS game EXEC request");
    }
    game_exec_requested_ = true;
    game_exec_sequence_ = observation.sequence;
    state_ = MillenniumDosTitleToGameState::awaiting_game_process_entry;
}

void MillenniumDosTitleToGameSession::observe_parent_exec_return(
    const std::uint16_t interrupt_address, const bool carry) {
    if (state_ != MillenniumDosTitleToGameState::awaiting_parent_exec_return
        || parent_exec_returned_
        || interrupt_address != 0x0337 || carry) {
        throw std::runtime_error("Millennium DOS title-process EXEC did not take the proven noncarry route");
    }
    parent_exec_returned_ = true;
    state_ = MillenniumDosTitleToGameState::awaiting_child_status;
}

void MillenniumDosTitleToGameSession::observe_child_status(const std::uint16_t interrupt_address,
    const std::uint8_t al, const bool carry) {
    if (!parent_exec_returned_ || child_status_observed_
        || state_ != MillenniumDosTitleToGameState::awaiting_child_status
        || interrupt_address != 0x0348 || carry || al != 0) {
        throw std::runtime_error("Millennium DOS title status does not select game request");
    }
    child_status_al_ = al;
    child_status_observed_ = true;
    state_ = MillenniumDosTitleToGameState::game_exec_boundary;
}

void MillenniumDosTitleToGameSession::observe_game_process_entry(
    const MillenniumDosTitleToGameProcessEntry& entry) {
    const bool known_provenance =
        entry.provenance == MillenniumDosTitleToGameEntryProvenance::observed_process_entry
        || entry.provenance
            == MillenniumDosTitleToGameEntryProvenance::eon_dos_compatibility_service;
    if (state_ != MillenniumDosTitleToGameState::awaiting_game_process_entry
        || !game_exec_requested_
        || entry.sequence <= game_exec_sequence_ || entry.sequence <= child_entry_sequence_
        || entry.call_instruction != 0x024c || entry.exec_interrupt_instruction != 0x0337
        || entry.ax != 0x4b00 || entry.dx != 0x069a || entry.parameter_block != 0x067a
        || entry.child_entry_ip != 0x0100 || entry.child_code_segment == 0
        || (entry.provenance == MillenniumDosTitleToGameEntryProvenance::observed_process_entry
            && entry.initial_stack_segment != entry.child_code_segment)
        || !known_provenance) {
        throw std::runtime_error("Detached Millennium DOS 2200AD.EXE process entry");
    }
    child_entry_sequence_ = entry.sequence;
    child_code_segment_ = entry.child_code_segment;
    initial_stack_segment_ = entry.initial_stack_segment;
    initial_stack_pointer_ = entry.initial_stack_pointer;
    child_entry_provenance_ = entry.provenance;
    state_ = MillenniumDosTitleToGameState::game_process_entry_boundary;
}

void MillenniumDosTitleToGameSession::execute_game_entry_prefix(
    const std::span<const std::uint8_t> game_executable) {
    constexpr auto game_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    constexpr auto entry_prefix_sha256 =
        "f1f5e9feec70638ff17dd1c96e8542bb8f989aaaafaa856910a3d7eded4575e7";
    if (state_ != MillenniumDosTitleToGameState::game_process_entry_boundary
        || child_code_segment_ == 0
        || to_hex(sha256(game_executable)) != game_sha256
        || game_executable.size() < 7
        || to_hex(sha256(game_executable.first(7))) != entry_prefix_sha256) {
        throw std::runtime_error("Unsupported Millennium DOS game COM entry prefix");
    }
    const auto flow = parse_millennium_dos_game_flow(game_executable);
    if (flow.entry_address != 0xd2b0) {
        throw std::runtime_error("Unsupported Millennium DOS game COM entry target");
    }
    // The exact seven bytes are PUSH CS / POP DS / PUSH CS / POP ES / JMP
    // $d2b0. The pushes leave SP unchanged but overwrite the original stack
    // word, so retain both instruction-ordered writes at the observed SS:SP.
    const auto pushed_stack_offset = static_cast<std::uint16_t>(
        initial_stack_pointer_ - 2U);
    stack_word_effects_.insert(stack_word_effects_.end(), {
        {0x0100, initial_stack_segment_, pushed_stack_offset, child_code_segment_},
        {0x0102, initial_stack_segment_, pushed_stack_offset, child_code_segment_},
    });
    register_effects_.insert(register_effects_.end(), {
        {0x0101, "DS", child_code_segment_},
        {0x0103, "ES", child_code_segment_},
        {0x0104, "IP", flow.entry_address},
    });
    state_ = MillenniumDosTitleToGameState::game_entry_prefix_executed;
}

void MillenniumDosTitleToGameSession::execute_game_startup_prefix(
    const std::span<const std::uint8_t> game_executable) {
    if (state_ != MillenniumDosTitleToGameState::game_entry_prefix_executed) {
        throw std::runtime_error("Millennium DOS startup prefix requires the exact COM entry transfer");
    }
    const auto flow = parse_millennium_dos_game_flow(game_executable);
    const auto startup = evaluate_millennium_dos_english_startup_prefix(game_executable);
    if (flow.entry_address != 0xd2b0 || flow.startup_address != 0xd2b4
        || flow.startup_stack_pointer != 0xda00
        || flow.startup_first_call_address != 0x0124
        || flow.startup_first_call_interrupt != 0x91
        || startup.outcome
            != MillenniumDosEnglishStartupPrefixOutcome::first_private_interrupt_boundary
        || startup.boundary_address != 0x0129 || startup.stack_pointer != 0xda00) {
        throw std::runtime_error("Unsupported Millennium DOS pre-INT 91h startup prefix");
    }
    const auto pushed_stack_offset = static_cast<std::uint16_t>(
        initial_stack_pointer_ - 2U);
    stack_word_effects_.insert(stack_word_effects_.end(), {
        {0xd2b0, initial_stack_segment_, pushed_stack_offset, child_code_segment_},
        {0xd2b2, initial_stack_segment_, pushed_stack_offset, child_code_segment_},
    });
    // Execute only the hash-validated local instructions through the call
    // site. Stop before CALL $0124; the wrapper and private INT 91h remain
    // external. CS is the observed DOS child segment.
    register_effects_.insert(register_effects_.end(), {
        {0xd2b1, "DS", child_code_segment_},
        {0xd2b3, "ES", child_code_segment_},
        {0xd2b4, "AX", child_code_segment_},
        {0xd2b6, "SS", child_code_segment_},
        {0xd2b8, "AX", 0xda00},
        {0xd2bb, "SP", 0xda00},
        {0xd2bd, "AX", 0x001f},
        {0xd2c1, "ES", child_code_segment_},
        {0xd2c2, "BX", 0xd19e},
    });
    state_ = MillenniumDosTitleToGameState::game_startup_call_boundary;
}

} // namespace eon
