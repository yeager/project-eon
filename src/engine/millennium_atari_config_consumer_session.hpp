#pragma once

#include "data/atari_st_prg.hpp"
#include "engine/millennium_atari_read_only_gemdos_session.hpp"
#include "engine/native_runtime_memory.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace eon {

enum class MillenniumAtariConfigConsumerState : std::uint8_t {
    status_register_boundary,
    revoked,
};

struct MillenniumAtariConfigConsumerCheckpoint {
    std::uint64_t generation = 0;
    MillenniumAtariConfigConsumerState state =
        MillenniumAtariConfigConsumerState::revoked;
    std::uint32_t jsr_instruction_address = 0;
    std::uint32_t jsr_return_address = 0;
    std::uint32_t jsr_target_address = 0;
    std::uint16_t entry_jump_opcode = 0;
    std::uint32_t entry_jump_target_address = 0;
    std::uint32_t entry_jump_file_offset = 0;
    std::uint32_t boundary_instruction_address = 0;
    std::uint16_t boundary_opcode = 0;
    std::string boundary_dependency;
    std::string mapped_prelude_sha256;
    std::size_t local_control_transfers_executed = 0;
    bool return_address_materialized = false;
    bool status_register_read = false;
    bool hardware_write_executed = false;
};

struct MillenniumAtariConfigConsumerResult {
    bool accepted = false;
    std::string error;
};

// Executes the caller-connected JSR and the config file's absolute JMP. The
// following MOVE SR,D0 depends on original CPU privilege/status state, so the
// session stops before that instruction and does not choose either branch or
// perform the fall-through hardware writes.
class MillenniumAtariConfigConsumerSession {
public:
    MillenniumAtariConfigConsumerSession(std::uint64_t generation,
        const NativeRuntimeMemory& memory,
        const MillenniumAtariReadOnlyGemdosCheckpoint& gemdos,
        const MillenniumAtariFreadConfigLoadAddressBoundary& load_boundary,
        const MillenniumAtariFreadMappedConfigPrelude& prelude);

    [[nodiscard]] const MillenniumAtariConfigConsumerCheckpoint& checkpoint() const noexcept {
        return checkpoint_;
    }
    [[nodiscard]] MillenniumAtariConfigConsumerResult revoke(std::uint64_t generation);

private:
    MillenniumAtariConfigConsumerCheckpoint checkpoint_;
};

} // namespace eon
