.meta name="Invisible MAG"
.meta description="Makes MAGs invisible"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

.versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC

  .data     <VERS 0x80114F04 0x80115118 0x8011521C 0x801150B0 0x801151A8 0x801151A8 0x801150C0 0x80115298>
  .data     0x00000004
  .data     0x480000D4

  .data     0x00000000
  .data     0x00000000
