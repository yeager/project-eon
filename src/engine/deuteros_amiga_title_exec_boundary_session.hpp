#pragma once

#include "data/amiga_adf.hpp"
#include "data/deuteros_amiga_loader.hpp"
#include "data/deuteros_amiga_title_stage.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace eon {

enum class DeuterosAmigaTitleExecBoundaryState {
    awaiting_local_prefix,
    awaiting_exec_base_read,
};

struct DeuterosAmigaDeferredExecCall {
    std::uint32_t exec_base_read_address = 0;
    std::uint32_t exec_base_source_address = 0;
    std::uint32_t call_address = 0;
    std::int16_t vector = 0;
    std::uint32_t return_address = 0;
    std::optional<std::uint32_t> preceding_d0_literal;
};

struct DeuterosAmigaTitleExecBoundaryCheckpoint {
    DeuterosAmigaTitleExecBoundaryState state =
        DeuterosAmigaTitleExecBoundaryState::awaiting_local_prefix;
    std::uint32_t stack_pointer_value = 0;
    std::uint32_t stop_before_address = 0;
    std::uint32_t exec_base_source_address = 0;
    std::array<DeuterosAmigaDeferredExecCall, 2> deferred_calls{};
    std::string boundary_sha256;
};

// The final executable title-entry step before the first unresolved Amiga
// Exec dependency. Deferred calls are byte-proven future requirements only;
// this session has no API for supplying an Exec base or return value.
class DeuterosAmigaTitleExecBoundarySession {
public:
    DeuterosAmigaTitleExecBoundarySession(const AmigaAdf& disk,
        const DeuterosAmigaLoadPlan& plan, const DeuterosAmigaTitleExecPrelude& prelude,
        const DeuterosAmigaTitleStageProfile& profile);

    [[nodiscard]] std::optional<DeuterosAmigaTitleExecBoundaryCheckpoint>
    enter_after_local_prefix(std::uint32_t stack_pointer_value);
    [[nodiscard]] const DeuterosAmigaTitleExecBoundaryCheckpoint& checkpoint() const noexcept {
        return checkpoint_;
    }

private:
    DeuterosAmigaTitleExecBoundaryCheckpoint checkpoint_;
};

} // namespace eon
