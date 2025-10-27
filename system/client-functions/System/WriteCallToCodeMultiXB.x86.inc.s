# This function has the same signature as WriteCallToCodeMulti-59NL.

write_call_to_code:
  .include  GetVersionInfoXB
  push      ebx
  push      ebp
  push      esi
  push      edi
  mov       edi, eax

  # [esp + 0x14] = code ptr
  # [esp + 0x18] = code size
  # [esp + 0x1C] = callsite count
  # [esp + 0x20] = callsite address
  # [esp + 0x24] = callsite size (if zero, write absolute address instead)
  # ... (further callsite address/size pairs)
  # esi = allocated code addr
  # edi = version_info

  # Allocate memory for the copied code
  mov       ecx, [esp + 0x18]
  mov       edx, [edi + 0x14]
  mov       edx, [edx]
  call      [edi + 0x10]  # malloc7(code_size, version_info->malloc7_instance)
  test      eax, eax
  je        done
  mov       esi, eax

  # Copy the code to the newly-allocated memory
  # eax = dest pointer (from malloc7 call above)
  mov       edx, [esp + 0x14]  # edx = source pointer
  mov       ecx, [esp + 0x18]  # ecx = source size
memcpy_again:
  dec       ecx
  mov       bl, [edx + ecx]  # Copy one byte from source to dest
  mov       [esi + ecx], bl
  test      ecx, ecx
  jne       memcpy_again

  # Make the memory executable
  push      0x40
  push      dword [esp + 0x1C]
  push      esi
  mov       ecx, [edi + 0x08]
  call      [ecx] # MmSetAddressProtect(dest_addr, code_size, XBOX_PAGE_EXECUTE_READWRITE)

  # Write the call opcodes
  mov       ebx, [esp + 0x1C]  # ebx = callsite count
  mov       ebp, 0x20  # Stack offset of first callsite pair

next_callsite:
  # Make the memory writable
  push      esi
  mov       ecx, [edi + 0x0C]
  call      [ecx]  # MmQueryAddressProtect(callsite_addr)
  push      eax

  mov       edx, 4
  push      edx  # XBOX_PAGE_READWRITE
  mov       ecx, [esp + ebp + 0x0C]  # callsite_size
  test      ecx, ecx
  cmovz     ecx, edx
  push      ecx
  push      dword [esp + ebp + 0x0C]
  mov       ecx, [edi + 0x08]
  call      [ecx]  # MmSetAddressProtect(callsite_addr, callsite_size, XBOX_PAGE_READWRITE)

  mov       edx, [esp + ebp + 4]  # edx = callsite addr
  mov       eax, [esp + ebp + 8]  # eax = callsite size
  test      eax, eax
  jnz       write_call_opcode_and_nops
write_address:
  mov       [edx], esi
  jmp       this_callsite_done

write_call_opcode_and_nops:
  lea       ecx, [esi - 5]
  sub       ecx, edx  # ecx = (dest code addr) - (callsite addr) - 5
  mov       byte [edx], 0xE8
  mov       [edx + 1], ecx  # Write E8 (call) followed by delta

  # Write as many nops after the call opcode as necessary
  mov       ecx, 5
write_nop_again:
  cmp       ecx, eax
  jge       this_callsite_done
  mov       byte [edx + ecx], 0x90
  inc       ecx
  jmp       write_nop_again

this_callsite_done:
  # Restore the previous protection
  # Previous protection is still on the stack from MmQueryAddressProtect call
  mov       edx, 4
  mov       ecx, [esp + ebp + 8]
  test      ecx, ecx
  cmovz     ecx, edx
  push      ecx
  push      dword [esp + ebp + 8]
  mov       ecx, [edi + 0x08]
  call      [ecx]  # MmSetAddressProtect(callsite_addr, callsite_size, prev_protection)

  add       ebp, 8
  dec       ebx
  jnz       next_callsite

  mov       ecx, ebp

done:
  mov       edi, [esp]
  mov       esi, [esp + 0x04]
  mov       ebp, [esp + 0x08]
  mov       ebx, [esp + 0x0C]
  mov       eax, [esp + 0x10]
  add       esp, ecx
  jmp       eax
