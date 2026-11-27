#include "engine/release_runtime.hpp"
#include "data/millennium_dos_sound_driver.hpp"
#include "platform/game_data.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <stdexcept>
#include <vector>

// Genuine direct media, with explicitly synthetic external service returns.
// This checks coordinator wiring; it is not a captured driver/BIOS execution.
int test_millennium_dos_title_runtime(const std::filesystem::path& root) {
    const auto releases = eon::find_release_archives(root);
    const auto release = std::ranges::find_if(releases, [](const auto& item) {
        return item.sha256 == "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123";
    });
    assert(release != releases.end());
    const auto media = eon::VerifiedReleaseMedia::open(*release);
    const auto launcher = media.borrow(
        "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e");
    assert(launcher && launcher->size() == 1445);
    const auto parent_stack = eon::parse_millennium_dos_exec_parent_stack_profile(*launcher);
    assert(parent_stack.entry_stack_pointer == 0x0100
        && parent_stack.helper_call_address == 0x0240
        && parent_stack.helper_entry_address == 0x031c
        && parent_stack.stack_store_address == 0x032f
        && parent_stack.stack_store_destination == 0x05f7
        && parent_stack.stack_at_store == 0x00fe
        && parent_stack.helper_return_address == 0x0243);
    std::vector<std::uint8_t> altered_launcher(launcher->begin(), launcher->end());
    altered_launcher[0x0140] ^= 0x01;
    bool altered_stack_path_rejected = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_exec_parent_stack_profile(
            altered_launcher));
    } catch (const std::runtime_error&) {
        altered_stack_path_rejected = true;
    }
    assert(altered_stack_path_rejected);
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
    const auto startup = runtime.millennium_dos_startup_input();
    assert(startup && startup->sound_selection_active
        && !startup->selected_original_filename);
    assert(runtime.observe_input(eon::RuntimeInputObservation::ascii('1'))
        == eon::RuntimeInputDisposition::boundary_reached);
    const auto title_request = runtime.tick_millennium_dos_compatibility_runner();
    assert(title_request && title_request->boundary.instruction_address == 0x0336
        && title_request->title_exec_requested
        && title_request->last_sequence == 14
        && title_request->automatic_operation_count == 13);
    assert(!runtime.observe_millennium_dos_sound_driver_load(
        eon::MillenniumDosSoundDriverStackObservation{
            title_request->last_sequence + 1, 0x032f, 0x05f7, 0xabcd}).accepted);
    const auto checkpoint = [&]() {
        const auto value = runtime.millennium_dos_title_exec_entry_checkpoint();
        assert(value && value->title_initialization);
        return *value->title_initialization;
    };
    const auto title_entry = runtime.millennium_dos_title_exec_entry_checkpoint();
    assert(title_entry && title_entry->title_initialization
        && title_entry->title_initialization->boundary.call_address == 0x1b95
        && title_entry->title_initialization->boundary.interrupt == 0x91
        && title_entry->title_initialization->boundary.function == 0
        && !title_entry->title_initialization->boundary.result_observed);
    const auto sequence = [&]() { return checkpoint().last_sequence + 1; };
    assert(runtime.observe_millennium_dos_title_private_interrupt_result(
        {sequence(), 0x0127, 0x0129, 0x0101, 0x7202}).accepted);
    assert(runtime.drive_millennium_dos_session().accepted);
    assert(runtime.observe_millennium_dos_title_selected_callee_result(
        {sequence(), 0x0127, 0x0129, 0x1ad1, 0xabcd, 0x0202}).accepted);
    const auto loaded = runtime.drive_millennium_dos_session();
    assert(loaded.accepted && loaded.stop_before_address == 0x10f4
        && loaded.external_observation_requirement
        && loaded.external_observation_requirement->kind
            == eon::MillenniumDosTitleExternalObservationKind::dos_vector_result);
    const auto palette = checkpoint();
    assert(palette.state
            == eon::MillenniumDosTitleInitializationState::dos_get_vector_zero_result_boundary
        && palette.bios_results.size() == 17
        && palette.palette_writes.size() == 271
        && std::ranges::all_of(palette.bios_results, [](const auto& result) {
            return result.native_compatibility_result;
        }));
    for (unsigned index = 0; index < 16; ++index) {
        const auto& write = palette.palette_writes[index];
        assert(write.kind == eon::MillenniumDosTitlePaletteWriteKind::dac_color_register
            && write.register_index == index
            && write.source_address == 0x014c + index * 3);
    }
    for (unsigned index = 0; index < 255; ++index) {
        const auto& write = palette.palette_writes[16 + index];
        assert(write.kind == eon::MillenniumDosTitlePaletteWriteKind::dac_color_register
            && write.register_index == index
            && write.source_address == 0x014c + index * 3
            && write.red == (*library)[0x25f9 + index * 3]
            && write.green == (*library)[0x25fa + index * 3]
            && write.blue == (*library)[0x25fb + index * 3]);
    }
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
    assert(checkpoint().continuation_address == 0x10f4);
    for (const auto site : {0x10f4, 0x1106, 0x110b, 0x111d}) {
        assert(runtime.observe_millennium_dos_title_continuation(
            eon::MillenniumDosTitleDosVectorResultObservation{sequence(),
                static_cast<std::uint16_t>(site), static_cast<std::uint16_t>(site + 2),
                0, 0, 0, 0}).accepted);
    }
    const auto owned_memory = runtime.native_runtime_memory_checkpoint();
    assert(owned_memory);
    const auto read_dos_byte = [&](const std::uint16_t segment, const std::uint16_t offset) {
        const auto found = std::find_if(owned_memory->initialized_bytes.begin(),
            owned_memory->initialized_bytes.end(), [&](const auto& cell) {
                return cell.location.address_space == eon::NativeRuntimeAddressSpace::dos_segmented
                    && cell.location.segment == segment && cell.location.offset == offset;
            });
        return found == owned_memory->initialized_bytes.end()
            ? std::optional<std::uint8_t>{} : std::optional<std::uint8_t>{found->value};
    };
    assert(read_dos_byte(0, 0x00) == 0x24 && read_dos_byte(0, 0x01) == 0x11);
    assert(read_dos_byte(0, 0x02) == static_cast<std::uint8_t>(palette.child_code_segment)
        && read_dos_byte(0, 0x03) == static_cast<std::uint8_t>(palette.child_code_segment >> 8));
    assert(read_dos_byte(0, 0x10) == 0x24 && read_dos_byte(0, 0x11) == 0x11);
    assert(read_dos_byte(0, 0x12) == static_cast<std::uint8_t>(palette.child_code_segment)
        && read_dos_byte(0, 0x13) == static_cast<std::uint8_t>(palette.child_code_segment >> 8));
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
    const auto before_selected_result = runtime.native_runtime_memory_diagnostics();
    const auto before_selected_checkpoint = checkpoint();
    assert(before_selected_result);
    assert(!runtime.observe_millennium_dos_title_selected_callee_result(
        {sequence(), 0x0127, 0x0129, 0x1ad0, 0x7777, 0x0202}).accepted);
    assert(checkpoint().last_sequence == before_selected_checkpoint.last_sequence
        && checkpoint().continuation_address == 0x0127
        && runtime.native_runtime_memory_diagnostics()->checksum
            == before_selected_result->checksum);
    assert(runtime.observe_millennium_dos_title_selected_callee_result(
        {sequence(), 0x0127, 0x0129, 0x1ad1, 0, 0}).accepted);
    const auto repeated = runtime.drive_millennium_dos_session();
    assert(repeated.accepted && repeated.stop_before_address == 0x0127
        && checkpoint().bios_results.size() == 33
        && checkpoint().palette_writes.size() == 287
        && std::ranges::all_of(checkpoint().bios_results, [](const auto& result) {
            return result.native_compatibility_result;
        }));
    assert(checkpoint().continuation_address == 0x0127
        && checkpoint().boundary.function == 0x0019);
    return 0;
}
