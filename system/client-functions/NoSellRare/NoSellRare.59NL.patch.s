# Credits to Soly from Blue Burst Patch Project

.meta name="No rare selling"
.meta description="Stops you from accidentally\nselling rares to vendor"

entry_ptr:
reloc0:
  .offsetof start
start:
  push    ebx
  jmp     get_patch_data_ptr

get_patch_data_ptr_ret:
  pop     ebx                   # ebx = patch header

apply_next_patch:
  cmp       dword [ebx + 4], 0
  jne       copy_code_and_apply_again
  pop       ebx
  jmp       patch_code_start

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
  .data     0x005D25AF          # Rare Armor
  .data     0x00000004
  .data     0x00000000

  .data     0x005D26F1          # Untekked Weapons
  .data     0x00000004
  .data     0x00000000

  .data     0x005D2706          # Rare Weapons
  .data     0x00000004
  .data     0x00000000

  .data     0x00000000
  .data     0x00000000

patch_code_start:
  pop       ecx
  push      5
  push      0x005D2528
  call      get_code_size
  .deltaof  patch_code, patch_code_end

get_code_size:
  pop       eax
  push      dword [eax]
  call      patch_code_end

patch_code:
  mov       edi, 0x005D2576     # change return address
  mov       [esp], edi
  mov       edi, [eax + 0x14]
  and       edi, 0x80
  je        _not_rare
  mov       edi, 0
  ret
_not_rare:
  mov       edi, [eax + 0x10]
  ret
patch_code_end:
  push      ecx
  .include  WriteCallToCode-59NL
