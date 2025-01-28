.meta name="Blinking SD"
.meta description="Makes the Shifta\nand Deband status\nicons flicker when\nthey are about\nto run out"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC

  .data     0x8000B86C
  .data     0x00000054
code_start:
  mr        r3, r0
  andi.     r0, r31, 2
  beqlr
  lwz       r4, [r3 + 0x0028]
  cmplwi    r4, 0
  beqlr
  lwz       r4, [r4]
  cmplwi    r4, 0
  beqlr
  mulli     r0, r31, 12
  add       r5, r29, r0
  lwz       r6, [r5 + 0x025C]
  cmplwi    r6, 450
  bge       full_intensity
  lbz       r6, [r4 + 0x002C]
  subi      r6, r6, 0x0008
  cmpwi     r6, 0
  bge       not_full_intensity
full_intensity:
  li        r6, 0x00FF
not_full_intensity:
  stb       [r4 + 0x002C], r6
  blr
code_end:

  .data     0x8026FC1C
  .data     4
  .data     0x4BD9BC51

  .data     0
  .data     0
