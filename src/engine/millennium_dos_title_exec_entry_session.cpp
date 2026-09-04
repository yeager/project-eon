#include "engine/millennium_dos_title_exec_entry_session.hpp"

#include "data/sha256.hpp"

#include <stdexcept>

namespace eon {
namespace {
constexpr auto mill_sha =
    "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e";
constexpr auto titles_sha =
    "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6";
constexpr auto titles_entry_prefix_sha =
    "f68952a9bbb82fa876f35aa293b010e2fb0be9f2814c77d2f8604391716ccd07";
}

MillenniumDosTitleExecEntrySession::MillenniumDosTitleExecEntrySession(
    const std::span<const std::uint8_t> mill_com,
    const std::span<const std::uint8_t> titles_executable) {
    if (to_hex(sha256(mill_com)) != mill_sha
        || to_hex(sha256(titles_executable)) != titles_sha
        || titles_executable.size() < 7
        || to_hex(sha256(titles_executable.first(7))) != titles_entry_prefix_sha) {
        throw std::runtime_error("Unsupported Millennium DOS title EXEC-entry media");
    }
}

void MillenniumDosTitleExecEntrySession::observe_child_process_entry(
    const MillenniumDosTitleExecProcessEntry& entry) {
    if (state_ != MillenniumDosTitleExecEntryState::awaiting_child_process_entry
        || entry.sequence == 0 || entry.sequence <= last_sequence_
        || entry.parent_exec_instruction != 0x0336 || entry.ax != 0x4b00
        || entry.dx != 0x068f || entry.parameter_block != 0x067a
        || entry.child_entry_ip != 0x0100 || entry.child_code_segment == 0) {
        throw std::runtime_error("Detached Millennium DOS TITLES.EXE process entry");
    }
    last_sequence_ = entry.sequence;
    child_code_segment_ = entry.child_code_segment;
    provenance_ = entry.provenance;
    state_ = MillenniumDosTitleExecEntryState::entry_prefix_boundary;
}

void MillenniumDosTitleExecEntrySession::execute_exact_entry_prefix(
    const std::uint64_t sequence, const std::uint16_t prefix_address,
    const std::uint16_t jump_address, const std::uint16_t jump_destination) {
    if (state_ != MillenniumDosTitleExecEntryState::entry_prefix_boundary
        || sequence == 0 || sequence <= last_sequence_
        || prefix_address != 0x0100 || jump_address != 0x0104
        || jump_destination != 0x1b80) {
        throw std::runtime_error("Detached Millennium DOS TITLES.EXE entry prefix");
    }
    effects_ = {
        {0x0101, "DS", child_code_segment_},
        {0x0103, "ES", child_code_segment_},
        {0x0104, "IP", 0x1b80},
    };
    last_sequence_ = sequence;
    state_ = MillenniumDosTitleExecEntryState::title_entry_boundary;
}

MillenniumDosTitleExecEntryCheckpoint
MillenniumDosTitleExecEntrySession::checkpoint() const {
    return {state_, last_sequence_, child_code_segment_, provenance_, effects_};
}

} // namespace eon
