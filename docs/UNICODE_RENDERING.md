# Unicode launcher rendering integration plan

## Current evidence

Project Eon's PO catalogs are UTF-8 and include Arabic, Devanagari, CJK,
Hangul, Cyrillic, Greek, and Latin text. The current `SDL_RenderDebugText`
path is not a Unicode renderer: SDL3 documents it as accepting UTF-8 while
rendering only ASCII, with a fixed 8×8 debug font.

SDL_ttf 3 is the appropriate cross-platform renderer candidate. Its official
API provides an SDL 2D renderer text engine (`TTF_CreateRendererTextEngine`),
UTF-8 text objects, and ordered font fallback through
`TTF_AddFallbackFont`. SDL_ttf 3 is a FreeType/HarfBuzz wrapper; HarfBuzz must
remain enabled because SDL_ttf documents non-left-to-right shaping as
unavailable without it. This is essential for the Arabic launcher catalog.

Sources consulted on 2026-08-29:

- [SDL_RenderDebugText](https://wiki.libsdl.org/SDL3/SDL_RenderDebugText)
- [SDL_ttf text engines](https://wiki.libsdl.org/SDL3_ttf/TTF_TextEngine)
- [SDL_ttf fallback fonts](https://wiki.libsdl.org/SDL3_ttf/TTF_AddFallbackFont)
- [SDL_ttf font direction](https://wiki.libsdl.org/SDL3_ttf/TTF_SetFontDirection)
- [Noto script coverage](https://github.com/notofonts/noto-docs/blob/main/docs/website/use.md)

## Why it is not yet enabled

The current CI deliberately builds only SDL3, SDL3_image, zlib, and libpng.
SDL_ttf release `release-3.2.2` is a valid pin, and its source archive hashes
to `ff6b81d3dc39d843cc3ead6dedd68043a79513d266792ea89445547ef4e9b073`.
However, that archive has empty `external/freetype`, `external/harfbuzz`,
`external/plutosvg`, and `external/plutovg` submodule directories. Enabling
`SDLTTF_VENDORED=ON` from that URL archive therefore cannot supply FreeType or
HarfBuzz; enabling it off instead requires system dependencies that Windows
and iPadOS CI do not install. A URL-only `FetchContent` change would look
reproducible while failing or silently depending on host state.

No selected, license-reviewed font bundle is currently in the repository.
Noto is intentionally modular: Latin/Cyrillic/Greek, Arabic, Devanagari, and
CJK coverage require separate families. A system-font fallback would make the
result package- and host-dependent, so it is not acceptable.

## Required implementation

1. Fetch SDL_ttf using its Git repository pinned to `release-3.2.2` with
   recursive submodules enabled, rather than its incomplete source archive.
   Set `SDLTTF_VENDORED=ON`, `SDLTTF_HARFBUZZ=ON`, disable samples/tests and
   unneeded SVG/color-emoji support, and link `SDL3_ttf::SDL3_ttf` to the
   launcher. Record the resolved main and submodule commits in CMake and CI
   logs.
2. Bundle unmodified, SHA-256-locked OFL-1.1 font files and their license
   notices under `assets/fonts/`. The minimum intended chain is a Noto Sans
   base font, Noto Arabic UI, Noto Sans Devanagari UI, and regular Noto Sans
   CJK language fonts for Simplified Chinese, Japanese, and Korean. Do not
   subset or rename a font until its license notice and glyph coverage have
   been reviewed.
3. Open all bundled fonts with SDL_ttf, add them in deterministic fallback
   order to the base font, and render UI strings with an SDL renderer text
   engine. Set the Arabic font's shaping direction to RTL and retain HarfBuzz
   version/runtime checks. Rendering failure must show the original UTF-8
   string in diagnostics; it must never replace a translation with an
   invented transliteration.
4. Add glyph-coverage and screenshot tests for every catalog below, including
   Arabic joining/bidi, Devanagari shaping, Japanese kana/kanji, Korean
   Hangul, and Simplified Chinese. Tests must run against bundled fonts, never
   fonts installed on the build host.
5. Package font files, OFL notices, and the SDL_ttf runtime/dependencies in
   Linux DEB/RPM, Windows Inno Setup, both macOS bundles, and the iPadOS IPA.
   Extend package-content tests to require all of them. For macOS use the
   existing recursive dylib bundler; for Windows stage the SDL_ttf DLL and
   every dependent DLL explicitly; for iPadOS compile SDL_ttf and its vendored
   dependencies into the app target.

## Catalog coverage contract

| Catalogs | Required bundled fallback family |
| --- | --- |
| `de`, `el`, `en_GB`, `es`, `fi`, `fr`, `it`, `nl`, `no`, `pl`, `pt_BR`, `ru`, `sv`, `tr`, `uk` | Noto Sans base |
| `ar` | Noto Arabic UI |
| `hi` | Noto Sans Devanagari UI |
| `ja` | Noto Sans CJK JP |
| `ko` | Noto Sans CJK KR |
| `zh_CN` | Noto Sans CJK SC |

This plan concerns Project Eon's launcher only. It never changes original
in-game text, game-media bytes, saved data, or preservation parsers.
