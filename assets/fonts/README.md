# Project Eon launcher fonts

These files are the complete, renderer-only Project Eon launcher font bundle.
They are not original game media and must never be used to replace, translate,
or alter text stored in a supplied Millennium 2.2 or Deuteros release.

All six font programs are unmodified upstream Noto files licensed under the
[SIL Open Font License 1.1](OFL-1.1.txt) (OFL-1.1).  `OFL-1.1.txt` is shipped
with every copy of the bundle.  The source revisions and SHA-256 values below
make the vendored bytes independently reviewable; a package test rejects a
missing or altered font rather than falling back to a workstation font.

| File | Launcher coverage role | Official upstream source | Revision | SHA-256 |
| --- | --- | --- | --- | --- |
| `NotoSans-Regular.ttf` | Latin, Greek, Cyrillic; `de`, `el`, `en_GB`, `es`, `fi`, `fr`, `it`, `nl`, `no`, `pl`, `pt_BR`, `ru`, `sv`, `tr`, `uk` | [notofonts/noto-fonts](https://github.com/notofonts/noto-fonts/blob/ffebf8c1ee449e544955a7e813c54f9b73848eac/hinted/ttf/NotoSans/NotoSans-Regular.ttf) | `ffebf8c1ee449e544955a7e813c54f9b73848eac` | `b85c38ecea8a7cfb39c24e395a4007474fa5a4fc864f6ee33309eb4948d232d5` |
| `NotoSansArabic-Regular.ttf` | Arabic and RTL shaping; `ar` | [notofonts/noto-fonts](https://github.com/notofonts/noto-fonts/blob/ffebf8c1ee449e544955a7e813c54f9b73848eac/hinted/ttf/NotoSansArabic/NotoSansArabic-Regular.ttf) | `ffebf8c1ee449e544955a7e813c54f9b73848eac` | `ceea25b464a656dc3b26849bab9356740401af62aedf1bfa8b7f0d9b75925b1b` |
| `NotoSansDevanagari-Regular.ttf` | Devanagari shaping; `hi` | [notofonts/noto-fonts](https://github.com/notofonts/noto-fonts/blob/ffebf8c1ee449e544955a7e813c54f9b73848eac/hinted/ttf/NotoSansDevanagari/NotoSansDevanagari-Regular.ttf) | `ffebf8c1ee449e544955a7e813c54f9b73848eac` | `385e78e6359a9d88a0f243d53b1209d7548361ba2194e2b9ec779bcaa7e8949d` |
| `NotoSansJP-Regular.otf` | Japanese; `ja` | [notofonts/noto-cjk](https://github.com/notofonts/noto-cjk/blob/f8d157532fbfaeda587e826d4cd5b21a49186f7c/Sans/SubsetOTF/JP/NotoSansJP-Regular.otf) | `f8d157532fbfaeda587e826d4cd5b21a49186f7c` | `dff723ba59d57d136764a04b9b2d03205544f7cd785a711442d6d2d085ac5073` |
| `NotoSansKR-Regular.otf` | Korean; `ko` | [notofonts/noto-cjk](https://github.com/notofonts/noto-cjk/blob/f8d157532fbfaeda587e826d4cd5b21a49186f7c/Sans/SubsetOTF/KR/NotoSansKR-Regular.otf) | `f8d157532fbfaeda587e826d4cd5b21a49186f7c` | `69975a0ac8472717870aefeab0a4d52739308d90856b9955313b2ad5e0148d68` |
| `NotoSansSC-Regular.otf` | Simplified Chinese; `zh_CN` | [notofonts/noto-cjk](https://github.com/notofonts/noto-cjk/blob/f8d157532fbfaeda587e826d4cd5b21a49186f7c/Sans/SubsetOTF/SC/NotoSansSC-Regular.otf) | `f8d157532fbfaeda587e826d4cd5b21a49186f7c` | `faa6c9df652116dde789d351359f3d7e5d2285a2b2a1f04a2d7244df706d5ea9` |

The CJK files are upstream's official `SubsetOTF` releases, not generated
subsets.  Their combination keeps the packaged bundle practical while retaining
the glyph coverage expected by the shipped Japanese, Korean, and Simplified
Chinese catalogs.  SDL_ttf with HarfBuzz must use the ordered bundle; a single
font alone is not evidence of coverage for all locales.

To review the local immutable bytes:

```sh
sha256sum assets/fonts/NotoSans-Regular.ttf \
  assets/fonts/NotoSansArabic-Regular.ttf \
  assets/fonts/NotoSansDevanagari-Regular.ttf \
  assets/fonts/NotoSansJP-Regular.otf \
  assets/fonts/NotoSansKR-Regular.otf \
  assets/fonts/NotoSansSC-Regular.otf
```
