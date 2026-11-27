#include "data/deuteros_amiga_vblank_callback.hpp"

#include "data/sha256.hpp"

#include <limits>
#include <map>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace eon {
namespace {

constexpr std::uint32_t callback_address = 0x207fe;
constexpr std::uint32_t callback_size = 0x6c;
constexpr std::uint32_t main_stage_disk_offset = 0x5800;
constexpr std::uint32_t main_stage_runtime_address = 0x20000;
constexpr std::uint32_t callback_disk_offset = 0x5ffe;
constexpr std::uint32_t linear_address_limit = 0x1000000;
constexpr std::string_view callback_sha256 =
    "0e1f71cfdcd6b0d5e5f206ab945a56a260e7bb9fa09abe85dfc519eab9170813";
static_assert(main_stage_disk_offset + callback_address - main_stage_runtime_address
    == callback_disk_offset);

class SparseMemory {
public:
    explicit SparseMemory(
        const std::function<std::optional<std::uint8_t>(std::uint32_t)>& read_byte)
        : read_byte_(read_byte) {}

    [[nodiscard]] std::uint8_t read(const std::uint32_t address) {
        validate_address(address, 1);
        if (const auto changed = changes_.find(address); changed != changes_.end())
            return changed->second;
        if (const auto original = originals_.find(address); original != originals_.end())
            return original->second;
        const auto value = read_byte_(address);
        if (!value) throw std::runtime_error("Deuteros VBL callback read is not owned");
        originals_.emplace(address, *value);
        return *value;
    }

    [[nodiscard]] std::uint16_t read_word(const std::uint32_t address) {
        validate_address(address, 2);
        if ((address & 1U) != 0)
            throw std::runtime_error("Deuteros VBL callback word read is unaligned");
        return static_cast<std::uint16_t>((static_cast<std::uint16_t>(read(address)) << 8U)
            | read(address + 1U));
    }

    [[nodiscard]] std::uint32_t read_long(const std::uint32_t address) {
        validate_address(address, 4);
        if ((address & 1U) != 0)
            throw std::runtime_error("Deuteros VBL callback long read is unaligned");
        return (static_cast<std::uint32_t>(read(address)) << 24U)
            | (static_cast<std::uint32_t>(read(address + 1U)) << 16U)
            | (static_cast<std::uint32_t>(read(address + 2U)) << 8U)
            | read(address + 3U);
    }

    void write_word(const std::uint32_t address, const std::uint16_t value) {
        validate_address(address, 2);
        if ((address & 1U) != 0)
            throw std::runtime_error("Deuteros VBL callback word write is unaligned");
        static_cast<void>(read(address));
        static_cast<void>(read(address + 1U));
        changes_[address] = static_cast<std::uint8_t>(value >> 8U);
        changes_[address + 1U] = static_cast<std::uint8_t>(value);
    }

    void write_long(const std::uint32_t address, const std::uint32_t value) {
        validate_address(address, 4);
        if ((address & 1U) != 0)
            throw std::runtime_error("Deuteros VBL callback long write is unaligned");
        for (std::uint32_t index = 0; index < 4; ++index)
            static_cast<void>(read(address + index));
        changes_[address] = static_cast<std::uint8_t>(value >> 24U);
        changes_[address + 1U] = static_cast<std::uint8_t>(value >> 16U);
        changes_[address + 2U] = static_cast<std::uint8_t>(value >> 8U);
        changes_[address + 3U] = static_cast<std::uint8_t>(value);
    }

    [[nodiscard]] std::vector<DeuterosAmigaVblankCallbackMemoryWrite> changed_bytes() const {
        std::vector<DeuterosAmigaVblankCallbackMemoryWrite> result;
        result.reserve(changes_.size());
        for (const auto& [address, value] : changes_) {
            const auto original = originals_.find(address);
            if (original == originals_.end())
                throw std::runtime_error("Deuteros VBL callback wrote an unread byte");
            if (original->second != value)
                result.push_back({result.size() + 1, address, value});
        }
        return result;
    }

private:
    static void validate_address(const std::uint32_t address, const std::uint32_t size) {
        if (size == 0 || address >= linear_address_limit
            || size > linear_address_limit - address)
            throw std::runtime_error("Deuteros VBL callback address is outside 24-bit memory");
    }

    const std::function<std::optional<std::uint8_t>(std::uint32_t)>& read_byte_;
    std::map<std::uint32_t, std::uint8_t> originals_;
    std::map<std::uint32_t, std::uint8_t> changes_;
};

void verify_callback_identity(
    const std::function<std::optional<std::uint8_t>(std::uint32_t)>& read_owned_byte) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(callback_size);
    for (std::uint32_t index = 0; index < callback_size; ++index) {
        const auto value = read_owned_byte(callback_address + index);
        if (!value) throw std::runtime_error("Deuteros VBL callback code is not owned");
        bytes.push_back(*value);
    }
    if (to_hex(sha256(bytes)) != callback_sha256)
        throw std::runtime_error("Deuteros VBL callback code hash or load mapping changed");
}

} // namespace

DeuterosAmigaVblankCallbackResult evaluate_deuteros_amiga_vblank_callback(
    const std::function<std::optional<std::uint8_t>(std::uint32_t)>& read_owned_byte) {
    if (!read_owned_byte)
        throw std::runtime_error("Deuteros VBL callback requires an owned-memory reader");
    verify_callback_identity(read_owned_byte);
    SparseMemory memory(read_owned_byte);

    DeuterosAmigaVblankCallbackResult result;
    result.counter_before = memory.read_long(0x2079e);
    result.counter_after = result.counter_before + 4U;
    memory.write_long(0x2079e, result.counter_after);

    result.palette_flag = memory.read(0x207ea) != 0;
    if (result.palette_flag) {
        result.palette_index_before = memory.read_word(0x207b0);
        result.palette_index_after = static_cast<std::uint16_t>(result.palette_index_before + 1U);
        memory.write_word(0x207b0, result.palette_index_after);
        result.palette_row = static_cast<std::uint8_t>(result.palette_index_after & 3U);
        const auto source = 0x207b2U + static_cast<std::uint32_t>(result.palette_row) * 10U;
        bool recorded_first_copy = false;
        for (const auto pointer_cell : {0x2197aU, 0x2197eU}) {
            const auto palette = memory.read_long(pointer_cell);
            if ((palette & 1U) != 0 || palette > linear_address_limit - 0x30U)
                throw std::runtime_error("Deuteros VBL callback palette pointer is invalid");
            for (std::uint32_t word = 0; word < 2; ++word) {
                const auto value = memory.read_word(source + word * 2U);
                if (!recorded_first_copy) {
                    if (word == 0) result.first_palette_word = value;
                    else result.second_palette_word = value;
                }
                memory.write_word(palette + 0x2aU + word * 4U, value);
            }
            recorded_first_copy = true;
        }
    }

    result.audio_dispatch_requested = memory.read(0x207eb) != 0;
    if (result.audio_dispatch_requested) {
        result.audio_dispatch_address = 0x22bea;
        result.continuation_address = 0;
        result.stop_before_address = 0x20860;
    }
    result.counter_after = memory.read_long(0x2079e);
    result.memory_writes = memory.changed_bytes();
    return result;
}

} // namespace eon
