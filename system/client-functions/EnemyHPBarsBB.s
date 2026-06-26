.meta visibility="all"
.meta key="EnemyHPBars"
.meta name="Enemy HP bars"
.meta description="Shows HP bars in\nenemy info windows"

.versions 50YJ 59NJ 59NL

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
  .label    TBoss2DeRolLe_movement_data, <VERS 0x00A378C8 0x00A41848 0x00A43CC8>

get_enemy_hp_values:  # [std](TObjectV8047c128* enemy @ edi) -> current_hp @ eax, max_hp @ ecx, is_masked @ dl
  # Check if target is De Rol Le joint (segment)
  mov       eax, [edi + 4]  # r4 = type name pointer

  xor       dl, dl

  cmp       eax, <VERS 0x00A379AC 0x00A4192C 0x00A43DAC>
  je        get_enemy_hp_values_target_is_de_rol_le_joint
  cmp       eax, <VERS 0x00A3B6EC 0x00A4568C 0x00A47B0C>
  je        get_enemy_hp_values_target_is_barba_ray_joint
  cmp       eax, <VERS 0x00A3792C 0x00A418AC 0x00A43D2C>
  je        get_enemy_hp_values_target_is_de_rol_le
  cmp       eax, <VERS 0x00A3B6D8 0x00A45678 0x00A47AF8>
  je        get_enemy_hp_values_target_is_barba_ray

  # If the target is neither De Rol Le nor Barba Ray, then it uses the normal HP system; show those values
  movsx     ecx, word [edi + 0x02BC]  # max_hp
  movsx     eax, word [edi + 0x0334]  # current_hp
  jmp       get_enemy_hp_values_end

get_enemy_hp_values_target_is_de_rol_le_joint:
  # Check if shell (armor) is broken - if not, show the armor's remaining HP
  test      byte [edi + 0x0394], 0x01  # flags & 1 => shell is broken
  jnz       get_enemy_hp_values_de_rol_le_joint_shell_broken
  mov       eax, [edi + 0x039C]
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
  test      byte [edi + 0x03C8], 0x08  # flags & 8 => mask is broken
  jnz       get_enemy_hp_values_de_rol_le_mask_broken
  mov       eax, [edi + 0x06B8]
  mov       ecx, [TBoss2DeRolLe_movement_data]
  mov       ecx, [ecx + 0x20]
  mov       dl, 1
  jmp       get_enemy_hp_values_end
get_enemy_hp_values_de_rol_le_mask_broken:
  # If the mask is broken, show De Rol Le's true HP
  mov       eax, [edi + 0x06B4]  # body_current_hp
  mov       ecx, [edi + 0x06B0]  # body_max_hp
  jmp       get_enemy_hp_values_end

get_enemy_hp_values_target_is_barba_ray_joint:
  # Check if shell (armor) is broken - if not, show the armor's remaining HP
  test      byte [edi + 0x03C4], 0x01  # flags & 1 => shell is broken
  jnz       get_enemy_hp_values_barba_ray_joint_shell_broken
  mov       eax, [edi + 0x03CC]
  mov       ecx, [edi + 0x14]
  mov       ecx, [ecx + 0x0628]
  mov       ecx, [ecx + 0x1C]  # max_hp = enemy->parent->movement_data_0F->iparam2
  mov       dl, 1
  jmp       get_enemy_hp_values_end
get_enemy_hp_values_barba_ray_joint_shell_broken:
  # If the shell is broken, show Barba Ray's HP instead, even if BR's mask is intact (since the face isn't targeted)
  mov       edi, [edi + 0x14]  # enemy = enemy->parent
  jmp       get_enemy_hp_values_barba_ray_mask_broken

get_enemy_hp_values_target_is_barba_ray:
  # Check if mask (facial armor) is broken
  test      byte [edi + 0x0630], 0x08  # flags & 8 => mask is broken
  jnz       get_enemy_hp_values_barba_ray_mask_broken
  mov       eax, [edi + 0x0708]
  mov       ecx, [edi + 0x0628]
  mov       ecx, [ecx + 0x20]  # max_hp = enemy->parent->movement_data_0F->iparam3
  mov       dl, 1
  jmp       get_enemy_hp_values_end
get_enemy_hp_values_barba_ray_mask_broken:
  # If the mask is broken, show Barba Ray's true HP
  mov       eax, [edi + 0x0704]  # body_current_hp
  mov       ecx, [edi + 0x0700]  # body_max_hp

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

  push      dword [edi + 0x0378]  # enemy->rt_index
  mov       esi, <VERS 0x0078CA14 0x00793E60 0x00793014>  # enemy_name_for_rt_index[std+0](uint32_t rt_index @ [esp + 4]) -> const char* name @ eax
  call      esi
  add       esp, 4
  mov       esi, eax

  call      get_enemy_hp_values
  push      ecx  # max hp
  push      eax  # current hp
  push      ecx  # max_hp
  push      eax  # current_hp
  call      get_shell_str_ret
shell_str:
  .binary   ' shell'0000
get_shell_str_ret:
  pop       eax
  lea       ecx, [eax + 0x0C]
  test      dl, dl
  cmovz     eax, ecx
  push      eax
  push      esi
  call      get_hp_format_str_ret
hp_format_str:
  .binary   '%s%s\n\nHP: %d / %d'0000
get_hp_format_str_ret:
  lea       esi, [ebx + 0x74]
  push      esi
  mov       eax, <VERS 0x0082C2F9 0x00835578 0x00857E29>  # swprintf[std+0](const wchar_t* fmt @ [esp+4], ... @ [esp+...]) -> uint32_t count @ eax
  call      eax
  add       esp, 0x18

  mov       eax, esi  # text pointer
  pop       edx  # current hp
  pop       ecx  # max hp
  pop       ebx
  pop       esi
  pop       edi
  ret

hook4_get_max_hp:  # 59NL:007318B7; [eax,ecx/](TWindowLockOn* window @ ebx, TObjectV8047c128* enemy @ eax) -> int32_t max_hp @ edx
  push      eax
  push      ecx
  mov       edx, ebx
  call      update_enemy_hp_text
  mov       edx, ecx
  pop       ecx
  pop       eax
  ret

hook5_get_current_hp:  # 59NL:007318C7; [eax,ecx/](TWindowLockOn* window @ ebx, TObjectV8047c128* enemy @ eax) -> int32_t current_hp @ edx
  push      eax
  push      ecx
  mov       edx, ebx
  call      update_enemy_hp_text
  pop       ecx
  pop       eax
  ret

hook6_update_window_text:  # 59NL:00731F08; [ecx/](TWindowLockOn* window @ ebp, TObjectV8047c128* enemy @ ecx) -> wchar_t* text @ eax
  push      ecx
  mov       eax, ecx
  mov       edx, ebp
  call      update_enemy_hp_text
  pop       ecx
  ret
hooks_end:

  call      write_call_to_code_multi
  mov       edi, eax

  mov       eax, <VERS 0x0072B11B 0x00731957 0x007318B7>  # hook4_get_max_hp_call
  mov       byte [eax], 0xE8
  lea       ecx, [edi + (hook4_get_max_hp - hooks_start + 5)]
  sub       ecx, eax
  mov       [eax + 1], ecx
  mov       word [eax + 5], 0x9090

  mov       eax, <VERS 0x0072B12B 0x00731967 0x007318C7>  # hook5_get_current_hp_call
  mov       byte [eax], 0xE8
  lea       ecx, [edi + (hook5_get_current_hp - hooks_start + 5)]
  sub       ecx, eax
  mov       [eax + 1], ecx
  mov       word [eax + 5], 0x9090

  mov       eax, <VERS 0x0072B76C 0x00731FA8 0x00731F08>  # hook6_update_window_text_call
  mov       byte [eax], 0xE8
  lea       ecx, [edi + (hook6_update_window_text - hooks_start + 5)]
  sub       ecx, eax
  mov       [eax + 1], ecx

  pop       edi
  .include  WriteCodeBlocks

  # Clear window item flag that suppresses HP bar
  .label    flag_clear_patch, <VERS 0x0072B141 0x0073197D 0x007318DD>
  .data     flag_clear_patch
  .data     6
  .address  flag_clear_patch
  and       edx, 0xFFFFFFFD

  # Make TWindowLockOn 0x80 bytes bigger, for string buffer
  .label    TWindowLockOn_size_load, <VERS 0x0072B4A8 0x00731CE4 0x00731C44>
  .data     TWindowLockOn_size_load
  .data     9
  .address  TWindowLockOn_size_load
  push      0xF4  # Originally `push 0x74`; deleted a preceding opcode, which writes a value which seemingly isn't used
  nop
  nop
  nop
  nop

  # Update window size
  .label    TWindowLockOn_window_size_init, <VERS 0x0072B78E 0x00731FCA 0x00731F2A>
  .data     TWindowLockOn_window_size_init
  .data     7
  .address  TWindowLockOn_window_size_init
  mov       dword [ebp + 0x3C], encode_float(125)
  .label    TWindowLockOn_window_size_update, <VERS 0x0072B413 0x00731C4F 0x00731BAF>
  .data     TWindowLockOn_window_size_update
  .data     7
  .address  TWindowLockOn_window_size_update
  mov       dword [ebp + 0x3C], encode_float(125)
  .data     <VERS 0x009649F8 0x0096F098 0x009710B8>
  .data     4
  .data     encode_float(125)

  .data     <VERS 0x009E6D84 0x009F0DA4 0x009F2DA4>
  .data     0x00000004
  .data     encode_float(75)

  .data     <VERS 0x009E6DB4 0x009F0DD4 0x009F2DD4>
  .data     0x00000004
  .data     encode_float(75)

  .data     <VERS 0x009E6DE4 0x009F0E04 0x009F2E04>
  .data     0x00000004
  .data     encode_float(75)

  .data     <VERS 0x009E6E14 0x009F0E34 0x009F2E34>
  .data     0x00000004
  .data     encode_float(75)

  .data     <VERS 0x009E6E44 0x009F0E64 0x009F2E64>
  .data     0x00000004
  .data     encode_float(62)

  .data     <VERS 0x009E6E60 0x009F0E80 0x009F2E80>
  .data     0x00000004
  .data     0xFF00FF15

  .data     0x00000000
  .data     0x00000000
