# Millennium DOS GX canvas evidence

This note records a direct, read-only decode of two resources in the supplied
English DOS `GX.LIB` (312,748 bytes, SHA-256
`4adf9991226deab4749ac07ad637851994f57d11f6dc45f3f5ce862b5bc34c2f`).
It does not identify a gameplay screen, menu, or simulation state from the
images alone.

`IMG00` is the first 3,461-byte resource at library offset `$000006`. It is a
codec-2 240×33, 48-index bitmap. Its compressed-pixel stream is `$09d9`
bytes. The 912-byte tail contains 768 RGB6 bytes followed by three 48-byte
original tables. The RGB6 table hashes to
`ef964b58025c425fcd6b19aa24f42531eaee10e380891472a5d9a11ca52ca06f`.

`IMG01` follows at `$000d8b` and is 14,079 bytes. It is a codec-2 320×167,
68-index bitmap with a `$3617`-byte stream. Its 204-byte tail splits into a
68-byte logical-index-to-DAC table and 136 bytes retained opaque. The decoded
indices hash to
`1ea177a0fe10a1cae9201e6d31bc91f78a943af5fae8ab36a4c882ea32b6f5a8`.

The native reader combines `IMG01`'s first tail table with `IMG00`'s original
RGB6 DAC, expands RGB6 only for host SDL presentation, and yields a 320×167
RGBA buffer whose SHA-256 is
`b433c77e91dc66e98c2d76a90d63eaabccf706d537ff1258c8af7fbab93efe98`.
No archive entry is unpacked to disk and the two remaining tables are not
assigned unproven semantics.
