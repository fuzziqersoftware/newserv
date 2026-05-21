.meta visibility="all"
.meta name="Kill count fix"
.meta description="Fixes client-side\nkill counts when\nmultiple enemies are\nkilled on the same\nframe"

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocks



  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

  .label    TItemWeapon_SealedJSword_count_kill_loc, <VERS 0x8012D2D4 0x8012D518 0x8012D550 0x8012D4B0 0x8012D578 0x8012D578 0x8012D4C0 0x8012D698>
  .data     TItemWeapon_SealedJSword_count_kill_loc
  .deltaof  TItemWeapon_SealedJSword_count_kill, TItemWeapon_SealedJSword_count_kill_end
  .address  TItemWeapon_SealedJSword_count_kill_loc
TItemWeapon_SealedJSword_count_kill:  # [std](TItemWeapon_SealedJSword* this @ r3) -> void
  lwz       r4, [r3 + 0xF0]  # r4 = this->owner_player
  lha       r5, [r4 + 0x11A]  # r5 = this->owner_player->num_kills_since_map_load
  lha       r6, [r3 + 0x1F8]  # r6 = this->last_owner_player_kill_count
  lhz       r7, [r3 + 0xE8]  # r7 = this->kill_count
  cmp       r6, r5
  bge       TItemWeapon_SealedJSword_count_kill_skip_update
  lwz       r8, [r3 + 0xDC]
  andi.     r8, r8, 0x100
  beq       TItemWeapon_SealedJSword_count_kill_skip_incr  # if (!(flags & 0x100)) don't incr kill count
  sub       r8, r5, r6
  add       r7, r7, r8
  sth       [r3 + 0xE8], r7
TItemWeapon_SealedJSword_count_kill_skip_incr:
  sth       [r3 + 0x1F8], r5
TItemWeapon_SealedJSword_count_kill_skip_update:
  cmplwi    r7, 23000
  blt       TItemWeapon_SealedJSword_count_kill_skip_set_flag
  lwz       r8, [r3 + 0xDC]
  ori       r8, r8, 0x200
  stw       [r3 + 0xDC], r8
TItemWeapon_SealedJSword_count_kill_skip_set_flag:
  blr
TItemWeapon_SealedJSword_count_kill_end:



  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU

  .data     <VERS 0x00197610 0x001977A0 0x00197920 0x00197880 0x00197810 0x001978A0 0x00197840>
  .deltaof  TItemWeapon_SealedJSword_count_kill, TItemWeapon_SealedJSword_count_kill_end
  .address  <VERS 0x00197610 0x001977A0 0x00197920 0x00197880 0x00197810 0x001978A0 0x00197840>
TItemWeapon_SealedJSword_count_kill:
  mov       eax, [ecx + 0xF0]
  movsx     eax, word [eax + 0x11A]
  movsx     edx, word [ecx + 0x1F8]
  sub       edx, eax
  jge       TItemWeapon_SealedJSword_count_kill_skip_update
  test      dword [ecx + 0xDC], 0x100
  jz        TItemWeapon_SealedJSword_count_kill_skip_incr
  sub       [ecx + 0xE8], dx
TItemWeapon_SealedJSword_count_kill_skip_incr:
  mov       [ecx + 0x1F8], ax
TItemWeapon_SealedJSword_count_kill_skip_update:
  cmp       word [ecx + 0xE8], 23000
  jb        TItemWeapon_SealedJSword_count_kill_skip_set_flag
  or        dword [ecx + 0xDC], 0x200
TItemWeapon_SealedJSword_count_kill_skip_set_flag:
  ret
TItemWeapon_SealedJSword_count_kill_end:



  .versions 59NJ 59NL

  .data     <VERS 0x005E32A4 0x005E32C8>
  .deltaof  TItemUnitUnsealable_count_kill, TItemUnitUnsealable_count_kill_end
  .address  <VERS 0x005E32A4 0x005E32C8>
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
  jmp       <VERS 0x005E2C10 0x005E2C34>
TItemUnitUnsealable_count_kill_end:

  .data     <VERS 0x005F3E94 0x005F3EFC>
  .deltaof  TItemWeapon_LameDArgent_count_kill, TItemWeapon_LameDArgent_count_kill_end
  .address  <VERS 0x005F3E94 0x005F3EFC>
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

  .data     <VERS 0x005FC95C 0x005FCA74>
  .deltaof  TItemWeapon_SealedJSword_count_kill, TItemWeapon_SealedJSword_count_kill_end
  .address  <VERS 0x005FC95C 0x005FCA74>
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



  .all_versions

  .data     0x00000000
  .data     0x00000000
