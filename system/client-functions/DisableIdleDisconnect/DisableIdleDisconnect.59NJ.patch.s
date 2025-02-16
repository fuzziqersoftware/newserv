.meta name="Disable idle DC"
.meta description="Disables the idle\ndisconnect timeout"

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksBB

  .data     0x007A1233
  .data     0x00000005
  mov       eax, 0

  .data     0x00000000
  .data     0x00000000
