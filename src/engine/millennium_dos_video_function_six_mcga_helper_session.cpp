#include "engine/millennium_dos_video_function_six_mcga_helper_session.hpp"

#include "data/millennium_dos_video_driver.hpp"
#include "data/sha256.hpp"

#include <stdexcept>
#include <string>

namespace eon {

MillenniumDosVideoFunctionSixMcgaHelperSession::MillenniumDosVideoFunctionSixMcgaHelperSession(
    const std::span<const std::uint8_t> english_mcga_driver,
    const std::uint16_t ax, const std::uint16_t bx, const std::uint16_t dx,
    const MillenniumDosVideoDataSegment ds,
    const MillenniumDosVideoFunctionSixMcgaHelperStackSegment ss,
    const std::uint16_t sp)
    : initial_ax_(ax), initial_bx_(bx), initial_dx_(dx), ax_(ax), bx_(bx), dx_(dx),
      ds_(ds), ss_(ss), initial_sp_(sp), sp_(sp) {
    constexpr std::size_t offset = 0x0666;
    constexpr std::size_t span_size = 0x1b;
    if (english_mcga_driver.size() != 4'366
        || to_hex(sha256(english_mcga_driver))
            != "bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228"
        || english_mcga_driver.size() - offset < span_size
        || to_hex(sha256(english_mcga_driver.subspan(offset, span_size)))
            != "31abe7cebe0a9fca4b7efca96e5d76636b878c28e59999e3b473df21e0dcf182") {
        throw std::runtime_error("Unsupported English MCGA function-$06 helper span");
    }
    const auto profile = parse_millennium_dos_video_driver(
        english_mcga_driver, MillenniumDosVideoDriverKind::mcga);
    if (profile.function_six_address != 0x0705) {
        throw std::runtime_error("Unexpected MCGA function-$06 entry");
    }
}

void MillenniumDosVideoFunctionSixMcgaHelperSession::observe_threshold(
    const MillenniumDosVideoFunctionSixMcgaHelperDataByteRead& read) {
    if (state_ != MillenniumDosVideoFunctionSixMcgaHelperState::awaiting_threshold
        || read.sequence != last_sequence_ + 1 || read.instruction_address != 0x0666
        || read.ds != ds_ || read.offset != 0x00ac) {
        throw std::runtime_error("MCGA function-$06 helper threshold read mismatch");
    }
    last_sequence_ = read.sequence;
    if ((ax_ & 0x00ffU) >= read.value) {
        // CMP / JNC / STC / RET. STC supplies the caller-visible carry.
        carry_ = true;
        state_ = MillenniumDosVideoFunctionSixMcgaHelperState::awaiting_ret;
        return;
    }

    // PUSH BX uses a wrapping 16-bit SP; the caller supplies the later SS:SP
    // read rather than this session pretending to own/emulate stack memory.
    sp_ = static_cast<std::uint16_t>(sp_ - 2U);
    stack_writes_.push_back({0x066c, ss_, sp_, bx_});

    // CBW and two 16-bit SHL instructions, then MOV BX,AX.
    const auto al = static_cast<std::uint8_t>(ax_ & 0x00ffU);
    ax_ = (al & 0x80U) != 0
        ? static_cast<std::uint16_t>(0xff00U | al)
        : static_cast<std::uint16_t>(al);
    ax_ = static_cast<std::uint16_t>(ax_ << 1U);
    ax_ = static_cast<std::uint16_t>(ax_ << 1U);
    bx_ = ax_;

    const auto sum = static_cast<std::uint32_t>(bx_) + 0x0080U;
    bx_ = static_cast<std::uint16_t>(sum);
    carry_ = sum > 0xffffU;
    table_offset_ = bx_;
    state_ = MillenniumDosVideoFunctionSixMcgaHelperState::awaiting_table_word_zero;
}

void MillenniumDosVideoFunctionSixMcgaHelperSession::observe_table_word(
    const MillenniumDosVideoFunctionSixMcgaHelperTableWordRead& read) {
    const bool first = state_ == MillenniumDosVideoFunctionSixMcgaHelperState::awaiting_table_word_zero;
    const bool second = state_ == MillenniumDosVideoFunctionSixMcgaHelperState::awaiting_table_word_two;
    if ((!first && !second) || read.sequence != last_sequence_ + 1
        || read.instruction_address != (first ? 0x0678 : 0x067a)
        || read.ds != ds_
        || read.offset != (first ? table_offset_
                                  : static_cast<std::uint16_t>(table_offset_ + 2U))) {
        throw std::runtime_error("MCGA function-$06 helper table word read mismatch");
    }
    last_sequence_ = read.sequence;
    table_reads_.push_back(read);
    if (first) {
        ax_ = read.value;
        state_ = MillenniumDosVideoFunctionSixMcgaHelperState::awaiting_table_word_two;
    } else {
        dx_ = read.value;
        state_ = MillenniumDosVideoFunctionSixMcgaHelperState::awaiting_bx_pop;
    }
}

void MillenniumDosVideoFunctionSixMcgaHelperSession::observe_stack_word(
    const MillenniumDosVideoFunctionSixMcgaHelperStackWordRead& read) {
    const bool pop = state_ == MillenniumDosVideoFunctionSixMcgaHelperState::awaiting_bx_pop;
    const bool ret = state_ == MillenniumDosVideoFunctionSixMcgaHelperState::awaiting_ret;
    if ((!pop && !ret) || read.sequence != last_sequence_ + 1 || read.ss != ss_
        || read.offset != sp_ || read.instruction_address != (pop ? 0x067d : (table_reads_.empty() ? 0x0680 : 0x067e))) {
        throw std::runtime_error("MCGA function-$06 helper stack read mismatch at instruction $"
            + std::to_string(read.instruction_address) + " expected offset $"
            + std::to_string(sp_) + " observed $" + std::to_string(read.offset));
    }
    if (pop && read.value != initial_bx_) {
        throw std::runtime_error("MCGA function-$06 helper BX pop value mismatch");
    }
    last_sequence_ = read.sequence;
    stack_reads_.push_back(read);
    if (pop) {
        bx_ = read.value;
        sp_ = static_cast<std::uint16_t>(sp_ + 2U);
        state_ = MillenniumDosVideoFunctionSixMcgaHelperState::awaiting_ret;
        return;
    }
    const auto return_ip = read.value;
    sp_ = static_cast<std::uint16_t>(sp_ + 2U);
    finish(return_ip);
}

void MillenniumDosVideoFunctionSixMcgaHelperSession::finish(const std::uint16_t return_ip) {
    outcome_ = MillenniumDosVideoFunctionSixMcgaHelperOutcome{
        initial_ax_, initial_bx_, initial_dx_, ds_, ss_, initial_sp_,
        ax_, bx_, dx_, sp_, carry_, return_ip,
        table_reads_.empty() ? std::nullopt : std::optional<std::uint16_t>{table_offset_},
        table_reads_, stack_writes_, stack_reads_};
    state_ = MillenniumDosVideoFunctionSixMcgaHelperState::complete;
}

} // namespace eon
