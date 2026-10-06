#pragma once

#include "data/millennium_dos_video_driver.hpp"
#include "engine/millennium_dos_video_function_six_mcga_descriptor_byte_read.hpp"

#include <cstdint>
#include <span>

namespace eon {

// Keep segment-register identities explicit. In particular, a later CS read
// of the same numeric offset is not interchangeable with this model's DS data.
struct MillenniumDosVideoDataSegment {
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoDataSegment&) const = default;
};

struct MillenniumDosVideoCodeSegment {
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoCodeSegment&) const = default;
};

struct MillenniumDosVideoFunctionSixMcgaTableByteBoundary {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    MillenniumDosVideoDataSegment ds;
    std::uint16_t di = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaTableByteBoundary&) const = default;
};

struct MillenniumDosVideoFunctionSixMcgaTableByteRead {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    MillenniumDosVideoDataSegment ds;
    std::uint16_t di = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaTableByteRead&) const = default;
};

struct MillenniumDosVideoFunctionSixMcgaDataByteStore {
    std::uint16_t instruction_address = 0;
    MillenniumDosVideoDataSegment ds;
    std::uint16_t offset = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaDataByteStore&) const = default;
};

enum class MillenniumDosVideoFunctionSixMcgaTableState {
    awaiting_descriptor_byte,
    awaiting_table_byte,
    complete,
};

// Standalone model of exactly MCGA.BIN [$071d,$072d). It does not assume this
// span is reached from function-$06's preceding clipping code.
class MillenniumDosVideoFunctionSixMcgaTableSession {
public:
    MillenniumDosVideoFunctionSixMcgaTableSession(
        std::span<const std::uint8_t> english_mcga_driver,
        std::uint16_t descriptor_es,
        std::uint16_t descriptor_bx,
        MillenniumDosVideoDataSegment ds);

    [[nodiscard]] MillenniumDosVideoFunctionSixMcgaTableState state() const { return state_; }
    [[nodiscard]] const MillenniumDosVideoFunctionSixMcgaDescriptorByteRead& descriptor_read() const {
        return descriptor_read_;
    }
    [[nodiscard]] const MillenniumDosVideoFunctionSixMcgaTableByteBoundary& table_boundary() const {
        return table_boundary_;
    }
    [[nodiscard]] const MillenniumDosVideoFunctionSixMcgaTableByteRead& table_read() const {
        return table_read_;
    }
    [[nodiscard]] const MillenniumDosVideoFunctionSixMcgaDataByteStore& store() const {
        return store_;
    }

    void observe_descriptor_byte_read(
        const MillenniumDosVideoFunctionSixMcgaDescriptorByteRead& read);
    void observe_table_byte_read(const MillenniumDosVideoFunctionSixMcgaTableByteRead& read);

private:
    std::uint16_t descriptor_es_ = 0;
    std::uint16_t descriptor_offset_ = 0;
    MillenniumDosVideoDataSegment ds_;
    MillenniumDosVideoFunctionSixMcgaTableState state_ =
        MillenniumDosVideoFunctionSixMcgaTableState::awaiting_descriptor_byte;
    std::uint64_t last_sequence_ = 0;
    MillenniumDosVideoFunctionSixMcgaDescriptorByteRead descriptor_read_;
    std::uint16_t di_ = 0;
    MillenniumDosVideoFunctionSixMcgaTableByteBoundary table_boundary_;
    MillenniumDosVideoFunctionSixMcgaTableByteRead table_read_;
    MillenniumDosVideoFunctionSixMcgaDataByteStore store_;
};

} // namespace eon
