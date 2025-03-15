# Original patch by Soly, in Blue Burst Patch Project
# https://github.com/Solybum/Blue-Burst-Patch-Project
# Xbox port by fuzziqersoftware

.meta name="No rare selling"
.meta description="Stops you from\naccidentally\nselling rares\nto vendors"

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksXB

  # See comments in the 59NL version of this patch for details on how it works.

  .data     0x0017DEB6  # Rare weapons and armors
  .data     0x00000004
  .data     0x00000000

  .data     0x0017DE9C  # Unidentified weapons
  .data     0x00000004
  .data     0x00000000

  .data     0x0017E05E
  .data     0x00000005
  .binary   E98E0C0000  # jmp tool_check_start

  .data     0x0017ECF1
  .deltaof  tool_check_start, tool_check_end
tool_check_start:
  xor       edi, edi
  test      byte [eax + 0x10], 0x80
  cmovz     edi, [eax + 0x0C]
  .binary   E995F3FFFF  # jmp tool_check_ret
tool_check_end:

  .data     0x00000000
  .data     0x00000000
