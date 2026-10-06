#include "data/deuteros_amiga_interrupt_worker.hpp"

#include <array>
#include <functional>
#include <limits>
#include <map>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace eon {
namespace {

using ReadByte = std::function<std::optional<std::uint8_t>(std::uint32_t)>;

class SparseMemory {
public:
    explicit SparseMemory(ReadByte read_byte) : read_byte_(std::move(read_byte)) {}

    [[nodiscard]] std::uint8_t read(const std::uint32_t address) {
        if (const auto changed = changes_.find(address); changed != changes_.end()) {
            return changed->second;
        }
        if (const auto original = originals_.find(address); original != originals_.end()) {
            return original->second;
        }
        const auto value = read_byte_(address);
        if (!value) throw std::runtime_error("Deuteros interrupt worker read is not owned");
        originals_.emplace(address, *value);
        return *value;
    }

    [[nodiscard]] std::uint16_t read_word(const std::uint32_t address) {
        if ((address & 1U) != 0 || address == std::numeric_limits<std::uint32_t>::max()) {
            throw std::runtime_error("Deuteros interrupt worker word read is unaligned or overflows");
        }
        return static_cast<std::uint16_t>((static_cast<std::uint16_t>(read(address)) << 8U)
            | read(address + 1U));
    }

    [[nodiscard]] std::uint32_t read_long(const std::uint32_t address) {
        if ((address & 1U) != 0 || address > std::numeric_limits<std::uint32_t>::max() - 3U) {
            throw std::runtime_error("Deuteros interrupt worker long read is unaligned or overflows");
        }
        return (static_cast<std::uint32_t>(read(address)) << 24U)
            | (static_cast<std::uint32_t>(read(address + 1U)) << 16U)
            | (static_cast<std::uint32_t>(read(address + 2U)) << 8U)
            | read(address + 3U);
    }

    void write_word(const std::uint32_t address, const std::uint16_t value) {
        if ((address & 1U) != 0 || address == std::numeric_limits<std::uint32_t>::max()) {
            throw std::runtime_error("Deuteros interrupt worker word write is unaligned or overflows");
        }
        static_cast<void>(read(address));
        static_cast<void>(read(address + 1U));
        changes_[address] = static_cast<std::uint8_t>(value >> 8U);
        changes_[address + 1U] = static_cast<std::uint8_t>(value);
    }

    void write_long(const std::uint32_t address, const std::uint32_t value) {
        if ((address & 1U) != 0 || address > std::numeric_limits<std::uint32_t>::max() - 3U) {
            throw std::runtime_error("Deuteros interrupt worker long write is unaligned or overflows");
        }
        for (std::uint32_t byte = 0; byte < 4; ++byte) {
            static_cast<void>(read(address + byte));
        }
        changes_[address] = static_cast<std::uint8_t>(value >> 24U);
        changes_[address + 1U] = static_cast<std::uint8_t>(value >> 16U);
        changes_[address + 2U] = static_cast<std::uint8_t>(value >> 8U);
        changes_[address + 3U] = static_cast<std::uint8_t>(value);
    }

    void write_byte(const std::uint32_t address, const std::uint8_t value) {
        static_cast<void>(read(address));
        changes_[address] = value;
    }

    [[nodiscard]] std::vector<DeuterosAmigaInterruptMemoryWrite> changed_bytes() const {
        std::vector<DeuterosAmigaInterruptMemoryWrite> result;
        result.reserve(changes_.size());
        for (const auto& [address, value] : changes_) {
            const auto original = originals_.find(address);
            if (original == originals_.end()) {
                throw std::runtime_error("Deuteros interrupt worker wrote an unread byte");
            }
            if (original->second != value) {
                result.push_back({result.size() + 1, address, value});
            }
        }
        return result;
    }

private:
    ReadByte read_byte_;
    std::map<std::uint32_t, std::uint8_t> originals_;
    std::map<std::uint32_t, std::uint8_t> changes_;
};

void append_write(std::vector<DeuterosAmigaInterruptRegisterWrite>& writes,
    const std::uint32_t address, const DeuterosAmigaInterruptRegisterWidth width,
    const std::uint32_t value) {
    writes.push_back({writes.size() + 1, address, width, value});
}

void validate_worker(const DeuterosAmigaInstalledInterruptWorker& worker) {
    constexpr std::string_view expected_hash =
        "661854d6976ab520b0398e2545003d3fe59692fc0de54f0f810f379cf25ccaf8";
    if (worker.caller_address != 0x224ea || worker.entry_address != 0x22816
        || worker.source_disk_offset != 0x8016 || worker.source_length != 0x1d2
        || worker.return_instruction_address != 0x229e6
        || worker.raw_sha256 != expected_hash) {
        throw std::runtime_error("Unexpected Deuteros installed interrupt worker identity");
    }
}

DeuterosAmigaInstalledInterruptWorkerSparseResult translate_worker(
    const DeuterosAmigaInstalledInterruptWorker& worker, SparseMemory& memory) {
    validate_worker(worker);
    DeuterosAmigaInstalledInterruptWorkerSparseResult result;
    result.register_writes.reserve(17);

    constexpr std::array<std::uint32_t, 4> addend_cells{0x229ea, 0x229ec, 0x229ee, 0x229f0};
    constexpr std::array<std::uint32_t, 4> accumulator_cells{0x22a02, 0x22a04, 0x22a06, 0x22a08};
    constexpr std::array<std::uint32_t, 4> period_registers{0xdff0a6, 0xdff0b6, 0xdff0c6, 0xdff0d6};
    for (std::size_t index = 0; index < addend_cells.size(); ++index) {
        const auto sum = static_cast<std::uint16_t>(memory.read_word(accumulator_cells[index])
            + memory.read_word(addend_cells[index]));
        memory.write_word(accumulator_cells[index], sum);
        append_write(result.register_writes, period_registers[index],
            DeuterosAmigaInterruptRegisterWidth::word, sum);
    }

    constexpr std::array<std::uint32_t, 4> pointer_cells{0x229f2, 0x229f6, 0x229fa, 0x229fe};
    constexpr std::array<std::uint32_t, 4> location_registers{0xdff0a0, 0xdff0b0, 0xdff0c0, 0xdff0d0};
    constexpr std::array<std::uint32_t, 4> length_registers{0xdff0a4, 0xdff0b4, 0xdff0c4, 0xdff0d4};
    auto flags = memory.read(0x229e8);
    for (std::size_t index = 0; index < pointer_cells.size(); ++index) {
        const auto mask = static_cast<std::uint8_t>(1U << index);
        if ((flags & mask) == 0) continue;
        flags = static_cast<std::uint8_t>(flags & static_cast<std::uint8_t>(~mask));
        memory.write_byte(0x229e8, flags);
        const auto pointer = memory.read_long(pointer_cells[index]);
        const auto location_address = static_cast<std::uint64_t>(pointer) + 8U;
        const auto length_address = static_cast<std::uint64_t>(pointer) + 12U;
        if (length_address > std::numeric_limits<std::uint32_t>::max()) {
            throw std::runtime_error("Deuteros interrupt worker pointer addition overflows");
        }
        append_write(result.register_writes, location_registers[index],
            DeuterosAmigaInterruptRegisterWidth::longword,
            memory.read_long(static_cast<std::uint32_t>(location_address)));
        append_write(result.register_writes, length_registers[index],
            DeuterosAmigaInterruptRegisterWidth::word,
            memory.read_word(static_cast<std::uint32_t>(length_address)));
        memory.write_long(pointer_cells[index], 0);
    }

    std::uint16_t enable_word = 0x8000;
    flags = memory.read(0x229e8);
    for (std::size_t index = 0; index < pointer_cells.size(); ++index) {
        if (memory.read_long(pointer_cells[index]) == 0) continue;
        const auto mask = static_cast<std::uint8_t>(1U << index);
        flags = static_cast<std::uint8_t>(flags | mask);
        enable_word = static_cast<std::uint16_t>(enable_word | mask);
    }
    memory.write_byte(0x229e8, flags);
    append_write(result.register_writes, 0xdff096,
        DeuterosAmigaInterruptRegisterWidth::word, enable_word);

    const auto scale = memory.read_word(0x2229a);
    constexpr std::array<std::uint32_t, 4> volume_cells{0x22a0a, 0x22a0c, 0x22a0e, 0x22a10};
    constexpr std::array<std::uint32_t, 4> volume_registers{0xdff0a8, 0xdff0b8, 0xdff0c8, 0xdff0d8};
    for (std::size_t index = 0; index < volume_cells.size(); ++index) {
        const auto product = static_cast<std::uint32_t>(scale) * memory.read_word(volume_cells[index]);
        // MULU.W fills D0.L; the following LSR.W affects only D0's low word.
        const auto value = static_cast<std::uint16_t>((product & 0xffffU) >> 8U);
        append_write(result.register_writes, volume_registers[index],
            DeuterosAmigaInterruptRegisterWidth::word, value);
    }
    result.memory_writes = memory.changed_bytes();
    return result;
}

} // namespace

DeuterosAmigaInstalledInterruptWorkerSparseResult
evaluate_deuteros_amiga_installed_interrupt_worker_sparse(
    const DeuterosAmigaInstalledInterruptWorker& worker,
    const std::function<std::optional<std::uint8_t>(std::uint32_t)>& read_byte) {
    if (!read_byte) throw std::runtime_error("Deuteros interrupt worker requires an owned-memory reader");
    SparseMemory memory(read_byte);
    return translate_worker(worker, memory);
}

DeuterosAmigaInstalledInterruptWorkerResult
evaluate_deuteros_amiga_installed_interrupt_worker(
    const DeuterosAmigaInstalledInterruptWorker& worker,
    const std::uint32_t memory_base_address,
    const std::span<const std::uint8_t> memory) {
    if (memory.empty() || static_cast<std::uint64_t>(memory_base_address) + memory.size()
            > (std::uint64_t{1} << 32U)) {
        throw std::runtime_error("Invalid Deuteros interrupt worker scenario memory range");
    }
    auto translated = evaluate_deuteros_amiga_installed_interrupt_worker_sparse(worker,
        [memory_base_address, memory](const std::uint32_t address)
            -> std::optional<std::uint8_t> {
            if (address < memory_base_address) return std::nullopt;
            const auto offset = static_cast<std::uint64_t>(address) - memory_base_address;
            if (offset >= memory.size()) return std::nullopt;
            return memory[static_cast<std::size_t>(offset)];
        });
    DeuterosAmigaInstalledInterruptWorkerResult result{memory_base_address,
        {memory.begin(), memory.end()}, std::move(translated.register_writes)};
    for (const auto& write : translated.memory_writes) {
        if (write.address < memory_base_address
            || static_cast<std::uint64_t>(write.address) - memory_base_address >= result.memory.size()) {
            throw std::runtime_error("Deuteros interrupt worker write escaped scenario memory");
        }
        result.memory[static_cast<std::size_t>(write.address - memory_base_address)] = write.value;
    }
    return result;
}

} // namespace eon
