.meta visibility="all"
.meta name="No item loss"
.meta description="Disables logic that\ndeletes items if\nyou don't log off\nnormally"

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocks



  .versions 1OJ4 1OEF 1OPF

  .align    4
  .data     <VERS 0x8C0254B2 0x8C0254BE 0x8C0254D2>
  .data     4
  bs        +0x38
  nop

  .align    4
  .data     <VERS 0x8C150B2C 0x8C150F9C 0x8C150D58>
  .data     2
  sett

  .align    4
  .data     <VERS 0x8C15F346 0x8C15F856 0x8C15F612>
  .data     2
  and       r0, 0xFE

  .align    4
  .data     <VERS 0x8C16053A 0x8C160A4A 0x8C160806>
  .data     2
  and       r0, 0xFE

  .align    4
  .data     <VERS 0x8C1617DA 0x8C161D6A 0x8C161B26>
  .data     2
  and       r0, 0xFE

  .align    4
  .data     <VERS 0x8C15F3BA 0x8C15F8CA 0x8C15F686>
  .data     2
  nop

  .align    4
  .data     <VERS 0x8C1605A6 0x8C160AB6 0x8C160872>
  .data     2
  nop

  .align    4
  .data     <VERS 0x8C161808 0x8C161D98 0x8C161B54>
  .data     2
  nop

  .align    4



  .versions 1OJ2

  .align    4
  .data     0x8C14C71A
  .data     2
  nop
  .align    4



  .versions 2OJ4 2OJ5 2OJF 2OEF 2OPF

  .align    4
  .data     <VERS 0x8C0280AA 0x8C0280AA 0x8C028276 0x8C0280AA 0x8C0280AA>
  .data     6
  nop
  bs        +0x2C
  nop

  .align    4
  .data     <VERS 0x8C16BDFE 0x8C16BDFE 0x8C16B50A 0x8C16BDFE 0x8C16BA22>
  .data     2
  sett

  .align    4
  .data     <VERS 0x8C17F1DC 0x8C17F1DC 0x8C17E738 0x8C17F1DC 0x8C17EC74>
  .data     2
  and       r0, 0xFE

  .align    4
  .data     <VERS 0x8C17F2BA 0x8C17F2BA 0x8C17E816 0x8C17F2BA 0x8C17ED52>
  .data     2
  nop

  .align    4
  .data     <VERS 0x8C180D0A 0x8C180D0A 0x8C18005A 0x8C180D0A 0x8C1807A2>
  .data     2
  and       r0, 0xFE

  .align    4
  .data     <VERS 0x8C180DB0 0x8C180DB0 0x8C180100 0x8C180DB0 0x8C180848>
  .data     2
  nop

  .align    4
  .data     <VERS 0x8C181BC4 0x8C181BC4 0x8C180EC8 0x8C181BC4 0x8C18165C>
  .data     2
  and       r0, 0xFE

  .align    4
  .data     <VERS 0x8C181C92 0x8C181C92 0x8C180F96 0x8C181C92 0x8C18172A>
  .data     2
  nop

  .align    4
  .data     <VERS 0x8C182BC6 0x8C182BC6 0x8C181DBE 0x8C182BC6 0x8C18265E>
  .data     2
  and       r0, 0xFE

  .align    4
  .data     <VERS 0x8C182BF4 0x8C182BF4 0x8C181DEC 0x8C182BF4 0x8C18268C>
  .data     2
  nop

  .align    4
  .data     <VERS 0x8C1834D0 0x8C1834D0 0x8C1825F0 0x8C1834D0 0x8C182F68>
  .data     2
  and       r0, 0xFE

  .align    4



  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

  .data     <VERS 0x801D33E4 0x801D38EC 0x801D3CC4 0x801D39B8 0x801D381C 0x801D381C 0x801D3A1C 0x801D3ED8>
  .data     0x00000004
  b         +0x4C

  .data     <VERS 0x801FE900 0x801FF174 0x8020010C 0x801FF710 0x801FF0FC 0x801FF0FC 0x801FFA44 0x801FF9E0>
  .data     0x00000004
  nop

  .data     <VERS 0x801FFE5C 0x802006D0 0x802016CC 0x80200C9C 0x80200658 0x80200658 0x80200FD0 0x80200F3C>
  .data     0x00000004
  nop

  .data     <VERS 0x802019C8 0x8020223C 0x801FD944 0x80202860 0x802021C4 0x802021C4 0x80202B94 0x80202AA8>
  .data     0x00000004
  li       r0, 0

  .data     <VERS 0x802C2060 0x802C2F98 0x802C42E4 0x802C3E78 0x802C2A40 0x802C2A84 0x802C402C 0x802C37C0>
  .data     0x00000004
  b         +0x4C

  .data     <VERS 0x802D0AA0 0x802D1A58 0x802D2C10 0x802D2938 0x802D1480 0x802D14C4 0x802D2AEC 0x802D2280>
  .data     0x00000004
  b         +0x20



  .versions 4OED 4OEU 4OJB 4OJD 4OJU 4OPD 4OPU

  .data     <VERS 0x000D1B85 0x000D1BD5 0x000D1A35 0x000D1B05 0x000D1AF5 0x000D1BA5 0x000D1BD5>
  .data     0x00000001
  .binary   00

  .data     <VERS 0x000D1BFC 0x000D1C4C 0x000D1AAC 0x000D1B7C 0x000D1B6C 0x000D1C1C 0x000D1C4C>
  .data     0x00000002
  .binary   EB08

  .data     <VERS 0x0020E805 0x0020E805 0x0020E5D5 0x0020E755 0x0020E9D5 0x0020E825 0x0020E895>
  .data     0x00000001
  .binary   EB

  .data     <VERS 0x002119CA 0x00211A2A 0x0021170A 0x0021197A 0x00211BFA 0x002119EA 0x00211ABA>
  .data     0x00000002
  .binary   EB74

  .data     <VERS 0x002291B5 0x00229255 0x00228F15 0x00229125 0x00229415 0x002291D5 0x002292E5>
  .data     0x00000002
  .binary   9090

  .data     <VERS 0x00229237 0x002292D7 0x00228F97 0x002291A7 0x00229497 0x00229257 0x00229367>
  .data     0x00000002
  .binary   EB08

  .data     <VERS 0x0022A222 0x0022A2C2 0x00229F82 0x0022A192 0x0022A482 0x0022A242 0x0022A352>
  .data     0x00000002
  .binary   9090

  .data     <VERS 0x0022A29B 0x0022A33B 0x00229FFB 0x0022A20B 0x0022A4FB 0x0022A2BB 0x0022A3CB>
  .data     0x00000002
  .binary   EB08

  .data     <VERS 0x0022BF35 0x0022BFD5 0x0022BC95 0x0022BEA5 0x0022C195 0x0022BF55 0x0022C065>
  .data     0x00000001
  .binary   00

  .data     <VERS 0x0022BF6E 0x0022C00E 0x0022BCCE 0x0022BEDE 0x0022C1CE 0x0022BF8E 0x0022C09E>
  .data     0x00000002
  .binary   EB08

  .data     <VERS 0x0022C2E6 0x0022C386 0x0022C046 0x0022C256 0x0022C546 0x0022C306 0x0022C416>
  .data     0x00000001
  .binary   00

  .data     <VERS 0x002418E8 0x00241A78 0x00241608 0x00241858 0x00241BD8 0x00241908 0x00241B08>
  .data     0x00000001
  .binary   00

  .data     <VERS 0x0024195C 0x00241AEC 0x0024167C 0x002418CC 0x00241C4C 0x0024197C 0x00241B7C>
  .data     0x00000002
  .binary   EB08

  .data     <VERS 0x002A2904 0x002A2B34 0x002A0FA4 0x002A19F4 0x002A2EC4 0x002A2924 0x002A2BF4>
  .data     0x00000001
  .binary   00

  .data     <VERS 0x002A297C 0x002A2BAC 0x002A101C 0x002A1A6C 0x002A2F3C 0x002A299C 0x002A2C6C>
  .data     0x00000002
  .binary   EB08

  .data     <VERS 0x002D677A 0x002D6C8A 0x002D481A 0x002D53DA 0x002D6CBA 0x002D67AA 0x002D6D0A>
  .data     0x00000001
  .binary   00

  .data     <VERS 0x002D67ED 0x002D6CFD 0x002D488D 0x002D544D 0x002D6D2D 0x002D681D 0x002D6D7D>
  .data     0x00000002
  .binary   EB08

  .data     <VERS 0x002F0E1E 0x002F0FCE 0x002EEEBE 0x002EF9CE 0x002F0FCE 0x002F0E4E 0x002F103E>
  .data     0x00000001
  .binary   EB



  .all_versions

  .data     0x00000000
  .data     0x00000000
