# This function implements the $nativecall chat command on DC clients.

entry_ptr:
reloc0:
  .offsetof start

start:
  sts.l     -[r15], pr
  mov.l     r0, [call_addr]
  mov.l     r4, [arg0]
  mov.l     r5, [arg1]
  mov.l     r6, [arg2]
  mov.l     r7, [arg3]
  calls     r0
  nop
  lds.l     pr, [r15]+
  rets
  nop

  .align 4
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
