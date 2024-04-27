start:
  mov    eax, 0x88888888  # eax = &player_compute_implied_stats (the value is overwritten by the including patch)
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
  call   eax  # player_compute_implied_stats

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
