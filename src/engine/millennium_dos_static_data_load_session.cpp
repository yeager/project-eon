#include "engine/millennium_dos_static_data_load_session.hpp"

#include "data/sha256.hpp"

#include <stdexcept>
#include <string>

namespace eon {
namespace {

constexpr std::uint16_t load_bias = 0x0100;
constexpr std::uint16_t loader_call_address = 0xd332;
constexpr std::uint16_t loader_entry_address = 0x101a;
constexpr std::uint16_t loader_return_address = 0xd335;
constexpr std::size_t static_data_size = 12'494;
constexpr std::uint16_t static_data_read_size = 0xffff;
constexpr std::string_view game_executable_sha256 =
    "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
constexpr std::string_view loader_sha256 =
    "d81719b0293c15ad5edbc5c816feb0c44e78abdde749473e5b5795848e4c86cb";
constexpr std::string_view caller_sha256 =
    "f8b1e2bed8701d623133bbe3d5d24e133a2e78ee068a7f335fb43289bffaf286";
constexpr std::string_view source_name_sha256 =
    "91032791cbe9e4cfaa88d2f3d9d4882e58dd66ccfbc8a0c457af21dfcefd63ae";

std::string sha256_hex(const std::span<const std::uint8_t> bytes) {
    return to_hex(sha256(bytes));
}

} // namespace

MillenniumDosStaticDataLoadSession::MillenniumDosStaticDataLoadSession(
    const std::span<const std::uint8_t> game_executable,
    const std::uint16_t code_segment, const std::uint16_t data_segment)
    : code_segment_(code_segment), data_segment_(data_segment) {
    constexpr std::size_t loader_offset = loader_entry_address - load_bias;
    constexpr std::size_t caller_offset = loader_call_address - load_bias;
    constexpr std::size_t source_name_offset = 0x100d - load_bias;
    constexpr std::size_t loader_size = 39;
    constexpr std::size_t caller_size = 3;
    constexpr std::size_t source_name_size = 12;
    if (code_segment == 0 || data_segment == 0 || game_executable.size() != 54'391
        || sha256_hex(game_executable) != game_executable_sha256
        || sha256_hex(game_executable.subspan(loader_offset, loader_size)) != loader_sha256
        || sha256_hex(game_executable.subspan(caller_offset, caller_size)) != caller_sha256
        || sha256_hex(game_executable.subspan(source_name_offset, source_name_size))
            != source_name_sha256) {
        throw std::runtime_error("Unsupported Millennium DOS static-data loader profile");
    }
}

MillenniumDosStaticDataLoadBoundary
MillenniumDosStaticDataLoadSession::make_boundary() const {
    MillenniumDosStaticDataLoadBoundary result{
        .state = state_,
        .code_segment = code_segment_,
    };
    switch (state_) {
    case MillenniumDosStaticDataLoadState::awaiting_loader_entry:
        result.instruction_address = loader_call_address;
        result.target_address = loader_entry_address;
        result.return_address = loader_return_address;
        break;
    case MillenniumDosStaticDataLoadState::awaiting_open_return:
        result.instruction_address = 0x054d;
        result.return_address = 0x054f;
        result.interrupt_number = 0x21;
        result.service = 0x3d;
        result.ax = 0x3d00;
        result.dx = 0x100d;
        result.ds = code_segment_;
        break;
    case MillenniumDosStaticDataLoadState::awaiting_read_return:
        result.instruction_address = 0x0580;
        result.return_address = 0x0582;
        result.interrupt_number = 0x21;
        result.service = 0x3f;
        result.ax = static_cast<std::uint16_t>(0x3f00U | (file_handle_ & 0x00ffU));
        result.bx = file_handle_;
        result.cx = static_data_read_size;
        result.ds = data_segment_;
        result.dx = 0;
        break;
    case MillenniumDosStaticDataLoadState::awaiting_close_return:
        result.instruction_address = 0x05a2;
        result.return_address = 0x05a4;
        result.interrupt_number = 0x21;
        result.service = 0x3e;
        result.ax = static_cast<std::uint16_t>(0x3e00U | (last_read_ax_ & 0x00ffU));
        result.bx = file_handle_;
        result.cx = last_read_cx_;
        result.dx = last_read_dx_;
        break;
    case MillenniumDosStaticDataLoadState::awaiting_loader_return:
        result.instruction_address = 0x1040;
        result.return_address = loader_return_address;
        break;
    case MillenniumDosStaticDataLoadState::complete:
    case MillenniumDosStaticDataLoadState::failed:
        break;
    }
    return result;
}

MillenniumDosStaticDataLoadBoundary MillenniumDosStaticDataLoadSession::boundary() const {
    return make_boundary();
}

MillenniumDosStaticDataLoadCheckpoint
MillenniumDosStaticDataLoadSession::checkpoint() const {
    return {state_, make_boundary(), code_segment_, data_segment_, file_handle_, bytes_read_};
}

void MillenniumDosStaticDataLoadSession::observe_loader_entry(
    const std::uint64_t sequence, const std::uint16_t call_address,
    const std::uint16_t target_address, const std::uint16_t return_address,
    const std::uint16_t code_segment) {
    if (state_ != MillenniumDosStaticDataLoadState::awaiting_loader_entry
        || sequence == 0 || call_address != loader_call_address
        || target_address != loader_entry_address || return_address != loader_return_address
        || code_segment != code_segment_) {
        throw std::runtime_error("Millennium DOS static-data loader entry is detached");
    }
    last_sequence_ = sequence;
    state_ = MillenniumDosStaticDataLoadState::awaiting_open_return;
}

std::optional<MillenniumDosStaticDataReadCommit>
MillenniumDosStaticDataLoadSession::observe_dos_return(
    const MillenniumDosStaticDataDosReturnObservation& observation) {
    if (observation.sequence == 0 || observation.sequence <= last_sequence_
        || observation.interrupt_number != 0x21
        || observation.return_code_segment != code_segment_) {
        throw std::runtime_error("Millennium DOS static-data service return is detached");
    }

    switch (state_) {
    case MillenniumDosStaticDataLoadState::awaiting_open_return:
        if (observation.interrupt_instruction_address != 0x054d
            || observation.service != 0x3d
            || observation.return_instruction_pointer != 0x054f) {
            throw std::runtime_error("Static-data open return is detached from $054d");
        }
        last_sequence_ = observation.sequence;
        if ((observation.flags & 1U) != 0U) {
            state_ = MillenniumDosStaticDataLoadState::failed;
            return std::nullopt;
        }
        file_handle_ = observation.ax;
        state_ = MillenniumDosStaticDataLoadState::awaiting_read_return;
        return std::nullopt;
    case MillenniumDosStaticDataLoadState::awaiting_read_return:
        if (observation.interrupt_instruction_address != 0x0580
            || observation.service != 0x3f
            || observation.return_instruction_pointer != 0x0582
            || observation.bx != file_handle_) {
            throw std::runtime_error("Static-data read return is detached from $0580");
        }
        last_sequence_ = observation.sequence;
        if ((observation.flags & 1U) != 0U
            || observation.ax != static_data_size) {
            state_ = MillenniumDosStaticDataLoadState::failed;
            return std::nullopt;
        }
        bytes_read_ = true;
        last_read_ax_ = observation.ax;
        last_read_cx_ = observation.cx;
        last_read_dx_ = observation.dx;
        state_ = MillenniumDosStaticDataLoadState::awaiting_close_return;
        return MillenniumDosStaticDataReadCommit{
            observation.sequence, data_segment_, 0, static_data_read_size,
            static_cast<std::uint16_t>(static_data_size)};
    case MillenniumDosStaticDataLoadState::awaiting_close_return:
        if (observation.interrupt_instruction_address != 0x05a2
            || observation.service != 0x3e
            || observation.return_instruction_pointer != 0x05a4
            || observation.bx != file_handle_) {
            throw std::runtime_error("Static-data close return is detached from $05a2");
        }
        last_sequence_ = observation.sequence;
        if ((observation.flags & 1U) != 0U) {
            state_ = MillenniumDosStaticDataLoadState::failed;
            return std::nullopt;
        }
        state_ = MillenniumDosStaticDataLoadState::awaiting_loader_return;
        return std::nullopt;
    case MillenniumDosStaticDataLoadState::awaiting_loader_entry:
    case MillenniumDosStaticDataLoadState::awaiting_loader_return:
    case MillenniumDosStaticDataLoadState::complete:
    case MillenniumDosStaticDataLoadState::failed:
        throw std::runtime_error("No Millennium DOS static-data DOS return is pending");
    }
    throw std::runtime_error("Invalid Millennium DOS static-data loader state");
}

void MillenniumDosStaticDataLoadSession::observe_loader_return(
    const std::uint64_t sequence, const std::uint16_t instruction_address,
    const std::uint16_t returned_to, const std::uint16_t code_segment) {
    if (state_ != MillenniumDosStaticDataLoadState::awaiting_loader_return
        || sequence == 0 || sequence <= last_sequence_
        || instruction_address != 0x1040 || returned_to != loader_return_address
        || code_segment != code_segment_) {
        throw std::runtime_error("Millennium DOS static-data loader return is detached");
    }
    last_sequence_ = sequence;
    state_ = MillenniumDosStaticDataLoadState::complete;
}

} // namespace eon
