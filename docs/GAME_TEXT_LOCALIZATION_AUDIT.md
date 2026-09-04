# Game-text localization audit

Project Eon's localization contract covers every string presented to a player
in both Original and Modern: menus, object and item names, messages, help, and
labels. Original preserves source pixels and simulation behaviour, but it does
not bypass the selected UI language. Localization is a presentation mapping;
the original media bytes remain immutable and independently hash-addressed.

## Enforced path

`game_text_localization` is the only admitted path from recovered commercial
text to player-visible UTF-8. A definition binds a stable semantic key to its
game, platform, complete source-leaf SHA-256, byte range, source spelling,
source language, and canonical English catalogue message. Media admission
rehashes the complete leaf and copies only provenance tokens. Runtime code can
resolve one token by stable ID or original spelling, or resolve a complete
table with `localize_admitted_game_text_table`.

The table API is deliberately independent of presentation mode, so Original
and Modern cannot select different translations. It rejects empty, reordered,
overlapping, mixed-source, forged, uncatalogued, or incompletely translated
tables before returning any text. Every non-English shipped language must
contain a non-empty PO entry; English uses the canonical English message.

## Current audited coverage

The compiled registry and `docs/game-text-map.json` currently admit 98 exact
ranges from genuine media. Millennium DOS contributes ten English
sound-selection strings and two 41-name celestial tables from the English and
Spanish releases. The clean Deuteros Amiga system ADF contributes six
unambiguous prompt literals from `$78c71..$78cfc`. All 57 canonical messages
are present in every shipped PO catalogue.

No Millennium Amiga or Atari ST text, and no Deuteros Atari ST text, has yet
crossed this admission boundary. Their recovered graphics and native execution scaffolding
must not be described as localized game prose. As each visible table is
recovered, the same change must add exact source identity/ranges, stable keys,
all catalogue translations, runtime admission, and tests. Until then, a
runtime attempt to present such text must fail as uncatalogued rather than
fall back to the source spelling.

## Repository audit findings

- SDL launcher and settings chrome passes through the shared `Translator`.
- Hashes, addresses, register notation, and preservation diagnostics are
  technical provenance, not recovered game prose.
- The only currently rendered original strings are the admitted Millennium
  DOS sound-selection lines and selected driver name; both use the guarded
  game-text renderer in Original and Modern.
- The celestial table has an admitted runtime-table path but is not yet shown
  as gameplay UI because its link to mutable world records remains unproven.
- Future object/item panels are incomplete until they consume admitted table
  tokens; direct rendering of parser strings is prohibited.
