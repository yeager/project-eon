#include "data/millennium_dos_video_driver.hpp"
#include "engine/millennium_dos_video_function_six_mcga_helper_session.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace {

std::vector<std::uint8_t> read_mcga(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Missing direct MCGA.BIN media");
    std::vector<std::uint8_t> bytes(4'366);
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (stream.gcount() != static_cast<std::streamsize>(bytes.size())) {
        throw std::runtime_error("MCGA.BIN size changed during bounded read");
    }
    char extra = 0;
    if (stream.get(extra) || !stream.eof() || stream.bad()) {
        throw std::runtime_error("MCGA.BIN is larger than its bounded size or unreadable");
    }
    static_cast<void>(eon::parse_millennium_dos_video_driver(
        bytes, eon::MillenniumDosVideoDriverKind::mcga));
    return bytes;
}

std::vector<std::uint8_t> find_mcga(const std::filesystem::path& root) {
    constexpr std::size_t max_entries = 10'000;
    std::size_t visited = 0;
    std::error_code error;
    std::filesystem::recursive_directory_iterator it(root,
        std::filesystem::directory_options::skip_permission_denied, error), end;
    if (error) throw std::runtime_error("Cannot inspect direct media directory");
    for (; it != end; it.increment(error)) {
        if (error) throw std::runtime_error("Cannot continue scanning direct media directory");
        if (++visited > max_entries) throw std::runtime_error("Direct media scan bound exceeded");
        const auto status = it->symlink_status(error);
        if (error) throw std::runtime_error("Cannot inspect direct media entry");
        if (std::filesystem::is_symlink(status)) {
            if (it->is_directory(error)) it.disable_recursion_pending();
            if (error) throw std::runtime_error("Cannot inspect direct media symlink");
            continue;
        }
        if (!std::filesystem::is_regular_file(status) || it->path().filename() != "MCGA.BIN") continue;
        if (it->file_size(error) != 4'366 || error) {
            error.clear();
            continue;
        }
        try { return read_mcga(it->path()); }
        catch (const std::exception&) { /* Continue past unrelated same-name files. */ }
    }
    if (error) throw std::runtime_error("Cannot finish direct media scan");
    throw std::runtime_error("No hash-admitted English MCGA.BIN found in direct media directory");
}

template<typename F>
void expect_rejected(F&& f) {
    bool rejected = false;
    try { f(); } catch (const std::exception&) { rejected = true; }
    assert(rejected);
}

} // namespace

int main(const int argc, char** argv) {
    if (argc != 2) return 2;
    const auto driver = find_mcga(argv[1]);
    using Session = eon::MillenniumDosVideoFunctionSixMcgaHelperSession;
    using State = eon::MillenniumDosVideoFunctionSixMcgaHelperState;
    using DataRead = eon::MillenniumDosVideoFunctionSixMcgaHelperDataByteRead;
    using TableRead = eon::MillenniumDosVideoFunctionSixMcgaHelperTableWordRead;
    using StackRead = eon::MillenniumDosVideoFunctionSixMcgaHelperStackWordRead;
    const eon::MillenniumDosVideoDataSegment ds{0x1111};
    const eon::MillenniumDosVideoFunctionSixMcgaHelperStackSegment ss{0x2222};

    Session wrapped(driver, 0x12df, 0x4567, 0x89ab, ds, ss, 1);
    expect_rejected([&] { wrapped.observe_threshold({1,0x0666,{0x3333},0x00ac,0xe0}); });
    assert(wrapped.state() == State::awaiting_threshold && wrapped.stack_writes().empty());
    wrapped.observe_threshold({1,0x0666,ds,0x00ac,0xe0});
    assert(wrapped.state() == State::awaiting_table_word_zero
        && wrapped.stack_writes() == (std::vector<eon::MillenniumDosVideoFunctionSixMcgaHelperStackWordWrite>{
            {0x066c,ss,0xffff,0x4567}}));
    expect_rejected([&] { wrapped.observe_table_word({2,0x0678,ds,0xfffe,0x1111}); });
    wrapped.observe_table_word({2,0x0678,ds,0xfffc,0x1357});
    wrapped.observe_table_word({3,0x067a,ds,0xfffe,0x2468});
    assert(wrapped.state() == State::awaiting_bx_pop);
    expect_rejected([&] { wrapped.observe_stack_word({4,0x067d,ss,0xffff,0x4568}); });
    wrapped.observe_stack_word({4,0x067d,ss,0xffff,0x4567});
    assert(wrapped.state() == State::awaiting_ret);
    wrapped.observe_stack_word({5,0x067e,ss,1,0x0772});
    assert(wrapped.state() == State::complete && wrapped.outcome().has_value());
    const auto& out = *wrapped.outcome();
    assert(out.table_offset == 0xfffc && out.table_reads.size() == 2
        && out.final_ax == 0x1357 && out.final_bx == 0x4567 && out.final_dx == 0x2468
        && !out.carry && out.return_ip == 0x0772 && out.final_sp == 3
        && out.stack_reads.size() == 2);

    Session carry_clear(driver, 0x12fe, 0x0102, 0x0304, ds, ss, 0xfffe);
    carry_clear.observe_threshold({1,0x0666,ds,0x00ac,0xff});
    assert(carry_clear.state() == State::awaiting_table_word_zero
        && carry_clear.stack_writes().front().offset == 0xfffc
        && carry_clear.stack_writes().front().value == 0x0102);
    carry_clear.observe_table_word({2,0x0678,ds,0x0078,0xaabb});
    carry_clear.observe_table_word({3,0x067a,ds,0x007a,0xccdd});
    carry_clear.observe_stack_word({4,0x067d,ss,0xfffc,0x0102});
    carry_clear.observe_stack_word({5,0x067e,ss,0xfffe,0x0888});
    assert(carry_clear.outcome()->table_offset == 0x0078
        && carry_clear.outcome()->carry && carry_clear.outcome()->final_sp == 0);

    Session short_path(driver, 0x12e0, 0x1112, 0x1314, ds, ss, 0x2000);
    short_path.observe_threshold({1,0x0666,ds,0x00ac,0xe0});
    assert(short_path.state() == State::awaiting_ret && short_path.stack_writes().empty());
    short_path.observe_stack_word({2,0x0680,ss,0x2000,0x4321});
    assert(short_path.outcome()->carry && short_path.outcome()->return_ip == 0x4321
        && short_path.outcome()->final_ax == 0x12e0
        && short_path.outcome()->final_bx == 0x1112
        && short_path.outcome()->final_dx == 0x1314
        && short_path.outcome()->final_sp == 0x2002
        && !short_path.outcome()->table_offset.has_value());

    auto wrong_helper = driver;
    wrong_helper[0x0666] ^= 1;
    expect_rejected([&] { Session rejected(wrong_helper,0,0,0,ds,ss,0); });
}
