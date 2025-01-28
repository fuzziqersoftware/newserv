.meta name="MAG alert"
.meta description="Plays a sound when\nyour MAG is hungry"

entry_ptr:
reloc0:
  .offsetof start
start:
  pop       ecx
  push      6
  push      0x005D91E2
  call      get_code_size
  .deltaof  patch_code, patch_code_end
get_code_size:
  pop       eax
  push      dword [eax]
  call      patch_code_end
patch_code:
  mov       dword [ecx + 0x01B8], eax
  push      0
  push      0
  push      0
  push      0x0576
  mov       eax, 0x00814298
  call      eax
  add       esp, 0x10
  ret
patch_code_end:
  push      ecx
  .include  WriteCallToCode-59NL
