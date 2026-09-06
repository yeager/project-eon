#pragma once

#include "engine/deuteros_amiga_title_stage_session.hpp"
#include "engine/native_runtime_memory.hpp"

#include <cstdint>
#include <string>

namespace eon {

// Immutable ownership boundary reached after either admitted loader has
// installed the title image and returned to its real entry stub. The bytes
// remain in coordinator-owned runtime memory.
struct DeuterosAmigaTitleProgramEntrySnapshot {
    std::uint16_t profile = 5;
    std::uint32_t entry_address = 0x13000;
    std::uint32_t target_address = 0x40426;
    std::uint32_t controller_pointer = 0;
    std::uint64_t runtime_memory_checksum = 0;
    std::string jmp_sha256;
};

struct DeuterosAmigaTitleProgramEntryTransaction {
    bool accepted = false;
    std::string error;
    NativeRuntimeEffectBatch batch;
};

// Validates that the retained JMP still belongs to the exact memory image
// that produced the boundary, then constructs (but does not apply) the
// profile-selected entry writes. Publication and title-session replacement
// remain a single coordinator transaction.
[[nodiscard]] DeuterosAmigaTitleProgramEntryTransaction
prepare_deuteros_amiga_title_program_entry_transaction(
    const DeuterosAmigaTitleProgramEntrySnapshot& snapshot,
    const NativeRuntimeMemory& memory,
    const DeuterosAmigaTitleStageSession::LocalPrefixAdvance& prefix);

} // namespace eon
