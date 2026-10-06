#include "data/millennium_dos_video_driver.hpp"
#include "data/sha256.hpp"
#include "engine/millennium_dos_video_function_six_mcga_overlay_loop_session.hpp"

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
        throw std::runtime_error("MCGA.BIN changed during bounded read");
    }
    char extra = 0;
    if (stream.get(extra) || !stream.eof() || stream.bad()) {
        throw std::runtime_error("MCGA.BIN exceeds its admitted size or is unreadable");
    }
    static_cast<void>(eon::parse_millennium_dos_video_driver(bytes, eon::MillenniumDosVideoDriverKind::mcga));
    return bytes;
}

std::vector<std::uint8_t> find_mcga(const std::filesystem::path& root) {
    std::error_code ec;
    std::size_t count = 0;
    std::filesystem::recursive_directory_iterator it(root,
        std::filesystem::directory_options::skip_permission_denied, ec), end;
    if (ec) throw std::runtime_error("Cannot scan direct media directory");
    for (; it != end; it.increment(ec)) {
        if (ec || ++count > 10'000) throw std::runtime_error("Direct media scan failed or exceeded bound");
        const auto status = it->symlink_status(ec);
        if (ec) throw std::runtime_error("Cannot inspect direct media entry");
        if (std::filesystem::is_symlink(status)) {
            if (it->is_directory(ec)) it.disable_recursion_pending();
            if (ec) throw std::runtime_error("Cannot inspect direct media symlink");
            continue;
        }
        if (!std::filesystem::is_regular_file(status) || it->path().filename() != "MCGA.BIN") continue;
        if (it->file_size(ec) != 4'366 || ec) { ec.clear(); continue; }
        try { return read_mcga(it->path()); } catch (const std::exception&) { }
    }
    throw std::runtime_error("No hash-admitted direct English MCGA.BIN found");
}

using Session = eon::MillenniumDosVideoFunctionSixMcgaOverlayLoopSession;
using State = eon::MillenniumDosVideoFunctionSixMcgaOverlayState;
using Registers = eon::MillenniumDosVideoFunctionSixMcgaOverlayRegisters;

void run_one_byte(Session& session, std::uint64_t& sequence,
    const std::uint8_t source_value, const std::uint8_t destination_before,
    const std::uint16_t source_offset, const std::uint16_t destination_offset) {
    session.observe_source_read({++sequence,0x07b7,0x1111,source_offset,source_value});
    session.observe_destination_read({++sequence,0x07b8,0x2222,destination_offset,destination_before});
    session.observe_destination_write({++sequence,0x07b8,0x2222,destination_offset,
        static_cast<std::uint8_t>(source_value | destination_before)});
}

} // namespace

int main(const int argc, char** argv) {
    if (argc != 2) return 2;
    const auto driver = find_mcga(argv[1]);
    assert(eon::to_hex(eon::sha256(std::span(driver).subspan(0x07b5,0x12)))
        == "1f74b1e9894303e3e6772b8c95a687554f7b989175ea27e756f24293d32fcded");

    // XCHG selects AX as the first row's count. PUSH/POP CX brackets every
    // row, LODSB follows DF, and INC DI remains forward-only.
    Session session(driver,Registers{2,1,1,1,0x1000,0x2000,2,0x1111,0x2222,
        0x3333,0x5000,false});
    std::uint64_t sequence = 0;
    assert(session.state() == State::awaiting_stack_push);
    session.observe_stack_write({++sequence,0x07b6,0x3333,0x4ffe,2});
    run_one_byte(session,sequence,0x0f,0xf0,0x1000,0x2000);
    run_one_byte(session,sequence,0x01,0x02,0x1001,0x2001);
    assert(session.state() == State::awaiting_stack_pop);
    session.observe_stack_read({++sequence,0x07c2,0x3333,0x4ffe,2});
    session.observe_stack_write({++sequence,0x07b6,0x3333,0x4ffe,2});
    run_one_byte(session,sequence,0x03,0x10,0x1003,0x2003);
    run_one_byte(session,sequence,0x00,0x80,0x1004,0x2004);
    session.observe_stack_read({++sequence,0x07c2,0x3333,0x4ffe,2});
    assert(session.state() == State::complete && session.outcome());
    const auto& outcome = *session.outcome();
    assert(outcome.instruction_boundary == 0x07c6 && outcome.rows.size() == 2
        && outcome.rows[0].byte_count == 2 && outcome.rows[1].byte_count == 2
        && outcome.rows[0].effects[0].destination_after == 0xff
        && outcome.rows[0].effects[1].destination_after == 0x03
        && outcome.rows[0].source_after_stride == 0x1003
        && outcome.rows[0].destination_after_stride == 0x2003
        && outcome.rows[1].source_after_stride == 0x1006
        && outcome.rows[1].destination_after_stride == 0x2006
        && outcome.final_registers.sp == 0x5000 && outcome.final_registers.bp == 0
        && outcome.final_registers.cx == 2 && outcome.final_registers.ax == 0);

    // A changed popped value is explicit memory evidence. It becomes the next
    // row's count, while the initial XCHG is not repeated at $07b6.
    Session changed_pop(driver,Registers{1,0,2,0,0x4000,0x5000,2,0x1111,0x2222,
        0x3333,0x6000,false});
    sequence = 0;
    changed_pop.observe_stack_write({++sequence,0x07b6,0x3333,0x5ffe,1});
    run_one_byte(changed_pop,sequence,0x01,0x00,0x4000,0x5000);
    changed_pop.observe_stack_read({++sequence,0x07c2,0x3333,0x5ffe,2});
    changed_pop.observe_stack_write({++sequence,0x07b6,0x3333,0x5ffe,2});
    run_one_byte(changed_pop,sequence,0x04,0x20,0x4001,0x5001);
    run_one_byte(changed_pop,sequence,0x08,0x40,0x4002,0x5002);
    changed_pop.observe_stack_read({++sequence,0x07c2,0x3333,0x5ffe,2});
    assert(changed_pop.outcome() && changed_pop.outcome()->rows.size() == 2
        && changed_pop.outcome()->rows[0].byte_count == 1
        && changed_pop.outcome()->rows[1].byte_count == 2);

    Session backwards(driver,Registers{1,0,0,0,0,0xffff,1,0x1111,0x2222,
        0x3333,0x7000,true});
    sequence = 0;
    backwards.observe_stack_write({++sequence,0x07b6,0x3333,0x6ffe,1});
    run_one_byte(backwards,sequence,0x80,0x01,0x0000,0xffff);
    backwards.observe_stack_read({++sequence,0x07c2,0x3333,0x6ffe,1});
    assert(backwards.outcome() && backwards.outcome()->rows[0].effects[0].source_offset == 0
        && backwards.outcome()->rows[0].source_after_stride == 0xffff
        && backwards.outcome()->rows[0].effects[0].destination_after == 0x81
        && backwards.outcome()->rows[0].destination_after_stride == 0);

    bool rejected_zero_count = false;
    try {
        Session unbounded(driver,Registers{0,0,0,0,0,0,1,0x1111,0x2222,0x3333,0x8000,false},
            1,65'535);
        static_cast<void>(unbounded);
    } catch (const std::runtime_error&) {
        rejected_zero_count = true;
    }
    assert(rejected_zero_count);

    bool rejected_bad_or_result = false;
    Session malformed(driver,Registers{1,0,0,0,0,0,1,0x1111,0x2222,0x3333,0x9000,false});
    sequence = 0;
    malformed.observe_stack_write({++sequence,0x07b6,0x3333,0x8ffe,1});
    malformed.observe_source_read({++sequence,0x07b7,0x1111,0,0x0f});
    malformed.observe_destination_read({++sequence,0x07b8,0x2222,0,0xf0});
    try {
        malformed.observe_destination_write({++sequence,0x07b8,0x2222,0,0x0f});
    } catch (const std::runtime_error&) {
        rejected_bad_or_result = true;
    }
    assert(rejected_bad_or_result && malformed.state() == State::awaiting_destination_write);
    return 0;
}
