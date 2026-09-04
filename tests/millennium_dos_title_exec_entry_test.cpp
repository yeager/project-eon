#include "engine/millennium_dos_title_exec_entry_session.hpp"
#include "engine/millennium_dos_title_child_compatibility_service.hpp"
#include "engine/millennium_dos_title_initialization_session.hpp"
#include "engine/millennium_dos_paragraph_arena.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {
std::vector<std::uint8_t> read(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Missing real Millennium DOS leaf");
    return {std::istreambuf_iterator<char>(stream), {}};
}
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const auto root = std::filesystem::path(argv[1]);
    std::filesystem::path mill_path;
    std::filesystem::path titles_path;
    std::filesystem::path title_library_path;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().filename() == "MILL.COM") mill_path = entry.path();
        if (entry.path().filename() == "TITLES.EXE") titles_path = entry.path();
        if (entry.path().filename() == "TITLE.LIB") title_library_path = entry.path();
        if (!mill_path.empty() && !titles_path.empty()
            && mill_path.parent_path() == titles_path.parent_path()) break;
    }
    if (mill_path.empty() || titles_path.empty() || title_library_path.empty()
        || mill_path.parent_path() != titles_path.parent_path()) return 3;
    const auto mill = read(mill_path);
    const auto titles = read(titles_path);
    const auto title_library = read(title_library_path);
    eon::MillenniumDosTitleExecEntrySession session(mill, titles);
    assert(session.state()
        == eon::MillenniumDosTitleExecEntryState::awaiting_child_process_entry);

    session.observe_child_process_entry({1, 0x0336, 0x4b00, 0x068f, 0x067a,
        0x0100, 0x2468,
        eon::MillenniumDosTitleExecEntryProvenance::eon_dos_compatibility_service});
    session.execute_exact_entry_prefix(2, 0x0100, 0x0104, 0x1b80);
    const auto checkpoint = session.checkpoint();
    assert(checkpoint.state
        == eon::MillenniumDosTitleExecEntryState::title_entry_boundary);
    assert(checkpoint.last_sequence == 2 && checkpoint.child_code_segment == 0x2468);
    assert(checkpoint.provenance
        == eon::MillenniumDosTitleExecEntryProvenance::eon_dos_compatibility_service);
    assert(checkpoint.register_effects.size() == 3);
    assert((checkpoint.register_effects[0]
        == eon::MillenniumDosTitleExecRegisterEffect{0x0101, "DS", 0x2468}));
    assert((checkpoint.register_effects[1]
        == eon::MillenniumDosTitleExecRegisterEffect{0x0103, "ES", 0x2468}));
    assert((checkpoint.register_effects[2]
        == eon::MillenniumDosTitleExecRegisterEffect{0x0104, "IP", 0x1b80}));

    bool rejected = false;
    try {
        eon::MillenniumDosTitleExecEntrySession detached(mill, titles);
        detached.observe_child_process_entry({1, 0x0337, 0x4b00, 0x068f,
            0x067a, 0x0100, 0x2468,
            eon::MillenniumDosTitleExecEntryProvenance::observed_process_entry});
    } catch (const std::runtime_error&) { rejected = true; }
    assert(rejected);

    eon::MillenniumDosParagraphArena arena(1);
    const auto allocation=arena.allocate(
        eon::MillenniumDosTitleChildCompatibilityService::required_paragraphs());
    assert(allocation.allocation&&allocation.allocation->segment==0xe100
        &&allocation.allocation->paragraph_count==455);
    eon::MillenniumDosTitleChildCompatibilityService child(
        titles,*allocation.allocation);
    const auto child_checkpoint=child.checkpoint();
    assert(child_checkpoint.exact_leaf_admitted
        &&child_checkpoint.admitted_image_byte_count==titles.size()
        &&child_checkpoint.image_load_offset==0x0100
        &&!child_checkpoint.psp_modeled
        &&!child_checkpoint.environment_modeled
        &&!child_checkpoint.parent_exec_return_observed);
    assert(child.image_effects().size()==titles.size()
        &&child.image_effects().front().offset==0x0100
        &&child.image_effects().front().value==titles.front()
        &&child.image_effects().back().offset==0x1c6d
        &&child.image_effects().back().value==titles.back());

    eon::MillenniumDosTitleInitializationSession initialization(
        titles,0x2468,2);
    initialization.execute_exact_startup(3,0x1b80,0x1b95,0x0122,0x91);
    const auto initialized=initialization.checkpoint();
    assert(initialized.state
        ==eon::MillenniumDosTitleInitializationState::private_interrupt_result_boundary
        &&initialized.last_sequence==3
        &&initialized.child_code_segment==0x2468
        &&initialized.register_effects.size()==9
        &&initialized.register_effects[3].instruction_address==0x1b86
        &&initialized.register_effects[3].value==0x2468
        &&initialized.register_effects[5].instruction_address==0x1b8b
        &&initialized.register_effects[5].value==0xda00
        &&initialized.register_effects.back().instruction_address==0x1b92
        &&initialized.register_effects.back().value==0x1ac4
        &&initialized.boundary.call_address==0x1b95
        &&initialized.boundary.wrapper_address==0x0122
        &&initialized.boundary.interrupt_address==0x0127
        &&initialized.boundary.interrupt==0x91
        &&initialized.boundary.function==0
        &&initialized.boundary.record_segment==0x2468
        &&initialized.boundary.record_offset==0x1ac4
        &&!initialized.boundary.result_observed
        &&!initialized.boundary.stack_storage_modeled);
    initialization.observe_private_interrupt_result(
        {4,0x0127,0x0129,0x0101,0x7202});
    const auto selected=initialization.checkpoint();
    assert(selected.state
        ==eon::MillenniumDosTitleInitializationState::selected_local_call_boundary
        &&selected.last_sequence==4&&selected.observed_ax==0x0101
        &&selected.observed_flags==0x7202&&selected.selected_mode==1
        &&selected.selected_call_address==0x1bad
        &&selected.selected_call_target==0x1ac6
        &&selected.boundary.result_observed
        &&selected.memory_effects.size()==4
        &&selected.memory_effects[0].offset==0x1a9c
        &&selected.memory_effects[0].width
            ==eon::MillenniumDosTitleInitializationEffectWidth::word
        &&selected.memory_effects[0].value==0x0101
        &&selected.memory_effects[1].offset==0x1aaa
        &&selected.memory_effects[1].value==1
        &&selected.memory_effects[2].offset==0x0107
        &&selected.memory_effects[2].value==1
        &&selected.memory_effects[3].offset==0x1aa0
        &&selected.memory_effects[3].value==0xda00);
    initialization.execute_selected_callee_start(5,0x1bad,0x1ac6);
    const auto mode_one_boundary=initialization.checkpoint();
    assert(mode_one_boundary.state==eon::MillenniumDosTitleInitializationState::
            selected_callee_private_interrupt_result_boundary
        &&mode_one_boundary.last_sequence==5
        &&mode_one_boundary.register_effects.size()==12
        &&mode_one_boundary.register_effects[9].instruction_address==0x1ac6
        &&mode_one_boundary.register_effects[9].register_name=="AX"
        &&mode_one_boundary.register_effects[9].value==4
        &&mode_one_boundary.register_effects[10].instruction_address==0x1ac9
        &&mode_one_boundary.register_effects[10].register_name=="ES"
        &&mode_one_boundary.register_effects[10].value==0x2468
        &&mode_one_boundary.register_effects[11].instruction_address==0x1acb
        &&mode_one_boundary.register_effects[11].register_name=="BX"
        &&mode_one_boundary.register_effects[11].value==0x1ac5
        &&mode_one_boundary.selected_callee_boundary.call_address==0x1ace
        &&mode_one_boundary.selected_callee_boundary.interrupt_address==0x0127
        &&mode_one_boundary.selected_callee_boundary.function==4
        &&mode_one_boundary.selected_callee_boundary.record_segment==0x2468
        &&mode_one_boundary.selected_callee_boundary.record_offset==0x1ac5
        &&!mode_one_boundary.selected_callee_boundary.result_observed);
    initialization.observe_selected_callee_private_interrupt_result(
        {6,0x0127,0x0129,0x1ad1,0xabcd,0x1234});
    const auto mode_one_followup=initialization.checkpoint();
    assert(mode_one_followup.state
        ==eon::MillenniumDosTitleInitializationState::selected_followup_call_boundary
        &&mode_one_followup.selected_callee_observed_ax==0xabcd
        &&mode_one_followup.selected_callee_observed_flags==0x1234
        &&mode_one_followup.selected_followup_call_address==0x1ad1
        &&mode_one_followup.selected_followup_call_target==0x044c
        &&mode_one_followup.selected_callee_boundary.result_observed);
    initialization.execute_selected_followup_start(7,0x1ad1,0x044c);
    const auto mode_one_bios=initialization.checkpoint();
    assert(mode_one_bios.state
        ==eon::MillenniumDosTitleInitializationState::bios_palette_interrupt_boundary
        &&mode_one_bios.last_sequence==7
        &&mode_one_bios.memory_effects.size()==5
        &&mode_one_bios.memory_effects.back().instruction_address==0x044e
        &&mode_one_bios.memory_effects.back().offset==0x0107
        &&mode_one_bios.memory_effects.back().value==1
        &&mode_one_bios.bios_boundary.selected_followup_call_address==0x1ad1
        &&mode_one_bios.bios_boundary.selected_followup_call_target==0x044c
        &&mode_one_bios.bios_boundary.interrupt_address==0x046d
        &&mode_one_bios.bios_boundary.interrupt==0x10
        &&mode_one_bios.bios_boundary.ax==0x1010
        &&mode_one_bios.bios_boundary.bx==0
        &&mode_one_bios.bios_boundary.cx==0
        &&mode_one_bios.bios_boundary.dx_known_mask==0xff00
        &&mode_one_bios.bios_boundary.dx_known_value==0
        &&mode_one_bios.bios_boundary.source_address==0x014c
        &&!mode_one_bios.bios_boundary.result_observed);
    for(std::uint64_t index=0;index<16;++index){
        initialization.observe_bios_palette_result(
            {8+index,0x046d,0x046f,static_cast<std::uint16_t>(0x2000+index),
                static_cast<std::uint16_t>(0x0200+index)},titles);
        const auto step=initialization.checkpoint();
        assert(step.bios_results.size()==index+1);
        if(index<15){
            assert(step.state==eon::MillenniumDosTitleInitializationState::
                    bios_palette_interrupt_boundary
                &&step.bios_boundary.source_address==0x014c+(index+1)*3
                &&step.bios_boundary.bx==index+1
                &&!step.bios_boundary.result_observed);
        }
    }
    const auto mode_one_complete=initialization.checkpoint();
    assert(mode_one_complete.state
        ==eon::MillenniumDosTitleInitializationState::title_main_allocation_call_boundary
        &&mode_one_complete.last_sequence==23
        &&mode_one_complete.bios_results.size()==16
        &&mode_one_complete.memory_effects.size()==6
        &&mode_one_complete.memory_effects.back().instruction_address==0x1ad6
        &&mode_one_complete.memory_effects.back().offset==0x0107
        &&mode_one_complete.memory_effects.back().value==1
        &&mode_one_complete.title_main_call_address==0x1bb8
        &&mode_one_complete.title_main_call_target==0x1b1f);
    initialization.execute_title_main_allocation_start(24,0x1bb8,0x1b1f);
    assert(initialization.checkpoint().state
        ==eon::MillenniumDosTitleInitializationState::dos_resize_result_boundary
        &&initialization.checkpoint().dos_boundary.interrupt_address==0x1b26
        &&initialization.checkpoint().dos_boundary.interrupt==0x21
        &&initialization.checkpoint().dos_boundary.service==0x4a
        &&initialization.checkpoint().dos_boundary.ax_known_value==0x4a01
        &&initialization.checkpoint().dos_boundary.bx==0x1000
        &&initialization.checkpoint().dos_boundary.segment==0x2468);
    initialization.observe_dos_memory_result(
        {25,0x1b26,0x1b28,true,0x1203,0x4567,0x0001});
    assert(initialization.checkpoint().state
        ==eon::MillenniumDosTitleInitializationState::
            dos_large_allocation_result_boundary
        &&initialization.checkpoint().dos_boundary.interrupt_address==0x1b2d
        &&initialization.checkpoint().dos_boundary.ax_known_value==0x4803
        &&initialization.checkpoint().dos_boundary.bx==0xfa00);
    initialization.observe_dos_memory_result(
        {26,0x1b2d,0x1b2f,false,0x3004,0x5678,0x0202});
    assert(initialization.checkpoint().dos_boundary.interrupt_address==0x1b38
        &&initialization.checkpoint().dos_boundary.service==0x49
        &&initialization.checkpoint().dos_boundary.ax_known_value==0x4904
        &&initialization.checkpoint().dos_boundary.segment==0x3004);
    initialization.observe_dos_memory_result(
        {27,0x1b38,0x1b3a,false,0x0005,0,0x0202});
    assert(initialization.checkpoint().dos_boundary.interrupt_address==0x1b3f
        &&initialization.checkpoint().dos_boundary.ax_known_value==0x4805);
    initialization.observe_dos_memory_result(
        {28,0x1b3f,0x1b41,false,0x4006,0,0x0202});
    assert(initialization.checkpoint().dos_boundary.interrupt_address==0x1b4f
        &&initialization.checkpoint().dos_boundary.bx==0x0fa1
        &&initialization.checkpoint().dos_boundary.ax_known_value==0x4806);
    initialization.observe_dos_memory_result(
        {29,0x1b4f,0x1b51,false,0x5007,0,0x0202});
    const auto allocation_success=initialization.checkpoint();
    assert(allocation_success.state
        ==eon::MillenniumDosTitleInitializationState::dos_file_open_result_boundary
        &&allocation_success.last_sequence==29
        &&allocation_success.dos_results.size()==5
        &&allocation_success.dos_boundary.interrupt_address==0x1af9
        &&allocation_success.dos_boundary.return_address==0x1afb
        &&allocation_success.dos_boundary.service==0x3d
        &&allocation_success.dos_boundary.ax_known_value==0x3d00
        &&allocation_success.dos_boundary.segment==0x2468
        &&allocation_success.dos_boundary.dx==0x0e4e
        &&allocation_success.dos_boundary.source_address==0x0e4e
        &&allocation_success.dos_boundary.source_size==10
        &&allocation_success.memory_effects.size()==9
        &&allocation_success.memory_effects[6].offset==0x1aa2
        &&allocation_success.memory_effects[6].value==0x5678
        &&allocation_success.memory_effects[7].offset==0x010e
        &&allocation_success.memory_effects[7].value==0x4006
        &&allocation_success.memory_effects[8].offset==0x0112
        &&allocation_success.memory_effects[8].value==0x5007);
    auto open_failure=initialization;
    open_failure.observe_dos_file_result(
        {30,0x1af9,0x1afb,true,2,0,0,0,0x0001});
    assert(open_failure.checkpoint().state
        ==eon::MillenniumDosTitleInitializationState::dos_file_failure_boundary
        &&open_failure.checkpoint().failure_address==0x05a3
        &&open_failure.checkpoint().dos_file_results.size()==1);
    initialization.observe_dos_file_result(
        {30,0x1af9,0x1afb,false,0x0042,0xabcd,0x1234,0x5678,0x0202});
    const auto seek_boundary=initialization.checkpoint();
    assert(seek_boundary.state
        ==eon::MillenniumDosTitleInitializationState::dos_file_seek_result_boundary
        &&seek_boundary.dos_boundary.interrupt_address==0x1b09
        &&seek_boundary.dos_boundary.return_address==0x1b0b
        &&seek_boundary.dos_boundary.service==0x42
        &&seek_boundary.dos_boundary.ax_known_value==0x4202
        &&seek_boundary.dos_boundary.bx==0x0042
        &&seek_boundary.dos_boundary.cx==0
        &&seek_boundary.dos_boundary.dx==0);
    auto seek_failure=initialization;
    seek_failure.observe_dos_file_result(
        {31,0x1b09,0x1b0b,true,0x0005,0x0042,0,0,0x0001});
    assert(seek_failure.checkpoint().state
        ==eon::MillenniumDosTitleInitializationState::dos_file_failure_boundary
        &&seek_failure.checkpoint().failure_address==0x05a3);
    initialization.observe_dos_file_result(
        {31,0x1b09,0x1b0b,false,0x49db,0x0042,0,0,0x0202});
    const auto close_boundary=initialization.checkpoint();
    assert(close_boundary.state
        ==eon::MillenniumDosTitleInitializationState::dos_file_close_result_boundary
        &&close_boundary.dos_boundary.interrupt_address==0x1b12
        &&close_boundary.dos_boundary.return_address==0x1b14
        &&close_boundary.dos_boundary.service==0x3e
        &&close_boundary.dos_boundary.ax_known_value==0x3e00
        &&close_boundary.dos_boundary.bx==0x0042
        &&close_boundary.dos_boundary.cx==0
        &&close_boundary.dos_boundary.dx==0);
    initialization.observe_dos_file_result(
        {32,0x1b12,0x1b14,true,0x0006,0x0042,0,0xbeef,0x0001});
    const auto sized_allocation=initialization.checkpoint();
    assert(sized_allocation.state
        ==eon::MillenniumDosTitleInitializationState::
            dos_file_sized_allocation_result_boundary
        &&sized_allocation.last_sequence==32
        &&sized_allocation.dos_file_results.size()==3
        &&sized_allocation.dos_file_results[0].ax==0x0042
        &&sized_allocation.dos_file_results[1].ax==0x49db
        &&sized_allocation.dos_file_results[1].dx==0
        &&sized_allocation.dos_file_results[2].carry
        &&sized_allocation.dos_boundary.interrupt_address==0x1b64
        &&sized_allocation.dos_boundary.return_address==0x1b66
        &&sized_allocation.dos_boundary.service==0x48
        &&sized_allocation.dos_boundary.ax_known_value==0x489e
        &&sized_allocation.dos_boundary.bx==0x049e
        &&sized_allocation.dos_boundary.cx==0x0004
        &&sized_allocation.dos_boundary.dx==0xbeef
        &&sized_allocation.memory_effects.size()==9);
    initialization.observe_dos_memory_result(
        {33,0x1b64,0x1b66,false,0x3000,0,0x0202});
    assert(initialization.checkpoint().state
        ==eon::MillenniumDosTitleInitializationState::
            dos_single_paragraph_allocation_result_boundary
        &&initialization.checkpoint().dos_boundary.interrupt_address==0x1b74
        &&initialization.checkpoint().dos_boundary.bx==1);
    initialization.observe_dos_memory_result(
        {34,0x1b74,0x1b76,true,0x4000,0,0x0001});
    initialization.observe_dos_memory_result(
        {35,0x1bca,0x1bcc,true,0x5000,0x6000,0x0001});
    initialization.observe_dos_memory_result(
        {36,0x1bd5,0x1bd7,true,0x0007,0,0x0001});
    const auto library_open=initialization.checkpoint();
    assert(library_open.state
        ==eon::MillenniumDosTitleInitializationState::dos_library_open_result_boundary
        &&library_open.dos_boundary.interrupt_address==0x0549
        &&library_open.dos_boundary.service==0x3d
        &&library_open.dos_boundary.ax_known_value==0x3d02
        &&library_open.dos_boundary.source_address==0x0e4e);
    initialization.observe_dos_file_result(
        {37,0x0549,0x054b,false,0x0055,0,0,0,0x0202},title_library);
    assert(initialization.checkpoint().state
        ==eon::MillenniumDosTitleInitializationState::dos_library_read_result_boundary
        &&initialization.checkpoint().dos_boundary.interrupt_address==0x057c
        &&initialization.checkpoint().dos_boundary.service==0x3f
        &&initialization.checkpoint().dos_boundary.bx==0x0055
        &&initialization.checkpoint().dos_boundary.cx==0x8000
        &&initialization.checkpoint().dos_boundary.segment==0x3000);
    bool oversized_library_read_rejected=false;
    try {
        initialization.observe_dos_file_result(
            {38,0x057c,0x057e,false,0x49dc,0x0055,0x8000,0,0x0202},
            title_library);
    } catch(const std::runtime_error&) { oversized_library_read_rejected=true; }
    assert(oversized_library_read_rejected
        &&initialization.checkpoint().last_sequence==37);
    initialization.observe_dos_file_result(
        {38,0x057c,0x057e,false,0x49db,0x0055,0x8000,0,0x0202},
        title_library);
    for(std::uint64_t sequence=39;sequence<=46;++sequence){
        initialization.observe_dos_file_result(
            {sequence,0x057c,0x057e,false,0,0x0055,0,0,0x0202},
            title_library);
    }
    assert(initialization.checkpoint().state
        ==eon::MillenniumDosTitleInitializationState::dos_library_close_result_boundary
        &&initialization.checkpoint().dos_boundary.interrupt_address==0x059e);
    initialization.observe_dos_file_result(
        {47,0x059e,0x05a0,true,6,0x0055,0,0,0x0001},title_library);
    const auto relocation=initialization.checkpoint();
    assert(relocation.state
        ==eon::MillenniumDosTitleInitializationState::library_relocation_complete
        &&relocation.last_sequence==47
        &&relocation.continuation_address==0x0f6b
        &&relocation.dos_file_results.size()==14
        &&relocation.memory_effects.size()==18924
        &&relocation.memory_effects[14].segment==0x3000
        &&relocation.memory_effects[14].offset==0
        &&relocation.memory_effects[14].value==0x26
        &&relocation.memory_effects[18921].offset==0x0e5d
        &&relocation.memory_effects[18921].value==0x0026
        &&relocation.memory_effects[18922].offset==0x0e4a
        &&relocation.memory_effects[18922].value==3
        &&relocation.memory_effects[18923].offset==0x0e4c
        &&relocation.memory_effects[18923].value==0x3481);
    initialization.execute_post_relocation(48,title_library);
    const auto palette=initialization.checkpoint();
    assert(palette.state==eon::MillenniumDosTitleInitializationState::
            library_palette_copy_boundary
        &&palette.last_sequence==48&&palette.continuation_address==0x0fc6
        &&palette.memory_effects.size()==19694
        &&palette.memory_effects[18924].offset==0x0e59
        &&palette.memory_effects[18924].value==6
        &&palette.memory_effects[18925].offset==0x0e5b
        &&palette.memory_effects[18925].value==0x3000
        &&palette.memory_effects[18926].offset==0x014c
        &&palette.memory_effects[18926].value==0);
    eon::MillenniumDosTitleInitializationSession other_mode(titles,0x2468,2);
    other_mode.execute_exact_startup(3,0x1b80,0x1b95,0x0122,0x91);
    other_mode.observe_private_interrupt_result({4,0x0127,0x0129,0x02ff,0});
    assert(other_mode.checkpoint().selected_mode==2
        &&other_mode.checkpoint().selected_call_address==0x1bb2
        &&other_mode.checkpoint().selected_call_target==0x1ada);
    other_mode.execute_selected_callee_start(5,0x1bb2,0x1ada);
    assert(other_mode.checkpoint().state==eon::MillenniumDosTitleInitializationState::
            selected_callee_private_interrupt_result_boundary
        &&other_mode.checkpoint().selected_callee_boundary.call_address==0x1ae2
        &&other_mode.checkpoint().selected_callee_boundary.function==4
        &&other_mode.checkpoint().selected_callee_boundary.record_offset==0x1ac5);
    other_mode.observe_selected_callee_private_interrupt_result(
        {6,0x0127,0x0129,0x1ae5,0x9876,0x0202});
    other_mode.execute_selected_followup_start(7,0x1ae5,0x0487);
    const auto other_bios=other_mode.checkpoint();
    assert(other_bios.state
        ==eon::MillenniumDosTitleInitializationState::bios_palette_interrupt_boundary
        &&other_bios.memory_effects.size()==4
        &&other_bios.selected_callee_observed_ax==0x9876
        &&other_bios.selected_callee_observed_flags==0x0202
        &&other_bios.bios_boundary.selected_followup_call_address==0x1ae5
        &&other_bios.bios_boundary.selected_followup_call_target==0x0487
        &&other_bios.bios_boundary.interrupt_address==0x0497
        &&other_bios.bios_boundary.ax==0x1000
        &&other_bios.bios_boundary.bx==0
        &&other_bios.bios_boundary.cx==0x0010
        &&other_bios.bios_boundary.dx_known_mask==0
        &&other_bios.bios_boundary.source_address==0x0477);
    for(std::uint64_t index=0;index<16;++index){
        other_mode.observe_bios_palette_result(
            {8+index,0x0497,0x0499,static_cast<std::uint16_t>(0x3000+index),
                static_cast<std::uint16_t>(0x0300+index)},titles);
        const auto step=other_mode.checkpoint();
        assert(step.bios_results.size()==index+1);
        if(index<15){
            assert(step.state==eon::MillenniumDosTitleInitializationState::
                    bios_palette_interrupt_boundary
                &&step.bios_boundary.source_address==0x0477+index+1
                &&step.bios_boundary.cx==15-index
                &&!step.bios_boundary.result_observed);
        }
    }
    const auto other_complete=other_mode.checkpoint();
    assert(other_complete.state
        ==eon::MillenniumDosTitleInitializationState::title_main_allocation_call_boundary
        &&other_complete.last_sequence==23
        &&other_complete.bios_results.size()==16
        &&other_complete.memory_effects.size()==5
        &&other_complete.memory_effects.back().instruction_address==0x1af2
        &&other_complete.memory_effects.back().offset==0x010a
        &&other_complete.memory_effects.back().width
            ==eon::MillenniumDosTitleInitializationEffectWidth::word
        &&other_complete.memory_effects.back().value==0xb800
        &&other_complete.title_main_call_address==0x1bb8
        &&other_complete.title_main_call_target==0x1b1f);
    other_mode.execute_title_main_allocation_start(24,0x1bb8,0x1b1f);
    assert(other_mode.checkpoint().dos_boundary.ax_known_value==0x4a00);
    bool inconsistent_carry_rejected=false;
    try {
        other_mode.observe_dos_memory_result(
            {25,0x1b26,0x1b28,true,0,0,0});
    } catch(const std::runtime_error&) { inconsistent_carry_rejected=true; }
    assert(inconsistent_carry_rejected&&other_mode.checkpoint().last_sequence==24
        &&other_mode.checkpoint().dos_results.empty());
    other_mode.observe_dos_memory_result({25,0x1b26,0x1b28,false,0,0,0});
    other_mode.observe_dos_memory_result({26,0x1b2d,0x1b2f,false,0x6000,0x7000,0});
    other_mode.observe_dos_memory_result({27,0x1b38,0x1b3a,false,0,0,0});
    auto other_success=other_mode;
    other_success.observe_dos_memory_result({28,0x1b3f,0x1b41,false,0x4000,0,0});
    other_success.observe_dos_memory_result({29,0x1b4f,0x1b51,false,0x5000,0,0});
    other_success.observe_dos_file_result({30,0x1af9,0x1afb,false,0x42,0,0,0,0});
    other_success.observe_dos_file_result({31,0x1b09,0x1b0b,false,0x49db,0,0,0,0});
    other_success.observe_dos_file_result({32,0x1b12,0x1b14,false,0,0,0,0,0});
    other_success.observe_dos_memory_result({33,0x1b64,0x1b66,false,0x3000,0,0});
    other_success.observe_dos_memory_result({34,0x1b74,0x1b76,false,0x4000,0,0});
    other_success.observe_dos_memory_result({35,0x1bca,0x1bcc,false,0x5000,0x6000,0});
    other_success.observe_dos_memory_result({36,0x1bd5,0x1bd7,false,0,0,0});
    other_success.observe_dos_file_result({37,0x0549,0x054b,false,0x55,0,0,0,0},title_library);
    other_success.observe_dos_file_result({38,0x057c,0x057e,false,0x49db,0,0,0,0},title_library);
    for(std::uint64_t sequence=39;sequence<=46;++sequence)
        other_success.observe_dos_file_result({sequence,0x057c,0x057e,false,0,0,0,0,0},title_library);
    other_success.observe_dos_file_result({47,0x059e,0x05a0,false,0,0,0,0,0},title_library);
    other_success.execute_post_relocation(48,title_library);
    assert(other_success.checkpoint().state==eon::MillenniumDosTitleInitializationState::
            post_library_setup_call_boundary
        &&other_success.checkpoint().continuation_address==0x1bef
        &&other_success.checkpoint().memory_effects.size()==18923);
    other_success.execute_post_library_setup(49,0x1bef,0x1aac);
    assert(other_success.checkpoint().state==eon::MillenniumDosTitleInitializationState::
            dos_get_vector_zero_result_boundary
        &&other_success.checkpoint().continuation_address==0x10f4
        &&other_success.checkpoint().dos_boundary.interrupt_address==0x10f4
        &&other_success.checkpoint().dos_boundary.service==0x35
        &&other_success.checkpoint().dos_boundary.ax_known_value==0x3500);
    other_success.observe_dos_vector_result({50,0x10f4,0x10f6,0x3500,0x1234,0xabcd,0x0246});
    assert(other_success.checkpoint().state==eon::MillenniumDosTitleInitializationState::dos_set_vector_zero_result_boundary
        &&other_success.checkpoint().dos_vector_results.size()==1
        &&other_success.checkpoint().memory_effects[18923].offset==0x10e4
        &&other_success.checkpoint().memory_effects[18923].value==0x1234
        &&other_success.checkpoint().memory_effects[18924].offset==0x10e6
        &&other_success.checkpoint().memory_effects[18924].value==0xabcd
        &&other_success.checkpoint().dos_boundary.interrupt_address==0x1106
        &&other_success.checkpoint().dos_boundary.service==0x25
        &&other_success.checkpoint().dos_boundary.dx==0x1124);
    const auto vector_write_effect_count=other_success.checkpoint().memory_effects.size();
    other_success.observe_dos_vector_result({51,0x1106,0x1108,0x9999,0x2222,0x3333,0x0247});
    assert(other_success.checkpoint().state==eon::MillenniumDosTitleInitializationState::dos_get_vector_four_result_boundary
        &&other_success.checkpoint().dos_vector_results.size()==2
        &&other_success.checkpoint().dos_vector_results.back().flags==0x0247
        &&other_success.checkpoint().memory_effects.size()==vector_write_effect_count
        &&other_success.checkpoint().dos_boundary.interrupt_address==0x110b
        &&other_success.checkpoint().dos_boundary.ax_known_value==0x3504);
    other_success.observe_dos_vector_result({52,0x110b,0x110d,0x3504,0x5678,0xcdef,0x0202});
    assert(other_success.checkpoint().state==eon::MillenniumDosTitleInitializationState::dos_set_vector_four_result_boundary
        &&other_success.checkpoint().dos_vector_results.size()==3
        &&other_success.checkpoint().memory_effects[18925].offset==0x10e8
        &&other_success.checkpoint().memory_effects[18925].value==0x5678
        &&other_success.checkpoint().memory_effects[18926].offset==0x10ea
        &&other_success.checkpoint().memory_effects[18926].value==0xcdef
        &&other_success.checkpoint().dos_boundary.interrupt_address==0x111d
        &&other_success.checkpoint().dos_boundary.ax_known_value==0x2504
        &&other_success.checkpoint().dos_boundary.dx==0x1124);
    const auto saved_vector_effect_count=other_success.checkpoint().memory_effects.size();
    other_success.observe_dos_vector_result({53,0x111d,0x111f,0x7777,0x9a9b,0x8888,0x0246});
    assert(other_success.checkpoint().state==eon::MillenniumDosTitleInitializationState::bios_int15_first_result_boundary
        &&other_success.checkpoint().dos_vector_results.size()==4
        &&other_success.checkpoint().dos_vector_results.back().ax==0x7777
        &&other_success.checkpoint().dos_vector_results.back().flags==0x0246
        &&other_success.checkpoint().memory_effects.size()==saved_vector_effect_count
        &&other_success.checkpoint().setup_bios_boundary.interrupt_address==0x1ab9
        &&other_success.checkpoint().setup_bios_boundary.interrupt==0x15
        &&other_success.checkpoint().setup_bios_boundary.ax_known_mask==0xffff
        &&other_success.checkpoint().setup_bios_boundary.ax_known_value==0x011b
        &&other_success.checkpoint().setup_bios_boundary.bx_known_mask==0xffff
        &&other_success.checkpoint().setup_bios_boundary.bx_known_value==0x9a46);
    other_success.observe_setup_bios_result({54,0x1ab9,0x1abb,0xdead,0x7654,0x0247});
    assert(other_success.checkpoint().state==eon::MillenniumDosTitleInitializationState::bios_int15_second_result_boundary
        &&other_success.checkpoint().setup_bios_results.size()==1
        &&other_success.checkpoint().setup_bios_results[0].ax==0xdead
        &&other_success.checkpoint().setup_bios_results[0].flags==0x0247
        &&other_success.checkpoint().setup_bios_boundary.interrupt_address==0x1ac1
        &&other_success.checkpoint().setup_bios_boundary.ax_known_value==0x011c
        &&other_success.checkpoint().setup_bios_boundary.bx_known_value==0x7646);
    other_success.observe_setup_bios_result({55,0x1ac1,0x1ac3,0xbeef,0x1357,0x0203});
    assert(other_success.checkpoint().state==eon::MillenniumDosTitleInitializationState::post_library_next_setup_call_boundary
        &&other_success.checkpoint().last_sequence==55
        &&other_success.checkpoint().continuation_address==0x1bf2
        &&other_success.checkpoint().setup_bios_results.size()==2
        &&other_success.checkpoint().setup_bios_results.back().ax==0xbeef
        &&other_success.checkpoint().setup_bios_results.back().bx==0x1357
        &&other_success.checkpoint().setup_bios_results.back().flags==0x0203);
    other_success.execute_next_setup(56,0x1bf2,0x11a7);
    assert(other_success.checkpoint().state==eon::MillenniumDosTitleInitializationState::post_library_followup_call_boundary
        &&other_success.checkpoint().continuation_address==0x1bf5
        &&other_success.checkpoint().memory_effects[18927].offset==0x118d
        &&other_success.checkpoint().memory_effects[18928].offset==0x1181
        &&other_success.checkpoint().memory_effects[18929].value==0x0444
        &&other_success.checkpoint().memory_effects[18930].value==0x1178);
    other_success.execute_followup_setup(57,0x1bf5,0x114e);
    assert(other_success.checkpoint().state==eon::MillenniumDosTitleInitializationState::timer_vector_far_read_boundary
        &&other_success.checkpoint().continuation_address==0x115d
        &&other_success.checkpoint().far_read_boundary.instruction_address==0x115d
        &&other_success.checkpoint().far_read_boundary.source_segment==0
        &&other_success.checkpoint().far_read_boundary.source_offset==0x0070
        &&other_success.checkpoint().far_read_boundary.word_count==2
        &&other_success.checkpoint().far_read_boundary.destination_segment==0x2468
        &&other_success.checkpoint().far_read_boundary.destination_offset==0x10dc);
    other_success.observe_far_words({58,0x115d,0,0x0070,0xaaaa,0xbbbb});
    assert(other_success.checkpoint().state==eon::MillenniumDosTitleInitializationState::post_vector_hook_call_boundary
        &&other_success.checkpoint().continuation_address==0x1bf8
        &&other_success.checkpoint().far_word_observations.size()==1
        &&other_success.checkpoint().memory_effects[18931].value==0xaaaa
        &&other_success.checkpoint().memory_effects[18932].value==0xbbbb
        &&other_success.checkpoint().memory_effects[18933].explicit_segment
        &&other_success.checkpoint().memory_effects[18933].segment==0
        &&other_success.checkpoint().memory_effects[18933].offset==0x0070
        &&other_success.checkpoint().memory_effects[18933].value==0x11d8
        &&other_success.checkpoint().memory_effects[18934].value==0x2468
        &&other_success.checkpoint().memory_effects[18935].offset==0x112c
        &&other_success.checkpoint().memory_effects[18935].value==1);
    other_success.execute_video_hook_setup(59,0x1bf8,0x12a0);
    assert(other_success.checkpoint().state==eon::MillenniumDosTitleInitializationState::video_vector_far_read_boundary
        &&other_success.checkpoint().continuation_address==0x12ad
        &&other_success.checkpoint().far_read_boundary.instruction_address==0x12ad
        &&other_success.checkpoint().far_read_boundary.source_segment==0
        &&other_success.checkpoint().far_read_boundary.source_offset==0x0024
        &&other_success.checkpoint().far_read_boundary.word_count==2
        &&other_success.checkpoint().far_read_boundary.destination_offset==0x1266);
    other_success.observe_far_words({60,0x12ad,0,0x0024,0xcccc,0xdddd});
    assert(other_success.checkpoint().state==eon::MillenniumDosTitleInitializationState::post_video_hook_mode_call_boundary
        &&other_success.checkpoint().continuation_address==0x1c02
        &&other_success.checkpoint().far_word_observations.size()==2
        &&other_success.checkpoint().memory_effects[18936].offset==0x1266
        &&other_success.checkpoint().memory_effects[18936].value==0xcccc
        &&other_success.checkpoint().memory_effects[18937].offset==0x1268
        &&other_success.checkpoint().memory_effects[18937].value==0xdddd
        &&other_success.checkpoint().memory_effects[18938].explicit_segment
        &&other_success.checkpoint().memory_effects[18938].segment==0
        &&other_success.checkpoint().memory_effects[18938].offset==0x0024
        &&other_success.checkpoint().memory_effects[18938].value==0x126a
        &&other_success.checkpoint().memory_effects[18939].explicit_segment
        &&other_success.checkpoint().memory_effects[18939].offset==0x0026
        &&other_success.checkpoint().memory_effects[18939].value==0x2468);
    other_success.execute_post_video_mode_call(61,0x1c02,0x1ada);
    assert(other_success.checkpoint().state==eon::MillenniumDosTitleInitializationState::selected_callee_private_interrupt_result_boundary
        &&other_success.checkpoint().continuation_address==0x0127
        &&other_success.checkpoint().selected_callee_boundary.call_address==0x1ae2);
    other_success.observe_selected_callee_private_interrupt_result(
        {62,0x0127,0x0129,0x1ae5,0x4444,0x0202});
    other_success.execute_selected_followup_start(63,0x1ae5,0x0487);
    for(std::uint64_t sequence=64;sequence<80;++sequence)
        other_success.observe_bios_palette_result(
            {sequence,0x0497,0x0499,0x1000,0x0202},titles);
    assert(other_success.checkpoint().state==eon::MillenniumDosTitleInitializationState::post_video_setup_call_boundary
        &&other_success.checkpoint().last_sequence==79
        &&other_success.checkpoint().continuation_address==0x1c0e
        &&other_success.checkpoint().memory_effects.size()==18941
        &&other_success.checkpoint().memory_effects.back().offset==0x010a
        &&other_success.checkpoint().memory_effects.back().value==0xb800);
    other_success.execute_post_video_setup(80,0x1c0e,0x135e);
    assert(other_success.checkpoint().state==eon::MillenniumDosTitleInitializationState::post_video_graphics_call_boundary
        &&other_success.checkpoint().continuation_address==0x1c11
        &&other_success.checkpoint().memory_effects.size()==18944
        &&other_success.checkpoint().memory_effects[18941].offset==0x1341
        &&other_success.checkpoint().memory_effects[18941].value==0
        &&other_success.checkpoint().memory_effects[18942].offset==0x1343
        &&other_success.checkpoint().memory_effects[18942].value==0x5000
        &&other_success.checkpoint().memory_effects[18943].offset==0x134b
        &&other_success.checkpoint().memory_effects[18943].value==0x2468);
    bool detached_post_video_setup_rejected=false;
    try {
        other_success.execute_post_video_setup(81,0x1c0e,0x135f);
    } catch(const std::runtime_error&) { detached_post_video_setup_rejected=true; }
    assert(detached_post_video_setup_rejected
        &&other_success.checkpoint().memory_effects.size()==18944);
    auto detached_graphics=other_success;
    bool detached_graphics_rejected=false;
    try {
        detached_graphics.execute_post_video_graphics_call(81,0x1c11,0x0ff4);
    } catch(const std::runtime_error&) { detached_graphics_rejected=true; }
    assert(detached_graphics_rejected
        &&detached_graphics.checkpoint().memory_effects.size()==18944);
    other_success.execute_post_video_graphics_call(81,0x1c11,0x0ff3);
    assert(other_success.checkpoint().state==eon::MillenniumDosTitleInitializationState::post_video_private_interrupt_result_boundary
        &&other_success.checkpoint().continuation_address==0x0127
        &&other_success.checkpoint().boundary.call_address==0x1000
        &&other_success.checkpoint().boundary.function==0x0019
        &&other_success.checkpoint().boundary.record_offset==0x0fe9
        &&other_success.checkpoint().memory_effects.size()==18945
        &&other_success.checkpoint().memory_effects.back().offset==0x0ff1
        &&other_success.checkpoint().memory_effects.back().value==0x2468);
    other_success.observe_private_interrupt_result({82,0x0127,0x0129,0xabcd,0x0246});
    assert(other_success.checkpoint().state==eon::MillenniumDosTitleInitializationState::post_video_followup_call_boundary
        &&other_success.checkpoint().continuation_address==0x1c17
        &&other_success.checkpoint().post_video_observed_ax==0xabcd
        &&other_success.checkpoint().post_video_observed_flags==0x0246
        &&other_success.checkpoint().observed_ax==0x02ff);
    other_success.execute_post_video_followup(83,0x1c17,0x1725);
    assert(other_success.checkpoint().state==eon::MillenniumDosTitleInitializationState::graphics_descriptor_far_read_boundary
        &&other_success.checkpoint().continuation_address==0x13aa
        &&other_success.checkpoint().far_read_boundary.instruction_address==0x13aa
        &&other_success.checkpoint().far_read_boundary.word_count==2
        &&other_success.checkpoint().far_read_boundary.source_segment==0x3481
        &&other_success.checkpoint().far_read_boundary.source_offset==0x0003
        &&other_success.checkpoint().far_read_boundary.destination_offset==0x138c);
    auto contradictory_descriptor=other_success;
    bool contradictory_descriptor_rejected=false;
    try {
        contradictory_descriptor.observe_far_words({84,0x13aa,0x3481,0x0003,7,0});
    } catch(const std::runtime_error&) { contradictory_descriptor_rejected=true; }
    assert(contradictory_descriptor_rejected
        &&contradictory_descriptor.checkpoint().far_word_observations.size()==2
        &&contradictory_descriptor.checkpoint().memory_effects.size()==18945);
    other_success.observe_far_words({84,0x13aa,0x3481,0x0003,6,0});
    assert(other_success.checkpoint().state==eon::MillenniumDosTitleInitializationState::graphics_record_word_read_boundary
        &&other_success.checkpoint().continuation_address==0x13cd
        &&other_success.checkpoint().far_read_boundary.source_segment==0x3000
        &&other_success.checkpoint().far_read_boundary.source_offset==0x001e
        &&other_success.checkpoint().far_read_boundary.word_count==1
        &&other_success.checkpoint().memory_effects.size()==18947
        &&other_success.checkpoint().memory_effects[18945].offset==0x138c
        &&other_success.checkpoint().memory_effects[18945].value==6
        &&other_success.checkpoint().memory_effects[18946].offset==0x138e
        &&other_success.checkpoint().memory_effects[18946].value==0x3000);
    auto contradictory_record=other_success;
    bool contradictory_record_rejected=false;
    try { contradictory_record.observe_far_word({85,0x13cd,0x3000,0x001e,0x0141}); }
    catch(const std::runtime_error&) { contradictory_record_rejected=true; }
    assert(contradictory_record_rejected
        &&contradictory_record.checkpoint().far_single_word_observations.empty());
    other_success.observe_far_word({85,0x13cd,0x3000,0x001e,0x0140});
    assert(other_success.checkpoint().state==eon::MillenniumDosTitleInitializationState::graphics_record_second_word_read_boundary
        &&other_success.checkpoint().continuation_address==0x13d0
        &&other_success.checkpoint().far_single_word_observations.size()==1
        &&other_success.checkpoint().far_single_word_observations[0].word==0x0140
        &&other_success.checkpoint().far_read_boundary.source_segment==0x3000
        &&other_success.checkpoint().far_read_boundary.source_offset==0x001c
        &&other_success.checkpoint().far_read_boundary.word_count==1);
    auto contradictory_second_record=other_success;
    bool contradictory_second_record_rejected=false;
    try { contradictory_second_record.observe_far_word({86,0x13d0,0x3000,0x001c,0x00c9}); }
    catch(const std::runtime_error&) { contradictory_second_record_rejected=true; }
    assert(contradictory_second_record_rejected
        &&contradictory_second_record.checkpoint().far_single_word_observations.size()==1);
    other_success.observe_far_word({86,0x13d0,0x3000,0x001c,0x00c8});
    assert(other_success.checkpoint().state==eon::MillenniumDosTitleInitializationState::graphics_record_third_word_read_boundary
        &&other_success.checkpoint().continuation_address==0x13e2
        &&other_success.checkpoint().far_single_word_observations.size()==2
        &&other_success.checkpoint().far_read_boundary.source_offset==0x001a
        &&other_success.checkpoint().memory_effects.size()==18950
        &&other_success.checkpoint().memory_effects[18947].offset==0x1357
        &&other_success.checkpoint().memory_effects[18947].value==0x00c8
        &&other_success.checkpoint().memory_effects[18948].value==0x0140
        &&other_success.checkpoint().memory_effects[18949].value==0xfa00);
    auto contradictory_third_record=other_success;
    bool contradictory_third_record_rejected=false;
    try { contradictory_third_record.observe_far_word({87,0x13e2,0x3000,0x001a,1}); }
    catch(const std::runtime_error&) { contradictory_third_record_rejected=true; }
    assert(contradictory_third_record_rejected
        &&contradictory_third_record.checkpoint().far_single_word_observations.size()==2);
    other_success.observe_far_word({87,0x13e2,0x3000,0x001a,0});
    assert(other_success.checkpoint().state==eon::MillenniumDosTitleInitializationState::graphics_record_byte_read_boundary
        &&other_success.checkpoint().continuation_address==0x13e9
        &&other_success.checkpoint().far_single_word_observations.size()==3
        &&other_success.checkpoint().far_byte_boundary.instruction_address==0x13e9
        &&other_success.checkpoint().far_byte_boundary.source_segment==0x3000
        &&other_success.checkpoint().far_byte_boundary.source_offset==0x0007
        &&other_success.checkpoint().far_byte_boundary.destination_offset==0x1389
        &&other_success.checkpoint().memory_effects.size()==18951
        &&other_success.checkpoint().memory_effects.back().offset==0x138a
        &&other_success.checkpoint().memory_effects.back().value==0xfa00);
    auto contradictory_first_byte=other_success;
    bool contradictory_first_byte_rejected=false;
    try { contradictory_first_byte.observe_far_byte({88,0x13e9,0x3000,0x0007,0x22}); }
    catch(const std::runtime_error&) { contradictory_first_byte_rejected=true; }
    assert(contradictory_first_byte_rejected
        &&contradictory_first_byte.checkpoint().far_byte_observations.empty()
        &&contradictory_first_byte.checkpoint().memory_effects.size()==18951);
    other_success.observe_far_byte({88,0x13e9,0x3000,0x0007,0x23});
    assert(other_success.checkpoint().state==eon::MillenniumDosTitleInitializationState::graphics_record_second_byte_read_boundary
        &&other_success.checkpoint().continuation_address==0x13f2
        &&other_success.checkpoint().far_byte_observations.size()==1
        &&other_success.checkpoint().far_byte_observations[0].byte==0x23
        &&other_success.checkpoint().far_byte_boundary.instruction_address==0x13f2
        &&other_success.checkpoint().far_byte_boundary.source_segment==0x3000
        &&other_success.checkpoint().far_byte_boundary.source_offset==0x000a
        &&other_success.checkpoint().memory_effects.size()==18952
        &&other_success.checkpoint().memory_effects.back().offset==0x1389
        &&other_success.checkpoint().memory_effects.back().value==0x24);
    auto contradictory_second_byte=other_success;
    bool contradictory_second_byte_rejected=false;
    try { contradictory_second_byte.observe_far_byte({89,0x13f2,0x3000,0x000a,1}); }
    catch(const std::runtime_error&) { contradictory_second_byte_rejected=true; }
    assert(contradictory_second_byte_rejected
        &&contradictory_second_byte.checkpoint().far_byte_observations.size()==1
        &&contradictory_second_byte.checkpoint().memory_effects.size()==18952);
    other_success.observe_far_byte({89,0x13f2,0x3000,0x000a,0});
    assert(other_success.checkpoint().state==eon::MillenniumDosTitleInitializationState::graphics_record_private_interrupt_result_boundary
        &&other_success.checkpoint().continuation_address==0x0127
        &&other_success.checkpoint().far_byte_observations.size()==2
        &&other_success.checkpoint().boundary.call_address==0x1764
        &&other_success.checkpoint().boundary.function==6
        &&other_success.checkpoint().boundary.record_offset==0x1349
        &&other_success.checkpoint().memory_effects.size()==18957
        &&other_success.checkpoint().memory_effects[18952].offset==0x1388
        &&other_success.checkpoint().memory_effects[18953].offset==0x1351
        &&other_success.checkpoint().memory_effects[18955].value==0x00c8
        &&other_success.checkpoint().memory_effects[18956].value==0x0140);
    other_mode.observe_dos_memory_result({28,0x1b3f,0x1b41,true,0x8000,0,1});
    const auto allocation_failure=other_mode.checkpoint();
    assert(allocation_failure.state
        ==eon::MillenniumDosTitleInitializationState::allocation_failure_boundary
        &&allocation_failure.failure_address==0x1c6a
        &&allocation_failure.dos_results.size()==4
        &&allocation_failure.memory_effects.size()==8
        &&allocation_failure.memory_effects[5].offset==0x1aa2
        &&allocation_failure.memory_effects[5].value==0x7000
        &&allocation_failure.memory_effects[6].offset==0x010e
        &&allocation_failure.memory_effects[6].value==0x8000
        &&allocation_failure.memory_effects[7].offset==0x1a9c
        &&allocation_failure.memory_effects[7].value==0x8000);
    bool detached_initialization_rejected=false;
    try {
        eon::MillenniumDosTitleInitializationSession detached(titles,0x2468,2);
        detached.execute_exact_startup(4,0x1b80,0x1b95,0x0122,0x91);
    } catch(const std::runtime_error&) { detached_initialization_rejected=true; }
    assert(detached_initialization_rejected);
    bool detached_result_rejected=false;
    try {
        eon::MillenniumDosTitleInitializationSession detached(titles,0x2468,2);
        detached.execute_exact_startup(3,0x1b80,0x1b95,0x0122,0x91);
        detached.observe_private_interrupt_result({4,0x0128,0x0129,0,0});
    } catch(const std::runtime_error&) { detached_result_rejected=true; }
    assert(detached_result_rejected);
    bool detached_callee_rejected=false;
    try {
        eon::MillenniumDosTitleInitializationSession detached(titles,0x2468,2);
        detached.execute_exact_startup(3,0x1b80,0x1b95,0x0122,0x91);
        detached.observe_private_interrupt_result({4,0x0127,0x0129,0x0101,0});
        detached.execute_selected_callee_start(5,0x1bb2,0x1ada);
    } catch(const std::runtime_error&) { detached_callee_rejected=true; }
    assert(detached_callee_rejected);
    bool detached_second_result_rejected=false;
    try {
        eon::MillenniumDosTitleInitializationSession detached(titles,0x2468,2);
        detached.execute_exact_startup(3,0x1b80,0x1b95,0x0122,0x91);
        detached.observe_private_interrupt_result({4,0x0127,0x0129,0x0101,0});
        detached.execute_selected_callee_start(5,0x1bad,0x1ac6);
        detached.observe_selected_callee_private_interrupt_result(
            {6,0x0127,0x0129,0x1ae5,0,0});
    } catch(const std::runtime_error&) { detached_second_result_rejected=true; }
    assert(detached_second_result_rejected);
    bool duplicate_bios_rejected=false;
    try {
        initialization.observe_bios_palette_result(
            {24,0x046d,0x046f,0,0},titles);
    } catch(const std::runtime_error&) { duplicate_bios_rejected=true; }
    assert(duplicate_bios_rejected);
}
