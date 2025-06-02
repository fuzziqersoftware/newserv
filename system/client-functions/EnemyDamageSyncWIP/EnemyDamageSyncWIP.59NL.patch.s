.meta hide_from_patches_menu
.meta name="DMC"
.meta description="Mitigates effects\nof enemy health\ndesync"

entry_ptr:
reloc0:
  .offsetof start

write_call_to_code_multi:
  .include  WriteCallToCodeMulti-59NL
write_address_of_code:
  .include  WriteAddressOfCode-59NL

start:

  # Change class_flags check to read only low 16 bits
  # This is annoying since the opcode we need is one byte longer than the
  # original, so we have to write a call to allocated code here, sigh
  push      6
  push      0x00773448
  push      1
  call      +4
  .deltaof  class_flags_check_start, class_flags_check_end
  pop       eax
  push      dword [eax]
  call      class_flags_check_end
class_flags_check_start:
  movzx     eax, word [esi + 0x2E8]
  ret
class_flags_check_end:
  call write_call_to_code_multi



  # Replace 6x09 with 6xE4 in subcommand handler table
  mov       dword [0x00A0FC30], 0x000600E4  # subcommand=0xE4, flags=6
  push      0x00A0FC34
  call      +4
  .deltaof  on_6xE4_start, on_6xE4_end
  pop       eax
  push      dword [eax]
  call      on_6xE4_end
on_6xE4_start:  # (G_6xE4* cmd @ [esp + 4])
  mov       edx, [esp + 4]
  movzx     eax, word [edx + 2]
  .include  GetEnemyEntity-59NL  # eax = get_enemy_entity(cmd->header.entity_id)
  test      eax, eax
  je        on_6xE4_no_entity
  movzx     ecx, word [eax + 0x2EA]
  movzx     edx, word [edx + 4]
  add       ecx, edx
  xor       edx, edx
  cmp       ecx, 0
  cmovl     ecx, edx
  movzx     edx, word [eax + 0x2BC]
  cmp       ecx, edx
  cmovg     ecx, edx
  mov       [eax + 0x2EA], cx
on_6xE4_no_entity:
  ret
on_6xE4_end:
  call      write_address_of_code



  # Write TObjectV00b441c0::add_hp_with_sync
  push      5
  push      0x00774448  # TObjectV00b441c0::v18_accept_hit (presumably Resta)
  push      1
  call      +4
  .deltaof  TObjectV00b441c0_add_hp_with_sync_start, TObjectV00b441c0_add_hp_with_sync_end
  pop       eax
  push      dword [eax]
  call      TObjectV00b441c0_add_hp_with_sync_end
TObjectV00b441c0_add_hp_with_sync_start:  # (TObjectV00b441c0* this @ ecx, int16_t amount @ [esp + 4]) -> bool @ eax
  mov       ax, [ecx + 0x1C]
  cmp       ax, 0x1000
  jl        TObjectV00b441c0_add_hp_with_sync_skip_send
  cmp       ax, 0x4000
  jge       TObjectV00b441c0_add_hp_with_sync_skip_send
  sub       esp, 0x0C
  mov       word [esp], 0x03E4
  mov       [esp + 0x02], ax  # cmd.header.entity_id = this->entity_id
  mov       ax, [esp + 0x10]
  neg       ax
  mov       [esp + 0x04], ax  # cmd.hit_amount = -amount
  mov       ax, [ecx + 0x2EA]
  mov       [esp + 0x06], ax  # cmd.total_damage_before_hit = this->total_damage
  mov       ax, [ecx + 0x334]
  mov       [esp + 0x08], ax  # cmd.current_hp_before_hit = this->current_hp
  mov       ax, [ecx + 0x2BC]
  mov       [esp + 0x0A], ax  # cmd.max_hp = this->max_hp
  push      ecx
  lea       ecx, [esp + 4]
  mov       eax, 0x008003E0
  call      eax  # send_and_handle_60(void* data @ ecx)
  pop       ecx
  add       esp, 0x0C
TObjectV00b441c0_add_hp_with_sync_skip_send:
  mov       eax, 0x007773D4  # TObjectV00b441c0::add_hp
  jmp       eax
TObjectV00b441c0_add_hp_with_sync_end:
  call      write_call_to_code_multi



  # Write TObjectV00b441c0::subtract_hp_with_sync
  push      5
  push      0x00777287  # TObjectV00b441c0::subtract_hp_if_in_state_2
  push      5
  push      0x00776CD6  # TObjectV00b441c0::v19_handle_hit_special_effects
  push      5
  push      0x00776D4F  # TObjectV00b441c0::v19_handle_hit_special_effects
  push      5
  push      0x00776E20  # TObjectV00b441c0::v19_handle_hit_special_effects
  push      5
  push      0x00776E99  # TObjectV00b441c0::v19_handle_hit_special_effects
  push      5
  push      0x00775F51  # TObjectV00b441c0::v19_handle_hit_special_effects
  push      5
  push      0x00775BE6  # TObjectV00b441c0::v19_handle_hit_special_effects
  push      5
  push      0x00775A60  # TObjectV00b441c0::v19_handle_hit_special_effects
  push      5
  push      0x00775726  # TObjectV00b441c0::v19_handle_hit_special_effects
  push      5
  push      0x00774D7B  # TObjectV00b441c0::v18_accept_hit
  push      5
  push      0x00774C47  # TObjectV00b441c0::v18_accept_hit
  push      5
  push      0x00774A14  # TObjectV00b441c0::v18_accept_hit
  push      5
  push      0x0077482A  # TObjectV00b441c0::v18_accept_hit
  push      5
  push      0x007746E0  # TObjectV00b441c0::v18_accept_hit
  push      5
  push      0x00774061  # TObjectV00b441c0::v18_accept_hit
  push      5
  push      0x00773EFA  # TObjectV00b441c0::v18_accept_hit
  push      5
  push      0x00773937  # TObjectV00b441c0::v17
  push      17
  call      +4
  .deltaof  TObjectV00b441c0_subtract_hp_with_sync_start, TObjectV00b441c0_subtract_hp_with_sync_end
  pop       eax
  push      dword [eax]
  call      TObjectV00b441c0_subtract_hp_with_sync_end
TObjectV00b441c0_subtract_hp_with_sync_start:  # (TObjectV00b441c0* this @ ecx, int16_t amount @ [esp + 4]) -> bool @ eax
  mov       ax, [ecx + 0x1C]
  cmp       ax, 0x1000
  jl        TObjectV00b441c0_subtract_hp_with_sync_skip_send
  cmp       ax, 0x4000
  jge       TObjectV00b441c0_subtract_hp_with_sync_skip_send
  sub       esp, 0x0C
  mov       word [esp], 0x03E4
  mov       [esp + 0x02], ax  # cmd.header.entity_id = this->entity_id
  mov       ax, [esp + 0x10]
  mov       [esp + 0x04], ax  # cmd.hit_amount = amount
  mov       ax, [ecx + 0x2EA]
  mov       [esp + 0x06], ax  # cmd.total_damage_before_hit = this->total_damage
  mov       ax, [ecx + 0x334]
  mov       [esp + 0x08], ax  # cmd.current_hp_before_hit = this->current_hp
  mov       ax, [ecx + 0x2BC]
  mov       [esp + 0x0A], ax  # cmd.max_hp = this->max_hp
  push      ecx
  lea       ecx, [esp + 4]
  mov       eax, 0x008003E0
  call      eax  # send_and_handle_60(void* data @ ecx)
  pop       ecx
  add       esp, 0x0C
TObjectV00b441c0_subtract_hp_with_sync_skip_send:
  mov       eax, 0x00777414  # TObjectV00b441c0::subtract_hp
  jmp       eax
TObjectV00b441c0_subtract_hp_with_sync_end:
  call      write_call_to_code_multi



  # Write TObjectV00b441c0::subtract_hp_without_sync
  push      5
  push      0x00777CBD  # TObjectV00b441c0::v25_give_poison_damage
  push      1
  call      +4
  .deltaof  TObjectV00b441c0_subtract_hp_without_sync_start, TObjectV00b441c0_subtract_hp_without_sync_end
  pop       eax
  push      dword [eax]
  call      TObjectV00b441c0_subtract_hp_without_sync_end
TObjectV00b441c0_subtract_hp_without_sync_start:  # (TObjectV00b441c0* this @ ecx, int16_t amount @ [esp + 4]) -> bool @ eax
  movzx     edx, word [ecx + 0x2EA]
  movsx     eax, word [esp + 2]
  add       edx, eax
  movzx     eax, word [ecx + 0x2BC]
  cmp       edx, eax
  cmovg     edx, eax
  mov       [ecx + 0x2EA], dx
  mov       eax, 0x00777414  # TObjectV00b441c0::subtract_hp
  jmp       eax
TObjectV00b441c0_subtract_hp_without_sync_end:
  call      write_call_to_code_multi



  # Write handle_6x0A_update_total_damage_hook
  push      5
  push      0x0078781F
  push      1
  call      +4
  .deltaof  handle_6x0A_update_total_damage_hook_start, handle_6x0A_update_total_damage_hook_end
  pop       eax
  push      dword [eax]
  call      handle_6x0A_update_total_damage_hook_end
handle_6x0A_update_total_damage_hook_start:  # (G_6x0A* cmd @ eax, int16_t cmd_total_damage @ cx) -> void
  # Nonstandard calling convention:
  #   Caller-save: ecx, ebx
  #   Callee-save: eax, edx, ebp, esi, edi
  push      eax
  movzx     eax, word [eax + 2]
  .include  GetEnemyEntity-59NL  # eax = get_enemy_entity(cmd->header.entity_id)
  test      eax, eax
  jz        handle_6x0A_update_total_damage_hook_no_entity
  mov       ebx, [eax + 0x2EA]
  cmp       ecx, ebx
  cmovl     ecx, ebx
  mov       ebx, [eax + 0x2BC]
  cmp       ecx, ebx
  cmovg     ecx, ebx
handle_6x0A_update_total_damage_hook_no_entity:
  mov       [esp + 0x0E], cx  # ene_st.total_damage = cx
  pop       eax
  ret
handle_6x0A_update_total_damage_hook_end:
  call      write_call_to_code_multi



  # Write handle_6x0A_call_object_update_vfn
  push      6
  push      0x007878CC
  push      1
  call      +4
  .deltaof  handle_6x0A_call_object_update_vfn_start, handle_6x0A_call_object_update_vfn_end
  pop       eax
  push      dword [eax]
  call      handle_6x0A_call_object_update_vfn_end
handle_6x0A_call_object_update_vfn_start:  # (TObjectV00b441c0* this @ ecx, EnemyState* ene_st @ [esp + 4]) -> void
  # Standard calling conventions
  push      dword [edx + 0x148]  # vfn to call at end (which we do via ret)
  push      ecx
  test      dword [ecx + 0x30], 0x800  # this->game_flags & 0x800
  jnz       handle_6x0A_call_object_update_vfn_tail_call
  mov       edx, [esp + 0x0C]
  test      dword [edx], 0x800  # ene_st->flags & 0x800
  jnz       handle_6x0A_call_object_update_vfn_tail_call
  mov       ax, [edx + 6]
  cmp       ax, [ecx + 0x2BC]  # ene_st->total_damage >= ene->max_hp
  jl        handle_6x0A_call_object_update_vfn_tail_call
  or        dword [edx], 0x800  # ene_st->game_flags |= 0x800 (set dead flag)
  mov       eax, [0x00AAB284]
  test      eax, eax
  jz        handle_6x0A_call_object_update_vfn_tail_call
  push      dword [edx]  # cmd.game_flags = ene_st->game_flags
  sub       esp, 8
  mov       ax, [edx + 6]
  mov       [esp + 0x06], ax  # cmd.total_damage = ene_st->total_damage
  mov       ax, [ecx + 0x2C]
  mov       [esp + 0x04], ax  # cmd.enemy_index = this->enemy_index
  mov       ax, [ecx + 0x1C]
  mov       [esp + 0x02], ax  # cmd.header.entity_id = this->entity_id
  mov       word [esp], 0x030A  # cmd.header.subommand = 0x0A, cmd.header.size = 0x03
  push      0x0C
  lea       ecx, [esp + 4]
  push      ecx
  mov       ecx, [0x00AAB284]
  mov       eax, 0x007D3F38
  call      eax  # send_60(TGameProtocol* this @ ecx, void* data @ [esp + 4], uint32_t size @ [esp + 8])
  add       esp, 0x0C
handle_6x0A_call_object_update_vfn_tail_call:
  pop       ecx
  ret
handle_6x0A_call_object_update_vfn_end:
  call      write_call_to_code_multi



  ret
