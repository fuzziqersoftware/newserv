# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049
# Xbox port by fuzziqersoftware

.meta visibility="all"
.meta name="Decoction"
.meta description="Makes the Decoction\nitem reset your\nmaterial usage"


entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocks



  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

  .data     <VERS 0x80350740 0x80351B44 0x803530A0 0x80352E54 0x803515F4 0x80351638 0x80353220 0x80352614>
  .data     0x00000098
  .address  <VERS 0x80350740 0x80351B44 0x803530A0 0x80352E54 0x803515F4 0x80351638 0x80353220 0x80352614>
  lbz       r0, [r3 + 0xEE]
  cmplwi    r0, 11
  bne       +0x144
  lwz       r31, [r3 + 0xF0]
  li        r0, 0
  nop
  li        r4, 0x0374
  li        r5, 0x0D38
  bl        +0x58
  li        r5, 0x0D3A
  bl        +0x50
  li        r5, 0x0D3C
  bl        +0x48
  li        r5, 0x0D40
  bl        +0x40
  li        r5, 0x0D44
  bl        +0x38
  mr        r3, r31
  .data     <VERS 0x4BE656A1 0x4BE646F1 0x4BE654CD 0x4BE634AD 0x4BE64BD9 0x4BE64B95 0x4BE63145 0x4BE6420D>
  lhz       r0, [r31 + 0x032C]
  lhz       r3, [r31 + 0x02B8]
  cmpl      r0, r3
  ble       +0x08
  sth       [r31 + 0x032C], r3
  lhz       r0, [r31 + 0x032E]
  lhz       r3, [r31 + 0x02BA]
  cmpl      r0, r3
  ble       +0x08
  sth       [r31 + 0x032E], r3
  b         +0xD8
  lbzx      r6, [r31 + r4]
  lhzx      r7, [r31 + r5]
  rlwinm    r6, r6, 1, 0, 30
  subf      r7, r6, r7
  sthx      [r31 + r5], r7
  stbx      [r31 + r4], r0
  addi      r4, r4, 0x0001
  blr



  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU

  .data     <VERS 0x00184160 0x00184350 0x00184400 0x00184340 0x00184310 0x00184360 0x001842D0>
  .deltaof  code_start, code_end
  .address  <VERS 0x00184160 0x00184350 0x00184400 0x00184340 0x00184310 0x00184360 0x001842D0>
code_start:

  .label player_compute_implied_stats, <VERS 0x001FD3D0 0x001FD530 0x001FD7B0 0x001FD5E0 0x001FD5E0 0x001FD600 0x001FD670>

  lea    edx, [ecx + 0x78]
  cmp    byte [edx + 0x76], 11  # this->data1[2] (item+0xEE) ?= 0x0B
  jne    skip_all

  push   esi
  push   ebx

  mov    ebx, [edx + 0x78]  # ebx = item->owner_player (item+0xF0)
  lea    esi, [ebx + 0x037C]  # esi = &ebx->material_usage
  lea    edx, [ebx + 0x0D48]  # edx = &ebx->stats.char_stats.atp
  push   0
  push   4
  push   4
  push   2
  push   2
next_stat:
  xor    ecx, ecx
  xchg   cl, byte [esi]  # ecx = material count; material count = 0
  shl    ecx, 1  # ecx = material count * 2
  sub    [edx], cx  # stat -= cx
  inc    esi
  pop    ecx
  add    edx, ecx
  test   ecx, ecx
  jne    next_stat

  mov    ecx, ebx
  call   player_compute_implied_stats

  lea    ecx, [ebx + 0x0330]

  mov    eax, [ecx - 0x74]  # ax = max_hp (player+0x2BC) and eax high = max_tp (player+0x2BE)
  mov    edx, [ecx]  # dx = current_hp (player+0x330) and edx high = current_tp (player+0x332)
  cmp    dx, ax  # current_hp vs. max_hp
  cmovg  dx, ax  # if current_hp above max_hp, current_hp = max_hp
  mov    ax, dx  # max_hp = current_hp (space optimization so we can use 32-bit opcodes below)
  cmp    edx, eax  # current_tp vs. max_tp
  cmovg  edx, eax  # if current_tp above max_tp, current_tp = max_tp (low 16 bits of both regs are always equal at this point)
  mov    [ecx], edx  # write current_hp and current_tp

  pop    ebx
  pop    esi
skip_all:
  ret
code_end:



  .all_versions

  .data     0x00000000
  .data     0x00000000
