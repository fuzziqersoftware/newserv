  push ebx

  jmp    get_data_ptr
get_data_ptr_ret:
  pop    ebx

  # Copy part1 data into place
  mov    eax, [ebx]
  mov    eax, [eax]
  lea    edx, [ebx + 8]
  mov    ecx, 0x041C
  call   memcpy

  # Copy part2 data into place, but retain the values of a few metadata fields
  # so the game won't think the file is corrupt
  mov    eax, [ebx + 4]
  mov    eax, [eax]
  push   dword [eax + 0x04]  # creation_timestamp
  push   dword [eax + 0x08]  # signature
  push   dword [eax + 0x14]  # save_count
  lea    edx, [ebx + 0x0424]
  mov    ecx, 0x23B8  # Intentionally skip the last 0xF0 bytes since they aren't populated by the server
  call   memcpy
  mov    eax, [ebx + 4]
  mov    eax, [eax]
  pop    dword [eax + 0x14]  # save_count
  pop    dword [eax + 0x08]  # signature
  pop    dword [eax + 0x04]  # creation_timestamp

  mov    eax, 1
  pop    ebx
  ret

memcpy:
  .include CopyData
  ret

get_data_ptr:
  call   get_data_ptr_ret
