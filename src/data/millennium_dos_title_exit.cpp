#include "data/millennium_dos_title_exit.hpp"

#include "data/sha256.hpp"

#include <stdexcept>

namespace eon {
MillenniumDosTitleExitClosure parse_millennium_dos_title_exit_closure(
    const std::span<const std::uint8_t> titles_executable) {
    constexpr std::size_t load_bias = 0x100;
    constexpr auto executable_sha256 =
        "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6";
    constexpr std::size_t nonzero_route_size = 22;
    constexpr std::size_t exit_tail_size = 11;
    constexpr auto nonzero_sha256 =
        "d0981a03e0f8fdc9449e080668b7808952a48d0d3de4beb3a528ba5fc0f05951";
    constexpr auto exit_tail_sha256 =
        "b8160617c570a0dafcfea4e57187b7dd9182ced8da1153f6f77c63d5e7fe6a88";
    constexpr auto offset = [](const std::uint16_t address) {
        return static_cast<std::size_t>(address) - load_bias;
    };
    if (titles_executable.size() != 7'022
        || to_hex(sha256(titles_executable)) != executable_sha256
        || to_hex(sha256(titles_executable.subspan(offset(0x1c54), nonzero_route_size)))
            != nonzero_sha256
        || to_hex(sha256(titles_executable.subspan(offset(0x1a0f), exit_tail_size)))
            != exit_tail_sha256) {
        throw std::runtime_error("Unsupported Millennium DOS title exit closure");
    }
    return {std::string(executable_sha256), 0x1c54, nonzero_route_size,
        std::string(nonzero_sha256), 0x1c54, 0x1968, 0x1c57, 0x12c0,
        0x1c5a, 0x1a0e, 0, 0x1c60, 0x1aa0, 0x1c64, 0x0916, 0x1c67,
        0x1a0f, 0x1a0f, 0x112e, 0x21, 0x4c, std::string(exit_tail_sha256)};
}

} // namespace eon
