#pragma once

#include "data/millennium_dos_video_driver.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace eon {

enum class MillenniumDosVideoDriverLoadState {
    awaiting_open_result,
    awaiting_seek_end_result,
    awaiting_allocation_result,
    awaiting_seek_start_result,
    awaiting_read_result,
    awaiting_close_result,
    set_vector_request_boundary,
    set_vector_result_boundary,
    set_vector_result_observed,
};

enum class MillenniumDosVideoSelectorSource { command_tail, hardware_detector };
struct MillenniumDosVideoSelectorObservation {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0x0204;
    MillenniumDosVideoSelectorSource source = MillenniumDosVideoSelectorSource::command_tail;
    char command = 0;
    std::uint8_t raw_detector_result = 0;
};

enum class MillenniumDosVideoDriverLoadBoundaryKind { dos_result, interrupt_request, dos_result_observation };

struct MillenniumDosVideoDriverLoadBoundary {
    MillenniumDosVideoDriverLoadBoundaryKind kind = MillenniumDosVideoDriverLoadBoundaryKind::dos_result;
    std::uint16_t instruction_address = 0;
    std::uint16_t ax = 0;
    std::uint16_t dx = 0;
    std::uint16_t cx = 0;
    std::uint8_t interrupt_number = 0x21;
    bool carry = false;
    bool result_observed = false;
    constexpr bool operator==(const MillenniumDosVideoDriverLoadBoundary&) const = default;
};

struct MillenniumDosVideoDriverMemoryEffect {
    std::uint16_t instruction_address = 0;
    std::uint16_t segment = 0;
    std::uint16_t offset = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoDriverMemoryEffect&) const = default;
};

// MILL.COM's hash-locked English video loader. DOS results remain explicit
// observations; SetVect is exposed as a request/result boundary only and does
// not establish that an INT 91h handler was installed or dispatched.
class MillenniumDosVideoDriverLoadSession {
public:
    MillenniumDosVideoDriverLoadSession(std::span<const std::uint8_t> titles_executable,
        std::span<const std::uint8_t> mill_com,
        std::span<const std::uint8_t> selected_driver, MillenniumDosVideoDriverKind kind,
        MillenniumDosVideoSelectorSource selector_source, std::uint8_t selector,
        char observed_command, std::uint8_t raw_detector_result);

    [[nodiscard]] MillenniumDosVideoDriverLoadState state() const { return state_; }
    [[nodiscard]] MillenniumDosVideoDriverLoadBoundary boundary() const;
    [[nodiscard]] const MillenniumDosVideoDriverProfile& driver() const { return driver_; }
    [[nodiscard]] const char* filename() const { return filename_; }
    [[nodiscard]] const std::vector<MillenniumDosVideoDriverMemoryEffect>& memory_effects() const {
        return memory_effects_;
    }
    [[nodiscard]] std::uint16_t file_handle() const { return file_handle_; }
    [[nodiscard]] std::uint16_t load_segment() const { return load_segment_; }
    [[nodiscard]] bool set_vector_carry() const { return set_vector_carry_; }
    [[nodiscard]] std::uint16_t set_vector_ax() const { return set_vector_ax_; }
    [[nodiscard]] MillenniumDosVideoSelectorSource selector_source() const { return selector_source_; }
    [[nodiscard]] std::uint8_t selector() const { return selector_; }
    [[nodiscard]] char observed_command() const { return observed_command_; }
    [[nodiscard]] std::uint8_t raw_detector_result() const { return raw_detector_result_; }

    void observe_open_result(std::uint16_t instruction, bool carry, std::uint16_t ax);
    void observe_seek_end_result(std::uint16_t instruction, bool carry,
        std::uint16_t bx, std::uint16_t ax, std::uint16_t dx);
    void observe_allocation_result(std::uint16_t instruction, bool carry, std::uint16_t ax);
    void observe_seek_start_result(std::uint16_t instruction, bool carry,
        std::uint16_t bx, std::uint16_t ax, std::uint16_t dx);
    void observe_read_result(std::uint16_t instruction, bool carry,
        std::uint16_t bx, std::uint16_t ax);
    void observe_close_result(std::uint16_t instruction, bool carry, std::uint16_t bx);
    void observe_set_vector_request(std::uint16_t instruction, std::uint8_t interrupt,
        std::uint16_t ax, std::uint16_t dx);
    void observe_set_vector_dos_result(std::uint16_t instruction, bool carry, std::uint16_t ax);

private:
    MillenniumDosVideoSelectorSource selector_source_ = MillenniumDosVideoSelectorSource::command_tail;
    std::uint8_t selector_ = 0;
    char observed_command_ = 0;
    std::uint8_t raw_detector_result_ = 0;
    MillenniumDosVideoDriverLoadState state_ = MillenniumDosVideoDriverLoadState::awaiting_open_result;
    MillenniumDosVideoDriverProfile driver_;
    std::vector<std::uint8_t> driver_bytes_;
    std::vector<MillenniumDosVideoDriverMemoryEffect> memory_effects_;
    const char* filename_ = nullptr;
    std::uint16_t filename_address_ = 0;
    std::uint16_t file_handle_ = 0;
    std::uint16_t load_segment_ = 0;
    bool set_vector_carry_ = false;
    std::uint16_t set_vector_ax_ = 0;
};

} // namespace eon
