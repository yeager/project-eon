#include "engine/millennium_atari_stx_exec_session.hpp"

#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace eon {
namespace {

constexpr std::string_view exec_tos_sha256 =
    "e8ecdb43275ecf1a69d159a4c7a170b2462dd4a430fb09804f7bd0e9823627a5";
constexpr std::uint32_t exec_copy_source_offset = 0x1cU;
constexpr std::uint32_t exec_copy_destination = 0x77000U;
constexpr std::uint32_t exec_copy_word_count = 0x101U;

std::uint32_t read_be32(const std::span<const std::uint8_t> bytes,
    const std::size_t offset) {
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U)
        | (static_cast<std::uint32_t>(bytes[offset + 1U]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 2U]) << 8U)
        | bytes[offset + 3U];
}

} // namespace

MillenniumAtariStxExecSession::MillenniumAtariStxExecSession(
    const std::uint64_t generation, const AtariStStxPairedFileSource& source) {
    if (generation == 0) throw std::runtime_error("Invalid Millennium Atari STX generation");
    const auto executable = source.read_file("EXEC.TOS");
    if (executable.disk != AtariStStxSourceDisk::disk1
        || to_hex(sha256(executable.bytes)) != exec_tos_sha256) {
        throw std::runtime_error("Unsupported Millennium Atari STX EXEC.TOS source");
    }

    const auto prg = parse_atari_st_prg(executable.bytes);
    if (prg.text_bytes != 0xbaU || prg.data_bytes != 0U || prg.bss_bytes != 0U
        || prg.symbol_bytes != 0U || prg.absolute_flag != 0U
        || prg.relocation_count != 3U || prg.relocations.size() != 3U
        || prg.relocations[0] != AtariStRelocation{0x08U, 0x1cU}
        || prg.relocations[1] != AtariStRelocation{0x22U, 0xaeU}
        || prg.relocations[2] != AtariStRelocation{0x6aU, 0xa2U}) {
        throw std::runtime_error("Unexpected Millennium Atari STX EXEC.TOS PRG layout");
    }
    prg_load_ = materialize_atari_st_prg_load(executable.bytes, prg, native_load_base);
    if (prg_load_.source_sha256 != exec_tos_sha256 || prg_load_.image.size() != 0xbaU
        || prg_load_.relocation_effects.size() != 3U
        || prg_load_.relocation_effects[0].relocated_value != native_load_base + 0x1cU
        || prg_load_.relocation_effects[1].relocated_value != native_load_base + 0xaeU
        || prg_load_.relocation_effects[2].relocated_value != native_load_base + 0xa2U) {
        throw std::runtime_error("Millennium Atari STX EXEC.TOS native image mismatch");
    }

    // These are the exact hash-pinned first six instructions after GEMDOS has
    // loaded and relocated the PRG. In particular, this prefix uses no stack,
    // basepage word, TOS call, or uninitialized process-memory byte.
    constexpr std::array<std::uint8_t, 28> expected_entry{
        0x22, 0x7c, 0x00, 0x07, 0x70, 0x00,
        0x20, 0x7c, 0x00, 0x01, 0x00, 0x1c,
        0x30, 0x3c, 0x01, 0x00,
        0x32, 0xd8,
        0x51, 0xc8, 0xff, 0xfc,
        0x4e, 0xf9, 0x00, 0x07, 0x70, 0x00};
    if (!std::equal(expected_entry.begin(), expected_entry.end(), prg_load_.image.begin())) {
        throw std::runtime_error("Millennium Atari STX EXEC.TOS entry instructions changed");
    }

    checkpoint_.generation = generation;
    checkpoint_.executable_sha256 = std::string(exec_tos_sha256);
    checkpoint_.load_base = prg_load_.load_base;
    checkpoint_.entry_address = prg_load_.entry_address;
    checkpoint_.text_bytes = prg_load_.text_bytes;
    checkpoint_.data_bytes = prg_load_.data_bytes;
    checkpoint_.bss_bytes = prg_load_.bss_bytes;
    checkpoint_.relocation_count = prg_load_.relocation_effects.size();
}

NativeRuntimeEffectBatch MillenniumAtariStxExecSession::make_prg_load_effect_batch(
    std::string id) const {
    if (checkpoint_.generation == 0 || id.empty()
        || checkpoint_.executable_sha256 != exec_tos_sha256) {
        throw std::runtime_error("Unadmitted Millennium Atari STX PRG load batch");
    }
    return make_atari_st_prg_load_effect_batch(prg_load_, std::move(id));
}

MillenniumAtariStxExecResult MillenniumAtariStxExecSession::execute_known_copy_prefix(
    const NativeRuntimeMemory& memory) {
    if (checkpoint_.generation == 0
        || checkpoint_.state != MillenniumAtariStxExecState::image_admitted) {
        return {false, "Millennium Atari STX EXEC.TOS copy prefix is unavailable"};
    }
    const auto loaded = memory.read_linear_range(prg_load_.load_base, prg_load_.image.size());
    if (!loaded || *loaded != prg_load_.image) {
        return {false, "Millennium Atari STX EXEC.TOS image is absent or changed in native memory"};
    }

    // TEXT ends at +$ba. EXEC.TOS requests 257 words from +$1c, so only the
    // first 79 words are backed by the admitted PRG bytes. Stop before the
    // next source read; GEMDOS' allocator tail is not known here.
    constexpr auto available = 0xbaU - exec_copy_source_offset;
    static_assert((available & 1U) == 0U);
    constexpr auto copied_words = available / 2U;
    constexpr auto copied_bytes = copied_words * 2U;
    if (copied_words == 0 || copied_words >= exec_copy_word_count) {
        return {false, "Millennium Atari STX EXEC.TOS bounded prefix has invalid extent"};
    }
    const auto source = memory.read_linear_range(
        prg_load_.load_base + exec_copy_source_offset, copied_bytes);
    if (!source || !std::equal(source->begin(), source->end(),
            prg_load_.image.begin() + exec_copy_source_offset)) {
        return {false, "Millennium Atari STX EXEC.TOS copy source is absent or changed"};
    }

    checkpoint_.state = MillenniumAtariStxExecState::copy_prefix_ready;
    checkpoint_.copy_source_address = prg_load_.load_base + exec_copy_source_offset;
    checkpoint_.copy_destination_address = exec_copy_destination;
    checkpoint_.known_source_bytes = copied_bytes;
    checkpoint_.copied_words = copied_words;
    checkpoint_.local_instructions_executed = 3U + 2U * copied_words;
    checkpoint_.remaining_d0 = 0x100U - copied_words;
    checkpoint_.next_instruction_address = prg_load_.load_base + 0x10U;
    checkpoint_.next_source_read_address = checkpoint_.copy_source_address + copied_bytes;
    checkpoint_.next_destination_write_address = exec_copy_destination + copied_bytes;
    checkpoint_.known_copy_bytes = *source;
    return {true, {}};
}

NativeRuntimeEffectBatch MillenniumAtariStxExecSession::make_known_copy_effect_batch(
    std::string id) const {
    if (checkpoint_.generation == 0 || id.empty()
        || checkpoint_.state != MillenniumAtariStxExecState::copy_prefix_ready
        || checkpoint_.known_copy_bytes.size() != checkpoint_.known_source_bytes
        || checkpoint_.known_source_bytes != 158U || checkpoint_.copied_words != 79U
        || checkpoint_.remaining_d0 != 0xb1U
        || checkpoint_.next_source_read_address != prg_load_.load_base + 0xbaU
        || checkpoint_.next_destination_write_address != exec_copy_destination + 158U) {
        throw std::runtime_error("Unadmitted Millennium Atari STX known-copy effect batch");
    }
    NativeRuntimeEffectBatch batch{std::move(id), true, {}};
    batch.effects.reserve(checkpoint_.known_copy_bytes.size());
    for (std::size_t index = 0; index < checkpoint_.known_copy_bytes.size(); ++index) {
        batch.effects.push_back({index + 1,
            {NativeRuntimeAddressSpace::linear, std::nullopt,
                static_cast<std::uint64_t>(checkpoint_.copy_destination_address) + index},
            MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian,
            checkpoint_.known_copy_bytes[index]});
    }
    return batch;
}

} // namespace eon
