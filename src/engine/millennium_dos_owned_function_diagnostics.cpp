#include "engine/millennium_dos_owned_function_diagnostics.hpp"

#include "data/function_map.hpp"

#include <array>
#include <string_view>

namespace eon {
namespace {
struct Profile {
    RuntimeSessionKind kind;
    std::size_t index;
    std::uint16_t handler;
    std::string_view id;
};
constexpr std::array profiles{
    Profile{RuntimeSessionKind::millennium_dos_first_function, 0, 0x6f9a,
        "millennium-dos-en-f1-handler"},
    Profile{RuntimeSessionKind::millennium_dos_second_function, 1, 0x71ca,
        "millennium-dos-en-f2-handler"},
    Profile{RuntimeSessionKind::millennium_dos_second_function_callback,1,0x71ca,
        "millennium-dos-en-f2-handler"},
    Profile{RuntimeSessionKind::millennium_dos_third_function, 2, 0x6faa,
        "millennium-dos-en-f3-handler"},
    Profile{RuntimeSessionKind::millennium_dos_fourth_function, 3, 0x72f9,
        "millennium-dos-en-f4-handler"},
    Profile{RuntimeSessionKind::millennium_dos_fifth_function, 4, 0x7597,
        "millennium-dos-en-f5-handler"},
    Profile{RuntimeSessionKind::millennium_dos_sixth_function, 5, 0x7415,
        "millennium-dos-en-f6-handler"},
    Profile{RuntimeSessionKind::millennium_dos_seventh_function, 6, 0x7521,
        "millennium-dos-en-f7-handler"},
    Profile{RuntimeSessionKind::millennium_dos_eighth_function, 7, 0x7306,
        "millennium-dos-en-f8-prefix"},
    Profile{RuntimeSessionKind::millennium_dos_ninth_function, 8, 0x7339,
        "millennium-dos-en-f9-handler"},
    Profile{RuntimeSessionKind::millennium_dos_ninth_function_handoff,8,0x7339,
        "millennium-dos-en-f9-handler"},
    Profile{RuntimeSessionKind::millennium_dos_tenth_function, 9, 0x7384,
        "millennium-dos-en-f10-handler"},
};
constexpr std::string_view release_sha256 =
    "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123";
constexpr std::string_view game_sha256 =
    "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
}

std::optional<MillenniumDosOwnedFunctionDiagnostics>
make_millennium_dos_owned_function_diagnostics(
    const MillenniumDosOwnedFunctionDiagnosticInput& input) {
    if (input.session.game != Game::millennium || input.session.platform != Platform::dos
        || input.session.language != "en" || input.session.release_sha256 != release_sha256
        || input.game_executable_sha256 != game_sha256
        || !runtime_session_declaration_is_valid(input.session.kind,
            input.session.boundary, input.session.capabilities)
        || input.session.input_contract != RuntimeInputContract::none) return std::nullopt;
    for (const auto& profile : profiles) {
        if (input.session.kind != profile.kind) continue;
        const auto mapped = function_map_runtime_address_for(release_sha256, profile.id);
        if (input.function_key_index != profile.index
            || input.handler_address != profile.handler || !mapped
            || *mapped != profile.handler || input.boundary.instruction_address == 0) {
            return std::nullopt;
        }
        return MillenniumDosOwnedFunctionDiagnostics{
            input.session.release_sha256, input.game_executable_sha256,
            input.session.kind, std::string(profile.id), profile.index,
            profile.handler, input.boundary, "typed-observation"};
    }
    return std::nullopt;
}

} // namespace eon
