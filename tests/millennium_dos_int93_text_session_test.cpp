#include "engine/millennium_dos_int93_text_session.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> read_member(const std::filesystem::path& path,
    const std::size_t expected_size) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Missing original DOS text-driver member");
    std::vector<std::uint8_t> bytes(expected_size);
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (stream.gcount() != static_cast<std::streamsize>(bytes.size())) {
        throw std::runtime_error("Original DOS text-driver member changed size");
    }
    char trailing = 0;
    if (stream.get(trailing) || !stream.eof() || stream.bad()) {
        throw std::runtime_error("Original DOS text-driver member exceeds its expected size");
    }
    return bytes;
}

template<typename Function>
void expect_rejected(Function&& function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::exception&) {
        rejected = true;
    }
    assert(rejected);
}

} // namespace

int main(const int argc, char** argv) {
    if (argc != 2) return 2;
    const auto directory = std::filesystem::path(argv[1])
        / "millennium-return-to-earth-2-2";
    const auto vga_bytes = read_member(directory / "VGATXT.BIN", 1024);
    const auto ega6_bytes = read_member(directory / "EG6TXT.BIN", 2040);
    const auto vga = eon::parse_millennium_dos_text_driver(
        vga_bytes, eon::MillenniumDosTextDriverKind::vga);
    const auto ega6 = eon::parse_millennium_dos_text_driver(
        ega6_bytes, eon::MillenniumDosTextDriverKind::ega6);
    assert(vga.page_stride == 0x0a00 && vga.handler_offsets[7] == 0x0046
        && vga.handler_offsets[8] == 0x03f3 && vga.handler_offsets[9] == 0x004f);
    assert(ega6.page_stride == 0x0280 && ega6.handler_offsets[7] == 0x0046
        && ega6.handler_offsets[8] == 0x07eb && ega6.handler_offsets[9] == 0x004f);

    constexpr std::uint16_t driver_segment = 0x3456;
    const eon::MillenniumDosInt93InstallationObservation vga_installation{
        vga.sha256, driver_segment, driver_segment, 0};
    eon::MillenniumDosInt93TextSession vga_session(
        vga_bytes, eon::MillenniumDosTextDriverKind::vga, vga_installation);
    const auto vga_ah0 = vga_session.observe_call({1, driver_segment, 0x0026, 0, 0, 0, 0});
    assert(vga_ah0.writes.empty() && vga_ah0.dispatcher_cleanup_offset == 0x001b);
    const auto vga_ah6 = vga_session.observe_call({2, driver_segment, 0x0026, 6, 0, 0, 0});
    assert(vga_ah6.writes.empty() && vga_ah6.dispatcher_cleanup_offset == 0x001b);
    const auto ah3 = vga_session.observe_call({3, driver_segment, 0x0040, 3, 0, 0x1234, 0});
    assert((ah3.writes == std::vector<eon::MillenniumDosInt93TextWrite>{
        {driver_segment, 0x0020, 2, 0x1234}}));
    const auto ah4 = vga_session.observe_call({4, driver_segment, 0x03e9, 4, 0x42, 0, 0});
    assert((ah4.writes == std::vector<eon::MillenniumDosInt93TextWrite>{
        {driver_segment, 0x0024, 1, 0x42}}));
    const auto ah5 = vga_session.observe_call({5, driver_segment, 0x03ee, 5, 0xe1, 0, 0});
    assert((ah5.writes == std::vector<eon::MillenniumDosInt93TextWrite>{
        {driver_segment, 0x0025, 1, 0xe1}}));
    const auto ah7 = vga_session.observe_call({6, driver_segment, 0x0046, 7, 0, 0, 0xabcd, 0});
    assert((ah7.writes == std::vector<eon::MillenniumDosInt93TextWrite>{
        {driver_segment, 0x0022, 2, 0xabcd}}));
    const auto ah8 = vga_session.observe_call({7, driver_segment, 0x03f3, 8, 0, 0, 0, 0xf800});
    assert((ah8.writes == std::vector<eon::MillenniumDosInt93TextWrite>{
        {driver_segment, 0x0022, 2, 0x0200},
        {driver_segment, 0x0020, 2, 0x0200}}));
    const auto ah9 = vga_session.observe_call({8, driver_segment, 0x004f, 9, 0, 0, 0, 0xabcd});
    assert((ah9.writes == std::vector<eon::MillenniumDosInt93TextWrite>{
        {driver_segment, 0x0020, 2, 0xabcd}}));
    assert(ah9.dispatcher_cleanup_offset == 0x001b);

    eon::MillenniumDosInt93TextSession ega6_session(ega6_bytes,
        eon::MillenniumDosTextDriverKind::ega6,
        {ega6.sha256, driver_segment, driver_segment, 0});
    const auto ega6_ah0 = ega6_session.observe_call({1, driver_segment, 0x0026, 0, 0, 0, 0});
    assert(ega6_ah0.writes.empty());
    const auto ega6_ah6 = ega6_session.observe_call({2, driver_segment, 0x0026, 6, 0, 0, 0});
    assert(ega6_ah6.writes.empty());
    const auto ega6_ah4 = ega6_session.observe_call(
        {3, driver_segment, 0x07c3, 4, 0xfe, 0, 0});
    assert((ega6_ah4.writes == std::vector<eon::MillenniumDosInt93TextWrite>{
        {driver_segment, 0x0024, 1, 0x00fe},
        {driver_segment, 0x0688, 2, 0x0df8}}));
    const auto ega6_ah5 = ega6_session.observe_call(
        {4, driver_segment, 0x07d7, 5, 0x03, 0, 0});
    assert((ega6_ah5.writes == std::vector<eon::MillenniumDosInt93TextWrite>{
        {driver_segment, 0x0025, 1, 0x0003},
        {driver_segment, 0x068a, 2, 0x0620}}));
    const auto ega6_ah7 = ega6_session.observe_call(
        {5, driver_segment, 0x0046, 7, 0, 0, 0x0180, 0});
    assert((ega6_ah7.writes == std::vector<eon::MillenniumDosInt93TextWrite>{
        {driver_segment, 0x0022, 2, 0x0180}}));
    const auto ega6_ah8 = ega6_session.observe_call(
        {6, driver_segment, 0x07eb, 8, 0, 0, 0, 0xff00});
    assert((ega6_ah8.writes == std::vector<eon::MillenniumDosInt93TextWrite>{
        {driver_segment, 0x0022, 2, 0x0180},
        {driver_segment, 0x0020, 2, 0x0180}}));

    expect_rejected([&] {
        static_cast<void>(eon::MillenniumDosInt93TextSession(vga_bytes,
            eon::MillenniumDosTextDriverKind::vga,
            {vga.sha256, driver_segment, 0x4567, 0}));
    });
    expect_rejected([&] {
        static_cast<void>(eon::MillenniumDosInt93TextSession(vga_bytes,
            eon::MillenniumDosTextDriverKind::vga,
            {ega6.sha256, driver_segment, driver_segment, 0}));
    });
    expect_rejected([&] {
        static_cast<void>(vga_session.observe_call(
            {7, driver_segment, 0x03f3, 7, 0, 0, 0, 0}));
    });
    expect_rejected([&] {
        static_cast<void>(vga_session.observe_call(
            {7, driver_segment, 0x0040, 10, 0, 0, 0, 0}));
    });
    expect_rejected([&] {
        static_cast<void>(vga_session.observe_call(
            {7, driver_segment, 0x0330, 1, 0, 0, 0, 0}));
    });
    auto altered = vga_bytes;
    altered[0x40] ^= 0x01;
    expect_rejected([&] {
        static_cast<void>(eon::parse_millennium_dos_text_driver(
            altered, eon::MillenniumDosTextDriverKind::vga));
    });
    return 0;
}
