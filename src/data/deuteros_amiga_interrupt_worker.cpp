#include "data/deuteros_amiga_interrupt_worker.hpp"

#include <array>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace eon {
namespace {

[[nodiscard]] std::size_t checked_offset(const std::uint32_t base,
    const std::size_t size, const std::uint32_t address, const std::size_t width) {
    const auto end = static_cast<std::uint64_t>(base) + size;
    const auto access_end = static_cast<std::uint64_t>(address) + width;
    if ((width > 1 && (address & 1U) != 0)
        || end > (std::uint64_t{1} << 32U) || address < base || access_end > end) {
        throw std::runtime_error("Deuteros interrupt worker scenario access is unaligned or out of bounds");
    }
    return static_cast<std::size_t>(address - base);
}

[[nodiscard]] std::uint16_t read_word(const std::uint32_t base,
    const std::vector<std::uint8_t>& memory, const std::uint32_t address) {
    const auto offset = checked_offset(base, memory.size(), address, 2);
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(memory[offset]) << 8U)
        | memory[offset + 1]);
}

[[nodiscard]] std::uint32_t read_long(const std::uint32_t base,
    const std::vector<std::uint8_t>& memory, const std::uint32_t address) {
    const auto offset = checked_offset(base, memory.size(), address, 4);
    return (static_cast<std::uint32_t>(memory[offset]) << 24U)
        | (static_cast<std::uint32_t>(memory[offset + 1]) << 16U)
        | (static_cast<std::uint32_t>(memory[offset + 2]) << 8U)
        | memory[offset + 3];
}

void write_word(const std::uint32_t base, std::vector<std::uint8_t>& memory,
    const std::uint32_t address, const std::uint16_t value) {
    const auto offset = checked_offset(base, memory.size(), address, 2);
    memory[offset] = static_cast<std::uint8_t>(value >> 8U);
    memory[offset + 1] = static_cast<std::uint8_t>(value);
}

void write_long(const std::uint32_t base, std::vector<std::uint8_t>& memory,
    const std::uint32_t address, const std::uint32_t value) {
    const auto offset = checked_offset(base, memory.size(), address, 4);
    memory[offset] = static_cast<std::uint8_t>(value >> 24U);
    memory[offset + 1] = static_cast<std::uint8_t>(value >> 16U);
    memory[offset + 2] = static_cast<std::uint8_t>(value >> 8U);
    memory[offset + 3] = static_cast<std::uint8_t>(value);
}

void append_write(std::vector<DeuterosAmigaInterruptRegisterWrite>& writes,
    const std::uint32_t address, const DeuterosAmigaInterruptRegisterWidth width,
    const std::uint32_t value) {
    writes.push_back({writes.size() + 1, address, width, value});
}

} // namespace

DeuterosAmigaInstalledInterruptWorkerResult
evaluate_deuteros_amiga_installed_interrupt_worker(
    const DeuterosAmigaInstalledInterruptWorker& worker,
    const std::uint32_t memory_base_address,
    const std::span<const std::uint8_t> memory) {
    constexpr std::string_view expected_hash =
        "661854d6976ab520b0398e2545003d3fe59692fc0de54f0f810f379cf25ccaf8";
    if (worker.caller_address != 0x224ea || worker.entry_address != 0x22816
        || worker.source_disk_offset != 0x8016 || worker.source_length != 0x1d2
        || worker.return_instruction_address != 0x229e6
        || worker.raw_sha256 != expected_hash) {
        throw std::runtime_error("Unexpected Deuteros installed interrupt worker identity");
    }
    if (memory.empty() || static_cast<std::uint64_t>(memory_base_address) + memory.size()
            > (std::uint64_t{1} << 32U)) {
        throw std::runtime_error("Invalid Deuteros interrupt worker scenario memory range");
    }

    DeuterosAmigaInstalledInterruptWorkerResult result{
        memory_base_address, {memory.begin(), memory.end()}, {}};
    result.register_writes.reserve(17);

    constexpr std::array<std::uint32_t, 4> addend_cells{0x229ea, 0x229ec, 0x229ee, 0x229f0};
    constexpr std::array<std::uint32_t, 4> accumulator_cells{0x22a02, 0x22a04, 0x22a06, 0x22a08};
    constexpr std::array<std::uint32_t, 4> period_registers{0xdff0a6, 0xdff0b6, 0xdff0c6, 0xdff0d6};
    for (std::size_t index = 0; index < addend_cells.size(); ++index) {
        const auto sum = static_cast<std::uint16_t>(read_word(memory_base_address,
            result.memory, accumulator_cells[index])
            + read_word(memory_base_address, result.memory, addend_cells[index]));
        write_word(memory_base_address, result.memory, accumulator_cells[index], sum);
        append_write(result.register_writes, period_registers[index],
            DeuterosAmigaInterruptRegisterWidth::word, sum);
    }

    constexpr std::array<std::uint32_t, 4> pointer_cells{0x229f2, 0x229f6, 0x229fa, 0x229fe};
    constexpr std::array<std::uint32_t, 4> location_registers{0xdff0a0, 0xdff0b0, 0xdff0c0, 0xdff0d0};
    constexpr std::array<std::uint32_t, 4> length_registers{0xdff0a4, 0xdff0b4, 0xdff0c4, 0xdff0d4};
    const auto flags_offset = checked_offset(memory_base_address, result.memory.size(), 0x229e8, 1);
    auto flags = result.memory[flags_offset];
    for (std::size_t index = 0; index < pointer_cells.size(); ++index) {
        const auto mask = static_cast<std::uint8_t>(1U << index);
        if ((flags & mask) == 0) continue;
        flags = static_cast<std::uint8_t>(flags & static_cast<std::uint8_t>(~mask));
        result.memory[flags_offset] = flags;
        const auto pointer = read_long(memory_base_address, result.memory, pointer_cells[index]);
        const auto location_address = static_cast<std::uint64_t>(pointer) + 8U;
        const auto length_address = static_cast<std::uint64_t>(pointer) + 12U;
        if (length_address > std::numeric_limits<std::uint32_t>::max()) {
            throw std::runtime_error("Deuteros interrupt worker pointer addition overflows");
        }
        append_write(result.register_writes, location_registers[index],
            DeuterosAmigaInterruptRegisterWidth::longword,
            read_long(memory_base_address, result.memory, static_cast<std::uint32_t>(location_address)));
        append_write(result.register_writes, length_registers[index],
            DeuterosAmigaInterruptRegisterWidth::word,
            read_word(memory_base_address, result.memory, static_cast<std::uint32_t>(length_address)));
        write_long(memory_base_address, result.memory, pointer_cells[index], 0);
    }

    std::uint16_t enable_word = 0x8000;
    flags = result.memory[flags_offset];
    for (std::size_t index = 0; index < pointer_cells.size(); ++index) {
        if (read_long(memory_base_address, result.memory, pointer_cells[index]) == 0) continue;
        const auto mask = static_cast<std::uint8_t>(1U << index);
        flags = static_cast<std::uint8_t>(flags | mask);
        enable_word = static_cast<std::uint16_t>(enable_word | mask);
    }
    result.memory[flags_offset] = flags;
    append_write(result.register_writes, 0xdff096,
        DeuterosAmigaInterruptRegisterWidth::word, enable_word);

    const auto scale = read_word(memory_base_address, result.memory, 0x2229a);
    constexpr std::array<std::uint32_t, 4> volume_cells{0x22a0a, 0x22a0c, 0x22a0e, 0x22a10};
    constexpr std::array<std::uint32_t, 4> volume_registers{0xdff0a8, 0xdff0b8, 0xdff0c8, 0xdff0d8};
    for (std::size_t index = 0; index < volume_cells.size(); ++index) {
        const auto product = static_cast<std::uint32_t>(scale)
            * read_word(memory_base_address, result.memory, volume_cells[index]);
        // MULU.W fills D0.L; the following LSR.W affects only D0's low word.
        const auto value = static_cast<std::uint16_t>((product & 0xffffU) >> 8U);
        append_write(result.register_writes, volume_registers[index],
            DeuterosAmigaInterruptRegisterWidth::word, value);
    }
    return result;
}

} // namespace eon
