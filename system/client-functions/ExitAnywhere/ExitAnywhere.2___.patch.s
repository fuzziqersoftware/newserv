# This function implements $exit in a game when no quest is loaded.

.meta name="Exit anywhere"
.meta description=""
.meta hide_from_patches_menu

.versions 2OJ5 2OJF 2OEF 2OPF

entry_ptr:
reloc0:
  .offsetof start

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
