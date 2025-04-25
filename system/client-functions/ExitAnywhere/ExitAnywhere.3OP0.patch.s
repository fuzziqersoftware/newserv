# This function implements $exit in a game when no quest is loaded.

.meta name="Exit anywhere"
.meta description=""
.meta hide_from_patches_menu

entry_ptr:
reloc0:
  .offsetof start

start:
  li     r0, 0
  stw    [r13 - 0x46E8], r0
  stw    [r13 - 0x46E4], r0
  li     r0, 1
  sth    [r13 - 0x48D8], r0
  blr
