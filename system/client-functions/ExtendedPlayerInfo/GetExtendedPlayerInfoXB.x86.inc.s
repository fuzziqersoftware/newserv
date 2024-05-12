  # esp = 0xd0031ce0
  push   ebx
  push   edi
  push   esi

  jmp    get_data_ptr
get_data_ptr_ret:
  pop    ebx

  push   0x28CC
  call   [ebx]  # malloc9(0x28CC)
  add    esp, 4
  test   eax, eax
  jz     malloc9_failed
  mov    edi, eax

  mov    dword [edi], 0x28CC0030  # header = 30 00 CC 28

  lea    eax, [edi + 0x0004]
  mov    edx, [ebx + 0x04]
  mov    edx, [edx]
  mov    ecx, 0x41C
  call   memcpy  # memcpy(data + 4, char_file_part1, sizeof(char_file_part1))

  lea    eax, [edi + 0x0420]
  mov    edx, [ebx + 0x08]
  mov    edx, [edx]
  mov    ecx, 0x24AC
  call   memcpy  # memcpy(data + 4 + sizeof(char_file_part1), char_file_part2, sizeof(char_file_part2))

  push   0x28CC             # remaining_bytes = 0x28CC
  push   edi                # orig_send_ptr
  mov    esi, [ebx + 0x0C]
  mov    esi, [esi]         # root_protocol

send_again:  # while (remaining_bytes != 0)
  call   [ebx + 0x14]  # root_protocol->wait_send_drain()
  test   eax, eax
  jnz    drain_failed

  mov    eax, [esi]  # eax = root_protocol->vtable
  mov    ecx, 0x550
  mov    edx, [esp + 4]
  cmp    edx, ecx
  cmovg  edx, ecx  # this_chunk_size = min<uint32_t>(remaining_bytes, 0x550)
  push   edx  # this_chunk_size (for after return)
  push   edx
  push   edi
  mov    ecx, esi
  call   [eax + 0x20]  # root_protocol->send(send_ptr, this_chunk_size)

  pop    edx
  add    edi, edx  # send_ptr += this_chunk_size
  sub    [esp + 4], edx  # remaining_bytes -= this_chunk_size
  cmp    dword [esp + 4], 0
  jne    send_again

drain_failed:
  # orig_send_ptr is still on the stack from before the above loop
  call   [ebx + 0x10]  # free9(orig_send_ptr)
  add    esp, 8  # orig_send_ptr, remaining_bytes

  mov    eax, 0

malloc9_failed:
  pop    esi
  pop    edi
  pop    ebx
  ret

memcpy:
  .include CopyData
  ret

get_data_ptr:
  call   get_data_ptr_ret
