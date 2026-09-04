#pragma once

#include "data/atari_st_prg.hpp"
#include "data/fat12.hpp"
#include "engine/native_runtime_memory.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace eon {

enum class MillenniumAtariReadOnlyGemdosState : std::uint8_t {
    config_jsr_boundary,
    revoked,
};

struct MillenniumAtariReadOnlyGemdosCheckpoint {
    std::uint64_t generation = 0;
    MillenniumAtariReadOnlyGemdosState state =
        MillenniumAtariReadOnlyGemdosState::revoked;
    std::string disk_sha256;
    std::string filename;
    std::string payload_sha256;
    std::uint32_t payload_size = 0;
    std::uint16_t requested_fopen_mode = 0;
    std::uint16_t compatibility_handle = 0;
    std::uint32_t fread_request_bytes = 0;
    std::uint32_t fread_return_bytes = 0;
    std::uint32_t fread_destination_address = 0;
    std::uint32_t stop_before_jsr_address = 0;
    bool source_opened_read_only = false;
    bool source_mutated = false;
    bool fclose_modeled = false;
};

struct MillenniumAtariReadOnlyGemdosResult {
    bool accepted = false;
    std::string error;
};

// Narrow native compatibility service for the exact loader request already
// proven in the Equinox bootstrap. The original asks Fopen mode 2, but Eon
// never grants write access: one private compatibility handle exposes only an
// immutable FAT-chain snapshot to the immediately following Fread. The
// service stops before JSR $2a500 and models no general GEMDOS behaviour.
class MillenniumAtariReadOnlyGemdosSession {
public:
    MillenniumAtariReadOnlyGemdosSession(std::uint64_t generation,
        const Fat12Disk& disk, const MillenniumAtariTrapEntry& fopen,
        const MillenniumAtariFreadFramePrefixExecution& fread,
        const MillenniumAtariFreadConfigTransferBoundary& transfer);

    [[nodiscard]] const MillenniumAtariReadOnlyGemdosCheckpoint& checkpoint() const noexcept {
        return checkpoint_;
    }
    [[nodiscard]] NativeRuntimeEffectBatch make_fread_effect_batch(std::string id) const;
    [[nodiscard]] MillenniumAtariReadOnlyGemdosResult revoke(std::uint64_t generation);

private:
    MillenniumAtariReadOnlyGemdosCheckpoint checkpoint_;
    std::vector<std::uint8_t> payload_;
};

} // namespace eon
