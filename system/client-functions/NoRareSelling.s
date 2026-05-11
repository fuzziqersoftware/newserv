# Original patch by Soly, in Blue Burst Patch Project
# https://github.com/Solybum/Blue-Burst-Patch-Project
# GC and Xbox ports by fuzziqersoftware

.meta visibility="all"
.meta name="No rare selling"
.meta description="Stops you from\naccidentally\nselling rares\nto shops"

entry_ptr:
reloc0:
  .offsetof start



.versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

start:
  .include  WriteCodeBlocks

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



.versions 4OED 4OEU 4OJB 4OJD 4OJU 4OPD 4OPU

start:
  .include  WriteCodeBlocks

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



.versions 59NJ 59NL

start:
  # This works by setting the item price to zero if it's rare, which causes the game to prevent you from selling the
  # item. For armors and weapons, this is easy because there are easily-patchable opcodes within branches that return a
  # constant price for rare items.
  xor       eax, eax
  mov       [<VERS 0x005D258F 0x005D25AF>], eax      # Rare armors
  mov       [<VERS 0x005D26D1 0x005D26F1>], eax      # Unidentified weapons
  mov       [<VERS 0x005D26E6 0x005D2706>], eax      # Rare weapons

  # For tools, it's harder to implement this, because the price comes from the ItemPMT tools table and there is no
  # branch for rares. Still, we can add a branch to a stub to handle tools.
  pop       ecx
  push      5
  push      <VERS 0x005D2508 0x005D2528>
  call      get_code_size
  .deltaof  patch_code, patch_code_end
get_code_size:
  pop       eax
  push      dword [eax]
  call      patch_code_end
patch_code:
  # TODO: It'd be nice to have something like WriteJumpToAndFromCode, since this hook is supposed to return to a
  # different place than where it was called, hence this mov [esp].
  mov       dword [esp], <VERS 0x005D2556 0x005D2576>
  xor       edi, edi
  test      byte [eax + 0x14], 0x80  # flags & 0x80 = is rare
  cmovz     edi, [eax + 0x10]  # Use price from table if not rare
  ret
patch_code_end:
  push      ecx
  .include  WriteCallToCode
