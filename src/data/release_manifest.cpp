#include "data/release_manifest.hpp"

#include "data/sha256.hpp"

#include <algorithm>
#include <array>

namespace eon {
namespace {

// Every outer fingerprint and profile span below was measured from the six
// supplied archives in the local Downloads directory. Keep this table compact:
// the durable,
// machine-readable companion in docs/release-manifest.json records the same
// provenance and is intended for independent preservation tooling.
constexpr std::array<ReleaseManifestEntry, 8> releases{{
    {"0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69", Game::millennium, Platform::atari_st, "en", 299'516},
    {"f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04", Game::deuteros, Platform::amiga, "en", 4'066'771},
    {"c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653", Game::deuteros, Platform::atari_st, "en", 3'021'682},
    {"2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400", Game::millennium, Platform::amiga, "en", 2'558'009},
    {"ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01", Game::millennium, Platform::atari_st, "en", 1'524'836},
    {"e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", Game::millennium, Platform::dos, "en", 328'383},
    {"b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4", Game::millennium, Platform::dos, "es", 330'050},
    {"ec0424445d494809d2661492e289af71b056a429dde13b053a472ccc8347d4dd", Game::millennium, Platform::amiga, "en", 425'912},
}};

constexpr std::array<DirectMediaSetMember, 31> millennium_dos_en_installed{{
    {"2200AD.EXE", 54'391, "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57"},
    {"2200AD4.BIN", 12'494, "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d"},
    {"2200GX.EXE", 46'634, "093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb"},
    {"2200SAVE.I", 9'538, "a9b3d77534d3d575012f9553bfed9520edf92a83af408c977e7f0fd226a470e7"},
    {"EG6TXT.BIN", 2'040, "063eefa58c98360d0ca2b4eaf9a77f8f9d13c619aee605dff6b0d0ee8b4a6b20"},
    {"EGA640.BIN", 4'632, "ba003dd155fee868980f6ece933c33f9b22af68ed376cd64f4e027abd65baf6a"},
    {"GX.LIB", 312'748, "4adf9991226deab4749ac07ad637851994f57d11f6dc45f3f5ce862b5bc34c2f"},
    {"LAST.LIB", 18'117, "a3f5c0b447795881dd4cd5316a091ecc218b1bf563f567b6fe3f093f89781510"},
    {"MCGA.BIN", 4'366, "bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228"},
    {"MILL.COM", 1'445, "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e"},
    {"SCVX.DRV", 4'053, "99e110b91534206a6b83680a3e11cceadd0e5ddf863560aed53dcbd2c49df7c4"},
    {"SFX1.VOC", 771, "5f796a7fe8bcf5113a65087f76853061f8d96065f9a3cbe66b6c61303b677a88"},
    {"SFX2.VOC", 514, "7da8ec44f635e5968a7d909a63c2539991a720718ef325a65231ebbe52d3aed3"},
    {"SFX3.VOC", 511, "95d2029cd015023057a3b911e2683b6612cf13aaad232e701dddd1713aa0126f"},
    {"SFX4.VOC", 3'938, "3d814191f3d91bb7f9ba8788782c1772e4327310b057a97ee96ce8971b030a66"},
    {"SFX5.VOC", 4'514, "3e87ca7997f0f8001ab575203c0878222eabfcdca7582c6fbd9c83ea3a85c11b"},
    {"SFX6.VOC", 4'130, "834bc620d70aea606164c17758bb98aafc7eb43f3f12d62e5e4fea96b02771ed"},
    {"SFX7.VOC", 2'850, "934d6607b2b042cad283dfd347efae1756fdeecb046a267b70ffe5f8605fd7e0"},
    {"SFX8.VOC", 2'850, "073576f03b20a037469b7e40b380b9f592218db9edfd3ad5e0fa6dcc9398bf7d"},
    {"SFX9.VOC", 1'634, "14561107b7d2cdb45ec3a5664654a9ba16f4b77ad821ae42d470b82f350b5e63"},
    {"SFXA.VOC", 2'146, "5e3aab196c2199de9e045610cf1e5094e04619e78e2f27166d0b10a28a2b65c3"},
    {"SFXB.VOC", 3'554, "e534ebc67c441b9eb6a944e8ddcb10042e22703847bc869bc2946a76f1157234"},
    {"SFXC.VOC", 2'530, "a0b87f1e6e8038505cb67d3b0a730fe8e1307921ff816ebce2c92acc462fdca6"},
    {"SFXD.VOC", 4'066, "004b74bfa6569f2bb86bd63f2c6e6bd97f692af8358591506c1745f174cf66f1"},
    {"SFXE.VOC", 3'554, "a7f1984ff031b451262eb9f201080dd340a8fe0896f3bd57440c2e1c1c839ea4"},
    {"SIBM.DRV", 2'871, "f3224caa43c1149907f852fa98816ed68c489b70f1ba795592d684d4e51f31b1"},
    {"SSBL.DRV", 9'194, "be5a00e0b71d893a3aeaaa1127b1e5b870fe734dc876e636c6a933b6444f1b72"},
    {"TITLE.LIB", 18'907, "6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678"},
    {"TITLES.EXE", 7'022, "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6"},
    {"VGA.BIN", 5'336, "a0483497aece13a001e405b6524388f8e35ad13174a36388ce6fee7eddf4fcf4"},
    {"VGATXT.BIN", 1'024, "c31cb760d5f62a21b3baf9c09a6be413514780bd88eeac0273620e81b5d69318"},
}};

constexpr std::array<DirectMediaSetManifestEntry, 1> direct_media_sets{{
    {"d938cd6a611a83897a745b257a371613b73a7dddffb2d336ec2167a192803783",
        "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
        Game::millennium, Platform::dos, "en", millennium_dos_en_installed},
}};

// Canonical set digest input is the ordered lexical sequence
// `outer-sha256<TAB>outer-size<TAB>leaf-sha256<TAB>leaf-size<LF>`.
// The order is semantic: it is the documented disk order, never directory
// order or a filename inference.
constexpr std::array<ContainerSetMember, 2> deuteros_atari_split_disks{{
    {"a9318feb83ff34b79f5a5ea1e5ffcb45828e4432ac75a859f55c3de97d724c93", 292'448,
        "aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee", 737'280},
    {"7842adb599dbc4cf79827e31e912740f259af45718c124d5806e1c8860f2253d", 264'245,
        "5501ce3fd79c9b37cf695692a8012267db23dacd8a2cc64c0c7b7e4305971193", 737'280},
}};

constexpr std::array<ContainerSetMember, 2> deuteros_amiga_clean_split_disks{{
    {"7ecaa0457ad2b61b417bbe62943a4a11b4d164acfbc5a5097e95f8f7d1360533", 449'666,
        "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38", 901'120},
    {"b98ee3c36141773485c5e03dd8bb4aa59784eaf08a1363fa6a2951a5eb5fdc0a", 490'962,
        "99909db1e190be02e049084743af44f00e331be6bf2d97b4831ada5fe4c30b4a", 901'120},
}};

constexpr std::array<ContainerSetManifestEntry, 2> container_sets{{
    {"0a87871cdfc6e0f11c598b86be0726c842c2cdcb1cb7d0dba651f1d43b835ffa", "c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653",
        Game::deuteros, Platform::atari_st, "en", deuteros_atari_split_disks},
    {"3d5dc5cf605f5b19a1ba42038321d79f9e4d35d3e56f7e4de90d8f732d8a8c45", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
        Game::deuteros, Platform::amiga, "en", deuteros_amiga_clean_split_disks},
}};

constexpr std::array<ParserProfileManifestEntry, 42> profiles{{
    {"millennium-atari-equinox-direct-bootstrap", "0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69", "3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7", 819'200, 0x0, 0x200},
    {"millennium-atari-equinox-direct-root-inventory", "0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69", "3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7", 819'200, 0x0, 819'200},
    {"millennium-atari-equinox-direct-prg-chain", "0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69", "3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7", 819'200, 0x0, 819'200},
    {"millennium-atari-equinox-direct-config-chain", "0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69", "3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7", 819'200, 0x0, 819'200},
    {"millennium-atari-equinox-direct-auxiliary-resource", "0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69", "3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7", 819'200, 0x0, 819'200},
    {"millennium-atari-direct-physical-control-text", "0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69", "081d8bc102b8c7669c5cb21abace9b08532bc0b34164f11465d0c87b63a422fd", 423'696, 0x0, 423'696},
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
    {"millennium-dos-sound-selection", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e", 1'445, 0x411, 100},
    {"millennium-dos-sound-blaster-driver", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "be5a00e0b71d893a3aeaaa1127b1e5b870fe734dc876e636c6a933b6444f1b72", 9'194, 0x0, 9'194},
    {"millennium-dos-covox-driver", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "99e110b91534206a6b83680a3e11cceadd0e5ddf863560aed53dcbd2c49df7c4", 4'053, 0x0, 4'053},
    {"millennium-dos-sfx1-voice", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "5f796a7fe8bcf5113a65087f76853061f8d96065f9a3cbe66b6c61303b677a88", 771, 0x0, 771},
    {"millennium-dos-spanish-startup", "b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4", "1cb7d399ab22110317b1c7486a575c00895f12a17268d0c984ac264a5695961d", 737'280, 0x0, 0x200},
    {"millennium-dos-spanish-title-boundary", "b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4", "1cb7d399ab22110317b1c7486a575c00895f12a17268d0c984ac264a5695961d", 737'280, 0x0, 737'280},
    {"millennium-dos-spanish-static-text", "b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4", "1cb7d399ab22110317b1c7486a575c00895f12a17268d0c984ac264a5695961d", 737'280, 0x0, 737'280},
    {"millennium-dos-spanish-launch-manual", "b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4", "1cb7d399ab22110317b1c7486a575c00895f12a17268d0c984ac264a5695961d", 737'280, 0x0, 737'280},
    {"millennium-amiga-defjam-direct-bootstrap", "ec0424445d494809d2661492e289af71b056a429dde13b053a472ccc8347d4dd", "8263e19b431b61c3c34363bb282703476145a45259c94132be82b529ec13b53c", 901'120, 0x400, 0x400},
    {"millennium-amiga-direct-shared-resident", "ec0424445d494809d2661492e289af71b056a429dde13b053a472ccc8347d4dd", "8263e19b431b61c3c34363bb282703476145a45259c94132be82b529ec13b53c", 901'120, 0x16400, 0x2c000},
}};

} // namespace

std::span<const ReleaseManifestEntry> release_manifest() { return releases; }

std::span<const DirectMediaSetManifestEntry> direct_media_set_manifest() { return direct_media_sets; }

bool direct_media_set_manifest_is_valid() {
    for (const auto& set : direct_media_sets) {
        if (set.set_sha256.empty() || set.content_release_sha256.empty() || set.members.empty()) return false;
        const auto release_matches = std::count_if(releases.begin(), releases.end(), [&set](const auto& release) {
            return release.sha256 == set.content_release_sha256 && release.game == set.game
                && release.platform == set.platform && release.language == set.language;
        });
        if (release_matches != 1) return false;
        const auto set_matches = std::count_if(direct_media_sets.begin(), direct_media_sets.end(), [&set](const auto& candidate) {
            return candidate.set_sha256 == set.set_sha256;
        });
        if (set_matches != 1) return false;
        for (const auto& member : set.members) {
            if (member.name.empty() || member.size == 0 || member.sha256.empty()) return false;
            const auto name_matches = std::count_if(set.members.begin(), set.members.end(), [&member](const auto& candidate) {
                return candidate.name == member.name;
            });
            if (name_matches != 1) return false;
        }
    }
    return true;
}

std::span<const ContainerSetManifestEntry> container_set_manifest() { return container_sets; }

bool container_set_manifest_is_valid() {
    for (const auto& set : container_sets) {
        if (set.set_sha256.empty() || set.content_release_sha256.empty() || set.members.empty()) return false;
        if (std::count_if(releases.begin(), releases.end(), [&set](const auto& release) {
                return release.sha256 == set.content_release_sha256 && release.game == set.game
                    && release.platform == set.platform && release.language == set.language;
            }) != 1) return false;
        if (std::count_if(container_sets.begin(), container_sets.end(), [&set](const auto& candidate) {
                return candidate.set_sha256 == set.set_sha256;
            }) != 1) return false;
        for (const auto& member : set.members) {
            if (member.outer_sha256.empty() || member.outer_size == 0 || member.leaf_sha256.empty()
                || member.leaf_size == 0) return false;
            if (std::count_if(set.members.begin(), set.members.end(), [&member](const auto& candidate) {
                    return candidate.outer_sha256 == member.outer_sha256;
                }) != 1) return false;
        }
        std::string canonical;
        for (const auto& member : set.members) {
            canonical += std::string(member.outer_sha256) + "\t" + std::to_string(member.outer_size) + "\t"
                + std::string(member.leaf_sha256) + "\t" + std::to_string(member.leaf_size) + "\n";
        }
        const auto bytes = std::span(reinterpret_cast<const std::uint8_t*>(canonical.data()), canonical.size());
        if (to_hex(sha256(bytes)) != set.set_sha256) return false;
    }
    return true;
}

std::span<const ParserProfileManifestEntry> parser_profile_manifest() { return profiles; }

bool release_has_parser_profile(std::string_view release_sha256, std::string_view profile_id) {
    for (const auto& profile : profiles) {
        if (profile.release_sha256 == release_sha256 && profile.id == profile_id) return true;
    }
    return false;
}

} // namespace eon
