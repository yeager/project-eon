#include "data/millennium_dos_video_driver.hpp"
#include "engine/millennium_dos_video_function_six_mcga_register_setup_session.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <map>
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

void provide_reads(eon::MillenniumDosVideoFunctionSixMcgaRegisterSetupSession& session,
    const std::uint16_t bx) {
    using Word = eon::MillenniumDosVideoFunctionSixMcgaDescriptorRead;
    using Byte = eon::MillenniumDosVideoFunctionSixMcgaDescriptorByteRead;
    session.observe_word_read(Word{1,0x073d,0x3333,static_cast<std::uint16_t>(bx + 0x0a),4});
    session.observe_word_read(Word{2,0x0741,0x3333,static_cast<std::uint16_t>(bx + 0x0c),2});
    session.observe_byte_read(Byte{3,0x0747,0x3333,static_cast<std::uint16_t>(bx + 0x06),0x12});
    session.observe_word_read(Word{4,0x0755,0x3333,static_cast<std::uint16_t>(bx + 0x08),3});
    session.observe_word_read(Word{5,0x0759,0x3333,static_cast<std::uint16_t>(bx + 0x10),0x0200});
    session.observe_word_read(Word{6,0x0764,0x3333,static_cast<std::uint16_t>(bx + 0x0e),0x3344});
    session.observe_byte_read(Byte{7,0x076b,0x3333,static_cast<std::uint16_t>(bx + 0x04),0xab});
}

} // namespace

int main(const int argc, char** argv) {
    if (argc != 2) return 2;
    const auto driver = find_mcga(argv[1]);
    using Session = eon::MillenniumDosVideoFunctionSixMcgaRegisterSetupSession;
    using State = eon::MillenniumDosVideoFunctionSixMcgaRegisterSetupState;
    const eon::MillenniumDosVideoFunctionSixMcgaRegisterSnapshot initial{
        3,0x0200,0x5678,0x9999,0x1000,0x2222,0x1111,0x3333,0x4444,0x0100};
    Session session(driver, initial);
    assert(session.state() == State::awaiting_signed_multiply_operand
        && session.registers().ds == initial.cx
        && session.registers().sp == 0x00fe
        && session.stack_writes().size() == 1
        && session.stack_writes().front()
            == (eon::MillenniumDosVideoFunctionSixMcgaSetupStackWrite{
                0x073a,initial.ss,0x00fe,initial.ds}));
    expect_rejected([&] {
        session.observe_word_read({1,0x073d,initial.es,0x020b,4});
    });
    assert(session.state() == State::awaiting_signed_multiply_operand
        && session.stack_writes().size() == 1);
    provide_reads(session, initial.bx);
    const eon::MillenniumDosVideoFunctionSixMcgaRegisterSnapshot expected_final{
        0x22ab,initial.bx,initial.cx,0xff40,0x100e,0x1683,initial.cx,initial.es,
        initial.ss,0x00f6};
    assert(session.state() == State::before_helper_call
        && session.registers() == expected_final
        && session.outcome()
        && session.outcome()->initial_registers == initial
        && session.outcome()->final_registers == expected_final
        && session.outcome()->word_reads.size() == 5
        && session.outcome()->byte_reads.size() == 2
        && session.outcome()->stack_writes
            == (std::vector<eon::MillenniumDosVideoFunctionSixMcgaSetupStackWrite>{
                {0x073a,initial.ss,0x00fe,initial.ds},
                {0x075d,initial.ss,0x00fc,0x0200},
                {0x0764,initial.ss,0x00fa,0x3344},
                {0x0768,initial.ss,0x00f8,initial.di},
                {0x076a,initial.ss,0x00f6,0xff40}}));

    auto wrapping_stack = initial;
    wrapping_stack.sp = 0;
    Session stack_wrap(driver, wrapping_stack);
    provide_reads(stack_wrap, wrapping_stack.bx);
    assert(stack_wrap.registers().sp == 0xfff6
        && stack_wrap.stack_writes().front().offset == 0xfffe
        && stack_wrap.stack_writes().back().offset == 0xfff6);

    auto highest_descriptor = initial;
    highest_descriptor.bx = 0xffee;
    Session highest_valid(driver, highest_descriptor);
    provide_reads(highest_valid, highest_descriptor.bx);
    assert(highest_valid.state() == State::before_helper_call
        && highest_valid.outcome()->word_reads[3].offset == 0xfffe);

    expect_rejected([&] {
        auto wrapping_descriptor = initial;
        wrapping_descriptor.bx = 0xffef;
        Session invalid(driver, wrapping_descriptor);
    });
    expect_rejected([&] {
        static_cast<void>(Session(std::span<const std::uint8_t>(driver).subspan(1), initial));
    });
    return 0;
}
