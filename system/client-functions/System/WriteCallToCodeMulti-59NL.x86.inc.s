# This file defines the following function:
#   write_call_to_code(
#     const void* patch_code,
#     size_t patch_code_size,
#     size_t call_count,
#     void* call_opcode_address,
#     ssize_t call_opcode_bytes,
#     ...);
# This function allocates memory for patch_code, copies patch_code to that
# memory, then writes a call or jmp opcode to call_opcode_address that calls
# the code in the allocated memory region. The allocated memory is never freed.
# call_opcode_bytes specifies how many bytes at the callsite should be
# overwritten. This value must be at least 5; the first 5 bytes are overwritten
# with the call/jmp opcode itself; the rest are overwritten with nop opcodes.
# This function pops its arguments off the stack before returning (including
# all the varargs).

write_call_to_code:
  # [esp + 0x04] = code ptr
  # [esp + 0x08] = code size
  # [esp + 0x0C] = callsite count
  # [esp + 0x10] = callsite address
  # [esp + 0x14] = callsite size
  # ... (further callsite address/size pairs)

  # Allocate memory for the copied code
  mov       ecx, [0x00AAB404]
  push      dword [esp + 0x08]
  mov       eax, 0x007A8A38
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
