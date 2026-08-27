# Deuteros Atari ST protected boot evidence

This record describes bytes read directly from the supplied Deuteros Atari ST
release archive. It is a preservation trace, not a replacement executable and
does not unpack or alter the media.

## Disk 2 KILLER_BOOT vector setup

The supplied unlabelled Disk 2 image has SHA-256
`5501ce3fd79c9b37cf695692a8012267db23dacd8a2cc64c0c7b7e4305971193`.
Its boot sector has the Atari checksum `0x1234`, branches to offset `0x22`,
and contains `KILLER_BOOT\0` at offset `0x24`.

At boot offset `0xd8`, a literal 68000 sequence:

1. enters supervisor mode with status word `0x2700`;
2. copies ten longwords from boot offset `0xee` to absolute RAM `0x000008`;
3. jumps to absolute address `0x000012`.

Project Eon records these fixed operands only. It makes no claim that the
copied values are a Deuteros title, a game loader, or a usable executable;
they belong to the supplied crack/protection boot path. The runtime continues
to read the original image in memory and refuses to turn this trace into an
unpacked game-data representation.
