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

## Unicode-rendering boundary

The catalogs are UTF-8 and include scripts such as Arabic, Japanese, Korean,
Hindi, Russian, Ukrainian, and Simplified Chinese. The current SDL launcher
draws them through `SDL_RenderDebugText`, which SDL3 documents as an
ASCII-only debug convenience API, not a production text renderer. Therefore
the package layout proves that all 20 catalogs are shipped and selected, but
does **not** prove correct on-screen rendering for non-ASCII locales.

Project Eon intentionally does not transliterate, replace, or otherwise alter
those translations. A future fix must bundle a license-reviewed Unicode font
and a portable shaping/rasterization stack (for example SDL_ttf plus its
runtime dependencies) across Linux DEB/RPM, Windows Inno Setup, both macOS
architectures, and iPadOS. Current CI builds/package scripts stage SDL3,
SDL3_image, zlib, and libpng only; they neither build SDL_ttf/freetype nor
bundle a font. Adding an untested platform-specific system-font fallback would
break the project's reproducible package contract, so no such fallback is
used.

The exact SDL_ttf 3 and bundled-Noto implementation requirements, including
the current source-archive/submodule constraint and required package tests,
are recorded in [`docs/UNICODE_RENDERING.md`](../docs/UNICODE_RENDERING.md).
