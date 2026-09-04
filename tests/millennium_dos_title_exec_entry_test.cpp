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
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().filename() == "MILL.COM") mill_path = entry.path();
        if (entry.path().filename() == "TITLES.EXE") titles_path = entry.path();
        if (!mill_path.empty() && !titles_path.empty()
            && mill_path.parent_path() == titles_path.parent_path()) break;
    }
    if (mill_path.empty() || titles_path.empty()
        || mill_path.parent_path() != titles_path.parent_path()) return 3;
    const auto mill = read(mill_path);
    const auto titles = read(titles_path);
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
    eon::MillenniumDosTitleInitializationSession other_mode(titles,0x2468,2);
    other_mode.execute_exact_startup(3,0x1b80,0x1b95,0x0122,0x91);
    other_mode.observe_private_interrupt_result({4,0x0127,0x0129,0x00ff,0});
    assert(other_mode.checkpoint().selected_mode==0
        &&other_mode.checkpoint().selected_call_address==0x1bb2
        &&other_mode.checkpoint().selected_call_target==0x1ada);
    other_mode.execute_selected_callee_start(5,0x1bb2,0x1ada);
    assert(other_mode.checkpoint().state==eon::MillenniumDosTitleInitializationState::
            selected_callee_private_interrupt_result_boundary
        &&other_mode.checkpoint().selected_callee_boundary.call_address==0x1ae2
        &&other_mode.checkpoint().selected_callee_boundary.function==4
        &&other_mode.checkpoint().selected_callee_boundary.record_offset==0x1ac5);
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
}
