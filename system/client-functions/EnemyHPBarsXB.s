.meta visibility="all"
.meta key="EnemyHPBars"
.meta name="Enemy HP bars"
.meta description="Shows HP bars in\nenemy info windows"

.versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU

entry_ptr:
reloc0:
  .data   start

write_call_to_code_multi:
  .include  WriteCallToCodeMulti

start:
  push      edi
  push      0  # call count
  push      (hooks_end - hooks_start)  # code size
  call      hooks_end

hooks_start:
  .label    TBoss2DeRolLe_movement_data, <VERS 0x005FC0DC 0x005FC67C 0x006041BC 0x0060169C 0x00600F3C 0x0060169C 0x00601A3C>

enemy_name_for_rt_index:  # [std](uint32_t rt_index @ eax) -> const char* name @ eax
  mov       ecx, [<VERS 0x0062EE60 0x0062F4C0 0x00637008 0x006344F0 0x00633D88 0x006344F0 0x00634888>]  # language_text_string_tables
  mov       edx, [ecx + 0x08]  # Non-Ultimate names
  mov       ecx, [ecx + 0x0C]  # Ultimate names
  cmp       dword [<VERS 0x0071EF88 0x0071F5E8 0x0072712C 0x00724628 0x00723EAC 0x00724628 0x007249AC>], 3  # Difficulty level
  cmovl     ecx, edx
  mov       eax, [ecx + eax * 4]
  ret

get_enemy_hp_values:  # [std](TObjectV8047c128* enemy @ edi) -> current_hp @ eax, max_hp @ ecx, is_masked @ dl
  # Check if target is De Rol Le joint (segment)
  mov       eax, [edi + 4]  # r4 = type name pointer
  xor       dl, dl

  cmp       eax, <VERS 0x00487928 0x00488160 0x0048F110 0x0048F850 0x0048F108 0x0048F868 0x0048FC18>
  je        get_enemy_hp_values_target_is_de_rol_le_joint
  cmp       eax, <VERS 0x00481338 0x00481B70 0x00488B28 0x00489270 0x00488B38 0x00489288 0x00489648>
  je        get_enemy_hp_values_target_is_barba_ray_joint
  cmp       eax, <VERS 0x00487B20 0x00488358 0x0048F308 0x0048FA48 0x0048F300 0x0048FA60 0x0048FE10>
  je        get_enemy_hp_values_target_is_de_rol_le
  cmp       eax, <VERS 0x004814E4 0x00481D1C 0x00488CD4 0x0048941C 0x00488CE4 0x00489434 0x004897F4>
  je        get_enemy_hp_values_target_is_barba_ray

  # If the target is neither De Rol Le nor Barba Ray, then it uses the normal HP system; show those values
  movsx     ecx, word [edi + 0x02BC]  # max_hp
  movsx     eax, word [edi + 0x0330]  # current_hp
  jmp       get_enemy_hp_values_end

get_enemy_hp_values_target_is_de_rol_le_joint:
  # Check if shell (armor) is broken - if not, show the armor's remaining HP
  test      byte [edi + 0x0390], 0x01  # flags & 1 => shell is broken
  jnz       get_enemy_hp_values_de_rol_le_joint_shell_broken
  mov       eax, [edi + 0x0398]
  mov       ecx, [TBoss2DeRolLe_movement_data]
  mov       ecx, [ecx + 0x1C]
  mov       dl, 1
  jmp       get_enemy_hp_values_end
get_enemy_hp_values_de_rol_le_joint_shell_broken:
  # If the shell is broken, show De Rol Le's HP instead, even if DRL's mask is intact (since the face isn't targeted)
  mov       edi, [edi + 0x14]  # enemy = enemy->parent
  jmp       get_enemy_hp_values_de_rol_le_mask_broken

get_enemy_hp_values_target_is_de_rol_le:
  # Check if mask (facial armor) is broken
  test      byte [edi + 0x03C4], 0x08  # flags & 8 => mask is broken
  jnz       get_enemy_hp_values_de_rol_le_mask_broken
  mov       eax, [edi + 0x06B4]
  mov       ecx, [TBoss2DeRolLe_movement_data]
  mov       ecx, [ecx + 0x20]
  mov       dl, 1
  jmp       get_enemy_hp_values_end
get_enemy_hp_values_de_rol_le_mask_broken:
  # If the mask is broken, show De Rol Le's true HP
  mov       eax, [edi + 0x06B0]  # body_current_hp
  mov       ecx, [edi + 0x06AC]  # body_max_hp
  jmp       get_enemy_hp_values_end

get_enemy_hp_values_target_is_barba_ray_joint:
  # Check if shell (armor) is broken - if not, show the armor's remaining HP
  test      byte [edi + 0x03C0], 0x01  # flags & 1 => shell is broken
  jnz       get_enemy_hp_values_barba_ray_joint_shell_broken
  mov       eax, [edi + 0x03C8]
  mov       ecx, [edi + 0x14]
  mov       ecx, [ecx + 0x0624]
  mov       ecx, [ecx + 0x1C]  # max_hp = enemy->parent->movement_data_0F->iparam2
  mov       dl, 1
  jmp       get_enemy_hp_values_end
get_enemy_hp_values_barba_ray_joint_shell_broken:
  # If the shell is broken, show Barba Ray's HP instead, even if BR's mask is intact (since the face isn't targeted)
  mov       edi, [edi + 0x14]  # enemy = enemy->parent
  jmp       get_enemy_hp_values_barba_ray_mask_broken

get_enemy_hp_values_target_is_barba_ray:
  # Check if mask (facial armor) is broken
  test      byte [edi + 0x062C], 0x08  # flags & 8 => mask is broken
  jnz       get_enemy_hp_values_barba_ray_mask_broken
  mov       eax, [edi + 0x0704]
  mov       ecx, [edi + 0x0624]
  mov       ecx, [ecx + 0x20]  # max_hp = enemy->parent->movement_data_0F->iparam3
  mov       dl, 1
  jmp       get_enemy_hp_values_end
get_enemy_hp_values_barba_ray_mask_broken:
  # If the mask is broken, show Barba Ray's true HP
  mov       eax, [edi + 0x0700]  # body_current_hp
  mov       ecx, [edi + 0x06FC]  # body_max_hp

get_enemy_hp_values_end:
  xor       edi, edi
  cmp       eax, 0
  cmovl     eax, edi
  ret

update_enemy_hp_text:  # [std](TObjectV8047c128* enemy @ eax, TWindowLockOn* window @ edx) -> char* text @ eax, int32_t max_hp @ ecx, int32_t current_hp @ edx
  push      edi
  push      esi
  push      ebx
  mov       ebx, edx
  mov       edi, eax

  mov       eax, [edi + 0x0374]  # enemy->rt_index
  call      enemy_name_for_rt_index
  mov       esi, eax

  call      get_enemy_hp_values
  push      ecx  # max hp
  push      eax  # current hp
  push      ecx  # max_hp
  push      eax  # current_hp
  call      get_shell_str_ret
shell_str:
  .binary   " shell"00
get_shell_str_ret:
  pop       eax
  lea       ecx, [eax + 6]
  test      dl, dl
  cmovz     eax, ecx
  push      eax
  push      esi
  call      get_hp_format_str_ret
hp_format_str:
  .binary   "%s%s\n\nHP: %d/%d"00
get_hp_format_str_ret:
  lea       esi, [ebx + 0x74]
  push      esi
  mov       eax, <VERS 0x00313B22 0x00314722 0x00317D7A 0x00318308 0x00317D7A 0x00318338 0x00318858>  # swprintf[std+0](const char* fmt @ [esp+4], ... @ [esp+...]) -> uint32_t count @ eax
  call      eax
  add       esp, 0x18

  mov       eax, esi  # text pointer
  pop       edx  # current hp
  pop       ecx  # max hp
  pop       ebx
  pop       esi
  pop       edi
  ret

hook4_get_max_hp:  # 4OEU:0026B371; [std](TWindowLockOn* window @ ebx, TObjectV8047c128* enemy @ ebp) -> int32_t max_hp @ ecx
  mov       eax, ebp
  mov       edx, ebx
  jmp       update_enemy_hp_text

hook5_get_current_hp:  # 4OEU:0026B38F; [eax,ecx/](TWindowLockOn* window @ ebx, TObjectV8047c128* enemy @ ebp) -> int32_t current_hp @ edx
  push      eax
  push      ecx
  mov       edx, ebx
  mov       eax, ebp
  call      update_enemy_hp_text
  pop       ecx
  pop       eax
  ret

hook6_update_window_text:  # 4OEU:0026B158; [std+8](TWindowLockOn* window @ edi, TObjectV8047c128* enemy @ ebx) -> void
  mov       eax, ebx
  mov       edx, edi
  call      update_enemy_hp_text
  mov       [esp + 4], eax
  mov       eax, <VERS 0x002649C0 0x00264D80 0x00265130 0x00264E80 0x00264F80 0x00264EA0 0x00264FD0>
  jmp       eax
hooks_end:

  call      write_call_to_code_multi

  # Patch the patches with the correct call deltas before writing them

  .label    hook4_get_max_hp_call, <VERS 0x0026AD81 0x0026B0F1 0x0026B4D1 0x0026B241 0x0026B371 0x0026B261 0x0026B371>
  .label    hook5_get_current_hp_call, <VERS 0x0026AD9F 0x0026B10F 0x0026B4EF 0x0026B25F 0x0026B38F 0x0026B27F 0x0026B38F>
  .label    hook6_update_window_text_call, <VERS 0x0026AB68 0x0026AED8 0x0026B2B8 0x0026B028 0x0026B158 0x0026B048 0x0026B158>

  call      get_reference_point
get_reference_point:
  pop       edx

  lea       ecx, [eax + ((hook4_get_max_hp - hooks_start) - (hook4_get_max_hp_call + 5))]
  mov       [edx + (hook4_get_max_hp_call_opcode + 1 - get_reference_point)], ecx

  lea       ecx, [eax + ((hook5_get_current_hp - hooks_start) - (hook5_get_current_hp_call + 5))]
  mov       [edx + (hook5_get_current_hp_call_opcode + 1 - get_reference_point)], ecx

  lea       ecx, [eax + ((hook6_update_window_text - hooks_start) - (hook6_update_window_text_call + 5))]
  mov       [edx + (hook6_update_window_text_call_opcode + 1 - get_reference_point)], ecx

  pop       edi
  .include  WriteCodeBlocks

  .data     hook4_get_max_hp_call
  .data     7
hook4_get_max_hp_call_opcode:
  call      hook4_get_max_hp  # Delta is incorrect; will be fixed up before WriteCodeBlocks is called
  nop
  nop

  .data     hook5_get_current_hp_call
  .data     7
hook5_get_current_hp_call_opcode:
  call      hook5_get_current_hp  # Delta is incorrect; will be fixed up before WriteCodeBlocks is called
  nop
  nop

  .data     hook6_update_window_text_call
  .data     5
hook6_update_window_text_call_opcode:
  call      hook6_update_window_text  # Delta is incorrect; will be fixed up before WriteCodeBlocks is called

  # Clear window item flag that suppresses HP bar
  .label    flag_clear_patch, <VERS 0x0026ADA6 0x0026B116 0x0026B4F6 0x0026B266 0x0026B396 0x0026B286 0x0026B396>
  .data     flag_clear_patch
  .data     4
  .address  flag_clear_patch
  and       byte [eax + 4], 0xFD

  # Make TWindowLockOn 0x40 bytes bigger, for string buffer
  .label    TWindowLockOn_size_load, <VERS 0x0026A810 0x0026AB80 0x0026AF60 0x0026ACD0 0x0026AE00 0x0026ACF0 0x0026AE00>
  .data     TWindowLockOn_size_load
  .data     5
  .address  TWindowLockOn_size_load
  mov       ecx, 0xB4

  # Update window size
  .label    TWindowLockOn_window_size_solo, <VERS 0x0026ABA7 0x0026AF17 0x0026B2F7 0x0026B067 0x0026B197 0x0026B087 0x0026B197>
  .data     TWindowLockOn_window_size_solo
  .data     7
  .address  TWindowLockOn_window_size_solo
  mov       dword [edi + 0x3C], encode_float(125)
  .label    TWindowLockOn_window_size_multi, <VERS 0x0026AB9E 0x0026AF0E 0x0026B2EE 0x0026B05E 0x0026B18E 0x0026B07E 0x0026B18E>
  .data     TWindowLockOn_window_size_multi
  .data     7
  .address  TWindowLockOn_window_size_multi
  mov       dword [edi + 0x3C], encode_float(96)

  .data     <VERS 0x00545334 0x005459C4 0x0054D4AC 0x0054A92C 0x0054A1CC 0x0054A92C 0x0054ACCC>
  .data     0x00000004
  .data     encode_float(75)

  .data     <VERS 0x00545364 0x005459F4 0x0054D4DC 0x0054A95C 0x0054A1FC 0x0054A95C 0x0054ACFC>
  .data     0x00000004
  .data     encode_float(75)

  .data     <VERS 0x00545394 0x00545A24 0x0054D50C 0x0054A98C 0x0054A22C 0x0054A98C 0x0054AD2C>
  .data     0x00000004
  .data     encode_float(75)

  .data     <VERS 0x005453C4 0x00545A54 0x0054D53C 0x0054A9BC 0x0054A25C 0x0054A9BC 0x0054AD5C>
  .data     0x00000004
  .data     encode_float(75)

  .data     <VERS 0x005453F4 0x00545A84 0x0054D56C 0x0054A9EC 0x0054A28C 0x0054A9EC 0x0054AD8C>
  .data     0x00000004
  .data     encode_float(62)

  .data     <VERS 0x00545410 0x00545AA0 0x0054D588 0x0054AA08 0x0054A2A8 0x0054AA08 0x0054ADA8>
  .data     0x00000004
  .data     0xFF00FF15

  .data     0x00000000
  .data     0x00000000
