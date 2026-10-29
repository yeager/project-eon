#pragma once

#include "engine/native_runtime_memory.hpp"

#include <cstdint>
#include <optional>

namespace eon {

enum class DeuterosAmigaDisplaySurfaceOrigin : std::uint8_t {
    native_compatibility,
};

// Eon's virtual 24-bit address for a native four-plane display buffer. It is
// not an Amiga physical allocation or a captured Chip RAM pointer.
struct DeuterosAmigaNativeDisplaySurface {
    std::uint64_t runtime_generation = 0;
    std::uint64_t service_sequence = 0;
    std::uint64_t allocation_id = 0;
    std::uint32_t base_address = 0;
    std::uint32_t byte_count = 0;
    std::uint32_t alignment = 0;
    DeuterosAmigaDisplaySurfaceOrigin origin =
        DeuterosAmigaDisplaySurfaceOrigin::native_compatibility;
};

// Allocates only the title's proven four-plane display span from an Eon-owned
// virtual range, avoiding all addresses already initialized in this runtime.
class DeuterosAmigaNativeDisplaySurfaceSession {
public:
    explicit DeuterosAmigaNativeDisplaySurfaceSession(
        const std::uint64_t runtime_generation) noexcept
        : runtime_generation_(runtime_generation) {}

    [[nodiscard]] std::optional<DeuterosAmigaNativeDisplaySurface>
    allocate_title_surface(const NativeRuntimeMemoryCheckpoint& memory) noexcept {
        constexpr std::uint32_t arena_begin = 0x00e00000;
        constexpr std::uint32_t arena_end = 0x00f00000;
        constexpr std::uint32_t byte_count = 0x7d00;
        constexpr std::uint32_t alignment = 0x1000;
        if (runtime_generation_ == 0 || allocation_) return std::nullopt;

        std::uint32_t candidate = arena_begin;
        while (candidate <= arena_end - byte_count) {
            candidate = (candidate + alignment - 1U) & ~(alignment - 1U);
            if (candidate > arena_end - byte_count) break;
            bool overlaps = false;
            std::uint32_t next = candidate;
            for (const auto& cell : memory.initialized_bytes) {
                if (cell.location.address_space != NativeRuntimeAddressSpace::linear
                    || cell.location.segment || cell.location.offset < candidate
                    || cell.location.offset >= static_cast<std::uint64_t>(candidate) + byte_count) {
                    continue;
                }
                overlaps = true;
                next = static_cast<std::uint32_t>(cell.location.offset + 1U);
            }
            if (!overlaps) {
                allocation_ = DeuterosAmigaNativeDisplaySurface{
                    runtime_generation_, 2, 1, candidate, byte_count, alignment,
                    DeuterosAmigaDisplaySurfaceOrigin::native_compatibility};
                return allocation_;
            }
            if (next <= candidate) return std::nullopt;
            candidate = next;
        }
        return std::nullopt;
    }

    void reset() noexcept { allocation_.reset(); }
    [[nodiscard]] const std::optional<DeuterosAmigaNativeDisplaySurface>&
    allocation() const noexcept { return allocation_; }
    [[nodiscard]] std::uint64_t runtime_generation() const noexcept {
        return runtime_generation_;
    }

private:
    std::uint64_t runtime_generation_ = 0;
    std::optional<DeuterosAmigaNativeDisplaySurface> allocation_;
};

} // namespace eon
