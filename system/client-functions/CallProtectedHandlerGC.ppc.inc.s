  stwu     [r1 - 0x10], r1
  mflr     r0
  stw      [r1 + 0x14], r0
  stw      [r1 + 0x08], r31
  stw      [r1 + 0x0C], r30

  b        get_data_addr
resume:
  mflr     r31

  lwz      r30, [r31]
  li       r0, 1
  stw      [r30], r0

  addi     r3, r31, 0x0C
  lwz      r4, [r31 + 8]
  lwz      r0, [r31 + 4]
  mtctr    r0
  bctrl

  li       r0, 0
  stw      [r30], r0

  lwz      r30, [r1 + 0x0C]
  lwz      r31, [r1 + 0x08]
  lwz      r0, [r1 + 0x14]
  mtlr     r0
  addi     r1, r1, 0x10
  blr

get_data_addr:
  bl       resume
