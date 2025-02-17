.meta index=E5

entry_ptr:
reloc0:
  .offsetof start

start:
  call     resume
token:
  .data    0x00000000
resume:
  pop      eax
  mov      eax, [eax]
  ret
