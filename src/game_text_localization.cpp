#include "game_text_localization.hpp"
#include "data/sha256.hpp"

#include <array>
#include <algorithm>
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
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.00-inner-system", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 978, 12,
        "Inner System", "Inner System"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.01-sun", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 991, 3,
        "Sun", "Sun"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.02-mercury", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 995, 8,
        "Mercury ", "Mercury"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.03-venus", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1004, 6,
        "Venus ", "Venus"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.04-earth", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1011, 6,
        "Earth ", "Earth"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.05-mars", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1018, 5,
        "Mars ", "Mars"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.06-jupiter", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1024, 8,
        "Jupiter ", "Jupiter"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.07-saturn", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1033, 7,
        "Saturn ", "Saturn"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.08-uranus", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1041, 7,
        "Uranus ", "Uranus"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.09-neptune", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1049, 8,
        "Neptune ", "Neptune"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.10-pluto", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1058, 6,
        "Pluto ", "Pluto"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.11-moon", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1065, 5,
        "Moon ", "Moon"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.12-phobos", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1071, 7,
        "Phobos ", "Phobos"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.13-deimos", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1079, 7,
        "Deimos ", "Deimos"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.14-amalthea", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1087, 9,
        "Amalthea ", "Amalthea"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.15-io", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1097, 3,
        "Io ", "Io"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.16-europa", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1101, 7,
        "Europa ", "Europa"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.17-ganymede", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1109, 9,
        "Ganymede ", "Ganymede"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.18-callisto", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1119, 9,
        "Callisto ", "Callisto"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.19-leda", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1129, 5,
        "Leda ", "Leda"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.20-himalia", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1135, 8,
        "Himalia ", "Himalia"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.21-elara", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1144, 6,
        "Elara ", "Elara"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.22-pasiphae", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1151, 9,
        "Pasiphae ", "Pasiphae"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.23-mimas", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1161, 6,
        "Mimas ", "Mimas"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.24-enceladus", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1168, 9,
        "Enceladus", "Enceladus"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.25-tethys", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1178, 7,
        "Tethys ", "Tethys"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.26-dione", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1186, 6,
        "Dione ", "Dione"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.27-rhea", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1193, 5,
        "Rhea ", "Rhea"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.28-titan", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1199, 6,
        "Titan ", "Titan"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.29-hyperion", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1206, 9,
        "Hyperion ", "Hyperion"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.30-iapetus", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1216, 8,
        "Iapetus ", "Iapetus"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.31-phoebe", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1225, 7,
        "Phoebe ", "Phoebe"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.32-miranda", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1233, 8,
        "Miranda ", "Miranda"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.33-ariel", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1242, 6,
        "Ariel ", "Ariel"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.34-umbriel", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1249, 8,
        "Umbriel ", "Umbriel"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.35-titania", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1258, 8,
        "Titania ", "Titania"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.36-oberon", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1267, 7,
        "Oberon ", "Oberon"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.37-triton", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1275, 7,
        "Triton ", "Triton"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.38-nereid", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1283, 7,
        "Nereid ", "Nereid"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.39-charon", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1291, 7,
        "Charon ", "Charon"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.40-asteroids", "2200AD4.BIN", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 1299, 10,
        "Asteroids ", "Asteroids"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.00-inner-system", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 987, 14,
        "Sistema inter.", "Inner System", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.01-sun", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1002, 3,
        "Sol", "Sun", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.02-mercury", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1006, 9,
        "Mercurio ", "Mercury", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.03-venus", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1016, 6,
        "Venus ", "Venus", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.04-earth", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1023, 7,
        "Tierra ", "Earth", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.05-mars", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1031, 6,
        "Marte ", "Mars", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.06-jupiter", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1038, 8,
        "Jupiter ", "Jupiter", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.07-saturn", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1047, 8,
        "Saturno ", "Saturn", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.08-uranus", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1056, 6,
        "Urano ", "Uranus", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.09-neptune", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1063, 8,
        "Neptuno ", "Neptune", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.10-pluto", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1072, 7,
        "Pluton ", "Pluto", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.11-moon", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1080, 5,
        "Luna ", "Moon", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.12-phobos", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1086, 6,
        "Fobos ", "Phobos", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.13-deimos", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1093, 7,
        "Deimos ", "Deimos", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.14-amalthea", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1101, 8,
        "Amaltea ", "Amalthea", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.15-io", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1110, 3,
        "Io ", "Io", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.16-europa", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1114, 7,
        "Europa ", "Europa", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.17-ganymede", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1122, 10,
        "Ganimedes ", "Ganymede", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.18-callisto", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1133, 8,
        "Calisto ", "Callisto", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.19-leda", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1142, 5,
        "Leda ", "Leda", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.20-himalia", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1148, 8,
        "Himalia ", "Himalia", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.21-elara", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1157, 6,
        "Elara ", "Elara", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.22-pasiphae", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1164, 9,
        "Parsifae ", "Pasiphae", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.23-mimas", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1174, 6,
        "Mimas ", "Mimas", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.24-enceladus", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1181, 7,
        "Enclado", "Enceladus", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.25-tethys", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1189, 6,
        "Tetis ", "Tethys", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.26-dione", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1196, 6,
        "Dione ", "Dione", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.27-rhea", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1203, 4,
        "Rea ", "Rhea", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.28-titan", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1208, 6,
        "Titan ", "Titan", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.29-hyperion", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1215, 9,
        "Hiperion ", "Hyperion", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.30-iapetus", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1225, 7,
        "Japeto ", "Iapetus", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.31-phoebe", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1233, 5,
        "Febe ", "Phoebe", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.32-miranda", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1239, 8,
        "Miranda ", "Miranda", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.33-ariel", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1248, 6,
        "Ariel ", "Ariel", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.34-umbriel", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1255, 8,
        "Umbriel ", "Umbriel", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.35-titania", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1264, 8,
        "Titania ", "Titania", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.36-oberon", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1273, 7,
        "Oberon ", "Oberon", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.37-triton", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1281, 7,
        "Triton ", "Triton", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.38-nereid", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1289, 8,
        "Nereida ", "Nereid", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.39-charon", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1298, 7,
        "Charon ", "Charon", "es"},
    GameTextDefinition{Game::millennium, Platform::dos,
        "millennium.dos.celestial.40-asteroids", "2200AD4.BIN", "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31", 1306, 11,
        "Asteroides ", "Asteroids", "es"},
    GameTextDefinition{Game::deuteros, Platform::amiga,
        "deuteros.amiga.prompt.hardware-failure", "Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 1 of 2).adf", "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38", 494705, 25,
        "General Hardware Failure.", "General hardware failure."},
    GameTextDefinition{Game::deuteros, Platform::amiga,
        "deuteros.amiga.prompt.insert-deuteros", "Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 1 of 2).adf", "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38", 494739, 22,
        "Please Insert Deuteros", "Please insert Deuteros"},
    GameTextDefinition{Game::deuteros, Platform::amiga,
        "deuteros.amiga.prompt.data-disk-drive", "Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 1 of 2).adf", "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38", 494762, 24,
        "DATA Disk Into Drive DFO", "DATA disk into drive DFO"},
    GameTextDefinition{Game::deuteros, Platform::amiga,
        "deuteros.amiga.prompt.press-any-key", "Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 1 of 2).adf", "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38", 494797, 25,
        "Press Any Key When Ready.", "Press any key when ready."},
    GameTextDefinition{Game::deuteros, Platform::amiga,
        "deuteros.amiga.prompt.yes", "Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 1 of 2).adf", "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38", 494823, 11,
        "Yes, Please", "Yes, please"},
    GameTextDefinition{Game::deuteros, Platform::amiga,
        "deuteros.amiga.prompt.no", "Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 1 of 2).adf", "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38", 494835, 10,
        "No, Thanks", "No, thanks"},
};

LocalizedGameText localize_definition(const GameTextDefinition& definition,
    const std::string_view selected_language, const Translator& translator) {
    const auto language = canonical_launcher_language(selected_language);
    if (language == "en") {
        return {std::string(definition.id), std::string(definition.original_text),
            std::string(definition.canonical_english), language,
            std::string(definition.source_sha256), definition.source_offset,
            definition.source_size, std::string(definition.source_language), true, false};
    }
    if (!translator.has_translation(definition.canonical_english)) {
        throw std::runtime_error("Selected catalog lacks recovered game text: "
            + std::string(definition.id));
    }
    return {std::string(definition.id), std::string(definition.original_text),
        std::string(translator.translate(definition.canonical_english)), language,
        std::string(definition.source_sha256), definition.source_offset,
        definition.source_size, std::string(definition.source_language), true, true};
}

bool game_text_range_matches(const GameTextDefinition& definition,
    const std::span<const std::uint8_t> source_bytes) {
    if (definition.source_offset > source_bytes.size()
        || definition.source_size > source_bytes.size() - definition.source_offset
        || definition.source_size != definition.original_text.size()) return false;
    const auto source = source_bytes.subspan(definition.source_offset, definition.source_size);
    for (std::size_t index = 0; index < source.size(); ++index) {
        if (source[index] != static_cast<std::uint8_t>(
                static_cast<unsigned char>(definition.original_text[index]))) return false;
    }
    return true;
}

} // namespace

std::span<const GameTextDefinition> game_text_definitions() {
    return definitions;
}

bool verify_game_text_source(const GameTextDefinition& definition,
    const std::span<const std::uint8_t> source_bytes) {
    return to_hex(sha256(source_bytes)) == definition.source_sha256
        && game_text_range_matches(definition, source_bytes);
}

LocalizedGameText localize_game_text(const Game game, const Platform platform,
    const std::string_view source_sha256, const std::string_view original_text,
    const std::string_view selected_language, const Translator& translator) {
    const auto language = canonical_launcher_language(selected_language);
    for (const auto& definition : definitions) {
        if (definition.game != game || definition.platform != platform
            || definition.source_sha256 != source_sha256
            || definition.original_text != original_text) continue;
        return localize_definition(definition, language, translator);
    }
    throw std::runtime_error("Uncatalogued user-presented original game text");
}

LocalizedGameText localize_game_text_at_source(const Game game, const Platform platform,
    const std::string_view source_leaf, const std::span<const std::uint8_t> source_bytes,
    const std::size_t source_offset, const std::size_t source_size,
    const std::string_view selected_language, const Translator& translator) {
    const auto source_sha256 = to_hex(sha256(source_bytes));
    for (const auto& definition : definitions) {
        if (definition.game != game || definition.platform != platform
            || definition.source_leaf != source_leaf
            || definition.source_sha256 != source_sha256
            || definition.source_offset != source_offset
            || definition.source_size != source_size) continue;
        if (!game_text_range_matches(definition, source_bytes))
            throw std::runtime_error("Recovered game-text source range is invalid");
        return localize_definition(definition, selected_language, translator);
    }
    throw std::runtime_error("Uncatalogued user-presented original game-text range");
}

std::vector<LocalizedGameText> localize_all_game_text_from_source(
    const Game game, const Platform platform, const std::string_view source_leaf,
    const std::span<const std::uint8_t> source_bytes,
    const std::string_view selected_language, const Translator& translator) {
    const auto admitted = admit_all_game_text_from_source(
        game, platform, source_leaf, source_bytes);
    std::vector<LocalizedGameText> localized;
    localized.reserve(admitted.size());
    for (const auto& token : admitted) {
        localized.push_back(localize_admitted_game_text(
            game, platform, token, selected_language, translator));
    }
    return localized;
}

std::vector<AdmittedGameText> admit_all_game_text_from_source(
    const Game game, const Platform platform, const std::string_view source_leaf,
    const std::span<const std::uint8_t> source_bytes) {
    const auto source_sha256 = to_hex(sha256(source_bytes));
    std::vector<const GameTextDefinition*> matches;
    for (const auto& definition : definitions) {
        if (definition.game == game && definition.platform == platform
            && definition.source_leaf == source_leaf
            && definition.source_sha256 == source_sha256) matches.push_back(&definition);
    }
    if (matches.empty()) throw std::runtime_error("Uncatalogued original game-text source leaf");
    std::ranges::sort(matches, {}, &GameTextDefinition::source_offset);
    std::size_t preceding_end = 0;
    std::vector<AdmittedGameText> admitted;
    admitted.reserve(matches.size());
    for (const auto* definition : matches) {
        if (!game_text_range_matches(*definition, source_bytes))
            throw std::runtime_error("Recovered game-text source range is invalid");
        if (!admitted.empty() && definition->source_offset < preceding_end)
            throw std::runtime_error("Recovered game-text source ranges overlap");
        preceding_end = definition->source_offset + definition->source_size;
        admitted.push_back({std::string(definition->id), std::string(definition->original_text),
            std::string(definition->canonical_english), std::string(definition->source_leaf),
            std::string(definition->source_sha256), definition->source_offset,
            definition->source_size, std::string(definition->source_language)});
    }
    return admitted;
}

LocalizedGameText localize_admitted_game_text(const Game game, const Platform platform,
    const AdmittedGameText& admitted, const std::string_view selected_language,
    const Translator& translator) {
    for (const auto& definition : definitions) {
        if (definition.game == game && definition.platform == platform
            && definition.id == admitted.id
            && definition.original_text == admitted.original_text
            && definition.canonical_english == admitted.canonical_english
            && definition.source_leaf == admitted.source_leaf
            && definition.source_sha256 == admitted.source_sha256
            && definition.source_offset == admitted.source_offset
            && definition.source_size == admitted.source_size
            && definition.source_language == admitted.source_language) {
            return localize_definition(definition, selected_language, translator);
        }
    }
    throw std::runtime_error("Invalid admitted game-text provenance token");
}

namespace {

template <typename Projection>
LocalizedGameText localize_unique_admitted_game_text(const Game game,
    const Platform platform, const std::span<const AdmittedGameText> admitted,
    const std::string_view key, const std::string_view selected_language,
    const Translator& translator, Projection projection) {
    const AdmittedGameText* match = nullptr;
    for (const auto& token : admitted) {
        if (projection(token) != key) continue;
        if (match) throw std::runtime_error("Ambiguous admitted game-text lookup");
        match = &token;
    }
    if (!match) throw std::runtime_error("Rendered game text lacks an admitted source token");
    return localize_admitted_game_text(
        game, platform, *match, selected_language, translator);
}

} // namespace

LocalizedGameText localize_admitted_game_text_by_id(const Game game,
    const Platform platform, const std::span<const AdmittedGameText> admitted,
    const std::string_view id, const std::string_view selected_language,
    const Translator& translator) {
    return localize_unique_admitted_game_text(game, platform, admitted, id,
        selected_language, translator,
        [](const AdmittedGameText& token) -> std::string_view { return token.id; });
}

LocalizedGameText localize_admitted_game_text_by_original(const Game game,
    const Platform platform, const std::span<const AdmittedGameText> admitted,
    const std::string_view original_text, const std::string_view selected_language,
    const Translator& translator) {
    return localize_unique_admitted_game_text(game, platform, admitted, original_text,
        selected_language, translator,
        [](const AdmittedGameText& token) -> std::string_view { return token.original_text; });
}

std::vector<LocalizedGameText> localize_admitted_game_text_table(
    const Game game, const Platform platform,
    const std::span<const AdmittedGameText> admitted,
    const std::string_view selected_language, const Translator& translator) {
    if (admitted.empty())
        throw std::runtime_error("Player-visible game-text table is empty");

    const auto& first = admitted.front();
    std::size_t preceding_end = 0;
    std::vector<LocalizedGameText> localized;
    localized.reserve(admitted.size());
    for (const auto& token : admitted) {
        if (token.source_leaf != first.source_leaf
            || token.source_sha256 != first.source_sha256
            || token.source_language != first.source_language) {
            throw std::runtime_error("Player-visible game-text table mixes original sources");
        }
        if (!localized.empty() && token.source_offset < preceding_end)
            throw std::runtime_error("Player-visible game-text table is reordered or overlaps");
        preceding_end = token.source_offset + token.source_size;
        localized.push_back(localize_admitted_game_text(
            game, platform, token, selected_language, translator));
    }
    return localized;
}

} // namespace eon
