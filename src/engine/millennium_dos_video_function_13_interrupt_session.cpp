#include "engine/millennium_dos_video_function_13_interrupt_session.hpp"

#include "data/millennium_dos_video_driver.hpp"
#include "data/sha256.hpp"

#include <stdexcept>

namespace eon {

MillenniumDosVideoFunction13InterruptSession::MillenniumDosVideoFunction13InterruptSession(
    const std::span<const std::uint8_t> english_driver,
    const MillenniumDosVideoDriverKind kind,
    const std::uint16_t driver_segment)
    : kind_(kind), driver_segment_(driver_segment), retrace_(english_driver, kind) {
    if (kind_ == MillenniumDosVideoDriverKind::mcga) {
        constexpr std::size_t callback_offset = 0x0d22;
        constexpr std::size_t callback_size = 0x13;
        if (callback_offset > english_driver.size()
            || english_driver.size() - callback_offset < callback_size
            || to_hex(sha256(english_driver.subspan(callback_offset, callback_size)))
                != "4a470e322e180bdecc72bee6717ea0452be755951694ef22cfdd85c76763de56") {
            throw std::runtime_error("Unsupported Millennium MCGA function-$13 callback prefix");
        }
    }
}

std::optional<MillenniumDosVideoFunction13InterruptBoundary>
MillenniumDosVideoFunction13InterruptSession::boundary() const {
    switch (state_) {
    case MillenniumDosVideoFunction13InterruptState::awaiting_interrupt_request:
        return MillenniumDosVideoFunction13InterruptBoundary{0x0127};
    case MillenniumDosVideoFunction13InterruptState::awaiting_retrace_port_read:
        if (const auto next = retrace_.boundary()) {
            return MillenniumDosVideoFunction13InterruptBoundary{next->instruction_address};
        }
        return std::nullopt;
    case MillenniumDosVideoFunction13InterruptState::awaiting_mcga_postlude_byte:
        return MillenniumDosVideoFunction13InterruptBoundary{0x001a};
    case MillenniumDosVideoFunction13InterruptState::iret_boundary:
        return MillenniumDosVideoFunction13InterruptBoundary{
            static_cast<std::uint16_t>(kind_ == MillenniumDosVideoDriverKind::ega640 ? 0x0012 : 0x0020)};
    case MillenniumDosVideoFunction13InterruptState::callback_boundary:
        return MillenniumDosVideoFunction13InterruptBoundary{0x0d22};
    case MillenniumDosVideoFunction13InterruptState::awaiting_mcga_callback_word:
        return MillenniumDosVideoFunction13InterruptBoundary{0x0d2d};
    case MillenniumDosVideoFunction13InterruptState::callback_local_boundary:
        return MillenniumDosVideoFunction13InterruptBoundary{callback_next_instruction_};
    case MillenniumDosVideoFunction13InterruptState::returned:
        return std::nullopt;
    }
    return std::nullopt;
}

void MillenniumDosVideoFunction13InterruptSession::observe_interrupt_request(
    const MillenniumDosVideoFunction13InterruptRequest& request) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::awaiting_interrupt_request
        || request.sequence == 0
        || request.instruction_address != 0x0127
        || request.ax != 0x0013
        || request.return_ip != 0x0129) {
        throw std::runtime_error("Detached Millennium DOS function-$13 INT 91h request");
    }
    request_ = request;
    last_sequence_ = request.sequence;
    state_ = MillenniumDosVideoFunction13InterruptState::awaiting_retrace_port_read;
}

void MillenniumDosVideoFunction13InterruptSession::observe_port_read(
    const MillenniumDosVideoFunction13PortRead& read) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::awaiting_retrace_port_read
        || read.sequence != last_sequence_ + 1) {
        throw std::runtime_error("Detached Millennium DOS function-$13 VGA observation");
    }
    retrace_.observe_status(read);
    last_sequence_ = read.sequence;
    retrace_outcome_ = retrace_.outcome();
    if (!retrace_outcome_) {
        return;
    }
    state_ = kind_ == MillenniumDosVideoDriverKind::ega640
        ? MillenniumDosVideoFunction13InterruptState::iret_boundary
        : MillenniumDosVideoFunction13InterruptState::awaiting_mcga_postlude_byte;
}

void MillenniumDosVideoFunction13InterruptSession::observe_mcga_postlude_byte(
    const MillenniumDosVideoFunction13McgaPostludeByte& read) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::awaiting_mcga_postlude_byte
        || read.sequence != last_sequence_ + 1
        || read.instruction_address != 0x001a
        || read.segment != driver_segment_
        || read.offset != 0x01e5) {
        throw std::runtime_error("Detached Millennium DOS MCGA function-$13 postlude byte");
    }
    last_sequence_ = read.sequence;
    state_ = read.value == 0
        ? MillenniumDosVideoFunction13InterruptState::iret_boundary
        : MillenniumDosVideoFunction13InterruptState::callback_boundary;
}

void MillenniumDosVideoFunction13InterruptSession::observe_mcga_callback_read(
    const MillenniumDosVideoFunction13McgaCallbackRead& read) {
    const bool first_read = state_ == MillenniumDosVideoFunction13InterruptState::callback_boundary;
    const bool second_read = state_ == MillenniumDosVideoFunction13InterruptState::awaiting_mcga_callback_word;
    if ((!first_read && !second_read)
        || read.sequence != last_sequence_ + 1
        || read.segment != driver_segment_
        || (first_read
            && (read.instruction_address != 0x0d22 || read.offset != 0x0c88
                || read.width != MillenniumDosVideoFunction13McgaCallbackReadWidth::byte
                || read.value > 0x00ff))
        || (second_read
            && (read.instruction_address != 0x0d2d || read.offset != 0x0d18
                || read.width != MillenniumDosVideoFunction13McgaCallbackReadWidth::word))) {
        throw std::runtime_error("Detached Millennium DOS MCGA function-$13 callback read");
    }
    callback_reads_.push_back(read);
    last_sequence_ = read.sequence;
    if (first_read && read.value == 0) {
        state_ = MillenniumDosVideoFunction13InterruptState::awaiting_mcga_callback_word;
        return;
    }
    callback_next_instruction_ = first_read
        ? 0x0c94
        : (read.value == 0 ? 0x0d10 : 0x0d35);
    state_ = MillenniumDosVideoFunction13InterruptState::callback_local_boundary;
}

void MillenniumDosVideoFunction13InterruptSession::execute_iret(
    const std::uint64_t sequence, const std::uint16_t instruction_address) {
    const auto iret_address = static_cast<std::uint16_t>(
        kind_ == MillenniumDosVideoDriverKind::ega640 ? 0x0012 : 0x0020);
    if (state_ != MillenniumDosVideoFunction13InterruptState::iret_boundary
        || sequence != last_sequence_ + 1 || instruction_address != iret_address
        || !request_ || !retrace_outcome_ || retrace_outcome_->reads.empty()) {
        throw std::runtime_error("Detached Millennium DOS function-$13 IRET");
    }
    last_sequence_ = sequence;
    outcome_ = MillenniumDosVideoFunction13InterruptOutcome{
        retrace_outcome_->reads.back().value,
        0x03da,
        0x0026,
        driver_segment_,
        request_->return_ip,
        request_->return_cs,
        request_->return_flags,
        retrace_outcome_->reads,
        kind_ == MillenniumDosVideoDriverKind::mcga
            ? std::vector<MillenniumDosVideoFunction13DriverByteEffect>{{0x0012,driver_segment_,0x01e4,0}}
            : std::vector<MillenniumDosVideoFunction13DriverByteEffect>{}};
    state_ = MillenniumDosVideoFunction13InterruptState::returned;
}

} // namespace eon
