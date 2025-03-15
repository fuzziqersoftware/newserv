# Credits to Soly from Blue Burst Patch Project

.meta name="Unsellable rare items"
.meta description="Stops you from accidentally\nselling rares to vendor"

entry_ptr:
reloc0:
  .offsetof start
start:
  xor       eax, eax
  mov       [0x005D25AF], eax      # Rare Armor
  mov       [0x005D26F1], eax      # Untekked Weapons
  mov       [0x005D2706], eax      # Rare Weapons
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
  mov       edi, 0x005D2576        # change return address
  mov       [esp], edi
  mov       edi, [eax + 0x14]
  and       edi, 0x80
  je        _not_rare
  mov       edi, 0
  ret
_not_rare:
  mov       edi, [eax + 0x10]
  ret
patch_code_end:
  push      ecx
  .include  WriteCallToCode-59NL
