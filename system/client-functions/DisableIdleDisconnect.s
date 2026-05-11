.meta visibility="all"
.meta name="Disable idle DC"
.meta description="Disables the idle\ndisconnect timeout"

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocks



  .versions 1OJ3 1OJ4 1OJF 1OEF 1OPF 2OJ4 2OJ5 2OJF 2OEF 2OPF
  .align 4
  .data     <VERS 0x8C01A454 0x8C01A6D0 0x8C01A414 0x8C01A6C8 0x8C01A6DC 0x8C01B6A4 0x8C01B6A4 0x8C01B684 0x8C01B6A4 0x8C01B6A8>
  .data     0x00000002
  mov       r0, 0
  .align 4



  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0 3SJT 3SJ0 3SE0 3SP0
  .data     <VERS 0x80134D3C 0x80134FA0 0x80135108 0x80135040 0x80134FE0 0x80134FE0 0x80135050 0x801352D0 0x80092C78 0x8009242C 0x80092380 0x80092588>
  .data     0x00000004
  li        r3, 0



  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x002C0AEE 0x002C167E 0x002C2BEE 0x002C272E 0x002C291E 0x002C275E 0x002C2A7E>
  .data     0x00000004
  xor       ecx, ecx
  jmp       +3



  .versions 59NJ 59NL
  .data     <VERS 0x007A1233 0x007A03F7>
  .data     0x00000005
  mov       eax, 0



  .all_versions

  .data     0x00000000
  .data     0x00000000
