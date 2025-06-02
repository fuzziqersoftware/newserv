# Original patch by Soly, in Blue Burst Patch Project
# https://github.com/Solybum/Blue-Burst-Patch-Project
# GC port by fuzziqersoftware

.versions 3OE0 3OE1 3OE2 3OJ2 3OJ3 3OJ4 3OJ5 3OP0

.meta name="No rare selling"
.meta description="Stops you from\naccidentally\nselling rares\nto shops"

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC

  # See comments in the 59NL version of this patch for details on how it works.

  .data     <VERS 0x8010E114 0x8010E114 0x8010E00C 0x8010DE70 0x8010E070 0x8010E1BC 0x8010DFFC 0x8010E1F0>
  .data     0x00000004
  li        r29, 0

  .data     <VERS 0x8010E100 0x8010E100 0x8010DFF8 0x8010DE5C 0x8010E05C 0x8010E1A8 0x8010DFE8 0x8010E1DC>
  .data     0x00000004
  li        r29, 0

  .data     <VERS 0x8010E248 0x8010E248 0x8010E140 0x8010DFA4 0x8010E1A4 0x8010E2F0 0x8010E130 0x8010E324>
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

  .data     <VERS 0x8010E3BC 0x8010E3BC 0x8010E2B4 0x8010E118 0x8010E318 0x8010E464 0x8010E2A4 0x8010E498>
  .data     0x00000004
  .address  <VERS 0x8010E3BC 0x8010E3BC 0x8010E2B4 0x8010E118 0x8010E318 0x8010E464 0x8010E2A4 0x8010E498>
  bl        tool_check_start

  .data     0x00000000
  .data     0x00000000
