#include "i18n.hpp"
#include "platform/resource_paths.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

int main(const int argc, const char* const argv[]) {
    assert(argc == 2);
    const auto root = std::filesystem::path(argv[1]) / "relocated-resource-test";
    std::filesystem::remove_all(root);
    const auto executable = root / "bin" / "project-eon";
    const auto asset = root / "share" / "project-eon" / "assets" / "cards" / "millennium.png";
    const auto font = root / "share" / "project-eon" / "assets" / "fonts" / "NotoSans-Regular.ttf";
    const auto locale = root / "share" / "project-eon" / "po" / "sv.po";
    std::filesystem::create_directories(asset.parent_path());
    std::filesystem::create_directories(font.parent_path());
    std::filesystem::create_directories(locale.parent_path());
    { std::ofstream(asset).put('x'); }
    { std::ofstream(font).put('x'); }
    {
        std::ofstream po(locale);
        po << "msgid \"RESOURCE PATH PROBE\"\n"
              "msgstr \"RESURS-S\u00d6KV\u00c4G\"\n";
    }

    const auto asset_paths = eon::launcher_asset_paths(executable, "cards", "millennium.png");
    assert(asset_paths[0] == executable.parent_path() / "assets/cards/millennium.png");
    assert(asset_paths[2] == root / "share/project-eon/assets/cards/millennium.png");
    assert(asset_paths[3].empty());
    assert(asset_paths[4] == std::filesystem::path("assets/cards/millennium.png"));
    assert(std::filesystem::is_regular_file(asset));
    assert(std::filesystem::is_regular_file(asset_paths[2]));

    const auto font_paths = eon::launcher_font_paths(executable);
    assert(font_paths[2] == root / "share/project-eon/assets/fonts");
    assert(font_paths[3].empty());
    assert(std::filesystem::is_regular_file(font));
    assert(std::filesystem::is_regular_file(font_paths[2] / "NotoSans-Regular.ttf"));
    const auto development_paths = eon::launcher_asset_paths(
        executable, "cards", "millennium.png", root / "checkout");
    assert(development_paths[3] == root / "checkout/assets/cards/millennium.png");

    const auto locale_paths = eon::launcher_locale_paths(executable);
    assert(locale_paths[1] == root / "share/project-eon/po");
    const auto translator = eon::Translator::from_language("sv", executable);
    assert(translator.translate("RESOURCE PATH PROBE") == "RESURS-SÖKVÄG");

    const auto app_executable = root / "ProjectEon.app/Contents/MacOS/ProjectEon";
    const auto app_asset = root / "ProjectEon.app/Contents/Resources/assets/cards/deuteros.png";
    std::filesystem::create_directories(app_asset.parent_path());
    { std::ofstream(app_asset).put('x'); }
    const auto app_paths = eon::launcher_asset_paths(app_executable, "cards", "deuteros.png");
    assert(app_paths[1] == app_asset);
    assert(std::filesystem::is_regular_file(app_paths[1]));
    const auto app_locale = root / "ProjectEon.app/Contents/Resources/po/sv.po";
    std::filesystem::create_directories(app_locale.parent_path());
    std::filesystem::copy_file(locale, app_locale);
    const auto app_translator = eon::Translator::from_language("sv", app_executable);
    assert(app_translator.translate("RESOURCE PATH PROBE") == "RESURS-SÖKVÄG");
    const auto app_font = root
        / "ProjectEon.app/Contents/Resources/assets/fonts/NotoSans-Regular.ttf";
    std::filesystem::create_directories(app_font.parent_path());
    std::filesystem::copy_file(font, app_font);
    const auto app_font_paths = eon::launcher_font_paths(app_executable);
    assert(app_font_paths[1] == app_font.parent_path());
    assert(std::filesystem::is_regular_file(app_font_paths[1] / "NotoSans-Regular.ttf"));

    const auto windows_executable = root / "Project Eon" / "project-eon.exe";
    const auto windows_asset = root / "Project Eon/assets/cards/millennium.png";
    const auto windows_font = root / "Project Eon/assets/fonts/NotoSans-Regular.ttf";
    const auto windows_locale = root / "Project Eon/po/sv.po";
    std::filesystem::create_directories(windows_asset.parent_path());
    std::filesystem::create_directories(windows_font.parent_path());
    std::filesystem::create_directories(windows_locale.parent_path());
    std::filesystem::copy_file(asset, windows_asset);
    std::filesystem::copy_file(font, windows_font);
    std::filesystem::copy_file(locale, windows_locale);
    const auto windows_asset_paths = eon::launcher_asset_paths(
        windows_executable, "cards", "millennium.png");
    const auto windows_font_paths = eon::launcher_font_paths(windows_executable);
    const auto windows_locale_paths = eon::launcher_locale_paths(windows_executable);
    assert(windows_asset_paths[0] == windows_asset);
    assert(windows_font_paths[0] == windows_font.parent_path());
    assert(windows_locale_paths[0] == windows_locale.parent_path());
    assert(eon::Translator::from_language("sv", windows_executable)
        .translate("RESOURCE PATH PROBE") == "RESURS-SÖKVÄG");

    std::filesystem::remove_all(root);
    std::cout << "Relocated resource lookup passed\n";
}
