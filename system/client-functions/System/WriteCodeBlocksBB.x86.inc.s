start:
  push    ebx
  jmp     get_patch_data_ptr
get_patch_data_ptr_ret:
  pop     ebx                   # ebx = patch header

apply_next_patch:
  cmp     dword [ebx + 4], 0
  jne     copy_code_and_apply_again
  pop     ebx
  mov     eax, 1
  ret

copy_code_and_apply_again:
  xor     ecx, ecx              # ecx = offset
  mov     edx, [ebx]            # edx = dest addr
copy_next_byte:
  mov     al, [ebx + ecx + 8]   # copy one byte to dest
  mov     [edx + ecx], al
  inc     ecx                   # offset++
  cmp     [ebx + 4], ecx        # check if all bytes have been copied
  jne     copy_next_byte

  lea     ebx, [ebx + ecx + 8]  # advance to next block
  jmp     apply_next_patch

get_patch_data_ptr:
  call    get_patch_data_ptr_ret
first_patch_header:
