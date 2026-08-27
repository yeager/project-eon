# Generated Millennium DOS binary report

## `2200AD.EXE`

- Size: 54391 bytes
- Entry file offset: `0xd1b0`
- Entry load address: `0xd2b0`
- Syntactic interrupt occurrences: {'0x21': 33, '0x15': 18, '0x33': 17, '0x75': 4, '0xe8': 4, '0x10': 3, '0x4f': 2, '0x00': 2, '0x1a': 2, '0x91': 1, '0x93': 1, '0x74': 1, '0x89': 1, '0xce': 1, '0x8e': 1, '0xf9': 1, '0xea': 1, '0x40': 1, '0x80': 1, '0x9f': 1, '0x13': 1, '0x18': 1, '0x88': 1, '0xd2': 1, '0xe9': 1, '0xc3': 1, '0x2e': 1, '0xa1': 1, '0x9d': 1, '0x8c': 1, '0xfa': 1, '0xc2': 1, '0x0e': 1, '0x95': 1}

```asm
0d2b0  push     cs
0d2b1  pop      ds
0d2b2  push     cs
0d2b3  pop      es
0d2b4  mov      ax, cs
0d2b6  mov      ss, ax
0d2b8  mov      ax, 0xda00
0d2bb  mov      sp, ax
0d2bd  mov      ax, 0x1f
0d2c0  push     cs
0d2c1  pop      es
0d2c2  mov      bx, 0xd19e
0d2c5  call     0x10124
0d2c8  mov      word ptr cs:[0xd128], ax
0d2cc  mov      al, ah
0d2ce  mov      byte ptr cs:[0x4368], al
0d2d2  mov      byte ptr [0xda05], al
0d2d5  mov      word ptr [0xd12c], sp
0d2d9  cmp      al, 1
0d2db  jne      0xd2e2
0d2dd  call     0xd1a1
0d2e0  jmp      0xd2e5
0d2e2  call     0xd1b5
0d2e5  push     dx
0d2e6  push     cs
0d2e7  pop      ds
0d2e8  call     0xd1fa
0d2eb  mov      word ptr [0xd128], ax
0d2ee  and      dx, dx
0d2f0  je       0xd2f5
0d2f2  jmp      0xd44b
0d2f5  call     0x11161
0d2f8  mov      si, 0x82
0d2fb  xor      ah, ah
0d2fd  mov      al, byte ptr cs:[si]
0d300  sub      al, 0x30
0d302  mov      byte ptr [0x122], al
0d305  call     0xd07a
0d308  mov      bx, 0xfa00
0d30b  mov      ah, 0x48
0d30d  int      0x21
```

Selected strings:

- `0x4b1`: `ERROR!`
- `0x6a2`: `X< r`
- `0xc0f`: `PWV.`
- `0xcc6`: `PWVR.`
- `0xd18`: `Z^_X`
- `0xf0d`: `2200AD4.BIN`
- `0xf4a`: `GX.LIB`
- `0xf60`: `LAST.LIB`
- `0x1031`: `VGATXT.BIN`
- `0x103d`: `EG3TXT.BIN`
- `0x1049`: `EG6TXT.BIN`
- `0x1055`: `TDYTXT.BIN`
- `0x10c2`: `2200GX.EXE`
- `0x2e6a`: `A:\2200AD\SECURITY.HID`
- `0x2ef5`: `!u*0`
- `0x2f05`: `9s6<`
- `0x2f0e`: `s"+4H`
- `0x2f1f`: `(1e~`
- `0x2f3d`: `B~,5B]`
- `0x2f47`: `<D6N`
- `0x2f4f`: `$.4L`
- `0x2f67`: `,6Kj`
- `0x307f`: `0:&J`
- `0x3087`: `;D:X`
- `0x308f`: `@J\`z`
- `0x30af`: ` *\`z`
- `0x30b7`: `%/:X`
- `0x30d7`: `57ow`
- `0x30e7`: `13CK`
- `0x3107`: `*.6B`
- `0x310e`: `t*.FR`
- `0x3116`: `t*.Vb`
- `0x313e`: `u $@H`
- `0x3146`: `u $HP`
- `0x314e`: `u $PX`
- `0x3156`: `u $X\``
- `0x317d`: ` "$%'(*+-.0135689;<>?ABDEGHJKMNPQSTVWYZ\]_\`acdfgijlmnpqstuwxz{|~`
- `0x349a`: ` !!"##$%%&''())**+,,-../001223445667899:;;<==>??@AABCCDEFFGHHIJKKLMMNOPPQRRSTUUVWXXYZ[[\]^^_\`aabcddefghhijkllmnoppqrstuvvwxyz{||}~`
- `0x372b`: `z@xZvjtoripWn9l`
- `0x374d`: `S;PvM<J`

## `2200GX.EXE`

- Size: 46634 bytes
- Entry file offset: `0x0`
- Entry load address: `0x100`
- Syntactic interrupt occurrences: {'0x91': 1, '0x93': 1, '0xce': 1, '0x8e': 1, '0xf9': 1, '0xea': 1, '0x40': 1, '0x80': 1, '0x9f': 1, '0x13': 1, '0x18': 1, '0x21': 1}

```asm
00100  xor      ah, ah
00102  push     cs
00103  pop      es
00104  push     cs
00105  pop      ds
00106  mov      si, 0x15
00109  shl      ax, 1
0010b  add      si, ax
0010d  lodsw    ax, word ptr [si]
0010e  mov      si, 0x14
00111  push     si
00112  push     ax
00113  ret
00114  retf
00115  or       al, 8
00117  and      ax, 0x2557
0011b  mov      ah, byte ptr [di]
0011d  jae      0x144
0011f  adc      bp, word ptr [0x25a1]
00123  adc      al, 0x25
```

Selected strings:

- `0x17`: `&%W%`
- `0x184`: `@D@D,(,(`
- `0x1b0`: `wwww`
- `0x476`: `< PL`
- `0x8ab`: `@YIt`
- `0x12a9`: `(CPQ`
- `0x12c4`: `YXPSQRW`
- `0x12d5`: `_ZY[X`
- `0x13d4`: `(PSQRW2`
- `0x13f6`: `x]_ZY[X`
- `0x14e5`: `xrZ_X`
- `0x1653`: `WQRS`
- `0x16a1`: `[ZY_=`
- `0x1811`: ` "$%'(*+-.0135689;<>?ABDEGHJKMNPQSTVWYZ\]_\`acdfgijlmnpqstuwxz{|~`
- `0x1b2e`: ` !!"##$%%&''())**+,,-../001223445667899:;;<==>??@AABCCDEFFGHHIJKKLMMNOPPQRRSTUUVWXXYZ[[\]^^_\`aabcddefghhijkllmnoppqrstuvvwxyz{||}~`
- `0x1dbf`: `z@xZvjtoripWn9l`
- `0x1de1`: `S;PvM<J`
- `0x1de9`: `FHCo?u;`
- `0x240f`: `SPQ3`
- `0x242e`: `X[R2`
- `0x2814`: `@YIt`
- `0x281f`: `R2D"2`
- `0x284b`: `2D#2`
- `0x2875`: `2D$2`
- `0x2c69`: `I-J-`
- `0x2d53`: `+L8s`
- `0x2d7c`: `+L:s`
- `0x2da6`: `+L<s`
- `0x2e23`: `L(<(<(T(`
- `0x2e32`: `(L(<(<(T( )`
- `0x2e40`: `( )Z(<(<(a(`
- `0x2e52`: `(Z(<(<(a( )`
- `0x4189`: `A6BtB`
- `0x4191`: `B.ClC`
- `0x4199`: `C&DdD`

## `MILL.COM`

- Size: 1445 bytes
- Entry file offset: `0x0`
- Entry load address: `0x100`
- Syntactic interrupt occurrences: {'0x21': 38, '0x10': 6, '0x00': 1, '0x91': 1}

```asm
00100  mov      sp, 0x100
00103  jmp      0x10a
00106  js       0x15e
00108  xor      al, 0x12
0010a  mov      ax, 0x6a5
0010d  sub      ax, 0x100
00110  add      ax, 0x110
00113  mov      cl, 4
00115  shr      ax, cl
00117  mov      bx, ax
00119  mov      ah, 0x4a
0011b  int      0x21
0011d  jae      0x12b
0011f  mov      dx, 0x35a
00122  mov      dx, dx
00124  mov      ah, 9
00126  int      0x21
00128  jmp      0x269
0012b  mov      ax, 0x3508
0012e  int      0x21
00130  mov      word ptr [0x5d7], bx
00134  mov      word ptr [0x5d9], es
00138  mov      ax, 0x3509
0013b  int      0x21
0013d  mov      word ptr [0x5db], bx
00141  mov      word ptr [0x5dd], es
00145  mov      ax, 0x351c
00148  int      0x21
0014a  mov      word ptr [0x5df], bx
0014e  mov      word ptr [0x5e1], es
00152  mov      ax, 0x3524
00155  int      0x21
00157  mov      word ptr [0x5e3], bx
0015b  mov      word ptr [0x5e5], es
```

Selected strings:

- `0x259`: `!Modify Memory error`
- `0x26f`: `$Unable to load subprogram`
- `0x28b`: `$Insufficient Memory`
- `0x2a1`: `$chargen.dat`
- `0x2ae`: `mcga.bin`
- `0x2b7`: `Error during graphic driver operation.$Error during sound driver operation.$`
- `0x306`: `$Welcome To MILLENIUM.`
- `0x31f`: `Please Select Sound Effect Type`
- `0x340`: `By Typing The Appropriate Number.`
- `0x364`: `0 = IBM Speaker`
- `0x375`: `1 = Sound Blaster`
- `0x388`: `2 = Covox Sound Master`
- `0x3a5`: `Thank You. Please Wait...`
- `0x3c0`: `$Thank You For Playing Millenium.`
- `0x3ec`: `EGA Selected.`
- `0x3ff`: `MCGA Selected.`
- `0x477`: `Colour adapter not found.`
- `0x4f9`: `mcga.bin`
- `0x502`: `tandy.bin`
- `0x50c`: `ega320.bin`
- `0x517`: `ega640.bin`
- `0x522`: `vga.bin`
- `0x52a`: `sibm.drv`
- `0x533`: `sadl.drv`
- `0x53c`: `srol.drv`
- `0x545`: `ssbl.drv`
- `0x54e`: `scvx.drv`
- `0x557`: `stdy.drv`
- `0x589`: ` 0 1N`
- `0x58f`: `TITLES.EXE`
- `0x59a`: `2200ad.exe`

## `TITLES.EXE`

- Size: 7022 bytes
- Entry file offset: `0x1a80`
- Entry load address: `0x1b80`
- Syntactic interrupt occurrences: {'0x21': 26, '0x33': 17, '0x15': 10, '0x10': 4, '0x07': 3, '0x91': 1, '0x93': 1, '0x75': 1, '0x74': 1, '0x89': 1, '0x95': 1, '0x58': 1}

```asm
01b80  push     cs
01b81  pop      ds
01b82  push     cs
01b83  pop      es
01b84  mov      ax, cs
01b86  mov      ss, ax
01b88  mov      ax, 0xda00
01b8b  mov      sp, ax
01b8d  mov      ax, 0
01b90  push     cs
01b91  pop      es
01b92  mov      bx, 0x1ac4
01b95  call     0x122
01b98  mov      word ptr cs:[0x1a9c], ax
01b9c  mov      al, ah
01b9e  mov      byte ptr cs:[0x1aaa], al
01ba2  mov      byte ptr [0x107], al
01ba5  mov      word ptr [0x1aa0], sp
01ba9  cmp      al, 1
01bab  jne      0x1bb2
01bad  call     0x1ac6
01bb0  jmp      0x1bb5
01bb2  call     0x1ada
01bb5  push     dx
01bb6  push     cs
01bb7  pop      ds
01bb8  call     0x1b1f
01bbb  mov      word ptr [0x1a9c], ax
01bbe  and      dx, dx
01bc0  je       0x1bc5
01bc2  jmp      0x1c6a
01bc5  mov      bx, 0xfa00
01bc8  mov      ah, 0x48
01bca  int      0x21
01bcc  mov      word ptr cs:[0x1aa4], bx
01bd1  mov      es, ax
01bd3  mov      ah, 0x49
01bd5  int      0x21
01bd7  push     cs
01bd8  pop      ds
01bd9  pop      dx
01bda  lds      dx, ptr [0xe46]
```

Selected strings:

- `0x4ad`: `ERROR!`
- `0x675`: `RSP.`
- `0x68d`: `X< r`
- `0xaff`: `PWV.`
- `0xd12`: `2200AD4.BIN`
- `0xd4e`: `title.lib`
- `0xd74`: `NOT OPEN`
- `0xf26`: `VGATXT.BIN`
- `0xf32`: `EG3TXT.BIN`
- `0xf3e`: `EG6TXT.BIN`
- `0xfa4`: `2200GX.EXE`
- `0x10db`: `VPSQR`
- `0x1115`: `ZY[X^`
- `0x15ec`: `VSQRP`
- `0x1602`: `ZY[^`
- `0x16a4`: `RETURN TO EARTH2   A PARAGON   `
- `0x16c4`: `  PRODUCTION   `
- `0x16d4`: `COPYRIGHT  19912 DESIGNED AND  `
- `0x16f4`: `  WRITTEN BY   `
- `0x1704`: `   IAN BIRD    2 GRAFIX ENGINE `
- `0x1724`: `  GLENN DILL   2ORIGINAL GRAFIX`
- `0x1744`: `    J REDMAN   ( IBM  GRAPHICS `
- `0x1764`: `  STEVE SUHY   2 PRESS ANY KEY 2    LOADING    2`
- `0x18c8`: `A:\2200AD\SECURITY.HID`
