.meta hide_from_patches_menu
.meta name="SetExtendedPlayerInfo"
.meta description=""

.versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU

entry_ptr:
reloc0:
  .offsetof start
start:
  push   ebx

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
data:
  .data  <VERS 0x0062D844 0x0062DDE4 0x0063591C 0x00632E04 0x0063269C 0x00632E04 0x0063319C>  # char_file_part1
  .data  <VERS 0x0062D8E8 0x0062DE88 0x006359C0 0x00632EA8 0x00632740 0x00632EA8 0x00633240>  # char_file_part2
  # Server adds a PSOXBCharacterFile::Character here
