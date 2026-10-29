#include "engine/release_runtime.hpp"
#include "engine/millennium_dos_file_read_session.hpp"
#include "platform/game_data.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <optional>
#include <string>

// Genuine direct media, with explicitly synthetic external service returns.
// This checks coordinator wiring; it is not a captured driver/BIOS execution.
int test_millennium_dos_title_runtime(const std::filesystem::path& root) {
    const auto releases = eon::find_release_archives(root);
    const auto release = std::ranges::find_if(releases, [](const auto& item) {
        return item.sha256 == "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123";
    });
    assert(release != releases.end());
    const auto media = eon::VerifiedReleaseMedia::open(*release);
    const auto vga_text = media.borrow(
        "c31cb760d5f62a21b3baf9c09a6be413514780bd88eeac0273620e81b5d69318");
    assert(vga_text && vga_text->size() == 1024);
    {
        eon::NativeRuntimeMemory text_memory;
        const auto loaded = eon::commit_millennium_dos_native_file_read(
            text_memory, 1, 2, 0x4321, 0, 0xffff,
            static_cast<std::uint16_t>(vga_text->size()), false, *vga_text);
        assert(loaded.accepted && loaded.committed_byte_count == vga_text->size());
        for (std::size_t index = 0; index < vga_text->size(); ++index) {
            assert(text_memory.read_byte({eon::NativeRuntimeAddressSpace::dos_segmented,
                0x4321, index}) == std::optional<std::uint8_t>{(*vga_text)[index]});
        }
        const auto before_conflict = text_memory.checkpoint();
        const auto wrong_count = eon::commit_millennium_dos_native_file_read(
            text_memory, 1, 3, 0x4321, 0, 0xffff,
            static_cast<std::uint16_t>(vga_text->size() - 1U), false, *vga_text);
        const auto failed_read = eon::commit_millennium_dos_native_file_read(
            text_memory, 1, 4, 0x4321, 0, 0xffff,
            static_cast<std::uint16_t>(vga_text->size()), true, *vga_text);
        assert(!wrong_count.accepted && !failed_read.accepted
            && text_memory.checkpoint().checksum == before_conflict.checksum
            && text_memory.checkpoint().applied_batch_count
                == before_conflict.applied_batch_count);
        const auto idempotent = eon::commit_millennium_dos_native_file_read(
            text_memory, 1, 5, 0x4321, 0, 0xffff,
            static_cast<std::uint16_t>(vga_text->size()), false, *vga_text);
        assert(idempotent.accepted
            && text_memory.checkpoint().applied_batch_count
                == before_conflict.applied_batch_count);
        eon::NativeRuntimeMemory conflicting_text_memory;
        assert(conflicting_text_memory.apply({"conflicting-vga-text-preimage", true, {{1,
            {eon::NativeRuntimeAddressSpace::dos_segmented, 0x4321, 0},
            eon::MemoryTransferElementWidth::byte,
            eon::NativeRuntimeByteOrder::little_endian,
            static_cast<std::uint8_t>((*vga_text)[0] ^ 1U)}}}).accepted);
        const auto before_rejection = conflicting_text_memory.checkpoint();
        const auto conflicting_read = eon::commit_millennium_dos_native_file_read(
            conflicting_text_memory, 1, 6, 0x4321, 0, 0xffff,
            static_cast<std::uint16_t>(vga_text->size()), false, *vga_text);
        assert(!conflicting_read.accepted
            && conflicting_text_memory.checkpoint().checksum == before_rejection.checksum
            && conflicting_text_memory.checkpoint().applied_batch_count
                == before_rejection.applied_batch_count);
    }
    const auto library = media.borrow(
        "6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678");
    assert(library && library->size() == 18907);
    const auto titles = media.borrow(
        "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6");
    assert(titles && titles->size() == 7022);
    eon::MillenniumDosNativeProcessCheckpoint owned_byte_process;
    owned_byte_process.release_sha256 = release->sha256;
    owned_byte_process.game_executable_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    owned_byte_process.state =
        eon::MillenniumDosNativeProcessState::startup_zero_path_continuation_byte_boundary;
    owned_byte_process.boundary = {eon::MillenniumDosNativeBoundaryKind::runtime_byte,
        0xd2fd, std::nullopt, 0x0082, 0, std::nullopt};
    owned_byte_process.observed_child_process_entry = true;
    owned_byte_process.child_entry_sequence = 7;
    owned_byte_process.child_code_segment = 0x1234;
    owned_byte_process.last_observation_sequence = 48;
    eon::NativeRuntimeMemory owned_byte_memory;
    assert(owned_byte_memory.apply({"test-owned-selector", true, {{1,
        {eon::NativeRuntimeAddressSpace::dos_segmented, 0x1234, 0x0082},
        eon::MemoryTransferElementWidth::byte,
        eon::NativeRuntimeByteOrder::little_endian, 0x31}, {2,
        {eon::NativeRuntimeAddressSpace::dos_segmented, 0x1234, 0x0122},
        eon::MemoryTransferElementWidth::byte,
        eon::NativeRuntimeByteOrder::little_endian, 0x00}}}).accepted);
    const auto owned_byte_request = eon::derive_millennium_dos_owned_runtime_byte_request(
        owned_byte_process, owned_byte_memory, 50);
    assert(owned_byte_request.accepted && owned_byte_request.observation
        && owned_byte_request.observation->sequence == 51
        && owned_byte_request.observation->instruction_address == 0xd2fd
        && owned_byte_request.observation->runtime_address == 0x0082
        && owned_byte_request.observation->value == 0x31
        && static_cast<std::uint8_t>(owned_byte_request.observation->value - 0x30U) == 1);
    const auto missing_owned_byte = eon::derive_millennium_dos_owned_runtime_byte_request(
        owned_byte_process, eon::NativeRuntimeMemory{}, 50);
    assert(!missing_owned_byte.accepted && !missing_owned_byte.observation);
    eon::NativeRuntimeMemory missing_target_memory;
    assert(missing_target_memory.apply({"test-source-only", true, {{1,
        {eon::NativeRuntimeAddressSpace::dos_segmented, 0x1234, 0x0082},
        eon::MemoryTransferElementWidth::byte,
        eon::NativeRuntimeByteOrder::little_endian, 0x31}}}).accepted);
    const auto missing_target_request =
        eon::derive_millennium_dos_owned_runtime_byte_request(
            owned_byte_process, missing_target_memory, 50);
    assert(!missing_target_request.accepted && !missing_target_request.observation
        && missing_target_request.error.find("CS:$0122") != std::string::npos);

    eon::NativeRuntimeMemory post_overlay_memory;
    const eon::MillenniumDosPostOverlayRuntimeByteEffect toggle_effect{0x07f9, 0, 1};
    const auto committed_toggle =
        eon::commit_millennium_dos_post_overlay_runtime_byte_effect(
            post_overlay_memory, 7, 1, 0x1234, toggle_effect);
    assert(committed_toggle.accepted
        && post_overlay_memory.read_byte({eon::NativeRuntimeAddressSpace::dos_segmented,
            0x1234, 0x07f9}) == std::optional<std::uint8_t>{1});
    eon::NativeRuntimeMemory conflicting_post_overlay_memory;
    assert(conflicting_post_overlay_memory.apply({"test-post-overlay-preimage", true, {{1,
        {eon::NativeRuntimeAddressSpace::dos_segmented, 0x1234, 0x07f9},
        eon::MemoryTransferElementWidth::byte,
        eon::NativeRuntimeByteOrder::little_endian, 2}}}).accepted);
    const auto rejected_toggle =
        eon::commit_millennium_dos_post_overlay_runtime_byte_effect(
            conflicting_post_overlay_memory, 7, 1, 0x1234, toggle_effect);
    assert(!rejected_toggle.accepted
        && conflicting_post_overlay_memory.read_byte({eon::NativeRuntimeAddressSpace::dos_segmented,
            0x1234, 0x07f9}) == std::optional<std::uint8_t>{2});

    eon::NativeRuntimeMemory shared_helper_memory;
    const eon::MillenniumDosSharedHelperEffect shared_helper_effect{0x066e, 0x05c8, 0};
    assert(shared_helper_memory.apply({"test-shared-helper-preimage", true, {{1,
        {eon::NativeRuntimeAddressSpace::dos_segmented, 0x1234, 0x05c8},
        eon::MemoryTransferElementWidth::byte,
        eon::NativeRuntimeByteOrder::little_endian, 0x7f}}}).accepted);
    const auto committed_helper_store = eon::commit_millennium_dos_shared_helper_effect(
        shared_helper_memory, 7, 1, 0x1234, shared_helper_effect);
    assert(committed_helper_store.accepted
        && shared_helper_memory.read_byte({eon::NativeRuntimeAddressSpace::dos_segmented,
            0x1234, 0x05c8}) == std::optional<std::uint8_t>{0});
    const auto rejected_helper_store = eon::commit_millennium_dos_shared_helper_effect(
        shared_helper_memory, 7, 2, 0x1234, {0x066e, 0x05c9, 0});
    assert(!rejected_helper_store.accepted
        && shared_helper_memory.read_byte({eon::NativeRuntimeAddressSpace::dos_segmented,
            0x1234, 0x05c8}) == std::optional<std::uint8_t>{0});
    eon::NativeRuntimeMemory shared_helper_table_memory;
    const auto committed_table_word =
        eon::commit_millennium_dos_shared_helper_table_word(
            shared_helper_table_memory, 7, 0x4321, 0x031e, 0x2468);
    assert(committed_table_word.accepted
        && shared_helper_table_memory.read_byte({eon::NativeRuntimeAddressSpace::dos_segmented,
            0x4321, 0x031e}) == std::optional<std::uint8_t>{0x68}
        && shared_helper_table_memory.read_byte({eon::NativeRuntimeAddressSpace::dos_segmented,
            0x4321, 0x031f}) == std::optional<std::uint8_t>{0x24});
    const auto rejected_table_word =
        eon::commit_millennium_dos_shared_helper_table_word(
            shared_helper_table_memory, 7, 0x4321, 0x0321, 0x1234);
    assert(!rejected_table_word.accepted
        && !shared_helper_table_memory.read_byte({eon::NativeRuntimeAddressSpace::dos_segmented,
            0x4321, 0x0321}));

    const auto game_executable = media.borrow(
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57");
    assert(game_executable);
    eon::MillenniumDosSharedHelperSession owned_helper(*game_executable, 0x018f);
    eon::NativeRuntimeMemory owned_helper_memory;
    assert(owned_helper_memory.apply({"owned-shared-helper-operands", true, {
        {1, {eon::NativeRuntimeAddressSpace::dos_segmented, 0x1234, 0x0116},
            eon::MemoryTransferElementWidth::word,
            eon::NativeRuntimeByteOrder::little_endian, 0x4321},
        {2, {eon::NativeRuntimeAddressSpace::dos_segmented, 0x4321, 0x031e},
            eon::MemoryTransferElementWidth::word,
            eon::NativeRuntimeByteOrder::little_endian, 0x2468}}}).accepted);
    const auto owned_helper_advance =
        eon::advance_millennium_dos_shared_helper_from_owned_memory(
            owned_helper, owned_helper_memory, 9, 0x1234);
    assert(owned_helper_advance.accepted && owned_helper_advance.boundary
        && owned_helper_advance.boundary->instruction_address == 0x067b
        && owned_helper_advance.boundary->address == 0x05f7
        && owned_helper.selected_offset() == 0x2468
        && owned_helper_memory.read_byte({eon::NativeRuntimeAddressSpace::dos_segmented,
            0x1234, 0x05c8}) == std::optional<std::uint8_t>{0});
    const auto helper_memory_revision = owned_helper_memory.revision();
    eon::MillenniumDosSharedHelperSession repeated_owned_helper(*game_executable, 0x018f);
    const auto repeated_owned_helper_advance =
        eon::advance_millennium_dos_shared_helper_from_owned_memory(
            repeated_owned_helper, owned_helper_memory, 9, 0x1234);
    assert(repeated_owned_helper_advance.accepted
        && repeated_owned_helper_advance.boundary
        && owned_helper_memory.revision() == helper_memory_revision + 1U);

    eon::MillenniumDosSharedHelperSession missing_owned_helper(*game_executable, 0x018f);
    eon::NativeRuntimeMemory missing_owned_helper_memory;
    const auto missing_owned_helper_advance =
        eon::advance_millennium_dos_shared_helper_from_owned_memory(
            missing_owned_helper, missing_owned_helper_memory, 9, 0x1234);
    assert(!missing_owned_helper_advance.accepted
        && missing_owned_helper.state() == eon::MillenniumDosSharedHelperState::awaiting_segment
        && missing_owned_helper_memory.diagnostics().initialized_byte_count == 0);

    eon::MillenniumDosSharedHelperSession partial_owned_helper(*game_executable, 0x018f);
    eon::NativeRuntimeMemory partial_owned_helper_memory;
    assert(partial_owned_helper_memory.apply({"partial-owned-helper-segment", true, {{1,
        {eon::NativeRuntimeAddressSpace::dos_segmented, 0x1234, 0x0116},
        eon::MemoryTransferElementWidth::byte,
        eon::NativeRuntimeByteOrder::little_endian, 0x21}}}).accepted);
    const auto partial_owned_helper_advance =
        eon::advance_millennium_dos_shared_helper_from_owned_memory(
            partial_owned_helper, partial_owned_helper_memory, 9, 0x1234);
    assert(!partial_owned_helper_advance.accepted
        && partial_owned_helper.state() == eon::MillenniumDosSharedHelperState::awaiting_segment
        && !partial_owned_helper_memory.read_byte({eon::NativeRuntimeAddressSpace::dos_segmented,
            0x1234, 0x05c8}));

    eon::MillenniumDosSharedHelperSession missing_table_helper(*game_executable, 0x018f);
    eon::NativeRuntimeMemory missing_table_memory;
    assert(missing_table_memory.apply({"owned-helper-segment-only", true, {{1,
        {eon::NativeRuntimeAddressSpace::dos_segmented, 0x1234, 0x0116},
        eon::MemoryTransferElementWidth::word,
        eon::NativeRuntimeByteOrder::little_endian, 0x4321}}}).accepted);
    const auto missing_table_advance =
        eon::advance_millennium_dos_shared_helper_from_owned_memory(
            missing_table_helper, missing_table_memory, 9, 0x1234);
    assert(!missing_table_advance.accepted
        && missing_table_helper.state() == eon::MillenniumDosSharedHelperState::awaiting_segment
        && !missing_table_memory.read_byte({eon::NativeRuntimeAddressSpace::dos_segmented,
            0x1234, 0x05c8}));

    eon::MillenniumDosSharedHelperSession partial_table_helper(*game_executable, 0x018f);
    eon::NativeRuntimeMemory partial_table_memory;
    assert(partial_table_memory.apply({"partial-owned-helper-table", true, {
        {1, {eon::NativeRuntimeAddressSpace::dos_segmented, 0x1234, 0x0116},
            eon::MemoryTransferElementWidth::word,
            eon::NativeRuntimeByteOrder::little_endian, 0x4321},
        {2, {eon::NativeRuntimeAddressSpace::dos_segmented, 0x4321, 0x031e},
            eon::MemoryTransferElementWidth::byte,
            eon::NativeRuntimeByteOrder::little_endian, 0x68}}}).accepted);
    const auto partial_table_advance =
        eon::advance_millennium_dos_shared_helper_from_owned_memory(
            partial_table_helper, partial_table_memory, 9, 0x1234);
    assert(!partial_table_advance.accepted
        && partial_table_helper.state() == eon::MillenniumDosSharedHelperState::awaiting_segment
        && !partial_table_memory.read_byte({eon::NativeRuntimeAddressSpace::dos_segmented,
            0x1234, 0x05c8}));

    auto wrong_owned_byte_boundary = owned_byte_process;
    wrong_owned_byte_boundary.boundary.address = 0xd2fe;
    const auto wrong_boundary_request = eon::derive_millennium_dos_owned_runtime_byte_request(
        wrong_owned_byte_boundary, owned_byte_memory, 50);
    assert(!wrong_boundary_request.accepted && !wrong_boundary_request.observation);
    eon::ResolvedLaunchRequest launch;
    launch.release = *release;
    launch.request.game = eon::Game::millennium;
    launch.request.platform = eon::Platform::dos;
    launch.request.release_language = "en";
    launch.request.release_sha256 = release->sha256;
    eon::ReleaseRuntimeCoordinator runtime;
    assert(runtime.acquire(launch));
    const auto before_owned_runtime_byte = runtime.millennium_dos_native_process_checkpoint();
    assert(before_owned_runtime_byte);
    const auto rejected_owned_runtime_byte =
        runtime.execute_millennium_dos_native_runtime_byte_from_owned_memory();
    assert(!rejected_owned_runtime_byte.accepted);
    assert(runtime.millennium_dos_native_process_checkpoint()
        ->last_observation_sequence == before_owned_runtime_byte->last_observation_sequence);
    assert(runtime.observe_input(eon::RuntimeInputObservation::ascii('1'))
        == eon::RuntimeInputDisposition::boundary_reached);
    const auto driver = runtime.tick_millennium_dos_compatibility_runner();
    assert(driver && driver->state
        == eon::MillenniumDosSoundDriverLoadState::title_exec_requested
        && driver->title_exec_requested && driver->external_result_required);
    const auto driver_load = runtime.millennium_dos_sound_driver_load_checkpoint();
    assert(driver_load && driver_load->load_segment != 0
        && driver_load->installed_int95_vector
        && *driver_load->installed_int95_vector
            == (eon::MillenniumDosSoundDriverIvtVector{
                0x95, 0, driver_load->load_segment})
        && driver_load->vector_install_provenance
            == eon::MillenniumDosSoundDriverVectorInstallProvenance::eon_dos_compatibility_service
        && driver_load->state == eon::MillenniumDosSoundDriverLoadState::title_exec_requested
        && driver_load->parent_stack_provenance
            == eon::MillenniumDosSoundDriverParentStackProvenance::eon_dos_compatibility_service
        && driver_load->runtime_word_effects.size() == 4
        && driver_load->runtime_word_effects.front().segment == driver->code_segment
        && driver_load->runtime_word_effects.front().address == 0x05f7
        && driver_load->runtime_word_effects.front().value == 0x00fe);
    const auto title_entry = runtime.millennium_dos_title_exec_entry_checkpoint();
    assert(title_entry && title_entry->entry.state
        == eon::MillenniumDosTitleExecEntryState::title_entry_boundary
        && title_entry->entry.provenance
            == eon::MillenniumDosTitleExecEntryProvenance::eon_dos_compatibility_service
        && title_entry->compatibility_child
        && title_entry->compatibility_child->admitted_image_byte_count == titles->size()
        && !title_entry->compatibility_child->psp_modeled
        && !title_entry->compatibility_child->environment_modeled);
    assert(title_entry->title_initialization
        && title_entry->title_initialization->state
            == eon::MillenniumDosTitleInitializationState::private_interrupt_result_boundary
        && title_entry->title_initialization->last_sequence == driver->last_sequence
        && title_entry->title_initialization->boundary.interrupt_address == 0x0127);
    const auto child_memory = runtime.native_runtime_memory_checkpoint();
    assert(child_memory);
    const auto child_segment = title_entry->entry.child_code_segment;
    std::size_t child_image_bytes = 0;
    for (std::size_t index = 0; index < titles->size(); ++index) {
        const auto address = static_cast<std::uint16_t>(0x0100 + index);
        const auto found = std::ranges::find_if(child_memory->initialized_bytes,
            [child_segment, address](const auto& cell) {
                return cell.location.address_space
                        == eon::NativeRuntimeAddressSpace::dos_segmented
                    && cell.location.segment == child_segment
                    && cell.location.offset == address;
            });
        assert(found != child_memory->initialized_bytes.end()
            && found->value == (*titles)[index]);
        ++child_image_bytes;
    }
    assert(child_image_bytes == titles->size());
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
    const auto palette_drive = runtime.drive_millennium_dos_session();
    assert(palette_drive.accepted
        &&palette_drive.stop_before_address==0x0127
        &&palette_drive.external_observation_requirement
        &&palette_drive.external_observation_requirement->kind
            ==eon::MillenniumDosTitleExternalObservationKind::title_private_interrupt_result);
    const auto palette_checkpoint = checkpoint();
    assert(std::ranges::count_if(palette_checkpoint.bios_results,
        [](const auto& result){return result.interrupt_address==0x046d;})==16
        &&std::ranges::all_of(palette_checkpoint.bios_results,
            [](const auto& result){return result.interrupt_address!=0x046d
                ||(!result.ax_known&&!result.flags_known);})
        &&palette_checkpoint.dac_writes.size()==271
        &&std::ranges::count_if(palette_checkpoint.bios_results,
            [](const auto& result){return result.interrupt_address==0x0fd8;})==1
        &&std::ranges::all_of(palette_checkpoint.dac_writes,[](const auto& write){
            return write.red<=63&&write.green<=63&&write.blue<=63;
        }));
    for (unsigned index = 0; index < 16; ++index) {
        const auto& write = palette_checkpoint.dac_writes[index];
        assert(write.register_index == index
            &&write.red == (*titles)[0x014c - 0x0100 + index * 3]
            &&write.green == (*titles)[0x014d - 0x0100 + index * 3]
            &&write.blue == (*titles)[0x014e - 0x0100 + index * 3]);
    }
    const auto loaded = palette_drive;
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
    for (unsigned index = 0; index < 255; ++index) {
        const auto& write = palette.dac_writes[16 + index];
        assert(write.register_index == index
            &&write.red == (*library)[0x25f9 + index * 3]
            &&write.green == (*library)[0x25fa + index * 3]
            &&write.blue == (*library)[0x25fb + index * 3]);
    }
    const auto before = runtime.native_runtime_memory_diagnostics();
    assert(before);
    assert(!runtime.observe_millennium_dos_title_continuation(
        eon::MillenniumDosTitleBiosResultObservation{
            sequence(), 0x0fd8, 0x0fdb, 0, 0}).accepted);
    assert(checkpoint().last_sequence == palette.last_sequence
        && runtime.native_runtime_memory_diagnostics()->checksum == before->checksum);
    assert(checkpoint().continuation_address == 0x0127);
    const auto vector_checkpoint = checkpoint();
    assert(vector_checkpoint.state
        == eon::MillenniumDosTitleInitializationState::selected_callee_private_interrupt_result_boundary
        && vector_checkpoint.selected_callee_boundary.function == 4
        && vector_checkpoint.selected_callee_boundary.record_segment == child_segment
        && vector_checkpoint.selected_callee_boundary.record_offset == 0x1ac5
        && vector_checkpoint.vector_installs.size() == 2
        && vector_checkpoint.vector_installs[0].vector == 0
        && vector_checkpoint.vector_installs[0].offset == 0x1124
        && vector_checkpoint.vector_installs[0].segment == child_segment
        && vector_checkpoint.vector_installs[1].vector == 4
        && vector_checkpoint.vector_installs[1].offset == 0x1124
        && vector_checkpoint.vector_installs[1].segment == child_segment
        && vector_checkpoint.vector_installs[0].provenance
            == eon::MillenniumDosTitleVectorInstallProvenance::eon_dos_compatibility_service);
    assert(vector_checkpoint.dos_vector_results.size() == 2);
    assert(std::ranges::all_of(vector_checkpoint.dos_vector_results, [](const auto& record) {
        return record.eon_virtual_ivt_policy && record.es != 0;
    }));
    assert(vector_checkpoint.setup_bios_boundary.bx_known_mask == 0x00ff
        && vector_checkpoint.setup_bios_boundary.bx_known_value == 0x0046);
    const auto vector_memory = runtime.native_runtime_memory_checkpoint();
    assert(vector_memory);
    const auto ivt_word = [](const auto& memory, std::uint16_t offset) {
        const auto found = std::ranges::find_if(memory.initialized_bytes,
            [offset](const auto& cell) {
                return cell.location.address_space
                        == eon::NativeRuntimeAddressSpace::dos_segmented
                    && cell.location.segment == 0 && cell.location.offset == offset;
            });
        assert(found != memory.initialized_bytes.end());
        const auto high = std::ranges::find_if(memory.initialized_bytes,
            [offset](const auto& cell) {
                return cell.location.address_space
                        == eon::NativeRuntimeAddressSpace::dos_segmented
                    && cell.location.segment == 0
                    && cell.location.offset == static_cast<std::uint16_t>(offset + 1);
            });
        assert(high != memory.initialized_bytes.end());
        return static_cast<std::uint16_t>(found->value
            | (static_cast<std::uint16_t>(high->value) << 8U));
    };
    assert(ivt_word(*vector_memory, 0) == 0x1124 && ivt_word(*vector_memory, 2) == child_segment
        && ivt_word(*vector_memory, 0x10) == 0x1124 && ivt_word(*vector_memory, 0x12) == child_segment
        && ivt_word(*vector_memory, 0x70) == 0x11d8
        && ivt_word(*vector_memory, 0x72) == child_segment);
    const auto setup_drive = runtime.drive_millennium_dos_session();
    assert(setup_drive.accepted && setup_drive.stop_before_address == 0x0127
        && setup_drive.external_observation_requirement
        && setup_drive.external_observation_requirement->kind
            == eon::MillenniumDosTitleExternalObservationKind::title_private_interrupt_result);
    const auto setup_checkpoint = checkpoint();
    assert(setup_checkpoint.setup_bios_results.empty()
        && setup_checkpoint.setup_bios_returns.size() == 2
        && std::ranges::all_of(setup_checkpoint.setup_bios_returns, [](const auto& record) {
            return record.provenance
                == eon::MillenniumDosTitleSetupBiosReturnProvenance::
                    eon_ah01_cassette_off_compatibility_noop;
        }));
    assert(vector_checkpoint.far_word_observations.size() == 2
        && vector_checkpoint.far_word_observations.front().instruction_address == 0x115d
        && vector_checkpoint.far_word_observations.front().first_word_known
        && vector_checkpoint.far_word_observations.front().second_word_known
        && vector_checkpoint.far_word_observations.front().eon_virtual_ivt_policy
        && vector_checkpoint.far_word_observations.back().instruction_address == 0x12ad
        && vector_checkpoint.far_word_observations.back().eon_virtual_ivt_policy);
    return 0;
}
