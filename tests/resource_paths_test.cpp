#include "i18n.hpp"
#include "platform/resource_paths.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

int main(const int argc, const char* const argv[]) {
    assert(argc == 2);
    constexpr std::string_view translated_probe = "RESURS-S\xC3\x96KV\xC3\x84G";
    const auto root = std::filesystem::path(argv[1]) / "relocated-resource-test";
    std::filesystem::remove_all(root);
    const auto executable = root / "bin" / "project-eon";
    const auto asset = root / "share" / "project-eon" / "assets" / "cards" / "millennium.png";
    const auto font = root / "share" / "project-eon" / "assets" / "fonts" / "NotoSans-Regular.ttf";
    const auto locale = root / "share" / "project-eon" / "po" / "sv.po";
    std::filesystem::create_directories(executable.parent_path());
    std::filesystem::create_directories(asset.parent_path());
    std::filesystem::create_directories(font.parent_path());
    std::filesystem::create_directories(locale.parent_path());
    { std::ofstream(asset).put('x'); }
    { std::ofstream(font).put('x'); }
    {
        std::ofstream po(locale);
        po << "msgid \"RESOURCE PATH PROBE\"\nmsgstr \""
            << translated_probe << "\"\n";
    }

    const auto asset_paths = eon::launcher_asset_paths(executable, "cards", "millennium.png");
    assert(asset_paths[0] == executable.parent_path() / "assets/cards/millennium.png");
    assert(asset_paths[3].lexically_normal()
        == (root / "share/project-eon/assets/cards/millennium.png").lexically_normal());
    assert(asset_paths[4].empty());
    assert(asset_paths[5] == std::filesystem::path("assets/cards/millennium.png"));
    assert(std::filesystem::is_regular_file(asset));
    assert(std::filesystem::is_regular_file(asset_paths[3]));

    // SDL_GetBasePath returns the executable directory (not the executable
    // filename). Keep that form covered because the native macOS app bundle
    // places runtime assets beside Contents/MacOS/ProjectEon.
    const auto app_base = root / "ProjectEon.app/Contents/MacOS";
    const auto app_base_asset = app_base / "assets/cards/millennium.png";
    std::filesystem::create_directories(app_base_asset.parent_path());
    { std::ofstream(app_base_asset).put('x'); }
    const auto app_base_paths = eon::launcher_asset_paths(app_base, "cards", "millennium.png");
    assert(app_base_paths[0] == app_base_asset);
    assert(std::filesystem::is_regular_file(app_base_paths[0]));

    // On macOS and iOS SDL_GetBasePath returns the app's Resources directory.
    // The macOS desktop workflow stages renderer assets in Contents/MacOS.
    const auto app_resources = root / "ProjectEon.app/Contents/Resources";
    std::filesystem::create_directories(app_resources);
    const auto macos_asset = root
        / "ProjectEon.app/Contents/MacOS/assets/cards/millennium.png";
    std::filesystem::create_directories(macos_asset.parent_path());
    { std::ofstream(macos_asset).put('x'); }
    const auto macos_bundle_paths = eon::launcher_asset_paths(
        app_resources, "cards", "millennium.png");
    assert(macos_bundle_paths[1] == macos_asset);
    assert(std::filesystem::is_regular_file(macos_bundle_paths[1]));
    const auto sdl_resource_base = std::filesystem::path(app_resources.string() + "/");
    const auto sdl_base_paths = eon::launcher_asset_paths(
        sdl_resource_base, "cards", "millennium.png");
    assert(sdl_base_paths[1] == macos_asset);
    assert(std::filesystem::is_regular_file(sdl_base_paths[1]));
    const auto macos_fonts = root
        / "ProjectEon.app/Contents/MacOS/assets/fonts/NotoSans-Regular.ttf";
    std::filesystem::create_directories(macos_fonts.parent_path());
    { std::ofstream(macos_fonts).put('x'); }
    const auto macos_bundle_font_paths = eon::launcher_font_paths(app_resources);
    assert(macos_bundle_font_paths[1] == macos_fonts.parent_path());
    const auto sdl_font_paths = eon::launcher_font_paths(sdl_resource_base);
    assert(sdl_font_paths[1] == macos_fonts.parent_path());

    const auto font_paths = eon::launcher_font_paths(executable);
    assert(font_paths[3].lexically_normal()
        == (root / "share/project-eon/assets/fonts").lexically_normal());
    assert(font_paths[4].empty());
    assert(std::filesystem::is_regular_file(font));
    assert(std::filesystem::is_regular_file(font_paths[3] / "NotoSans-Regular.ttf"));
    const auto development_paths = eon::launcher_asset_paths(
        executable, "cards", "millennium.png", root / "checkout");
    assert(development_paths[4] == root / "checkout/assets/cards/millennium.png");

    const auto locale_paths = eon::launcher_locale_paths(executable);
    assert(locale_paths[1].lexically_normal()
        == (root / "share/project-eon/po").lexically_normal());
    const auto translator = eon::Translator::from_language("sv", executable);
    assert(translator.translate("RESOURCE PATH PROBE") == translated_probe);

    const auto app_executable = root / "ProjectEon.app/Contents/MacOS/ProjectEon";
    const auto app_asset = root / "ProjectEon.app/Contents/Resources/assets/cards/deuteros.png";
    std::filesystem::create_directories(app_asset.parent_path());
    { std::ofstream(app_asset).put('x'); }
    const auto app_paths = eon::launcher_asset_paths(app_executable, "cards", "deuteros.png");
    assert(app_paths[2] == app_asset);
    assert(std::filesystem::is_regular_file(app_paths[2]));
    const auto app_locale = root / "ProjectEon.app/Contents/Resources/po/sv.po";
    std::filesystem::create_directories(app_locale.parent_path());
    std::filesystem::copy_file(locale, app_locale);
    const auto app_translator = eon::Translator::from_language("sv", app_executable);
    assert(app_translator.translate("RESOURCE PATH PROBE") == translated_probe);
    const auto app_font = root
        / "ProjectEon.app/Contents/Resources/assets/fonts/NotoSans-Regular.ttf";
    std::filesystem::create_directories(app_font.parent_path());
    std::filesystem::copy_file(font, app_font);
    const auto app_font_paths = eon::launcher_font_paths(app_executable);
    assert(app_font_paths[2] == app_font.parent_path());
    assert(std::filesystem::is_regular_file(app_font_paths[2] / "NotoSans-Regular.ttf"));

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
        .translate("RESOURCE PATH PROBE") == translated_probe);

    std::filesystem::remove_all(root);
    std::cout << "Relocated resource lookup passed\n";
}
