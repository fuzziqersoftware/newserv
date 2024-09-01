.meta name="Disable idle DC"
.meta description="Disables the idle\ndisconnect timeout"

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksBB

  .data     0x007A03F7
  .data     0x00000005
  .binary   B800000000

  .data     0x00000000
  .data     0x00000000
