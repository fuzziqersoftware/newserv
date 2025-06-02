.meta hide_from_patches_menu
.meta name="CallProtectedHandler"
.meta description=""

.versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

entry_ptr:
reloc0:
  .offsetof start
start:
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
  .data     <VERS 0x805C4D58 0x805CF320 0x805D67A0 0x805D6540 0x805C5650 0x805CC630 0x805D5E50 0x805D2090>
  .data     <VERS 0x801E3B38 0x801E40BC 0x801E4290 0x801E4008 0x801E3F9C 0x801E3F9C 0x801E405C 0x801E4698>

size:
  .data     0x00000000
data:
