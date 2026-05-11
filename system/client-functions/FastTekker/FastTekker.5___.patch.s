.meta name="Fast tekker"
.meta description="Skips wind-up sound\nat tekker window"

.versions 59NJ 59NL

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksBB

  .data     <VERS 0x006DA14B 0x006DA113>
  .deltaof  patch1_start, patch1_end
patch1_start:
  mov       dword [edi + 0x14C], 1
patch1_end:

  .data     <VERS 0x006DA168 0x006DA130>
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
  nop
patch2_end:

  .data     0x00000000
  .data     0x00000000
