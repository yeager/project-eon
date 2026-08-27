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
    constexpr std::array<std::uint8_t, 13> transition_setup{
        0xb9, 0x25, 0x00, 0xba, 0x70, 0x01, 0x51, 0x52,
        0xbe, 0x0c, 0x01, 0x8b, 0x04};
    constexpr std::array<std::uint8_t, 7> input_poll{
        0xb4, 0x06, 0xb2, 0xff, 0xcd, 0x21, 0xc3};
    constexpr std::array<std::uint8_t, 11> input_branch{
        0xe8, 0xdf, 0xf0, 0x22, 0xc0, 0x75, 0x25, 0xb8,
        0x13, 0x00, 0xe8};
    constexpr std::array<std::uint8_t, 10> clean_exit{
        0x32, 0xc0, 0x2e, 0xa2, 0x0e, 0x1a, 0x8b, 0x26,
        0xa0, 0x1a};
    constexpr std::array<std::uint8_t, 8> dos_exit{
        0x2e, 0xa0, 0x0e, 0x1a, 0xb4, 0x4c, 0xcd, 0x21};

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
        || !has_bytes(titles_executable, dos_exit_offset, dos_exit)) {
        throw std::runtime_error("Unsupported Millennium DOS title control flow");
    }
    static_cast<void>(require_unique(titles_executable, transition_setup, "title transition loop"));
    static_cast<void>(require_unique(titles_executable, input_poll, "title input poll"));

    // This is a caller-side fact only.  MILL.COM loads DX with each adjacent
    // program string, makes two near calls to the same local target, and
    // tests AX between them.  We deliberately do not assign a meaning to
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
    // claiming either call's return behavior or assigning meaning to AX.
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
    constexpr std::array<std::uint8_t, 11> title_name{
        'T', 'I', 'T', 'L', 'E', 'S', '.', 'E', 'X', 'E', 0};
    constexpr std::array<std::uint8_t, 11> game_name{
        '2', '2', '0', '0', 'a', 'd', '.', 'e', 'x', 'e', 0};
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
    const auto title_offset = require_unique(mill_launcher, title_name, "launcher title program");
    const auto game_offset = require_unique(mill_launcher, game_name, "launcher game program");
    if (title_offset >= game_offset || game_offset != title_offset + title_name.size()) {
        throw std::runtime_error("Invalid Millennium DOS launcher hand-off order");
    }

    return {
        .title_entry_address = 0x1b80,
        .title_resource_index = 0,
        .intro_transition_steps = 0x25,
        .intro_step_stride = 0x170,
        .input_interrupt = 0x21,
        .input_service = 0x06,
        .input_parameter = 0xff,
        .exit_code = 0,
        .launcher_title_program_address = static_cast<std::uint16_t>(title_offset + mill_load_bias),
        .launcher_game_program_address = static_cast<std::uint16_t>(game_offset + mill_load_bias),
        .launcher_title_call_address = static_cast<std::uint16_t>(title_call_address),
        .launcher_game_call_address = static_cast<std::uint16_t>(game_call_address),
        .launcher_common_call_target = static_cast<std::uint16_t>(title_call_target),
        .launcher_common_branch_address = 0x345,
        .launcher_common_branch_target = common_branch_target,
        .launcher_common_fallthrough_return = 0x34b,
        .launcher_common_branch_target_static_boundary = 0x35a,
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
        .launcher_title_offset = title_offset,
        .launcher_game_offset = game_offset,
        .launcher_title_program = "TITLES.EXE",
        .launcher_game_program = "2200ad.exe",
    };
}

} // namespace eon
