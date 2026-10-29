#include "engine/millennium_dos_sibm_function_zero_session.hpp"
#include "engine/millennium_dos_sibm_timer_session.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace {

std::vector<std::uint8_t> read_exact(const std::filesystem::path& path,
    const std::size_t expected_size) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Missing direct Millennium DOS media leaf");
    std::vector<std::uint8_t> bytes(expected_size);
    stream.read(reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    if (stream.gcount() != static_cast<std::streamsize>(bytes.size())) {
        throw std::runtime_error("Millennium DOS media leaf has an unexpected size");
    }
    char trailing = 0;
    if (stream.get(trailing) || !stream.eof() || stream.bad()) {
        throw std::runtime_error("Millennium DOS media leaf is not exactly bounded");
    }
    return bytes;
}

template<typename F>
bool throws(F&& operation) {
    try {
        operation();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const std::filesystem::path root(argv[1]);
    const auto game_path = root / "2200AD.EXE";
    const auto sibm_path = root / "SIBM.DRV";
    if (!std::filesystem::is_regular_file(game_path)
        || !std::filesystem::is_regular_file(sibm_path)) return 77;

    const auto game = read_exact(game_path, 54'391);
    const auto sibm = read_exact(sibm_path, 2'871);
    eon::MillenniumDosSibmFunctionZeroSession function_zero(game, sibm);
    const auto prefix = function_zero.execute_startup_helper_prefix(1);
    assert(prefix.state == eon::MillenniumDosSibmFunctionZeroState::awaiting_timer_vector_get
        && prefix.function_input == 0x0380 && prefix.dos_ax == 0x3508
        && prefix.dos_interrupt_instruction == 0x0233);

    eon::MillenniumDosSibmTimerSession timer(sibm, prefix);
    const auto before_activation = timer.checkpoint();
    assert(before_activation.state
            == eon::MillenniumDosSibmTimerState::awaiting_native_activation
        && before_activation.driver_flags == 0x10
        && before_activation.software_current_divisor == 0
        && before_activation.countdown == 1
        && before_activation.timer_accumulator == 0
        && before_activation.timer_divisor == 0xffff
        && before_activation.sample_cursor_flag == 0
        && !before_activation.last_action);

    const auto active = timer.activate_native_callback();
    assert(active.state == eon::MillenniumDosSibmTimerState::running);
    const auto first_tick = timer.advance_callback();
    assert(first_tick.state == eon::MillenniumDosSibmTimerState::running
        && first_tick.callback_count == 1
        && first_tick.countdown == 1
        && first_tick.timer_accumulator == 0
        && first_tick.last_action
            == eon::MillenniumDosSibmTimerAction::request_host_clock_chain
        && first_tick.next_instruction_address == 0x0424);
    const auto second_tick = timer.advance_callback();
    assert(second_tick.callback_count == 2
        && second_tick.timer_accumulator == 0
        && second_tick.last_action
            == eon::MillenniumDosSibmTimerAction::request_host_clock_chain);
    const auto repeated_activation = timer.activate_native_callback();
    assert(repeated_activation.state == eon::MillenniumDosSibmTimerState::rejected
        && !repeated_activation.error.empty());

    auto altered_handler = sibm;
    altered_handler[0x034d] ^= 1U;
    assert(throws([&] {
        static_cast<void>(eon::MillenniumDosSibmTimerSession(altered_handler, prefix));
    }));
    auto detached_prefix = prefix;
    detached_prefix.dos_ax = 0x2508;
    assert(throws([&] {
        static_cast<void>(eon::MillenniumDosSibmTimerSession(sibm, detached_prefix));
    }));
    detached_prefix = prefix;
    detached_prefix.memory_effects[0].value ^= 1U;
    assert(throws([&] {
        static_cast<void>(eon::MillenniumDosSibmTimerSession(sibm, detached_prefix));
    }));
    return 0;
}
