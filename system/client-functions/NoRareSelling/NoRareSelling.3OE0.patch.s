# Original patch by Soly, in Blue Burst Patch Project
# https://github.com/Solybum/Blue-Burst-Patch-Project
# GC port by fuzziqersoftware

.meta name="No rare selling"
.meta description="Stops you from\naccidentally\nselling rares\nto vendors"

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC

  # See comments in the 59NL version of this patch for details on how it works.

  .data     0x8010E114  # Rare weapons
  .data     0x00000004
  li        r29, 0

  .data     0x8010E100  # Unidentified weapons
  .data     0x00000004
  li        r29, 0

  .data     0x8010E248  # Rare armors
  .data     0x00000004
  li        r29, 0

  .data     0x800041A0
  .deltaof  tool_check_start, tool_check_end
  .address  0x800041A0
tool_check_start:
  lwz       r29, [r3 + 0x10]  # Flags
  xori      r29, r29, 0x0080
  andi.     r29, r29, 0x0080
  beq       is_rare_tool
  lwz       r29, [r3 + 0x0C]  # Cost
is_rare_tool:
  blr
tool_check_end:

  .data     0x8010E3BC
  .data     0x00000004
  .address  0x8010E3BC
  bl        tool_check_start

  .data     0x00000000
  .data     0x00000000
