#include "data/deuteros_amiga_interrupt_worker.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::uint32_t base = 0x22000;
constexpr std::size_t size = 0x2000;
const auto worker = eon::DeuterosAmigaInstalledInterruptWorker{
    0x224ea, 0x22816, 0x8016, 0x1d2, 0x229e6,
    "661854d6976ab520b0398e2545003d3fe59692fc0de54f0f810f379cf25ccaf8"};

void put_word(std::vector<std::uint8_t>& memory, const std::uint32_t address,
    const std::uint16_t value) {
    const auto offset = static_cast<std::size_t>(address - base);
    memory[offset] = static_cast<std::uint8_t>(value >> 8U);
    memory[offset + 1] = static_cast<std::uint8_t>(value);
}

void put_long(std::vector<std::uint8_t>& memory, const std::uint32_t address,
    const std::uint32_t value) {
    put_word(memory, address, static_cast<std::uint16_t>(value >> 16U));
    put_word(memory, address + 2, static_cast<std::uint16_t>(value));
}

std::uint16_t get_word(const std::vector<std::uint8_t>& memory, const std::uint32_t address) {
    const auto offset = static_cast<std::size_t>(address - base);
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(memory[offset]) << 8U)
        | memory[offset + 1]);
}

std::uint32_t get_long(const std::vector<std::uint8_t>& memory, const std::uint32_t address) {
    return (static_cast<std::uint32_t>(get_word(memory, address)) << 16U)
        | get_word(memory, address + 2);
}

std::vector<std::uint8_t> fixture() {
    std::vector<std::uint8_t> memory(size, 0);
    put_word(memory, 0x2229a, 0x0100);
    put_word(memory, 0x229ea, 0x0003);
    put_word(memory, 0x229ec, 0x0004);
    put_word(memory, 0x229ee, 0x0005);
    put_word(memory, 0x229f0, 0x0006);
    put_word(memory, 0x22a02, 0xfffe);
    put_word(memory, 0x22a04, 0xffff);
    put_word(memory, 0x22a06, 0x0001);
    put_word(memory, 0x22a08, 0x0002);
    put_word(memory, 0x22a0a, 0xffff);
    put_word(memory, 0x22a0c, 0x0101);
    put_word(memory, 0x22a0e, 0x8080);
    put_word(memory, 0x22a10, 0x00ff);
    return memory;
}

} // namespace

int main() {
    {
        auto memory = fixture();
        memory[0x229e8 - base] = 0x05;
        put_long(memory, 0x229f2, 0x23000);
        put_long(memory, 0x229fa, 0x23100);
        put_long(memory, 0x23008, 0x12345678);
        put_word(memory, 0x2300c, 0x9abc);
        put_long(memory, 0x23108, 0xdeadbeef);
        put_word(memory, 0x2310c, 0x7654);
        const auto input = memory;
        const auto result = eon::evaluate_deuteros_amiga_installed_interrupt_worker(
            worker, base, memory);

        assert(memory == input);
        assert(result.memory_base_address == base);
        assert(result.register_writes.size() == 13);
        assert(result.register_writes[0] == (eon::DeuterosAmigaInterruptRegisterWrite{
            1, 0xdff0a6, eon::DeuterosAmigaInterruptRegisterWidth::word, 1}));
        assert(result.register_writes[1].address == 0xdff0b6
            && result.register_writes[1].value == 3);
        assert(result.register_writes[2].address == 0xdff0c6
            && result.register_writes[2].value == 6);
        assert(result.register_writes[3].address == 0xdff0d6
            && result.register_writes[3].value == 8);
        assert(result.register_writes[4].address == 0xdff0a0
            && result.register_writes[4].width == eon::DeuterosAmigaInterruptRegisterWidth::longword
            && result.register_writes[4].value == 0x12345678);
        assert(result.register_writes[5].address == 0xdff0a4
            && result.register_writes[5].value == 0x9abc);
        assert(result.register_writes[6].address == 0xdff0c0
            && result.register_writes[6].value == 0xdeadbeef);
        assert(result.register_writes[8].address == 0xdff096
            && result.register_writes[8].value == 0x8000);
        assert(result.register_writes[9].address == 0xdff0a8
            && result.register_writes[9].value == 0x00ff);
        assert(result.register_writes[10].address == 0xdff0b8
            && result.register_writes[10].value == 0x0001);
        assert(result.register_writes[11].address == 0xdff0c8
            && result.register_writes[11].value == 0x0080);
        assert(result.register_writes[12].address == 0xdff0d8
            && result.register_writes[12].value == 0x00ff);
        assert(result.memory[0x229e8 - base] == 0);
        assert(get_long(result.memory, 0x229f2) == 0);
        assert(get_long(result.memory, 0x229fa) == 0);
        assert(get_word(result.memory, 0x22a02) == 1);
        assert(get_word(result.memory, 0x22a04) == 3);
        assert(get_word(result.memory, 0x22a06) == 6);
        assert(get_word(result.memory, 0x22a08) == 8);
    }
    {
        auto memory = fixture();
        put_long(memory, 0x229f2, 0x23000);
        put_long(memory, 0x229fa, 0x23100);
        const auto result = eon::evaluate_deuteros_amiga_installed_interrupt_worker(
            worker, base, memory);
        assert(result.register_writes.size() == 9);
        assert(result.register_writes[4].address == 0xdff096
            && result.register_writes[4].value == 0x8005);
        assert(result.memory[0x229e8 - base] == 0x05);
        assert(get_long(result.memory, 0x229f2) == 0x23000);
        assert(get_long(result.memory, 0x229fa) == 0x23100);
    }
    {
        auto memory = fixture();
        memory[0x229e8 - base] = 0x0f;
        const std::array<std::uint32_t, 4> cells{0x229f2, 0x229f6, 0x229fa, 0x229fe};
        const std::array<std::uint32_t, 4> records{0x23000, 0x23100, 0x23200, 0x23300};
        const std::array<std::uint32_t, 4> custom{0xdff0a0, 0xdff0b0, 0xdff0c0, 0xdff0d0};
        for (std::size_t index = 0; index < cells.size(); ++index) {
            put_long(memory, cells[index], records[index]);
            put_long(memory, records[index] + 8, 0x10000000U + static_cast<std::uint32_t>(index));
            put_word(memory, records[index] + 12, static_cast<std::uint16_t>(0x2000 + index));
        }
        const auto result = eon::evaluate_deuteros_amiga_installed_interrupt_worker(
            worker, base, memory);
        assert(result.register_writes.size() == 17);
        for (std::size_t index = 0; index < custom.size(); ++index) {
            const auto first = 4 + index * 2;
            assert(result.register_writes[first].address == custom[index]);
            assert(result.register_writes[first].value == 0x10000000U + index);
            assert(result.register_writes[first + 1].address == custom[index] + 4);
            assert(result.register_writes[first + 1].value == 0x2000U + index);
            assert(get_long(result.memory, cells[index]) == 0);
        }
        assert(result.register_writes[12].address == 0xdff096
            && result.register_writes[12].value == 0x8000);
        assert(result.memory[0x229e8 - base] == 0);
    }
    {
        auto memory = fixture();
        memory[0x229e8 - base] = 0x01;
        put_long(memory, 0x229f2, 0x25000);
        const auto input = memory;
        bool rejected = false;
        try {
            static_cast<void>(eon::evaluate_deuteros_amiga_installed_interrupt_worker(
                worker, base, memory));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected && memory == input);
    }
    {
        auto altered = worker;
        altered.raw_sha256[0] = '0';
        const auto memory = fixture();
        bool rejected = false;
        try {
            static_cast<void>(eon::evaluate_deuteros_amiga_installed_interrupt_worker(
                altered, base, memory));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
}
