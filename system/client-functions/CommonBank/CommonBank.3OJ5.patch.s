.meta name="Common bank"
.meta description="Hold L and open\nthe bank to use a\ncommon bank stored\nin temp character\n3's data"
# Original code by Ralf @ GC-Forever ("Common Bank (Hold L And Open Bank)")
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC

  .data     0x8000BAB4
  .deltaof  hook1, hooks_end
  .address  0x8000BAB4

hook1:
  cmplwi    r27, 2
  bne       hook1_skip
  lis       r0, 0x8000
  ori       r0, r0, 0xBAD8
  stw       [r3 + 0x0004], r0
  li        r0, 0x0000
  stw       [r3 + 0x0008], r0
hook1_skip:
  lwz       r3, [r31 + 0x0040]
  blr
  .binary   434F4D4D4F4E2042414E4B00

hook2:
  lwz       r0, [r13 - 0x7150]
  cmplwi    r0, 1
  bne       hook2_skip
  lis       r4, 0x8051
  lhz       r4, [r4 - 0x1690]
  andi.     r0, r4, 0x0002
  beq       hook2_default_bank
  lwz       r0, [r13 - 0x469C]
  cmplwi    r0, 6
  beq       hook2_default_bank
  lwz       r3, [r13 - 0x46B8]
  cmplwi    r3, 0
  beq       hook2_default_bank
  li        r0, 0x0000
  ori       r0, r0, 0xF1B0
  add       r3, r3, r0
hook2_default_bank:
  lis       r4, 0x8001
  stw       [r4 - 0x3CD4], r3
hook2_skip:
  cmplwi    r3, 0
  blr

hook3:
  lwz       r0, [r13 - 0x7150]
  cmplwi    r0, 1
  bne       hook3_skip
  lis       r3, 0x8001
  lwz       r3, [r3 - 0x3CD4]
hook3_skip:
  mr.       r8, r3
  blr
hooks_end:

  .data     0x80210E88
  .data     0x00000004
  .address  0x80210E88
  bl        hook3

  .data     0x80210EFC
  .data     0x00000004
  .address  0x80210EFC
  bl        hook2

  .data     0x8030CCA4
  .data     0x00000004
  .address  0x8030CCA4
  bl        hook1

  .data     0x8030CCFC
  .data     0x00000004
  .address  0x8030CCFC
  bl        hook1

  .data     0x80471C14
  .data     0x00000004
  .address  0x80471C14
  .data     0xFFFFFFFF

  .data     0x00000000
  .data     0x00000000
