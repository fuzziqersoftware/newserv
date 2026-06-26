.versions X86

entry_ptr:
reloc0:
  .data   start

start:
  call     resume
token:
  .data    0x00000000
resume:
  pop      eax
  mov      eax, [eax]
  ret
