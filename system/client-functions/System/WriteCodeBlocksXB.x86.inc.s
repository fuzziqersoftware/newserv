start:
  .include GetVersionInfoXB
  test     eax, eax
  jnz      can_patch
  ret

can_patch:
  push     esi
  push     edi
  push     ebx
  mov      edi, eax              # edi = ptr to version info struct
  jmp      get_patch_data_ptr
get_patch_data_ptr_ret:
  pop      ebx                   # ebx = patch header

apply_next_patch:
  cmp      dword [ebx + 4], 0
  jne      copy_code_and_apply_again
  pop      ebx
  pop      edi
  pop      esi
  mov      eax, 1
  ret

copy_code_and_apply_again:
  push     dword [ebx]           # dest addr
  mov      ecx, [edi + 0x0C]
  call     [ecx]                 # MmQueryAddressProtect
  mov      esi, eax              # esi = prev protection flags

  push     4                     # new protection flags
  push     dword [ebx + 4]       # size
  push     dword [ebx]           # base address
  mov      ecx, [edi + 0x08]
  call     [ecx]                 # MmSetAddressProtect

  xor      ecx, ecx              # ecx = offset
  mov      edx, [ebx]            # edx = dest addr
copy_next_byte:
  mov      al, [ebx + ecx + 8]   # copy one byte to dest
  mov      [edx + ecx], al
  inc      ecx                   # offset++
  cmp      [ebx + 4], ecx        # check if all bytes have been copied
  jne      copy_next_byte

  push     esi                   # new protection flags
  push     dword [ebx + 4]       # size
  push     dword [ebx]           # base address
  lea      ebx, [ebx + ecx + 8]  # advance to next block
  mov      ecx, [edi + 0x08]
  call     [ecx]                 # MmSetAddressProtect
  jmp      apply_next_patch

get_patch_data_ptr:
  call     get_patch_data_ptr_ret

first_patch_header:
