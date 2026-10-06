#pragma once

#include "engine/millennium_dos_video_function_six_mcga_table_session.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace eon {

struct MillenniumDosVideoStackSegment {
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoStackSegment&) const = default;
};

struct MillenniumDosVideoFunctionSixMcgaStackWordWrite {
    std::uint16_t instruction_address = 0;
    MillenniumDosVideoStackSegment ss;
    std::uint16_t offset = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaStackWordWrite&) const = default;
};

struct MillenniumDosVideoFunctionSixMcgaDescriptorPointerWordRead {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    std::uint16_t es = 0;
    std::uint16_t offset = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaDescriptorPointerWordRead&) const = default;
};

struct MillenniumDosVideoFunctionSixMcgaSourceWordRead {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    MillenniumDosVideoDataSegment ds;
    std::uint16_t offset = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaSourceWordRead&) const = default;
};

struct MillenniumDosVideoFunctionSixMcgaStackWordRead {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    MillenniumDosVideoStackSegment ss;
    std::uint16_t offset = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaStackWordRead&) const = default;
};

enum class MillenniumDosVideoFunctionSixMcgaPointerSetupState {
    awaiting_descriptor_pointer_offset,
    awaiting_descriptor_pointer_segment,
    awaiting_source_header_word,
    awaiting_nested_pointer_offset,
    awaiting_nested_pointer_segment,
    awaiting_stack_pop,
    complete,
};

struct MillenniumDosVideoFunctionSixMcgaPointerSetupOutcome {
    MillenniumDosVideoDataSegment initial_ds;
    std::uint16_t initial_sp = 0;
    MillenniumDosVideoFunctionSixMcgaDescriptorPointerWordRead descriptor_offset_read;
    MillenniumDosVideoFunctionSixMcgaDescriptorPointerWordRead descriptor_segment_read;
    MillenniumDosVideoFunctionSixMcgaSourceWordRead source_header_read;
    MillenniumDosVideoFunctionSixMcgaSourceWordRead nested_pointer_offset_read;
    MillenniumDosVideoFunctionSixMcgaSourceWordRead nested_pointer_segment_read;
    MillenniumDosVideoFunctionSixMcgaStackWordRead stack_pop_read;
    std::uint16_t final_ds = 0;
    std::uint16_t si = 0;
    std::uint16_t ax = 0;
    std::uint16_t di = 0;
    std::uint16_t cx = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaPointerSetupOutcome&) const = default;
};

// Standalone hash-bound execution facts for MCGA.BIN [$072d,$073a). This
// session reports typed stack/segment effects only and does not call helper
// $0666 or assume reachability from another function-$06 span.
class MillenniumDosVideoFunctionSixMcgaPointerSetupSession {
public:
    MillenniumDosVideoFunctionSixMcgaPointerSetupSession(
        std::span<const std::uint8_t> english_mcga_driver,
        std::uint16_t descriptor_es,
        std::uint16_t descriptor_bx,
        MillenniumDosVideoDataSegment initial_ds,
        MillenniumDosVideoStackSegment ss,
        std::uint16_t initial_sp);

    [[nodiscard]] MillenniumDosVideoFunctionSixMcgaPointerSetupState state() const { return state_; }
    [[nodiscard]] const MillenniumDosVideoFunctionSixMcgaStackWordWrite& pushed_ds() const { return pushed_ds_; }
    [[nodiscard]] const std::optional<MillenniumDosVideoFunctionSixMcgaPointerSetupOutcome>& outcome() const {
        return outcome_;
    }

    void observe_descriptor_pointer_offset(
        const MillenniumDosVideoFunctionSixMcgaDescriptorPointerWordRead& read);
    void observe_descriptor_pointer_segment(
        const MillenniumDosVideoFunctionSixMcgaDescriptorPointerWordRead& read);
    void observe_source_header_word(const MillenniumDosVideoFunctionSixMcgaSourceWordRead& read);
    void observe_nested_pointer_offset(const MillenniumDosVideoFunctionSixMcgaSourceWordRead& read);
    void observe_nested_pointer_segment(const MillenniumDosVideoFunctionSixMcgaSourceWordRead& read);
    void observe_stack_pop(const MillenniumDosVideoFunctionSixMcgaStackWordRead& read);

private:
    void require_state(MillenniumDosVideoFunctionSixMcgaPointerSetupState expected,
        std::uint64_t sequence, std::uint16_t instruction_address) const;

    std::uint16_t descriptor_es_ = 0;
    std::uint16_t descriptor_bx_ = 0;
    MillenniumDosVideoDataSegment initial_ds_;
    MillenniumDosVideoStackSegment ss_;
    std::uint16_t initial_sp_ = 0;
    std::uint16_t stack_offset_ = 0;
    MillenniumDosVideoFunctionSixMcgaPointerSetupState state_ =
        MillenniumDosVideoFunctionSixMcgaPointerSetupState::awaiting_descriptor_pointer_offset;
    std::uint64_t last_sequence_ = 0;
    std::uint16_t first_pointer_offset_ = 0;
    MillenniumDosVideoDataSegment first_pointer_segment_;
    std::uint16_t header_word_ = 0;
    std::uint16_t nested_pointer_offset_ = 0;
    std::uint16_t nested_pointer_segment_ = 0;
    MillenniumDosVideoFunctionSixMcgaDescriptorPointerWordRead descriptor_offset_read_;
    MillenniumDosVideoFunctionSixMcgaDescriptorPointerWordRead descriptor_segment_read_;
    MillenniumDosVideoFunctionSixMcgaSourceWordRead source_header_read_;
    MillenniumDosVideoFunctionSixMcgaSourceWordRead nested_pointer_offset_read_;
    MillenniumDosVideoFunctionSixMcgaSourceWordRead nested_pointer_segment_read_;
    MillenniumDosVideoFunctionSixMcgaStackWordWrite pushed_ds_;
    std::optional<MillenniumDosVideoFunctionSixMcgaPointerSetupOutcome> outcome_;
};

} // namespace eon
