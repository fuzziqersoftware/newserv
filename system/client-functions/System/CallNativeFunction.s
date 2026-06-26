# This function implements the $nativecall chat command.

entry_ptr:
reloc0:
  .data   start



.versions SH4

start:
  sts.l     -[r15], pr
  mov.l     r0, [call_addr]
  mov.l     r4, [arg0]
  mov.l     r5, [arg1]
  mov.l     r6, [arg2]
  mov.l     r7, [arg3]
  calls     [r0]
  nop
  lds.l     pr, [r15]+
  rets
  nop

  .align 4
call_addr:
  .data     0
arg0:
  .data     0
arg1:
  .data     0
arg2:
  .data     0
arg3:
  .data     0



.versions PPC

start:
  mflr   r0
  stw    [r1 + 4], r0
  stwu   [r1 - 0x20], r1
  bl     resume
call_addr:
  .data  0
arg0:
  .data  0
arg1:
  .data  0
arg2:
  .data  0
arg3:
  .data  0
arg4:
  .data  0
arg5:
  .data  0
arg6:
  .data  0
arg7:
  .data  0
arg8:
  .data  0
arg9:
  .data  0
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
