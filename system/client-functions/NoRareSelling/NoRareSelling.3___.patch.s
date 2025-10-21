# Original patch by Soly, in Blue Burst Patch Project
# https://github.com/Solybum/Blue-Burst-Patch-Project
# GC port by fuzziqersoftware

.versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

.meta name="No rare selling"
.meta description="Stops you from\naccidentally\nselling rares\nto shops"

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC

  # See comments in the 59NL version of this patch for details on how it works.

  .data     <VERS 0x8010DE70 0x8010E070 0x8010E1BC 0x8010DFFC 0x8010E114 0x8010E114 0x8010E00C 0x8010E1F0>
  .data     0x00000004
  li        r29, 0

  .data     <VERS 0x8010DE5C 0x8010E05C 0x8010E1A8 0x8010DFE8 0x8010E100 0x8010E100 0x8010DFF8 0x8010E1DC>
  .data     0x00000004
  li        r29, 0

  .data     <VERS 0x8010DFA4 0x8010E1A4 0x8010E2F0 0x8010E130 0x8010E248 0x8010E248 0x8010E140 0x8010E324>
  .data     0x00000004
  li        r29, 0

  .label    tool_check_hook_loc, 0x800041A0
  .data     tool_check_hook_loc
  .deltaof  tool_check_hook_start, tool_check_hook_end
  .address  tool_check_hook_loc
tool_check_hook_start:
  lwz       r29, [r3 + 0x10]  # Flags
  xori      r29, r29, 0x0080
  andi.     r29, r29, 0x0080
  bnelr     # Not rare; r29 (returned price) is zero already
  lwz       r29, [r3 + 0x0C]  # Cost
  blr
tool_check_hook_end:

  .label    tool_check_hook_call, <VERS 0x8010E118 0x8010E318 0x8010E464 0x8010E2A4 0x8010E3BC 0x8010E3BC 0x8010E2B4 0x8010E498>
  .data     tool_check_hook_call
  .data     0x00000004
  .address  tool_check_hook_call
  bl        tool_check_hook_start

  .data     0x00000000
  .data     0x00000000
