#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace eon {

enum class MillenniumDosStaticDataLoadState {
    awaiting_loader_entry,
    awaiting_open_return,
    awaiting_read_return,
    awaiting_close_return,
    awaiting_loader_return,
    complete,
    failed,
};

struct MillenniumDosStaticDataLoadBoundary {
    MillenniumDosStaticDataLoadState state =
        MillenniumDosStaticDataLoadState::awaiting_loader_entry;
    std::uint16_t instruction_address = 0;
    std::uint16_t target_address = 0;
    std::uint16_t return_address = 0;
    std::uint16_t code_segment = 0;
    std::uint8_t interrupt_number = 0;
    std::uint8_t service = 0;
    std::uint16_t ax = 0;
    std::uint16_t bx = 0;
    std::uint16_t cx = 0;
    std::uint16_t dx = 0;
    std::uint16_t ds = 0;
};

struct MillenniumDosStaticDataDosReturnObservation {
    std::uint64_t sequence = 0;
    std::uint16_t interrupt_instruction_address = 0;
    std::uint8_t interrupt_number = 0;
    std::uint8_t service = 0;
    std::uint16_t return_code_segment = 0;
    std::uint16_t return_instruction_pointer = 0;
    std::uint16_t ax = 0;
    std::uint16_t bx = 0;
    std::uint16_t cx = 0;
    std::uint16_t dx = 0;
    std::uint16_t flags = 0;
};

struct MillenniumDosStaticDataReadCommit {
    std::uint64_t sequence = 0;
    std::uint16_t destination_segment = 0;
    std::uint16_t destination_offset = 0;
    std::uint16_t requested_byte_count = 0;
    std::uint16_t returned_byte_count = 0;
};

struct MillenniumDosStaticDataLoadCheckpoint {
    MillenniumDosStaticDataLoadState state =
        MillenniumDosStaticDataLoadState::awaiting_loader_entry;
    MillenniumDosStaticDataLoadBoundary boundary;
    std::uint16_t code_segment = 0;
    std::uint16_t data_segment = 0;
    std::uint16_t file_handle = 0;
    bool bytes_read = false;
};

class MillenniumDosStaticDataLoadSession {
public:
    MillenniumDosStaticDataLoadSession(
        std::span<const std::uint8_t> game_executable,
        std::uint16_t code_segment,
        std::uint16_t data_segment);

    [[nodiscard]] MillenniumDosStaticDataLoadBoundary boundary() const;
    [[nodiscard]] MillenniumDosStaticDataLoadCheckpoint checkpoint() const;
    void observe_loader_entry(std::uint64_t sequence,
        std::uint16_t call_address, std::uint16_t target_address,
        std::uint16_t return_address, std::uint16_t code_segment);
    [[nodiscard]] std::optional<MillenniumDosStaticDataReadCommit> observe_dos_return(
        const MillenniumDosStaticDataDosReturnObservation& observation);
    void observe_loader_return(std::uint64_t sequence,
        std::uint16_t instruction_address, std::uint16_t returned_to,
        std::uint16_t code_segment);

private:
    [[nodiscard]] MillenniumDosStaticDataLoadBoundary make_boundary() const;

    std::uint16_t code_segment_ = 0;
    std::uint16_t data_segment_ = 0;
    std::uint16_t file_handle_ = 0;
    std::uint16_t last_read_ax_ = 0;
    std::uint16_t last_read_cx_ = 0;
    std::uint16_t last_read_dx_ = 0;
    std::uint64_t last_sequence_ = 0;
    MillenniumDosStaticDataLoadState state_ =
        MillenniumDosStaticDataLoadState::awaiting_loader_entry;
    bool bytes_read_ = false;
};

} // namespace eon
