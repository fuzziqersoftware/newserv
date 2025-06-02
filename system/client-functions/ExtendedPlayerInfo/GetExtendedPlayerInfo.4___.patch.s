.meta hide_from_patches_menu
.meta name="GetExtendedPlayerInfo"
.meta description=""

.versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU

entry_ptr:
reloc0:
  .offsetof start
start:
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

data:
  .data  <VERS 0x002FC5C0 0x002FD110 0x002FE700 0x002FE5A0 0x002FE700 0x002FE5D0 0x002FE770>  # malloc9(uint32_t size @ stack)
  .data  <VERS 0x0062D844 0x0062DDE4 0x0063591C 0x00632E04 0x0063269C 0x00632E04 0x0063319C>  # char_file_part1
  .data  <VERS 0x0062D8E8 0x0062DE88 0x006359C0 0x00632EA8 0x00632740 0x00632EA8 0x00633240>  # char_file_part2
  .data  <VERS 0x0071EEFC 0x0071F55C 0x007270A0 0x0072459C 0x00723E20 0x0072459C 0x00724920>  # root_protocol
  .data  <VERS 0x002FC670 0x002FD1C0 0x002FE7B0 0x002FE650 0x002FE7B0 0x002FE680 0x002FE820>  # free9(void* ptr @ stack)
  .data  <VERS 0x002ABE30 0x002AC910 0x002ADDE0 0x002AD870 0x002ADA50 0x002AD890 0x002ADB10>  # TProtocol::wait_send_drain(TProtocol* this @ esi)
