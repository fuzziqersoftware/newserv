.meta name="MAG alert"
.meta description="Plays a sound when\nyour MAG is hungry"

entry_ptr:
reloc0:
  .offsetof start
start:
  pop       ecx
  push      6
  push      0x005D91E2
  call      get_code_size
  .deltaof  patch_code, patch_code_end
get_code_size:
  pop       eax
  push      dword [eax]
  call      patch_code_end
patch_code:  # [eax] (TItemMag* this @ ecx) -> void
  mov       dword [ecx + 0x01B8], eax
  mov       eax, [ecx + 0x00F8]
  movzx     eax, word [eax + 0x001C]  # eax = this->owner_player->entity_id
  cmp       [0x00A9C4F4], eax
  jne       patch_code_skip_sound
  push      0
  push      0
  push      0
  push      0xAC
  mov       eax, 0x00814298
  call      eax
  add       esp, 0x10
patch_code_skip_sound:
  ret
patch_code_end:
  push      ecx
  .include  WriteCallToCode-59NL
