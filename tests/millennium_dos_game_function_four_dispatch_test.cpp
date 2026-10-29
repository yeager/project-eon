#include "data/millennium_dos_video_driver.hpp"
#include "engine/millennium_dos_game_function_four_dispatch.hpp"
#include "engine/millennium_dos_ega_startup_palette_session.hpp"
#include "engine/millennium_dos_native_process.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

std::vector<std::uint8_t> read_original(const std::filesystem::path& path,
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

void check_driver(const std::span<const std::uint8_t> game,
    const std::span<const std::uint8_t> source_driver,
    const eon::MillenniumDosVideoDriverKind kind) {
    auto loaded = std::vector<std::uint8_t>(source_driver.begin(), source_driver.end());
    const auto profile = eon::parse_millennium_dos_video_driver(source_driver, kind);
    const auto first_ax = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(profile.function_thirty_one_return_ah) << 8U)
        | loaded[profile.function_thirty_one_state_address]);
    const auto result = eon::execute_millennium_dos_game_function_four_dispatch(
        game, first_ax, source_driver, loaded, kind, 0x2345, 0x3456,
        0x91, 0, 0x3456);
    assert(result.accepted && result.error.empty());
    assert(result.kind == kind && result.record_byte == 0);
    assert(result.masked_record_byte == 0 && result.driver_state_after == 0);
    assert(result.handler_function_address == profile.function_four_address);
    assert(result.wrapper_call_address == (kind == eon::MillenniumDosVideoDriverKind::ega640
        ? 0xd1bd : 0xd1a9));
    assert(result.wrapper_return_address == (kind == eon::MillenniumDosVideoDriverKind::ega640
        ? 0xd1c0 : 0xd1ac));
    assert(result.continuation_address == (kind == eon::MillenniumDosVideoDriverKind::ega640
        ? 0x0476 : 0x0455));
    assert(result.final_stack_pointer == 0xd9fc);
    assert(result.stack_words.size() == 11);
    assert(result.stack_words[0].offset == 0xd9ea
        && result.stack_words[0].value == std::optional<std::uint16_t>{0x0012});
    assert(result.stack_words[1].offset == 0xd9ec
        && result.stack_words[1].value == std::optional<std::uint16_t>{0x012b});
    assert(!result.stack_words[3].value); // INT FLAGS remain unknown.
    assert(!result.stack_words[5].value); // Saved BP remains unknown.
    assert(!result.stack_words[6].value); // Saved DI remains unknown.
    assert(!result.stack_words[7].value); // Saved SI remains unknown.
    assert(result.stack_words[9].value == std::optional<std::uint16_t>{
        kind == eon::MillenniumDosVideoDriverKind::ega640 ? 0xd1c3 : 0xd1af});
    assert(result.stack_words[10].value == result.branch_call_return_address);

    if (kind == eon::MillenniumDosVideoDriverKind::mcga) {
        const auto allocation = eon::advance_millennium_dos_startup_to_allocation_request(
            game, result, 0x2345);
        assert(allocation.accepted && allocation.error.empty());
        assert(allocation.instruction_address == 0xd201);
        assert(allocation.interrupt_number == 0x21 && allocation.ah == 0x4a);
        assert(allocation.bx == 0x1000 && allocation.ds == 0x2345
            && allocation.code_segment == 0x2345);
        assert(allocation.stack_pointer == 0xd9fc);
        assert(allocation.stack_words.size() == 3);
        assert(allocation.stack_words[0].offset == 0xd9fa
            && allocation.stack_words[0].value == std::optional<std::uint16_t>{0x2345});
        assert(allocation.stack_words[1].offset == 0xd9fc
            && allocation.stack_words[1].value == std::optional<std::uint16_t>{0xd2eb});
        assert(allocation.stack_words[2].offset == 0xd9fe
            && !allocation.stack_words[2].value);
        auto detached_frame = result;
        detached_frame.stack_words[8].value = 0x9999;
        const auto detached = eon::advance_millennium_dos_startup_to_allocation_request(
            game, detached_frame, 0x2345);
        assert(!detached.accepted);
        detached_frame = result;
        detached_frame.stack_words[9].value = 0x9999;
        assert(!eon::advance_millennium_dos_startup_to_allocation_request(
            game, detached_frame, 0x2345).accepted);
        detached_frame = result;
        detached_frame.stack_words[10].value = 0x9999;
        assert(!eon::advance_millennium_dos_startup_to_allocation_request(
            game, detached_frame, 0x2345).accepted);
        assert(!eon::advance_millennium_dos_startup_to_allocation_request(
            game, result, 0x2346).accepted);
        auto wrong_boundary = result;
        wrong_boundary.continuation_address = 0x0476;
        assert(!eon::advance_millennium_dos_startup_to_allocation_request(
            game, wrong_boundary, 0x2345).accepted);
        auto altered_game = std::vector<std::uint8_t>(game.begin(), game.end());
        altered_game[0xd201 - 0x100] ^= 1U;
        assert(!eon::advance_millennium_dos_startup_to_allocation_request(
            altered_game, result, 0x2345).accepted);

        auto process = eon::MillenniumDosNativeProcess::startup(game);
        process.observe_private_interrupt_return(0x0129, first_ax);
        process.observe_private_interrupt_return(0x0129, result.returned_ax);
        assert(process.boundary().kind == eon::MillenniumDosNativeBoundaryKind::local_return
            && process.boundary().address == 0x0455);
        bool rejected_wrong_site = false;
        try { process.observe_startup_allocation_request(0xd200); }
        catch (const std::exception&) { rejected_wrong_site = true; }
        assert(rejected_wrong_site);
        process.observe_startup_allocation_request(0xd201);
        assert(process.boundary().kind == eon::MillenniumDosNativeBoundaryKind::dos_interrupt
            && process.boundary().address == 0xd201
            && process.boundary().interrupt == 0x21
            && process.boundary().service == 0x4a);
        process.observe_dos_interrupt_return(
            {1,0xd201,0x21,0x4a,0x2345,0xd203,0,0xfa00,0,0,0});
        assert(process.boundary().kind == eon::MillenniumDosNativeBoundaryKind::dos_interrupt
            && process.boundary().address == 0xd208
            && process.boundary().service == 0x48);
        process.observe_dos_interrupt_return(
            {2,0xd208,0x21,0x48,0x2345,0xd20a,0x5678,0xfa00,0x10,0,1});
        assert(process.boundary().kind == eon::MillenniumDosNativeBoundaryKind::dos_interrupt
            && process.boundary().address == 0xd213
            && process.boundary().service == 0x49);
        assert(process.startup_allocator_stored_bx() == std::uint16_t{0xfa00}
            && process.startup_allocator_es_segment() == std::uint16_t{0x5678}
            && process.runtime_byte(0xd12e) == std::uint8_t{0x00}
            && process.runtime_byte(0xd12f) == std::uint8_t{0xfa});
        process.observe_dos_interrupt_return(
            {3,0xd213,0x21,0x49,0x2345,0xd215,0,0x00a6,0,0,1});
        assert(process.boundary().kind == eon::MillenniumDosNativeBoundaryKind::dos_interrupt
            && process.boundary().address == 0xd21a
            && process.boundary().service == 0x48);
        process.observe_dos_interrupt_return(
            {4,0xd21a,0x21,0x48,0x2345,0xd21c,0x1234,0x00a6,0,0,0});
        assert(process.startup_allocator_third_dos_return()
            && process.startup_allocator_third_dos_return()->ax == 0x1234
            && process.runtime_byte(0xd114) == std::uint8_t{0x34}
            && process.runtime_byte(0xd115) == std::uint8_t{0x12}
            && process.startup_file_open_request()
            && process.startup_file_open_request()->instruction_address == 0xd1d4
            && process.startup_file_open_request()->return_instruction_pointer == 0xd1d6
            && process.startup_file_open_request()->ax == 0x3d00
            && process.startup_file_open_request()->data_segment_is_code_segment
            && process.startup_file_open_request()->dx_name_address == 0x100d
            && process.boundary().address == 0xd1d4
            && process.boundary().service == 0x3d);
        process.observe_dos_interrupt_return(
            {5,0xd1d4,0x21,0x3d,0x2345,0xd1d6,0x0005,0,0,0,0});
        assert(process.startup_file_handle() == std::uint16_t{5}
            && process.startup_file_seek_request()
            && process.startup_file_seek_request()->instruction_address == 0xd1e4
            && process.startup_file_seek_request()->ax == 0x4202
            && process.startup_file_seek_request()->bx == 5
            && process.boundary().address == 0xd1e4
            && process.boundary().service == 0x42);
        process.observe_dos_interrupt_return(
            {6,0xd1e4,0x21,0x42,0x2345,0xd1e6,0x0123,5,0,0,0});
        assert(process.startup_file_size() == std::uint16_t{0x0123}
            && process.startup_file_close_request()
            && process.startup_file_close_request()->instruction_address == 0xd1ed
            && process.startup_file_close_request()->ax == 0x3e00
            && process.startup_file_close_request()->bx == 5
            && process.boundary().address == 0xd1ed
            && process.boundary().service == 0x3e);
        process.observe_dos_interrupt_return(
            {7,0xd1ed,0x21,0x3e,0x2345,0xd1ef,0,5,0,0,0});
        assert(process.startup_allocator_fourth_request()
            && process.startup_allocator_fourth_request()->instruction_address == 0xd232
            && process.startup_allocator_fourth_request()->bx == 0x0013
            && process.startup_allocator_fourth_request()->ax == 0x4813
            && process.boundary().address == 0xd232
            && process.boundary().service == 0x48);
        process.observe_dos_interrupt_return(
            {8,0xd232,0x21,0x48,0x2345,0xd234,0x3456,0x0013,0,0,0});
        assert(process.startup_allocator_fourth_dos_return()
            && process.startup_allocator_fourth_segment() == std::uint16_t{0x3456}
            && process.runtime_byte(0x0116) == std::uint8_t{0x56}
            && process.runtime_byte(0x0117) == std::uint8_t{0x34}
            && process.startup_allocator_fifth_request()
            && process.startup_allocator_fifth_request()->instruction_address == 0xd23e
            && process.startup_allocator_fifth_request()->bx == 0x00c0
            && process.boundary().address == 0xd23e);
        process.observe_dos_interrupt_return(
            {9,0xd23e,0x21,0x48,0x2345,0xd240,0x4567,0x00c0,0,0,0});
        assert(process.startup_allocator_fifth_dos_return()
            && process.runtime_byte(0x011c) == std::uint8_t{0x67}
            && process.runtime_byte(0x011d) == std::uint8_t{0x45}
            && process.startup_file_open_request()
            && process.startup_file_open_request()->dx_name_address == 0x11c2
            && process.boundary().address == 0xd1d4);
        process.observe_dos_interrupt_return(
            {10,0xd1d4,0x21,0x3d,0x2345,0xd1d6,0x0007,0,0,0,0});
        process.observe_dos_interrupt_return(
            {11,0xd1e4,0x21,0x42,0x2345,0xd1e6,0xffff,7,0,0,0});
        process.observe_dos_interrupt_return(
            {12,0xd1ed,0x21,0x3e,0x2345,0xd1ef,0,7,0,0,0});
        assert(process.startup_allocator_fifth_request()
            && process.startup_allocator_fifth_request()->instruction_address == 0xd251
            && process.startup_allocator_fifth_request()->bx == 0x0000
            && process.startup_allocator_fifth_request()->ax == 0x4800
            && process.boundary().address == 0xd251
            && process.boundary().service == 0x48);
        process.observe_dos_interrupt_return(
            {13,0xd251,0x21,0x48,0x2345,0xd253,0x456a,0x0000,0,0,0});
        assert(process.runtime_byte(0x0118) == std::uint8_t{0x6a}
            && process.runtime_byte(0x0119) == std::uint8_t{0x45}
            && process.startup_allocator_seventh_request()
            && process.startup_allocator_seventh_request()->instruction_address == 0xd25d
            && process.startup_allocator_seventh_request()->bx == 0x1000
            && process.startup_allocator_seventh_request()->ax == 0x486a);
        process.observe_dos_interrupt_return(
            {14,0xd25d,0x21,0x48,0x2345,0xd25f,0x1234,0x1000,0,0,0});
        assert(process.runtime_byte(0x010c) == std::uint8_t{0x34}
            && process.runtime_byte(0x010d) == std::uint8_t{0x12}
            && process.startup_allocator_eighth_request()
            && process.startup_allocator_eighth_request()->instruction_address == 0xd26d
            && process.startup_allocator_eighth_request()->bx == 0x0fa1
            && process.startup_allocator_eighth_request()->ax == 0x4834);
        process.observe_dos_interrupt_return(
            {15,0xd26d,0x21,0x48,0x2345,0xd26f,0xabcd,0x0fa1,0,0,0});
        assert(process.runtime_byte(0x0110) == std::uint8_t{0xcd}
            && process.runtime_byte(0x0111) == std::uint8_t{0xab}
            && process.startup_allocator_ninth_request()
            && process.startup_allocator_ninth_request()->instruction_address == 0xd27d
            && process.startup_allocator_ninth_request()->bx == 0x0fa1
            && process.startup_allocator_ninth_request()->ax == 0x48cd
            && process.boundary().address == 0xd27d);
        process.observe_dos_interrupt_return(
            {16,0xd27d,0x21,0x48,0x2345,0xd27f,0x9876,0x0fa1,0,0,0});
        assert(process.runtime_byte(0x1044) == std::uint8_t{0x76}
            && process.runtime_byte(0x1045) == std::uint8_t{0x98}
            && process.runtime_byte(0x1048) == std::uint8_t{0x76}
            && process.runtime_byte(0x1049) == std::uint8_t{0x98}
            && process.startup_allocator_tenth_request()
            && process.startup_allocator_tenth_request()->instruction_address == 0xd294
            && process.startup_allocator_tenth_request()->ax == 0x4876
            && process.startup_allocator_tenth_request()->bx == 0x1000);
        process.observe_dos_interrupt_return(
            {17,0xd294,0x21,0x48,0x2345,0xd296,0x1357,0x1000,0,0,0});
        assert(process.runtime_byte(0x105a) == std::uint8_t{0x57}
            && process.runtime_byte(0x105b) == std::uint8_t{0x13}
            && process.startup_allocator_eleventh_request()
            && process.startup_allocator_eleventh_request()->instruction_address == 0xd2a4
            && process.startup_allocator_eleventh_request()->ax == 0x4857
            && process.startup_allocator_eleventh_request()->bx == 1);
        process.observe_dos_interrupt_return(
            {18,0xd2a4,0x21,0x48,0x2345,0xd2a6,0x2468,1,0,0,1});
        assert(process.startup_allocator_return_ax() == std::uint16_t{0x2468}
            && process.runtime_byte(0xd12a) == std::uint8_t{0x68}
            && process.runtime_byte(0xd12b) == std::uint8_t{0x24}
            && process.runtime_byte(0xd128) == std::uint8_t{0x68}
            && process.runtime_byte(0xd129) == std::uint8_t{0x24}
            && process.boundary().kind == eon::MillenniumDosNativeBoundaryKind::dos_interrupt
            && process.boundary().address == 0x054d);
        const auto selector_mode = process.runtime_byte(0xda05);
        assert(selector_mode);
        assert(process.startup_zero_path_file_open_request()
            && process.startup_zero_path_file_open_request()->instruction_address == 0x054d
            && process.startup_zero_path_file_open_request()->return_instruction_pointer == 0x054f
            && process.startup_zero_path_file_open_request()->interrupt_number == 0x21
            && process.startup_zero_path_file_open_request()->service == 0x3d
            && process.startup_zero_path_file_open_request()->ax == 0x3d02
            && process.startup_zero_path_file_open_request()->data_segment_is_code_segment
            && process.startup_zero_path_file_open_request()->dx_name_address
                == (*selector_mode == 1 ? 0x1131 : *selector_mode == 3 ? 0x113d
                    : *selector_mode == 2 ? 0x1155 : 0x1149)
            && process.boundary().kind == eon::MillenniumDosNativeBoundaryKind::dos_interrupt
            && process.boundary().address == 0x054d
            && process.boundary().service == 0x3d);
        process.observe_dos_interrupt_return(
            {19,0x054d,0x21,0x3d,0x2345,0x054f,5,0,0,0,0});
        assert(process.startup_zero_path_file_open_dos_return()
            && process.runtime_byte(0xda00) == std::uint8_t{5}
            && process.runtime_byte(0xda01) == std::uint8_t{0}
            && process.startup_zero_path_file_read_request()
            && process.startup_zero_path_file_read_request()->instruction_address == 0x0580
            && process.startup_zero_path_file_read_request()->return_instruction_pointer == 0x0582
            && process.startup_zero_path_file_read_request()->service == 0x3f
            && process.startup_zero_path_file_read_request()->ax == 0x3f05
            && process.startup_zero_path_file_read_request()->bx == 5
            && process.startup_zero_path_file_read_request()->cx == 0xffff
            && process.startup_zero_path_file_read_request()->dx == 0
            && process.startup_zero_path_file_read_request()->ds == 0x4567
            && process.boundary().address == 0x0580
            && process.boundary().service == 0x3f);
        process.observe_dos_interrupt_return(
            {20,0x0580,0x21,0x3f,0x2345,0x0582,0x0400,5,0xffff,0,0});
        assert(process.startup_zero_path_file_read_dos_return()
            && process.startup_zero_path_file_close_request()
            && process.startup_zero_path_file_close_request()->instruction_address == 0x05a2
            && process.startup_zero_path_file_close_request()->service == 0x3e
            && process.startup_zero_path_file_close_request()->ax == 0x3e00
            && process.startup_zero_path_file_close_request()->bx == 5
            && process.startup_zero_path_file_close_request()->cx == 0xffff
            && process.startup_zero_path_file_close_request()->dx == 0
            && process.startup_zero_path_file_close_request()->ds == 0x2345
            && process.boundary().address == 0x05a2
            && process.boundary().service == 0x3e);
        process.observe_dos_interrupt_return(
            {21,0x05a2,0x21,0x3e,0x2345,0x05a4,0,5,0xffff,0,0});
        assert(process.startup_zero_path_file_close_dos_return()
            && process.startup_zero_path_vector_get_request()
            && process.startup_zero_path_vector_get_request()->instruction_address == 0x4260
            && process.startup_zero_path_vector_get_request()->service == 0x35
            && process.startup_zero_path_vector_get_request()->ax == 0x3593
            && process.startup_zero_path_vector_get_request()->bx == 5
            && process.startup_zero_path_vector_get_request()->cx == 0xffff
            && process.startup_zero_path_vector_get_request()->dx == 0
            && process.startup_zero_path_vector_get_request()->ds == 0x2345
            && process.boundary().address == 0x4260
            && process.boundary().service == 0x35);
        process.observe_dos_interrupt_return(
            {22,0x4260,0x21,0x35,0x2345,0x4262,0,0x1234,0xabcd,0,0,0x4567});
        assert(process.startup_zero_path_vector_get_dos_return()
            && process.runtime_byte(0x4255) == std::uint8_t{0x34}
            && process.runtime_byte(0x4256) == std::uint8_t{0x12}
            && process.runtime_byte(0x4257) == std::uint8_t{0x67}
            && process.runtime_byte(0x4258) == std::uint8_t{0x45}
            && process.startup_zero_path_vector_set_request()
            && process.startup_zero_path_vector_set_request()->instruction_address == 0x4271
            && process.startup_zero_path_vector_set_request()->service == 0x25
            && process.startup_zero_path_vector_set_request()->ax == 0x2593
            && process.startup_zero_path_vector_set_request()->bx == 0x1234
            && process.startup_zero_path_vector_set_request()->cx == 0xabcd
            && process.startup_zero_path_vector_set_request()->dx == 0
            && process.startup_zero_path_vector_set_request()->ds == 0x4567
            && process.boundary().address == 0x4271
            && process.boundary().service == 0x25);
        process.observe_dos_interrupt_return(
            {23,0x4271,0x21,0x25,0x2345,0x4273,0,0x1234,0x4567,0,0,0});
        assert(process.startup_zero_path_vector_set_dos_return()
            && process.boundary().kind
                == eon::MillenniumDosNativeBoundaryKind::runtime_byte
            && process.boundary().address == 0xd2fd
            && process.boundary().runtime_byte_address == 0x0082);
        process.observe_runtime_byte(0xd2fd, 0x0082, 0x31, 24);
        assert(process.startup_zero_path_continuation_byte()
            && process.startup_zero_path_continuation_byte()->sequence == 24
            && process.runtime_byte(0x0122) == std::uint8_t{1}
            && process.boundary().kind
                == eon::MillenniumDosNativeBoundaryKind::local_instruction
            && process.boundary().address == 0xd305);

        auto failed_process = eon::MillenniumDosNativeProcess::startup(game);
        failed_process.observe_private_interrupt_return(0x0129, first_ax);
        failed_process.observe_private_interrupt_return(0x0129, result.returned_ax);
        failed_process.observe_startup_allocation_request(0xd201);
        failed_process.observe_dos_interrupt_return(
            {1,0xd201,0x21,0x4a,0x2345,0xd203,0,0xfa00,0,0,0});
        failed_process.observe_dos_interrupt_return(
            {2,0xd208,0x21,0x48,0x2345,0xd20a,0x5678,0xfa00,0x10,0,1});
        failed_process.observe_dos_interrupt_return(
            {3,0xd213,0x21,0x49,0x2345,0xd215,0,0x00a6,0,0,1});
        failed_process.observe_dos_interrupt_return(
            {4,0xd21a,0x21,0x48,0x2345,0xd21c,0x1234,0x00a6,0,0,1});
        assert(failed_process.startup_allocator_third_dos_return()
            && !failed_process.startup_file_open_request()
            && failed_process.runtime_byte(0xd114) == std::uint8_t{0x34}
            && failed_process.runtime_byte(0xd115) == std::uint8_t{0x12}
            && failed_process.boundary().kind
                == eon::MillenniumDosNativeBoundaryKind::local_instruction
            && failed_process.boundary().address == 0xd2ac);
    } else {
        eon::MillenniumDosEgaStartupPaletteSession palette(game, result);
        assert(palette.completed_write_count() == 0 && !palette.complete()
            && palette.next_instruction_address() == 0x0476);
        const auto initial = palette.pending_write();
        assert(initial && initial->register_index == 0);
        assert(!palette.palette_registers()[0]);
        std::array<std::uint8_t, 16> expected_palette_values{};
        auto rejected = palette.observe_applied_write({1, 0x0476, 0x10, 0x1001,
            static_cast<std::uint16_t>((initial->color_value << 8U) | initial->register_index)});
        assert(!rejected.accepted && palette.completed_write_count() == 0);
        for (std::uint8_t index = 0; index < 16; ++index) {
            const auto pending = palette.pending_write();
            assert(pending && pending->register_index == index
                && palette.next_instruction_address() == 0x0476);
            expected_palette_values[index] = pending->color_value;
            const auto bx = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(pending->color_value) << 8U) | index);
            const auto applied = palette.observe_applied_write(
                {static_cast<std::uint64_t>(index + 1U), 0x0476, 0x10, 0x1000, bx});
            assert(applied.accepted && applied.write == *pending
                && applied.ordinal == static_cast<std::size_t>(index + 1U)
                && applied.complete == (index == 15));
        }
        assert(palette.complete() && !palette.pending_write()
            && palette.next_instruction_address() == 0x047c);
        for (std::size_t index = 0; index < palette.palette_registers().size(); ++index) {
            assert(palette.palette_registers()[index]
                == std::optional<std::uint8_t>{expected_palette_values[index]});
        }
        const auto extra = palette.observe_applied_write({17, 0x0476, 0x10, 0x1000, 0});
        assert(!extra.accepted && palette.completed_write_count() == 16);

        auto active_process = eon::MillenniumDosNativeProcess::startup(game);
        active_process.observe_private_interrupt_return(0x0129, first_ax);
        active_process.observe_private_interrupt_return(0x0129, result.returned_ax);
        active_process.begin_startup_ega_palette(result);
        assert(active_process.boundary().kind
                == eon::MillenniumDosNativeBoundaryKind::bios_interrupt
            && active_process.boundary().address == 0x0476
            && active_process.startup_ega_palette_session()
            && active_process.startup_ega_palette_session()->completed_write_count() == 0);
        const auto first_write = active_process.startup_ega_palette_session()->pending_write();
        assert(first_write);
        const auto wrong_bx = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(first_write->color_value) << 8U)
                | static_cast<std::uint8_t>(first_write->register_index + 1U));
        bool wrong_bx_rejected = false;
        try {
            active_process.observe_bios_interrupt_return(
                {1, 0x0476, 0x10, 0x2345, 0x0478, 0x1000, 0, wrong_bx});
        } catch (const std::runtime_error&) {
            wrong_bx_rejected = true;
        }
        assert(wrong_bx_rejected
            && active_process.boundary().kind
                == eon::MillenniumDosNativeBoundaryKind::bios_interrupt
            && active_process.startup_ega_palette_session()->completed_write_count() == 0);
        for (std::uint8_t index = 0; index < 16; ++index) {
            const auto pending = active_process.startup_ega_palette_session()->pending_write();
            assert(pending && pending->register_index == index);
            const auto bx = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(pending->color_value) << 8U) | index);
            active_process.observe_bios_interrupt_return({
                static_cast<std::uint64_t>(index + 1U), 0x0476, 0x10,
                0x2345, 0x0478, 0x1000, 0, bx});
            if (index < 15) {
                assert(active_process.state()
                        == eon::MillenniumDosNativeProcessState::startup_palette_interrupt_boundary
                    && active_process.boundary().kind
                        == eon::MillenniumDosNativeBoundaryKind::bios_interrupt
                    && active_process.boundary().address == 0x0476);
            }
        }
        assert(active_process.state()
                == eon::MillenniumDosNativeProcessState::startup_palette_loop_return_boundary
            && active_process.boundary().kind
                == eon::MillenniumDosNativeBoundaryKind::local_return
            && active_process.boundary().address == 0x047c
            && active_process.startup_ega_palette_session()->complete()
            && active_process.startup_ega_palette_session()->completed_write_count() == 16);
        for (const auto& register_value :
            active_process.startup_ega_palette_session()->palette_registers()) {
            assert(!register_value);
        }
        const auto selector = active_process.runtime_byte(0xda05);
        assert(selector);
        bool detached_ega_caller_rejected = false;
        try {
            active_process.continue_startup_ega_caller(*selector, 0xd1c3, 0x0000);
        } catch (const std::runtime_error&) {
            detached_ega_caller_rejected = true;
        }
        assert(detached_ega_caller_rejected
            && active_process.state()
                == eon::MillenniumDosNativeProcessState::startup_palette_loop_return_boundary);
        active_process.continue_startup_ega_caller(*selector, 0xd1c3, 0xd2e5);
        assert(active_process.state()
                == eon::MillenniumDosNativeProcessState::startup_ega_caller_return_boundary
            && active_process.boundary().kind
                == eon::MillenniumDosNativeBoundaryKind::local_return
            && active_process.boundary().address == 0xd1d0
            && active_process.runtime_byte(0x0107)
                == (*selector == 2 ? std::optional<std::uint8_t>{0x00} : std::nullopt)
            && active_process.runtime_byte(0x0108)
                == (*selector == 2 ? std::optional<std::uint8_t>{0xb8} : std::nullopt));
        bool detached_allocator_rejected = false;
        try {
            active_process.continue_startup_ega_allocator_request(0x0000);
        } catch (const std::runtime_error&) {
            detached_allocator_rejected = true;
        }
        assert(detached_allocator_rejected
            && active_process.state()
                == eon::MillenniumDosNativeProcessState::startup_ega_caller_return_boundary);
        active_process.continue_startup_ega_allocator_request(0xd2e5);
        assert(active_process.state()
                == eon::MillenniumDosNativeProcessState::startup_allocation_interrupt_boundary
            && active_process.boundary().kind
                == eon::MillenniumDosNativeBoundaryKind::dos_interrupt
            && active_process.boundary().address == 0xd201
            && active_process.boundary().interrupt == 0x21
            && active_process.boundary().service == 0x4a);
    }

    const auto detached = eon::execute_millennium_dos_game_function_four_dispatch(
        game, first_ax, source_driver, loaded, kind, 0x2345, 0x3456,
        0x91, 2, 0x3456);
    assert(!detached.accepted);
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const std::filesystem::path root(argv[1]);
    const auto game_path = root / "2200AD.EXE";
    const auto ega_path = root / "EGA640.BIN";
    const auto mcga_path = root / "MCGA.BIN";
    if (!std::filesystem::is_regular_file(game_path)
        || !std::filesystem::is_regular_file(ega_path)
        || !std::filesystem::is_regular_file(mcga_path)) return 77;
    const auto game = read_original(game_path, 54'391);
    const auto ega = read_original(ega_path, 4'632);
    const auto mcga = read_original(mcga_path, 4'366);
    check_driver(game, ega, eon::MillenniumDosVideoDriverKind::ega640);
    check_driver(game, mcga, eon::MillenniumDosVideoDriverKind::mcga);
    return 0;
}
