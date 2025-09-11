.meta hide_from_patches_menu
.meta name="DMC"
.meta description="Mitigates effects\nof enemy health\ndesync"
.meta client_flag="0x2000000000000000"

.versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC

  .label    TObjectV8047c128_add_hp, <VERS 0x8001143C 0x8001145C 0x800116EC 0x8001140C 0x80011454 0x80011454 0x8001140C 0x80011484>
  .label    TObjectV8047c128_subtract_hp, <VERS 0x8001147C 0x8001149C 0x8001172C 0x8001144C 0x80011494 0x80011494 0x8001144C 0x800114C4>
  .label    get_enemy_entity, <VERS 0x800F6608 0x800F690C 0x800F6C00 0x800F6A34 0x800F68AC 0x800F68AC 0x800F6A44 0x800F6A0C>
  .label    send_60, <VERS 0x801DBBA8 0x801DC0D4 0x801DD314 0x801DC0D0 0x801DBFE0 0x801DBFE0 0x801DC134 0x801DC6C0>
  .label    send_and_handle_60, <VERS 0x801E404C 0x801E45D0 0x801E47A4 0x801E451C 0x801E44B0 0x801E44B0 0x801E4570 0x801E4BAC>



  .data     <VERS 0x801E40C4 0x801E4648 0x801E481C 0x801E4594 0x801E4528 0x801E4528 0x801E45E8 0x801E4C24>
  .data     8
  cmpwi     r0, 0
  beq       +0x0C



  # Don't allow 6x0A to set total_damage; we'll do it with 6xE4 instead
  .data     <VERS 0x800F6064 0x800F6368 0x800F6594 0x800F6490 0x800F6308 0x800F6308 0x800F64A0 0x800F6468>
  .data     4
  .address  <VERS 0x800F6064 0x800F6368 0x800F6594 0x800F6490 0x800F6308 0x800F6308 0x800F64A0 0x800F6468>
  bl        set_enemy_total_damage_hook



  # Enemy state setup debug hook
  .data     <VERS 0x800F60BC 0x800F63C0 0x800F65EC 0x800F64E8 0x800F6360 0x800F6360 0x800F64F8 0x800F64C0>
  .data     4
  .address  <VERS 0x800F60BC 0x800F63C0 0x800F65EC 0x800F64E8 0x800F6360 0x800F6360 0x800F64F8 0x800F64C0>
  bl        debug_hook1



  # Replace 6x09 with 6xE4 in subcommand handler table
  .data     <VERS 0x804C00C4 0x804C37FC 0x804C5C9C 0x804C5A3C 0x804C0894 0x804C0D74 0x804C5314 0x804C57B4>
  .data     8
  .data     0x00E40006  # subcommand=0xE4, flags=6
  .data     0x800041C0  # handle_6xE4



  # add_hp callsite in TObjectV8047c128_v17_accept_hit
  .data     <VERS 0x80013A78 0x80013AA8 0x80013E18 0x80013A58 0x80013A90 0x80013A90 0x80013A58 0x80013AD0>
  .data     4
  .address  <VERS 0x80013A78 0x80013AA8 0x80013E18 0x80013A58 0x80013A90 0x80013A90 0x80013A58 0x80013AD0>
  bl        on_TObjectV8047c128_add_hp_with_sync



  # subtract_hp callsites in TObjectV8047c128_subtract_hp_if_not_in_state_2
  .data     <VERS 0x800114F0 0x80011510 0x800117A0 0x800114C0 0x80011508 0x80011508 0x800114C0 0x80011538>
  .data     4
  .address  <VERS 0x800114F0 0x80011510 0x800117A0 0x800114C0 0x80011508 0x80011508 0x800114C0 0x80011538>
  bl        on_TObjectV8047c128_subtract_hp_with_sync
  # subtract_hp callsites in TObjectV8047c128_v18_handle_hit_special_effects
  .data     <VERS 0x80011C50 0x80011C80 0x80011F48 0x80011C30 0x80011C68 0x80011C68 0x80011C30 0x80011CA8>
  .data     4
  .address  <VERS 0x80011C50 0x80011C80 0x80011F48 0x80011C30 0x80011C68 0x80011C68 0x80011C30 0x80011CA8>
  bl        on_TObjectV8047c128_subtract_hp_with_sync
  .data     <VERS 0x80011CA0 0x80011CD0 0x80011F98 0x80011C80 0x80011CB8 0x80011CB8 0x80011C80 0x80011CF8>
  .data     4
  .address  <VERS 0x80011CA0 0x80011CD0 0x80011F98 0x80011C80 0x80011CB8 0x80011CB8 0x80011C80 0x80011CF8>
  bl        on_TObjectV8047c128_subtract_hp_with_sync
  .data     <VERS 0x80011D1C 0x80011D4C 0x80012014 0x80011CFC 0x80011D34 0x80011D34 0x80011CFC 0x80011D74>
  .data     4
  .address  <VERS 0x80011D1C 0x80011D4C 0x80012014 0x80011CFC 0x80011D34 0x80011D34 0x80011CFC 0x80011D74>
  bl        on_TObjectV8047c128_subtract_hp_with_sync
  .data     <VERS 0x80011D6C 0x80011D9C 0x80012064 0x80011D4C 0x80011D84 0x80011D84 0x80011D4C 0x80011DC4>
  .data     4
  .address  <VERS 0x80011D6C 0x80011D9C 0x80012064 0x80011D4C 0x80011D84 0x80011D84 0x80011D4C 0x80011DC4>
  bl        on_TObjectV8047c128_subtract_hp_with_sync
  .data     <VERS 0x80012774 0x800127A4 0x80012A6C 0x80012754 0x8001278C 0x8001278C 0x80012754 0x800127CC>
  .data     4
  .address  <VERS 0x80012774 0x800127A4 0x80012A6C 0x80012754 0x8001278C 0x8001278C 0x80012754 0x800127CC>
  bl        on_TObjectV8047c128_subtract_hp_with_sync
  .data     <VERS 0x80012AE8 0x80012B18 0x80012DE0 0x80012AC8 0x80012B00 0x80012B00 0x80012AC8 0x80012B40>
  .data     4
  .address  <VERS 0x80012AE8 0x80012B18 0x80012DE0 0x80012AC8 0x80012B00 0x80012B00 0x80012AC8 0x80012B40>
  bl        on_TObjectV8047c128_subtract_hp_with_sync
  .data     <VERS 0x80012C58 0x80012C88 0x80012F50 0x80012C38 0x80012C70 0x80012C70 0x80012C38 0x80012CB0>
  .data     4
  .address  <VERS 0x80012C58 0x80012C88 0x80012F50 0x80012C38 0x80012C70 0x80012C70 0x80012C38 0x80012CB0>
  bl        on_TObjectV8047c128_subtract_hp_with_sync
  .data     <VERS 0x8001300C 0x8001303C 0x80013304 0x80012FEC 0x80013024 0x80013024 0x80012FEC 0x80013064>
  .data     4
  .address  <VERS 0x8001300C 0x8001303C 0x80013304 0x80012FEC 0x80013024 0x80013024 0x80012FEC 0x80013064>
  bl        on_TObjectV8047c128_subtract_hp_with_sync
  # subtract_hp callsites in TObjectV8047c128_v17_accept_hit
  .data     <VERS 0x80013454 0x80013484 0x800137F4 0x80013434 0x8001346C 0x8001346C 0x80013434 0x800134AC>
  .data     4
  .address  <VERS 0x80013454 0x80013484 0x800137F4 0x80013434 0x8001346C 0x8001346C 0x80013434 0x800134AC>
  bl        on_TObjectV8047c128_subtract_hp_with_sync
  .data     <VERS 0x8001354C 0x8001357C 0x800138EC 0x8001352C 0x80013564 0x80013564 0x8001352C 0x800135A4>
  .data     4
  .address  <VERS 0x8001354C 0x8001357C 0x800138EC 0x8001352C 0x80013564 0x80013564 0x8001352C 0x800135A4>
  bl        on_TObjectV8047c128_subtract_hp_with_sync
  .data     <VERS 0x80013644 0x80013674 0x800139E4 0x80013624 0x8001365C 0x8001365C 0x80013624 0x8001369C>
  .data     4
  .address  <VERS 0x80013644 0x80013674 0x800139E4 0x80013624 0x8001365C 0x8001365C 0x80013624 0x8001369C>
  bl        on_TObjectV8047c128_subtract_hp_with_sync
  .data     <VERS 0x800137AC 0x800137DC 0x80013B4C 0x8001378C 0x800137C4 0x800137C4 0x8001378C 0x80013804>
  .data     4
  .address  <VERS 0x800137AC 0x800137DC 0x80013B4C 0x8001378C 0x800137C4 0x800137C4 0x8001378C 0x80013804>
  bl        on_TObjectV8047c128_subtract_hp_with_sync
  .data     <VERS 0x800138EC 0x8001391C 0x80013C8C 0x800138CC 0x80013904 0x80013904 0x800138CC 0x80013944>
  .data     4
  .address  <VERS 0x800138EC 0x8001391C 0x80013C8C 0x800138CC 0x80013904 0x80013904 0x800138CC 0x80013944>
  bl        on_TObjectV8047c128_subtract_hp_with_sync
  .data     <VERS 0x80013E00 0x80013E30 0x800141A0 0x80013DE0 0x80013E18 0x80013E18 0x80013DE0 0x80013E58>
  .data     4
  .address  <VERS 0x80013E00 0x80013E30 0x800141A0 0x80013DE0 0x80013E18 0x80013E18 0x80013DE0 0x80013E58>
  bl        on_TObjectV8047c128_subtract_hp_with_sync
  .data     <VERS 0x80013F04 0x80013F34 0x800142A4 0x80013EE4 0x80013F1C 0x80013F1C 0x80013EE4 0x80013F5C>
  .data     4
  .address  <VERS 0x80013F04 0x80013F34 0x800142A4 0x80013EE4 0x80013F1C 0x80013F1C 0x80013EE4 0x80013F5C>
  bl        on_TObjectV8047c128_subtract_hp_with_sync
  # subtract_hp callsites in TObjectV8047c128_v16
  .data     <VERS 0x80014770 0x800147A0 0x80014B38 0x80014750 0x80014788 0x80014788 0x80014750 0x800147C8>
  .data     4
  .address  <VERS 0x80014770 0x800147A0 0x80014B38 0x80014750 0x80014788 0x80014788 0x80014750 0x800147C8>
  bl        on_TObjectV8047c128_subtract_hp_with_sync



  .data     0x800041C0
  .deltaof  code_start, code_end
  .address  0x800041C0
code_start:
handle_6xE4:  # [std] (G_IncrementEnemyDamage_Extension_6xE4* cmd @ r3) -> void
  lwz       r12, [r13 - <VERS 0x50A0 0x5098 0x5078 0x5078 0x5088 0x5088 0x5068 0x5028>]
  andi.     r12, r12, 0x0080
  beqlr

  mflr      r0
  stw       [r1 + 4], r0
  stwu      [r1 - 0x20], r1
  stw       [r1 + 0x08], r31
  stw       [r1 + 0x0C], r30
  mr        r31, r3

  li        r3, 2
  lhbrx     r3, [r31 + r3]
  cmplwi    r3, 0x1000
  blt       handle_6xE4_return
  cmplwi    r3, 0x1B50
  bge       handle_6xE4_return

  bl        get_enemy_entity
  stw       [r1 + 0x18], r3  # TObjEnemy* ene @ var18 = get_enemy_entity(cmd->header.entity_id);

  li        r3, 2
  lhbrx     r3, [r31 + r3]
  bl        state_for_enemy  # EnemyState* st = state_for_enemy(cmd->header.entity_id);

  lhz       r4, [r3 + 6]  # st->total_damage
  li        r5, 0x04
  lhbrx     r5, [r31 + r5]  # cmd->hit_amount
  add       r4, r4, r5  # st->total_damage + cmd->hit_amount
  li        r5, 0x0A
  lhbrx     r5, [r31 + r5]  # cmd->max_hp
  cmp       r4, r5
  blt       handle_6xE4_damage_less_than_max_hp

  sth       [r3 + 6], r5  # st->total_damage = cmd->max_hp;
  li        r4, 0x0C
  bl        send_debug_info  # TODO: Remove this when no longer necessary
  lwz       r4, [r3]
  andi.     r0, r4, 0x800
  bne       handle_6xE4_return
  ori       r4, r4, 0x800
  stw       [r3], r4  # st->game_flags |= 0x800;

  # Send 6x0A with dead flag, but only if the entity is constructed
  lwz       r6, [r1 + 0x18]
  cmplwi    r6, 0
  beq       handle_6xE4_return
  stw       [r1 + 0x14], r4
  li        r6, 0x12
  sthbrx    [r1 + r6], r5
  lhz       r6, [r31 + 2]
  oris      r6, r6, 0x0A03
  stw       [r1 + 0x0C], r6
  andi.     r6, r6, 0xFF0F
  sth       [r1 + 0x10], r6
  addi      r3, r1, 0x0C
  bl        send_and_handle_60
  b         handle_6xE4_return

handle_6xE4_damage_less_than_max_hp:
  cmpwi     r4, 0
  bge       handle_6xE4_damage_nonnegative
  li        r4, 0
handle_6xE4_damage_nonnegative:
  sth       [r3 + 6], r4  # st->total_damage = std::max<int16_t>(st->total_damage + cmd->hit_amount, 0);
  li        r4, 0x0C
  mr        r30, r3
  bl        send_debug_info

  lwz       r3, [r1 + 0x18]  # if (ene) ene->v50_on_state_updated(&st);
  cmplwi    r3, 0
  beq       handle_6xE4_return
  mr        r4, r30
  lwz       r12, [r3 + 0x18]
  lwz       r12, [r12 + 0x140]
  mtctr     r12
  bctrl

handle_6xE4_return:
  lwz       r30, [r1 + 0x0C]
  lwz       r31, [r1 + 0x08]
  addi      r1, r1, 0x20
  lwz       r0, [r1 + 4]
  mtlr      r0
  blr



state_for_enemy:  # [/r4] (uint16_t entity_id @ r3) -> EnemyState* @ r3
  # return &enemy_states[entity_id & 0x0FFF];
  lwz       r4, [r13 - <VERS 0x44F4 0x44EC 0x44CC 0x44CC 0x44DC 0x44DC 0x44BC 0x447C>]
  andi.     r3, r3, 0x0FFF
  mulli     r3, r3, 0x0C
  add       r3, r3, r4
  blr



on_TObjectV8047c128_add_hp_with_sync:  # [std] (TObjectV8047c128* ene @ r3, int16_t amount @ r4) -> void
  li        r5, 1
  b         on_add_or_subtract_hp
on_TObjectV8047c128_subtract_hp_with_sync:  # [std] (TObjectV8047c128* ene @ r3, int16_t amount @ r4) -> void
  li        r5, 0
on_add_or_subtract_hp:  # [std] (TObjectV8047c128* ene @ r3, int16_t amount @ r4, bool is_add @ r5) -> void

  lwz       r12, [r13 - <VERS 0x50A0 0x5098 0x5078 0x5078 0x5088 0x5088 0x5068 0x5028>]
  andi.     r12, r12, 0x0080
  beq       on_add_or_subtract_hp_skip_send

  lhz       r0, [r3 + 0x1C]
  cmplwi    r0, 0x1000
  blt       on_add_or_subtract_hp_skip_send
  cmplwi    r0, 0x1B50
  bge       on_add_or_subtract_hp_skip_send

  lwz       r11, [r13 - <VERS 0x50B8 0x50B0 0x5090 0x5090 0x50A0 0x50A0 0x5080 0x5040>]
  cmplwi    r11, 0
  beq       on_add_or_subtract_hp_skip_send

  mflr      r0
  stw       [r1 + 4], r0
  stwu      [r1 - 0x20], r1
  stw       [r1 + 0x14], r29
  stw       [r1 + 0x18], r30
  stw       [r1 + 0x1C], r31
  mr        r29, r3
  mr        r30, r4
  mr        r31, r5

  lhz       r3, [r29 + 0x1C]
  bl        state_for_enemy  # EnemyState* st = state_for_enemy(ene->entity_id);

  mr        r5, r30
  cmplwi    r31, 0
  beq       on_add_or_subtract_hp_skip_negate_amount
  neg       r5, r5
on_add_or_subtract_hp_skip_negate_amount:

  li        r4, 0x1C
  lhbrx     r4, [r29 + r4]
  oris      r4, r4, 0xE403
  stw       [r1 + 0x08], r4
  li        r4, 0x0C
  sthbrx    [r1 + r4], r5
  li        r4, 6
  lhbrx     r4, [r3 + r4]
  sth       [r1 + 0x0E], r4
  li        r4, 0x32C
  lhbrx     r4, [r29 + r4]
  sth       [r1 + 0x10], r4
  li        r4, 0x2B8
  lhbrx     r4, [r29 + r4]
  sth       [r1 + 0x12], r4
  mr        r3, r11
  addi      r4, r1, 0x08
  li        r5, 0x0C
  bl        send_60

on_add_or_subtract_hp_tail_call:
  mr        r3, r29
  mr        r4, r30
  mr        r5, r31
  lwz       r31, [r1 + 0x1C]
  lwz       r30, [r1 + 0x18]
  lwz       r29, [r1 + 0x14]
  addi      r1, r1, 0x20
  lwz       r0, [r1 + 4]
  mtlr      r0

on_add_or_subtract_hp_skip_send:
  cmplwi    r5, 0
  beq       on_add_or_subtract_hp_tail_call_subtract_hp
  b         TObjectV8047c128_add_hp
on_add_or_subtract_hp_tail_call_subtract_hp:
  b         TObjectV8047c128_subtract_hp



set_enemy_total_damage_hook:
  lwz       r12, [r13 - <VERS 0x50A0 0x5098 0x5078 0x5078 0x5088 0x5088 0x5068 0x5028>]
  andi.     r12, r12, 0x0080
  bnelr
  sth       [r1 + 0x0E], r3
  blr



# TODO: Remove this when no longer necessary
debug_hook1:
  mflr      r0
  stw       [r1 + 4], r0
  stwu      [r1 - 0x20], r1
  mr        r6, r3
  mr        r7, r4
  mr        r3, r4
  li        r4, 0x0C
  li        r5, -1
  bl        send_debug_info
  mr        r3, r6
  mr        r4, r7
  addi      r1, r1, 0x20
  lwz       r0, [r1 + 4]
  mtlr      r0
  mtctr     r12
  bctr



send_debug_info:  # (void* data @ r3, uint32_t size @ r4, uint16_t what @ r5) -> void
  mflr      r0
  stw       [r1 + 0x04], r0
  stw       [r1 - 0x04], r3
  stw       [r1 - 0x08], r4
  stw       [r1 - 0x0C], r5
  stw       [r1 - 0x10], r6
  stw       [r1 - 0x14], r7
  stw       [r1 - 0x18], r8
  stw       [r1 - 0x1C], r9
  stw       [r1 - 0x20], r10
  stw       [r1 - 0x24], r11
  stw       [r1 - 0x28], r12
  subi      r6, r1, 0x40
  sub       r6, r6, r4
  stw       [r6], r1
  mr        r1, r6

  rlwinm    r6, r4, 14, 8, 15
  addis     r6, r6, 1
  oris      r6, r6, 0xFF00
  rlwinm    r5, r5, 0, 16, 31
  or        r5, r5, r6
  stw       [r1 + 0x08], r5
  li        r6, 0
  subi      r3, r3, 4
  addi      r7, r1, 0x08
  rlwinm    r0, r4, 30, 24, 31
  mtctr     r0
copy_again:
  lwzu      r0, [r3 + 4]
  stwu      [r7 + 4], r0
  bdnz      copy_again

  addi      r3, r1, 0x08
  bl        send_and_handle_60

  lwz       r1, [r1]
  lwz       r3, [r1 - 0x04]
  lwz       r4, [r1 - 0x08]
  lwz       r5, [r1 - 0x0C]
  lwz       r6, [r1 - 0x10]
  lwz       r7, [r1 - 0x14]
  lwz       r8, [r1 - 0x18]
  lwz       r9, [r1 - 0x1C]
  lwz       r10, [r1 - 0x20]
  lwz       r11, [r1 - 0x24]
  lwz       r12, [r1 - 0x28]
  lwz       r0, [r1 + 0x04]
  mtlr      r0
  blr



code_end:

  .data   0x00000000
  .data   0x00000000
