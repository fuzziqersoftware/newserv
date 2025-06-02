# Original patch by Soly, in Blue Burst Patch Project
# https://github.com/Solybum/Blue-Burst-Patch-Project
# Xbox port by fuzziqersoftware

.versions 4OED 4OEU 4OJB 4OJD 4OJU 4OPD 4OPU

.meta name="No rare selling"
.meta description="Stops you from\naccidentally\nselling rares\nto shops"

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksXB

  # See comments in the 59NL version of this patch for details on how it works.

  .data     <VERS 0x0017DEA6 0x0017DED6 0x0017DD36 0x0017DEB6 0x0017DF66 0x0017DEC6 0x0017DE96>
  .data     0x00000004
  .data     0x00000000

  .data     <VERS 0x0017DE8C 0x0017DEBC 0x0017DD1C 0x0017DE9C 0x0017DF4C 0x0017DEAC 0x0017DE7C>
  .data     0x00000004
  .data     0x00000000

  .data     <VERS 0x0017E04E 0x0017E07E 0x0017DEDE 0x0017E05E 0x0017E10E 0x0017E06E 0x0017E03E>
  .data     0x00000005
  .binary   E98E0C0000

  .data     <VERS 0x0017ECE1 0x0017ED11 0x0017EB71 0x0017ECF1 0x0017EDA1 0x0017ED01 0x0017ECD1>
  .deltaof  tool_check_start, tool_check_end
tool_check_start:
  xor       edi, edi
  test      byte [eax + 0x10], 0x80
  cmovz     edi, [eax + 0x0C]
  .binary   E995F3FFFF
tool_check_end:

  .data     0x00000000
  .data     0x00000000
