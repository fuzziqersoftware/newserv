.meta hide_from_patches_menu
.meta name="CallProtectedHandler"
.meta description=""

.versions 59NJ 59NL

entry_ptr:
reloc0:
  .offsetof start
start:
  jmp    get_data_addr
resume:
  xchg   ebx, [esp]

  mov    edx, [ebx]
  mov    dword [edx], 1

  mov    edx, [ebx + 4]
  push   dword [ebx + 8]
  lea    ecx, [ebx + 0x0C]
  push   ecx
  call   edx  # RcvPsoData2(data, size)
  add    esp, 8

  mov    edx, [ebx]
  mov    dword [edx], 0

  pop    ebx
  ret

get_data_addr:
  call   resume

  .data  <VERS 0x00AAC870 0x00AAECF0>  # should_allow_protected_commands
  .data  <VERS 0x008015D0 0x00800860>  # RcvPsoData2[std](void* data @ [esp + 4], uint32_t size @ [esp + 8])
size:
  .data  0x00000000
data:
