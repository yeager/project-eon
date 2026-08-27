# Generated Deuteros Amiga boot disassembly

- Source: `Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 1 of 2).adf`
- Disk identifier: `b'DOS\x00'`
- Root/custom block: `880` (`0x6e000`)
- Bootstrap track load: disk `0x2c00` → memory `0x12800`, length `0x1600`
- Bootstrap entry: `0x12a4e`
- Main stage load: disk `0x5800` → memory `0x20000`, length `0x4200`
- Main entry: `0x21734` (disk `0x6f34`)

## Resource catalogue

The main-stage loader at memory `$21932` indexes five big-endian disk offsets
from the table at `$21708`: `0x1b800`, `0x4ba00`, `0x37000`, `0x59600`, and
`0x6e000`. It reads a four-byte resource length, then loads that many decoded
bytes from the same disk offset.

The first two entries are verified resource bundles. Their 60-byte headers
contain a total length, a 16-bit object count, seven relative channel pointers,
six relative auxiliary pointers, and a 16-bit mode flag. Project Eon checks
that the declared bundle and every non-null pointer remain inside the original
ADF data before exposing them to the game layer.

| Offset | Length | Objects | Mode |
| ---: | ---: | ---: | ---: |
| `0x1b800` | `0x2f3f4` | 4 | 0 |
| `0x4ba00` | `0x215f0` | 6 | 1 |

Each channel catalogue entry addresses a 10-byte initial-state header followed
by a word-opcoded program interpreted at `$214aa`. The verified bundles expose
four and six channels respectively. Native parsing implements the original
operand widths for the entire recognized opcode range `$00`–`$14` while
leaving still-unverified gameplay semantics unnamed.

The table/payload pair in auxiliary slots 4 and 5 is consumed by bitmap
routine `$20c8c`. Its normal `$20da6` branch decodes four RLE control classes
into interleaved four-bitplane pixels. Bit-15 height records select a separate
plane-sequential layout at `$20eb2`; both layouts are decoded independently.

## Boot block

```asm
0000000c  move.l     a1, -(a7)
0000000e  move.w     #$8005, $1c(a1)
00000014  move.b     #$0, $1e(a1)
0000001a  movea.l    $4.w, a6
0000001e  jsr        -$1c8(a6)
00000022  move.l     #$c350, d0
00000028  subq.l     #$1, d0
0000002a  bne.b      $28
0000002c  move.w     #$8002, $1c(a1)
00000032  move.l     #$1600, $24(a1)
0000003a  move.l     #$12800, $28(a1)
00000042  move.l     #$2, d7
00000048  mulu.w     #$1600, d7
0000004c  move.l     d7, $2c(a1)
00000050  move.b     #$0, $1e(a1)
00000056  movea.l    $4.w, a6
0000005a  jsr        -$1c8(a6)
0000005e  move.l     $24(a1), d0
00000062  clr.l      $24(a1)
00000066  move.w     #$9, $1c(a1)
0000006c  move.b     #$0, $1e(a1)
00000072  movea.l    $4.w, a6
00000076  jsr        -$1c8(a6)
0000007a  movea.l    (a7)+, a1
0000007c  move.l     a1, $12ff8.l
00000082  movea.l    #$12a4e, a0
00000088  moveq      #$0, d0
0000008a  move.l     d0, $12ffc.l
00000090  rts
00000092  nop
00000094  nop
00000096  nop
00000098  nop
0000009a  nop
```

## Bootstrap entry

```asm
00012a4e  move.l     $12ffc.l, d0
00012a54  move.w     d0, $12a34.l
00012a5a  movea.l    $12ff8.l, a1
00012a60  move.l     a1, $12822.l
00012a66  bsr.w      $128b4
00012a6a  move.l     $12ff4.l, d0
00012a70  cmp.w      #$ab00, d0
00012a74  beq.b      $12a7e
00012a76  bsr.w      $13000
00012a7a  bsr.w      $1330e
00012a7e  bsr.w      $12932
00012a82  move.w     #$8005, $1c(a1)
00012a88  move.b     #$0, $1e(a1)
00012a8e  movea.l    $4.w, a6
00012a92  jsr        -$1c8(a6)
00012a96  lea.l      $12a36.l, a1
00012a9c  move.w     $12a34.l, d0
00012aa2  lsl.w      #$2, d0
00012aa4  movea.l    (a1, d0.w), a1
00012aa8  jsr        (a1)
00012aaa  movea.l    $12822.l, a1
00012ab0  move.w     #$8002, $1c(a1)
00012ab6  move.l     d0, $24(a1)
00012aba  move.l     d1, $28(a1)
00012abe  move.l     d1, -(a7)
00012ac0  mulu.w     #$1600, d2
00012ac4  move.l     d2, $2c(a1)
00012ac8  move.b     #$0, $1e(a1)
00012ace  movea.l    $4.w, a6
00012ad2  jsr        -$1c8(a6)
00012ad6  move.l     $24(a1), d0
00012ada  clr.l      $24(a1)
00012ade  move.w     #$9, $1c(a1)
00012ae4  move.b     #$0, $1e(a1)
00012aea  movea.l    $4.w, a6
00012aee  jsr        -$1c8(a6)
00012af2  movea.l    $12822.l, a1
00012af8  movea.l    $4.w, a6
00012afc  jsr        -$1c2(a6)
00012b00  lea.l      $1285e.l, a1
00012b06  movea.l    $4.w, a6
00012b0a  jsr        -$168(a6)
```

## Main entry

```asm
00021734  move.l     a1, $20976.l
0002173a  move.w     d0, $21704.l
00021740  movea.l    #$22296, a7
00021746  movea.l    $4.w, a6
0002174a  jsr        -$96(a6)
0002174e  move.l     #$7fff0, d0
00021754  movea.l    $4.w, a6
00021758  jsr        -$9c(a6)
0002175c  jsr        $20068.l
00021762  jsr        $2013a.l
00021768  move.l     $20128.l, $20510.l
00021772  move.l     $20128.l, $20c20.l
0002177c  jsr        $22a5a.l
00021782  jsr        $22bea.l
00021788  jsr        $22bea.l
0002178e  movea.l    #$dff000, a0
00021794  move.w     #$7fff, $40(a0)
0002179a  move.w     #$7fff, $42(a0)
000217a0  move.w     #$c000, $9a(a0)
000217a6  move.w     #$87ff, $96(a0)
000217ac  lea.l      $12e00.l, a0
000217b2  movea.l    $4(a0), a1
000217b6  movea.l    $4(a1), a1
000217ba  addq.l     #$4, a1
000217bc  move.l     a1, $2197a.l
000217c2  lea.l      $12f00.l, a0
000217c8  movea.l    $4(a0), a1
000217cc  movea.l    $4(a1), a1
000217d0  addq.l     #$4, a1
000217d2  move.l     a1, $2197e.l
000217d8  jsr        $20994.l
000217de  move.l     #$20000, d1
000217e4  bset.b     #$1, $bfe001.l
000217ec  move.w     $21704.l, d0
000217f2  bsr.w      $21926
000217f6  nop
000217f8  jsr        $22a5a.l
000217fe  move.w     #$0, $21720.l
00021806  move.w     #$0, $2171e.l
0002180e  move.w     #$1, $210f2.l
00021816  jsr        $21276.l
0002181c  jsr        $21380.l
00021822  btst.b     #$a, $dff016.l
0002182a  bne.b      $21834
0002182c  move.b     #$1, $21721.l
00021834  tst.b      $2171e.l
0002183a  bne.b      $21856
0002183c  tst.b      $21721.l
00021842  beq.b      $21856
00021844  move.w     $21696.l, d0
0002184a  btst.b     #$0, d0
0002184e  bne.b      $21856
00021850  bsr.w      $218cc
00021854  bra.b      $217f6
00021856  tst.b      $210f4.l
0002185c  bne.b      $21892
0002185e  btst.b     #$6, $bfe001.l
00021866  bne.b      $21870
00021868  move.b     #$1, $21720.l
00021870  tst.b      $2171e.l
00021876  bne.b      $21880
00021878  tst.b      $21720.l
0002187e  bne.b      $21882
00021880  bra.b      $2181c
00021882  move.w     $21696.l, d0
00021888  btst.b     #$0, d0
0002188c  bne.b      $2181c
0002188e  bra.w      $21982
00021892  move.l     $2126a.l, d0
00021898  beq.b      $218a2
0002189a  bsr.w      $2229c
0002189e  bsr.w      $224a2
000218a2  jsr        $22a5a.l
000218a8  move.l     $2079e.l, d0
000218ae  move.l     $2079e.l, d1
000218b4  cmp.l      d1, d0
000218b6  beq.b      $218ae
000218b8  jsr        $208ba.l
000218be  btst.b     #$6, $bfe001.l
000218c6  beq.b      $218be
000218c8  bra.w      $217f6
000218cc  move.l     $2126a.l, d0
000218d2  beq.b      $218dc
000218d4  bsr.w      $2229c
000218d8  bsr.w      $224a2
000218dc  jsr        $22a5a.l
000218e2  move.l     $2079e.l, d0
000218e8  move.l     $2079e.l, d1
000218ee  cmp.l      d1, d0
000218f0  beq.b      $218e8
000218f2  move.l     d0, $12fe4.l
000218f8  jsr        $208ba.l
000218fe  jsr        $20a74.l
00021904  move.w     $21704.l, d0
0002190a  addq.w     #$1, d0
0002190c  cmp.w      #$2, d0
00021910  beq.w      $21a4c
00021914  cmp.w      #$3, d0
00021918  beq.w      $219f8
0002191c  cmp.w      #$5, d0
00021920  bcs.b      $21924
00021922  moveq      #$0, d0
00021924  nop
00021926  move.w     d0, $21704.l
0002192c  move.w     d0, $21706.l
```
