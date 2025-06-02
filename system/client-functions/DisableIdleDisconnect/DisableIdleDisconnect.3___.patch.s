.meta name="Disable idle DC"
.meta description="Disables the idle\ndisconnect timeout"

.versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0 3SJT 3SJ0 3SE0 3SP0

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC

  .data     <VERS 0x80134D3C 0x80134FA0 0x80135108 0x80135040 0x80134FE0 0x80134FE0 0x80135050 0x801352D0 0x80092C78 0x8009242C 0x80092380 0x80092588>
  .data     0x00000004
  li        r3, 0

  .data     0x00000000
  .data     0x00000000
