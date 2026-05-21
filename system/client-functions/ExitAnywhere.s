# This function implements $exit in a game when no quest is loaded.

.meta name="Exit anywhere"
.meta description=""


entry_ptr:
reloc0:
  .offsetof start



.versions 2OJ5 2OJF 2OEF 2OPF
start:
  mov     r2, 0
  mova    r0, [addrs]
  mov.l   r1, [r0]
  mov.l   [r1], r2
  mov.l   r1, [r0 + 4]
  mov.l   [r1], r2
  mov     r2, 1
  mov.l   r1, [r0 + 8]
  rets
  mov.w   [r1], r2
  .align  4
addrs:
  .data   <VERS 0x8C4ED300 0x8C4E6DA0 0x8C4ED300 0x8C4DC800>
  .data   <VERS 0x8C4ED344 0x8C4E6DE4 0x8C4ED344 0x8C4DC844>
  .data   <VERS 0x8C4E8D88 0x8C4E2828 0x8C4E8D88 0x8C4D8288>



.versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
start:
  li     r0, 0
  stw    [r13 - <VERS 0x4760 0x4758 0x4738 0x4738 0x4748 0x4748 0x4728 0x46E8>], r0
  stw    [r13 - <VERS 0x475C 0x4754 0x4734 0x4734 0x4744 0x4744 0x4724 0x46E4>], r0
  li     r0, 1
  sth    [r13 - <VERS 0x4950 0x4948 0x4928 0x4928 0x4938 0x4938 0x4918 0x48D8>], r0
  blr



.versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
start:
  xor    eax, eax
  mov    [<VERS 0x0062D374 0x0062D914 0x0063544C 0x00632934 0x006321CC 0x00632934 0x00632CCC>], eax  # is_in_quest = false
  mov    [<VERS 0x0062D370 0x0062D910 0x00635448 0x00632930 0x006321C8 0x00632930 0x00632CC8>], eax  # dat_source_type = NONE
  inc    eax
  mov    [<VERS 0x0071E8E8 0x0071EF48 0x00726A88 0x00723F88 0x00723808 0x00723F88 0x00724308>], ax  # should_leave_game = true
  ret



.versions 59NJ 59NL
start:
  xor    eax, eax
  mov    [<VERS 0x00A931A4 0x00A95624>], eax  # is_in_quest = false
  mov    [<VERS 0x00A93160 0x00A955E0>], eax  # dat_source_type = NONE
  inc    eax
  mov    [<VERS 0x00AAC254 0x00AAE6D4>], ax  # should_leave_game = true
  ret
