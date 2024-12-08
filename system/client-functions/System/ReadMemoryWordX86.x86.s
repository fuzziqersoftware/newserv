entry_ptr:
reloc0:
  .offsetof start

start:
  call   resume
address:
  .data  0
resume:
  pop    eax
  mov    eax, [eax]
  mov    eax, [eax]
  ret
