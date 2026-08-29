#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace eon {

// Facts extracted from the supplied English DOS title executable and its
// MILL.COM launcher.  This deliberately models the verified hand-off only;
// it is not a replacement implementation of TITLES.EXE's renderer.
struct MillenniumDosTitleFlow {
    std::uint16_t title_entry_address = 0;
    std::uint16_t title_selection_callee_entry_address = 0;
    std::uint16_t title_selection_callee_branch_address = 0;
    std::uint16_t title_selection_callee_branch_target = 0;
    std::uint16_t title_selection_callee_fallthrough_return = 0;
    std::uint16_t title_selection_callee_jle_target_call_address = 0;
    std::uint16_t title_selection_callee_jle_target_call_target = 0;
    std::uint16_t title_selection_nested_callee_call_address = 0;
    std::uint16_t title_selection_nested_callee_call_target = 0;
    std::uint16_t title_selection_nested_callee_terminal_address = 0;
    std::uint16_t title_resource_index = 0;
    std::uint16_t intro_transition_steps = 0;
    std::uint16_t intro_step_stride = 0;
    std::uint8_t input_interrupt = 0;
    std::uint8_t input_service = 0;
    std::uint8_t input_parameter = 0;
    std::uint16_t input_nonzero_exit_address = 0;
    std::uint16_t input_exit_first_call_address = 0;
    std::uint16_t input_exit_first_call_target = 0;
    std::uint16_t input_exit_loading_text_address = 0;
    std::string input_exit_loading_text;
    // Static post-poll private-driver boundary; ABI and visible effects remain unknown.
    std::uint16_t input_exit_private_driver_entry_address = 0;
    std::uint16_t input_exit_private_driver_loop_address = 0;
    std::uint16_t input_exit_private_driver_wrapper_address = 0;
    std::uint16_t input_exit_private_driver_function = 0;
    std::uint16_t input_exit_private_driver_call_count = 0;
    std::uint16_t input_exit_private_driver_helper_address = 0;
    std::uint16_t input_exit_helper_selector_iterations = 0;
    std::uint16_t input_exit_helper_selector_state_address = 0;
    std::uint16_t input_exit_helper_selector_accumulator_address = 0;
    std::uint16_t input_exit_helper_selector_mask = 0;
    std::uint8_t input_exit_helper_selector_range = 0;
    std::uint8_t input_exit_helper_selector_subtract = 0;
    std::uint8_t input_exit_helper_resource_index_bias = 0;
    std::uint16_t input_exit_helper_resource_loader_address = 0;
    std::uint16_t input_exit_helper_patch_offset_builder_address = 0;
    std::uint16_t input_exit_helper_patch_offset_stride = 0;
    std::uint16_t input_exit_helper_patch_offset_cell_address = 0;
    std::uint16_t input_exit_helper_position_table_address = 0;
    std::uint16_t input_exit_helper_position_count = 0;
    std::uint16_t input_exit_helper_position_stride = 0;
    std::uint16_t input_exit_helper_private_driver_function = 0;
    std::uint8_t exit_code = 0;
    // The title entry makes a direct function-$00 request through the already
    // installed private INT 91h vector. These fields are byte-level operands;
    // neither handler installation nor AX/AL after the external interrupt is
    // inferred by the host.
    std::uint16_t title_private_interrupt_wrapper_address = 0;
    std::uint16_t title_private_interrupt_record_address = 0;
    std::uint16_t title_private_interrupt_function = 0;
    std::uint16_t title_private_interrupt_result_word_address = 0;
    std::uint16_t title_private_interrupt_result_low_byte_address = 0;
    std::uint16_t title_private_interrupt_result_high_byte_address = 0;
    std::uint16_t title_private_interrupt_equal_branch_target = 0;
    std::uint16_t title_private_interrupt_other_branch_target = 0;
    // Loaded addresses in the flat MILL.COM image.  These identify only the
    // observed register loads and near-call edges; they do not model EXEC or
    // any return value from the callee.
    std::uint16_t launcher_title_program_address = 0;
    std::uint16_t launcher_game_program_address = 0;
    std::uint16_t launcher_title_call_address = 0;
    std::uint16_t launcher_game_call_address = 0;
    std::uint16_t launcher_common_call_target = 0;
    std::uint16_t launcher_common_branch_address = 0;
    std::uint16_t launcher_common_branch_target = 0;
    std::uint16_t launcher_common_fallthrough_return = 0;
    std::uint16_t launcher_common_branch_target_static_boundary = 0;
    // The common program helper has a literal DOS EXEC boundary. The caller
    // invokes it for TITLES.EXE and, only after an unmodelled return/result,
    // for 2200AD.EXE. These fields expose the parent-owned parameter block and
    // post-EXEC restoration bytes without emulating DOS or child lifetime.
    std::uint16_t launcher_exec_helper_address = 0;
    std::uint16_t launcher_exec_param_block_address = 0;
    std::uint16_t launcher_exec_saved_stack_address = 0;
    std::uint16_t launcher_exec_interrupt_site = 0;
    std::uint16_t launcher_exec_result_interrupt_site = 0;
    std::uint16_t launcher_exec_carry_branch_address = 0;
    std::uint16_t launcher_exec_noncarry_return_address = 0;
    std::uint16_t launcher_pre_title_gate_address = 0;
    std::uint16_t launcher_pre_title_gate_target = 0;
    std::uint16_t launcher_pre_title_call_address = 0;
    std::uint16_t launcher_pre_title_call_target = 0;
    std::uint16_t launcher_pre_title_callee_branch_address = 0;
    std::uint16_t launcher_pre_title_callee_branch_target = 0;
    std::uint16_t launcher_pre_title_callee_fallthrough_jump_address = 0;
    std::uint16_t launcher_pre_title_callee_fallthrough_jump_target = 0;
    std::uint16_t launcher_pre_title_callee_jnc_target_branch_address = 0;
    std::uint16_t launcher_pre_title_callee_jnc_target_branch_target = 0;
    std::uint16_t launcher_pre_title_callee_jc_target_jump_address = 0;
    std::uint16_t launcher_pre_title_callee_jc_target_jump_target = 0;
    std::uint16_t launcher_pre_title_callee_join_branch_address = 0;
    std::uint16_t launcher_pre_title_callee_join_branch_target = 0;
    std::uint16_t launcher_pre_title_callee_join_branch_terminal_address = 0;
    // MILL.COM first reaches its private 91h service through this local load
    // routine. The following raw setup clears DX and invokes AX=$2591. That
    // fixes the handler offset at zero, but the segment comes from unmodelled
    // DOS results, so no handler bytes or segment are claimed here.
    std::uint16_t launcher_private_interrupt_loader_call_address = 0;
    std::uint16_t launcher_private_interrupt_loader_call_target = 0;
    std::uint16_t launcher_private_interrupt_install_address = 0;
    std::uint8_t launcher_private_interrupt_number = 0;
    std::uint16_t launcher_private_interrupt_handler_offset = 0;
    // The original 91h query stores BX/ES in this adjacent raw cell pair.
    // Later code loads DX/DS from that same pair before a second AX=$2591
    // call. Neither interrupt's effect nor the cells' runtime contents are
    // inferred.
    std::uint16_t launcher_private_interrupt_saved_offset_cell = 0;
    std::uint16_t launcher_private_interrupt_saved_segment_cell = 0;
    std::uint16_t launcher_private_interrupt_restore_address = 0;
    // The first $0124-loader call opens one of these in-archive video code
    // files, computes its paragraph count from the file length, allocates a
    // DOS segment, rewinds, and reads it to offset zero in that segment. The
    // numeric segment and every DOS result remain unmodelled. The following
    // AX=$2591 call therefore has a static ABI of DS:0000, not a guessed
    // pointer value or handler return contract.
    std::uint16_t launcher_private_interrupt_handler_loader_entry = 0;
    std::uint16_t launcher_private_interrupt_handler_destination_offset = 0;
    std::uint8_t launcher_private_interrupt_handler_open_service = 0;
    std::uint8_t launcher_private_interrupt_handler_seek_end_service = 0;
    std::uint8_t launcher_private_interrupt_handler_allocate_service = 0;
    std::uint8_t launcher_private_interrupt_handler_rewind_service = 0;
    std::uint8_t launcher_private_interrupt_handler_read_service = 0;
    std::uint8_t launcher_private_interrupt_handler_close_service = 0;
    // The original command-tail scanner maps e/E to selector 1 and m/M to
    // selector 2. An empty tail reaches the hardware-dependent detector;
    // neither source is a host-side driver-selection policy.
    std::uint16_t launcher_video_selection_scan_address = 0;
    std::uint16_t launcher_video_selection_default_detector_address = 0;
    std::uint16_t launcher_video_selection_map_address = 0;
    std::uint8_t launcher_private_interrupt_handler_first_selector = 0;
    std::uint16_t launcher_private_interrupt_handler_first_program_address = 0;
    std::uint8_t launcher_private_interrupt_handler_other_selector = 0;
    std::uint16_t launcher_private_interrupt_handler_other_program_address = 0;
    std::string launcher_private_interrupt_handler_first_program;
    std::string launcher_private_interrupt_handler_other_program;
    std::size_t launcher_title_offset = 0;
    std::size_t launcher_game_offset = 0;
    std::string launcher_title_program;
    std::string launcher_game_program;
};

// Validates code paths rather than trusting names or offsets alone.  The
// present evidence covers the supplied English DOS executables only; callers
// must not apply its behaviour to another release without fresh evidence.
[[nodiscard]] MillenniumDosTitleFlow parse_millennium_dos_title_flow(
    std::span<const std::uint8_t> titles_executable,
    std::span<const std::uint8_t> mill_launcher);

} // namespace eon
