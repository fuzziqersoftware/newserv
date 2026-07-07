# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

.meta visibility="all"
.meta key="EnemyHPBars"
.meta name="Enemy HP bars"
.meta description="Shows HP bars in\nenemy info windows"

.versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

entry_ptr:
reloc0:
  .data     start
start:
  .include  WriteCodeBlocks

  .label    hook_addr, 0x8000BF5C
  .label    sprintf, <VERS 0x80395EFC 0x80398904 0x8039A7A4 0x8039A554 0x803971CC 0x80397224 0x8039A924 0x80399414>
  .label    enemy_name_for_rt_index, <VERS 0x801326FC 0x80132960 0x80132AA0 0x80132A00 0x801329A0 0x801329A0 0x80132A10 0x80132BE8>  # [std](uint32_t rt_index @ [esp + 4]) -> wchar_t* name @ eax
  .label    TBoss7Joint_get_shell_max_hp, <VERS 0x802EB164 0x802EC13C 0x802ED628 0x802ED340 0x802EBAC8 0x802EBB0C 0x802ED4FC 0x802EC8D4>

  .data     hook_addr
  .data     hooks_end - hooks_start
  .address  hook_addr
hooks_start:

strcmp_r4_r5_cr0:  # [/r4,r5,r6,r7,cr0](const char* a @ r4, const char* b @ r5) -> bool is_equal @ cr0
  subi      r4, r4, 1
  subi      r5, r5, 1
strcmp_r4_r5_cr0_again:
  lbzu      r6, [r4 + 1]
  lbzu      r7, [r5 + 1]
  cmp       r6, r7
  bnelr
  cmpwi     r6, 0
  bne       strcmp_r4_r5_cr0_again
  blr

get_enemy_hp_values:  # [/r0,r3,r4,r5,r6,r7](TObjectV8047c128* enemy @ r3) -> current_hp @ r3, max_hp @ r4, is_masked @ r5
  mflr      r0
  stwu      [r1 - 0x10], r1
  stw       [r1 + 0x14], r0

  # Check if target is De Rol Le joint (segment)
  lwz       r4, [r3]  # r4 = type name pointer
  bl        get_enemy_hp_values_TBoss2Joint_string
  .binary   "TBoss2Joint"00
get_enemy_hp_values_TBoss2Joint_string:
  mflr      r5
  bl        strcmp_r4_r5_cr0
  bne       get_enemy_hp_values_not_de_rol_le_joint
  # Check if shell (armor) is broken - if not, show the armor's remaining HP
  lwz       r0, [r3 + 0x038C]
  rlwinm.   r0, r0, 0, 31, 31  # flags & 1 => shell is broken
  bne       get_enemy_hp_values_de_rol_le_joint_shell_broken
  lwz       r3, [r3 + 0x0394]
  lwz       r4, [r13 - <VERS 0x5A90 0x5A90 0x5A70 0x5A70 0x5A80 0x5A80 0x5A60 0x5A20>]  # TBoss2DeRolLe_movement_data
  lwz       r4, [r4 + 0x1C]  # r5 = TBoss2DeRolLe_movement_data->iparam2
  li        r5, 1
  b         get_enemy_hp_values_end
get_enemy_hp_values_de_rol_le_joint_shell_broken:
  # If the shell is broken, show De Rol Le's HP instead (ignoring whether its mask is intact)
  lwz       r3, [r3 + 0x10]  # enemy = enemy->parent
  b         get_enemy_hp_values_de_rol_le_mask_broken

  # Check if target is De Rol Le itself
get_enemy_hp_values_not_de_rol_le_joint:
  lwz       r4, [r3]  # r4 = type name pointer
  bl        get_enemy_hp_values_TBoss2DeRolLe_string
  .binary   "TBoss2DeRolLe"000000
get_enemy_hp_values_TBoss2DeRolLe_string:
  mflr      r5
  bl        strcmp_r4_r5_cr0
  bne       get_enemy_hp_values_not_de_rol_le
get_enemy_hp_values_from_de_rol_le:
  # Check if mask (facial armor) is broken
  lwz       r0, [r3 + 0x03C0]
  rlwinm.   r0, r0, 0, 28, 28  # flags & 8 => mask is broken
  bne       get_enemy_hp_values_de_rol_le_mask_broken
  lwz       r3, [r3 + 0x06B0]
  lwz       r4, [r13 - <VERS 0x5A90 0x5A90 0x5A70 0x5A70 0x5A80 0x5A80 0x5A60 0x5A20>]  # TBoss2DeRolLe_movement_data
  lwz       r4, [r4 + 0x20]  # r5 = TBoss2DeRolLe_movement_data->iparam3
  li        r5, 1
  b         get_enemy_hp_values_end
get_enemy_hp_values_de_rol_le_mask_broken:
  # If the mask is broken, show De Rol Le's true HP
  lwz       r4, [r3 + 0x06A8]  # body_max_hp
  lwz       r3, [r3 + 0x06AC]  # body_current_hp
  li        r5, 0
  b         get_enemy_hp_values_end

  # Check if target is Barba Ray joint (segment)
get_enemy_hp_values_not_de_rol_le:
  lwz       r4, [r3]  # r4 = type name pointer
  bl        get_enemy_hp_values_TBoss7Joint_string
  .binary   "TBoss7Joint"00
get_enemy_hp_values_TBoss7Joint_string:
  mflr      r5
  bl        strcmp_r4_r5_cr0
  bne       get_enemy_hp_values_not_barba_ray_joint
  # Check if shell (armor) is broken - if not, show the armor's remaining HP
  lwz       r0, [r3 + 0x03BC]
  rlwinm.   r0, r0, 0, 31, 31  # flags & 1 => shell is broken
  bne       get_enemy_hp_values_barba_ray_joint_shell_broken
  lwz       r4, [r3 + 0x10]
  lwz       r4, [r4 + 0x0620]
  lwz       r4, [r4 + 0x1C]  # max_hp = enemy->parent->movement_data_0F->iparam2
  lwz       r3, [r3 + 0x03C4]
  li        r5, 1
  b         get_enemy_hp_values_end
get_enemy_hp_values_barba_ray_joint_shell_broken:
  # If the shell is broken, show Barba Ray's HP instead (ignoring whether its mask is intact)
  lwz       r3, [r3 + 0x10]  # enemy = enemy->parent
  b         get_enemy_hp_values_barba_ray_mask_broken

  # Check if target is Barba Ray itself
get_enemy_hp_values_not_barba_ray_joint:
  lwz       r4, [r3]  # r4 = type name pointer
  bl        get_enemy_hp_values_TBoss7DeRolLeC_string
  .binary   "TBoss7DeRolLeC"0000
get_enemy_hp_values_TBoss7DeRolLeC_string:
  mflr      r5
  bl        strcmp_r4_r5_cr0
  bne       get_enemy_hp_values_not_barba_ray
get_enemy_hp_values_from_barba_ray:
  # Check if mask (facial armor) is broken
  lwz       r0, [r3 + 0x0628]
  rlwinm.   r0, r0, 0, 28, 28  # flags & 8 => mask is broken
  bne       get_enemy_hp_values_barba_ray_mask_broken
  lwz       r4, [r3 + 0x0620]
  lwz       r4, [r4 + 0x20]  # r5 = enemy->movement_data_0F->iparam3
  lwz       r3, [r3 + 0x0700]
  li        r5, 1
  b         get_enemy_hp_values_end
get_enemy_hp_values_barba_ray_mask_broken:
  # If the mask is broken, show Barba Ray's true HP
  lwz       r4, [r3 + 0x06F8]  # body_max_hp
  lwz       r3, [r3 + 0x06FC]  # body_current_hp
  li        r5, 0
  b         get_enemy_hp_values_end

get_enemy_hp_values_not_barba_ray:
  # If the target is neither De Rol Le nor Barba Ray, then it uses the normal HP system; show those values
  lha       r4, [r3 + 0x02B8]  # max_hp
  lha       r3, [r3 + 0x032C]  # current_hp
  li        r5, 0

get_enemy_hp_values_end:
  cmpwi     r3, 0
  bge       get_enemy_hp_values_current_hp_nonnegative
  li        r3, 0
get_enemy_hp_values_current_hp_nonnegative:
  lwz       r0, [r1 + 0x14]
  addi      r1, r1, 0x10
  mtlr      r0
  blr

update_enemy_hp_text:  # [std](TObjectV8047c128* enemy @ r3) -> char* text @ r3, int32_t current_hp @ r4, int32_t max_hp @ r5
  mflr      r0
  stwu      [r1 - 0x20], r1
  stw       [r1 + 0x24], r0
  stw       [r1 + 0x08], r31
  stw       [r1 + 0x0C], r30
  mr        r31, r3

  lwz       r3, [r31 + 0x0370]  # r3 = enemy->rt_index
  bl        enemy_name_for_rt_index
  mr        r30, r3

  mr        r3, r31
  bl        get_enemy_hp_values
  stw       [r1 + 0x14], r3
  stw       [r1 + 0x18], r4
  bl        get_shell_str_ret
shell_str:
  .binary   " shell"0000
get_shell_str_ret:
  mflr      r6  # r6 = shell str
  cmpwi     r5, 0
  bne       keep_shell_str
  addi      r6, r6, 6
keep_shell_str:
  mr        r5, r30
  mr        r7, r3
  mr        r8, r4

  bl        get_hp_format_str_ret
hp_format_str:
  .binary   "%s%s\n\nHP: %d/%d"00
get_hp_format_str_ret:
  mflr      r4  # r4 = hp format str
  b         get_hp_str_buffer
get_hp_str_buffer_ret:
  mflr      r3  # r3 = dest buffer
  stw       [r1 + 0x10], r3
  crxor     crb6, crb6, crb6
  bl        sprintf  # sprintf(r3=dest_buffer, r4=hp_format_str, r5=enemy_name, r6=shell_str, r7=current_hp, r8=max_hp)

  lwz       r3, [r1 + 0x10]  # text pointer
  lwz       r4, [r1 + 0x14]  # current hp
  lwz       r5, [r1 + 0x18]  # max hp
  lwz       r30, [r1 + 0x0C]
  lwz       r31, [r1 + 0x08]
  lwz       r0, [r1 + 0x24]
  addi      r1, r1, 0x20
  mtlr      r0
  blr

hook4_get_max_hp:  # 3OE1:80261B50; [std](TObjectV8047c128* enemy @ r3) -> int32_t max_hp @ r3
  mflr      r0
  stwu      [r1 - 0x20], r1
  stw       [r1 + 0x24], r0
  bl        update_enemy_hp_text
  mr        r3, r5
  lwz       r0, [r1 + 0x24]
  addi      r1, r1, 0x20
  mtlr      r0
  blr

hook5_get_current_hp:  # 3OE1:80261B84; [r3/](TObjectV8047c128* enemy @ r31) -> int32_t current_hp @ r0
  mflr      r0
  stwu      [r1 - 0x20], r1
  stw       [r1 + 0x24], r0
  stw       [r1 + 0x08], r3
  mr        r3, r31
  bl        update_enemy_hp_text
  mr        r0, r4
  lwz       r3, [r1 + 0x08]
  lwz       r4, [r1 + 0x24]
  addi      r1, r1, 0x20
  mtlr      r4
  blr

hook6_update_window_text:  # 3OE1:80261CF8; [std](const char* enemy_name @ r3, TObjectV8047c128* enemy @ r30) -> char* text @ r3
  mflr      r0
  stwu      [r1 - 0x20], r1
  stw       [r1 + 0x24], r0
  mr        r3, r30
  bl        update_enemy_hp_text
  mr        r4, r3
  lwz       r0, [r1 + 0x24]
  addi      r1, r1, 0x20
  mtlr      r0
  blr

get_hp_str_buffer:
  bl        get_hp_str_buffer_ret
hooks_end:

  .label    hook4_get_max_hp_call, <VERS 0x80261278 0x80261E50 0x80262E98 0x80262C4C 0x80261B50 0x80261B50 0x80262F10 0x80262758>
  .data     hook4_get_max_hp_call
  .data     0x00000004
  .address  hook4_get_max_hp_call
  bl        hook4_get_max_hp

  .label    hook5_get_current_hp_call, <VERS 0x802612AC 0x80261E84 0x80262ECC 0x80262C80 0x80261B84 0x80261B84 0x80262F44 0x8026278C>
  .data     hook5_get_current_hp_call
  .data     0x00000004
  .address  hook5_get_current_hp_call
  bl        hook5_get_current_hp

  .label    flag_clear_call, <VERS 0x802612C4 0x80261E9C 0x80262EE4 0x80262C98 0x80261B9C 0x80261B9C 0x80262F5C 0x802627A4>
  .data     flag_clear_call
  .data     0x00000004
  .address  flag_clear_call
  bl        <VERS 0x80242804 0x802431E4 0x80243548 0x80243ED8 0x802430E0 0x802430E0 0x8024420C 0x80243A54>

  .label    hook6_update_window_text_call, <VERS 0x80261420 0x80261FF8 0x80263040 0x80262DF4 0x80261CF8 0x80261CF8 0x802630B8 0x80262900>
  .data     hook6_update_window_text_call
  .data     0x00000004
  .address  hook6_update_window_text_call
  bl        hook6_update_window_text

  .data     <VERS 0x804CAE40 0x804CE590 0x804D0AE0 0x804D0880 0x804CB610 0x804CBAF0 0x804D0158 0x804D0548>
  .data     0x00000004
  .data     encode_float(75)

  .data     <VERS 0x804CAE70 0x804CE5C0 0x804D0B10 0x804D08B0 0x804CB640 0x804CBB20 0x804D0188 0x804D0578>
  .data     0x00000004
  .data     encode_float(75)

  .data     <VERS 0x804CAEA0 0x804CE5F0 0x804D0B40 0x804D08E0 0x804CB670 0x804CBB50 0x804D01B8 0x804D05A8>
  .data     0x00000004
  .data     encode_float(75)

  .data     <VERS 0x804CAED0 0x804CE620 0x804D0B70 0x804D0910 0x804CB6A0 0x804CBB80 0x804D01E8 0x804D05D8>
  .data     0x00000004
  .data     encode_float(75)

  .data     <VERS 0x804CAF00 0x804CE650 0x804D0BA0 0x804D0940 0x804CB6D0 0x804CBBB0 0x804D0218 0x804D0608>
  .data     0x00000004
  .data     encode_float(62)

  .data     <VERS 0x804CAF1C 0x804CE66C 0x804D0BBC 0x804D095C 0x804CB6EC 0x804CBBCC 0x804D0234 0x804D0624>
  .data     0x00000004
  .data     0xFF00FF15

  .data     <VERS 0x805CBFBC 0x805D65BC 0x805DDA5C 0x805DD7FC 0x805CC8C4 0x805D38E4 0x805DD104 0x805D9344>
  .data     0x00000004
  .data     encode_float(96)

  .data     0x00000000
  .data     0x00000000
