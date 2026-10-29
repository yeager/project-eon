#pragma once

#include "data/deuteros_amiga_loader.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

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
