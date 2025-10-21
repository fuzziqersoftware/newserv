.meta name="DC targets"
.meta description="Changes the target\nreticle colors to\nthose used on the\nDreamcast"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

.versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC

  .data     <VERS 0x802AB3FC 0x802AC2A4 0x802AD3D0 0x802AD184 0x802ABDB8 0x802ABDFC 0x802AD338 0x802ACACC>
  .data     0x00000004
  lis       r4, 0x00FF

  .data     <VERS 0x802AB410 0x802AC2B8 0x802AD3E4 0x802AD198 0x802ABDCC 0x802ABE10 0x802AD34C 0x802ACAE0>
  .data     0x00000004
  li        r4, 0x00FF

  .data     <VERS 0x802AB424 0x802AC2CC 0x802AD3F8 0x802AD1AC 0x802ABDE0 0x802ABE24 0x802AD360 0x802ACAF4>
  .data     0x00000004
  subi      r4, r4, 0x0100

  .data     <VERS 0x804A1F18 0x804A5638 0x804A7AD8 0x804A7898 0x804A26C8 0x804A2BA8 0x804A7168 0x804A75E8>
  .data     0x00000008
  .float    1
  .float    0

  .data     <VERS 0x804A1F28 0x804A5648 0x804A7AE8 0x804A78A8 0x804A26D8 0x804A2BB8 0x804A7178 0x804A75F8>
  .data     0x00000008
  .float    1
  .float    0

  .data     <VERS 0x804A1F38 0x804A5658 0x804A7AF8 0x804A78B8 0x804A26E8 0x804A2BC8 0x804A7188 0x804A7608>
  .data     0x0000000C
  .float    1
  .float    1
  .float    0

  .data     <VERS 0x804A1F48 0x804A5668 0x804A7B08 0x804A78C8 0x804A26F8 0x804A2BD8 0x804A7198 0x804A7618>
  .data     0x00000004
  .float    0

  .data     <VERS 0x804A1F50 0x804A5670 0x804A7B10 0x804A78D0 0x804A2700 0x804A2BE0 0x804A71A0 0x804A7620>
  .data     0x00000004
  .float    1

  .data     <VERS 0x804A1F58 0x804A5678 0x804A7B18 0x804A78D8 0x804A2708 0x804A2BE8 0x804A71A8 0x804A7628>
  .data     0x0000000C
  .float    0.4
  .float    0.1
  .float    0.1

  .data     0x00000000
  .data     0x00000000
