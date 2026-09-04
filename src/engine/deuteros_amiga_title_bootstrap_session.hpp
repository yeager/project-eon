#pragma once

#include "data/amiga_adf.hpp"
#include "data/deuteros_amiga_loader.hpp"

#include <cstdint>
#include <string>

namespace eon {

enum class DeuterosAmigaTitleBootstrapState {
    profile_return_observed,
    profile_selected,
    stage_transfer_validated,
    title_entry_dispatched,
};

struct DeuterosAmigaTitleBootstrapCheckpoint {
    DeuterosAmigaTitleBootstrapState state =
        DeuterosAmigaTitleBootstrapState::profile_return_observed;
    std::uint32_t profile_return_cell = 0;
    std::uint16_t profile_value = 0;
    std::uint32_t profile_table_address = 0;
    std::uint32_t profile_routine_address = 0;
    std::uint32_t stage_disk_offset = 0;
    std::uint32_t stage_length = 0;
    std::uint32_t stage_destination = 0;
    std::string stage_sha256;
    std::uint32_t entry_address = 0;
};

// Executes only the caller-connected, synchronous bootstrap chain selected
// by the recovered opening command. Each advance validates original bytes;
// no Amiga RAM, track I/O, clock, Exec result or host device is created.
class DeuterosAmigaTitleBootstrapSession {
public:
    DeuterosAmigaTitleBootstrapSession(const AmigaAdf& disk,
        const DeuterosAmigaLoadPlan& plan, const DeuterosAmigaTitleHandoffRoute& route);

    [[nodiscard]] bool advance();
    [[nodiscard]] bool complete() const noexcept {
        return checkpoint_.state == DeuterosAmigaTitleBootstrapState::title_entry_dispatched;
    }
    [[nodiscard]] const DeuterosAmigaTitleBootstrapCheckpoint& checkpoint() const noexcept {
        return checkpoint_;
    }

private:
    const AmigaAdf* disk_ = nullptr;
    const DeuterosAmigaLoadPlan* plan_ = nullptr;
    DeuterosAmigaTitleBootstrapCheckpoint checkpoint_;
};

} // namespace eon
