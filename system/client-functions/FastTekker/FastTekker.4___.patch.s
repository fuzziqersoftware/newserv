.meta name="Fast tekker"
.meta description="Skips wind-up sound\nat tekker window"

.versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksXB

  .data     <VERS 0x0023EC5C 0x0023EEAC 0x0023F21C 0x0023EF3C 0x0023F0BC 0x0023EF5C 0x0023F14C>
  .deltaof  patch1_start, patch1_end
patch1_start:
  mov       dword [ebp + 0x14C], 1
patch1_end:

  .data     <VERS 0x0023EC77 0x0023EEC7 0x0023F237 0x0023EF57 0x0023F0D7 0x0023EF77 0x0023F167>
  .deltaof  patch2_start, patch2_end
patch2_start:
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
patch2_end:

  .data     0x00000000
  .data     0x00000000
