# This function implements the $nativecall chat command on GameCube clients.

entry_ptr:
reloc0:
  .offsetof start

start:
  mflr   r0
  stw    [r1 + 4], r0
  stwu   [r1 - 0x20], r1
  bl     resume
call_addr:
  .zero
arg0:
  .zero
arg1:
  .zero
arg2:
  .zero
arg3:
  .zero
arg4:
  .zero
arg5:
  .zero
arg6:
  .zero
arg7:
  .zero
arg8:
  .zero
arg9:
  .zero
resume:
  mflr   r12
  lwz    r0, [r12]          # call_addr
  lwz    r3, [r12 + 0x04]   # arg0
  lwz    r4, [r12 + 0x08]   # arg1
  lwz    r5, [r12 + 0x0C]   # arg2
  lwz    r6, [r12 + 0x10]   # arg3
  lwz    r7, [r12 + 0x14]   # arg4
  lwz    r8, [r12 + 0x18]   # arg5
  lwz    r9, [r12 + 0x1C]   # arg6
  lwz    r10, [r12 + 0x20]  # arg7
  lwz    r11, [r12 + 0x24]  # arg8
  lwz    r12, [r12 + 0x28]  # arg9
  mtctr  r0
  bctrl
  addi   r1, r1, 0x20
  lwz    r0, [r1 + 4]
  mtlr   r0
  blr
