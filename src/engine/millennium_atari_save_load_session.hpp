#pragma once

#include "data/fat12.hpp"
#include "engine/native_runtime_memory.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace eon {

struct MillenniumAtariSaveLoadCheckpoint {
    std::uint64_t generation = 0;
    std::uint8_t slot = 0;
    std::string disk_sha256;
    std::string filename;
    std::string payload_sha256;
    std::uint32_t payload_bytes = 0;
    std::uint32_t copied_source_bytes = 0;
    bool source_opened_read_only = false;
    bool source_mutated = false;
};

// Hash-bound read-only source and exact local memory-write plan for the
// Equinox MILL22B.INF save consumer. This does not select the consumer's
// caller or claim that the resulting state is playable.
class MillenniumAtariSaveLoadSession {
public:
    MillenniumAtariSaveLoadSession(std::uint64_t generation,
        const Fat12Disk& save_disk, const Fat12Disk& equinox_disk,
        std::uint8_t slot);

    [[nodiscard]] const MillenniumAtariSaveLoadCheckpoint& checkpoint() const noexcept {
        return checkpoint_;
    }
    [[nodiscard]] NativeRuntimeEffectBatch make_effect_batch(std::string id) const;

private:
    MillenniumAtariSaveLoadCheckpoint checkpoint_;
    std::vector<std::uint8_t> payload_;
};

} // namespace eon
