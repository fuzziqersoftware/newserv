.meta name="Kill count fix"
.meta description="Fixes client-side\nkill counts when\nmultiple enemies are\nkilled on the same\nframe"

.versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC

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

  .data     0x00000000
  .data     0x00000000
