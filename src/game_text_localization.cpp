#include "game_text_localization.hpp"

#include <array>
#include <stdexcept>

namespace eon {
namespace {

constexpr std::array definitions{
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.sound.welcome", "Welcome To MILLENIUM.",
        "Welcome to Millennium."},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.sound.choose", "Please Select Sound Effect Type",
        "Please select a sound system"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.sound.instruction", "By Typing The Appropriate Number.",
        "Type the corresponding number."},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.sound.ibm", "0 = IBM Speaker", "0 = IBM PC speaker"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.sound.blaster", "1 = Sound Blaster", "1 = Sound Blaster"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.sound.covox", "2 = Covox Sound Master",
        "2 = Covox Sound Master"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.sound.wait", "Thank You. Please Wait...",
        "Thank you. Please wait…"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.sound.ibm-name", "sibm.drv", "IBM PC speaker"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.sound.blaster-name", "ssbl.drv", "Sound Blaster"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.sound.covox-name", "scvx.drv", "Covox Sound Master"},
};

} // namespace

std::span<const GameTextDefinition> game_text_definitions() {
    return definitions;
}

LocalizedGameText localize_game_text(const Game game, const Platform platform,
    const std::string_view original_text, const std::string_view selected_language,
    const Translator& translator) {
    const auto language = canonical_launcher_language(selected_language);
    for (const auto& definition : definitions) {
        if (definition.game != game || definition.platform != platform
            || definition.original_english != original_text) continue;
        if (language == "en") {
            return {std::string(definition.id), std::string(original_text),
                std::string(definition.canonical_english), language, true, false};
        }
        if (!translator.has_translation(definition.canonical_english)) {
            throw std::runtime_error("Selected catalog lacks recovered game text: "
                + std::string(definition.id));
        }
        return {std::string(definition.id), std::string(original_text),
            std::string(translator.translate(definition.canonical_english)),
            language, true, true};
    }
    throw std::runtime_error("Uncatalogued user-presented original game text");
}

} // namespace eon
