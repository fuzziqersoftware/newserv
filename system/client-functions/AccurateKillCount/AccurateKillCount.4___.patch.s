.meta name="Kill count fix"
.meta description="Fixes client-side\nkill counts when\nmultiple enemies are\nkilled on the same\nframe"
.meta hide_from_patches_menu

.versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksXB

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

  .data     0x00000000
  .data     0x00000000
