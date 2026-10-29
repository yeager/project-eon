#include "engine/millennium_dos_sibm_startup_session.hpp"

#include "data/sha256.hpp"

#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace eon {
namespace {

constexpr std::string_view sibm_driver_sha256 =
    "f3224caa43c1149907f852fa98816ed68c489b70f1ba795592d684d4e51f31b1";

} // namespace

MillenniumDosSibmStartupSession::MillenniumDosSibmStartupSession(
    const std::uint64_t generation, const std::uint16_t driver_segment,
    const std::uint16_t arena_begin_segment,
    const std::uint16_t arena_end_segment_exclusive,
    const std::uint64_t arena_generation)
    : generation_(generation), driver_segment_(driver_segment),
      arena_(arena_generation == 0 ? generation : arena_generation,
          arena_begin_segment, arena_end_segment_exclusive) {
    if (generation == 0 || driver_segment == 0) {
        throw std::invalid_argument("SIBM native startup needs an owned generation and segment");
    }
}

std::uint32_t MillenniumDosSibmStartupSession::required_startup_paragraphs(
    const MillenniumDosSibmNativeVoiceBank& bank) {
    if (bank.voices.size() != 14) {
        throw std::invalid_argument("SIBM native startup requires fourteen admitted voices");
    }
    const auto paragraphs_for_bytes = [](const std::uint32_t bytes) {
        if (bytes == 0 || bytes > 0xffffU) {
            throw std::invalid_argument("SIBM native startup allocation size is invalid");
        }
        return (bytes >> 4U) + 1U;
    };
    std::uint32_t total = paragraphs_for_bytes(15) + 3U * paragraphs_for_bytes(60);
    for (const auto& voice : bank.voices) {
        if (voice.unsigned_pcm.empty()
            || voice.unsigned_pcm.size() > std::numeric_limits<std::uint16_t>::max()) {
            throw std::invalid_argument("SIBM native voice allocation size is invalid");
        }
        total += paragraphs_for_bytes(26) + paragraphs_for_bytes(6)
            + paragraphs_for_bytes(static_cast<std::uint32_t>(voice.unsigned_pcm.size()));
    }
    if (total == 0 || total > 0xffffU) {
        throw std::invalid_argument("SIBM native startup arena size exceeds paragraph limit");
    }
    return total;
}

void MillenniumDosSibmStartupSession::fail(std::string message) {
    state_ = MillenniumDosSibmStartupState::failed;
    error_ = std::move(message);
}

bool MillenniumDosSibmStartupSession::load_driver_image(
    const std::span<const std::uint8_t> driver) {
    if (state_ != MillenniumDosSibmStartupState::ready || driver_image_loaded_) {
        fail("SIBM driver image load is one-shot and must precede startup");
        return false;
    }
    if (driver.size() != 2'871U || to_hex(sha256(driver)) != sibm_driver_sha256) {
        fail("SIBM driver image does not match the admitted English source");
        return false;
    }
    if (!apply_bytes(driver_segment_, 0, driver, "driver-image")) return false;
    driver_image_loaded_ = true;
    return true;
}

bool MillenniumDosSibmStartupSession::apply_function_zero_effects(
    const MillenniumDosSibmFunctionZeroCheckpoint& function_zero) {
    if (state_ != MillenniumDosSibmStartupState::ready || !driver_image_loaded_) {
        fail("SIBM function-zero effects require the loaded driver image");
        return false;
    }
    const auto initial_driver_flags = read_byte(driver_segment_, 0x00b1);
    if (function_zero.state != MillenniumDosSibmFunctionZeroState::awaiting_timer_vector_get
        || (function_zero.function_input != 0x0380 && function_zero.function_input != 0x0220)
        || function_zero.dos_ax != 0x3508
        || function_zero.dos_interrupt_instruction != 0x0233
        || function_zero.timer_handler_address != 0x034d
        || !function_zero.software_speaker_gate_disabled
        || function_zero.software_current_divisor != 0
        || function_zero.memory_effects.size() != 4U
        || function_zero.memory_effects[0]
            != MillenniumDosSibmFunctionZeroMemoryEffect{0x00cd, 1}
        || function_zero.memory_effects[1]
            != MillenniumDosSibmFunctionZeroMemoryEffect{0x00cf, 0}
        || function_zero.memory_effects[2]
            != MillenniumDosSibmFunctionZeroMemoryEffect{0x00d0, 0}
        || !initial_driver_flags
        || function_zero.memory_effects[3]
            != MillenniumDosSibmFunctionZeroMemoryEffect{0x00b1,
                static_cast<std::uint8_t>(*initial_driver_flags | 0x10U)}) {
        fail("SIBM function-zero effects do not match the exact first-call source path");
        return false;
    }
    for (const auto& effect : function_zero.memory_effects) {
        if (!apply_byte(driver_segment_, effect.address, effect.value, "function-zero"))
            return false;
    }
    return true;
}

bool MillenniumDosSibmStartupSession::apply_bytes(
    const std::uint16_t segment, const std::uint16_t offset,
    const std::span<const std::uint8_t> bytes, const std::string& label) {
    if (bytes.empty() || bytes.size() > 0x10000U - static_cast<std::size_t>(offset)) {
        fail("SIBM native memory write is empty or crosses a segment boundary");
        return false;
    }
    NativeRuntimeEffectBatch batch;
    batch.id = "millennium-dos-sibm-" + std::to_string(generation_) + "-"
        + std::to_string(next_batch_++) + "-" + label;
    batch.fully_admitted = true;
    batch.effects.reserve(bytes.size());
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        batch.effects.push_back({index + 1,
            {NativeRuntimeAddressSpace::dos_segmented, segment,
                static_cast<std::uint64_t>(offset) + index},
            MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::little_endian,
            bytes[index]});
    }
    const auto applied = memory_.apply(batch);
    if (!applied.accepted) {
        fail("SIBM native memory write rejected: " + applied.error);
        return false;
    }
    return true;
}

bool MillenniumDosSibmStartupSession::apply_byte(
    const std::uint16_t segment, const std::uint16_t offset,
    const std::uint8_t value, const std::string& label) {
    const std::array<std::uint8_t, 1> bytes{{value}};
    return apply_bytes(segment, offset, bytes, label);
}

bool MillenniumDosSibmStartupSession::apply_driver_word(
    const std::uint16_t address, const std::uint16_t value, const std::string& label) {
    const std::array<std::uint8_t, 2> bytes{{
        static_cast<std::uint8_t>(value), static_cast<std::uint8_t>(value >> 8U)}};
    return apply_bytes(driver_segment_, address, bytes, label);
}

bool MillenniumDosSibmStartupSession::allocate_and_store(
    const std::uint32_t byte_count, const std::uint16_t driver_pointer_address,
    const std::optional<std::pair<std::uint16_t, std::uint16_t>> table_entry) {
    if (byte_count == 0 || byte_count > 0xffffU) {
        fail("SIBM $0184 allocation request is outside its 16-bit byte count");
        return false;
    }
    // Exact helper `$0184`: `(size >> 4) + 1` paragraphs, without DOS side effects.
    const auto paragraphs = (byte_count >> 4U) + 1U;
    const auto result = arena_.allocate(paragraphs);
    if (!result.allocation) {
        // Earlier allocations and pointer writes intentionally remain owned,
        // matching function `$0c`'s lack of rollback on a later failure.
        fail("SIBM native paragraph allocation failed: " + result.error);
        return false;
    }
    if (driver_pointer_address != 0) {
        if (!apply_driver_word(driver_pointer_address, 0, "pointer-offset")) return false;
        if (!apply_driver_word(static_cast<std::uint16_t>(driver_pointer_address + 2U),
                result.allocation->segment, "pointer-segment")) return false;
    }
    if (driver_pointer_address == 0x72) status_segment_ = result.allocation->segment;
    if (driver_pointer_address == 0x76) sample_table_segment_ = result.allocation->segment;
    if (driver_pointer_address == 0x7a) header_table_segment_ = result.allocation->segment;
    if (driver_pointer_address == 0x7e) block_table_segment_ = result.allocation->segment;
    if (table_entry) {
        const std::array<std::uint8_t, 4> pointer{{
            0, 0,
            static_cast<std::uint8_t>(result.allocation->segment),
            static_cast<std::uint8_t>(result.allocation->segment >> 8U)}};
        const auto [table_segment, table_offset] = *table_entry;
        if (!apply_bytes(table_segment, table_offset, pointer, "voice-pointer")) return false;
    }
    return true;
}

MillenniumDosSibmStartupCheckpoint
MillenniumDosSibmStartupSession::execute_after_timer_install(
    const MillenniumDosSibmNativeVoiceBank& bank) {
    if (state_ != MillenniumDosSibmStartupState::ready) {
        fail("SIBM native startup session is one-shot");
        return checkpoint();
    }
    if (!driver_image_loaded_) {
        fail("SIBM startup requires its hash-verified image in owned driver memory");
        return checkpoint();
    }
    if (bank.voices.size() != 14 || bank.startup_profile.function_eleven_call_count != 14
        || bank.startup_profile.function_twelve_requested_bytes != 15) {
        fail("SIBM startup voice bank does not match the exact fourteen-call profile");
        return checkpoint();
    }

    if (!allocate_and_store(15, 0x72, std::nullopt)
        || !allocate_and_store(60, 0x76, std::nullopt)
        || !allocate_and_store(60, 0x7a, std::nullopt)
        || !allocate_and_store(60, 0x7e, std::nullopt)) {
        if (error_.starts_with("SIBM native paragraph allocation failed"))
            function_twelve_ax_ = 3;
        return checkpoint();
    }
    function_twelve_ax_ = 0;

    const std::vector<std::uint8_t> zero_status(15, 0);
    if (!apply_bytes(status_segment_, 0, zero_status, "status-zero")
        || !apply_byte(driver_segment_, 0x71, 1, "allocated-flag")) {
        return checkpoint();
    }

    for (std::size_t index = 0; index < bank.voices.size(); ++index) {
        const auto& voice = bank.voices[index];
        if (voice.driver_index != index + 1U
            || voice.original_filename != bank.startup_profile.function_eleven_filenames[index]
            || voice.unsigned_pcm.empty()
            || voice.unsigned_pcm.size() > std::numeric_limits<std::uint16_t>::max()) {
            fail("SIBM function-$0b voice order or payload is not source-bound");
            return checkpoint();
        }
        const auto table_offset = static_cast<std::uint16_t>((index + 1U) * 4U);
        const auto pointer_entry = [table_offset](const std::uint16_t segment) {
            return std::pair<std::uint16_t, std::uint16_t>{segment, table_offset};
        };
        const auto voice_name = "voice-" + std::to_string(voice.driver_index);
        const auto sample_bytes = static_cast<std::uint32_t>(voice.unsigned_pcm.size());
        if (!allocate_and_store(26, 0, pointer_entry(header_table_segment_))
            || !allocate_and_store(6, 0, pointer_entry(block_table_segment_))) {
            if (error_.starts_with("SIBM native paragraph allocation failed"))
                last_function_eleven_ax_ = 3;
            return checkpoint();
        }
        const auto arena_after_headers = arena_.checkpoint();
        if (arena_after_headers.allocations.size() < 6U) {
            fail("SIBM native header allocations are missing from the owned arena");
            last_function_eleven_ax_ = 3;
            return checkpoint();
        }
        const auto block_segment = arena_after_headers.allocations.back().segment;
        // The second allocation is the block header; the prior allocation is the VOC header.
        const auto voc_header_segment = arena_after_headers.allocations[
            arena_after_headers.allocations.size() - 2U].segment;
        if (!apply_bytes(voc_header_segment, 0, voice.original_header,
                voice_name + "-voc-header")
            || !apply_bytes(block_segment, 0, voice.first_block_header,
                voice_name + "-block-header")
            || !allocate_and_store(sample_bytes, 0, pointer_entry(sample_table_segment_))) {
            if (error_.starts_with("SIBM native paragraph allocation failed"))
                last_function_eleven_ax_ = 3;
            return checkpoint();
        }
        const auto arena_after_samples = arena_.checkpoint();
        if (arena_after_samples.allocations.empty()) {
            fail("SIBM native PCM allocation is missing from the owned arena");
            last_function_eleven_ax_ = 3;
            return checkpoint();
        }
        const auto pcm_segment = arena_after_samples.allocations.back().segment;
        if (!apply_bytes(pcm_segment, 0, voice.unsigned_pcm, voice_name + "-pcm")
            || !apply_byte(status_segment_, static_cast<std::uint16_t>(index + 1U),
                1, voice_name + "-loaded")) {
            return checkpoint();
        }
        ++loaded_voice_count_;
        last_function_eleven_ax_ = 0;
    }
    state_ = MillenniumDosSibmStartupState::complete;
    return checkpoint();
}

MillenniumDosSibmStartupCheckpoint MillenniumDosSibmStartupSession::checkpoint() const {
    return {state_, function_twelve_ax_, last_function_eleven_ax_,
        loaded_voice_count_, error_, arena_.checkpoint(), memory_.checkpoint()};
}

std::optional<std::uint8_t> MillenniumDosSibmStartupSession::read_byte(
    const std::uint16_t segment, const std::uint16_t offset) const {
    return memory_.read_byte({NativeRuntimeAddressSpace::dos_segmented, segment, offset});
}

} // namespace eon
