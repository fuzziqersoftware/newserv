.meta name="Xbox/BB targets"
.meta description="Changes the target\nreticle colors to\nthose used on the\nXbox and Blue Burst"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

.versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC

  .data     <VERS 0x802AB424 0x802AC2CC 0x802AD3F8 0x802AD1AC 0x802ABDE0 0x802ABE24 0x802AD360 0x802ACAF4>
  .data     0x00000004
  li        r4, 0xFF00

  .data     <VERS 0x804A1F38 0x804A5658 0x804A7AF8 0x804A78B8 0x804A26E8 0x804A2BC8 0x804A7188 0x804A7608>
  .data     0x0000000C
  .float    0
  .float    0
  .float    1

  .data     0x00000000
  .data     0x00000000
