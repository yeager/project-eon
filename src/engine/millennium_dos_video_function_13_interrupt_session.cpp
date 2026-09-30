#include "engine/millennium_dos_video_function_13_interrupt_session.hpp"

#include "data/millennium_dos_video_driver.hpp"
#include "data/sha256.hpp"

#include <array>
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
        constexpr std::size_t counter_path_offset = 0x0c94;
        constexpr std::size_t counter_path_size = 0x0e;
        constexpr std::size_t counter_update_offset = 0x0d11;
        constexpr std::size_t counter_update_size = 0x07;
        constexpr std::size_t callback_epilogue_offset = 0x0d04;
        constexpr std::size_t callback_epilogue_size = 0x0d;
        constexpr std::size_t alternate_prefix_offset = 0x0d35;
        constexpr std::size_t alternate_prefix_size = 0x1f;
        constexpr std::size_t alternate_dispatch_offset = 0x0d54;
        constexpr std::size_t alternate_dispatch_size = 0x15;
        constexpr std::size_t alternate_loop_offset = 0x0dad;
        constexpr std::size_t alternate_loop_size = 0x08;
        constexpr std::size_t alternate_copy_route_offset = 0x0dcb;
        constexpr std::size_t alternate_copy_route_size = 0x02;
        if (callback_offset > english_driver.size()
            || english_driver.size() - callback_offset < callback_size
            || to_hex(sha256(english_driver.subspan(callback_offset, callback_size)))
                != "4a470e322e180bdecc72bee6717ea0452be755951694ef22cfdd85c76763de56"
            || counter_path_offset > english_driver.size()
            || english_driver.size() - counter_path_offset < counter_path_size
            || to_hex(sha256(english_driver.subspan(counter_path_offset, counter_path_size)))
                != "91446b8b0a3831742642aa0595e2f78301c3e35f5eb6bebf33afa0189070e0ed"
            || counter_update_offset > english_driver.size()
            || english_driver.size() - counter_update_offset < counter_update_size
            || to_hex(sha256(english_driver.subspan(counter_update_offset, counter_update_size)))
                != "ac86356c47ac73bde697d8ca755674ac5b0847c8f18a82e5bc8b821140359ef4"
            || callback_epilogue_offset > english_driver.size()
            || english_driver.size() - callback_epilogue_offset < callback_epilogue_size
            || to_hex(sha256(english_driver.subspan(callback_epilogue_offset, callback_epilogue_size)))
                != "e8dfe66cd147eb9087b4d06a5f7b2d1923da7f5a7878663a97dbd0e09cd98473"
            || alternate_prefix_offset > english_driver.size()
            || english_driver.size() - alternate_prefix_offset < alternate_prefix_size
            || to_hex(sha256(english_driver.subspan(alternate_prefix_offset, alternate_prefix_size)))
                != "64b62dfa3070346ec8c3be344e278d9da3196949f56c66fafb722d0f3314ad5d"
            || alternate_dispatch_offset > english_driver.size()
            || english_driver.size() - alternate_dispatch_offset < alternate_dispatch_size
            || to_hex(sha256(english_driver.subspan(alternate_dispatch_offset, alternate_dispatch_size)))
                != "46bff2ad876711a99fe2de1331a888ba00255f5a3fcf1e5eb5b8e385af5c96cc"
            || alternate_loop_offset > english_driver.size()
            || english_driver.size() - alternate_loop_offset < alternate_loop_size
            || to_hex(sha256(english_driver.subspan(alternate_loop_offset, alternate_loop_size)))
                != "04de57200f0abc417bc2ed36052791161e8430819c378f87db5f2a8878a102a6"
            || alternate_copy_route_offset > english_driver.size()
            || english_driver.size() - alternate_copy_route_offset < alternate_copy_route_size
            || to_hex(sha256(english_driver.subspan(alternate_copy_route_offset, alternate_copy_route_size)))
                != "576c81f81d170ff0d2580e411b96e98c074321d8ff24b5d9d9a9f311b8a30d39") {
            throw std::runtime_error("Unsupported Millennium MCGA function-$13 callback spans");
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

void MillenniumDosVideoFunction13InterruptSession::observe_mcga_callback_counter(
    const MillenniumDosVideoFunction13McgaCallbackRead& read) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0c94
        || read.sequence != last_sequence_ + 1
        || read.instruction_address != 0x0c9a
        || read.segment != driver_segment_
        || read.offset != 0x0c92
        || read.width != MillenniumDosVideoFunction13McgaCallbackReadWidth::word) {
        throw std::runtime_error("Detached Millennium DOS MCGA callback counter read");
    }
    callback_reads_.push_back(read);
    last_sequence_ = read.sequence;
    callback_driver_effects_.push_back({0x0c94,driver_segment_,0x01e4,1});
    if (read.value == 0) {
        callback_next_instruction_ = 0x0ca2;
        return;
    }
    callback_driver_word_effects_.push_back({0x0d11,driver_segment_,0x0c92,
        static_cast<std::uint16_t>(read.value - 1U)});
    callback_driver_effects_.push_back({0x0d04,driver_segment_,0x01e4,0});
    callback_driver_effects_.push_back({0x0d0a,driver_segment_,0x01e5,0});
    callback_next_instruction_ = 0x0d10;
}

void MillenniumDosVideoFunction13InterruptSession::execute_mcga_callback_alternate_flag(
    const std::uint64_t sequence,
    const std::uint16_t instruction_address) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0d35
        || sequence != last_sequence_ + 1
        || instruction_address != 0x0d35) {
        throw std::runtime_error("Detached Millennium DOS MCGA alternate callback flag");
    }
    callback_driver_effects_.push_back({0x0d35,driver_segment_,0x01e4,1});
    last_sequence_ = sequence;
    callback_next_instruction_ = 0x0d3b;
}

void MillenniumDosVideoFunction13InterruptSession::execute_mcga_callback_register_saves(
    const MillenniumDosVideoFunction13McgaCallbackRegisterStackInput& input) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0d3b
        || input.sequence != last_sequence_ + 1
        || input.instruction_address != 0x0d3b) {
        throw std::runtime_error("Detached Millennium DOS MCGA callback register saves");
    }
    const std::array<std::uint16_t,9> values{
        input.ds,input.es,input.di,input.si,input.dx,input.cx,input.ax,input.bx,input.bp};
    callback_stack_effects_.reserve(callback_stack_effects_.size() + values.size());
    auto stack_pointer = input.sp;
    for (std::size_t i = 0; i < values.size(); ++i) {
        stack_pointer = static_cast<std::uint16_t>(stack_pointer - 2U);
        callback_stack_effects_.push_back({static_cast<std::uint16_t>(0x0d3bU + i),
            input.ss,stack_pointer,values[i]});
    }
    last_sequence_ = input.sequence;
    callback_next_instruction_ = 0x0d44;
}

void MillenniumDosVideoFunction13InterruptSession::observe_mcga_callback_alternate_cx_read(
    const MillenniumDosVideoFunction13McgaCallbackRead& read) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0d44
        || read.sequence != last_sequence_ + 1
        || read.instruction_address != 0x0d44
        || read.segment != driver_segment_
        || read.offset != 0x0d18
        || read.width != MillenniumDosVideoFunction13McgaCallbackReadWidth::word) {
        throw std::runtime_error("Detached Millennium DOS MCGA alternate CX read");
    }
    callback_reads_.reserve(callback_reads_.size() + 1);
    callback_register_effects_.reserve(callback_register_effects_.size() + 1);
    callback_reads_.push_back(read);
    callback_register_effects_.push_back({0x0d44,
        MillenniumDosVideoFunction13McgaCallbackRegister::cx,read.value});
    callback_cx_ = read.value;
    last_sequence_ = read.sequence;
    callback_next_instruction_ = 0x0d49;
}

void MillenniumDosVideoFunction13InterruptSession::observe_mcga_callback_alternate_pointer(
    const MillenniumDosVideoFunction13McgaCallbackFarPointerRead& read) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0d49
        || read.sequence != last_sequence_ + 1
        || read.instruction_address != 0x0d49
        || read.source_segment != driver_segment_
        || read.source_offset != 0x0d1a) {
        throw std::runtime_error("Detached Millennium DOS MCGA alternate far pointer");
    }
    callback_far_pointer_reads_.reserve(callback_far_pointer_reads_.size() + 1);
    callback_register_effects_.reserve(callback_register_effects_.size() + 2);
    callback_far_pointer_reads_.push_back(read);
    callback_far_pointer_ = read;
    callback_register_effects_.push_back({0x0d49,
        MillenniumDosVideoFunction13McgaCallbackRegister::ds,read.value_segment});
    callback_register_effects_.push_back({0x0d49,
        MillenniumDosVideoFunction13McgaCallbackRegister::si,read.value_offset});
    last_sequence_ = read.sequence;
    callback_next_instruction_ = 0x0d4e;
}

void MillenniumDosVideoFunction13InterruptSession::observe_mcga_callback_alternate_indirect_word(
    const MillenniumDosVideoFunction13McgaCallbackIndirectWordRead& read) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0d4e
        || !callback_far_pointer_
        || read.sequence != last_sequence_ + 1
        || read.instruction_address != 0x0d4e
        || read.segment != callback_far_pointer_->value_segment
        || read.offset != static_cast<std::uint16_t>(callback_far_pointer_->value_offset + 8U)) {
        throw std::runtime_error("Detached Millennium DOS MCGA alternate indirect word");
    }
    callback_indirect_word_reads_.push_back(read);
    last_sequence_ = read.sequence;
    callback_next_instruction_ = read.value == 0 ? 0x0d54 : 0x0dad;
}

void MillenniumDosVideoFunction13InterruptSession::observe_mcga_callback_alternate_video_pointer(
    const MillenniumDosVideoFunction13McgaCallbackFarPointerRead& read) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0d54
        || read.sequence != last_sequence_ + 1
        || read.instruction_address != 0x0d54
        || read.source_segment != driver_segment_
        || read.source_offset != 0x0d1e) {
        throw std::runtime_error("Detached Millennium DOS MCGA video buffer pointer");
    }
    callback_far_pointer_reads_.reserve(callback_far_pointer_reads_.size() + 1);
    callback_register_effects_.reserve(callback_register_effects_.size() + 2);
    callback_far_pointer_reads_.push_back(read);
    callback_register_effects_.push_back({0x0d54,
        MillenniumDosVideoFunction13McgaCallbackRegister::es,read.value_segment});
    callback_register_effects_.push_back({0x0d54,
        MillenniumDosVideoFunction13McgaCallbackRegister::di,read.value_offset});
    last_sequence_ = read.sequence;
    callback_next_instruction_ = 0x0d59;
}

void MillenniumDosVideoFunction13InterruptSession::observe_mcga_callback_alternate_selector_byte(
    const MillenniumDosVideoFunction13McgaCallbackRead& read) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0d59
        || !callback_far_pointer_
        || read.sequence != last_sequence_ + 1
        || read.instruction_address != 0x0d59
        || read.segment != callback_far_pointer_->value_segment
        || read.offset != callback_far_pointer_->value_offset
        || read.width != MillenniumDosVideoFunction13McgaCallbackReadWidth::byte
        || read.value > 0x00ff) {
        throw std::runtime_error("Detached Millennium DOS MCGA callback selector byte");
    }
    callback_reads_.reserve(callback_reads_.size() + 1);
    callback_register_effects_.reserve(callback_register_effects_.size() + 1);
    callback_reads_.push_back(read);
    callback_register_effects_.push_back({0x0d59,
        MillenniumDosVideoFunction13McgaCallbackRegister::al,read.value});
    last_sequence_ = read.sequence;
    callback_next_instruction_ = read.value == 1 ? 0x0dcd
        : (read.value >= 2 && read.value < 4 ? 0x0dcb : 0x0dad);
}

void MillenniumDosVideoFunction13InterruptSession::execute_mcga_callback_loop_iteration(
    const std::uint64_t sequence,
    const std::uint16_t instruction_address) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0dad
        || sequence != last_sequence_ + 1
        || instruction_address != 0x0dad
        || !callback_far_pointer_ || !callback_cx_ || callback_indirect_word_reads_.empty()) {
        throw std::runtime_error("Detached Millennium DOS MCGA callback loop iteration");
    }
    const auto& word_read = callback_indirect_word_reads_.back();
    const auto word_offset = static_cast<std::uint16_t>(callback_far_pointer_->value_offset + 8U);
    if (word_read.segment != callback_far_pointer_->value_segment
        || word_read.offset != word_offset) {
        throw std::runtime_error("Stale Millennium DOS MCGA callback descriptor word");
    }
    const auto si = static_cast<std::uint16_t>(callback_far_pointer_->value_offset + 0x0cU);
    const auto cx = static_cast<std::uint16_t>(*callback_cx_ - 1U);
    callback_memory_word_effects_.reserve(callback_memory_word_effects_.size() + 1);
    callback_register_effects_.reserve(callback_register_effects_.size() + 2);
    callback_memory_word_effects_.push_back({0x0dad,callback_far_pointer_->value_segment,
        word_offset,static_cast<std::uint16_t>(word_read.value - 1U)});
    callback_register_effects_.push_back({0x0db0,
        MillenniumDosVideoFunction13McgaCallbackRegister::si,si});
    callback_register_effects_.push_back({0x0db3,
        MillenniumDosVideoFunction13McgaCallbackRegister::cx,cx});
    callback_far_pointer_->value_offset = si;
    callback_cx_ = cx;
    last_sequence_ = sequence;
    callback_next_instruction_ = cx == 0 ? 0x0db5 : 0x0d4e;
}

void MillenniumDosVideoFunction13InterruptSession::execute_mcga_callback_copy_route(
    const std::uint64_t sequence,
    const std::uint16_t instruction_address) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0dcb
        || sequence != last_sequence_ + 1
        || instruction_address != 0x0dcb) {
        throw std::runtime_error("Detached Millennium DOS MCGA callback copy route");
    }
    last_sequence_ = sequence;
    callback_next_instruction_ = 0x0e46;
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
