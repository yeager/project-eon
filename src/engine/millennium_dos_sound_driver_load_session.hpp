#pragma once

#include "data/millennium_dos_sound_driver.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace eon {

enum class MillenniumDosSoundDriverLoadState {
    awaiting_open_result,
    awaiting_seek_end_result,
    awaiting_allocation_result,
    awaiting_seek_start_result,
    awaiting_read_result,
    awaiting_close_result,
    awaiting_vector_install,
    awaiting_parent_stack,
    title_exec_boundary,
    title_exec_requested,
};

enum class MillenniumDosSoundDriverLoadBoundaryKind {
    dos_result,
    runtime_word,
    interrupt_observation,
    dos_exec,
};

struct MillenniumDosSoundDriverLoadBoundary {
    MillenniumDosSoundDriverLoadBoundaryKind kind =
        MillenniumDosSoundDriverLoadBoundaryKind::dos_result;
    std::uint16_t instruction_address = 0;
    std::uint16_t ax = 0;
    std::uint16_t dx = 0;
    std::uint16_t cx = 0;
    constexpr bool operator==(const MillenniumDosSoundDriverLoadBoundary&) const = default;
};

struct MillenniumDosSoundDriverMemoryEffect {
    std::uint16_t instruction_address = 0;
    std::uint16_t segment = 0;
    std::uint16_t offset = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumDosSoundDriverMemoryEffect&) const = default;
};

struct MillenniumDosSoundDriverRuntimeWordEffect {
    std::uint16_t instruction_address = 0;
    std::uint16_t address = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosSoundDriverRuntimeWordEffect&) const = default;
};
struct MillenniumDosSoundDriverRuntimeByteEffect {
    std::uint16_t instruction_address = 0;
    std::uint16_t address = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumDosSoundDriverRuntimeByteEffect&) const = default;
};

// Exact MILL.COM selected-driver loader from the $0231 call through the first
// TITLES.EXE EXEC request. DOS results are observations; original driver bytes
// are copied only after an exact successful read observation.
class MillenniumDosSoundDriverLoadSession {
public:
    MillenniumDosSoundDriverLoadSession(std::span<const std::uint8_t> mill_com,
        std::span<const std::uint8_t> selected_driver, char selected_character,
        std::uint16_t code_segment);

    [[nodiscard]] MillenniumDosSoundDriverLoadState state() const { return state_; }
    [[nodiscard]] MillenniumDosSoundDriverLoadBoundary boundary() const;
    [[nodiscard]] const MillenniumDosSoundDriverLeaf& driver() const { return driver_; }
    [[nodiscard]] const std::vector<MillenniumDosSoundDriverMemoryEffect>& memory_effects() const {
        return memory_effects_;
    }
    [[nodiscard]] const std::vector<MillenniumDosSoundDriverRuntimeWordEffect>&
    runtime_word_effects() const { return runtime_word_effects_; }
    [[nodiscard]] const std::vector<MillenniumDosSoundDriverRuntimeByteEffect>&
    runtime_byte_effects() const { return runtime_byte_effects_; }
    [[nodiscard]] std::uint16_t file_handle() const { return file_handle_; }
    [[nodiscard]] std::uint16_t load_segment() const { return load_segment_; }

    void observe_open_result(std::uint16_t instruction, bool carry, std::uint16_t ax);
    void observe_seek_end_result(std::uint16_t instruction, bool carry,
        std::uint16_t bx, std::uint16_t ax, std::uint16_t dx);
    void observe_allocation_result(std::uint16_t instruction, bool carry, std::uint16_t ax);
    void observe_seek_start_result(std::uint16_t instruction, bool carry,
        std::uint16_t bx, std::uint16_t ax, std::uint16_t dx);
    void observe_read_result(std::uint16_t instruction, bool carry,
        std::uint16_t bx, std::uint16_t ax);
    void observe_close_result(std::uint16_t instruction, bool carry, std::uint16_t bx);
    void observe_vector_install(std::uint16_t instruction, std::uint16_t ax,
        std::uint16_t dx);
    void observe_parent_stack(std::uint16_t instruction, std::uint16_t address,
        std::uint16_t value);
    void observe_title_exec_request(std::uint16_t instruction, std::uint16_t ax,
        std::uint16_t dx, std::uint16_t parameter_block);

private:
    MillenniumDosSoundDriverLoadState state_ =
        MillenniumDosSoundDriverLoadState::awaiting_open_result;
    MillenniumDosSoundDriverLeaf driver_;
    std::vector<std::uint8_t> driver_bytes_;
    std::vector<MillenniumDosSoundDriverMemoryEffect> memory_effects_;
    std::vector<MillenniumDosSoundDriverRuntimeWordEffect> runtime_word_effects_;
    std::vector<MillenniumDosSoundDriverRuntimeByteEffect> runtime_byte_effects_;
    std::uint16_t code_segment_ = 0;
    std::uint16_t filename_address_ = 0;
    std::uint16_t file_handle_ = 0;
    std::uint16_t load_segment_ = 0;
};

} // namespace eon
