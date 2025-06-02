# This file defines the following function:
#   write_address_of_code(
#     const void* patch_code,
#     size_t patch_code_size,
#     void** ptr_addr);
# This function allocates memory for patch_code, copies patch_code to that
# memory, then writes the address of the allocated code at the specified
# pointer. The allocated memory is never freed.
# This function pops its arguments off the stack before returning.

write_call_to_code:
  # [esp + 0x04] = code ptr
  # [esp + 0x08] = code size
  # [esp + 0x0C] = ptr addr

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

  # Write the address
  mov       ecx, [esp + 0x0C]
  mov       [ecx], eax

done:
  ret       0x0C
