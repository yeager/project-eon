#include "data/millennium_dos_title_flow.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace eon {
namespace {

bool has_bytes(std::span<const std::uint8_t> bytes, std::size_t offset,
               std::span<const std::uint8_t> expected) {
    return offset <= bytes.size() && expected.size() <= bytes.size() - offset
        && std::equal(expected.begin(), expected.end(), bytes.begin() + static_cast<std::ptrdiff_t>(offset));
}

std::size_t require_unique(std::span<const std::uint8_t> bytes,
                           std::span<const std::uint8_t> expected,
                           const char* description) {
    std::size_t result = bytes.size();
    for (std::size_t offset = 0; offset + expected.size() <= bytes.size(); ++offset) {
        if (!has_bytes(bytes, offset, expected)) continue;
        if (result != bytes.size()) {
            throw std::runtime_error(std::string("Ambiguous Millennium DOS ") + description);
        }
        result = offset;
    }
    if (result == bytes.size()) {
        throw std::runtime_error(std::string("Missing Millennium DOS ") + description);
    }
    return result;
}

} // namespace

MillenniumDosTitleFlow parse_millennium_dos_title_flow(
    std::span<const std::uint8_t> titles_executable,
    std::span<const std::uint8_t> mill_launcher) {
    // Flat DOS files are loaded at offset 0x100.  The entry jump at file 0
    // lands at 0x1b80, where the title program establishes its stack.
    constexpr std::array<std::uint8_t, 7> entry_jump{
        0x0e, 0x1f, 0x0e, 0x07, 0xe9, 0x79, 0x1a};
    constexpr std::array<std::uint8_t, 6> title_selection{
        0xb8, 0x00, 0x00, 0xe8, 0x0b, 0xfb}; // AX=0; call 0x1725
    constexpr std::array<std::uint8_t, 13> title_selection_callee_prefix{
        0x0e, 0x1f, 0x0e, 0x07, 0x8b, 0x0e, 0x5d, 0x0e,
        0x3b, 0xc1, 0x7e, 0x01, 0xc3};
    constexpr std::array<std::uint8_t, 16> title_selection_callee_jle_target_prefix{
        0x50, 0xd1, 0xe0, 0x89, 0xc1, 0xd1, 0xe0, 0x03,
        0xc8, 0x58, 0x51, 0xe8, 0x50, 0xfc, 0x58, 0x0e};
    constexpr std::array<std::uint8_t, 46> title_selection_nested_callee_prefix{
        0xb9, 0x0c, 0x00, 0xf7, 0xe1, 0x2e, 0xc5, 0x36,
        0x4a, 0x0e, 0x2e, 0xc4, 0x3e, 0x46, 0x0e, 0x8c,
        0xc2, 0x89, 0xfb, 0x0e, 0x07, 0xbf, 0x8c, 0x13,
        0x01, 0xc6, 0xad, 0x01, 0xc3, 0xad, 0x88, 0xc4,
        0x32, 0xc0, 0xb9, 0x04, 0x00, 0xd3, 0xe0, 0x01,
        0xc2, 0x8b, 0xcb, 0xe8, 0x7e, 0xed};
    constexpr std::array<std::uint8_t, 16> title_selection_nested_leaf_prefix{
        0x89, 0xc8, 0x83, 0xe1, 0x0f, 0xd1, 0xe8, 0xd1,
        0xe8, 0xd1, 0xe8, 0xd1, 0xe8, 0x01, 0xc2, 0xc3};
    constexpr std::array<std::uint8_t, 13> transition_setup{
        0xb9, 0x25, 0x00, 0xba, 0x70, 0x01, 0x51, 0x52,
        0xbe, 0x0c, 0x01, 0x8b, 0x04};
    constexpr std::array<std::uint8_t, 7> input_poll{
        0xb4, 0x06, 0xb2, 0xff, 0xcd, 0x21, 0xc3};
    // All nonzero DOS poll results take this one exit path. The returned AL
    // is only tested, never decoded as a scan code or a named control. The
    // path subsequently reaches a private INT 91h wrapper, so it remains
    // static evidence rather than a host-side loading animation.
    constexpr std::array<std::uint8_t, 66> input_branch{
        0xe8, 0xdf, 0xf0, 0x22, 0xc0, 0x75, 0x25, 0xb8, 0x13, 0x00,
        0xe8, 0xed, 0xe4, 0xe8, 0x60, 0xfc, 0xa1, 0x96, 0x18, 0xd1,
        0xe0, 0xd1, 0xe0, 0x50, 0xe8, 0xc7, 0xf0, 0x22, 0xc0, 0x75,
        0x0c, 0xb8, 0x13, 0x00, 0xe8, 0xd5, 0xe4, 0x58, 0x48, 0x75,
        0xee, 0xeb, 0xcd, 0x58, 0xe8, 0x11, 0xfd, 0xe8, 0x66, 0xf6,
        0x32, 0xc0, 0x2e, 0xa2, 0x0e, 0x1a, 0x8b, 0x26, 0xa0, 0x1a,
        0xe8, 0xaf, 0xec, 0xe9, 0xa5, 0xfd};
    constexpr std::array<std::uint8_t, 16> input_exit_loading_text{
        ' ', ' ', ' ', ' ', 'L', 'O', 'A', 'D', 'I', 'N', 'G', ' ', ' ', ' ', ' ', '2'};
    constexpr std::array<std::uint8_t, 7> input_exit_private_driver_entry{
        0xb8, 0x05, 0x00, 0xe8, 0xc3, 0xff, 0x0e};
    constexpr std::array<std::uint8_t, 16> input_exit_private_driver_loop{
        0x89, 0xc1, 0x51, 0xb8, 0x13, 0x00, 0xe8, 0xe8,
        0xe7, 0xe8, 0xda, 0xff, 0x59, 0xe2, 0xf3, 0xc3};
    constexpr std::array<std::uint8_t, 25> input_exit_helper_loop{
        0x0e, 0x1f, 0xb9, 0x0f, 0x00, 0xbe, 0x68, 0x17, 0x51,
        0x56, 0xe8, 0xd5, 0xff, 0xfe, 0xc0, 0xe8, 0xe9, 0xfd,
        0x5e, 0x59, 0x83, 0xc6, 0x04, 0xe2, 0xef};
    constexpr std::array<std::uint8_t, 29> input_exit_helper_selector{
        0x56, 0xa1, 0x81, 0x11, 0x03, 0x06, 0xf7, 0x18, 0x25,
        0xff, 0x03, 0xa3, 0xf7, 0x18, 0x8b, 0xf0, 0xac, 0x5e,
        0x25, 0x3f, 0x00, 0x3c, 0x24, 0x72, 0x04, 0x2c, 0x18,
        0xeb, 0xf8};
    constexpr std::array<std::uint8_t, 13> input_exit_helper_patch_offset_builder{
        0x56, 0xb9, 0x70, 0x01, 0x32, 0xe4, 0xf7, 0xe1,
        0x2e, 0xa3, 0x41, 0x13, 0x5e};
    constexpr std::array<std::uint8_t, 24> input_exit_helper_position_dispatch{
        0xad, 0x2e, 0xa3, 0x51, 0x13, 0xad, 0x2e, 0xa3,
        0x4f, 0x13, 0xbf, 0x3d, 0x13, 0xbe, 0x57, 0x13,
        0xa5, 0xa5, 0x0e, 0x07, 0xbb, 0x49, 0x13, 0xb8};
    constexpr std::array<std::uint8_t, 8> input_exit_helper_position_table_first{
        0x1c, 0x00, 0x7c, 0x00, 0x2e, 0x00, 0x7c, 0x00};
    constexpr std::array<std::uint8_t, 4> input_exit_helper_position_table_last{
        0x18, 0x01, 0x7c, 0x00};
    constexpr std::array<std::uint8_t, 10> clean_exit{
        0x32, 0xc0, 0x2e, 0xa2, 0x0e, 0x1a, 0x8b, 0x26,
        0xa0, 0x1a};
    constexpr std::array<std::uint8_t, 8> dos_exit{
        0x2e, 0xa0, 0x0e, 0x1a, 0xb4, 0x4c, 0xcd, 0x21};
    constexpr std::array<std::uint8_t, 40> title_driver_setup{
        0x0e,0x1f,0x0e,0x07,0x8c,0xc8,0x8e,0xd0,0xb8,0x00,0xda,0x89,0xc4,
        0xb8,0x00,0x00,0x0e,0x07,0xbb,0xc4,0x1a,0xe8,0x8a,0xe5,0x2e,0xa3,
        0x9c,0x1a,0x88,0xe0,0x2e,0xa2,0xaa,0x1a,0xa2,0x07,0x01,0x89,0x26,0xa0};
    constexpr std::array<std::uint8_t, 13> private_wrapper{
        0x1e,0x56,0x57,0x55,0x06,0xcd,0x91,0x07,0x5d,0x5f,0x5e,0x1f,0xc3};
    constexpr std::array<std::uint8_t, 2> title_driver_record{0x01,0x00};

    if (!has_bytes(titles_executable, 0, entry_jump)) {
        throw std::runtime_error("Unsupported Millennium DOS title entry");
    }
    constexpr std::size_t file_to_load_bias = 0x100;
    constexpr std::size_t title_selection_offset = 0x1c14 - file_to_load_bias;
    constexpr std::size_t input_branch_offset = 0x1c28 - file_to_load_bias;
    constexpr std::size_t clean_exit_offset = 0x1c5a - file_to_load_bias;
    constexpr std::size_t dos_exit_offset = 0x1a12 - file_to_load_bias;
    if (!has_bytes(titles_executable, title_selection_offset, title_selection)
        || !has_bytes(titles_executable, input_branch_offset, input_branch)
        || !has_bytes(titles_executable, clean_exit_offset, clean_exit)
        || !has_bytes(titles_executable, dos_exit_offset, dos_exit)
        || !has_bytes(titles_executable, 0x1b80 - file_to_load_bias, title_driver_setup)
        || !has_bytes(titles_executable, 0x0122 - file_to_load_bias, private_wrapper)
        || !has_bytes(titles_executable, 0x1ac4 - file_to_load_bias, title_driver_record)
        || !has_bytes(titles_executable, 0x1884 - file_to_load_bias, input_exit_loading_text)
        || !has_bytes(titles_executable, 0x1968 - file_to_load_bias, input_exit_private_driver_entry)
        || !has_bytes(titles_executable, 0x1931 - file_to_load_bias, input_exit_private_driver_loop)
        || !has_bytes(titles_executable, 0x1917 - file_to_load_bias, input_exit_helper_loop)
        || !has_bytes(titles_executable, 0x18f9 - file_to_load_bias, input_exit_helper_selector)
        || !has_bytes(titles_executable, 0x1712 - file_to_load_bias, input_exit_helper_patch_offset_builder)
        || !has_bytes(titles_executable, 0x174a - file_to_load_bias, input_exit_helper_position_dispatch)
        || !has_bytes(titles_executable, 0x1768 - file_to_load_bias, input_exit_helper_position_table_first)
        || !has_bytes(titles_executable, 0x17a0 - file_to_load_bias, input_exit_helper_position_table_last)) {
        throw std::runtime_error("Unsupported Millennium DOS title control flow");
    }
    constexpr std::size_t title_selection_callee_address = 0x1725;
    if (!has_bytes(titles_executable, title_selection_callee_address - file_to_load_bias,
                   title_selection_callee_prefix)) {
        throw std::runtime_error("Unsupported Millennium DOS title selection callee");
    }
    constexpr std::size_t title_selection_callee_jle_target = 0x1732;
    if (!has_bytes(titles_executable, title_selection_callee_jle_target - file_to_load_bias,
                   title_selection_callee_jle_target_prefix)) {
        throw std::runtime_error("Unsupported Millennium DOS title selection JLE target");
    }
    if (!has_bytes(titles_executable, 0x1390 - file_to_load_bias,
                   title_selection_nested_callee_prefix)) {
        throw std::runtime_error("Unsupported Millennium DOS title selection nested callee");
    }
    if (!has_bytes(titles_executable, 0x13c - file_to_load_bias,
                   title_selection_nested_leaf_prefix)) {
        throw std::runtime_error("Unsupported Millennium DOS title selection nested leaf");
    }
    static_cast<void>(require_unique(titles_executable, transition_setup, "title transition loop"));
    static_cast<void>(require_unique(titles_executable, input_poll, "title input poll"));

    // This is a caller-side fact only.  MILL.COM loads DX with each adjacent
    // program string, makes two near calls to the same local target, and
    // tests AL between them.  We deliberately do not assign a meaning to
    // the target or to either post-call status test.
    constexpr std::array<std::uint8_t, 22> launcher_call_chain{
        0xba, 0x8f, 0x06, 0xe8, 0xd9, 0x00, 0x22, 0xc0,
        0x75, 0x19, 0x0e, 0x1f, 0xba, 0x9a, 0x06, 0xe8,
        0xcd, 0x00, 0x22, 0xc0, 0x75, 0x0d};
    // The shared local target is preserved as raw control flow.  Its first
    // local branch is JC +5 at 0x345: the non-taken bytes end in RET at
    // 0x34b, while the taken destination starts at 0x34c.  No interrupt or
    // return semantics are inferred here.
    constexpr std::array<std::uint8_t, 50> launcher_common_routine{
        0x8c, 0xc8, 0x89, 0x06, 0x7e, 0x06, 0x89, 0x06,
        0x82, 0x06, 0x89, 0x06, 0x86, 0x06, 0x8e, 0xc0,
        0xbb, 0x7a, 0x06, 0x89, 0x26, 0xf7, 0x05, 0xb8,
        0x00, 0x4b, 0xcd, 0x21, 0x8c, 0xc9, 0x8e, 0xd1,
        0x2e, 0x8b, 0x26, 0xf7, 0x05, 0x8e, 0xd9, 0x8e,
        0xc1, 0x72, 0x05, 0xb4, 0x4d, 0xcd, 0x21, 0xc3,
        0xba, 0x70};
    constexpr std::array<std::uint8_t, 14> launcher_branch_target_bytes{
        0xba, 0x70, 0x03, 0x89, 0xd2, 0xb4, 0x09, 0xcd,
        0x21, 0xb8, 0x0a, 0x4c, 0xcd, 0x21};
    // This static caller-side range is immediately before the DX=0x68f
    // setup.  It records a post-call JE and the later local near call without
    // claiming either call's return behavior or assigning meaning to AL.
    constexpr std::array<std::uint8_t, 45> launcher_pre_title_chain{
        0xe8, 0xfe, 0x02, 0x22, 0xc0, 0x74, 0x03, 0x05,
        0x02, 0x00, 0x8b, 0xd8, 0x04, 0x30, 0xbe, 0x88,
        0x06, 0x2e, 0x88, 0x44, 0x02, 0xd1, 0xe3, 0x8b,
        0x97, 0x6e, 0x06, 0xc7, 0x06, 0xd5, 0x05, 0xde,
        0x03, 0xe8, 0x9b, 0x00, 0x33, 0xd2, 0xb8, 0x95,
        0x25, 0xcd, 0x21, 0x0e, 0x1f};
    // The local target of the near call at 0x231 is only profiled through its
    // first conditional split.  The conditional's meaning and all interrupt
    // effects remain deliberately unmodelled.
    constexpr std::array<std::uint8_t, 19> launcher_pre_title_callee_prefix{
        0xb8, 0x00, 0x3d, 0xcd, 0x21, 0x73, 0x0c, 0x0e,
        0x1f, 0x8b, 0x16, 0xd5, 0x05, 0xb4, 0x09, 0xcd,
        0x21, 0xeb, 0x87};
    constexpr std::array<std::uint8_t, 14> launcher_pre_title_callee_jnc_target_prefix{
        0x50, 0x93, 0x33, 0xd2, 0x33, 0xc9, 0xb8, 0x02,
        0x42, 0xcd, 0x21, 0x72, 0xe7, 0x50};
    constexpr std::array<std::uint8_t, 12> launcher_pre_title_callee_jc_target_prefix{
        0x0e, 0x1f, 0x8b, 0x16, 0xd5, 0x05, 0xb4, 0x09,
        0xcd, 0x21, 0xeb, 0x87};
    constexpr std::array<std::uint8_t, 16> launcher_pre_title_callee_join_prefix{
        0x50, 0xc5, 0x16, 0xe7, 0x05, 0xb8, 0x91, 0x25,
        0xcd, 0x21, 0x2e, 0xc5, 0x16, 0xeb, 0x05, 0xb8};
    constexpr std::array<std::uint8_t, 10> launcher_pre_title_callee_join_branch{
        0xb8, 0x08, 0x25, 0xcd, 0x21, 0x58, 0x22, 0xc0,
        0x74, 0x14};
    constexpr std::array<std::uint8_t, 48> launcher_exec_helper{
        0x8c,0xc8,0x89,0x06,0x7e,0x06,0x89,0x06,0x82,0x06,0x89,0x06,0x86,0x06,
        0x8e,0xc0,0xbb,0x7a,0x06,0x89,0x26,0xf7,0x05,0xb8,0x00,0x4b,0xcd,0x21,
        0x8c,0xc9,0x8e,0xd1,0x2e,0x8b,0x26,0xf7,0x05,0x8e,0xd9,0x8e,0xc1,0x72,
        0x05,0xb4,0x4d,0xcd,0x21,0xc3};
    constexpr std::array<std::uint8_t, 14> launcher_exec_param_block{
        0x00,0x00,0x88,0x06,0x00,0x00,0x5c,0x00,0x00,0x00,0x5c,0x00,0x00,0x00};
    // The raw private-vector installation is preceded by a local loader call.
    // The call's DOS effects are intentionally not evaluated: this byte range
    // establishes only the direct call target and the literal DX=0 / AX=$2591
    // setup at the following interrupt instruction.
    constexpr std::array<std::uint8_t, 12> launcher_private_interrupt_install{
        0xe8, 0xc8, 0x00, 0x33, 0xd2, 0xb8, 0x91, 0x25,
        0xcd, 0x21, 0x0e, 0x1f};
    constexpr std::array<std::uint8_t, 13> launcher_private_interrupt_query{
        0xb8, 0x91, 0x35, 0xcd, 0x21, 0x89, 0x1e, 0xe7,
        0x05, 0x8c, 0x06, 0xe9, 0x05};
    constexpr std::array<std::uint8_t, 10> launcher_private_interrupt_restore{
        0x50, 0xc5, 0x16, 0xe7, 0x05, 0xb8, 0x91, 0x25,
        0xcd, 0x21};
    // Before the first call to $02cf, AL==1 keeps DX=$0617; the other path
    // loads DX=$05f9. $02cf opens the selected original file, seeks to its
    // end, rounds the length to paragraphs, allocates a segment, rewinds,
    // reads CX bytes at DS:0000, closes, and returns. The code proves this
    // transfer ABI but deliberately does not assign any DOS result, segment,
    // or handler execution semantics.
    constexpr std::array<std::uint8_t, 12> launcher_private_interrupt_handler_selection{
        0xba, 0x17, 0x06, 0x3c, 0x01, 0x74, 0x19, 0xba, 0xf9,
        0x05, 0xeb, 0x14};
    constexpr std::array<std::uint8_t, 45> launcher_video_selection_scan{
        0xbb, 0x80, 0x00, 0x43, 0x80, 0x3f, 0x0d, 0x74, 0x1a,
        0xb0, 0x01, 0x80, 0x3f, 0x65, 0x74, 0x1d, 0x80, 0x3f,
        0x45, 0x74, 0x18, 0xb0, 0x02, 0x80, 0x3f, 0x6d, 0x74,
        0x11, 0x80, 0x3f, 0x4d, 0x74, 0x0c, 0xeb, 0xe0, 0xe8,
        0xde, 0x03, 0x22, 0xc0, 0x75, 0x03, 0xe9, 0x9f, 0x00};
    constexpr std::array<std::uint8_t, 77> launcher_private_interrupt_handler_loader{
        0xb8, 0x00, 0x3d, 0xcd, 0x21, 0x73, 0x0c, 0x0e, 0x1f,
        0x8b, 0x16, 0xd5, 0x05, 0xb4, 0x09, 0xcd, 0x21, 0xeb,
        0x87, 0x50, 0x93, 0x33, 0xd2, 0x33, 0xc9, 0xb8, 0x02,
        0x42, 0xcd, 0x21, 0x72, 0xe7, 0x50, 0x05, 0x0f, 0x00,
        0xb1, 0x04, 0xd3, 0xe8, 0x93, 0xb4, 0x48, 0xcd, 0x21,
        0x72, 0xd8, 0x8e, 0xd8, 0x5f, 0x5b, 0x33, 0xd2, 0x33,
        0xc9, 0xb8, 0x00, 0x42, 0xcd, 0x21, 0x72, 0xc9, 0xb4,
        0x3f, 0x8b, 0xcf, 0x33, 0xd2, 0xcd, 0x21, 0x72, 0xbf,
        0xb4, 0x3e, 0xcd, 0x21, 0xc3};
    constexpr std::array<std::uint8_t, 7> launcher_pre_title_callee_join_target_prefix{
        0xb4, 0x4c, 0xcd, 0x21, 0x32, 0xc0, 0xcf};
    constexpr std::array<std::uint8_t, 11> title_name{
        'T', 'I', 'T', 'L', 'E', 'S', '.', 'E', 'X', 'E', 0};
    constexpr std::array<std::uint8_t, 11> game_name{
        '2', '2', '0', '0', 'a', 'd', '.', 'e', 'x', 'e', 0};
    constexpr std::array<std::uint8_t, 10> ega640_name{
        'e', 'g', 'a', '6', '4', '0', '.', 'b', 'i', 'n'};
    constexpr std::array<std::uint8_t, 8> mcga_name{
        'm', 'c', 'g', 'a', '.', 'b', 'i', 'n'};
    constexpr std::size_t mill_load_bias = 0x100;
    const auto launcher_chain_offset = require_unique(
        mill_launcher, launcher_call_chain, "launcher caller-side call chain");
    constexpr std::size_t title_call_in_chain = 3;
    constexpr std::size_t game_call_in_chain = 15;
    const auto title_call_offset = launcher_chain_offset + title_call_in_chain;
    const auto game_call_offset = launcher_chain_offset + game_call_in_chain;
    const auto title_call_address = title_call_offset + mill_load_bias;
    const auto game_call_address = game_call_offset + mill_load_bias;
    const auto title_call_target = title_call_address + 3 + 0x00d9;
    const auto game_call_target = game_call_address + 3 + 0x00cd;
    if (title_call_target != game_call_target) {
        throw std::runtime_error("Invalid Millennium DOS launcher common call target");
    }
    if (title_call_target < mill_load_bias
        || !has_bytes(mill_launcher, title_call_target - mill_load_bias, launcher_common_routine)) {
        throw std::runtime_error("Unsupported Millennium DOS launcher common routine");
    }
    if (!has_bytes(mill_launcher, 0x031c - mill_load_bias, launcher_exec_helper)
        || !has_bytes(mill_launcher, 0x067a - mill_load_bias, launcher_exec_param_block)) {
        throw std::runtime_error("Unsupported Millennium DOS launcher EXEC boundary");
    }
    constexpr std::size_t common_branch_target = 0x34c;
    if (!has_bytes(mill_launcher, common_branch_target - mill_load_bias,
                   launcher_branch_target_bytes)) {
        throw std::runtime_error("Unsupported Millennium DOS launcher branch target");
    }
    constexpr std::size_t pre_title_chain_address = 0x210;
    if (!has_bytes(mill_launcher, pre_title_chain_address - mill_load_bias,
                   launcher_pre_title_chain)) {
        throw std::runtime_error("Unsupported Millennium DOS launcher pre-title chain");
    }
    constexpr std::size_t pre_title_callee_address = 0x2cf;
    if (!has_bytes(mill_launcher, pre_title_callee_address - mill_load_bias,
                   launcher_pre_title_callee_prefix)) {
        throw std::runtime_error("Unsupported Millennium DOS launcher pre-title callee prefix");
    }
    constexpr std::size_t pre_title_callee_jnc_target = 0x2e2;
    if (!has_bytes(mill_launcher, pre_title_callee_jnc_target - mill_load_bias,
                   launcher_pre_title_callee_jnc_target_prefix)) {
        throw std::runtime_error("Unsupported Millennium DOS launcher JNC-target prefix");
    }
    constexpr std::size_t pre_title_callee_jc_target = 0x2d6;
    if (!has_bytes(mill_launcher, pre_title_callee_jc_target - mill_load_bias,
                   launcher_pre_title_callee_jc_target_prefix)) {
        throw std::runtime_error("Unsupported Millennium DOS launcher JC-target prefix");
    }
    constexpr std::size_t pre_title_callee_join = 0x269;
    if (!has_bytes(mill_launcher, pre_title_callee_join - mill_load_bias,
                   launcher_pre_title_callee_join_prefix)
        || !has_bytes(mill_launcher, 0x2aa - mill_load_bias,
                      launcher_pre_title_callee_join_branch)) {
        throw std::runtime_error("Unsupported Millennium DOS launcher join branch");
    }
    constexpr std::size_t pre_title_callee_join_target = 0x2c8;
    if (!has_bytes(mill_launcher, pre_title_callee_join_target - mill_load_bias,
                   launcher_pre_title_callee_join_target_prefix)) {
        throw std::runtime_error("Unsupported Millennium DOS launcher join target");
    }
    constexpr std::size_t private_interrupt_loader_call_address = 0x204;
    constexpr std::size_t private_interrupt_loader_call_target = 0x2cf;
    constexpr std::size_t private_interrupt_install_address = 0x209;
    if (!has_bytes(mill_launcher, private_interrupt_loader_call_address - mill_load_bias,
                   launcher_private_interrupt_install)) {
        throw std::runtime_error("Unsupported Millennium DOS private interrupt installation");
    }
    constexpr std::size_t private_interrupt_query_address = 0x167;
    constexpr std::size_t private_interrupt_restore_address = 0x269;
    if (!has_bytes(mill_launcher, private_interrupt_query_address - mill_load_bias,
                   launcher_private_interrupt_query)
        || !has_bytes(mill_launcher, private_interrupt_restore_address - mill_load_bias,
                      launcher_private_interrupt_restore)) {
        throw std::runtime_error("Unsupported Millennium DOS private interrupt preservation chain");
    }
    constexpr std::size_t private_interrupt_handler_selection_address = 0x1de;
    constexpr std::size_t video_selection_scan_address = 0x19d;
    if (!has_bytes(mill_launcher, video_selection_scan_address - mill_load_bias,
                   launcher_video_selection_scan)
        || !has_bytes(mill_launcher, private_interrupt_handler_selection_address - mill_load_bias,
                   launcher_private_interrupt_handler_selection)
        || !has_bytes(mill_launcher, private_interrupt_loader_call_target - mill_load_bias,
                      launcher_private_interrupt_handler_loader)
        || !has_bytes(mill_launcher, 0x0617 - mill_load_bias, ega640_name)
        || !has_bytes(mill_launcher, 0x05f9 - mill_load_bias, mcga_name)) {
        throw std::runtime_error("Unsupported Millennium DOS private interrupt handler loader");
    }
    const auto title_offset = require_unique(mill_launcher, title_name, "launcher title program");
    const auto game_offset = require_unique(mill_launcher, game_name, "launcher game program");
    if (title_offset >= game_offset || game_offset != title_offset + title_name.size()) {
        throw std::runtime_error("Invalid Millennium DOS launcher hand-off order");
    }

    return {
        .title_entry_address = 0x1b80,
        .title_selection_callee_entry_address = 0x1725,
        .title_selection_callee_branch_address = 0x172f,
        .title_selection_callee_branch_target = 0x1732,
        .title_selection_callee_fallthrough_return = 0x1731,
        .title_selection_callee_jle_target_call_address = 0x173d,
        .title_selection_callee_jle_target_call_target = 0x1390,
        .title_selection_nested_callee_call_address = 0x13bb,
        .title_selection_nested_callee_call_target = 0x13c,
        .title_selection_nested_callee_terminal_address = 0x14b,
        .title_resource_index = 0,
        .intro_transition_steps = 0x25,
        .intro_step_stride = 0x170,
        .input_interrupt = 0x21,
        .input_service = 0x06,
        .input_parameter = 0xff,
        .input_nonzero_exit_address = 0x1c54,
        .input_exit_first_call_address = 0x1c54,
        .input_exit_first_call_target = 0x1968,
        .input_exit_loading_text_address = 0x1884,
        .input_exit_loading_text = "    LOADING    2",
        .input_exit_private_driver_entry_address = 0x1968,
        .input_exit_private_driver_loop_address = 0x1931,
        .input_exit_private_driver_wrapper_address = 0x0122,
        .input_exit_private_driver_function = 0x0013,
        .input_exit_private_driver_call_count = 5,
        .input_exit_private_driver_helper_address = 0x1917,
        .input_exit_helper_selector_iterations = 15,
        .input_exit_helper_selector_state_address = 0x1181,
        .input_exit_helper_selector_accumulator_address = 0x18f7,
        .input_exit_helper_selector_mask = 0x03ff,
        .input_exit_helper_selector_range = 0x24,
        .input_exit_helper_selector_subtract = 0x18,
        .input_exit_helper_resource_index_bias = 1,
        .input_exit_helper_resource_loader_address = 0x1712,
        .input_exit_helper_patch_offset_builder_address = 0x1712,
        .input_exit_helper_patch_offset_stride = 0x0170,
        .input_exit_helper_patch_offset_cell_address = 0x1341,
        .input_exit_helper_position_table_address = 0x1768,
        .input_exit_helper_position_count = 15,
        .input_exit_helper_position_stride = 4,
        .input_exit_helper_private_driver_function = 6,
        .exit_code = 0,
        .title_private_interrupt_wrapper_address = 0x0122,
        .title_private_interrupt_record_address = 0x1ac4,
        .title_private_interrupt_function = 0,
        .title_private_interrupt_result_word_address = 0x1a9c,
        .title_private_interrupt_result_low_byte_address = 0x1aaa,
        .title_private_interrupt_result_high_byte_address = 0x0107,
        .title_private_interrupt_equal_branch_target = 0x1ac6,
        .title_private_interrupt_other_branch_target = 0x1ada,
        .launcher_title_program_address = static_cast<std::uint16_t>(title_offset + mill_load_bias),
        .launcher_game_program_address = static_cast<std::uint16_t>(game_offset + mill_load_bias),
        .launcher_title_call_address = static_cast<std::uint16_t>(title_call_address),
        .launcher_game_call_address = static_cast<std::uint16_t>(game_call_address),
        .launcher_common_call_target = static_cast<std::uint16_t>(title_call_target),
        .launcher_common_branch_address = 0x345,
        .launcher_common_branch_target = common_branch_target,
        .launcher_common_fallthrough_return = 0x34b,
        .launcher_common_branch_target_static_boundary = 0x35a,
        .launcher_exec_helper_address = 0x031c,
        .launcher_exec_param_block_address = 0x067a,
        .launcher_exec_saved_stack_address = 0x05f7,
        .launcher_exec_interrupt_site = 0x0337,
        .launcher_exec_result_interrupt_site = 0x0348,
        .launcher_exec_carry_branch_address = 0x0345,
        .launcher_exec_noncarry_return_address = 0x034b,
        .launcher_pre_title_gate_address = 0x215,
        .launcher_pre_title_gate_target = 0x21a,
        .launcher_pre_title_call_address = 0x231,
        .launcher_pre_title_call_target = 0x2cf,
        .launcher_pre_title_callee_branch_address = 0x2d4,
        .launcher_pre_title_callee_branch_target = 0x2e2,
        .launcher_pre_title_callee_fallthrough_jump_address = 0x2e0,
        .launcher_pre_title_callee_fallthrough_jump_target = 0x269,
        .launcher_pre_title_callee_jnc_target_branch_address = 0x2ed,
        .launcher_pre_title_callee_jnc_target_branch_target = 0x2d6,
        .launcher_pre_title_callee_jc_target_jump_address = 0x2e0,
        .launcher_pre_title_callee_jc_target_jump_target = 0x269,
        .launcher_pre_title_callee_join_branch_address = 0x2b2,
        .launcher_pre_title_callee_join_branch_target = 0x2c8,
        .launcher_pre_title_callee_join_branch_terminal_address = 0x2ce,
        .launcher_private_interrupt_loader_call_address = private_interrupt_loader_call_address,
        .launcher_private_interrupt_loader_call_target = private_interrupt_loader_call_target,
        .launcher_private_interrupt_install_address = private_interrupt_install_address,
        .launcher_private_interrupt_number = 0x91,
        .launcher_private_interrupt_handler_offset = 0,
        .launcher_private_interrupt_saved_offset_cell = 0x5e7,
        .launcher_private_interrupt_saved_segment_cell = 0x5e9,
        .launcher_private_interrupt_restore_address = private_interrupt_restore_address,
        .launcher_private_interrupt_handler_loader_entry = private_interrupt_loader_call_target,
        .launcher_private_interrupt_handler_destination_offset = 0,
        .launcher_private_interrupt_handler_open_service = 0x3d,
        .launcher_private_interrupt_handler_seek_end_service = 0x42,
        .launcher_private_interrupt_handler_allocate_service = 0x48,
        .launcher_private_interrupt_handler_rewind_service = 0x42,
        .launcher_private_interrupt_handler_read_service = 0x3f,
        .launcher_private_interrupt_handler_close_service = 0x3e,
        .launcher_video_selection_scan_address = static_cast<std::uint16_t>(video_selection_scan_address),
        .launcher_video_selection_default_detector_address = 0x05a1,
        .launcher_video_selection_map_address = static_cast<std::uint16_t>(private_interrupt_handler_selection_address),
        .launcher_private_interrupt_handler_first_selector = 1,
        .launcher_private_interrupt_handler_first_program_address = 0x0617,
        .launcher_private_interrupt_handler_other_selector = 2,
        .launcher_private_interrupt_handler_other_program_address = 0x05f9,
        .launcher_private_interrupt_handler_first_program = "ega640.bin",
        .launcher_private_interrupt_handler_other_program = "mcga.bin",
        .launcher_title_offset = title_offset,
        .launcher_game_offset = game_offset,
        .launcher_title_program = "TITLES.EXE",
        .launcher_game_program = "2200ad.exe",
    };
}

} // namespace eon
