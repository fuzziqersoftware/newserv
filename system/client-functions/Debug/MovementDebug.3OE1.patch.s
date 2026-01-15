.meta hide_from_patches_menu
.meta name="MovementDebug"
.meta description=""
.meta show_return_value

# Usage examples:
#   Read movement data 09 fparam1:
#     $patch MovementDebug e=0x09 f=1 r=1
#   Write to movement data 09 iparam6:
#     $patch MovementDebug e=0x09 i=6 v=8
#   Write to movement data 36 fparam1 (v is interpreted as float if it contains a '.'):
#     $patch MovementDebug e=0x36 f=1 v=60.0

entry_ptr:
reloc0:
  .offsetof start
start:
  mflr      r12
  b         get_data_ptr
get_data_ptr_ret:
  mflr      r11
  mtlr      r12

  li        r3, 0
  lwz       r7, [r11]  # table index
  cmplwi    r7, 0x60
  bgelr

  lwz       r8, [r13 - 0x5B70]
  mulli     r7, r7, 0x30
  add       r7, r7, r8

  lwz       r0, [r11 + 12]  # value
  lwz       r9, [r11 + 16]  # read-only

  lwz       r4, [r11 + 4]  # fparam number
  subi      r4, r4, 1
  cmplwi    r4, 6
  bge       not_fparam
  rlwinm    r4, r4, 2, 0, 31
  lwzx      r3, [r7 + r4]
  cmplwi    r9, 0
  bnelr
  stwx      [r7 + r4], r0
  blr

not_fparam:
  lwz       r4, [r11 + 8]  # iparam number
  subi      r4, r4, 1
  cmplwi    r4, 6
  bgelr
  rlwinm    r4, r4, 2, 0, 31
  addi      r4, r4, 0x18
  lwzx      r3, [r7 + r4]
  cmplwi    r9, 0
  bnelr
  stwx      [r7 + r4], r0
  blr

get_data_ptr:
  bl        get_data_ptr_ret
e:  # Movement data index
  .data     0xFFFFFFFF
f:  # Float param index
  .zero
i:  # Int parameter index
  .zero
v:  # Value
  .zero
r:  # Read-only
  .zero
