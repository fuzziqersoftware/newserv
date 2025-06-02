.meta name="No item loss"
.meta description="Disables logic that\ndeletes items if\nyou don't log off\nnormally"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

.versions 3OE0 3OE1 3OE2 3OJ2 3OJ3 3OJ4 3OJ5 3OP0

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC

  .data     <VERS 0x801D381C 0x801D381C 0x801D3A1C 0x801D33E4 0x801D38EC 0x801D3CC4 0x801D39B8 0x801D3ED8>
  .data     0x00000004
  b         +0x4C

  .data     <VERS 0x801FF0FC 0x801FF0FC 0x801FFA44 0x801FE900 0x801FF174 0x8020010C 0x801FF710 0x801FF9E0>
  .data     0x00000004
  nop

  .data     <VERS 0x80200658 0x80200658 0x80200FD0 0x801FFE5C 0x802006D0 0x802016CC 0x80200C9C 0x80200F3C>
  .data     0x00000004
  nop

  .data     <VERS 0x802021C4 0x802021C4 0x80202B94 0x802019C8 0x8020223C 0x801FD944 0x80202860 0x80202AA8>
  .data     0x00000004
  li       r0, 0

  .data     <VERS 0x802C2A40 0x802C2A84 0x802C402C 0x802C2060 0x802C2F98 0x802C42E4 0x802C3E78 0x802C37C0>
  .data     0x00000004
  b         +0x4C

  .data     <VERS 0x802D1480 0x802D14C4 0x802D2AEC 0x802D0AA0 0x802D1A58 0x802D2C10 0x802D2938 0x802D2280>
  .data     0x00000004
  b         +0x20

  .data     0x00000000
  .data     0x00000000
