#include "engine/millennium_dos_title_exec_entry_session.hpp"
#include "engine/millennium_dos_title_child_compatibility_service.hpp"
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
}
