.meta hide_from_patches_menu
.meta name="DMC"
.meta description="Mitigates effects\nof enemy health\ndesync"
.meta client_flag="0x2000000000000000"

entry_ptr:
reloc0:
  .offsetof start

write_call_to_code_multi:
  .include  WriteCallToCodeMulti-59NL
write_address_of_code:
  .include  WriteAddressOfCode-59NL

start:

  # Replace 6x09 with 6xE4 in subcommand handler table
  mov       dword [0x00A0FC30], 0x000600E4  # subcommand=0xE4, flags=6
  push      0x00A0FC34
  call      +4
  .deltaof  handle_6xE4_start, handle_6xE4_end
  pop       eax
  push      dword [eax]
  call      handle_6xE4_end

handle_6xE4_start:  # (G_6xE4* cmd @ [esp + 4]) -> void
  push      ebx
  push      esi
  push      edi

  test      byte [0x00AAB27C], 0x80
  jz        handle_6xE4_return
  mov       ebx, [esp + 0x10]  # cmd
  movzx     eax, word [ebx + 2]
  cmp       eax, 0x1000
  jl        handle_6xE4_return
  cmp       eax, 0x1B50
  jge       handle_6xE4_return

  movzx     eax, word [ebx + 2]
  .include  GetEnemyEntity-59NL  # auto* ene = get_enemy_entity(cmd->header.entity_id);
  push      eax

  movzx     eax, word [ebx + 2]
  and       eax, 0x0FFF
  imul      eax, eax, 0x0C
  add       eax, [0x00AB02B8]  # eax = state_for_enemy(cmd->header.entity_id)

  movzx     edx, word [eax + 0x06]  # st.total_damage
  movsx     esi, word [ebx + 0x04]  # cmd->hit_amount
  movzx     edi, word [ebx + 0x0A]  # cmd->max_hp
  add       edx, esi  # st.total_damage + cmd->hit_amount
  cmp       edx, edi
  jl        handle_6xE4_damage_less_than_max_hp
  mov       [eax + 0x06], di  # st.total_damage = cmd->max_hp;
  mov       edx, [eax]
  test      edx, 0x800
  jnz       handle_6xE4_return_pop_ene
  or        edx, 0x800
  mov       [eax], edx
  cmp       dword [esp], 0
  je        handle_6xE4_return_pop_ene
  push      edx  # out_cmd.flags
  sub       esp, 8
  mov       word [esp], 0x030A  # out_cmd.header.{subcommand,size}
  mov       si, [ebx + 2]
  mov       [esp + 2], si  # out_cmd.header.entity_id
  and       si, 0x0FFF
  mov       [esp + 4], si  # out_cmd.entity_index
  mov       [esp + 6], di  # out_cmd.total_damage
  mov       ecx, esp
  mov       edx, 0x008003E0
  call      edx  # send_and_handle_60(&out_cmd);
  add       esp, 0x10
  jmp       handle_6xE4_return

handle_6xE4_damage_less_than_max_hp:
  xor       edi, edi
  cmp       edx, edx
  cmovl     edx, edi
  mov       [eax + 0x06], dx  # st.total_damage = std::max<int16_t>(st.total_damage + cmd->hit_amount, 0);

  mov       edx, eax  # edx = ene_st
  mov       eax, [esp]  # eax = ene
  test      eax, eax
  jz        handle_6xE4_return_pop_ene
  mov       ecx, eax
  push      edx
  mov       edx, [ecx]
  call      [edx + 0x148]  # ene->vtable[0x52](ene, &st);

handle_6xE4_return_pop_ene:
  add       esp, 4
handle_6xE4_return:
  pop       edi
  pop       esi
  pop       ebx
  ret

handle_6xE4_end:
  call      write_address_of_code



  # Write TObjectV00b441c0::incr_hp_with_sync
  push      5
  push      0x00774448  # TObjectV00b441c0::v18_accept_hit (presumably Resta) - this is add_hp, not subtract_hp!
  push      5
  push      0x00777287  # TObjectV00b441c0::subtract_hp_if_not_in_state_2
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
  push      18
  call      +4
  .deltaof  on_add_or_subtract_hp_start, on_add_or_subtract_hp_end
  pop       eax
  push      dword [eax]
  call      on_add_or_subtract_hp_end

on_add_or_subtract_hp_start:  # (TObjectV00b441c0* this @ ecx, int16_t amount @ [esp + 4]) -> bool @ eax
  test      byte [0x00AAB27C], 0x80
  jz        on_add_or_subtract_hp_skip_send
  movzx     eax, word [ecx + 0x1C]  # ene->entity_id
  cmp       eax, 0x1000
  jl        on_add_or_subtract_hp_skip_send
  cmp       eax, 0x1B50
  jge       on_add_or_subtract_hp_skip_send

  and       eax, 0x0FFF
  imul      eax, eax, 0x0C
  add       eax, [0x00AB02B8]  # eax = state_for_enemy(cmd->header.entity_id)

  sub       esp, 0x0C
  mov       word [esp], 0x03E4
  mov       dx, [ecx + 0x1C]
  mov       [esp + 0x02], dx  # cmd.entity_id
  mov       dx, [esp + 0x10]
  cmp       dword [esp + 0x0C], 0x0077444D  # Check if callsite is add_hp
  jne       on_add_or_subtract_hp_skip_negate_amount
  neg       dx
on_add_or_subtract_hp_skip_negate_amount:
  mov       [esp + 0x04], dx  # cmd.hit_amount
  mov       dx, [eax + 6]
  mov       [esp + 0x06], dx  # cmd.total_damage_before_hit
  mov       dx, [ecx + 0x0334]
  mov       [esp + 0x08], dx  # cmd.current_hp
  mov       dx, [ecx + 0x02BC]
  mov       [esp + 0x0A], dx  # cmd.max_hp
  mov       edx, esp
  push      ecx
  push      0x0C
  push      edx
  mov       ecx, [0x00AAB284]
  mov       edx, 0x007D3F38
  call      edx  # send_60(root_protocol, &cmd, sizeof(cmd));
  pop       ecx
  add       esp, 0x0C

on_add_or_subtract_hp_skip_send:
  mov       eax, 0x00777414  # subtract_hp
  mov       edx, 0x007773D4  # add_hp
  cmp       dword [esp], 0x0077444D  # Check if callsite is add_hp
  cmove     eax, edx
  jmp       eax

on_add_or_subtract_hp_end:
  call      write_call_to_code_multi



  push      5
  push      0x0078781F
  push      1
  call      +4
  .deltaof  on_6x0A_patch_start, on_6x0A_patch_end
  pop       eax
  push      dword [eax]
  call      on_6x0A_patch_end

on_6x0A_patch_start:  # (TObjectV00b441c0* this @ ecx, int16_t amount @ [esp + 4]) -> bool @ eax
  test      byte [0x00AAB27C], 0x80
  jz        on_6x0A_patch_skip_write
  mov       [esp + 0x0A], cx
on_6x0A_patch_skip_write:
  ret

on_6x0A_patch_end:
  call      write_call_to_code_multi



  ret
