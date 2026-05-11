# This file defines the following function:
#   void [/std] write_call_to_code(
#     const void* patch_code @ [esp + 0x04],
#     size_t patch_code_size @ [esp + 0x08],
#     size_t call_count @ [esp + 0x0C],
#     void* call_opcode_address @ [esp + 0x10],
#     ssize_t call_opcode_bytes @ [esp + 0x14],
#     ...);
# This function allocates memory for patch_code, copies patch_code to that memory, then writes a call or jmp opcode to
# call_opcode_address that calls the code in the allocated memory region. The allocated memory is never freed.
# call_opcode_bytes specifies how many bytes at the callsite should be overwritten. This value must be at least 5; the
# first 5 bytes are overwritten with the call/jmp opcode itself; the rest are overwritten with nop opcodes. This
# function pops its arguments off the stack before returning (including all the varargs).



.versions 59NJ 59NL

write_call_to_code:
  # [esp + 0x04] = code ptr
  # [esp + 0x08] = code size
  # [esp + 0x0C] = callsite count
  # [esp + 0x10] = callsite address
  # [esp + 0x14] = callsite size
  # ... (further callsite address/size pairs)

  # Allocate memory for the copied code
  mov       ecx, [<VERS 0x00AA8F84 0x00AAB404>]
  push      dword [esp + 0x08]
  mov       eax, <VERS 0x007A984C 0x007A8A38>
  call      eax  # malloc7
  test      eax, eax
  je        done

  # Copy the code to the newly-allocated memory
  # eax = dest pointer (from malloc7 call above)
  mov       edx, [esp + 0x04]  # edx = source pointer
  mov       ecx, [esp + 0x08]  # ecx = source size
  push      ebx
memcpy_again:
  dec       ecx
  mov       bl, [edx + ecx]  # Copy one byte from source to dest
  mov       [eax + ecx], bl
  test      ecx, ecx
  jne       memcpy_again
  pop       ebx

  # Write the call opcodes
  xchg      ebx, [esp + 0x0C]  # Save ebx; get callsite count
  mov       [esp - 0x08], esi
  mov       [esp - 0x0C], eax
  mov       esi, 0x10  # Stack offset of first callsite pair

next_callsite:
  mov       edx, [esp + esi]  # edx = jump callsite
  lea       ecx, [eax - 5]
  sub       ecx, edx  # ecx = (dest code addr) - (jump callsite) - 5
  mov       byte [edx], 0xE8
  mov       [edx + 1], ecx  # Write E8 (call) followed by delta

  # Write as many nops after the call opcode as necessary
  mov       ecx, 5
  mov       eax, [esp + esi + 4]
write_nop_again:
  cmp       ecx, eax
  jge       this_callsite_done
  mov       byte [edx + ecx], 0x90
  inc       ecx
  jmp       write_nop_again

this_callsite_done:
  mov       eax, [esp - 0x0C]
  add       esi, 8
  dec       ebx
  jnz       next_callsite

  mov       ecx, esi
  mov       ebx, [esp + 0x0C]
  mov       esi, [esp - 0x08]

done:
  mov       eax, [esp]
  add       esp, ecx
  jmp       eax



.versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU

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
