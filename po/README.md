# Project Eon translations

Project Eon reads UTF-8 GNU PO source files directly at runtime.  Keep one
`<language>.po` file per supported language code; region-specific files such
as `pt_BR.po` and `zh_CN.po` are selected for their language family too.
English strings are source text and deliberately have no PO file.

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
