.meta name="Disable idle DC"
.meta description="Disables the idle\ndisconnect timeout"

.versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksXB

  .data     <VERS 0x002C0AEE 0x002C167E 0x002C2BEE 0x002C272E 0x002C291E 0x002C275E 0x002C2A7E>
  .data     0x00000004
  xor       ecx, ecx
  jmp       +3

  .data     0x00000000
  .data     0x00000000
