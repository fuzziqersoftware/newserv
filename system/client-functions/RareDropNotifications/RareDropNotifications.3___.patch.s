.meta name="Rare alerts"
.meta description="Shows rare items on\nthe map and plays a\nsound when a rare\nitem drops"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

.versions 3OE0 3OE1 3OE2 3OJ2 3OJ3 3OJ4 3OJ5 3OP0

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC

  .data     0x8000C660
  .data     0x00000028
  .data     0x881F00EF
  .data     0x28000004
  .data     0x40820018
  .data     0x387F0038
  .data     0x3C80FFFF
  .data     0x38A00001
  .data     0x38C00000
  .data     <VERS 0x481ED381 0x481ED381 0x481ED511 0x481ECE15 0x481ED4B1 0x481ED709 0x481ED4BD 0x481EDA8D>
  .data     0x7FE3FB78
  .data     <VERS 0x480F6240 0x480F6240 0x480F6108 0x480F5F9C 0x480F6178 0x480F6788 0x480F60F8 0x480F62F8>

  .data     0x8000C690
  .data     0x0000002C
  .data     0x28030000
  .data     0x41820020
  .data     0x880300EF
  .data     0x28000004
  .data     0x40820014
  .data     0x3C600005
  .data     0x60632813
  .data     0x38800000
  .data     <VERS 0x4802721D 0x4802721D 0x480271E5 0x48026FFD 0x4802702D 0x48027049 0x48026FDD 0x4802725D>
  .data     0x80010024
  .data     <VERS 0x4810E8F0 0x4810E8F0 0x4810E810 0x4810E64C 0x4810E868 0x4810EA38 0x4810E800 0x4810E9E8>

  .data     <VERS 0x801028C0 0x801028C0 0x80102788 0x8010261C 0x801027F8 0x80102E08 0x80102778 0x80102978>
  .data     0x00000004
  .data     <VERS 0x4BF09DA0 0x4BF09DA0 0x4BF09ED8 0x4BF0A044 0x4BF09E68 0x4BF09858 0x4BF09EE8 0x4BF09CE8>

  .data     <VERS 0x8011AFA4 0x8011AFA4 0x8011AEC4 0x8011AD00 0x8011AF1C 0x8011B0EC 0x8011AEB4 0x8011B09C>
  .data     0x00000004
  .data     <VERS 0x4BEF16EC 0x4BEF16EC 0x4BEF17CC 0x4BEF1990 0x4BEF1774 0x4BEF15A4 0x4BEF17DC 0x4BEF15F4>

  .data     0x00000000
  .data     0x00000000
