.meta name="Disable idle DC"
.meta description="Disables the idle\ndisconnect timeout"

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksXB

  .data     0x002C2BEE
  .data     0x00000004
  .binary   31C9EB03

  .data     0x00000000
  .data     0x00000000
