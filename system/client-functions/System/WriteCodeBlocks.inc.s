.versions SH4

  mova    r0, [first_patch_header]
  mov     r7, r0       # r7 = read ptr
  xor     r3, r3
  dec     r3
  shl     r3, 2        # r3 = 0xFFFFFFFC (mask for aligning r7)
apply_patch:
  add     r7, 3
  and     r7, r3       # r7 = (r7 + 3) & (~3) (align to 4-byte boundary)
  mov.l   r4, [r7]+    # r4 = dest addr
  mov.l   r5, [r7]+
  add     r5, r4       # r5 = dest end ptr (dest addr + size)
  cmpeq   r4, r5       # if (size == 0) return
  bt      done

again:
  cmpeq   r4, r5
  bt      apply_patch  # if (r4 == r5) done with the patch; go to next header
  mov.b   r0, [r7]+
  mov.b   [r4], r0     # *(r4) = *(r7++);
  bs      again        # r4++; continue
  add     r4, 1

done:
  rets
  nop

  .align 4
first_patch_header:



.versions PPC

  mflr    r8
  b       get_patch_data_ptr
get_patch_data_ptr_ret:
  mflr    r7  # r7 = patch header
apply_patch:
  addi    r4, r7, 8  # r4 = start of patch data
  lwz     r3, [r4 - 8]  # r3 = patch dest address
  lwz     r5, [r4 - 4]  # r5 = patch data size
  or      r0, r3, r5
  cmplwi  r0, 0
  mtlr    r8
  beqlr
  add     r7, r4, r5  # r7 = next patch header
  .include CopyCode
  b       apply_patch

get_patch_data_ptr:
  bl      get_patch_data_ptr_ret

first_patch_header:



.versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU

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



.versions 2OJW 2OJZ 59NJ 59NL

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
