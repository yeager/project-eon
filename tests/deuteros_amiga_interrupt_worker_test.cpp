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
        constexpr std::array<std::uint8_t, 12> wait_source{
            0x08, 0x39, 0x00, 0x05, 0x00, 0xdf, 0xf0, 0x1f,
            0x67, 0xf6, 0x4e, 0x75};
        std::array<eon::DeuterosAmigaScenarioMemoryCell, wait_source.size()> owned{};
        for (std::size_t index = 0; index < wait_source.size(); ++index) {
            owned[index] = {0x216f2U + static_cast<std::uint32_t>(index), wait_source[index]};
        }
        const auto original = owned;
        const auto report = eon::evaluate_deuteros_amiga_vertb_wait_scenarios(
            wait_source, owned);
        assert(report.synthetic_scenarios_only);
        assert(report.source_sha256 ==
            "3ee8aeaeb214bfb7cf210c7575043595d87340370ea8f85eda94eb51d53961a7");
        assert(report.owned_image_sha256.size() == 64);
        assert(report.scenarios[0].label == "synthetic-bit-clear"
            && report.scenarios[0].synthetic_port_value == 0
            && !report.scenarios[0].vertb_bit_set
            && report.scenarios[0].remains_in_wait
            && report.scenarios[0].next_instruction_address == 0x216f2);
        assert(report.scenarios[1].label == "synthetic-bit-set"
            && report.scenarios[1].synthetic_port_value == 0x20
            && report.scenarios[1].vertb_bit_set
            && !report.scenarios[1].remains_in_wait
            && report.scenarios[1].next_instruction_address == 0x216fc);
        assert(report.missing_runtime_evidence[0].find("$208b4") != std::string::npos);
        assert(report.missing_runtime_evidence[1].find("$2079e") != std::string::npos);
        assert(report.missing_runtime_evidence[2].find("$216ee") != std::string::npos);
        assert(report.missing_runtime_evidence[3].find("$dff01f") != std::string::npos);
        assert(owned == original);

        auto wrong_source = wait_source;
        wrong_source[8] ^= 1U;
        bool rejected = false;
        try {
            static_cast<void>(eon::evaluate_deuteros_amiga_vertb_wait_scenarios(
                wrong_source, owned));
        } catch (const std::runtime_error&) { rejected = true; }
        assert(rejected);

        auto changed_owned_image = owned;
        changed_owned_image[9].value ^= 1U;
        rejected = false;
        try {
            static_cast<void>(eon::evaluate_deuteros_amiga_vertb_wait_scenarios(
                wait_source, changed_owned_image));
        } catch (const std::runtime_error&) { rejected = true; }
        assert(rejected);
    }
    {
        const auto profile = eon::DeuterosAmigaVertbHandlerProfile{
            0x207ec, 0x5fec, 0x100,
            "21356e0e8cfe8b1323a7ffa3a59c8b98720794330251862e9978447c91006d0f"};
        eon::DeuterosAmigaVertbSession session(profile);
        bool rejected = false;
        try {
            static_cast<void>(session.observe_idle_invocation({2, 0x207ec, 0, 0, 0x100}));
        } catch (const std::runtime_error&) { rejected = true; }
        assert(rejected);
        session.observe_registration({1, 0x208b4, 0x208b8, -0xa8, 5, 0x2086a, 0x207ec});
        const auto effect = session.observe_idle_invocation({2, 0x207ec, 0, 0, 0xfffffffeU});
        assert(effect.trace_sequence == 2 && effect.address == 0x2079e
            && effect.before == 0xfffffffeU && effect.after == 2U);
        rejected = false;
        try {
            static_cast<void>(session.observe_idle_invocation({3, 0x207ec, 1, 0, 2}));
        } catch (const std::runtime_error&) { rejected = true; }
        assert(rejected);
    }
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
        auto memory = fixture();
        memory[0x229e8 - base] = 0x01;
        put_long(memory, 0x229f2, 0x23001);
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
