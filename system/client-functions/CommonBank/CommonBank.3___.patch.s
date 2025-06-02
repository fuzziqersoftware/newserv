.meta name="Common bank"
.meta description="Hold L and open\nthe bank to use a\ncommon bank stored\nin temp character\n3's data"
# Original code by Ralf @ GC-Forever ("Common Bank (Hold L And Open Bank)")
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

.versions 3OE0 3OE1 3OE2 3OJ2 3OJ3 3OJ4 3OJ5 3OP0

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
  lwz       r0, [r13 - <VERS 0x7148 0x7148 0x7148 0x7150 0x7150 0x7150 0x7150 0x7148>]
  cmplwi    r0, 1
  bne       hook2_skip
  lis       r4, 0x8051
  lhz       r4, [r4 - <VERS 0x6C50 0x6770 0x1D90 0x7530 0x3A70 0x1430 0x1690 0x0D70>]
  andi.     r0, r4, 0x0002
  beq       hook2_default_bank
  lwz       r0, [r13 - <VERS 0x46AC 0x46AC 0x468C 0x46C4 0x46BC 0x469C 0x469C 0x464C>]
  cmplwi    r0, 6
  beq       hook2_default_bank
  lwz       r3, [r13 - <VERS 0x46C8 0x46C8 0x46A8 0x46E0 0x46D8 0x46B8 0x46B8 0x4668>]
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
  lwz       r0, [r13 - <VERS 0x7148 0x7148 0x7148 0x7150 0x7150 0x7150 0x7150 0x7148>]
  cmplwi    r0, 1
  bne       hook3_skip
  lis       r3, 0x8001
  lwz       r3, [r3 - 0x3CD4]
hook3_skip:
  mr.       r8, r3
  blr
hooks_end:

  .data     <VERS 0x8021026C 0x8021026C 0x802111BC 0x8020F9F8 0x8021034C 0x8021112C 0x80210E88 0x80210BB8>
  .data     0x00000004
  .address  <VERS 0x8021026C 0x8021026C 0x802111BC 0x8020F9F8 0x8021034C 0x8021112C 0x80210E88 0x80210BB8>
  bl        hook3

  .data     <VERS 0x802102E0 0x802102E0 0x80211230 0x8020FAE4 0x802103C0 0x802111A0 0x80210EFC 0x80210C2C>
  .data     0x00000004
  .address  <VERS 0x802102E0 0x802102E0 0x80211230 0x8020FAE4 0x802103C0 0x802111A0 0x80210EFC 0x80210C2C>
  bl        hook2

  .data     <VERS 0x8030B414 0x8030B458 0x8030CE60 0x8030AA54 0x8030BAA4 0x8030CEF0 0x8030CCA4 0x8030C228>
  .data     0x00000004
  .address  <VERS 0x8030B414 0x8030B458 0x8030CE60 0x8030AA54 0x8030BAA4 0x8030CEF0 0x8030CCA4 0x8030C228>
  bl        hook1

  .data     <VERS 0x8030B46C 0x8030B4B0 0x8030CEB8 0x8030AAAC 0x8030BAFC 0x8030CF48 0x8030CCFC 0x8030C280>
  .data     0x00000004
  .address  <VERS 0x8030B46C 0x8030B4B0 0x8030CEB8 0x8030AAAC 0x8030BAFC 0x8030CF48 0x8030CCFC 0x8030C280>
  bl        hook1

  .data     <VERS 0x8046DC5C 0x8046E0DC 0x80471ACC 0x8046CECC 0x8046FCEC 0x80471E4C 0x80471C14 0x80471804>
  .data     0x00000004
  .address  <VERS 0x8046DC5C 0x8046E0DC 0x80471ACC 0x8046CECC 0x8046FCEC 0x80471E4C 0x80471C14 0x80471804>
  .data     0xFFFFFFFF

  .data     0x00000000
  .data     0x00000000
