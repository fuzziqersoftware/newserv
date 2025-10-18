.meta name="DMC"
.meta description="Mitigates effects\nof enemy health\ndesync"
.meta client_flag="0x2000000000000000"

.versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU

entry_ptr:
reloc0:
  .offsetof start

write_call_to_code_multi:
  .include  WriteCallToCodeMultiXB



start:
  call      write_static_patches
  call      write_incr_hp_with_sync
  call      write_6x0A_patch
  ret



write_6x0A_patch:
  push      5
  push      <VERS 0x002B3B55 0x002B4625 0x002B5BB5 0x002B56C5 0x002B58A5 0x002B56E5 0x002B59B5>
  push      1
  call      +4
  .deltaof  on_6x0A_patch_start, on_6x0A_patch_end
  pop       eax
  push      dword [eax]
  call      on_6x0A_patch_end

on_6x0A_patch_start:  # (TObjectV004434c8* this @ eax, int16_t amount @ cx) -> bool @ eax
  test      byte [0x006354B8], 0x80
  jnz       on_6x0A_patch_skip_write
  mov       [esp + 0x16], ax
on_6x0A_patch_skip_write:
  ret

on_6x0A_patch_end:
  call      write_call_to_code_multi
  ret



  # Write TObjectV004434c8::incr_hp_with_sync
write_incr_hp_with_sync:
  push      5
  push      <VERS 0x002A60CF 0x002A6BAF 0x002A807F 0x002A7B0F 0x002A7CEF 0x002A7B2F 0x002A7DAF>  # v17 inlined callsite + 5
  push      5
  push      <VERS 0x002A808D 0x002A8B6D 0x002AA03D 0x002A9ACD 0x002A9CAD 0x002A9AED 0x002A9D6D>  # TObjectV004434c8::subtract_hp_if_not_in_state_2 + D
  push      5
  push      <VERS 0x002A68FB 0x002A73DB 0x002A88AB 0x002A833B 0x002A851B 0x002A835B 0x002A85DB>  # TObjectV004434c8::v18_accept_hit (presumably Resta) - this is add_hp, not subtract_hp!
  push      5
  push      <VERS 0x002A650D 0x002A6FED 0x002A84BD 0x002A7F4D 0x002A812D 0x002A7F6D 0x002A81ED>  # TObjectV004434c8::v18_accept_hit cases 0 and 4
  push      5
  push      <VERS 0x002A65BA 0x002A709A 0x002A856A 0x002A7FFA 0x002A81DA 0x002A801A 0x002A829A>  # TObjectV004434c8::v18_accept_hit case 1
  push      5
  push      <VERS 0x002A6670 0x002A7150 0x002A8620 0x002A80B0 0x002A8290 0x002A80D0 0x002A8350>  # TObjectV004434c8::v18_accept_hit case 2
  push      5
  push      <VERS 0x002A6769 0x002A7249 0x002A8719 0x002A81A9 0x002A8389 0x002A81C9 0x002A8449>  # TObjectV004434c8::v18_accept_hit case 3
  push      5
  push      <VERS 0x002A6C19 0x002A76F9 0x002A8BC9 0x002A8659 0x002A8839 0x002A8679 0x002A88F9>  # TObjectV004434c8::v18_accept_hit case 0x13
  push      5
  push      <VERS 0x002A6CAC 0x002A778C 0x002A8C5C 0x002A86EC 0x002A88CC 0x002A870C 0x002A898C>  # TObjectV004434c8::v18_accept_hit case 0x15
  push      5
  push      <VERS 0x002A70AE 0x002A7B92 0x002A9062 0x002A8AF2 0x002A8CD2 0x002A8B12 0x002A8D92>  # TObjectV004434c8::v19_handle_hit_special_effects case 1
  push      5
  push      <VERS 0x002A7A53 0x002A7BD3 0x002A90A3 0x002A8B33 0x002A8D13 0x002A8B53 0x002A8DD3>  # TObjectV004434c8::v19_handle_hit_special_effects case 1
  push      5
  push      <VERS 0x002A76C4 0x002A81A8 0x002A9678 0x002A9108 0x002A92E8 0x002A9128 0x002A93A8>  # TObjectV004434c8::v19_handle_hit_special_effects case 6
  push      5
  push      <VERS 0x002A7953 0x002A8437 0x002A9907 0x002A9397 0x002A9577 0x002A93B7 0x002A9637>  # TObjectV004434c8::v19_handle_hit_special_effects case 9
  push      5
  push      <VERS 0x0024A140 0x002A8530 0x002A9A00 0x002A9490 0x002A9670 0x002A94B0 0x002A9730>  # TObjectV004434c8::v19_handle_hit_special_effects case 0x0A
  push      5
  push      <VERS 0x002A7CDB 0x002A87BF 0x002A9C8F 0x002A971F 0x002A98FF 0x002A973F 0x002A99BF>  # TObjectV004434c8::v19_handle_hit_special_effects case 0x0D
  push      15
  call      +4
  .deltaof  on_add_or_subtract_hp_start, on_add_or_subtract_hp_end
  pop       eax
  push      dword [eax]
  call      on_add_or_subtract_hp_end

on_add_or_subtract_hp_start:  # (TObjectV004434c8* this @ eax, int16_t amount @ cx) -> bool @ eax
  # Check if callsite is subtract_hp_if_not_in_state_2

  push      eax
  push      ecx
  push      ebx

  test      byte [0x006354B8], 0x80
  jz        on_add_or_subtract_hp_skip_send
  movzx     edx, word [eax + 0x1C]  # ene->entity_id
  cmp       edx, 0x1000
  jl        on_add_or_subtract_hp_skip_send
  cmp       edx, 0x1B50
  jge       on_add_or_subtract_hp_skip_send

  and       edx, 0x0FFF
  imul      edx, edx, 0x0C
  add       edx, [<VERS 0x00633068 0x006336C8 0x0063B210 0x006386F8 0x00637F90 0x006386F8 0x00638A90>]  # eax = state_for_enemy(cmd->header.entity_id)

  sub       esp, 0x10
  mov       word [esp], 0x04E4
  mov       bx, [eax + 0x1C]
  mov       [esp + 0x02], bx  # cmd.entity_id
  cmp       dword [esp + 0x1C], <VERS 0x002A6900 0x002A73E0 0x002A88B0 0x002A8340 0x002A8520 0x002A8360 0x002A85E0>  # Check if callsite is add_hp
  jne       on_add_or_subtract_hp_skip_negate_amount
  neg       cx
on_add_or_subtract_hp_skip_negate_amount:
  mov       [esp + 0x04], cx  # cmd.hit_amount
  mov       bx, [edx + 6]
  mov       [esp + 0x06], bx  # cmd.total_damage_before_hit
  mov       bx, [eax + 0x0330]
  mov       [esp + 0x08], bx  # cmd.current_hp
  mov       bx, [eax + 0x02BC]
  mov       [esp + 0x0A], bx  # cmd.max_hp
  mov       dword [esp + 0x0C], 0xBF800000  # cmd.factor

  cmp       dword [esp + 0x1C], <VERS 0x002A7CE0 0x002A87C4 0x002A9C94 0x002A9724 0x002A9904 0x002A9744 0x002A99C4>  # Check if callsite is Devil's/Demon's
  jne       on_add_or_subtract_hp_not_proportional
  # esp is 0x20 down from where it is in caller's context
  mov       cx, 100
  sub       cx, [esp + 0x34]  # cx = (100 - special_amount)
  movsx     ecx, cx
  push      ecx
  fild      st0, dword [esp]  # current_hp_factor = static_cast<float>(100 - special_amount)
  fmul      st0, dword [esp + 0x3C]  # *= weapon_reduction_factor
  mov       dword [esp], 0x42C80000  # 100.0f
  fdiv      st0, dword [esp]
  add       esp, 4
  fstp      dword [esp + 0x0C], st0  # cmd.factor = ((100 - special_amount) * weapon_reduction_factor) / 100
on_add_or_subtract_hp_not_proportional:

  mov       ecx, esp
  mov       ebx, [<VERS 0x0071EEFC 0x0071F55C 0x007270A0 0x0072459C 0x00723E20 0x0072459C 0x00724920>]  # root_protocol
  test      ebx, ebx
  jz        on_add_or_subtract_hp_skip_send
  mov       eax, 0x10
  # Can't just `call <addr>` here because this code is relocated at apply time
  mov       edx, <VERS 0x002DA120 0x002DACF0 0x002DC5B0 0x002DC080 0x002DC580 0x002DC0B0 0x002DC600>
  call      edx  # send_60(root_protocol, &out_cmd, sizeof(out_cmd))
  add       esp, 0x10

on_add_or_subtract_hp_skip_send:
  mov       edx, <VERS 0x002A80C0 0x002A8BA0 0x002AA070 0x002A9B00 0x002A9CE0 0x002A9B20 0x002A9DA0>  # subtract_hp
  mov       eax, <VERS 0x002A80F0 0x002A8BD0 0x002AA0A0 0x002A9B30 0x002A9D10 0x002A9B50 0x002A9DD0>  # add_hp
  cmp       dword [esp + 0x0C], <VERS 0x002A6900 0x002A73E0 0x002A88B0 0x002A8340 0x002A8520 0x002A8360 0x002A85E0>  # Check if callsite is add_hp
  cmove     edx, eax
  pop       ebx
  pop       ecx
  pop       eax
  jmp       edx

on_add_or_subtract_hp_end:
  call      write_call_to_code_multi
  ret



write_static_patches:
  .include WriteCodeBlocksXB

  .data     <VERS 0x002DB7A0 0x002DC370 0x002DDC30 0x002DD700 0x002DDC00 0x002DD730 0x002DDC80>
  .data     9
flag_check_start:
  cmp       dword [0x006354B8], 0
  je        +0x38
flag_check_end:

  .data     <VERS 0x00537180 0x00537800 0x0053EB20 0x0053BFA0 0x0053B840 0x0053BFA0 0x0053C340>
  .data     8
  .data     0x000600E4  # subcommand=0xE4, flags=6
  .addrof   handle_6xE4

  .data     <VERS 0x002DA510 0x002DB0E0 0x002DC9A0 0x002DC470 0x002DC970 0x002DC4A0 0x002DC9F0>
  .deltaof  handle_91_replacement, handle_6xE4_end
  .address  <VERS 0x002DA510 0x002DB0E0 0x002DC9A0 0x002DC470 0x002DC970 0x002DC4A0 0x002DC9F0>
handle_91_replacement:  # [std] (S_91* cmd @ [esp + 4]) -> void
  ret       4
handle_6xE4:  # [std] (G_6xE4* cmd @ [esp + 4]) -> void
  push      ebx
  push      esi
  push      edi

  test      byte [0x006354B8], 0x80
  jz        handle_6xE4_return
  mov       ebx, [esp + 0x10]  # cmd
  movzx     eax, word [ebx + 2]
  cmp       eax, 0x1000
  jl        handle_6xE4_return
  cmp       eax, 0x1B50
  jge       handle_6xE4_return

  mov       edi, eax
  call      <VERS 0x002B36B0 0x002B4180 0x002B5710 0x002B5220 0x002B5400 0x002B5240 0x002B5510>  # TObjEnemy* ene = get_enemy_entity(cmd->header.entity_id);
  push      eax

  movzx     eax, word [ebx + 2]
  and       eax, 0x0FFF
  imul      eax, eax, 0x0C
  add       eax, [<VERS 0x00633068 0x006336C8 0x0063B210 0x006386F8 0x00637F90 0x006386F8 0x00638A90>]  # eax = state_for_enemy(cmd->header.entity_id)

  cmp       dword [ebx + 0x0C], 0
  jl        handle_6xE4_not_proportional
  mov       cx, [ebx + 0x0A]  # cmd->max_hp
  sub       cx, [eax + 0x06]  # st.total_damage
  movzx     ecx, cx
  xor       edx, edx
  cmp       ecx, edx
  cmovl     ecx, edx
  push      ecx
  fild      st0, dword [esp]  # current_hp = static_cast<float>(max<int32_t>(cmd->max_hp - st.total_damage, 0))
  fld       st0, dword [ebx + 0x0C]
  fmulp     st1, st0
  fistp     dword [esp], st0
  mov       ecx, dword [esp]  # adjusted_hit_amount = static_cast<int16_t>(current_hp * cmd->factor)
  add       esp, 4
  xor       edx, edx
  inc       edx
  cmp       ecx, edx
  cmovl     ecx, edx
  mov       [ebx + 0x04], cx  # cmd->hit_amount = min<int32_t>(1, adjusted_hit_amount)
handle_6xE4_not_proportional:

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
  push      ecx  # For handle_60 later
  mov       ebx, [<VERS 0x0071EEFC 0x0071F55C 0x007270A0 0x0072459C 0x00723E20 0x0072459C 0x00724920>]  # root_protocol
  test      ebx, ebx
  jz        handle_6xE4_root_protocol_missing
  mov       eax, 0x0C
  call      <VERS 0x002DA120 0x002DACF0 0x002DC5B0 0x002DC080 0x002DC580 0x002DC0B0 0x002DC600>  # send_60(root_protocol, &out_cmd, sizeof(out_cmd))
handle_6xE4_root_protocol_missing:
  mov       dword [<VERS 0x0071E8C8 0x0071EF28 0x00726A68 0x00723F68 0x007237E8 0x00723F68 0x007242E8>], 1
  call      <VERS 0x002DBC30 0x002DC7B0 0x002DE070 0x002DDB90 0x002DE090 0x002DDBC0 0x002DE0C0>  # handle_60(&out_cmd)
  mov       dword [<VERS 0x0071E8C8 0x0071EF28 0x00726A68 0x00723F68 0x007237E8 0x00723F68 0x007242E8>], 0

  add       esp, 0x14
  jmp       handle_6xE4_return

handle_6xE4_damage_less_than_max_hp:
  xor       edi, edi
  cmp       edx, edx
  cmovl     edx, edi
  mov       [eax + 0x06], dx  # st.total_damage = std::max<int16_t>(st.total_damage + cmd->hit_amount, 0);

  mov       esi, eax  # esi = ene_st
  mov       eax, [esp]  # eax = ene
  test      eax, eax
  jz        handle_6xE4_return_pop_ene
  mov       ecx, eax
  push      esi
  mov       edx, [ecx]
  call      [edx + 0x138]  # ene->vtable[0x4E](ene, &st);

handle_6xE4_return_pop_ene:
  add       esp, 4
handle_6xE4_return:
  pop       edi
  pop       esi
  pop       ebx
  ret
handle_6xE4_end:



  # Rewrite TObjectV004434c8::subtract_hp_if_not_in_state_2
  .data     <VERS 0x002A8080 0x002A8B60 0x002AA030 0x002A9AC0 0x002A9CA0 0x002A9AE0 0x002A9D60>
  .deltaof  on_subtract_hp_if_not_in_state_2_start, on_subtract_hp_if_not_in_state_2_end
  .address  <VERS 0x002A8080 0x002A8B60 0x002AA030 0x002A9AC0 0x002A9CA0 0x002A9AE0 0x002A9D60>
on_subtract_hp_if_not_in_state_2_start:  # (TObjectV004434c8* this @ eax, int16_t amount @ cx) -> bool @ eax
  cmp       word [eax + 0x328], 2
  jne       on_subtract_hp_if_not_in_state_2_do_subtract
  xor       eax, eax
  ret
on_subtract_hp_if_not_in_state_2_do_subtract:
  call      -1  # Overwritten by write_call_to_code_multi later
  ret
on_subtract_hp_if_not_in_state_2_end:



  # Inlined callsite of subtract_hp in TObjectV004434c8::v17
  .data     <VERS 0x002A60CA 0x002A6BAA 0x002A807A 0x002A7B0A 0x002A7CEA 0x002A7B2A 0x002A7DAA>
  .deltaof  v17_subtract_hp_inlined_callsite_start, v17_subtract_hp_inlined_callsite_end
  .address  <VERS 0x002A60CA 0x002A6BAA 0x002A807A 0x002A7B0A 0x002A7CEA 0x002A7B2A 0x002A7DAA>
v17_subtract_hp_inlined_callsite_start:
  # This must assemble to exactly 0x1A bytes. There is a vfn call shortly after
  # this, and fortunately it appears eax, ecx, and edx are not used before
  # then, so we don't have to save any registers here; we just have to move the
  # args into the right places.
  mov       cx, ax
  mov       eax, edi
  call      -1  # Overwritten by write_call_to_code_multi later
  jmp       v17_subtract_hp_inlined_callsite_end
  int       3
  int       3
  int       3
  int       3
  int       3
  int       3
  int       3
  int       3
  int       3
  int       3
  int       3
  int       3
  int       3
  int       3
v17_subtract_hp_inlined_callsite_end:



  .data     0x00000000
  .data     0x00000000
