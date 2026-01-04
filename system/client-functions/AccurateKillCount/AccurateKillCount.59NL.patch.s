.meta name="Kill count fix"
.meta description="Fixes client-side\nkill counts when\nmultiple enemies are\nkilled on the same\nframe"

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksBB



  .data     0x005E32C8
  .deltaof  TItemUnitUnsealable_count_kill, TItemUnitUnsealable_count_kill_end
  .address  0x005E32C8
TItemUnitUnsealable_count_kill:  # [std] (TItemUnitUnsealable* this @ ecx) -> void
  mov       eax, [ecx + 0xF8]
  movsx     eax, word [eax + 0x11A]  # eax = this->owner_player->num_kills_since_map_load
  movsx     edx, word [ecx + 0x1E4]  # edx = this->last_owner_player_kill_count
  sub       edx, eax  # edx = this->last_owner_player_kill_count - this->owner_player->num_kills_since_map_load (edx should be 0 or negative)
  jge       TItemUnitUnsealable_count_kill_skip_update
  test      dword [ecx + 0xDC], 0x100
  jz        TItemUnitUnsealable_count_kill_skip_incr  # if (!(this->flags & 0x100)) don't incr kill count
  sub       [ecx + 0xE8], dx  # this->kill_count -= edx
TItemUnitUnsealable_count_kill_skip_incr:
  mov       [ecx + 0x1E4], ax  # this->last_owner_player_kill_count = this->owner_player->num_kills_since_map_load
TItemUnitUnsealable_count_kill_skip_update:
  xor       edx, edx
  cmp       word [ecx + 0xE8], 20000
  setae     dh
  shl       edx, 1
  or        dword [ecx + 0xDC], edx
  jmp       0x005E2C34
TItemUnitUnsealable_count_kill_end:



  .data     0x005F3EFC
  .deltaof  TItemWeapon_LameDArgent_count_kill, TItemWeapon_LameDArgent_count_kill_end
  .address  0x005F3EFC
TItemWeapon_LameDArgent_count_kill:
  mov       eax, [ecx + 0xF8]
  movsx     eax, word [eax + 0x11A]
  movsx     edx, word [ecx + 0x240]
  sub       edx, eax
  jge       TItemWeapon_LameDArgent_count_kill_skip_update
  test      dword [ecx + 0xDC], 0x100
  jz        TItemWeapon_LameDArgent_count_kill_skip_incr
  sub       [ecx + 0xE8], dx
TItemWeapon_LameDArgent_count_kill_skip_incr:
  mov       [ecx + 0x240], ax
TItemWeapon_LameDArgent_count_kill_skip_update:
  xor       edx, edx
  cmp       word [ecx + 0xE8], 10000
  setae     dh
  shl       edx, 1
  or        dword [ecx + 0xDC], edx
  ret
TItemWeapon_LameDArgent_count_kill_end:



  .data     0x005FCA74
  .deltaof  TItemWeapon_SealedJSword_count_kill, TItemWeapon_SealedJSword_count_kill_end
  .address  0x005FCA74
TItemWeapon_SealedJSword_count_kill:
  mov       eax, [ecx + 0xF8]
  movsx     eax, word [eax + 0x11A]
  movsx     edx, word [ecx + 0x240]
  sub       edx, eax
  jge       TItemWeapon_SealedJSword_count_kill_skip_update
  test      dword [ecx + 0xDC], 0x100
  jz        TItemWeapon_SealedJSword_count_kill_skip_incr
  sub       [ecx + 0xE8], dx
TItemWeapon_SealedJSword_count_kill_skip_incr:
  mov       [ecx + 0x240], ax
TItemWeapon_SealedJSword_count_kill_skip_update:
  xor       edx, edx
  cmp       word [ecx + 0xE8], 23000
  setae     dh
  shl       edx, 1
  or        dword [ecx + 0xDC], edx
  ret
TItemWeapon_SealedJSword_count_kill_end:



  .data     0x00000000
  .data     0x00000000
