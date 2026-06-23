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
  .offsetof start
start:
  .include  WriteCodeBlocks

  .label    hook_addr, 0x8000BF5C
  .label    sprintf, <VERS 0x80395EFC 0x80398904 0x8039A7A4 0x8039A554 0x803971CC 0x80397224 0x8039A924 0x80399414>
  .label    enemy_name_for_rt_index, <VERS 0x801326FC 0x80132960 0x80132AA0 0x80132A00 0x801329A0 0x801329A0 0x80132A10 0x80132BE8>
  .label    TBoss2Joint_get_shell_max_hp, <VERS 0x80034AF8 0x80034B28 0x80034BC4 0x80034AD8 0x80034D18 0x80034D18 0x80034CE0 0x80034D58>
  .label    TBoss7Joint_get_shell_max_hp, <VERS 0x802EB164 0x802EC13C 0x802ED628 0x802ED340 0x802EBAC8 0x802EBB0C 0x802ED4FC 0x802EC8D4>

  .data     hook_addr
  .deltaof  hooks_start, hooks_end
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

get_boss_hp_values:  # [/r0,r3,r4,r5,r6,r7](TObjectV8047c128* enemy @ r3) -> current_hp @ r3, max_hp @ r4, is_masked @ r5
  mflr      r0
  stwu      [r1 - 0x10], r1
  stw       [r1 + 0x14], r0

  lwz       r4, [r3]  # r4 = type name pointer
  bl        get_boss_hp_values_TBoss2Joint_string
  .binary   "TBoss2Joint"00
get_boss_hp_values_TBoss2Joint_string:
  mflr      r5
  bl        strcmp_r4_r5_cr0
  bne       get_boss_hp_values_not_de_rol_le_joint
  lwz       r0, [r3 + 0x038C]
  rlwinm.   r0, r0, 0, 31, 31  # flags & 1 => shell is broken
  bne       get_boss_hp_values_de_rol_le_shell_broken
  mr        r5, r3
  bl        TBoss2Joint_get_shell_max_hp  # [/r3]() -> int32_t max_hp @ r3
  mr        r4, r3
  lwz       r3, [r5 + 0x0394]
  li        r5, 1
  b         get_boss_hp_values_end
get_boss_hp_values_de_rol_le_shell_broken:
  lwz       r3, [r3 + 0x10]  # enemy = enemy->parent
  b         get_boss_hp_values_from_de_rol_le

get_boss_hp_values_not_de_rol_le_joint:
  lwz       r4, [r3]  # r4 = type name pointer
  bl        get_boss_hp_values_TBoss2DeRolLe_string
  .binary   "TBoss2DeRolLe"000000
get_boss_hp_values_TBoss2DeRolLe_string:
  mflr      r5
  bl        strcmp_r4_r5_cr0
  bne       get_boss_hp_values_not_de_rol_le
get_boss_hp_values_from_de_rol_le:
  lwz       r4, [r3 + 0x06A8]  # body_max_hp
  lwz       r3, [r3 + 0x06AC]  # body_current_hp
  li        r5, 0
  b         get_boss_hp_values_end

get_boss_hp_values_not_de_rol_le:
  lwz       r4, [r3]  # r4 = type name pointer
  bl        get_boss_hp_values_TBoss7Joint_string
  .binary   "TBoss7Joint"00
get_boss_hp_values_TBoss7Joint_string:
  mflr      r5
  bl        strcmp_r4_r5_cr0
  bne       get_boss_hp_values_not_barba_ray_joint
  lwz       r0, [r3 + 0x03BC]
  rlwinm.   r0, r0, 0, 31, 31  # flags & 1 => shell is broken
  bne       get_boss_hp_values_barba_ray_shell_broken
  mr        r5, r3
  bl        TBoss7Joint_get_shell_max_hp  # [/r3]() -> int32_t max_hp @ r3
  mr        r4, r3
  lwz       r3, [r5 + 0x03C4]
  li        r5, 1
  b         get_boss_hp_values_end
get_boss_hp_values_barba_ray_shell_broken:
  lwz       r3, [r3 + 0x10]  # enemy = enemy->parent
  b         get_boss_hp_values_from_barba_ray

get_boss_hp_values_not_barba_ray_joint:
  lwz       r4, [r3]  # r4 = type name pointer
  bl        get_boss_hp_values_TBoss7DeRolLeC_string
  .binary   "TBoss7DeRolLeC"0000
get_boss_hp_values_TBoss7DeRolLeC_string:
  mflr      r5
  bl        strcmp_r4_r5_cr0
  bne       get_boss_hp_values_not_barba_ray
get_boss_hp_values_from_barba_ray:
  lwz       r4, [r3 + 0x06F8]  # body_max_hp
  lwz       r3, [r3 + 0x06FC]  # body_current_hp
  li        r5, 0
  b         get_boss_hp_values_end

get_boss_hp_values_not_barba_ray:
  lha       r4, [r3 + 0x02B8]  # max_hp
  lha       r3, [r3 + 0x032C]  # current_hp
  li        r5, 0

get_boss_hp_values_end:
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
  bl        get_boss_hp_values
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
  .float    75

  .data     <VERS 0x804CAE4C 0x804CE59C 0x804D0AEC 0x804D088C 0x804CB61C 0x804CBAFC 0x804D0164 0x804D0554>
  .data     0x00000004
  .float    75

  .data     <VERS 0x804CAE58 0x804CE5A8 0x804D0AF8 0x804D0898 0x804CB628 0x804CBB08 0x804D0170 0x804D0560>
  .data     0x00000004
  .float    75

  .data     <VERS 0x804CAE64 0x804CE5B4 0x804D0B04 0x804D08A4 0x804CB634 0x804CBB14 0x804D017C 0x804D056C>
  .data     0x00000004
  .float    75

  .data     <VERS 0x804CAE70 0x804CE5C0 0x804D0B10 0x804D08B0 0x804CB640 0x804CBB20 0x804D0188 0x804D0578>
  .data     0x00000004
  .float    75

  .data     <VERS 0x804CAEA0 0x804CE5F0 0x804D0B40 0x804D08E0 0x804CB670 0x804CBB50 0x804D01B8 0x804D05A8>
  .data     0x00000004
  .float    75

  .data     <VERS 0x804CAED0 0x804CE620 0x804D0B70 0x804D0910 0x804CB6A0 0x804CBB80 0x804D01E8 0x804D05D8>
  .data     0x00000004
  .float    75

  .data     <VERS 0x804CAF00 0x804CE650 0x804D0BA0 0x804D0940 0x804CB6D0 0x804CBBB0 0x804D0218 0x804D0608>
  .data     0x00000004
  .float    62

  .data     <VERS 0x804CAF1C 0x804CE66C 0x804D0BBC 0x804D095C 0x804CB6EC 0x804CBBCC 0x804D0234 0x804D0624>
  .data     0x00000004
  .data     0xFF00FF15

  .data     <VERS 0x805CBFBC 0x805D65BC 0x805DDA5C 0x805DD7FC 0x805CC8C4 0x805D38E4 0x805DD104 0x805D9344>
  .data     0x00000004
  .float    96

  .data     0x00000000
  .data     0x00000000
