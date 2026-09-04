#include "game_text_localization.hpp"
#include "data/sha256.hpp"

#include <array>
#include <stdexcept>

namespace eon {
namespace {

constexpr std::array definitions{
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.sound.welcome", "MILL.COM", "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e", 775, 21, "Welcome To MILLENIUM.",
        "Welcome to Millennium."},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.sound.choose", "MILL.COM", "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e", 799, 31, "Please Select Sound Effect Type",
        "Please select a sound system"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.sound.instruction", "MILL.COM", "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e", 832, 33, "By Typing The Appropriate Number.",
        "Type the corresponding number."},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.sound.ibm", "MILL.COM", "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e", 868, 15, "0 = IBM Speaker", "0 = IBM PC speaker"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.sound.blaster", "MILL.COM", "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e", 885, 17, "1 = Sound Blaster", "1 = Sound Blaster"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.sound.covox", "MILL.COM", "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e", 904, 22, "2 = Covox Sound Master",
        "2 = Covox Sound Master"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.sound.wait", "MILL.COM", "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e", 933, 25, "Thank You. Please Wait...",
        "Thank you. Please wait…"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.sound.ibm-name", "MILL.COM", "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e", 1322, 8, "sibm.drv", "IBM PC speaker"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.sound.blaster-name", "MILL.COM", "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e", 1349, 8, "ssbl.drv", "Sound Blaster"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.sound.covox-name", "MILL.COM", "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e", 1358, 8, "scvx.drv", "Covox Sound Master"},
};

} // namespace

std::span<const GameTextDefinition> game_text_definitions() {
    return definitions;
}

bool verify_game_text_source(const GameTextDefinition& definition,
    const std::span<const std::uint8_t> source_bytes) {
    if (to_hex(sha256(source_bytes)) != definition.source_sha256
        || definition.source_offset > source_bytes.size()
        || definition.source_size > source_bytes.size() - definition.source_offset
        || definition.source_size != definition.original_english.size()) return false;
    const auto source = source_bytes.subspan(definition.source_offset, definition.source_size);
    return std::equal(source.begin(), source.end(), definition.original_english.begin());
}

LocalizedGameText localize_game_text(const Game game, const Platform platform,
    const std::string_view source_sha256, const std::string_view original_text,
    const std::string_view selected_language, const Translator& translator) {
    const auto language = canonical_launcher_language(selected_language);
    for (const auto& definition : definitions) {
        if (definition.game != game || definition.platform != platform
            || definition.source_sha256 != source_sha256
            || definition.original_english != original_text) continue;
        if (language == "en") {
            return {std::string(definition.id), std::string(original_text),
                std::string(definition.canonical_english), language,
                std::string(definition.source_sha256), definition.source_offset,
                definition.source_size, true, false};
        }
        if (!translator.has_translation(definition.canonical_english)) {
            throw std::runtime_error("Selected catalog lacks recovered game text: "
                + std::string(definition.id));
        }
        return {std::string(definition.id), std::string(original_text),
            std::string(translator.translate(definition.canonical_english)),
            language, std::string(definition.source_sha256), definition.source_offset,
            definition.source_size, true, true};
    }
    throw std::runtime_error("Uncatalogued user-presented original game text");
}

} // namespace eon
