.meta name="GC targets"
.meta description="Changes the target\nreticle colors to\nthose used on the\nGameCube"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

.versions 4OED 4OEU 4OJB 4OJD 4OJU 4OPD 4OPU

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksXB

  .data     <VERS 0x0025BD09 0x0025BE29 0x0025B889 0x0025BC39 0x0025BFB9 0x0025BD29 0x0025BE59>
  .data     0x00000004
  .data     0x0000AA0E

  .data     <VERS 0x0025BD17 0x0025BE37 0x0025B897 0x0025BC47 0x0025BFC7 0x0025BD37 0x0025BE67>
  .data     0x00000004
  .data     0x00FF2417

  .data     <VERS 0x0025BD25 0x0025BE45 0x0025B8A5 0x0025BC55 0x0025BFD5 0x0025BD45 0x0025BE75>
  .data     0x00000004
  .data     0x00FFFFFF

  .data     <VERS 0x005427A0 0x00542040 0x0053D788 0x0053DE00 0x00545320 0x005427A0 0x00542B40>
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
