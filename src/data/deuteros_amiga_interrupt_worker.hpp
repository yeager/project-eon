#pragma once

#include "data/deuteros_amiga_loader.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>
#include <string>

namespace eon {

enum class DeuterosAmigaInterruptRegisterWidth : std::uint8_t {
    word,
    longword,
};

// A custom-register write intent in the exact order produced by one
// scenario evaluation. This does not apply hardware or audio effects.
struct DeuterosAmigaInterruptRegisterWrite {
    std::size_t order = 0;
    std::uint32_t address = 0;
    DeuterosAmigaInterruptRegisterWidth width = DeuterosAmigaInterruptRegisterWidth::word;
    std::uint32_t value = 0;
    constexpr bool operator==(const DeuterosAmigaInterruptRegisterWrite&) const = default;
};

struct DeuterosAmigaInstalledInterruptWorkerResult {
    std::uint32_t memory_base_address = 0;
    std::vector<std::uint8_t> memory;
    std::vector<DeuterosAmigaInterruptRegisterWrite> register_writes;
};

struct DeuterosAmigaInterruptMemoryWrite {
    std::size_t order = 0;
    std::uint32_t address = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const DeuterosAmigaInterruptMemoryWrite&) const = default;
};

struct DeuterosAmigaInstalledInterruptWorkerSparseResult {
    std::vector<DeuterosAmigaInterruptMemoryWrite> memory_writes;
    std::vector<DeuterosAmigaInterruptRegisterWrite> register_writes;
};

struct DeuterosAmigaVertbHandlerProfile {
    std::uint32_t entry_address = 0;
    std::uint32_t source_disk_offset = 0;
    std::uint32_t source_length = 0;
    std::string raw_sha256;
};

struct DeuterosAmigaVertbRegistrationObservation {
    std::uint64_t trace_sequence = 0;
    std::uint32_t call_address = 0;
    std::uint32_t return_address = 0;
    std::int32_t vector = 0;
    std::uint32_t interrupt_level = 0;
    std::uint32_t interrupt_node = 0;
    std::uint32_t handler_address = 0;
};

struct DeuterosAmigaVertbInvocationObservation {
    std::uint64_t trace_sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint8_t buffer_update_enabled = 0;
    std::uint8_t sound_enabled = 0;
    std::uint32_t counter_before = 0;
};

struct DeuterosAmigaVertbCounterEffect {
    std::uint64_t trace_sequence = 0;
    std::uint32_t address = 0;
    std::uint32_t before = 0;
    std::uint32_t after = 0;
};

struct DeuterosAmigaVertbTranslation {
    std::uint64_t trace_sequence = 0;
    std::vector<DeuterosAmigaInterruptMemoryWrite> memory_writes;
    std::vector<DeuterosAmigaInterruptRegisterWrite> register_writes;
};

// Result of evaluating the source-bound post-LoadView INTREQR poll against
// two explicitly hypothetical register values. This is a static diagnostic,
// never a runtime observation and never a source of native state changes.
struct DeuterosAmigaVertbWaitScenario {
    std::string label;
    std::uint8_t synthetic_port_value = 0;
    bool vertb_bit_set = false;
    bool remains_in_wait = false;
    std::uint32_t next_instruction_address = 0;
};

struct DeuterosAmigaVertbWaitScenarioReport {
    bool synthetic_scenarios_only = true;
    std::string source_sha256;
    std::string owned_image_sha256;
    std::array<DeuterosAmigaVertbWaitScenario, 2> scenarios{};
    // These are the minimum source-ordered runtime facts needed to turn the
    // branch analysis into an observed path; the diagnostic does not supply
    // any of them.
    std::array<std::string, 4> missing_runtime_evidence{};
};

struct DeuterosAmigaScenarioMemoryCell {
    std::uint32_t address = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const DeuterosAmigaScenarioMemoryCell&) const = default;
};

[[nodiscard]] DeuterosAmigaVertbHandlerProfile parse_deuteros_amiga_vertb_handler(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

// Stateful evidence gate for the installed VERTB server. It accepts one
// observed AddIntServer return and then only observed handler entries. It
// models the unconditional counter update only when both conditional branches
// are inactive; it supplies neither interrupt cadence nor device effects.
class DeuterosAmigaVertbSession {
public:
    explicit DeuterosAmigaVertbSession(DeuterosAmigaVertbHandlerProfile profile);

    void observe_registration(const DeuterosAmigaVertbRegistrationObservation& observation);
    [[nodiscard]] DeuterosAmigaVertbCounterEffect observe_idle_invocation(
        const DeuterosAmigaVertbInvocationObservation& observation);
    [[nodiscard]] DeuterosAmigaVertbTranslation observe_invocation(
        std::uint64_t trace_sequence, std::uint32_t instruction_address,
        const std::function<std::optional<std::uint8_t>(std::uint32_t)>& read_owned_byte);

private:
    DeuterosAmigaVertbHandlerProfile profile_;
    std::uint64_t registration_sequence_ = 0;
    std::uint64_t last_invocation_sequence_ = 0;
};

// Execute one explicit, source-bound VERTB entry over an owned sparse memory
// view. It returns a private write set and ordered custom-register intents;
// missing reads or either unimplemented callback prerequisite reject the
// whole result. The caller remains responsible for atomic publication.
[[nodiscard]] DeuterosAmigaVertbTranslation evaluate_deuteros_amiga_vertb_entry(
    const DeuterosAmigaVertbHandlerProfile& profile, std::uint64_t trace_sequence,
    std::uint32_t instruction_address,
    const std::function<std::optional<std::uint8_t>(std::uint32_t)>& read_owned_byte);

// Compare the clear/set arms of the hash-bound $216f2 INTREQR loop using a
// cloned sparse owned-memory image. `source_wait_bytes` and the corresponding
// bytes at $216f2 in `owned_memory` must match the admitted 12-byte span. The
// caller's memory is never modified and the synthetic DFF01F values are never
// returned as observations.
[[nodiscard]] DeuterosAmigaVertbWaitScenarioReport
evaluate_deuteros_amiga_vertb_wait_scenarios(
    std::span<const std::uint8_t> source_wait_bytes,
    std::span<const DeuterosAmigaScenarioMemoryCell> owned_memory);

// Translates one hash-admitted worker over an explicit owned-memory scenario.
// It returns a cloned memory image and ordered custom-register write intents;
// it does not infer invocation, cadence, device effects, or captured state.
[[nodiscard]] DeuterosAmigaInstalledInterruptWorkerResult
evaluate_deuteros_amiga_installed_interrupt_worker(
    const DeuterosAmigaInstalledInterruptWorker& worker,
    std::uint32_t memory_base_address,
    std::span<const std::uint8_t> memory);

// Same bounded translator for an owned sparse address space. Every actual
// read and write destination must be supplied by the callback; no padding or
// missing-memory default is introduced.
[[nodiscard]] DeuterosAmigaInstalledInterruptWorkerSparseResult
evaluate_deuteros_amiga_installed_interrupt_worker_sparse(
    const DeuterosAmigaInstalledInterruptWorker& worker,
    const std::function<std::optional<std::uint8_t>(std::uint32_t)>& read_byte);

} // namespace eon
