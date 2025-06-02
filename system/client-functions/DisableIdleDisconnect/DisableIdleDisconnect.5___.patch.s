.meta name="Disable idle DC"
.meta description="Disables the idle\ndisconnect timeout"

.versions 59NJ 59NL

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksBB

  .data     <VERS 0x007A1233 0x007A03F7>
  .data     0x00000005
  mov       eax, 0

  .data     0x00000000
  .data     0x00000000
