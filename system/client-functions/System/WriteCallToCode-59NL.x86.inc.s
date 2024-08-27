# This file defines the following function:
#   write_call_to_code(
#     const void* patch_code,
#     size_t patch_code_size,
#     void* call_opcode_address,
#     size_t call_opcode_bytes);
# This function allocates memory for patch_code, copies patch_code to that
# memory, then writes a call opcode to call_opcode_address that calls the code
# in the allocated memory region. The allocated memory is never freed.
# call_opcode_bytes specifies how many bytes at the callsite should be
# overwritten; this value must be at least 5. The first 5 bytes are overwritten
# with the call opcode itself; the rest are overwritten with nop opcodes.
# This function pops its arguments off the stack before returning.

write_call_to_code:
  # [esp + 0x04] = code ptr
  # [esp + 0x08] = code size
  # [esp + 0x0C] = jump callsite
  # [esp + 0x10] = callsite size

  # Check if the opcode is already a call; if so, do nothing
  mov       edx, [esp + 0x0C]
  cmp       byte [edx], 0xE8
  je        done

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

  # Write the call opcode
  mov       edx, [esp + 0x0C]  # edx = jump callsite
  lea       ecx, [eax - 5]
  sub       ecx, edx  # ecx = (dest code addr) - (jump callsite) - 5
  mov       byte [edx], 0xE8
  mov       [edx + 1], ecx  # Write E8 (call) followed by delta

  # Write as many nops after the call opcode as necessary
  mov       ecx, 5
  mov       eax, [esp + 0x10]
write_nop_again:
  cmp       ecx, eax
  jge       done
  mov       byte [edx + ecx], 0x90
  inc       ecx
  jmp       write_nop_again

done:
  ret       0x10
