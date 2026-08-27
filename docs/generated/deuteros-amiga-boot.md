# Generated Deuteros Amiga boot disassembly

- Source: `Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 1 of 2).adf`
- Disk identifier: `b'DOS\x00'`
- Root/custom block: `880` (`0x6e000`)

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
