# This function implements $exit in a game when no quest is loaded.

.meta name="Exit anywhere"
.meta description=""
.meta hide_from_patches_menu

.versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

entry_ptr:
reloc0:
  .offsetof start

start:
  li     r0, 0
  stw    [r13 - <VERS 0x4760 0x4758 0x4738 0x4738 0x4748 0x4748 0x4728 0x46E8>], r0
  stw    [r13 - <VERS 0x475C 0x4754 0x4734 0x4734 0x4744 0x4744 0x4724 0x46E4>], r0
  li     r0, 1
  sth    [r13 - <VERS 0x4950 0x4948 0x4928 0x4928 0x4938 0x4938 0x4918 0x48D8>], r0
  blr
