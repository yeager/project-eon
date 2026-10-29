#include "data/millennium_dos_sound_driver.hpp"
#include "engine/millennium_dos_game_sound_dispatch.hpp"

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
    const auto profile = eon::parse_millennium_dos_sibm_startup_profile(game, sibm);
    assert(profile.function_zero_target == 0x04a8
        && profile.function_eleven_target == 0x09e7
        && profile.function_twelve_target == 0x0a27
        && profile.first_private_interrupt_address == 0xcf98);
    assert(profile.function_zero_wrapper_address == 0xcf9f
        && profile.function_twelve_wrapper_address == 0xcfad
        && profile.function_eleven_wrapper_address == 0xcfb9
        && profile.function_eleven_argument_table == 0xd05c);
    assert(profile.function_eleven_name_addresses.front() == 0xcfdd
        && profile.function_eleven_name_addresses.back() == 0xd052
        && profile.function_eleven_filenames.front() == "SFX1.VOC"
        && profile.function_eleven_filenames.back() == "SFXE.VOC");
    assert(profile.function_twelve_requested_bytes == 15
        && profile.function_eleven_first_argument == 1
        && profile.function_eleven_last_argument == 14
        && profile.function_eleven_call_count == 14);
    assert(profile.function_zero_input_for_mode_one == 0x0380
        && profile.function_zero_input_for_other_modes == 0x0220
        && profile.function_eleven_argument_table_sha256
            == "b741567f5563ed48fd2050146acad4018020b122da50537493bbc2b896ad06ed");

    const auto voice_three = eon::evaluate_millennium_dos_game_sound_dispatch(game,
        {1, 0xd0c9, 0x7b03, 0xa500, 0, 0xd0e7});
    assert(voice_three.state
            == eon::MillenniumDosGameSoundDispatchState::sibm_function_0e
        && voice_three.private_function == 0x0e
        && voice_three.next_instruction_address == 0xcfc7
        && voice_three.voice_index == 3
        && voice_three.secondary_argument == 0x7b
        && voice_three.caller_bh_discarded);
    const auto voice_fourteen = eon::evaluate_millennium_dos_game_sound_dispatch(game,
        {2, 0xd0c9, 0xff0e, 0x0000, 0, 0xd0e7});
    assert(voice_fourteen.state
            == eon::MillenniumDosGameSoundDispatchState::sibm_function_0e
        && voice_fourteen.voice_index == 14
        && voice_fourteen.secondary_argument == 0xff);
    const auto no_voice = eon::evaluate_millennium_dos_game_sound_dispatch(game,
        {3, 0xd0c9, 0x1200, 0xffff, 0, 0xd0e7});
    assert(no_voice.state == eon::MillenniumDosGameSoundDispatchState::sibm_function_10
        && no_voice.private_function == 0x10
        && no_voice.next_instruction_address == 0xcfd4
        && no_voice.voice_index == 0);
    const auto waits_for_video = eon::evaluate_millennium_dos_game_sound_dispatch(game,
        {4, 0xd0c9, 0x1203, 0xffff, 0, 0});
    assert(waits_for_video.state
            == eon::MillenniumDosGameSoundDispatchState::awaiting_video_function_return
        && waits_for_video.next_instruction_address == 0xd0e7);
    const auto wrong_video_return = eon::evaluate_millennium_dos_game_sound_dispatch(game,
        {7, 0xd0c9, 0x1203, 0xffff, 0, 0xd0e9});
    assert(wrong_video_return.state == eon::MillenniumDosGameSoundDispatchState::rejected
        && !wrong_video_return.error.empty());
    const auto selector_blocked = eon::evaluate_millennium_dos_game_sound_dispatch(game,
        {5, 0xd0c9, 0x1203, 0xffff, 1, 0});
    assert(selector_blocked.state
            == eon::MillenniumDosGameSoundDispatchState::selector_gate_blocked
        && selector_blocked.next_instruction_address == 0xd101);
    auto detached_game = game;
    detached_game[0xcfc9] ^= 1U;
    const auto changed_source = eon::evaluate_millennium_dos_game_sound_dispatch(detached_game,
        {6, 0xd0c9, 0x0003, 0, 0, 0xd0e7});
    assert(changed_source.state == eon::MillenniumDosGameSoundDispatchState::rejected
        && !changed_source.error.empty());

    auto detached_driver = sibm;
    detached_driver[0x20] ^= 1U;
    bool rejected_dispatch_change = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_sibm_startup_profile(
            game, detached_driver));
    } catch (const std::runtime_error&) {
        rejected_dispatch_change = true;
    }
    assert(rejected_dispatch_change);
    return 0;
}
