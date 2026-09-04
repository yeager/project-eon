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
    awaiting_second_exec_return,
    before_open_library_boundary,
};

struct DeuterosAmigaDeferredExecCall {
    std::uint32_t exec_base_read_address = 0;
    std::uint32_t exec_base_source_address = 0;
    std::uint32_t call_address = 0;
    std::int16_t vector = 0;
    std::uint32_t return_address = 0;
    std::optional<std::uint32_t> preceding_d0_literal;
};

// A value copied from a separately admitted genuine trace. The engine does
// not call Exec to produce it and gives D0/SR no inferred semantics.
struct DeuterosAmigaObservedExecReturn {
    std::uint64_t trace_sequence = 0;
    std::uint32_t exec_base_source_address = 0;
    std::uint32_t call_address = 0;
    std::int16_t vector = 0;
    std::uint32_t return_address = 0;
    std::uint32_t result_d0 = 0;
    std::uint16_t result_sr = 0;
};

struct DeuterosAmigaTitleExecBoundaryCheckpoint {
    DeuterosAmigaTitleExecBoundaryState state =
        DeuterosAmigaTitleExecBoundaryState::awaiting_local_prefix;
    std::uint32_t stack_pointer_value = 0;
    std::uint32_t stop_before_address = 0;
    std::uint32_t exec_base_source_address = 0;
    std::array<DeuterosAmigaDeferredExecCall, 2> deferred_calls{};
    std::array<std::optional<DeuterosAmigaObservedExecReturn>, 2> observed_returns{};
    std::string boundary_sha256;
};

// The final executable title-entry step before the first unresolved Amiga
// Exec dependency. Deferred calls are byte-proven future requirements only;
// the ingestion API records already observed returns but cannot supply an
// Exec base, invoke a vector, or derive a return value.
class DeuterosAmigaTitleExecBoundarySession {
public:
    DeuterosAmigaTitleExecBoundarySession(const AmigaAdf& disk,
        const DeuterosAmigaLoadPlan& plan, const DeuterosAmigaTitleExecPrelude& prelude,
        const DeuterosAmigaTitleStageProfile& profile);

    [[nodiscard]] std::optional<DeuterosAmigaTitleExecBoundaryCheckpoint>
    enter_after_local_prefix(std::uint32_t stack_pointer_value);
    // Accepts only the next exact call in the byte-validated sequence. A
    // second observation completes this narrow bridge at $4046c, immediately
    // before the first unresolved title setup/library call.
    [[nodiscard]] std::optional<DeuterosAmigaTitleExecBoundaryCheckpoint>
    observe_exec_return(const DeuterosAmigaObservedExecReturn& observation);
    [[nodiscard]] const DeuterosAmigaTitleExecBoundaryCheckpoint& checkpoint() const noexcept {
        return checkpoint_;
    }

private:
    DeuterosAmigaTitleExecBoundaryCheckpoint checkpoint_;
};

} // namespace eon
