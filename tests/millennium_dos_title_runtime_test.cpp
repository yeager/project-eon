#include "engine/release_runtime.hpp"
#include "platform/game_data.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>

// Genuine direct media, with explicitly synthetic external service returns.
// This checks coordinator wiring; it is not a captured driver/BIOS execution.
int test_millennium_dos_title_runtime(const std::filesystem::path& root) {
    const auto releases = eon::find_release_archives(root);
    const auto release = std::ranges::find_if(releases, [](const auto& item) {
        return item.sha256 == "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123";
    });
    assert(release != releases.end());
    const auto media = eon::VerifiedReleaseMedia::open(*release);
    const auto library = media.borrow(
        "6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678");
    assert(library && library->size() == 18907);
    eon::ResolvedLaunchRequest launch;
    launch.release = *release;
    launch.request.game = eon::Game::millennium;
    launch.request.platform = eon::Platform::dos;
    launch.request.release_language = "en";
    launch.request.release_sha256 = release->sha256;
    eon::ReleaseRuntimeCoordinator runtime;
    assert(runtime.acquire(launch));
    assert(runtime.observe_input(eon::RuntimeInputObservation::ascii('1'))
        == eon::RuntimeInputDisposition::boundary_reached);
    const auto driver = runtime.tick_millennium_dos_compatibility_runner();
    assert(driver && driver->boundary.instruction_address == 0x032f);
    assert(runtime.observe_millennium_dos_sound_driver_load(
        eon::MillenniumDosSoundDriverStackObservation{
            driver->last_sequence + 1, 0x032f, 0x05f7, 0xabcd}).accepted);
    assert(runtime.tick_millennium_dos_compatibility_runner());
    const auto checkpoint = [&]() {
        const auto value = runtime.millennium_dos_title_exec_entry_checkpoint();
        assert(value && value->title_initialization);
        return *value->title_initialization;
    };
    const auto sequence = [&]() { return checkpoint().last_sequence + 1; };
    assert(runtime.observe_millennium_dos_title_private_interrupt_result(
        {sequence(), 0x0127, 0x0129, 0x0101, 0x7202}).accepted);
    assert(runtime.drive_millennium_dos_session().accepted);
    assert(runtime.observe_millennium_dos_title_selected_callee_result(
        {sequence(), 0x0127, 0x0129, 0x1ad1, 0xabcd, 0x0202}).accepted);
    assert(runtime.drive_millennium_dos_session().accepted);
    for (unsigned index = 0; index < 16; ++index) {
        assert(runtime.observe_millennium_dos_title_continuation(
            eon::MillenniumDosTitleBiosResultObservation{
                sequence(), 0x046d, 0x046f, 0, 0}).accepted);
    }
    const auto loaded = runtime.drive_millennium_dos_session();
    assert(loaded.accepted && loaded.stop_before_address == 0x0fd8
        && loaded.external_observation_requirement
        && loaded.external_observation_requirement->kind
            == eon::MillenniumDosTitleExternalObservationKind::palette_bios_result);
    const auto palette = checkpoint();
    assert(palette.bios_boundary.ax == 0x1012 && palette.bios_boundary.cx == 0xff);
    const auto memory = runtime.native_runtime_memory_checkpoint();
    assert(memory);
    const auto byte_at = [&](std::uint16_t segment, std::uint16_t offset) {
        const auto found = std::ranges::find_if(memory->initialized_bytes, [&](const auto& cell) {
            return cell.location.address_space == eon::NativeRuntimeAddressSpace::dos_segmented
                && cell.location.segment == segment && cell.location.offset == offset;
        });
        assert(found != memory->initialized_bytes.end());
        return found->value;
    };
    assert(byte_at(palette.child_code_segment, 0x0e49) == 0x4f
        && byte_at(palette.child_code_segment, 0x0e48) == 0xa1);
    for (unsigned index = 0; index < 768; ++index)
        assert(byte_at(palette.child_code_segment, static_cast<std::uint16_t>(0x014c + index))
            == (*library)[0x25f9 + index]);
    const auto before = runtime.native_runtime_memory_diagnostics();
    assert(before);
    assert(!runtime.observe_millennium_dos_title_continuation(
        eon::MillenniumDosTitleBiosResultObservation{
            sequence(), 0x0fd8, 0x0fdb, 0, 0}).accepted);
    assert(checkpoint().last_sequence == palette.last_sequence
        && runtime.native_runtime_memory_diagnostics()->checksum == before->checksum);
    assert(runtime.observe_millennium_dos_title_continuation(
        eon::MillenniumDosTitleBiosResultObservation{
            sequence(), 0x0fd8, 0x0fda, 0x1357, 0x2468}).accepted);
    assert(checkpoint().continuation_address == 0x10f4);
    for (const auto site : {0x10f4, 0x1106, 0x110b, 0x111d}) {
        assert(runtime.observe_millennium_dos_title_continuation(
            eon::MillenniumDosTitleDosVectorResultObservation{sequence(),
                static_cast<std::uint16_t>(site), static_cast<std::uint16_t>(site + 2),
                0, 0, 0, 0}).accepted);
    }
    for (const auto site : {0x1ab9, 0x1ac1}) {
        assert(runtime.observe_millennium_dos_title_continuation(
            eon::MillenniumDosTitleSetupBiosResultObservation{sequence(),
                static_cast<std::uint16_t>(site), static_cast<std::uint16_t>(site + 2),
                0, 0, 0}).accepted);
    }
    assert(runtime.observe_millennium_dos_title_continuation(
        eon::MillenniumDosTitleFarWordsObservation{sequence(), 0x115d, 0, 0x70, 0, 0}).accepted);
    assert(runtime.observe_millennium_dos_title_continuation(
        eon::MillenniumDosTitleFarWordsObservation{sequence(), 0x12ad, 0, 0x24, 0, 0}).accepted);
    assert(checkpoint().selected_call_address == 0x1c07
        && checkpoint().selected_call_target == 0x1ac6);
    assert(runtime.observe_millennium_dos_title_selected_callee_result(
        {sequence(), 0x0127, 0x0129, 0x1ad1, 0, 0}).accepted);
    const auto repeated = runtime.drive_millennium_dos_session();
    assert(repeated.accepted && repeated.stop_before_address == 0x046d
        && repeated.external_observation_requirement
        && repeated.external_observation_requirement->kind
            == eon::MillenniumDosTitleExternalObservationKind::palette_bios_result);
    for (unsigned index = 0; index < 16; ++index) {
        const auto request = checkpoint().bios_boundary;
        assert(request.bx == index && request.dx_known_mask == 0xff00
            && request.dx_known_value == (*library)[0x25f9 + index * 3] << 8U
            && request.cx == (((*library)[0x25fa + index * 3] << 8U)
                | (*library)[0x25fb + index * 3]));
        assert(runtime.observe_millennium_dos_title_continuation(
            eon::MillenniumDosTitleBiosResultObservation{
                sequence(), 0x046d, 0x046f, 0, 0}).accepted);
    }
    assert(checkpoint().continuation_address == 0x0127
        && checkpoint().boundary.function == 0x0019);
    return 0;
}
