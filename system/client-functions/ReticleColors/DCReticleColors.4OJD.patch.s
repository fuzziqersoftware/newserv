.meta name="DC targets"
.meta description="Changes the target\nreticle colors to\nthose used on the\nDreamcast"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksXB

  .data     0x0025BC39
  .data     0x00000004
  .data     0x00FF0000

  .data     0x0025BC47
  .data     0x00000004
  .data     0x000000FF

  .data     0x0025BC55
  .data     0x00000004
  .data     0x00FFFF00

  .data     0x0053DE00
  .data     0x00000060
  .data     0x3F800000
  .data     0x3F800000
  .data     0x00000000
  .data     0x00000000
  .data     0x3F800000
  .data     0x3F800000
  .data     0x00000000
  .data     0x00000000
  .data     0x3F800000
  .data     0x3F800000
  .data     0x3F800000
  .data     0x00000000
  .data     0x3F800000
  .data     0x00000000
  .data     0x00000000
  .data     0x3F800000
  .data     0x3F800000
  .data     0x3ECCCCCD
  .data     0x3DCCCCCD
  .data     0x3DCCCCCD
  .data     0x3F800000
  .data     0x00000000
  .data     0x00000000
  .data     0x00000000

  .data     0x00000000
  .data     0x00000000
