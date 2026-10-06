#include "engine/millennium_dos_video_function_six_clip_session.hpp"

#include "data/sha256.hpp"

#include <stdexcept>

namespace eon {

MillenniumDosVideoFunctionSixClipSession::MillenniumDosVideoFunctionSixClipSession(
    const std::span<const std::uint8_t> english_driver,
    const MillenniumDosVideoDriverKind kind,
    const std::uint16_t descriptor_es,
    const std::uint16_t descriptor_bx)
    : driver_(parse_millennium_dos_video_driver(english_driver, kind)),
      kind_(kind), descriptor_es_(descriptor_es), descriptor_bx_(descriptor_bx),
      state_(kind == MillenniumDosVideoDriverKind::ega640
          ? MillenniumDosVideoFunctionSixClipState::awaiting_width
          : MillenniumDosVideoFunctionSixClipState::awaiting_clip_limit) {
    const bool ega = kind == MillenniumDosVideoDriverKind::ega640;
    const auto offset = static_cast<std::size_t>(driver_.function_six_address);
    constexpr std::size_t span_size = 0x18;
    const auto expected_hash = ega
        ? "2a1d0e9f0ff2244c61ec6ff127fcb83b3375230f2a5175c60a2ad7b2c10e2e20"
        : "2186cda3575ed970bbb56e9a0fade7dd44af32ea4b6edaf22dccc4a209abe54d";
    if (offset > english_driver.size() || english_driver.size() - offset < span_size
        || to_hex(sha256(english_driver.subspan(offset, span_size))) != expected_hash
        || driver_.function_six_address != (ega ? 0x08a6 : 0x0705)) {
        throw std::runtime_error("Unsupported Millennium English function-$06 clip prefix");
    }
    if (ega) {
        // The signed-width branch lands on this one-byte RET immediately
        // before the function target, outside the 24-byte entry span.
        constexpr std::size_t ret_offset = 0x08a5;
        if (ret_offset >= english_driver.size() || english_driver[ret_offset] != 0xc3
            || to_hex(sha256(english_driver.subspan(ret_offset, 1)))
                != "ae3f4619b0413d70d3004b9131c3752153074e45725be13b9a148978895e359e") {
            throw std::runtime_error("Unsupported Millennium EGA function-$06 early RET");
        }
    }
}

std::uint16_t MillenniumDosVideoFunctionSixClipSession::descriptor_field_offset(
    const std::uint16_t field) const {
    return static_cast<std::uint16_t>(descriptor_bx_ + field);
}

std::optional<MillenniumDosVideoFunctionSixClipBoundary>
MillenniumDosVideoFunctionSixClipSession::boundary() const {
    switch (state_) {
    case MillenniumDosVideoFunctionSixClipState::awaiting_width:
        return MillenniumDosVideoFunctionSixClipBoundary{
            last_sequence_ + 1, 0x08a6, descriptor_es_, descriptor_field_offset(0x10)};
    case MillenniumDosVideoFunctionSixClipState::awaiting_clip_limit:
        return MillenniumDosVideoFunctionSixClipBoundary{
            last_sequence_ + 1,
            static_cast<std::uint16_t>(kind_ == MillenniumDosVideoDriverKind::ega640
                ? 0x08b0 : 0x0708),
            descriptor_es_, descriptor_field_offset(0x08)};
    case MillenniumDosVideoFunctionSixClipState::awaiting_mcga_unsigned_width:
        return MillenniumDosVideoFunctionSixClipBoundary{
            last_sequence_ + 1, 0x070c, descriptor_es_, descriptor_field_offset(0x10)};
    case MillenniumDosVideoFunctionSixClipState::awaiting_mcga_signed_width:
        return MillenniumDosVideoFunctionSixClipBoundary{
            last_sequence_ + 1, 0x0716, descriptor_es_, descriptor_field_offset(0x10)};
    case MillenniumDosVideoFunctionSixClipState::prefix_boundary:
    case MillenniumDosVideoFunctionSixClipState::ret_boundary:
        return std::nullopt;
    }
    throw std::runtime_error("Invalid Millennium function-$06 clip state");
}

void MillenniumDosVideoFunctionSixClipSession::finish(
    const MillenniumDosVideoFunctionSixClipEndpoint endpoint,
    const std::uint16_t instruction_address,
    const std::optional<std::uint16_t> clipped_value) {
    outcome_ = MillenniumDosVideoFunctionSixClipOutcome{
        endpoint, instruction_address, clipped_value, reads_, write_};
    state_ = endpoint == MillenniumDosVideoFunctionSixClipEndpoint::ret
        ? MillenniumDosVideoFunctionSixClipState::ret_boundary
        : MillenniumDosVideoFunctionSixClipState::prefix_boundary;
}

void MillenniumDosVideoFunctionSixClipSession::observe_word_read(
    const MillenniumDosVideoFunctionSixWordRead& read) {
    const auto expected = boundary();
    if (!expected || read.sequence != expected->sequence
        || read.instruction_address != expected->instruction_address
        || read.es != expected->es || read.offset != expected->offset) {
        throw std::runtime_error("Millennium function-$06 descriptor read mismatch");
    }
    last_sequence_ = read.sequence;
    reads_.push_back(read);

    const auto signed_less = [](const std::uint16_t left, const std::uint16_t right) {
        return static_cast<std::uint16_t>(left ^ 0x8000U)
            < static_cast<std::uint16_t>(right ^ 0x8000U);
    };
    if (state_ == MillenniumDosVideoFunctionSixClipState::awaiting_width) {
        width_read_ = read;
        if (read.value == 0 || (read.value & 0x8000U) != 0) {
            finish(MillenniumDosVideoFunctionSixClipEndpoint::ret, 0x08a5, std::nullopt);
            return;
        }
        state_ = MillenniumDosVideoFunctionSixClipState::awaiting_clip_limit;
        return;
    }
    if (state_ == MillenniumDosVideoFunctionSixClipState::awaiting_clip_limit) {
        clipped_value_ = static_cast<std::uint16_t>(0x0140U - read.value);
        if (kind_ == MillenniumDosVideoDriverKind::ega640) {
            if (!width_read_) throw std::runtime_error("Missing EGA function-$06 width read");
            if (signed_less(clipped_value_, width_read_->value)) {
                write_ = MillenniumDosVideoFunctionSixWordWrite{
                    0x08ba, descriptor_es_, descriptor_field_offset(0x10),
                    width_read_->value, clipped_value_};
            }
            finish(MillenniumDosVideoFunctionSixClipEndpoint::caller_driven_prefix,
                0x08be, clipped_value_);
            return;
        }
        state_ = MillenniumDosVideoFunctionSixClipState::awaiting_mcga_unsigned_width;
        return;
    }
    if (state_ == MillenniumDosVideoFunctionSixClipState::awaiting_mcga_unsigned_width) {
        // JNC skips the word store for unsigned limit >= width.
        if (clipped_value_ < read.value) {
            write_ = MillenniumDosVideoFunctionSixWordWrite{
                0x0712, descriptor_es_, descriptor_field_offset(0x10),
                read.value, clipped_value_};
        }
        state_ = MillenniumDosVideoFunctionSixClipState::awaiting_mcga_signed_width;
        return;
    }
    if (state_ == MillenniumDosVideoFunctionSixClipState::awaiting_mcga_signed_width) {
        if (read.value == 0 || (read.value & 0x8000U) != 0) {
            finish(MillenniumDosVideoFunctionSixClipEndpoint::ret, 0x079c, clipped_value_);
            return;
        }
        finish(MillenniumDosVideoFunctionSixClipEndpoint::caller_driven_prefix,
            0x071d, clipped_value_);
        return;
    }
    throw std::runtime_error("Millennium function-$06 read arrived after terminal boundary");
}

} // namespace eon
