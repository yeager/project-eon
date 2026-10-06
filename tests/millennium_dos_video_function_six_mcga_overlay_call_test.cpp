#include "data/millennium_dos_video_driver.hpp"
#include "engine/millennium_dos_video_function_six_mcga_overlay_call_session.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace {
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
            if (ec) throw std::runtime_error("Cannot inspect media symlink");
            continue;
        }
        if (!std::filesystem::is_regular_file(status) || it->path().filename() != "MCGA.BIN"
            || it->file_size(ec) != 4'366 || ec) { ec.clear(); continue; }
        std::ifstream in(it->path(), std::ios::binary);
        std::vector<std::uint8_t> bytes(4'366);
        in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        char extra = 0;
        if (in.gcount() == static_cast<std::streamsize>(bytes.size()) && !in.get(extra) && in.eof()) {
            try {
                static_cast<void>(eon::parse_millennium_dos_video_driver(bytes,
                    eon::MillenniumDosVideoDriverKind::mcga));
                return bytes;
            } catch (const std::exception&) { }
        }
    }
    throw std::runtime_error("No hash-admitted direct English MCGA.BIN found");
}

eon::MillenniumDosVideoFunctionSixMcgaCallerOutcome caller_outcome(std::uint8_t branch) {
    using namespace eon;
    MillenniumDosVideoFunctionSixMcgaCallerOutcome c;
    c.instruction_boundary = 0x07b5;
    c.final_sp = 0x5000;
    c.call_stack_write = {0x076f,{0x3333},0x4ffe,0x0772};
    constexpr std::uint16_t sites[] = {0x0778,0x0779,0x077a,0x077b,0x077c};
    for (std::size_t i = 0; i < 5; ++i) c.stack_reads.push_back({i+1,sites[i],{0x3333},
        static_cast<std::uint16_t>(0x4ffe + 2*i),static_cast<std::uint16_t>(0x6000 + i)});
    c.mode_read = MillenniumDosVideoFunctionSixMcgaCallerCodeByteRead{6,0x077f,0x4444,0x07b9,branch};
    c.overlay_registers = MillenniumDosVideoFunctionSixMcgaOverlayRegisters{
        1,0,0,0,0x1000,0x2000,1,0x1111,0x2222,0x3333,0x5000,false};
    return c;
}
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const auto driver = find_mcga(argv[1]);
    using namespace eon;
    using Session = MillenniumDosVideoFunctionSixMcgaOverlayCallSession;
    Session session(driver,caller_outcome(0x87));
    std::uint64_t seq = 6;
    session.observe_stack_write({++seq,0x07b6,0x3333,0x4ffe,1});
    session.observe_source_read({++seq,0x07b7,0x1111,0x1000,0x0f});
    session.observe_destination_read({++seq,0x07b8,0x2222,0x2000,0xf0});
    session.observe_destination_write({++seq,0x07b8,0x2222,0x2000,0xff});
    session.observe_stack_read({++seq,0x07c2,0x3333,0x4ffe,1});
    assert(session.state() == MillenniumDosVideoFunctionSixMcgaOverlayState::complete);
    session.observe_caller_return({++seq,0x07c6,0x3333,0x5000,0xbeef});
    assert(session.outcome() && session.outcome()->branch_byte == 0x87
        && session.outcome()->loop.instruction_boundary == 0x07c6
        && session.outcome()->return_ip == 0xbeef && session.outcome()->final_sp == 0x5002);

    bool rejected_88 = false;
    try { Session invalid(driver,caller_outcome(0x88)); }
    catch (const std::runtime_error&) { rejected_88 = true; }
    assert(rejected_88);
    bool rejected_bad_return = false;
    try {
        Session invalid(driver,caller_outcome(0x87));
        invalid.observe_caller_return({7,0x07c6,0x3333,0x5000,0xbeef});
    } catch (const std::runtime_error&) { rejected_bad_return = true; }
    assert(rejected_bad_return);
    return 0;
}
