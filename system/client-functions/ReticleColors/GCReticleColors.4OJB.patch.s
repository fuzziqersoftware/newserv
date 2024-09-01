.meta name="GC targets"
.meta description="Changes the target\nreticle colors to\nthose used on the\nGameCube"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksXB

  .data     0x0025B889
  .data     0x00000004
  .data     0x0000AA0E

  .data     0x0025B897
  .data     0x00000004
  .data     0x00FF2417

  .data     0x0025B8A5
  .data     0x00000004
  .data     0x00FFFFFF

  .data     0x0053D788
  .data     0x00000060
  .data     0x3F800000
  .data     0x00000000
  .data     0x3F47AE14
  .data     0x00000000
  .data     0x3F800000
  .data     0x00000000
  .data     0x3F47AE14
  .data     0x00000000
  .data     0x3F800000
  .data     0x3F333333
  .data     0x3F333333
  .data     0x3F333333
  .data     0x3F800000
  .data     0x3F800000
  .data     0x00000000
  .data     0x00000000
  .data     0x3F800000
  .data     0x00000000
  .data     0x3EC7AE14
  .data     0x00000000
  .data     0x3F800000
  .data     0x00000000
  .data     0x00000000
  .data     0x00000000

  .data     0x00000000
  .data     0x00000000
