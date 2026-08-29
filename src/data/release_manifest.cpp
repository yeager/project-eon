#include "data/release_manifest.hpp"

#include <array>

namespace eon {
namespace {

// Every outer fingerprint and profile span below was measured from the six
// supplied archives under Hämtningar/.  Keep this table compact: the durable,
// machine-readable companion in docs/release-manifest.json records the same
// provenance and is intended for independent preservation tooling.
constexpr std::array<ReleaseManifestEntry, 6> releases{{
    {"f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04", Game::deuteros, Platform::amiga, "en", 4'066'771},
    {"c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653", Game::deuteros, Platform::atari_st, "en", 3'021'682},
    {"2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400", Game::millennium, Platform::amiga, "en", 2'558'009},
    {"ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01", Game::millennium, Platform::atari_st, "en", 1'524'836},
    {"e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", Game::millennium, Platform::dos, "en", 328'383},
    {"b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4", Game::millennium, Platform::dos, "es", 330'050},
}};

constexpr std::array<ParserProfileManifestEntry, 31> profiles{{
    {"deuteros-amiga-clean-main-stage", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04", "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38", 901'120, 0x5800, 0x4200},
    {"deuteros-amiga-clean-title-handoff", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04", "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38", 901'120, 0x6e000, 0x6ca00},
    {"deuteros-amiga-bundle-0", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04", "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38", 901'120, 0x1b800, 0x2f3f4},
    {"deuteros-atari-replicants-first-stage", "c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653", "aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee", 737'280, 0x4ec00, 0x1200},
    {"deuteros-atari-killer-boot", "c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653", "5501ce3fd79c9b37cf695692a8012267db23dacd8a2cc64c0c7b7e4305971193", 737'280, 0x0, 0x200},
    {"millennium-amiga-defjam-bootstrap", "2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400", "8263e19b431b61c3c34363bb282703476145a45259c94132be82b529ec13b53c", 901'120, 0x400, 0x400},
    {"millennium-amiga-shared-resident", "2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400", "8263e19b431b61c3c34363bb282703476145a45259c94132be82b529ec13b53c", 901'120, 0x16400, 0x2c000},
    {"millennium-atari-equinox-bootstrap", "ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01", "3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7", 819'200, 0x0, 0x200},
    {"millennium-atari-equinox-root-inventory", "ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01", "3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7", 819'200, 0x0, 819'200},
    {"millennium-atari-equinox-prg-chain", "ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01", "3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7", 819'200, 0x0, 819'200},
    {"millennium-atari-equinox-config-chain", "ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01", "3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7", 819'200, 0x0, 819'200},
    {"millennium-atari-equinox-auxiliary-resource", "ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01", "3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7", 819'200, 0x0, 819'200},
    {"millennium-atari-physical-control-text", "ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01", "081d8bc102b8c7669c5cb21abace9b08532bc0b34164f11465d0c87b63a422fd", 423'696, 0x0, 423'696},
    {"millennium-dos-title-flow", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6", 7'022, 0x0, 7'022},
    {"millennium-dos-save-layout", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "a9b3d77534d3d575012f9553bfed9520edf92a83af408c977e7f0fd226a470e7", 9'538, 0x0, 9'538},
    {"millennium-dos-gx-canvas", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "4adf9991226deab4749ac07ad637851994f57d11f6dc45f3f5ce862b5bc34c2f", 312'748, 0x6, 3'461},
    {"millennium-dos-game-flow", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57", 54'391, 0x0, 54'391},
    {"millennium-dos-english-startup-callees", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57", 54'391, 0xd0a1, 48},
    {"millennium-dos-english-startup-followups", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57", 54'391, 0x356, 39},
    {"millennium-dos-static-data", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d", 12'494, 0x0, 12'494},
    {"millennium-dos-gx-overlay", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb", 46'634, 0x0, 46'634},
    {"millennium-dos-ega-video", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "ba003dd155fee868980f6ece933c33f9b22af68ed376cd64f4e027abd65baf6a", 4'632, 0x0, 4'632},
    {"millennium-dos-mcga-video", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228", 4'366, 0x0, 4'366},
    {"millennium-dos-last-screen", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "a3f5c0b447795881dd4cd5316a091ecc218b1bf563f567b6fe3f093f89781510", 18'117, 0x0, 18'117},
    {"millennium-dos-title-library", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678", 18'907, 0x0, 18'907},
    {"millennium-dos-launcher", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e", 1'445, 0x0, 1'445},
    {"millennium-dos-sfx1-voice", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "5f796a7fe8bcf5113a65087f76853061f8d96065f9a3cbe66b6c61303b677a88", 771, 0x0, 771},
    {"millennium-dos-spanish-startup", "b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4", "1cb7d399ab22110317b1c7486a575c00895f12a17268d0c984ac264a5695961d", 737'280, 0x0, 0x200},
    {"millennium-dos-spanish-title-boundary", "b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4", "1cb7d399ab22110317b1c7486a575c00895f12a17268d0c984ac264a5695961d", 737'280, 0x0, 737'280},
    {"millennium-dos-spanish-static-text", "b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4", "1cb7d399ab22110317b1c7486a575c00895f12a17268d0c984ac264a5695961d", 737'280, 0x0, 737'280},
    {"millennium-dos-spanish-launch-manual", "b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4", "1cb7d399ab22110317b1c7486a575c00895f12a17268d0c984ac264a5695961d", 737'280, 0x0, 737'280},
}};

} // namespace

std::span<const ReleaseManifestEntry> release_manifest() { return releases; }

std::span<const ParserProfileManifestEntry> parser_profile_manifest() { return profiles; }

bool release_has_parser_profile(std::string_view release_sha256, std::string_view profile_id) {
    for (const auto& profile : profiles) {
        if (profile.release_sha256 == release_sha256 && profile.id == profile_id) return true;
    }
    return false;
}

} // namespace eon
