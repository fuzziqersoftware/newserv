.meta name="CallProtectedHandler"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start



.versions 3OJT 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

start:
  stwu     [r1 - 0x10], r1
  mflr     r0
  stw      [r1 + 0x14], r0
  stw      [r1 + 0x08], r31
  stw      [r1 + 0x0C], r30

  b        get_data_addr
resume:
  mflr     r31

  lwz      r30, [r31]
  li       r0, 1
  stw      [r30], r0

  addi     r3, r31, 0x0C
  lwz      r4, [r31 + 8]
  lwz      r0, [r31 + 4]
  mtctr    r0
  bctrl

  li       r0, 0
  stw      [r30], r0

  lwz      r30, [r1 + 0x0C]
  lwz      r31, [r1 + 0x08]
  lwz      r0, [r1 + 0x14]
  mtlr     r0
  addi     r1, r1, 0x10
  blr

get_data_addr:
  bl       resume
  # allow_local_client_commands
  .data     <VERS 0x8065F458 0x805C4D58 0x805CF320 0x805D67A0 0x805D6540 0x805C5650 0x805CC630 0x805D5E50 0x805D2090>
  # RcvPsoData2
  .data     <VERS 0x80236F24 0x801E3B38 0x801E40BC 0x801E4290 0x801E4008 0x801E3F9C 0x801E3F9C 0x801E405C 0x801E4698>



.versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU

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



.versions 59NJ 59NL

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



.all_versions

size:
  .data  0x00000000
data:
