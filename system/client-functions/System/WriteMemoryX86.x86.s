.meta name="Write memory"
.meta description="Writes data to any location in memory"

entry_ptr:
reloc0:
  .offsetof start

start:
  jmp     get_block_ptr
get_block_ptr_ret:
  xchg    ebx, [esp]
  mov     eax, [ebx]
  mov     ecx, [ebx + 4]
  add     ebx, 8

again:
  test    ecx, ecx
  jz      done
  mov     dl, [ebx]
  mov     [eax], dl
  inc     ebx
  inc     eax
  dec     ecx
  jmp     again

done:
  pop     ebx
  ret

get_block_ptr:
  call    get_block_ptr_ret
dest_addr:
  .data   0
size:
  .data   0

data_to_write:
