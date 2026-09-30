#include "engine/millennium_dos_video_function_13_interrupt_session.hpp"

#include "data/millennium_dos_video_driver.hpp"
#include "data/sha256.hpp"

#include <algorithm>
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
        constexpr std::size_t alternate_copy_prefix_offset = 0x0e46;
        constexpr std::size_t alternate_copy_prefix_size = 0x2b;
        constexpr std::size_t alternate_copy_loop_offset = 0x0e71;
        constexpr std::size_t alternate_copy_loop_size = 0x1e;
        constexpr std::size_t alternate_copy_epilogue_offset = 0x0e8f;
        constexpr std::size_t alternate_copy_epilogue_size = 0x0d;
        constexpr std::size_t alternate_palette_prefix_offset = 0x0d6a;
        constexpr std::size_t alternate_palette_prefix_size = 0x18;
        constexpr std::size_t alternate_palette_out_offset = 0x0d82;
        constexpr std::size_t alternate_palette_post_out_offset = 0x0d83;
        constexpr std::size_t alternate_palette_post_out_size = 0x11;
        constexpr std::size_t alternate_palette_poll_offset = 0x0d94;
        constexpr std::size_t alternate_palette_poll_size = 0x05;
        constexpr std::size_t alternate_palette_transfer_offset = 0x0d99;
        constexpr std::size_t alternate_palette_transfer_size = 0x0a;
        constexpr std::size_t alternate_palette_restore_offset = 0x0da3;
        constexpr std::size_t alternate_palette_restore_size = 0x12;
        constexpr std::size_t alternate_palette_pops_offset = 0x0db5;
        constexpr std::size_t alternate_palette_pops_size = 0x09;
        constexpr std::size_t alternate_palette_clear_offset = 0x0dbe;
        constexpr std::size_t alternate_palette_clear_size = 0x0c;
        constexpr std::size_t mcga_dispatch_offset = 0x0000;
        constexpr std::size_t mcga_dispatch_size = 0x25;
        if (mcga_dispatch_offset > english_driver.size()
            || english_driver.size() - mcga_dispatch_offset < mcga_dispatch_size
            || to_hex(sha256(english_driver.subspan(mcga_dispatch_offset, mcga_dispatch_size)))
                != "e61647601d433d528ab51403c7a73371d58bdbfe0bd25789e936489844f3630f"
            || callback_offset > english_driver.size()
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
                != "576c81f81d170ff0d2580e411b96e98c074321d8ff24b5d9d9a9f311b8a30d39"
            || alternate_copy_prefix_offset > english_driver.size()
            || english_driver.size() - alternate_copy_prefix_offset < alternate_copy_prefix_size
            || to_hex(sha256(english_driver.subspan(alternate_copy_prefix_offset, alternate_copy_prefix_size)))
                != "8e9f07aba6a866c0c1ebd9192f5191d9b9c13608d07c74927adf6f1731cf4a3d"
            || alternate_copy_loop_offset > english_driver.size()
            || english_driver.size() - alternate_copy_loop_offset < alternate_copy_loop_size
            || to_hex(sha256(english_driver.subspan(alternate_copy_loop_offset, alternate_copy_loop_size)))
                != "23e2eb1588993fbf470114728fb5a7969573135dcd191a8812b82ea28d75fe51"
            || alternate_copy_epilogue_offset > english_driver.size()
            || english_driver.size() - alternate_copy_epilogue_offset < alternate_copy_epilogue_size
            || to_hex(sha256(english_driver.subspan(alternate_copy_epilogue_offset, alternate_copy_epilogue_size)))
                != "814984d5a32a5352b29097d23ace8eece29207312cf71f8e9d8ed494c6d217ff"
            || alternate_palette_prefix_offset > english_driver.size()
            || english_driver.size() - alternate_palette_prefix_offset < alternate_palette_prefix_size
            || to_hex(sha256(english_driver.subspan(alternate_palette_prefix_offset, alternate_palette_prefix_size)))
                != "a833f1826f7252c09cebd89a0ebbd5a80e11d0fd6e913e26f30383e3bb4be228"
            || alternate_palette_out_offset >= english_driver.size()
            || english_driver[alternate_palette_out_offset] != 0xeeU
            || alternate_palette_post_out_offset > english_driver.size()
            || english_driver.size() - alternate_palette_post_out_offset < alternate_palette_post_out_size
            || to_hex(sha256(english_driver.subspan(alternate_palette_post_out_offset,
                alternate_palette_post_out_size)))
                != "aa90f0362964c219d1d6f47b40f1c901ca310ea9b44333c748439b27c5954245"
            || alternate_palette_poll_offset > english_driver.size()
            || english_driver.size() - alternate_palette_poll_offset < alternate_palette_poll_size
            || to_hex(sha256(english_driver.subspan(alternate_palette_poll_offset,
                alternate_palette_poll_size)))
                != "e18d42465d19b1d9e34ae1529f154742a8f6815863fa90a6f589099fab753594"
            || alternate_palette_transfer_offset > english_driver.size()
            || english_driver.size() - alternate_palette_transfer_offset < alternate_palette_transfer_size
            || to_hex(sha256(english_driver.subspan(alternate_palette_transfer_offset,
                alternate_palette_transfer_size)))
                != "46b49bd4fcc7c6e76905a9e9ac35ee4af22d0419f698e5094e475b1a9054c014"
            || alternate_palette_restore_offset > english_driver.size()
            || english_driver.size() - alternate_palette_restore_offset < alternate_palette_restore_size
            || to_hex(sha256(english_driver.subspan(alternate_palette_restore_offset,
                alternate_palette_restore_size)))
                != "56a3162a4f2fa14b92c84e13bac182325373e8bade15a998f5dc82f2ed660e45"
            || alternate_palette_pops_offset > english_driver.size()
            || english_driver.size() - alternate_palette_pops_offset < alternate_palette_pops_size
            || to_hex(sha256(english_driver.subspan(alternate_palette_pops_offset,
                alternate_palette_pops_size)))
                != "d07d0c9e1774ee9e4716a0fcc0419979823a28305d91a8fa873b549dd395f076"
            || alternate_palette_clear_offset > english_driver.size()
            || english_driver.size() - alternate_palette_clear_offset < alternate_palette_clear_size
            || to_hex(sha256(english_driver.subspan(alternate_palette_clear_offset,
                alternate_palette_clear_size)))
                != "e418fe4a6546f6a404ab3307dcb7440f50df2d24be4db85ca5db5f3fb0957e0c") {
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
        return MillenniumDosVideoFunction13InterruptBoundary{static_cast<std::uint16_t>(
            kind_ == MillenniumDosVideoDriverKind::ega640 ? 0x0012
                : (callback_iret_pending_ ? 0x0024 : 0x0020))};
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

void MillenniumDosVideoFunction13InterruptSession::observe_mcga_callback_copy_height(
    const MillenniumDosVideoFunction13McgaCallbackRead& read) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0e46
        || !callback_far_pointer_
        || callback_far_pointer_reads_.size() != 2
        || callback_stack_effects_.size() != 9
        || read.sequence != last_sequence_ + 1
        || read.instruction_address != 0x0e49
        || read.segment != callback_far_pointer_->value_segment
        || read.offset != static_cast<std::uint16_t>(callback_far_pointer_->value_offset + 2U)
        || read.width != MillenniumDosVideoFunction13McgaCallbackReadWidth::byte
        || read.value > 0x00ff) {
        throw std::runtime_error("Detached Millennium DOS MCGA callback copy height");
    }
    callback_reads_.reserve(callback_reads_.size() + 1);
    callback_register_effects_.reserve(callback_register_effects_.size() + 6);
    callback_reads_.push_back(read);
    callback_register_effects_.push_back({0x0e49,
        MillenniumDosVideoFunction13McgaCallbackRegister::al,read.value});
    callback_register_effects_.push_back({0x0e4c,
        MillenniumDosVideoFunction13McgaCallbackRegister::ax,read.value});
    const auto rows = static_cast<std::uint16_t>(read.value + 1U);
    callback_register_effects_.push_back({0x0e4e,
        MillenniumDosVideoFunction13McgaCallbackRegister::ax,rows});
    callback_register_effects_.push_back({0x0e4f,
        MillenniumDosVideoFunction13McgaCallbackRegister::cx,rows});
    callback_register_effects_.push_back({0x0e51,
        MillenniumDosVideoFunction13McgaCallbackRegister::ax,
        static_cast<std::uint16_t>(rows << 1U)});
    callback_register_effects_.push_back({0x0e53,
        MillenniumDosVideoFunction13McgaCallbackRegister::cx,
        static_cast<std::uint16_t>(rows * 3U)});
    callback_copy_height_ = static_cast<std::uint8_t>(read.value);
    last_sequence_ = read.sequence;
    callback_next_instruction_ = 0x0e55;
}

void MillenniumDosVideoFunction13InterruptSession::observe_mcga_callback_copy_width(
    const MillenniumDosVideoFunction13McgaCallbackRead& read) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0e55
        || !callback_far_pointer_
        || read.sequence != last_sequence_ + 1
        || read.instruction_address != 0x0e55
        || read.segment != callback_far_pointer_->value_segment
        || read.offset != static_cast<std::uint16_t>(callback_far_pointer_->value_offset + 1U)
        || read.width != MillenniumDosVideoFunction13McgaCallbackReadWidth::byte
        || read.value > 0x00ff) {
        throw std::runtime_error("Detached Millennium DOS MCGA callback copy width");
    }
    callback_reads_.reserve(callback_reads_.size() + 1);
    callback_register_effects_.reserve(callback_register_effects_.size() + 5);
    callback_reads_.push_back(read);
    callback_register_effects_.push_back({0x0e55,
        MillenniumDosVideoFunction13McgaCallbackRegister::al,read.value});
    callback_register_effects_.push_back({0x0e58,
        MillenniumDosVideoFunction13McgaCallbackRegister::ax,read.value});
    callback_register_effects_.push_back({0x0e5a,
        MillenniumDosVideoFunction13McgaCallbackRegister::dx,read.value});
    callback_register_effects_.push_back({0x0e5c,
        MillenniumDosVideoFunction13McgaCallbackRegister::ax,
        static_cast<std::uint16_t>(read.value << 1U)});
    callback_register_effects_.push_back({0x0e5e,
        MillenniumDosVideoFunction13McgaCallbackRegister::ax,
        static_cast<std::uint16_t>(read.value * 3U)});
    callback_copy_width_ = static_cast<std::uint8_t>(read.value);
    last_sequence_ = read.sequence;
    callback_next_instruction_ = 0x0e62;
}

void MillenniumDosVideoFunction13InterruptSession::observe_mcga_callback_copy_limit(
    const MillenniumDosVideoFunction13McgaCallbackRead& read) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0e62
        || !callback_far_pointer_
        || !callback_copy_height_ || !callback_copy_width_
        || callback_far_pointer_reads_.size() != 2
        || read.sequence != last_sequence_ + 1
        || read.instruction_address != 0x0e62
        || read.segment != callback_far_pointer_->value_segment
        || read.offset != static_cast<std::uint16_t>(callback_far_pointer_->value_offset + 6U)
        || read.width != MillenniumDosVideoFunction13McgaCallbackReadWidth::word) {
        throw std::runtime_error("Detached Millennium DOS MCGA callback copy limit");
    }
    const auto video_pointer = callback_far_pointer_reads_.back();
    const auto width_bytes = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(*callback_copy_width_) * 3U);
    const auto initial_di = video_pointer.value_offset;
    const auto copy_start = static_cast<std::uint16_t>(initial_di + width_bytes);
    const auto destination = static_cast<std::uint16_t>(copy_start + 0x0900U + width_bytes);
    const auto row_count = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(*callback_copy_height_) + 1U) * 3U);
    callback_reads_.reserve(callback_reads_.size() + 1);
    callback_register_effects_.reserve(callback_register_effects_.size() + 9);
    callback_reads_.push_back(read);
    callback_register_effects_.push_back({0x0e60,
        MillenniumDosVideoFunction13McgaCallbackRegister::di,copy_start});
    callback_register_effects_.push_back({0x0e62,
        MillenniumDosVideoFunction13McgaCallbackRegister::dx,read.value});
    callback_register_effects_.push_back({0x0e65,
        MillenniumDosVideoFunction13McgaCallbackRegister::si,copy_start});
    callback_register_effects_.push_back({0x0e67,
        MillenniumDosVideoFunction13McgaCallbackRegister::di,
        static_cast<std::uint16_t>(copy_start + 0x0900U)});
    callback_register_effects_.push_back({0x0e6b,
        MillenniumDosVideoFunction13McgaCallbackRegister::di,destination});
    callback_register_effects_.push_back({0x0e6d,
        MillenniumDosVideoFunction13McgaCallbackRegister::ax,video_pointer.value_segment});
    callback_register_effects_.push_back({0x0e6f,
        MillenniumDosVideoFunction13McgaCallbackRegister::ds,video_pointer.value_segment});
    callback_copy_ax_ = video_pointer.value_segment;
    callback_copy_cx_ = row_count;
    callback_copy_dx_ = read.value;
    callback_copy_si_ = copy_start;
    callback_copy_di_ = destination;
    callback_copy_segment_ = video_pointer.value_segment;
    callback_copy_density_.reset();
    last_sequence_ = read.sequence;
    callback_next_instruction_ = 0x0e71;
}

void MillenniumDosVideoFunction13InterruptSession::observe_mcga_callback_copy_source_byte(
    const MillenniumDosVideoFunction13McgaCallbackRead& read) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0e71
        || !callback_copy_ax_ || !callback_copy_si_ || !callback_copy_segment_
        || read.sequence != last_sequence_ + 1
        || read.instruction_address != 0x0e71
        || read.segment != *callback_copy_segment_
        || read.offset != static_cast<std::uint16_t>(*callback_copy_si_ + 0x0600U)
        || read.width != MillenniumDosVideoFunction13McgaCallbackReadWidth::byte
        || read.value > 0x00ff) {
        throw std::runtime_error("Detached Millennium DOS MCGA callback copy source byte");
    }
    callback_reads_.reserve(callback_reads_.size() + 1);
    callback_register_effects_.reserve(callback_register_effects_.size() + 2);
    callback_reads_.push_back(read);
    callback_register_effects_.push_back({0x0e71,
        MillenniumDosVideoFunction13McgaCallbackRegister::al,read.value});
    callback_register_effects_.push_back({0x0e75,
        MillenniumDosVideoFunction13McgaCallbackRegister::ax,read.value});
    callback_copy_ax_ = read.value;
    callback_copy_density_.reset();
    last_sequence_ = read.sequence;
    callback_next_instruction_ = 0x0e77;
}

void MillenniumDosVideoFunction13InterruptSession::observe_mcga_callback_copy_destination_word(
    const MillenniumDosVideoFunction13McgaCallbackRead& read) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0e77
        || !callback_copy_ax_ || !callback_copy_dx_ || !callback_copy_di_
        || !callback_copy_segment_
        || read.sequence != last_sequence_ + 1
        || read.instruction_address != 0x0e77
        || read.segment != *callback_copy_segment_
        || read.offset != *callback_copy_di_
        || read.width != MillenniumDosVideoFunction13McgaCallbackReadWidth::word) {
        throw std::runtime_error("Detached Millennium DOS MCGA callback copy destination word");
    }
    const auto sum = static_cast<std::uint32_t>(*callback_copy_ax_) + read.value;
    const auto ax = static_cast<std::uint16_t>(sum);
    callback_reads_.reserve(callback_reads_.size() + 1);
    callback_register_effects_.reserve(callback_register_effects_.size() + 1);
    callback_reads_.push_back(read);
    callback_register_effects_.push_back({0x0e77,
        MillenniumDosVideoFunction13McgaCallbackRegister::ax,ax});
    callback_copy_ax_ = ax;
    last_sequence_ = read.sequence;
    callback_next_instruction_ = sum > 0xffffU || ax >= *callback_copy_dx_ ? 0x0e7f : 0x0e8b;
}

void MillenniumDosVideoFunction13InterruptSession::observe_mcga_callback_copy_density_byte(
    const MillenniumDosVideoFunction13McgaCallbackRead& read) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0e7f
        || !callback_copy_si_ || !callback_copy_segment_
        || read.sequence != last_sequence_ + 1
        || read.instruction_address != 0x0e7f
        || read.segment != *callback_copy_segment_
        || read.offset != static_cast<std::uint16_t>(*callback_copy_si_ + 0x0300U)
        || read.width != MillenniumDosVideoFunction13McgaCallbackReadWidth::byte
        || read.value > 0x00ff) {
        throw std::runtime_error("Detached Millennium DOS MCGA callback copy density byte");
    }
    callback_reads_.reserve(callback_reads_.size() + 1);
    callback_register_effects_.reserve(callback_register_effects_.size() + 1);
    callback_reads_.push_back(read);
    callback_register_effects_.push_back({0x0e7f,
        MillenniumDosVideoFunction13McgaCallbackRegister::bl,read.value});
    callback_copy_density_ = static_cast<std::uint8_t>(read.value);
    last_sequence_ = read.sequence;
    callback_next_instruction_ = 0x0e83;
}

void MillenniumDosVideoFunction13InterruptSession::observe_mcga_callback_copy_current_byte(
    const MillenniumDosVideoFunction13McgaCallbackRead& read) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0e83
        || !callback_copy_ax_ || !callback_copy_dx_ || !callback_copy_si_
        || !callback_copy_segment_ || !callback_copy_density_
        || read.sequence != last_sequence_ + 1
        || read.instruction_address != 0x0e83
        || read.segment != *callback_copy_segment_
        || read.offset != *callback_copy_si_
        || read.width != MillenniumDosVideoFunction13McgaCallbackReadWidth::byte
        || read.value > 0x00ff) {
        throw std::runtime_error("Detached Millennium DOS MCGA callback copy current byte");
    }
    const auto updated = static_cast<std::uint8_t>(read.value + *callback_copy_density_);
    const auto ax = static_cast<std::uint16_t>(*callback_copy_ax_ - *callback_copy_dx_);
    callback_reads_.reserve(callback_reads_.size() + 1);
    callback_memory_byte_effects_.reserve(callback_memory_byte_effects_.size() + 1);
    callback_register_effects_.reserve(callback_register_effects_.size() + 1);
    callback_reads_.push_back(read);
    callback_memory_byte_effects_.push_back({0x0e83,*callback_copy_segment_,*callback_copy_si_,updated});
    callback_register_effects_.push_back({0x0e85,
        MillenniumDosVideoFunction13McgaCallbackRegister::ax,ax});
    callback_copy_ax_ = ax;
    last_sequence_ = read.sequence;
    callback_next_instruction_ = ax >= *callback_copy_dx_ ? 0x0e83 : 0x0e8b;
}

void MillenniumDosVideoFunction13InterruptSession::execute_mcga_callback_copy_store(
    const std::uint64_t sequence,
    const std::uint16_t instruction_address) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0e8b
        || sequence != last_sequence_ + 1
        || instruction_address != 0x0e8b
        || !callback_copy_ax_ || !callback_copy_cx_ || !callback_copy_si_
        || !callback_copy_di_ || !callback_copy_segment_) {
        throw std::runtime_error("Detached Millennium DOS MCGA callback copy store");
    }
    const auto next_di = static_cast<std::uint16_t>(*callback_copy_di_ + 2U);
    const auto next_si = static_cast<std::uint16_t>(*callback_copy_si_ + 1U);
    const auto next_cx = static_cast<std::uint16_t>(*callback_copy_cx_ - 1U);
    callback_memory_word_effects_.reserve(callback_memory_word_effects_.size() + 1);
    callback_register_effects_.reserve(callback_register_effects_.size() + 3);
    callback_memory_word_effects_.push_back({0x0e8b,*callback_copy_segment_,
        *callback_copy_di_,*callback_copy_ax_});
    callback_register_effects_.push_back({0x0e8b,
        MillenniumDosVideoFunction13McgaCallbackRegister::di,next_di});
    callback_register_effects_.push_back({0x0e8c,
        MillenniumDosVideoFunction13McgaCallbackRegister::si,next_si});
    callback_register_effects_.push_back({0x0e8d,
        MillenniumDosVideoFunction13McgaCallbackRegister::cx,next_cx});
    callback_copy_di_ = next_di;
    callback_copy_si_ = next_si;
    callback_copy_cx_ = next_cx;
    callback_copy_density_.reset();
    last_sequence_ = sequence;
    callback_next_instruction_ = next_cx == 0 ? 0x0e8f : 0x0e71;
}

void MillenniumDosVideoFunction13InterruptSession::observe_mcga_callback_epilogue_stack_read(
    const MillenniumDosVideoFunction13McgaCallbackEpilogueStackRead& read) {
    const auto first = callback_next_instruction_ == 0x0e8f;
    const auto expected_instruction = static_cast<std::uint16_t>(first ? 0x0e8f : 0x0e90);
    if (state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || (callback_next_instruction_ != 0x0e8f && callback_next_instruction_ != 0x0e90)
        || read.sequence != last_sequence_ + 1
        || read.instruction_address != expected_instruction) {
        throw std::runtime_error("Detached Millennium DOS MCGA callback epilogue stack read");
    }
    if (!first && (!callback_epilogue_sp_ || !callback_epilogue_ss_
        || read.ss != *callback_epilogue_ss_
        || read.sp != static_cast<std::uint16_t>(*callback_epilogue_sp_ + 2U))) {
        throw std::runtime_error("Discontinuous Millennium DOS MCGA callback epilogue stack");
    }
    if (first) {
        callback_epilogue_ss_ = read.ss;
        callback_epilogue_sp_ = read.sp;
        callback_epilogue_si_ = read.value;
        callback_register_effects_.push_back({0x0e8f,
            MillenniumDosVideoFunction13McgaCallbackRegister::si,read.value});
        callback_next_instruction_ = 0x0e90;
    } else {
        callback_epilogue_ds_ = read.value;
        callback_register_effects_.push_back({0x0e90,
            MillenniumDosVideoFunction13McgaCallbackRegister::ds,read.value});
        callback_next_instruction_ = 0x0e91;
    }
    callback_stack_effects_.push_back({read.instruction_address,read.ss,read.sp,read.value});
    last_sequence_ = read.sequence;
}

void MillenniumDosVideoFunction13InterruptSession::observe_mcga_callback_epilogue_descriptor(
    const MillenniumDosVideoFunction13McgaCallbackRead& read) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0e91
        || !callback_epilogue_si_ || !callback_epilogue_ds_
        || read.sequence != last_sequence_ + 1
        || read.instruction_address != 0x0e91
        || read.segment != *callback_epilogue_ds_
        || read.offset != static_cast<std::uint16_t>(*callback_epilogue_si_ + 0x000aU)
        || read.width != MillenniumDosVideoFunction13McgaCallbackReadWidth::word) {
        throw std::runtime_error("Detached Millennium DOS MCGA callback descriptor decrement");
    }
    const auto decremented = static_cast<std::uint16_t>(read.value - 1U);
    callback_reads_.push_back(read);
    callback_memory_word_effects_.push_back({0x0e91,read.segment,read.offset,decremented});
    callback_next_instruction_ = decremented == 0 ? 0x0e96 : 0x0e99;
    last_sequence_ = read.sequence;
}

void MillenniumDosVideoFunction13InterruptSession::execute_mcga_callback_epilogue_clear(
    const std::uint64_t sequence, const std::uint16_t instruction_address) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0e96 || sequence != last_sequence_ + 1
        || instruction_address != 0x0e96 || !callback_epilogue_si_ || !callback_epilogue_ds_) {
        throw std::runtime_error("Detached Millennium DOS MCGA callback descriptor clear");
    }
    callback_memory_byte_effects_.push_back({0x0e96,*callback_epilogue_ds_,*callback_epilogue_si_,0});
    last_sequence_ = sequence;
    callback_next_instruction_ = 0x0e99;
}

void MillenniumDosVideoFunction13InterruptSession::execute_mcga_callback_epilogue_jump(
    const std::uint64_t sequence, const std::uint16_t instruction_address) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0e99 || sequence != last_sequence_ + 1
        || instruction_address != 0x0e99) {
        throw std::runtime_error("Detached Millennium DOS MCGA callback epilogue jump");
    }
    last_sequence_ = sequence;
    callback_next_instruction_ = 0x0d6a;
}

void MillenniumDosVideoFunction13InterruptSession::observe_mcga_callback_palette_descriptor_count(
    const MillenniumDosVideoFunction13McgaCallbackRead& read) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0d6a
        || !callback_epilogue_si_ || !callback_epilogue_ds_
        || read.sequence != last_sequence_ + 1 || read.instruction_address != 0x0d6a
        || read.segment != *callback_epilogue_ds_
        || read.offset != static_cast<std::uint16_t>(*callback_epilogue_si_ + 2U)
        || read.width != MillenniumDosVideoFunction13McgaCallbackReadWidth::byte
        || read.value > 0x00ff || !callback_copy_cx_ || *callback_copy_cx_ != 0) {
        throw std::runtime_error("Detached Millennium DOS MCGA palette descriptor count");
    }
    callback_reads_.push_back(read);
    callback_copy_cx_ = static_cast<std::uint16_t>(read.value);
    callback_register_effects_.push_back({0x0d6a,
        MillenniumDosVideoFunction13McgaCallbackRegister::cx,*callback_copy_cx_});
    callback_next_instruction_ = 0x0d6d;
    last_sequence_ = read.sequence;
}

void MillenniumDosVideoFunction13InterruptSession::observe_mcga_callback_palette_descriptor_index(
    const MillenniumDosVideoFunction13McgaCallbackRead& read) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0d6d
        || !callback_epilogue_si_ || !callback_epilogue_ds_ || !callback_copy_cx_
        || read.sequence != last_sequence_ + 1 || read.instruction_address != 0x0d6d
        || read.segment != *callback_epilogue_ds_
        || read.offset != static_cast<std::uint16_t>(*callback_epilogue_si_ + 3U)
        || read.width != MillenniumDosVideoFunction13McgaCallbackReadWidth::byte
        || read.value > 0x00ff) {
        throw std::runtime_error("Detached Millennium DOS MCGA palette descriptor index");
    }
    const auto incremented = static_cast<std::uint8_t>(read.value + 1U);
    callback_reads_.push_back(read);
    callback_memory_byte_effects_.push_back({0x0d6d,read.segment,read.offset,incremented});
    callback_next_instruction_ = incremented > static_cast<std::uint8_t>(*callback_copy_cx_)
        ? 0x0d75 : 0x0d79;
    last_sequence_ = read.sequence;
}

void MillenniumDosVideoFunction13InterruptSession::execute_mcga_callback_palette_descriptor_clear(
    const std::uint64_t sequence, const std::uint16_t instruction_address) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0d75 || sequence != last_sequence_ + 1
        || instruction_address != 0x0d75 || !callback_epilogue_si_ || !callback_epilogue_ds_) {
        throw std::runtime_error("Detached Millennium DOS MCGA palette descriptor clear");
    }
    callback_memory_byte_effects_.push_back({0x0d75,*callback_epilogue_ds_,
        static_cast<std::uint16_t>(*callback_epilogue_si_ + 3U),0});
    callback_next_instruction_ = 0x0d79;
    last_sequence_ = sequence;
}

void MillenniumDosVideoFunction13InterruptSession::execute_mcga_callback_palette_prefix(
    const std::uint64_t sequence, const std::uint16_t instruction_address) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0d79 || sequence != last_sequence_ + 1
        || instruction_address != 0x0d79 || !callback_copy_cx_) {
        throw std::runtime_error("Detached Millennium DOS MCGA palette prefix");
    }
    const auto cx = static_cast<std::uint16_t>(static_cast<std::uint8_t>(*callback_copy_cx_) + 1U);
    callback_register_effects_.push_back({0x0d79,
        MillenniumDosVideoFunction13McgaCallbackRegister::cx,cx});
    callback_register_effects_.push_back({0x0d7f,
        MillenniumDosVideoFunction13McgaCallbackRegister::dx,0x03c8});
    callback_copy_cx_ = cx;
    last_sequence_ = sequence;
    callback_next_instruction_ = 0x0d7c;
}

void MillenniumDosVideoFunction13InterruptSession::observe_mcga_callback_palette_source_index(
    const MillenniumDosVideoFunction13McgaCallbackRead& read) {
    if (state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0d7c || !callback_epilogue_si_
        || !callback_epilogue_ds_ || read.sequence != last_sequence_ + 1
        || read.instruction_address != 0x0d7c || read.segment != *callback_epilogue_ds_
        || read.offset != static_cast<std::uint16_t>(*callback_epilogue_si_ + 1U)
        || read.width != MillenniumDosVideoFunction13McgaCallbackReadWidth::byte
        || read.value > 0x00ff) {
        throw std::runtime_error("Detached Millennium DOS MCGA palette source index");
    }
    callback_reads_.push_back(read);
    callback_register_effects_.push_back({0x0d7c,
        MillenniumDosVideoFunction13McgaCallbackRegister::al,read.value});
    last_sequence_ = read.sequence;
    callback_next_instruction_ = 0x0d82;
}

void MillenniumDosVideoFunction13InterruptSession::execute_mcga_callback_palette_out(
    const std::uint64_t sequence, const std::uint16_t instruction_address) {
    if (kind_ != MillenniumDosVideoDriverKind::mcga
        || state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0d82 || sequence != last_sequence_ + 1
        || instruction_address != 0x0d82 || callback_register_effects_.empty()
        || callback_register_effects_.back().instruction_address != 0x0d7c
        || callback_register_effects_.back().reg != MillenniumDosVideoFunction13McgaCallbackRegister::al) {
        throw std::runtime_error("Detached Millennium DOS MCGA palette port write");
    }
    callback_port_writes_.push_back({sequence,0x0d82,0x03c8,
        static_cast<std::uint8_t>(callback_register_effects_.back().value)});
    last_sequence_ = sequence;
    callback_next_instruction_ = 0x0d94;
}

void MillenniumDosVideoFunction13InterruptSession::observe_mcga_callback_palette_retrace(
    const MillenniumDosVideoFunction13PortRead& read) {
    if (kind_ != MillenniumDosVideoDriverKind::mcga
        || state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0d94 || read.sequence != last_sequence_ + 1
        || read.instruction_address != 0x0d94 || read.port_address != 0x03da) {
        throw std::runtime_error("Detached Millennium DOS MCGA palette retrace observation");
    }
    callback_port_reads_.push_back(read);
    last_sequence_ = read.sequence;
    callback_next_instruction_ = (read.value & 1U) == 0 ? 0x0d94 : 0x0d99;
    if (callback_next_instruction_ == 0x0d99) {
        callback_register_effects_.push_back({0x0d99,
            MillenniumDosVideoFunction13McgaCallbackRegister::dx,0x03c9});
        callback_palette_cx_ = callback_copy_cx_.value_or(0);
        callback_palette_component_ = 0;
        callback_palette_source_ds_.reset();
        callback_palette_source_si_.reset();
        callback_next_instruction_ = 0x0d87;
    }
}

void MillenniumDosVideoFunction13InterruptSession::observe_mcga_callback_palette_source_pointer(
    const MillenniumDosVideoFunction13McgaCallbackFarPointerRead& read) {
    if (kind_ != MillenniumDosVideoDriverKind::mcga
        || state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0d87 || read.sequence != last_sequence_ + 1
        || read.instruction_address != 0x0d87 || read.source_segment != driver_segment_
        || read.source_offset != 0x0d1e) {
        throw std::runtime_error("Detached Millennium DOS MCGA palette source pointer");
    }
    const auto source_index = std::find_if(callback_register_effects_.rbegin(),
        callback_register_effects_.rend(), [](const auto& effect) {
            return effect.instruction_address == 0x0d7c
                && effect.reg == MillenniumDosVideoFunction13McgaCallbackRegister::al;
        });
    if (source_index == callback_register_effects_.rend()) {
        throw std::runtime_error("Missing Millennium DOS MCGA palette source index");
    }
    const auto index = static_cast<std::uint8_t>(source_index->value);
    callback_far_pointer_reads_.push_back(read);
    callback_palette_source_ds_ = read.value_segment;
    callback_palette_source_si_ = read.value_offset;
    callback_register_effects_.push_back({0x0d87,
        MillenniumDosVideoFunction13McgaCallbackRegister::ds,read.value_segment});
    callback_register_effects_.push_back({0x0d87,
        MillenniumDosVideoFunction13McgaCallbackRegister::si,read.value_offset});
    const auto byte_offset = static_cast<std::uint16_t>(3U * index);
    callback_palette_source_si_ = static_cast<std::uint16_t>(*callback_palette_source_si_ + byte_offset);
    callback_register_effects_.push_back({0x0d8c,
        MillenniumDosVideoFunction13McgaCallbackRegister::si,*callback_palette_source_si_});
    last_sequence_ = read.sequence;
    callback_next_instruction_ = 0x0d9b;
}

void MillenniumDosVideoFunction13InterruptSession::observe_mcga_callback_palette_source_byte(
    const MillenniumDosVideoFunction13McgaCallbackRead& read) {
    const auto expected_instruction = static_cast<std::uint16_t>(
        0x0d9b + callback_palette_component_ * 2U);
    const auto expected_offset = callback_palette_source_si_.value_or(0);
    if (kind_ != MillenniumDosVideoDriverKind::mcga
        || state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != expected_instruction || !callback_palette_source_ds_
        || !callback_palette_source_si_ || !callback_palette_cx_ || *callback_palette_cx_ == 0
        || read.sequence != last_sequence_ + 1 || read.instruction_address != expected_instruction
        || read.segment != *callback_palette_source_ds_ || read.offset != expected_offset
        || read.width != MillenniumDosVideoFunction13McgaCallbackReadWidth::byte
        || read.value > 0x00ff) {
        throw std::runtime_error("Detached Millennium DOS MCGA palette source byte");
    }
    callback_reads_.push_back(read);
    callback_register_effects_.push_back({expected_instruction,
        MillenniumDosVideoFunction13McgaCallbackRegister::al,read.value});
    *callback_palette_source_si_ = static_cast<std::uint16_t>(*callback_palette_source_si_ + 1U);
    callback_register_effects_.push_back({expected_instruction,
        MillenniumDosVideoFunction13McgaCallbackRegister::si,*callback_palette_source_si_});
    last_sequence_ = read.sequence;
    callback_next_instruction_ = static_cast<std::uint16_t>(expected_instruction + 1U);
}

void MillenniumDosVideoFunction13InterruptSession::execute_mcga_callback_palette_data_out(
    const std::uint64_t sequence, const std::uint16_t instruction_address) {
    const auto source_byte = std::find_if(callback_register_effects_.rbegin(),
        callback_register_effects_.rend(), [instruction_address](const auto& effect) {
            return static_cast<std::uint16_t>(effect.instruction_address + 1U) == instruction_address
                && effect.reg == MillenniumDosVideoFunction13McgaCallbackRegister::al;
        });
    if (kind_ != MillenniumDosVideoDriverKind::mcga
        || state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != instruction_address || sequence != last_sequence_ + 1
        || (instruction_address != 0x0d9c && instruction_address != 0x0d9e && instruction_address != 0x0da0)
        || source_byte == callback_register_effects_.rend()) {
        throw std::runtime_error("Detached Millennium DOS MCGA palette data write");
    }
    callback_port_writes_.push_back({sequence,instruction_address,0x03c9,
        static_cast<std::uint8_t>(source_byte->value)});
    last_sequence_ = sequence;
    ++callback_palette_component_;
    if (callback_palette_component_ < 3) {
        callback_next_instruction_ = static_cast<std::uint16_t>(instruction_address + 1U);
    } else {
        callback_palette_component_ = 0;
        callback_next_instruction_ = 0x0da1;
    }
}

void MillenniumDosVideoFunction13InterruptSession::execute_mcga_callback_palette_loop(
    const std::uint64_t sequence, const std::uint16_t instruction_address) {
    if (kind_ != MillenniumDosVideoDriverKind::mcga
        || state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0da1 || sequence != last_sequence_ + 1
        || instruction_address != 0x0da1 || !callback_palette_cx_ || *callback_palette_cx_ == 0) {
        throw std::runtime_error("Detached Millennium DOS MCGA palette LOOP");
    }
    --*callback_palette_cx_;
    callback_register_effects_.push_back({0x0da1,
        MillenniumDosVideoFunction13McgaCallbackRegister::cx,*callback_palette_cx_});
    last_sequence_ = sequence;
    callback_next_instruction_ = *callback_palette_cx_ == 0 ? 0x0da3 : 0x0d9b;
}

void MillenniumDosVideoFunction13InterruptSession::observe_mcga_callback_palette_stack_read(
    const MillenniumDosVideoFunction13McgaCallbackPaletteStackRead& read) {
    const auto first = callback_next_instruction_ == 0x0da3;
    const auto expected_sp = callback_palette_sp_
        ? static_cast<std::uint16_t>(*callback_palette_sp_ + 2U) : read.sp;
    if (kind_ != MillenniumDosVideoDriverKind::mcga
        || state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || (!first && callback_next_instruction_ != 0x0da4)
        || read.sequence != last_sequence_ + 1
        || read.instruction_address != (first ? 0x0da3 : 0x0da4)
        || (callback_palette_ss_ && read.ss != *callback_palette_ss_)
        || (callback_palette_sp_ && read.sp != expected_sp)) {
        throw std::runtime_error("Detached Millennium DOS MCGA palette stack restore");
    }
    if (!callback_palette_ss_) {
        callback_palette_ss_ = read.ss;
        callback_palette_sp_ = read.sp;
    } else {
        callback_palette_sp_ = read.sp;
    }
    callback_stack_effects_.push_back({read.instruction_address,read.ss,read.sp,read.value});
    if (first) {
        callback_palette_si_ = read.value;
        callback_register_effects_.push_back({0x0da3,
            MillenniumDosVideoFunction13McgaCallbackRegister::si,read.value});
        callback_next_instruction_ = 0x0da4;
    } else {
        callback_palette_ds_ = read.value;
        callback_register_effects_.push_back({0x0da4,
            MillenniumDosVideoFunction13McgaCallbackRegister::ds,read.value});
        callback_next_instruction_ = 0x0da5;
    }
    last_sequence_ = read.sequence;
}

void MillenniumDosVideoFunction13InterruptSession::observe_mcga_callback_palette_descriptor_word(
    const MillenniumDosVideoFunction13McgaCallbackRead& read) {
    if (kind_ != MillenniumDosVideoDriverKind::mcga
        || state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0da5 || !callback_palette_ds_ || !callback_palette_si_
        || read.sequence != last_sequence_ + 1 || read.instruction_address != 0x0da5
        || read.segment != *callback_palette_ds_
        || read.offset != static_cast<std::uint16_t>(*callback_palette_si_ + 4U)
        || read.width != MillenniumDosVideoFunction13McgaCallbackReadWidth::word) {
        throw std::runtime_error("Detached Millennium DOS MCGA palette descriptor word");
    }
    callback_reads_.push_back(read);
    const auto incremented = static_cast<std::uint16_t>(read.value + 1U);
    callback_register_effects_.push_back({0x0da5,
        MillenniumDosVideoFunction13McgaCallbackRegister::ax,incremented});
    callback_memory_word_effects_.push_back({0x0da9,read.segment,
        static_cast<std::uint16_t>(*callback_palette_si_ + 8U),incremented});
    last_sequence_ = read.sequence;
    callback_next_instruction_ = 0x0dac;
}

void MillenniumDosVideoFunction13InterruptSession::observe_mcga_callback_palette_loop_count_stack(
    const MillenniumDosVideoFunction13McgaCallbackPaletteStackRead& read) {
    if (kind_ != MillenniumDosVideoDriverKind::mcga
        || state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0dac || !callback_palette_ss_ || !callback_palette_sp_
        || read.sequence != last_sequence_ + 1 || read.instruction_address != 0x0dac
        || read.ss != *callback_palette_ss_
        || read.sp != static_cast<std::uint16_t>(*callback_palette_sp_ + 2U)) {
        throw std::runtime_error("Detached Millennium DOS MCGA palette outer count");
    }
    callback_palette_sp_ = read.sp;
    callback_palette_outer_cx_ = read.value;
    callback_stack_effects_.push_back({0x0dac,read.ss,read.sp,read.value});
    callback_register_effects_.push_back({0x0dac,
        MillenniumDosVideoFunction13McgaCallbackRegister::cx,read.value});
    last_sequence_ = read.sequence;
    callback_next_instruction_ = 0x0dad;
}

void MillenniumDosVideoFunction13InterruptSession::execute_mcga_callback_palette_descriptor_loop(
    const std::uint64_t sequence, const std::uint16_t instruction_address) {
    if (kind_ != MillenniumDosVideoDriverKind::mcga
        || state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_next_instruction_ != 0x0dad || sequence != last_sequence_ + 1
        || instruction_address != 0x0dad || !callback_palette_ds_ || !callback_palette_si_
        || !callback_palette_outer_cx_) {
        throw std::runtime_error("Detached Millennium DOS MCGA palette descriptor loop");
    }
    callback_memory_word_effects_.push_back({0x0dad,*callback_palette_ds_,
        static_cast<std::uint16_t>(*callback_palette_si_ + 8U),
        static_cast<std::uint16_t>(callback_memory_word_effects_.back().value - 1U)});
    callback_register_effects_.push_back({0x0db0,
        MillenniumDosVideoFunction13McgaCallbackRegister::si,
        static_cast<std::uint16_t>(*callback_palette_si_ + 0x0cU)});
    callback_palette_si_ = static_cast<std::uint16_t>(*callback_palette_si_ + 0x0cU);
    --*callback_palette_outer_cx_;
    callback_register_effects_.push_back({0x0db3,
        MillenniumDosVideoFunction13McgaCallbackRegister::cx,*callback_palette_outer_cx_});
    last_sequence_ = sequence;
    callback_next_instruction_ = *callback_palette_outer_cx_ == 0 ? 0x0db5 : 0x0d4e;
}

void MillenniumDosVideoFunction13InterruptSession::observe_mcga_callback_return_stack_read(
    const MillenniumDosVideoFunction13McgaCallbackReturnStackRead& read) {
    constexpr std::array registers{
        MillenniumDosVideoFunction13McgaCallbackRegister::bp,
        MillenniumDosVideoFunction13McgaCallbackRegister::bx,
        MillenniumDosVideoFunction13McgaCallbackRegister::ax,
        MillenniumDosVideoFunction13McgaCallbackRegister::cx,
        MillenniumDosVideoFunction13McgaCallbackRegister::dx,
        MillenniumDosVideoFunction13McgaCallbackRegister::si,
        MillenniumDosVideoFunction13McgaCallbackRegister::di,
        MillenniumDosVideoFunction13McgaCallbackRegister::es,
        MillenniumDosVideoFunction13McgaCallbackRegister::ds};
    const auto index = callback_return_pop_count_;
    if (kind_ != MillenniumDosVideoDriverKind::mcga
        || state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || index >= registers.size()
        || callback_next_instruction_ != static_cast<std::uint16_t>(0x0db5U + index)
        || read.sequence != last_sequence_ + 1
        || read.instruction_address != callback_next_instruction_
        || !callback_palette_ss_ || !callback_palette_sp_
        || read.ss != *callback_palette_ss_
        || read.sp != static_cast<std::uint16_t>(*callback_palette_sp_ + 2U)) {
        throw std::runtime_error("Detached Millennium DOS MCGA callback register restore");
    }
    callback_palette_sp_ = read.sp;
    callback_stack_effects_.push_back({read.instruction_address,read.ss,read.sp,read.value});
    callback_register_effects_.push_back({read.instruction_address,registers[index],read.value});
    ++callback_return_pop_count_;
    last_sequence_ = read.sequence;
    callback_next_instruction_ = static_cast<std::uint16_t>(0x0db5U + callback_return_pop_count_);
}

void MillenniumDosVideoFunction13InterruptSession::execute_mcga_callback_return_flag_clear(
    const std::uint64_t sequence, const std::uint16_t instruction_address) {
    if (kind_ != MillenniumDosVideoDriverKind::mcga
        || state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || callback_return_pop_count_ != 9 || callback_next_instruction_ != 0x0dbe
        || sequence != last_sequence_ + 1 || instruction_address != 0x0dbe) {
        throw std::runtime_error("Detached Millennium DOS MCGA callback flag clear");
    }
    callback_driver_effects_.push_back({0x0dbe,driver_segment_,0x01e4,0});
    callback_driver_effects_.push_back({0x0dc4,driver_segment_,0x01e5,0});
    last_sequence_ = sequence;
    callback_next_instruction_ = 0x0dca;
}

void MillenniumDosVideoFunction13InterruptSession::execute_mcga_callback_return(
    const std::uint64_t sequence, const std::uint16_t instruction_address) {
    const auto palette_return = instruction_address == 0x0dca
        && callback_next_instruction_ == 0x0dca && callback_return_pop_count_ == 9;
    const auto counter_return = instruction_address == 0x0d10
        && callback_next_instruction_ == 0x0d10 && callback_return_pop_count_ == 0;
    if (kind_ != MillenniumDosVideoDriverKind::mcga
        || state_ != MillenniumDosVideoFunction13InterruptState::callback_local_boundary
        || (!palette_return && !counter_return) || sequence != last_sequence_ + 1) {
        throw std::runtime_error("Detached Millennium DOS MCGA callback RET");
    }
    callback_iret_pending_ = true;
    last_sequence_ = sequence;
    callback_next_instruction_ = 0x0024;
    state_ = MillenniumDosVideoFunction13InterruptState::iret_boundary;
}

void MillenniumDosVideoFunction13InterruptSession::execute_iret(
    const std::uint64_t sequence, const std::uint16_t instruction_address) {
    const auto iret_address = static_cast<std::uint16_t>(kind_ == MillenniumDosVideoDriverKind::ega640
        ? 0x0012 : (callback_iret_pending_ ? 0x0024 : 0x0020));
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
