# Project Eon translations

Project Eon reads UTF-8 GNU PO source files directly at runtime.  Keep one
`<language>.po` file per supported language code; region-specific files such
as `pt_BR.po` and `zh_CN.po` are selected for their language family too.
English strings are source text and deliberately have no PO file.

Run `cmake --build <build-directory> --target l10n` before packaging. It uses
GNU gettext's `msgfmt` and `msgcmp` to validate PO syntax and ensure each of
the 20 shipped catalogs matches `ProjectEon.pot`. The target writes temporary
`.mo` probes only under the build directory; it never rewrites translation
sources or original game data.

The built application first looks beside itself and in its installed shared
data directory; development builds also use this source directory. Packaging
must keep all 20 catalogs, not merely the active developer locale:

- Linux installs catalogs under `share/project-eon/po` and cards beside the
  executable under `bin/assets/cards`.
- Windows stages catalogs in `po` beside `project-eon.exe`.
- macOS and iPadOS bundle catalogs in `Resources/po` and cards in
  `Resources/assets/cards` (macOS also keeps its executable-adjacent card
  layout for the desktop bundle).

None of these locations is a game-data directory. User-supplied media is
looked up separately and never created, populated, or packaged by localization
installation.

## Game text

Localization applies to all player-visible text in both Original and Modern:
menus, item names, status messages, prompts, help, and labels. Original media
bytes remain immutable and hash-addressed. `src/game_text_localization.*`
maps each recovered source string to a stable ID and canonical English message;
the selected PO catalog supplies only its presentation. A non-English catalog
missing a declared game-text message fails closed instead of silently showing
another language. Every newly rendered recovered string must therefore be
added to the registry, POT, and all shipped catalogs in the same change.

## Unicode rendering

The catalogs are UTF-8 and include scripts such as Arabic, Japanese, Korean,
Hindi, Russian, Ukrainian, and Simplified Chinese. The launcher renders them
through SDL_ttf with a hash-reviewed, bundled Noto fallback chain rather than
through SDL's ASCII-only debug text API.

Project Eon intentionally does not transliterate, replace, or otherwise alter
those translations. The same OFL-licensed font bytes and SDL_ttf runtime are
packaged across Linux DEB/RPM, Windows Inno Setup, both macOS architectures,
and iPadOS. A system-font fallback would break the reproducible package
contract and is never used.

The renderer and preservation boundary are recorded in
[`docs/UNICODE_RENDERING.md`](../docs/UNICODE_RENDERING.md).
