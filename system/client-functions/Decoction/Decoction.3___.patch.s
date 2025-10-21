.meta name="Decoction"
.meta description="Makes the Decoction\nitem reset your\nmaterial usage"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

.versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC

  .data     <VERS 0x80350740 0x80351B44 0x803530A0 0x80352E54 0x803515F4 0x80351638 0x80353220 0x80352614>
  .data     0x00000098
  .address  <VERS 0x80350740 0x80351B44 0x803530A0 0x80352E54 0x803515F4 0x80351638 0x80353220 0x80352614>
  lbz       r0, [r3 + 0xEE]
  cmplwi    r0, 11
  bne       +0x144
  lwz       r31, [r3 + 0xF0]
  li        r0, 0
  nop
  li        r4, 0x0374
  li        r5, 0x0D38
  bl        +0x58
  li        r5, 0x0D3A
  bl        +0x50
  li        r5, 0x0D3C
  bl        +0x48
  li        r5, 0x0D40
  bl        +0x40
  li        r5, 0x0D44
  bl        +0x38
  mr        r3, r31
  .data     <VERS 0x4BE656A1 0x4BE646F1 0x4BE654CD 0x4BE634AD 0x4BE64BD9 0x4BE64B95 0x4BE63145 0x4BE6420D>
  lhz       r0, [r31 + 0x032C]
  lhz       r3, [r31 + 0x02B8]
  cmpl      r0, r3
  ble       +0x08
  sth       [r31 + 0x032C], r3
  lhz       r0, [r31 + 0x032E]
  lhz       r3, [r31 + 0x02BA]
  cmpl      r0, r3
  ble       +0x08
  sth       [r31 + 0x032E], r3
  b         +0xD8
  lbzx      r6, [r31 + r4]
  lhzx      r7, [r31 + r5]
  rlwinm    r6, r6, 1, 0, 30
  subf      r7, r6, r7
  sthx      [r31 + r5], r7
  stbx      [r31 + r4], r0
  addi      r4, r4, 0x0001
  blr

  .data     0x00000000
  .data     0x00000000
