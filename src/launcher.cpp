#include "launcher.hpp"
#include "i18n.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <string_view>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace eon {
namespace {

std::optional<Game> parse_game(std::string_view value) {
    if (value == "millennium") return Game::millennium;
    if (value == "deuteros") return Game::deuteros;
    return std::nullopt;
}

std::optional<Platform> parse_platform(std::string_view value) {
    if (value == "dos") return Platform::dos;
    if (value == "amiga") return Platform::amiga;
    if (value == "atari-st") return Platform::atari_st;
    return std::nullopt;
}

bool is_sha256(const std::string_view value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](const unsigned char byte) {
        return (byte >= '0' && byte <= '9') || (byte >= 'a' && byte <= 'f');
    });
}

bool parse_display_resolution(const std::string_view value, DisplayPreferences& display) {
    if (value == "1280x720") display = {1280, 720, display.aspect_ratio_index};
    else if (value == "1600x900") display = {1600, 900, display.aspect_ratio_index};
    else if (value == "1920x1080") display = {1920, 1080, display.aspect_ratio_index};
    else return false;
    return true;
}

bool parse_display_aspect(const std::string_view value, DisplayPreferences& display) {
    if (value == "original") display.aspect_ratio_index = 0;
    else if (value == "square-pixels") display.aspect_ratio_index = 1;
    else if (value == "widescreen") display.aspect_ratio_index = 2;
    else return false;
    return true;
}

std::filesystem::path default_data_directory(const char* executable_path) {
#ifdef _WIN32
    std::error_code error;
    const auto absolute_executable = std::filesystem::absolute(executable_path, error);
    if (!error) return absolute_executable.parent_path() / "data";
    return std::filesystem::path("data");
#else
    static_cast<void>(executable_path);
    if (const auto* home = std::getenv("HOME"); home && *home) {
#if defined(__APPLE__) && TARGET_OS_IPHONE
        // IPA payloads deliberately contain no game data. iPadOS therefore
        // uses a Files-visible, non-hidden user-media location, without
        // creating it during this read-only lookup.
        return std::filesystem::path(home) / "Documents" / "ProjectEon";
#else
        return std::filesystem::path(home) / ".projecteon";
#endif
    }
#if defined(__APPLE__) && TARGET_OS_IPHONE
    return std::filesystem::path("Documents") / "ProjectEon";
#else
    return std::filesystem::path(".projecteon");
#endif
#endif
}

} // namespace

std::size_t LauncherCardFocus::current(const LauncherPage page) const {
    switch (page) {
    case LauncherPage::games: return game;
    case LauncherPage::platforms: return platform;
    case LauncherPage::releases: return release;
    case LauncherPage::profiles: return profile;
    }
    return 0;
}

void LauncherCardFocus::set(const LauncherPage page, const std::size_t count,
    const std::size_t index) {
    if (count == 0) return;
    const auto bounded = std::min(index, count - 1U);
    switch (page) {
    case LauncherPage::games: game = bounded; break;
    case LauncherPage::platforms: platform = bounded; break;
    case LauncherPage::releases: release = bounded; break;
    case LauncherPage::profiles: profile = bounded; break;
    }
}

void LauncherCardFocus::move(const LauncherPage page, const std::size_t count,
    const int direction) {
    if (count == 0 || direction == 0) return;
    // A scanner update can shrink a page between input events. Bound the
    // prior presentation index before wrapping so movement is defined by the
    // cards that are currently rendered, never by a stale, removed card.
    const auto index = std::min(current(page), count - 1U);
    set(page, count, direction < 0 ? (index + count - 1U) % count : (index + 1U) % count);
}

void LauncherCardFocus::first(const LauncherPage page, const std::size_t count) {
    set(page, count, 0);
}

void LauncherCardFocus::last(const LauncherPage page, const std::size_t count) {
    if (count != 0) set(page, count, count - 1U);
}

void LauncherCardFocus::reset_after_game_change() {
    platform = 0;
    release = 0;
    profile = 0;
}

void LauncherCardFocus::reset_after_platform_change() {
    release = 0;
    profile = 0;
}

std::string usage() {
    return
        "Usage:\n"
        "  project-eon [--data|--data-dir <directory-or-archive>]\n"
        "  project-eon [--data|--data-dir <directory-or-archive>] --game millennium|deuteros\n"
        "               --platform dos|amiga|atari-st\n"
        "               [--release-language en|es] [--release-sha256 <64-lowercase-hex>]\n"
        "               [--presentation original|modern] [--modern-pack <pack.eonmodern>]\n\n"
        "               [--launch-check]\n\n"
        "               [--launch-check-json]\n\n"
        "               [--resolution 1280x720|1600x900|1920x1080]\n"
        "               [--aspect original|square-pixels|widescreen]\n\n"
        "               [--language <language>]\n\n"
        "  project-eon [--data|--data-dir <directory-or-archive>] --verify-data millennium|deuteros\n\n"
        "  project-eon --data <directory-or-archive> --game millennium|deuteros\n"
        "               --platform dos|amiga|atari-st --reference-trace <manifest.eontrace>\n"
        "               [--reference-trace-json]\n\n"
        "  project-eon [--data|--data-dir <directory-or-archive>] --inspect\n"
        "               [--game millennium|deuteros] [--platform dos|amiga|atari-st]\n"
        "               [--release-language en|es]\n"
        "               [--inventory]\n"
        "               [--modern-packs <explicit-pack-root>]\n\n"
        "  project-eon [--data|--data-dir <directory-or-archive>] --inspect-json\n"
        "               [--game millennium|deuteros] [--platform dos|amiga|atari-st]\n\n"
        "  project-eon --inspect-save <2200SAVE.I|verified Millennium DOS archive>\n\n"
#if defined(__APPLE__) && TARGET_OS_IPHONE
        "Without --data/--data-dir, iPadOS reads user-supplied media from Documents/ProjectEon.\n"
#else
        "Without --data/--data-dir, game data is read from ~/.projecteon on Linux/macOS\n"
        "or <install directory>/data on Windows.\n"
#endif
        "Without --game, the graphical\n"
        "start menu is shown.\n";
}

ParseResult parse_command_line(int argc, char** argv) {
    LaunchRequest request;
    request.data_directory = default_data_directory(argc > 0 ? argv[0] : "project-eon");
    // The launcher must be deterministic and English-first when the user has
    // not selected a language. Host locale is not a presentation preference:
    // it would make the same command/menu route produce different UI text on
    // different machines and contradict the documented English default.
    request.language = "en";
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--help" || argument == "-h") return {{}, {}, true};
        if (argument == "--inspect") {
            request.inspect_data = true;
            continue;
        }
        if (argument == "--inspect-json") {
            request.inspect_data = true;
            request.inspect_json = true;
            continue;
        }
        if (argument == "--inventory") {
            request.inventory_assets = true;
            continue;
        }
        if (argument == "--launch-check") {
            request.launch_check = true;
            continue;
        }
        if (argument == "--launch-check-json") {
            request.launch_check = true;
            request.launch_check_json = true;
            continue;
        }
        if (argument == "--reference-trace-json") {
            request.reference_trace_json = true;
            continue;
        }
        if (index + 1 >= argc) return {{}, "Missing value for " + std::string(argument), false};
        const std::string_view value = argv[++index];
        if (argument == "--data" || argument == "--data-dir") {
            request.data_directory = value;
            request.data_directory_is_default = false;
        } else if (argument == "--game") {
            request.game = parse_game(value);
            if (!request.game) return {{}, "Unknown game: " + std::string(value), false};
        } else if (argument == "--verify-data") {
            request.verify_game = parse_game(value);
            if (!request.verify_game) return {{}, "Unknown game: " + std::string(value), false};
        } else if (argument == "--reference-trace") {
            request.reference_trace = std::filesystem::path(value);
        } else if (argument == "--inspect-save") {
            request.inspect_save = std::filesystem::path(value);
        } else if (argument == "--modern-packs") {
            request.modern_pack_root = std::filesystem::path(value);
        } else if (argument == "--modern-pack") {
            request.modern_pack_manifest = std::filesystem::path(value);
        } else if (argument == "--platform") {
            request.platform = parse_platform(value);
            if (!request.platform) return {{}, "Unknown platform: " + std::string(value), false};
        } else if (argument == "--release-language") {
            if (value != "en" && value != "es") {
                return {{}, "Unknown original release language: " + std::string(value), false};
            }
            request.release_language = std::string(value);
        } else if (argument == "--release-sha256") {
            if (!is_sha256(value)) {
                return {{}, "--release-sha256 must be 64 lowercase hexadecimal characters", false};
            }
            request.release_sha256 = std::string(value);
        } else if (argument == "--presentation") {
            if (value == "original") request.presentation = Presentation::original;
            else if (value == "modern") request.presentation = Presentation::modern;
            else return {{}, "Unknown presentation: " + std::string(value), false};
            request.presentation_explicit = true;
        } else if (argument == "--resolution") {
            if (!parse_display_resolution(value, request.display)) {
                return {{}, "Unsupported resolution: " + std::string(value), false};
            }
            request.display_resolution_explicit = true;
        } else if (argument == "--aspect") {
            if (!parse_display_aspect(value, request.display)) {
                return {{}, "Unknown aspect ratio: " + std::string(value), false};
            }
            request.display_aspect_explicit = true;
        } else if (argument == "--language" || argument == "-l") {
            request.language = normalize_language(value);
            if (request.language.empty()) return {{}, "Unknown language: " + std::string(value), false};
        } else {
            return {{}, "Unknown option: " + std::string(argument), false};
        }
    }
    if (request.game && request.verify_game) {
        return {{}, "--game and --verify-data cannot be combined", false};
    }
    if (request.inspect_save && (!request.data_directory_is_default || request.game || request.verify_game || request.inspect_data
        || request.reference_trace || request.reference_trace_json || request.modern_pack_root || request.modern_pack_manifest
        || request.platform || request.release_language || request.release_sha256
        || request.presentation_explicit || request.launch_check || request.launch_check_json)) {
        return {{}, "--inspect-save is standalone; it never selects game data, a release, or runtime", false};
    }
    if (request.verify_game && request.inspect_data) {
        return {{}, "--verify-data and --inspect cannot be combined", false};
    }
    if (request.launch_check && (request.verify_game || request.inspect_data || request.reference_trace)) {
        return {{}, "--launch-check cannot be combined with inspection, verification, or reference traces", false};
    }
    if (request.inventory_assets && !request.inspect_data) {
        return {{}, "--inventory requires --inspect; it is a read-only preservation report", false};
    }
    if (request.inspect_json && (request.inventory_assets || request.modern_pack_root)) {
        return {{}, "--inspect-json reports release-level diagnostics only; do not combine it with --inventory or --modern-packs", false};
    }
    if (request.modern_pack_root && !request.inspect_data) {
        return {{}, "--modern-packs requires --inspect; it is diagnostics-only and never selects a renderer pack", false};
    }
    if (request.modern_pack_manifest && (request.inspect_data || request.presentation != Presentation::modern
        || !request.game || !request.platform)) {
        return {{}, "--modern-pack requires --game, --platform, and --presentation modern; it cannot be used with --inspect", false};
    }
    // Every current renderer mapping is bound to a hash-identified English
    // release. Refuse non-English selection before any SDL or pack I/O instead
    // of letting external art become an optional cross-edition fallback.
    if (request.modern_pack_manifest && request.release_language
        && *request.release_language != "en") {
        return {{}, "--modern-pack currently supports only --release-language en; no cross-edition art fallback is permitted", false};
    }
    if (request.reference_trace) {
        if (request.data_directory_is_default) {
            return {{}, "--reference-trace requires an explicit --data or --data-dir path", false};
        }
        if (!request.game || !request.platform) {
            return {{}, "--reference-trace requires both --game and --platform", false};
        }
        if (request.verify_game || request.inspect_data) {
            return {{}, "--reference-trace cannot be combined with --verify-data or --inspect", false};
        }
    }
    if (request.reference_trace_json && !request.reference_trace) {
        return {{}, "--reference-trace-json requires --reference-trace", false};
    }
    if (request.platform && !request.game) return {{}, "--platform requires --game", false};
    if ((request.release_language || request.release_sha256) && (!request.game || !request.platform)) {
        return {{}, "--release-language and --release-sha256 require both --game and --platform", false};
    }
    // The graphical flow records a platform card before a profile can start
    // a game. A direct CLI launch needs the same unambiguous release choice:
    // never let an omitted flag choose a different platform's recovered path.
    // Inspection remains a filter and therefore may name a game alone.
    if (request.game && !request.inspect_data && !request.platform) {
        return {{}, "--game requires --platform for a direct launch; use --inspect to list verified platforms", false};
    }
    if (request.launch_check && (!request.game || !request.platform)) {
        return {{}, "--launch-check requires both --game and --platform", false};
    }
    return {request, {}, false};
}

bool release_available(
    const std::vector<ReleaseArchive>& releases,
    Game game,
    std::optional<Platform> platform) {
    for (const auto& release : releases) {
        if (release.game == game && (!platform || release.platform == *platform)) return true;
    }
    return false;
}

PlatformCardStatus platform_card_status(
    const std::vector<ReleaseArchive>& releases, const Game game, const Platform platform) {
    const auto identities = available_release_identities(releases, game, platform);
    if (identities.empty()) return PlatformCardStatus::unavailable;
    return select_available_release_sha256(releases, game, platform, std::nullopt)
        ? PlatformCardStatus::ready : PlatformCardStatus::release_selection_required;
}

bool platform_card_selectable(const PlatformCardStatus status) {
    return status != PlatformCardStatus::unavailable;
}

bool platform_card_startable(const PlatformCardStatus status) {
    return status == PlatformCardStatus::ready;
}

std::vector<Platform> supported_platforms(const Game game) {
    // Keep this declaration separate from available_platforms(): the latter
    // answers what immutable releases the user has supplied, while this
    // answers which original targets Project Eon recognises for this game.
    switch (game) {
    case Game::millennium:
        return {Platform::dos, Platform::amiga, Platform::atari_st};
    case Game::deuteros:
        return {Platform::amiga, Platform::atari_st};
    }
    return {};
}

std::vector<Platform> available_platforms(
    const std::vector<ReleaseArchive>& releases, const Game game) {
    std::vector<Platform> platforms;
    for (const auto platform : {Platform::dos, Platform::amiga, Platform::atari_st}) {
        if (release_available(releases, game, platform)) platforms.push_back(platform);
    }
    return platforms;
}

std::vector<std::string> available_release_languages(
    const std::vector<ReleaseArchive>& releases, const Game game, const Platform platform) {
    std::vector<std::string> languages;
    for (const auto& release : releases) {
        if (release.game != game || release.platform != platform) continue;
        if (std::find(languages.begin(), languages.end(), release.language) == languages.end()) {
            languages.push_back(release.language);
        }
    }
    std::sort(languages.begin(), languages.end());
    return languages;
}

std::optional<std::string> select_available_release_language(
    const std::vector<ReleaseArchive>& releases, const Game game, const Platform platform,
    const std::optional<std::string>& current) {
    const auto languages = available_release_languages(releases, game, platform);
    if (languages.empty()) return std::nullopt;
    if (current && std::find(languages.begin(), languages.end(), *current) != languages.end()) {
        return current;
    }
    // English is the stable default when it is installed. This choice is
    // scoped to the already selected game/platform and remains an exact
    // hash-verified release identity, not a UI-locale inference or fallback.
    if (std::find(languages.begin(), languages.end(), "en") != languages.end()) return "en";
    return languages.size() == 1 ? std::optional<std::string>{languages.front()} : std::nullopt;
}

std::vector<ReleaseArchive> available_release_identities(
    const std::vector<ReleaseArchive>& releases, const Game game, const Platform platform) {
    std::vector<ReleaseArchive> identities;
    for (const auto& release : releases) {
        if (release.game == game && release.platform == platform) identities.push_back(release);
    }
    std::sort(identities.begin(), identities.end(), [](const auto& left, const auto& right) {
        return left.sha256 < right.sha256;
    });
    return identities;
}

std::optional<std::string> select_available_release_sha256(
    const std::vector<ReleaseArchive>& releases, const Game game, const Platform platform,
    const std::optional<std::string>& current) {
    const auto identities = available_release_identities(releases, game, platform);
    if (identities.empty()) return std::nullopt;
    if (current && std::any_of(identities.begin(), identities.end(), [&](const auto& release) {
        return release.sha256 == *current;
    })) return current;
    std::vector<std::string> english;
    for (const auto& release : identities) {
        if (release.language == "en") english.push_back(release.sha256);
    }
    if (english.size() == 1) return english.front();
    return identities.size() == 1 ? std::optional<std::string>{identities.front().sha256}
                                  : std::nullopt;
}

std::optional<ReleaseArchive> resolve_release_identity(
    const std::vector<ReleaseArchive>& releases, const Game game, const Platform platform,
    const std::optional<std::string>& requested_sha256,
    const std::optional<std::string>& requested_language) {
    const auto identities = available_release_identities(releases, game, platform);
    const auto selected_sha256 = requested_sha256
        ? requested_sha256 : select_available_release_sha256(releases, game, platform, std::nullopt);
    if (!selected_sha256) return std::nullopt;
    const auto match = std::find_if(identities.begin(), identities.end(), [&](const auto& release) {
        return release.sha256 == *selected_sha256;
    });
    if (match == identities.end() || (requested_language && match->language != *requested_language)) {
        return std::nullopt;
    }
    return *match;
}

std::optional<ResolvedLaunchRequest> resolve_launch_request_identity(
    const LaunchRequest& candidate, const std::vector<ReleaseArchive>& releases) {
    if (!candidate.game || !candidate.platform) return std::nullopt;
    const auto release = resolve_release_identity(releases, *candidate.game, *candidate.platform,
        candidate.release_sha256, candidate.release_language);
    if (!release) return std::nullopt;

    auto resolved = candidate;
    // These values are copied from one ReleaseArchive only after the exact
    // identity resolver has checked all four fields together.  In particular,
    // a language card cannot leave a stale outer-container hash behind.
    resolved.game = release->game;
    resolved.platform = release->platform;
    resolved.release_language = release->language;
    resolved.release_sha256 = release->sha256;
    return {{std::move(resolved), std::move(*release)}};
}

void LauncherRouteState::focus_game(const std::vector<ReleaseArchive>& releases,
    const Game next_game) {
    const auto prior_platform = platform;
    game = next_game;
    platform = select_available_platform(releases, game, platform);
    if (platform != prior_platform) {
        release_language.reset();
        release_sha256.reset();
    }
}

bool LauncherRouteState::choose_platform(const std::vector<ReleaseArchive>& releases,
    const Platform next_platform) {
    if (!platform_card_selectable(platform_card_status(releases, game, next_platform))) return false;
    if (platform != next_platform) {
        platform = next_platform;
        release_language.reset();
        release_sha256.reset();
    }
    release_sha256 = select_available_release_sha256(releases, game, *platform, release_sha256);
    if (const auto release = resolve_release_identity(releases, game, *platform,
            release_sha256, std::nullopt)) {
        release_language = release->language;
    } else {
        release_language.reset();
    }
    page = release_sha256 ? LauncherPage::profiles : LauncherPage::releases;
    return true;
}

bool LauncherRouteState::choose_release(const std::vector<ReleaseArchive>& releases,
    const std::string_view next_sha256) {
    if (!platform) return false;
    const auto release = resolve_release_identity(releases, game, *platform,
        std::string(next_sha256), std::nullopt);
    if (!release) return false;
    release_sha256 = release->sha256;
    release_language = release->language;
    page = LauncherPage::profiles;
    return true;
}

void LauncherRouteState::enter_platforms() { page = LauncherPage::platforms; }

void LauncherRouteState::back(const std::vector<ReleaseArchive>& releases) {
    switch (page) {
    case LauncherPage::profiles:
        page = platform && available_release_identities(releases, game, *platform).size() > 1
            ? LauncherPage::releases : LauncherPage::platforms;
        break;
    case LauncherPage::releases:
        page = LauncherPage::platforms;
        break;
    case LauncherPage::platforms:
        page = LauncherPage::games;
        break;
    case LauncherPage::games:
        break;
    }
}

bool LauncherRouteState::release_is_selected() const {
    return platform.has_value() && release_sha256.has_value() && release_language.has_value();
}

std::optional<LaunchRequest> LauncherRouteState::launch_request(const LaunchRequest& base) const {
    if (!release_is_selected()) return std::nullopt;
    auto candidate = base;
    candidate.game = game;
    candidate.platform = platform;
    candidate.release_language = release_language;
    candidate.release_sha256 = release_sha256;
    candidate.presentation_explicit = true;
    return candidate;
}

std::optional<ResolvedLaunchRequest> LauncherRouteState::resolve_launch(
    const LaunchRequest& base, const std::vector<ReleaseArchive>& releases) const {
    const auto candidate = launch_request(base);
    return candidate ? resolve_launch_request_identity(*candidate, releases) : std::nullopt;
}

void LauncherSessionState::choose_original() {
    presentation = Presentation::original;
    custom_profile_ready = false;
    custom_profile_pending = false;
}

void LauncherSessionState::choose_modern() {
    presentation = Presentation::modern;
    custom_profile_ready = false;
    custom_profile_pending = false;
}

void LauncherSessionState::begin_custom() {
    presentation = Presentation::modern;
    custom_profile_ready = false;
    custom_profile_pending = true;
}

void LauncherSessionState::confirm_custom() {
    // Custom has no separate runtime identity: confirmation only permits the
    // same explicit Modern presentation after its renderer-only panel closes.
    presentation = Presentation::modern;
    custom_profile_ready = true;
    custom_profile_pending = false;
}

void LauncherSessionState::invalidate_custom() {
    custom_profile_ready = false;
    custom_profile_pending = false;
}

void LauncherSessionState::focus_game(const std::vector<ReleaseArchive>& releases,
    const Game game) {
    route.focus_game(releases, game);
    invalidate_custom();
}

bool LauncherSessionState::choose_platform(const std::vector<ReleaseArchive>& releases,
    const Platform platform) {
    if (!route.choose_platform(releases, platform)) return false;
    invalidate_custom();
    return true;
}

bool LauncherSessionState::choose_release(const std::vector<ReleaseArchive>& releases,
    const std::string_view sha256) {
    if (!route.choose_release(releases, sha256)) return false;
    invalidate_custom();
    return true;
}

void LauncherSessionState::back(const std::vector<ReleaseArchive>& releases) {
    route.back(releases);
    invalidate_custom();
}

void LauncherSessionState::reset_for_data(const Game initial_game) {
    // A new scanner has no authority to inherit a platform/release/profile
    // from the previous user-supplied source. Keep only the currently focused
    // game card; it is presentation focus rather than original-media identity.
    route.page = LauncherPage::games;
    route.game = initial_game;
    route.platform.reset();
    route.release_language.reset();
    route.release_sha256.reset();
    choose_original();
}

bool LauncherSessionState::can_launch() const {
    return route.release_is_selected() && !custom_profile_pending;
}

std::optional<LaunchRequest> LauncherSessionState::launch_request(const LaunchRequest& base) const {
    if (!can_launch()) return std::nullopt;
    auto candidate = route.launch_request(base);
    if (!candidate) return std::nullopt;
    candidate->presentation = presentation;
    candidate->presentation_explicit = true;
    return candidate;
}

std::optional<ResolvedLaunchRequest> LauncherSessionState::resolve_launch(
    const LaunchRequest& base, const std::vector<ReleaseArchive>& releases) const {
    const auto candidate = launch_request(base);
    return candidate ? resolve_launch_request_identity(*candidate, releases) : std::nullopt;
}

namespace {

constexpr std::array<Game, 2> launcher_games{Game::millennium, Game::deuteros};
constexpr std::size_t launcher_profile_count = 3;

std::size_t card_count_for(const LauncherSessionState& session,
    const std::vector<ReleaseArchive>& releases) {
    switch (session.route.page) {
    case LauncherPage::games: return launcher_games.size();
    case LauncherPage::platforms: return supported_platforms(session.route.game).size();
    case LauncherPage::releases:
        return session.route.platform
            ? available_release_identities(releases, session.route.game, *session.route.platform).size()
            : 0;
    case LauncherPage::profiles: return launcher_profile_count;
    }
    return 0;
}

} // namespace

void LauncherInteractionController::synchronize(const std::vector<ReleaseArchive>& releases) {
    focus.set(LauncherPage::games, launcher_games.size(), focus.game);
    if (session.route.page == LauncherPage::games) {
        session.focus_game(releases, launcher_games[focus.game]);
    }
    // Automatic selection is still a real card selection. Keep focus on its
    // visible platform rather than leaving Enter/South-A on an unavailable
    // neighbouring card after a partial scan changes the available release.
    if (session.route.platform) {
        const auto platforms = supported_platforms(session.route.game);
        const auto selected = std::find(platforms.begin(), platforms.end(), *session.route.platform);
        if (selected != platforms.end()) {
            focus.set(LauncherPage::platforms, platforms.size(),
                static_cast<std::size_t>(std::distance(platforms.begin(), selected)));
        }
    }
    const auto count = card_count_for(session, releases);
    focus.set(session.route.page, count, focus.current(session.route.page));
}

void LauncherInteractionController::move(const std::vector<ReleaseArchive>& releases, const int direction) {
    const auto page = session.route.page;
    focus.move(page, card_count_for(session, releases), direction);
    if (page == LauncherPage::games) synchronize(releases);
    if (page == LauncherPage::profiles) session.invalidate_custom();
}

void LauncherInteractionController::first(const std::vector<ReleaseArchive>& releases) {
    const auto page = session.route.page;
    focus.first(page, card_count_for(session, releases));
    if (page == LauncherPage::games) synchronize(releases);
    if (page == LauncherPage::profiles) session.invalidate_custom();
}

void LauncherInteractionController::last(const std::vector<ReleaseArchive>& releases) {
    const auto page = session.route.page;
    focus.last(page, card_count_for(session, releases));
    if (page == LauncherPage::games) synchronize(releases);
    if (page == LauncherPage::profiles) session.invalidate_custom();
}

void LauncherInteractionController::back(const std::vector<ReleaseArchive>& releases) {
    session.back(releases);
    synchronize(releases);
}

LauncherInteractionEffect LauncherInteractionController::activate(
    const std::vector<ReleaseArchive>& releases) {
    switch (session.route.page) {
    case LauncherPage::games:
        focus.reset_after_game_change();
        synchronize(releases);
        session.route.enter_platforms();
        return LauncherInteractionEffect::none;
    case LauncherPage::platforms: {
        const auto platforms = supported_platforms(session.route.game);
        if (platforms.empty()) {
            session.route.page = LauncherPage::games;
            return LauncherInteractionEffect::none;
        }
        focus.set(LauncherPage::platforms, platforms.size(), focus.platform);
        if (!session.choose_platform(releases, platforms[focus.platform])) return LauncherInteractionEffect::none;
        focus.reset_after_platform_change();
        return LauncherInteractionEffect::none;
    }
    case LauncherPage::releases: {
        if (!session.route.platform) return LauncherInteractionEffect::none;
        const auto identities = available_release_identities(releases,
            session.route.game, *session.route.platform);
        if (identities.empty()) {
            session.route.page = LauncherPage::platforms;
            return LauncherInteractionEffect::none;
        }
        focus.set(LauncherPage::releases, identities.size(), focus.release);
        static_cast<void>(session.choose_release(releases, identities[focus.release].sha256));
        return LauncherInteractionEffect::none;
    }
    case LauncherPage::profiles:
        focus.set(LauncherPage::profiles, launcher_profile_count, focus.profile);
        if (focus.profile == 0) session.choose_original();
        else if (focus.profile == 1) session.choose_modern();
        else if (session.custom_profile_ready) return session.can_launch()
            ? LauncherInteractionEffect::launch : LauncherInteractionEffect::none;
        else {
            session.begin_custom();
            return LauncherInteractionEffect::open_custom_settings;
        }
        return session.can_launch() ? LauncherInteractionEffect::launch : LauncherInteractionEffect::none;
    }
    return LauncherInteractionEffect::none;
}

LauncherInteractionEffect LauncherInteractionController::activate_card(
    const std::vector<ReleaseArchive>& releases, const std::size_t index) {
    const auto page = session.route.page;
    const auto count = card_count_for(session, releases);
    if (index >= count) return LauncherInteractionEffect::none;
    focus.set(page, count, index);
    if (page == LauncherPage::games) synchronize(releases);
    return activate(releases);
}

void LauncherInteractionController::reset_for_data(const Game initial_game) {
    session.reset_for_data(initial_game);
    focus.game = initial_game == Game::millennium ? 0 : 1;
    focus.reset_after_game_change();
}

LauncherSourceIdentity LauncherInteractionController::source_identity() const {
    return {session.route.game, session.route.platform, session.route.release_language,
        session.route.release_sha256};
}

bool LauncherInteractionController::source_changed_since(const LauncherSourceIdentity& before) const {
    return source_identity() != before;
}

std::optional<Platform> select_available_platform(
    const std::vector<ReleaseArchive>& releases, const Game game,
    const std::optional<Platform> current) {
    const auto platforms = available_platforms(releases, game);
    if (platforms.empty()) return std::nullopt;
    if (current && std::find(platforms.begin(), platforms.end(), *current) != platforms.end()) {
        return current;
    }
    return platforms.front();
}

bool deuteros_amiga_opening_supported(std::optional<Platform> platform) {
    return !platform || *platform == Platform::amiga;
}

} // namespace eon
