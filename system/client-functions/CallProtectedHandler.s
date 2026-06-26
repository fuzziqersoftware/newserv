.meta name="CallProtectedHandler"
.meta description=""

entry_ptr:
reloc0:
  .data    start



.versions 3OJT 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

  .label   allow_local_client_commands, <VERS 0x8065F458 0x805C4D58 0x805CF320 0x805D67A0 0x805D6540 0x805C5650 0x805CC630 0x805D5E50 0x805D2090>
  .label   RcvPsoData2, <VERS 0x80236F24 0x801E3B38 0x801E40BC 0x801E4290 0x801E4008 0x801E3F9C 0x801E3F9C 0x801E405C 0x801E4698>

start:
  stwu     [r1 - 0x10], r1
  mflr     r0
  stw      [r1 + 0x14], r0

  b        get_data_addr
resume:
  mflr     r4

  li       r0, 1
  lis      r3, addr_high(allow_local_client_commands)
  stw      [r3 + addr_low(allow_local_client_commands)], r0

  addi     r3, r4, 4
  lwz      r4, [r4]
  lis      r0, high_word(RcvPsoData2)
  ori      r0, r0, low_word(RcvPsoData2)
  mtctr    r0
  bctrl

  li       r0, 0
  lis      r3, addr_high(allow_local_client_commands)
  stw      [r3 + addr_low(allow_local_client_commands)], r0

  lwz      r0, [r1 + 0x14]
  mtlr     r0
  addi     r1, r1, 0x10
  blr

get_data_addr:
  bl       resume
size:
  .data     0x00000000
data:



.versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU

  .label    allow_local_client_commands, <VERS 0x0071E8C8 0x0071EF28 0x00726A68 0x00723F68 0x007237E8 0x00723F68 0x007242E8>
  .label    RcvPsoData2, <VERS 0x002DBBA0 0x002DC720 0x002DDFE0 0x002DDB00 0x002DE000 0x002DDB30 0x002DE030>

start:
  jmp       get_data_addr
resume:
  pop       eax
  mov       dword [allow_local_client_commands], 1
  mov       edx, RcvPsoData2
  lea       ecx, [eax + 4]
  mov       eax, [ebx]
  call      edx
  mov       dword [allow_local_client_commands], 0
  ret

get_data_addr:
  call      resume
size:
  .data     0x00000000
data:



.versions 50YJ 59NJ 59NL

  .label    allow_local_client_commands, <VERS 0x00AA2830 0x00AAC870 0x00AAECF0>
  .label    RcvPsoData2, <VERS 0x007F95E0 0x008015D0 0x00800860>  # [std](void* data @ [esp + 4], uint32_t size @ [esp + 8])

start:
  jmp       get_data_addr
resume:
  pop       eax
  mov       dword [allow_local_client_commands], 1
  push      dword [eax]
  lea       ecx, [eax + 4]
  push      ecx
  mov       edx, RcvPsoData2
  call      edx  # RcvPsoData2(data, size)
  add       esp, 8
  mov       dword [allow_local_client_commands], 0
  ret

get_data_addr:
  call      resume
size:
  .data     0x00000000
data:
