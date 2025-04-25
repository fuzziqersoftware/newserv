# This function implements $exit in a game when no quest is loaded.

.meta name="Exit anywhere"
.meta description=""
.meta hide_from_patches_menu

entry_ptr:
reloc0:
  .offsetof start

start:
  xor    eax, eax
  mov    [0x00A95624], eax  # is_in_quest = false
  mov    [0x00A955E0], eax  # dat_source_type = NONE
  inc    eax
  mov    [0x00AAE6D4], ax  # should_leave_game = true
  ret
