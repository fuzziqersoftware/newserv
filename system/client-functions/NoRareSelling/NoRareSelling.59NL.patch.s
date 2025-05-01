# Original patch by Soly, in Blue Burst Patch Project
# https://github.com/Solybum/Blue-Burst-Patch-Project

.meta name="No rare selling"
.meta description="Stops you from accidentally\nselling rares to vendors"

entry_ptr:
reloc0:
  .offsetof start
start:
  # This works by setting the item price to zero if it's rare, which causes
  # the game to prevent you from selling the item. For armors and weapons, this
  # is easy because there are easily-patchable opcodes within branches that
  # return a constant price for rare items.
  xor       eax, eax
  mov       [0x005D25AF], eax      # Rare armors
  mov       [0x005D26F1], eax      # Unidentified weapons
  mov       [0x005D2706], eax      # Rare weapons

  # For tools, it's harder to implement this, because the price comes from the
  # ItemPMT tools table and there is no branch for rares. Still, we can add a
  # branch to a stub to handle tools.
  pop       ecx
  push      5
  push      0x005D2528
  call      get_code_size
  .deltaof  patch_code, patch_code_end
get_code_size:
  pop       eax
  push      dword [eax]
  call      patch_code_end
patch_code:
  # TODO: It'd be nice to have something like WriteJumpToAndFromCode, since
  # this hook is supposed to return to a different place than where it was
  # called, hence this mov [esp].
  mov       dword [esp], 0x005D2576
  xor       edi, edi
  test      byte [eax + 0x14], 0x80  # flags & 0x80 = is rare
  cmovz     edi, [eax + 0x10]  # Use price from table if not rare
  ret
patch_code_end:
  push      ecx
  .include  WriteCallToCode-59NL
