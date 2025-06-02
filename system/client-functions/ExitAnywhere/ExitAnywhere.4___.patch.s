# This function implements $exit in a game when no quest is loaded.

.meta name="Exit anywhere"
.meta description=""
.meta hide_from_patches_menu

.versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU

entry_ptr:
reloc0:
  .offsetof start

start:
  xor    eax, eax
  mov    [<VERS 0x0062D374 0x0062D914 0x0063544C 0x00632934 0x006321CC 0x00632934 0x00632CCC>], eax  # is_in_quest = false
  mov    [<VERS 0x0062D370 0x0062D910 0x00635448 0x00632930 0x006321C8 0x00632930 0x00632CC8>], eax  # dat_source_type = NONE
  inc    eax
  mov    [<VERS 0x0071E8E8 0x0071EF48 0x00726A88 0x00723F88 0x00723808 0x00723F88 0x00724308>], ax  # should_leave_game = true
  ret
