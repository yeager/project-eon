#pragma once

#include "platform/game_data.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace eon {

// Original is the preservation contract. Modern is explicitly opt-in and may
// enable host-side improvements; it must never silently become the default
// presentation for a supplied original release.
enum class Presentation { original, modern };

// The card launcher has no SDL dependency: keyboard, gamepad, pointer, and
// touch routes all drive this one state machine and merely render its page.
enum class LauncherPage { games, platforms, releases, profiles };

// Focus is presentation state, separate from a selected release identity.
// Keeping its bounds and wrap behaviour in the SDL-free launcher core means
// keyboard, gamepad, mouse, and touch can render or set the same focus without
// making a card index part of a launch request.
struct LauncherCardFocus {
    std::size_t game = 0;
    std::size_t platform = 0;
    std::size_t release = 0;
    std::size_t profile = 0;

    [[nodiscard]] std::size_t current(LauncherPage page) const;
    void set(LauncherPage page, std::size_t count, std::size_t index);
    void move(LauncherPage page, std::size_t count, int direction);
    void first(LauncherPage page, std::size_t count);
    void last(LauncherPage page, std::size_t count);
    void reset_after_game_change();
    void reset_after_platform_change();
};

// The release page is a presentation window over the complete, SHA-sorted
// identity list. It deliberately carries indexes, not release values: media
// identity remains resolved by the existing launcher route and scanner.
struct ReleaseCardPage {
    std::size_t first_identity = 0;
    std::size_t visible_count = 0;
    std::size_t page = 0;
    std::size_t page_count = 0;
};

// Four cards fit in the 1280x720 launcher grid. A larger recognised release
// set follows keyboard/gamepad focus onto a new page without treating a
// page-local card number as a release identity.
[[nodiscard]] ReleaseCardPage release_card_page_for_focus(
    std::size_t identity_count, std::size_t focused_identity);

// Renderer-only preferences shared by CLI startup and the F10 overlay. They
// never enter a game simulation, save file, or original media access path.
struct DisplayPreferences {
    int width = 1280;
    int height = 720;
    std::size_t aspect_ratio_index = 0; // Original 4:3, square 8:5, wide 16:9.
};

struct LaunchRequest {
    std::filesystem::path data_directory;
    bool data_directory_is_default = true;
    std::optional<Game> game;
    std::optional<Game> verify_game;
    std::optional<std::filesystem::path> reference_trace;
    // Emits only the hash-bound admission facts for --reference-trace. It
    // remains a diagnostics-only path and never opens a runtime session.
    bool reference_trace_json = false;
    // An explicitly supplied Millennium DOS save is a diagnostics-only input.
    // It is parsed read-only and never joins a launch, simulation, or save
    // write path.
    std::optional<std::filesystem::path> inspect_save;
    // Explicit diagnostics-only root for separately installed Modern packs.
    // There is deliberately no default lookup.
    std::optional<std::filesystem::path> modern_pack_root;
    // Explicit Modern renderer selection, revalidated against the selected
    // original release before the external bytes are used.
    std::optional<std::filesystem::path> modern_pack_manifest;
    bool inspect_data = false;
    // Deterministic preservation diagnostics for tooling. It never reports
    // paths, filenames, archive members, or original bytes.
    bool inspect_json = false;
    // Preservation-only leaf manifest. It is intentionally opt-in because
    // `--inspect` normally reports recovered structure, not every filename.
    bool inventory_assets = false;
    // Validates one full release/runtime admission without creating SDL
    // objects or entering a game loop. It is a startup diagnostic, never a
    // substitute runtime or replay mode.
    bool launch_check = false;
    // Machine-readable form of the same bounded launch check. It emits only
    // release identity and the safe admission result.
    bool launch_check_json = false;
    std::optional<Platform> platform;
    // Language of the immutable original release, distinct from the launcher
    // UI locale above.  A release selection must never infer this from the
    // user's desktop locale or substitute another edition.
    std::optional<std::string> release_language;
    // Exact immutable outer-container identity. This is deliberately distinct
    // from language: several recognised releases can share one platform and
    // language but still have different loader/provenance boundaries.
    std::optional<std::string> release_sha256;
    Presentation presentation = Presentation::original;
    // Keep diagnostics-only inputs from quietly accepting an otherwise
    // meaningless presentation switch.
    bool presentation_explicit = false;
    DisplayPreferences display;
    bool display_resolution_explicit = false;
    bool display_aspect_explicit = false;
    std::string language;
};

// The one admission result a live session is allowed to consume.  Keeping the
// original, scanner-produced archive next to its normalized request prevents a
// later lookup from accidentally following changed menu state or scan order.
struct ResolvedLaunchRequest {
    LaunchRequest request;
    ReleaseArchive release;
};

// The renderer never needs a path or original bytes to decide whether its
// release-bound cache is stale. This value is the complete menu provenance
// identity: presentation changes are deliberately excluded.
struct LauncherSourceIdentity {
    Game game = Game::millennium;
    std::optional<Platform> platform;
    std::optional<std::string> language;
    std::optional<std::string> sha256;
    [[nodiscard]] bool operator==(const LauncherSourceIdentity&) const = default;
};

// This is presentation-only navigation state. It contains no source paths or
// original bytes, and its only transition into a runnable session is the
// hash-bound resolve_launch() call below.
struct LauncherRouteState {
    LauncherPage page = LauncherPage::games;
    Game game = Game::millennium;
    std::optional<Platform> platform;
    std::optional<std::string> release_language;
    std::optional<std::string> release_sha256;
    // A unique release may be selected automatically while the scanner has
    // one identity. If a later scan result makes that platform ambiguous, the
    // user must see and choose an Original-release card before launch.
    bool release_explicit = false;

    void focus_game(const std::vector<ReleaseArchive>& releases, Game next_game);
    [[nodiscard]] bool choose_platform(const std::vector<ReleaseArchive>& releases,
        Platform next_platform);
    [[nodiscard]] bool choose_release(const std::vector<ReleaseArchive>& releases,
        std::string_view next_sha256);
    void enter_platforms();
    void back(const std::vector<ReleaseArchive>& releases);
    // Scanner results arrive incrementally. Revalidate this route only
    // against the supplied recognised identities and revoke an automatic
    // selection if it becomes ambiguous; never substitute another release.
    [[nodiscard]] bool reconcile_releases(const std::vector<ReleaseArchive>& releases);
    [[nodiscard]] bool release_is_selected() const;
    // Build the selected card route as a candidate only. The engine performs
    // the final scanner-identity resolution and runtime admission; this
    // SDL-free step has no source access and cannot launch by itself.
    [[nodiscard]] std::optional<LaunchRequest> launch_request(const LaunchRequest& base) const;
    [[nodiscard]] std::optional<ResolvedLaunchRequest> resolve_launch(
        const LaunchRequest& base, const std::vector<ReleaseArchive>& releases) const;
};

// One SDL-free owner for the mutable card-session state. The route owns the
// immutable original-release selection; this wrapper owns the selected
// presentation and Custom's explicit confirmation gate. It intentionally
// does not know about renderer packs, dialogs, textures, or game sessions.
struct LauncherSessionState {
    LauncherRouteState route;
    Presentation presentation = Presentation::original;
    bool custom_profile_ready = false;
    bool custom_profile_pending = false;

    void choose_original();
    void choose_modern();
    void begin_custom();
    void confirm_custom();
    void invalidate_custom();
    void focus_game(const std::vector<ReleaseArchive>& releases, Game game);
    [[nodiscard]] bool choose_platform(const std::vector<ReleaseArchive>& releases,
        Platform platform);
    [[nodiscard]] bool choose_release(const std::vector<ReleaseArchive>& releases,
        std::string_view sha256);
    void back(const std::vector<ReleaseArchive>& releases);
    void reset_for_data(Game initial_game);
    [[nodiscard]] bool reconcile_releases(const std::vector<ReleaseArchive>& releases);
    [[nodiscard]] bool can_launch() const;
    [[nodiscard]] std::optional<LaunchRequest> launch_request(const LaunchRequest& base) const;
    [[nodiscard]] std::optional<ResolvedLaunchRequest> resolve_launch(
        const LaunchRequest& base, const std::vector<ReleaseArchive>& releases) const;
};

// SDL adapts keyboard, gamepad, mouse, and touch to these three effects.  The
// controller deliberately owns no rectangles, translated labels, dialogs, or
// renderer resources: it can therefore prove that every input device follows
// the same release-identity and Custom-confirmation rules.
enum class LauncherInteractionEffect { none, open_custom_settings, launch };

struct LauncherInteractionController {
    LauncherSessionState session;
    LauncherCardFocus focus;

    // Scanner results may arrive incrementally. Reconcile the current game
    // focus against only the supplied hash-verified releases; this never
    // creates a release selection or invents a platform.
    void synchronize(const std::vector<ReleaseArchive>& releases);
    void move(const std::vector<ReleaseArchive>& releases, int direction);
    void first(const std::vector<ReleaseArchive>& releases);
    void last(const std::vector<ReleaseArchive>& releases);
    void back(const std::vector<ReleaseArchive>& releases);
    // Activates the focused card. A returned launch effect is admissible only
    // through session.resolve_launch(), which binds one exact outer hash.
    [[nodiscard]] LauncherInteractionEffect activate(const std::vector<ReleaseArchive>& releases);
    // Pointer/touch cards set focus before using the same activate() path as
    // keyboard and gamepad. Out-of-range cards are ignored safely.
    [[nodiscard]] LauncherInteractionEffect activate_card(
        const std::vector<ReleaseArchive>& releases, std::size_t index);
    void reset_for_data(Game initial_game);
    [[nodiscard]] LauncherSourceIdentity source_identity() const;
    [[nodiscard]] bool source_changed_since(const LauncherSourceIdentity& before) const;
};

struct ParseResult {
    std::optional<LaunchRequest> request;
    std::string error;
    bool help = false;
};

[[nodiscard]] ParseResult parse_command_line(int argc, char** argv);
[[nodiscard]] std::string usage();
[[nodiscard]] bool release_available(
    const std::vector<ReleaseArchive>& releases,
    Game game,
    std::optional<Platform> platform);

// A platform card is an admission decision, not merely a claim that files
// resembling a release were found.  The scanner supplies only hash-verified
// ReleaseArchive entries here.  A card with several verified language
// identities may be entered to choose one, but it must never start a game
// until that choice is recorded.
enum class PlatformCardStatus { unavailable, release_selection_required, ready };

[[nodiscard]] PlatformCardStatus platform_card_status(
    const std::vector<ReleaseArchive>& releases, Game game, Platform platform);
[[nodiscard]] bool platform_card_selectable(PlatformCardStatus status);
[[nodiscard]] bool platform_card_startable(PlatformCardStatus status);
// The launcher must distinguish an unsupported platform from a supported
// platform whose original media is absent. This fixed support catalogue is
// deliberately independent of the scanner's hash-verified availability.
[[nodiscard]] std::vector<Platform> supported_platforms(Game game);
// Ordered, hash-verified platforms that can be selected for one game. This is
// shared by the start menu and tests so a UI choice cannot silently fall back
// to another release.
[[nodiscard]] std::vector<Platform> available_platforms(
    const std::vector<ReleaseArchive>& releases, Game game);
// Languages are part of a release identity, rather than a UI preference.
// Keep their order deterministic so a card focus can never select an
// arbitrary archive occurrence.
[[nodiscard]] std::vector<std::string> available_release_languages(
    const std::vector<ReleaseArchive>& releases, Game game, Platform platform);
[[nodiscard]] std::optional<std::string> select_available_release_language(
    const std::vector<ReleaseArchive>& releases, Game game, Platform platform,
    const std::optional<std::string>& current);
// Exact release identities for one game/platform. The returned copies retain
// the scanner's immutable outer hash and path; ordering is by SHA-256 so no
// menu or CLI path can depend on filesystem scan order.
[[nodiscard]] std::vector<ReleaseArchive> available_release_identities(
    const std::vector<ReleaseArchive>& releases, Game game, Platform platform);
// Retain a selected hash only within its game/platform scope. English is a
// default only when it identifies exactly one eligible outer release; a
// duplicate English identity must be chosen explicitly.
[[nodiscard]] std::optional<std::string> select_available_release_sha256(
    const std::vector<ReleaseArchive>& releases, Game game, Platform platform,
    const std::optional<std::string>& current);
[[nodiscard]] std::optional<ReleaseArchive> resolve_release_identity(
    const std::vector<ReleaseArchive>& releases, Game game, Platform platform,
    const std::optional<std::string>& requested_sha256,
    const std::optional<std::string>& requested_language);
// Turn a CLI or card-menu candidate into one immutable launch identity.  The
// returned request contains the manifest-proven game, platform, language, and
// outer SHA-256 together; callers must not launch from independently mutable
// menu fields after this boundary has been crossed.
[[nodiscard]] std::optional<ResolvedLaunchRequest> resolve_launch_request_identity(
    const LaunchRequest& candidate, const std::vector<ReleaseArchive>& releases);
// Retain a choice only when it belongs to the newly focused game. Otherwise
// choose that game's first hash-verified platform; no platform means no start.
[[nodiscard]] std::optional<Platform> select_available_platform(
    const std::vector<ReleaseArchive>& releases, Game game,
    std::optional<Platform> current);

// The recovered Deuteros SDL opening is backed only by the clean Amiga ADF.
// An omitted platform may select that verified preview from the menu, while
// an explicit Atari ST selection remains at the protected-boot boundary.
[[nodiscard]] bool deuteros_amiga_opening_supported(std::optional<Platform> platform);

} // namespace eon
