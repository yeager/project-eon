# Unicode launcher rendering

Project Eon renders every launcher catalog through SDL_ttf 3 with HarfBuzz
enabled and a reviewed, bundled Noto fallback chain. SDL's debug text renderer
is retained only as an emergency development diagnostic; it is not evidence of
non-Latin UI coverage.

## Reproducible renderer boundary

SDL_ttf's public API supplies an SDL renderer text engine, UTF-8 text objects,
and ordered fallback fonts. CMake fetches SDL_ttf release `release-3.2.2`,
FreeType and HarfBuzz from immutable upstream source snapshots verified by
SHA-256, and links `SDL3_ttf::SDL3_ttf`. `SDLTTF_HARFBUZZ=ON` is explicit.
The SDL_ttf release source archive has empty dependency gitlinks, so Project Eon deliberately
stages the two pinned dependency trees before adding SDL_ttf; no platform is
allowed to consult a host font library.

At runtime, `src/main.cpp` searches only Project Eon's resource layouts:

- `assets/fonts` next to a desktop executable;
- `Resources/assets/fonts` in an Apple application bundle;
- the development asset directory or local development layout.

`src/launcher_text.cpp` refuses to initialize if any required file is missing.
It opens the following ordered fallback chain at one point size:

1. Noto Sans Regular — Latin, Greek and Cyrillic.
2. Noto Sans Arabic — Arabic.
3. Noto Sans Devanagari — Hindi.
4. Noto Sans JP — Japanese.
5. Noto Sans KR — Korean.
6. Noto Sans SC — Simplified Chinese.

Arabic text is explicitly shaped RTL because the base font is LTR.
Devanagari text explicitly selects the `Deva` script. HarfBuzz then performs
the script shaping and SDL_ttf selects glyphs through the ordered chain.
There is no host-font lookup, transliteration, or synthetic English fallback.

The unmodified official font files, upstream revisions, SHA-256 values,
coverage mapping and `OFL-1.1` SIL Open Font License 1.1 text are in
[`assets/fonts/README.md`](../assets/fonts/README.md). Tests require each
file and the license in Linux DEB/RPM, Windows Inno Setup, macOS x86_64/arm64
bundles and the iPadOS IPA.

## Catalog coverage

| Catalogs | Bundled family |
| --- | --- |
| `de`, `el`, `en_GB`, `es`, `fi`, `fr`, `it`, `nl`, `no`, `pl`, `pt_BR`, `ru`, `sv`, `tr`, `uk` | Noto Sans Regular |
| `ar` | Noto Sans Arabic |
| `hi` | Noto Sans Devanagari |
| `ja` | Noto Sans JP |
| `ko` | Noto Sans KR |
| `zh_CN` | Noto Sans SC |

## Preservation scope

This renderer concerns only Project Eon's launcher chrome. It never changes
original in-game text, supplied game-media bytes, saved data, input, or
preservation parsers. Original text remains the user's data and is not
translated or replaced by the launcher.

The F10 Modern graphics dialog is launcher chrome too, including its option
values and aspect-ratio labels. Its drawing function receives the selected
`Translator` explicitly and resolves every visible label through the shipped
PO catalogue before rendering. This keeps its settings legible in every
supported launcher language without translating original game text or data.
