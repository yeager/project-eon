#include "data/millennium_dos_game_flow.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace eon {
namespace {

bool has_bytes(std::span<const std::uint8_t> bytes, std::size_t offset,
               std::span<const std::uint8_t> expected) {
    return offset <= bytes.size() && expected.size() <= bytes.size() - offset
        && std::equal(expected.begin(), expected.end(), bytes.begin()
            + static_cast<std::ptrdiff_t>(offset));
}

} // namespace

MillenniumDosGameFlow parse_millennium_dos_game_flow(
    const std::span<const std::uint8_t> game_executable) {
    // 2200AD.EXE is a flat COM-style image loaded at 0x100.  Its startup
    // reaches this loop after initialization; it repeatedly calls 0x10f05,
    // tests AL, treats 0x0b/0x0c separately, then converts 0x3b..0x44 to a
    // zero-based eight-byte dispatch-table index passed to 0x76f0.
    constexpr std::size_t load_bias = 0x100;
    constexpr std::size_t entry_offset = 0xd2b0 - load_bias;
    constexpr std::size_t loop_offset = 0xd3d2 - load_bias;
    constexpr std::size_t f1_table_offset = 0x2fbf - load_bias;
    constexpr std::size_t f1_handler_offset = 0x6f9a - load_bias;
    constexpr std::size_t f1_setup_offset = 0x771d - load_bias;
    constexpr std::size_t f2_table_offset = 0x2fc7 - load_bias;
    constexpr std::size_t f2_handler_offset = 0x71ca - load_bias;
    constexpr std::size_t f2_setup_offset = 0x71de - load_bias;
    constexpr std::size_t f3_table_offset = 0x2fcf - load_bias;
    constexpr std::size_t f3_handler_offset = 0x6faa - load_bias;
    constexpr std::size_t f3_setup_offset = 0x6fc6 - load_bias;
    constexpr std::size_t f4_table_offset = 0x2fd7 - load_bias;
    constexpr std::size_t f4_handler_offset = 0x72f9 - load_bias;
    constexpr std::size_t f4_common_offset = 0xba5e - load_bias;
    constexpr std::size_t f5_table_offset = 0x2fdf - load_bias;
    constexpr std::size_t f5_handler_offset = 0x7597 - load_bias;
    constexpr std::size_t f6_table_offset = 0x2fe7 - load_bias;
    constexpr std::size_t f6_handler_offset = 0x7415 - load_bias;
    constexpr std::size_t record_pointer_offset = 0x27c4 - load_bias;
    constexpr std::size_t initial_record_offset = 0x12cc - load_bias;
    constexpr std::size_t initial_record_flag_offset = 0x12f0 - load_bias;
    constexpr std::array<std::uint8_t, 15> entry{
        0x0e, 0x1f, 0x0e, 0x07, 0x8c, 0xc8, 0x8e, 0xd0,
        0xb8, 0x00, 0xda, 0x89, 0xc4, 0xb8, 0x1f};
    constexpr auto loop = std::to_array<std::uint8_t>({
        0xe8, 0xe6, 0x3a, 0xe8, 0x29, 0xa2, 0xe8, 0xf0, 0xa7,
        0xe8, 0x27, 0x3b, 0x22, 0xc0, 0x74, 0xf0, 0x32, 0xe4,
        0x3c, 0x0b, 0x74,
        0x26, 0x8a, 0x0e, 0x3a, 0xda, 0x22, 0xc9, 0x75, 0xe2,
        0x3c, 0x0c, 0x75, 0x05, 0xe8, 0x79, 0x01, 0x33, 0xc0,
        0x2c, 0x3b, 0x3c, 0x0a, 0x73, 0xd3, 0xbe, 0xbf, 0x2f,
        0x32, 0xe4, 0xc0, 0xe0, 0x03, 0x01, 0xc6, 0xe8, 0xe4, 0xa2});
    // Table record 0 contains its non-semantic rectangle followed by the
    // handler entry.  The F1 handler clears AX, calls the common display
    // selector, then calls the setup block below.  That setup writes selector
    // zero and resolves it through the original word table at $27c4.
    constexpr auto f1_table = std::to_array<std::uint8_t>({
        0x00, 0x06, 0x09, 0x1b, 0x30, 0x00, 0x9a, 0x6f});
    constexpr auto f1_handler = std::to_array<std::uint8_t>({
        0x33, 0xc0, 0xe8, 0x2a, 0x61, 0xe8, 0x7b, 0x07,
        0xe8, 0x55, 0x9a, 0xd0, 0xeb, 0x72, 0xf9, 0xc3});
    constexpr auto f1_setup = std::to_array<std::uint8_t>({
        0xb8, 0xcc, 0x12, 0xc6, 0x06, 0x1f, 0xda, 0x00,
        0xa3, 0x20, 0xda, 0xb9, 0x0f, 0x30, 0xa0, 0x1f,
        0xda, 0x22, 0xc0, 0xb0, 0x07, 0x74, 0x05, 0xb0,
        0x05, 0xb9, 0x47, 0x30, 0xa2, 0xa8, 0x75});
    constexpr auto record_pointer_table = std::to_array<std::uint8_t>({
        0xcc, 0x12, 0x84, 0x13, 0x44, 0x14, 0x04, 0x15});
    constexpr auto initial_record = std::to_array<std::uint8_t>({
        0x03, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00});
    constexpr auto initial_record_flag = std::to_array<std::uint8_t>({0x00});
    // Record one (raw F2 / $3c) enters $71ca. It reads a runtime byte at
    // $da26, waits in the original loop if it is below two, otherwise enters
    // $71de. That admitted path creates a $c0-stride list beginning at
    // $1384 and installs callback $7221. No host-side value is supplied for
    // the runtime byte.
    constexpr auto f2_table = std::to_array<std::uint8_t>({
        0x06, 0x0c, 0x09, 0x1b, 0x31, 0x01, 0xca, 0x71});
    constexpr auto f2_handler = std::to_array<std::uint8_t>({
        0xb0, 0x02, 0xa0, 0x26, 0xda, 0x3c, 0x02, 0x72, 0x03,
        0xe9, 0x08, 0x00, 0xe8, 0x21, 0x98, 0xd0, 0xeb, 0x72,
        0xf9, 0xc3});
    constexpr auto f2_setup = std::to_array<std::uint8_t>({
        0xb8, 0x21, 0x72, 0xa3, 0x98, 0x6f, 0xc6, 0x06, 0x98,
        0x6e, 0x01, 0xb8, 0x18, 0x00, 0xe8, 0x3d, 0xdb, 0xb8,
        0x19, 0x00, 0xe8, 0x41, 0xdb, 0xb8, 0x84, 0x13, 0x8a,
        0x0e, 0x26, 0xda, 0x32, 0xed, 0x49, 0x88, 0x0e, 0x95,
        0x6e, 0xbf, 0x99, 0x6e, 0x0e, 0x07, 0xab, 0x05, 0xc0,
        0x00, 0xe2, 0xfa, 0x0e, 0x1f, 0xc6, 0x06, 0x93, 0x6e,
        0xff, 0xe8, 0x9d, 0x00, 0xc6, 0x06, 0x1e, 0xda, 0x08,
        0xe8, 0x56, 0x99, 0xc3});
    constexpr auto f3_table = std::to_array<std::uint8_t>({
        0x0c, 0x12, 0x09, 0x1b, 0x32, 0x02, 0xaa, 0x6f});
    // F3 returns when $a19e is nonzero, waits at $09fa while $da27 is zero,
    // and only then reaches this setup. The two runtime values are not known.
    constexpr auto f3_handler = std::to_array<std::uint8_t>({
        0xa1, 0x9e, 0xa1, 0x23, 0xc0, 0x74, 0x01, 0xc3,
        0xb0, 0x02, 0xa1, 0x27, 0xda, 0x22, 0xc0, 0x74,
        0x03, 0xe9, 0x08, 0x00, 0xe8, 0x39, 0x9a, 0xd0,
        0xeb, 0x72, 0xf9, 0xc3});
    constexpr auto f3_setup = std::to_array<std::uint8_t>({
        0xb8, 0x2a, 0x71, 0xa3, 0x98, 0x6f, 0xc6, 0x06,
        0x98, 0x6e, 0x00, 0xb8, 0x16, 0x00, 0xe8, 0x55,
        0xdd, 0xb8, 0x17, 0x00, 0xe8, 0x59, 0xdd, 0xb8,
        0x00, 0x00, 0xba, 0x00, 0x00, 0x8b, 0x0e, 0x27,
        0xda, 0x88, 0x0e, 0x95, 0x6e, 0xbb, 0x99, 0x6e,
        0xc5, 0x36, 0x12, 0x01});
    // Record three (raw F4 / $3e) only admits when $a19e is zero.  It loads
    // AL=$02 and transfers to $ba5e. That routine calls $4d2c, writes $07 to
    // $da13, calls $9dd5, writes $09 to $da1e and clears $75a9. The call
    // effects and all three cells are native runtime facts, not host state.
    constexpr auto f4_table = std::to_array<std::uint8_t>({
        0x12, 0x18, 0x09, 0x1b, 0x33, 0x03, 0xf9, 0x72});
    constexpr auto f4_handler = std::to_array<std::uint8_t>({
        0xa1, 0x9e, 0xa1, 0x23, 0xc0, 0x74, 0x01, 0xc3,
        0xb0, 0x02, 0xe9, 0x58, 0x47});
    constexpr auto f4_common = std::to_array<std::uint8_t>({
        0xb8, 0x05, 0x00, 0xe8, 0xc8, 0x92, 0xc6, 0x06,
        0x13, 0xda, 0x07, 0xe8, 0x69, 0xe3, 0xc6, 0x06,
        0x1e, 0xda, 0x09, 0xc6, 0x06, 0xa9, 0x75, 0x00, 0xc3});
    // Record four (raw F5 / $3f) enters $7597. Its only directly observable
    // behavior is AL=$02 followed by four near calls and a return; their
    // effects depend on native runtime state and are deliberately not run.
    constexpr auto f5_table = std::to_array<std::uint8_t>({
        0x18, 0x1e, 0x09, 0x1b, 0x34, 0x04, 0x97, 0x75});
    constexpr auto f5_handler = std::to_array<std::uint8_t>({
        0xb0, 0x02, 0xe8, 0x8c, 0x48, 0xe8, 0xfe, 0x95,
        0xe8, 0x55, 0xd6, 0xe8, 0xd1, 0x95, 0xc3});
    // Record five (raw F6 / $40) uses the same $a19e gate as F3/F4.  On its
    // admitted path the native image snapshots $75a8/$75ae/$75ac into its
    // own $7412/$740f/$7410 scratch cells, then installs literal temporary
    // values before calling the original $09fa polling routine.  The byte
    // following SHR BL,1 controls repetition of that poll; no host state is
    // supplied for it, and this parser deliberately does not execute it.
    constexpr auto f6_table = std::to_array<std::uint8_t>({
        0x1e, 0x24, 0x09, 0x1b, 0x35, 0x05, 0x15, 0x74});
    constexpr auto f6_handler = std::to_array<std::uint8_t>({
        0xa1, 0x9e, 0xa1, 0x23, 0xc0, 0x74, 0x01, 0xc3,
        0x33, 0xc0, 0xe8, 0xa7, 0x5c, 0xb8, 0x22, 0x00,
        0xe8, 0x04, 0xd9, 0xe8, 0x55, 0x55, 0xa0, 0xa8,
        0x75, 0xa2, 0x12, 0x74, 0xa0, 0xae, 0x75, 0xa2,
        0x0f, 0x74, 0xa1, 0xac, 0x75, 0xa3, 0x10, 0x74,
        0xc6, 0x06, 0xa8, 0x75, 0x0c, 0xc6, 0x06, 0xae,
        0x75, 0x00, 0xb8, 0x07, 0x32, 0xa3, 0xa6, 0x75,
        0xe8, 0xaa, 0x95, 0xd0, 0xeb, 0x72, 0xf9, 0xc3});
    if (!has_bytes(game_executable, entry_offset, entry)
        || !has_bytes(game_executable, loop_offset, loop)
        || !has_bytes(game_executable, f1_table_offset, f1_table)
        || !has_bytes(game_executable, f1_handler_offset, f1_handler)
        || !has_bytes(game_executable, f1_setup_offset, f1_setup)
        || !has_bytes(game_executable, f2_table_offset, f2_table)
        || !has_bytes(game_executable, f2_handler_offset, f2_handler)
        || !has_bytes(game_executable, f2_setup_offset, f2_setup)
        || !has_bytes(game_executable, f3_table_offset, f3_table)
        || !has_bytes(game_executable, f3_handler_offset, f3_handler)
        || !has_bytes(game_executable, f3_setup_offset, f3_setup)
        || !has_bytes(game_executable, f4_table_offset, f4_table)
        || !has_bytes(game_executable, f4_handler_offset, f4_handler)
        || !has_bytes(game_executable, f4_common_offset, f4_common)
        || !has_bytes(game_executable, f5_table_offset, f5_table)
        || !has_bytes(game_executable, f5_handler_offset, f5_handler)
        || !has_bytes(game_executable, f6_table_offset, f6_table)
        || !has_bytes(game_executable, f6_handler_offset, f6_handler)
        || !has_bytes(game_executable, record_pointer_offset, record_pointer_table)
        || !has_bytes(game_executable, initial_record_offset, initial_record)
        || !has_bytes(game_executable, initial_record_flag_offset, initial_record_flag)) {
        throw std::runtime_error("Unsupported Millennium DOS main-loop control flow");
    }
    return {
        .entry_address = 0xd2b0,
        .main_loop_address = 0xd3d2,
        .action_poll_address = 0x10f05,
        .special_action_0 = 0x0b,
        .special_action_1 = 0x0c,
        .function_key_first_action = 0x3b,
        .function_key_count = 10,
        .function_key_table_address = 0x2fbf,
        .function_key_table_stride = 8,
        .function_key_dispatch_address = 0x76f0,
        .first_function_key = {
            .handler_address = 0x6f9a,
            .selector_address = 0xda1f,
            .selector_value = 0,
            .record_pointer_table_address = 0x27c4,
            .selected_record_address = 0x12cc,
            .screen_descriptor_address = 0x300f,
            .screen_descriptor_mode = 7,
            .selected_record_byte_2 = 0x11,
            .selected_record_byte_36 = 0,
        },
        .second_function_key = {
            .handler_address = 0x71ca,
            .availability_address = 0xda26,
            .minimum_availability = 2,
            .wait_call_address = 0x9fa,
            .callback_slot_address = 0x6f98,
            .callback_address = 0x7221,
            .first_record_address = 0x1384,
            .record_stride = 0x00c0,
            .record_list_address = 0x6e99,
            .list_mode_address = 0x6e98,
            .list_mode_value = 1,
        },
        .third_function_key = {
            .handler_address = 0x6faa,
            .initialization_guard_address = 0xa19e,
            .availability_address = 0xda27,
            .wait_call_address = 0x09fa,
            .callback_slot_address = 0x6f98,
            .callback_address = 0x712a,
            .list_mode_address = 0x6e98,
            .list_mode_value = 0,
            .source_far_pointer_address = 0x0112,
            .list_address = 0x6e99,
        },
        .fourth_function_key = {
            .handler_address = 0x72f9,
            .initialization_guard_address = 0xa19e,
            .transfer_al_value = 2,
            .common_routine_address = 0xba5e,
            .first_call_address = 0x4d2c,
            .first_runtime_byte_address = 0xda13,
            .first_runtime_byte_value = 7,
            .second_call_address = 0x9dd5,
            .second_runtime_byte_address = 0xda1e,
            .second_runtime_byte_value = 9,
            .third_runtime_byte_address = 0x75a9,
            .third_runtime_byte_value = 0,
        },
        .fifth_function_key = {
            .handler_address = 0x7597,
            .transfer_al_value = 2,
            .first_call_address = 0xbe28,
            .second_call_address = 0x10b9d,
            .third_call_address = 0x14bf7,
            .fourth_call_address = 0x10b76,
        },
        .sixth_function_key = {
            .handler_address = 0x7415,
            .initialization_guard_address = 0xa19e,
            .display_selector_call_address = 0xd0c9,
            .command_value = 0x0022,
            .first_call_address = 0x4d2c,
            .second_call_address = 0xc980,
            .saved_first_byte_address = 0x7412,
            .first_byte_address = 0x75a8,
            .saved_second_byte_address = 0x740f,
            .second_byte_address = 0x75ae,
            .saved_word_address = 0x7410,
            .word_address = 0x75ac,
            .first_byte_value = 0x0c,
            .second_byte_value = 0x00,
            .callback_word_value = 0x3207,
            .callback_word_address = 0x75a6,
            .wait_call_address = 0x09fa,
        },
    };
}

} // namespace eon
