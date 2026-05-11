# This function is required for loading DOLs. If it's not present, newserv can't serve DOL files to GameCube clients.

entry_ptr:
reloc0:
  .offsetof start



.versions SH4

start:
  mova   r0, [address]
  mov.l  r0, [r0]
  rets
  mov.l  r0, [r0]

  .align 4
address:
  .data  0



.versions PPC

start:
  mflr   r12
  bl     read
address:
  .zero
read:
  mflr   r3
  lwz    r3, [r3]
  lwz    r3, [r3]
  mtlr   r12
  blr



.versions X86

start:
  call   resume
address:
  .data  0
resume:
  pop    eax
  mov    eax, [eax]
  mov    eax, [eax]
  ret
