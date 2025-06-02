.meta hide_from_patches_menu
.meta name="CallProtectedHandler"
.meta description=""

.versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU

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
  lea    ecx, [ebx + 0x0C]
  mov    eax, [ebx + 8]
  call   edx

  mov    edx, [ebx]
  mov    dword [edx], 0

  pop    ebx
  ret

get_data_addr:
  call   resume
  .data  <VERS 0x0071E8C8 0x0071EF28 0x00726A68 0x00723F68 0x007237E8 0x00723F68 0x007242E8>
  .data  <VERS 0x002DBBA0 0x002DC720 0x002DDFE0 0x002DDB00 0x002DE000 0x002DDB30 0x002DE030>

size:
  .data  0x00000000
data:
