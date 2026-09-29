#include "data/millennium_dos_video_driver.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> read_driver(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Missing real Millennium DOS video-driver leaf: " + path.filename().string());
    }
    return {std::istreambuf_iterator<char>(stream), {}};
}

} // namespace

int main(const int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
    const auto root = std::filesystem::path(argv[1]);
    const auto ega = eon::parse_millennium_dos_video_driver(
        read_driver(root / "EGA640.BIN"), eon::MillenniumDosVideoDriverKind::ega640);
    const auto mcga = eon::parse_millennium_dos_video_driver(
        read_driver(root / "MCGA.BIN"), eon::MillenniumDosVideoDriverKind::mcga);

    assert(ega.function_zero_cached_mode_known_branch_target == 0x1db);
    assert(ega.function_zero_mode_match_branch_target == 0x1eb);
    assert(ega.function_zero_mode_mismatch_return == 0x1ea);
    assert(mcga.function_zero_cached_mode_known_branch_target == 0x1f9);
    assert(mcga.function_zero_mode_match_branch_target == 0x209);
    assert(mcga.function_zero_mode_mismatch_return == 0x208);
}
