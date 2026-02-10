.meta name="Disable idle DC"
.meta description="Disables the idle\ndisconnect timeout"

.versions 1OJ3 1OJ4 1OJF 1OEF 1OPF 2OJ4 2OJ5 2OJF 2OEF 2OPF

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksDC

  .align 4
  .data     <VERS 0x8C01A454 0x8C01A6D0 0x8C01A414 0x8C01A6C8 0x8C01A6DC 0x8C01B6A4 0x8C01B6A4 0x8C01B684 0x8C01B6A4 0x8C01B6A8>
  .data     0x00000002
  mov       r0, 0

  .align 4
  .data     0x00000000
  .data     0x00000000
