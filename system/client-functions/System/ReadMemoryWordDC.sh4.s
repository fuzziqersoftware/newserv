entry_ptr:
reloc0:
  .offsetof start

start:
  mova   r0, [address]
  mov.l  r0, [r0]
  rets
  mov.l  r0, [r0]

  .align 4
address:
  .data  0
