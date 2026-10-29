#pragma once

#include "data/millennium_dos_voice_bank.hpp"
#include "engine/millennium_dos_paragraph_arena.hpp"
#include "engine/millennium_dos_sibm_function_zero_session.hpp"
#include "engine/native_runtime_memory.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace eon {

enum class MillenniumDosSibmStartupState : std::uint8_t {
    ready,
    complete,
    failed,
};

struct MillenniumDosSibmStartupCheckpoint {
    MillenniumDosSibmStartupState state = MillenniumDosSibmStartupState::ready;
    std::optional<std::uint16_t> function_twelve_ax;
    std::optional<std::uint16_t> last_function_eleven_ax;
    std::size_t loaded_voice_count = 0;
    std::string error;
    MillenniumDosParagraphArenaCheckpoint paragraph_arena;
    NativeRuntimeMemoryCheckpoint memory;
};

// Lower-level native translation of SIBM functions `$0c` and `$0b/$08b3`.
// Callers must first establish function zero's timer setup; this isolated
// session does not prove or perform that prerequisite. Segment values come
// from Eon's owned arena; this does not emulate DOS, install INT 8, program a
// PIT, or generate speaker output.
class MillenniumDosSibmStartupSession {
public:
    [[nodiscard]] static std::uint32_t required_startup_paragraphs(
        const MillenniumDosSibmNativeVoiceBank& bank);
    MillenniumDosSibmStartupSession(std::uint64_t generation,
        std::uint16_t driver_segment, std::uint16_t arena_begin_segment = 0xe100,
        std::uint16_t arena_end_segment_exclusive = 0xffff,
        std::uint64_t arena_generation = 0);

    [[nodiscard]] MillenniumDosSibmStartupCheckpoint execute_after_timer_install(
        const MillenniumDosSibmNativeVoiceBank& bank);
    [[nodiscard]] bool apply_function_zero_effects(
        const MillenniumDosSibmFunctionZeroCheckpoint& function_zero);
    // Loads the hash-identified driver into the Eon-owned segmented memory
    // used by this session. This is the native equivalent of the verified
    // driver file read; it does not write the guest process or DOS memory.
    [[nodiscard]] bool load_driver_image(std::span<const std::uint8_t> driver);
    [[nodiscard]] MillenniumDosSibmStartupCheckpoint checkpoint() const;
    [[nodiscard]] std::optional<std::uint8_t> read_byte(std::uint16_t segment,
        std::uint16_t offset) const;

private:
    [[nodiscard]] bool allocate_and_store(std::uint32_t byte_count,
        std::uint16_t driver_pointer_address,
        std::optional<std::pair<std::uint16_t, std::uint16_t>> table_entry);
    [[nodiscard]] bool apply_bytes(std::uint16_t segment, std::uint16_t offset,
        std::span<const std::uint8_t> bytes, const std::string& label);
    [[nodiscard]] bool apply_byte(std::uint16_t segment, std::uint16_t offset,
        std::uint8_t value, const std::string& label);
    [[nodiscard]] bool apply_driver_word(std::uint16_t address,
        std::uint16_t value, const std::string& label);
    void fail(std::string message);

    std::uint64_t generation_ = 0;
    std::uint16_t driver_segment_ = 0;
    MillenniumDosParagraphArena arena_;
    NativeRuntimeMemory memory_;
    MillenniumDosSibmStartupState state_ = MillenniumDosSibmStartupState::ready;
    std::optional<std::uint16_t> function_twelve_ax_;
    std::optional<std::uint16_t> last_function_eleven_ax_;
    std::size_t loaded_voice_count_ = 0;
    std::size_t next_batch_ = 1;
    std::string error_;
    std::uint16_t status_segment_ = 0;
    std::uint16_t sample_table_segment_ = 0;
    std::uint16_t header_table_segment_ = 0;
    std::uint16_t block_table_segment_ = 0;
    bool driver_image_loaded_ = false;
};

} // namespace eon
