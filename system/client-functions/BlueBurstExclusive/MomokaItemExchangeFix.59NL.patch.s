.meta name="Item exch. fix"
.meta description="Fixes Momoka item exchange\nopcode"

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksBB



  # Fix 6xDE failure label truncation
  .data     0x006B90DE
  .data     1
  .binary   03



  # Fix send_6xD9 not setting size field

  .data     0x006CA540
  .deltaof  send_6xD9_start, send_6xD9_end
  .address  0x006CA540
send_6xD9_start:  # [std](void* this @ ecx) -> void
  push      ebx
  mov       ebx, ecx
  push      0  # cmd.success_label, cmd.failure_label
  mov       eax, [0x00A9C4F4]  # local_client_id
  xor       eax, 1
  push      eax  # cmd.token2
  mov       ecx, [ebx + 0x2C]
  call      0x00737D90  # [std](void* this @ ecx = *(this + 0x2C)) -> void* @ eax
  mov       edx, [ebx + 0x3C]
  imul      eax, eax, 0x14
  add       edx, eax
  mov       eax, [edx + 0x10]
  xor       eax, [0x00A9C4F4]  # local_client_id
  push      eax  # cmd.token1
  push      dword [edx + 0x10]  # cmd.replace_item.data2d
  push      dword [edx + 0x0C]  # cmd.replace_item.id
  push      dword [edx + 0x08]  # cmd.replace_item.data1[8-11]
  push      dword [edx + 0x04]  # cmd.replace_item.data1[4-7]
  push      dword [edx]  # cmd.replace_item.data1[0-3]
  push      dword [ebx + 0x50]  # cmd.find_item.data2d
  push      dword [ebx + 0x4C]  # cmd.find_item.id
  push      dword [ebx + 0x48]  # cmd.find_item.data1[8-11]
  push      dword [ebx + 0x44]  # cmd.find_item.data1[4-7]
  push      dword [ebx + 0x40]  # cmd.find_item.data1[0-3]
  push      0x00000ED9  # cmd.header

  mov       ecx, esp
  call      0x008003E0  # send_and_handle_60[std](void* cmd @ ecx) -> void
  add       esp, 0x38

  mov       dword [ebx], 6
  push      0
  call      0x00859D2D  # time[std](void* t @ [esp + 4] = nullptr) -> uint32_t @ eax
  add       esp, 4
  mov       [ebx + 0x5C], eax

  pop       ebx
  ret
send_6xD9_end:



  # Same fix as above, but for quest_F95B_send_6xD9

  .data     0x006B9018
  .deltaof  quest_F95B_send_6xD9_start, quest_F95B_send_6xD9_end
  .address  0x006B9018
quest_F95B_send_6xD9_start:  # [std]() -> void
  mov       edx, 0x00A954CC  # quest_args_list
  mov       ax, [edx + 0x14]  # quest_args_list[5] (failure_label)
  shl       eax, 0x10
  mov       ax, [edx + 0x10]  # quest_args_list[4] (success_label)
  push      eax  # cmd.success_label, cmd.failure_label
  mov       ecx, [0x00A9C4F4]  # local_client_id
  mov       eax, [edx + 0x0C]  # quest_args_list[3] (token2)
  xor       eax, ecx
  push      eax  # cmd.token2
  mov       eax, [edx + 0x08]  # quest_args_list[2] (token1)
  xor       eax, ecx
  push      eax  # cmd.token1
  push      0x00000000  # cmd.replace_item.data2d
  push      0xFFFFFFFF  # cmd.replace_item.id
  push      0x00000000  # cmd.replace_item.data1[8-11]
  push      0x00000000  # cmd.replace_item.data1[4-7]
  mov       eax, [edx + 0x04]  # quest_args_list[1] (data1[0-2] in low 3 bytes)
  shl       eax, 8
  bswap     eax
  push      eax  # cmd.replace_item.data1[0-3]
  push      0x00000000  # cmd.find_item.data2d
  push      0xFFFFFFFF  # cmd.find_item.id
  push      0x00000000  # cmd.find_item.data1[8-11]
  push      0x00000000  # cmd.find_item.data1[4-7]
  mov       eax, [edx]  # quest_args_list[0] (data1[0-2] in low 3 bytes)
  shl       eax, 8
  bswap     eax
  push      eax  # cmd.find_item.data1[0-3]
  mov       eax, 0xD90E0000
  mov       ax, cx
  bswap     eax
  push      eax  # cmd.header

  mov       ecx, esp
  call      0x008003E0  # send_and_handle_60[std](void* cmd @ ecx) -> void
  add       esp, 0x38
  ret
quest_F95B_send_6xD9_end:



  .data     0x00000000
  .data     0x00000000
