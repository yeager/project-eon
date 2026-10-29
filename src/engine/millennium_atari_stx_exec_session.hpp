#pragma once

#include "data/atari_st_prg.hpp"
#include "data/atari_st_stx.hpp"
#include "engine/atari_st_prg_load_session.hpp"
#include "engine/native_runtime_memory.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace eon {

enum class MillenniumAtariStxExecState : std::uint8_t {
    image_admitted,
    copy_prefix_ready,
};

// Hash-bound first source-only part of the physical-STX EXEC.TOS loader.
// This stops at the first read past the PRG's initialized TEXT+DATA image;
// it does not assume a TOS basepage, stack, or allocator contents.
struct MillenniumAtariStxExecCheckpoint {
    std::uint64_t generation = 0;
    MillenniumAtariStxExecState state = MillenniumAtariStxExecState::image_admitted;
    std::string executable_sha256;
    std::uint32_t load_base = 0;
    std::uint32_t entry_address = 0;
    std::uint32_t text_bytes = 0;
    std::uint32_t data_bytes = 0;
    std::uint32_t bss_bytes = 0;
    std::size_t relocation_count = 0;
    std::uint32_t copy_source_address = 0;
    std::uint32_t copy_destination_address = 0;
    std::uint32_t known_source_bytes = 0;
    std::uint32_t copied_words = 0;
    std::uint32_t local_instructions_executed = 0;
    std::uint32_t remaining_d0 = 0;
    std::uint32_t next_instruction_address = 0;
    std::uint32_t next_source_read_address = 0;
    std::uint32_t next_destination_write_address = 0;
    std::vector<std::uint8_t> known_copy_bytes;
};

struct MillenniumAtariStxExecResult {
    bool accepted = false;
    std::string error;
};

class MillenniumAtariStxExecSession {
public:
    static constexpr std::uint32_t native_load_base = 0x00010000U;

    MillenniumAtariStxExecSession(std::uint64_t generation,
        const AtariStStxPairedFileSource& source);

    [[nodiscard]] const MillenniumAtariStxExecCheckpoint& checkpoint() const noexcept {
        return checkpoint_;
    }
    [[nodiscard]] NativeRuntimeEffectBatch make_prg_load_effect_batch(std::string id) const;
    [[nodiscard]] MillenniumAtariStxExecResult execute_known_copy_prefix(
        const NativeRuntimeMemory& memory);
    [[nodiscard]] NativeRuntimeEffectBatch make_known_copy_effect_batch(std::string id) const;

private:
    AtariStPrgLoadCheckpoint prg_load_;
    MillenniumAtariStxExecCheckpoint checkpoint_;
};

} // namespace eon
