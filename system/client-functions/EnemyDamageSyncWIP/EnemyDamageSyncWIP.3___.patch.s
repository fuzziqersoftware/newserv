.meta hide_from_patches_menu
.meta name="DMC"
.meta description="Mitigates effects\nof enemy health\ndesync"

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



  # Change class_flags check to read only low 16 bits
  .data     <VERS 0x800142D8 0x80014308 0x800146A0 0x800142B8 0x800142F0 0x800142F0 0x800142B8 0x80014330>
  .data     4
  lhz       r0, [r28 + 0x2E6]



  # Replace 6x09 with 6xE4 in subcommand handler table
  .data     <VERS 0x804C00C4 0x804C37FC 0x804C5C9C 0x804C5A3C 0x804C0894 0x804C0D74 0x804C5314 0x804C57B4>
  .data     8
  .data     0x00E40006  # subcommand=0xE4, flags=6
  .data     0x800041C0  # on_6xE4



  # Hooks in 6x0A handler
  .data     <VERS 0x800F605C 0x800F6360 0x800F658C 0x800F6488 0x800F6300 0x800F6300 0x800F6498 0x800F6460>
  .data     4
  .address  <VERS 0x800F605C 0x800F6360 0x800F658C 0x800F6488 0x800F6300 0x800F6300 0x800F6498 0x800F6460>
  bl        on_handle_6x0A_set_total_damage
  .data     <VERS 0x800F60BC 0x800F63C0 0x800F65EC 0x800F64E8 0x800F6360 0x800F6360 0x800F64F8 0x800F64C0>
  .data     4
  .address  <VERS 0x800F60BC 0x800F63C0 0x800F65EC 0x800F64E8 0x800F6360 0x800F6360 0x800F64F8 0x800F64C0>
  bl        on_handle_6x0A_call_object_update_handler



  # add_hp callsite in TObjectV8047c128_v17_accept_hit
  .data     <VERS 0x80013A78 0x80013AA8 0x80013E18 0x80013A58 0x80013A90 0x80013A90 0x80013A58 0x80013AD0>
  .data     4
  .address  <VERS 0x80013A78 0x80013AA8 0x80013E18 0x80013A58 0x80013A90 0x80013A90 0x80013A58 0x80013AD0>
  bl        on_TObjectV8047c128_add_hp



  # subtract_hp callsites in TObjectV8047c128_subtract_hp_if_in_state_2
  .data     <VERS 0x800114F0 0x80011510 0x800117A0 0x800114C0 0x80011508 0x80011508 0x800114C0 0x80011538>
  .data     4
  .address  <VERS 0x800114F0 0x80011510 0x800117A0 0x800114C0 0x80011508 0x80011508 0x800114C0 0x80011538>
  bl        on_TObjectV8047c128_subtract_hp
  # subtract_hp callsites in TObjectV8047c128_v18_handle_hit_special_effects
  .data     <VERS 0x80011C50 0x80011C80 0x80011F48 0x80011C30 0x80011C68 0x80011C68 0x80011C30 0x80011CA8>
  .data     4
  .address  <VERS 0x80011C50 0x80011C80 0x80011F48 0x80011C30 0x80011C68 0x80011C68 0x80011C30 0x80011CA8>
  bl        on_TObjectV8047c128_subtract_hp
  .data     <VERS 0x80011CA0 0x80011CD0 0x80011F98 0x80011C80 0x80011CB8 0x80011CB8 0x80011C80 0x80011CF8>
  .data     4
  .address  <VERS 0x80011CA0 0x80011CD0 0x80011F98 0x80011C80 0x80011CB8 0x80011CB8 0x80011C80 0x80011CF8>
  bl        on_TObjectV8047c128_subtract_hp
  .data     <VERS 0x80011D1C 0x80011D4C 0x80012014 0x80011CFC 0x80011D34 0x80011D34 0x80011CFC 0x80011D74>
  .data     4
  .address  <VERS 0x80011D1C 0x80011D4C 0x80012014 0x80011CFC 0x80011D34 0x80011D34 0x80011CFC 0x80011D74>
  bl        on_TObjectV8047c128_subtract_hp
  .data     <VERS 0x80011D6C 0x80011D9C 0x80012064 0x80011D4C 0x80011D84 0x80011D84 0x80011D4C 0x80011DC4>
  .data     4
  .address  <VERS 0x80011D6C 0x80011D9C 0x80012064 0x80011D4C 0x80011D84 0x80011D84 0x80011D4C 0x80011DC4>
  bl        on_TObjectV8047c128_subtract_hp
  .data     <VERS 0x80012774 0x800127A4 0x80012A6C 0x80012754 0x8001278C 0x8001278C 0x80012754 0x800127CC>
  .data     4
  .address  <VERS 0x80012774 0x800127A4 0x80012A6C 0x80012754 0x8001278C 0x8001278C 0x80012754 0x800127CC>
  bl        on_TObjectV8047c128_subtract_hp
  .data     <VERS 0x80012AE8 0x80012B18 0x80012DE0 0x80012AC8 0x80012B00 0x80012B00 0x80012AC8 0x80012B40>
  .data     4
  .address  <VERS 0x80012AE8 0x80012B18 0x80012DE0 0x80012AC8 0x80012B00 0x80012B00 0x80012AC8 0x80012B40>
  bl        on_TObjectV8047c128_subtract_hp
  .data     <VERS 0x80012C58 0x80012C88 0x80012F50 0x80012C38 0x80012C70 0x80012C70 0x80012C38 0x80012CB0>
  .data     4
  .address  <VERS 0x80012C58 0x80012C88 0x80012F50 0x80012C38 0x80012C70 0x80012C70 0x80012C38 0x80012CB0>
  bl        on_TObjectV8047c128_subtract_hp
  .data     <VERS 0x8001300C 0x8001303C 0x80013304 0x80012FEC 0x80013024 0x80013024 0x80012FEC 0x80013064>
  .data     4
  .address  <VERS 0x8001300C 0x8001303C 0x80013304 0x80012FEC 0x80013024 0x80013024 0x80012FEC 0x80013064>
  bl        on_TObjectV8047c128_subtract_hp
  # subtract_hp callsites in TObjectV8047c128_v17_accept_hit
  .data     <VERS 0x80013454 0x80013484 0x800137F4 0x80013434 0x8001346C 0x8001346C 0x80013434 0x800134AC>
  .data     4
  .address  <VERS 0x80013454 0x80013484 0x800137F4 0x80013434 0x8001346C 0x8001346C 0x80013434 0x800134AC>
  bl        on_TObjectV8047c128_subtract_hp
  .data     <VERS 0x8001354C 0x8001357C 0x800138EC 0x8001352C 0x80013564 0x80013564 0x8001352C 0x800135A4>
  .data     4
  .address  <VERS 0x8001354C 0x8001357C 0x800138EC 0x8001352C 0x80013564 0x80013564 0x8001352C 0x800135A4>
  bl        on_TObjectV8047c128_subtract_hp
  .data     <VERS 0x80013644 0x80013674 0x800139E4 0x80013624 0x8001365C 0x8001365C 0x80013624 0x8001369C>
  .data     4
  .address  <VERS 0x80013644 0x80013674 0x800139E4 0x80013624 0x8001365C 0x8001365C 0x80013624 0x8001369C>
  bl        on_TObjectV8047c128_subtract_hp
  .data     <VERS 0x800137AC 0x800137DC 0x80013B4C 0x8001378C 0x800137C4 0x800137C4 0x8001378C 0x80013804>
  .data     4
  .address  <VERS 0x800137AC 0x800137DC 0x80013B4C 0x8001378C 0x800137C4 0x800137C4 0x8001378C 0x80013804>
  bl        on_TObjectV8047c128_subtract_hp
  .data     <VERS 0x800138EC 0x8001391C 0x80013C8C 0x800138CC 0x80013904 0x80013904 0x800138CC 0x80013944>
  .data     4
  .address  <VERS 0x800138EC 0x8001391C 0x80013C8C 0x800138CC 0x80013904 0x80013904 0x800138CC 0x80013944>
  bl        on_TObjectV8047c128_subtract_hp
  .data     <VERS 0x80013E00 0x80013E30 0x800141A0 0x80013DE0 0x80013E18 0x80013E18 0x80013DE0 0x80013E58>
  .data     4
  .address  <VERS 0x80013E00 0x80013E30 0x800141A0 0x80013DE0 0x80013E18 0x80013E18 0x80013DE0 0x80013E58>
  bl        on_TObjectV8047c128_subtract_hp
  .data     <VERS 0x80013F04 0x80013F34 0x800142A4 0x80013EE4 0x80013F1C 0x80013F1C 0x80013EE4 0x80013F5C>
  .data     4
  .address  <VERS 0x80013F04 0x80013F34 0x800142A4 0x80013EE4 0x80013F1C 0x80013F1C 0x80013EE4 0x80013F5C>
  bl        on_TObjectV8047c128_subtract_hp
  # subtract_hp callsites in TObjectV8047c128_v16
  .data     <VERS 0x80014770 0x800147A0 0x80014B38 0x80014750 0x80014788 0x80014788 0x80014750 0x800147C8>
  .data     4
  .address  <VERS 0x80014770 0x800147A0 0x80014B38 0x80014750 0x80014788 0x80014788 0x80014750 0x800147C8>
  bl        on_TObjectV8047c128_subtract_hp



  # subtract_hp callsites in TObjectV8047c128_v23_give_poison_damage
  .data     <VERS 0x80017290 0x800172C0 0x80017808 0x80017270 0x800172A8 0x800172A8 0x80017270 0x800172E8>
  .data     4
  .address  <VERS 0x80017290 0x800172C0 0x80017808 0x80017270 0x800172A8 0x800172A8 0x80017270 0x800172E8>
  bl        on_TObjectV8047c128_subtract_hp_without_sync



  .data     0x800041C0
  .deltaof  code_start, code_end
  .address  0x800041C0
code_start:
on_6xE4:  # (G_6xE4* cmd @ r3) -> void
  mflr    r0
  stw     [r1 + 4], r0
  stwu    [r1 - 0x20], r1
  stw     [r1 + 8], r3

  li      r4, 2
  lhbrx   r3, [r3 + r4]
  bl      get_enemy_entity

  cmplwi  r3, 0
  beq     on_6xE4_skip

  lwz     r4, [r1 + 8]
  li      r5, 4
  lhbrx   r6, [r4 + r5]
  extsh   r6, r6
  lhz     r7, [r3 + 0x2E4]
  add     r6, r6, r7
  lhz     r7, [r3 + 0x2B8]
  cmp     r6, r7
  bgt     on_6xE4_use_r7
  li      r7, 0
  cmp     r6, r7
  blt     on_6xE4_use_r7
  sth     [r3 + 0x2E4], r6
  b       on_6xE4_skip
on_6xE4_use_r7:
  sth     [r3 + 0x2E4], r7

on_6xE4_skip:
  addi    r1, r1, 0x20
  lwz     r0, [r1 + 4]
  mtlr    r0
  blr



on_handle_6x0A_set_total_damage:  # (G_6x0A* cmd @ r30) -> int16_t @ r3
  # Nonstandard convention (patched callsite is not a call or return); must
  # save and restore r0
  mflr    r4
  stw     [r1 + 0x04], r4
  stwu    [r1 - 0x20], r1
  stw     [r1 + 0x08], r0

  lhz     r3, [r30 + 2]
  bl      get_enemy_entity

  cmplwi  r3, 0
  beq     on_handle_6x0A_set_total_damage_not_loaded

  lhz     r4, [r3 + 0x2E4]
  lhz     r5, [r3 + 0x2B8]
  lhz     r3, [r30 + 6]
  cmp     r3, r5
  bgt     on_handle_6x0A_set_total_damage_use_r5
  cmp     r3, r4
  blt     on_handle_6x0A_set_total_damage_use_r4
  b       on_handle_6x0A_set_total_damage_return
on_handle_6x0A_set_total_damage_use_r4:
  mr      r3, r4
  b       on_handle_6x0A_set_total_damage_return
on_handle_6x0A_set_total_damage_use_r5:
  mr      r3, r5
  b       on_handle_6x0A_set_total_damage_return

on_handle_6x0A_set_total_damage_not_loaded:
  lhz     r3, [r30 + 6]

on_handle_6x0A_set_total_damage_return:
  lwz     r0, [r1 + 0x08]
  addi    r1, r1, 0x20
  lwz     r4, [r1 + 4]
  mtlr    r4
  blr



on_handle_6x0A_call_object_update_handler:  # (TObjectV8047c128* this @ r3, EnemyState* ene_st @ r4, void (*vfn)(TObjectV8047c128* this @ r3, EnemyState* ene_st @ r4) @ r12) -> void
  mflr    r0
  stw     [r1 + 4], r0
  stwu    [r1 - 0x20], r1
  stw     [r1 + 0x08], r3
  stw     [r1 + 0x0C], r4
  stw     [r1 + 0x10], r12

  lwz     r5, [r3 + 0x30]
  lwz     r7, [r4]
  or      r5, r5, r7
  andi.   r5, r5, 0x0800
  bne     on_handle_6x0A_call_object_update_handler_return
  lhz     r5, [r4 + 6]
  lhz     r6, [r3 + 0x2B8]
  cmp     r5, r6
  blt     on_handle_6x0A_call_object_update_handler_return

  ori     r7, r7, 0x0800
  stw     [r4], r7

  lwz     r11, [r13 - <VERS 0x50B8 0x50B0 0x5090 0x5090 0x50A0 0x50A0 0x5080 0x5040>]
  cmplwi  r11, 0
  beq     on_handle_6x0A_call_object_update_handler_return

  addi    r10, r1, 0x14
  li      r9, 0x1C
  lhbrx   r5, [r3 + r9]
  oris    r5, r5, 0x0A03
  stw     [r10], r5
  lhz     r5, [r3 + 0x2C]
  li      r9, 4
  sthbrx  [r10 + r9], r5
  lhz     r5, [r4 + 6]
  li      r9, 6
  sthbrx  [r10 + r9], r5
  lwz     r5, [r4]
  stw     [r10 + 8], r5
  mr      r3, r11
  mr      r4, r10
  li      r5, 0x0C
  bl      send_60

on_handle_6x0A_call_object_update_handler_return:
  lwz     r3, [r1 + 0x08]
  lwz     r4, [r1 + 0x0C]
  lwz     r12, [r1 + 0x10]
  mtctr   r12
  addi    r1, r1, 0x20
  lwz     r0, [r1 + 4]
  mtlr    r0
  bctr



on_TObjectV8047c128_subtract_hp_without_sync:  # (TObjectV8047c128* this @ r3, int16_t amount @ r4)
  li      r5, 2
  b       on_TObjectV8047c128_hp_change
on_TObjectV8047c128_add_hp:  # (TObjectV8047c128* this @ r3, int16_t amount @ r4)
  li      r5, 1
  b       on_TObjectV8047c128_hp_change
on_TObjectV8047c128_subtract_hp:  # (TObjectV8047c128* this @ r3, int16_t amount @ r4)
  li      r5, 0

on_TObjectV8047c128_hp_change:  # (TObjectV8047c128* this @ r3, int16_t amount @ r4, uint8_t flags @ r5)
  lhz     r7, [r3 + 0x1C]
  cmplwi  r7, 0x1000
  blt     on_TObjectV8047c128_hp_change_skip_send
  cmplwi  r7, 0x4000
  bge     on_TObjectV8047c128_hp_change_skip_send

  mflr    r0
  stw     [r1 + 4], r0
  stwu    [r1 - 0x20], r1
  stw     [r1 + 0x08], r3
  stw     [r1 + 0x0C], r4
  stw     [r1 + 0x10], r5

  mr      r7, r3
  addi    r3, r1, 0x10
  li      r8,  0x1C
  lhbrx   r6, [r7 + r8]
  oris    r6, r6, 0xE403
  stw     [r3], r6  # cmd.header
  andi.   r0, r5, 1
  beq     on_TObjectV8047c128_hp_change_skip_negate
  neg     r4, r4
on_TObjectV8047c128_hp_change_skip_negate:
  li      r8, 4
  sthbrx  [r3 + r8], r4  # cmd.hit_amount
  lhz     r4, [r7 + 0x2E4]
  li      r8, 6
  sthbrx  [r3 + r8], r4  # cmd.total_damage_before_hit
  lhz     r4, [r7 + 0x32C]
  li      r8, 8
  sthbrx  [r3 + r8], r4  # cmd.current_hp_before_hit
  lhz     r4, [r7 + 0x2B8]
  li      r8, 0x0A
  sthbrx  [r3 + r8], r4  # cmd.max_hp

  andi.   r0, r5, 2
  bne     on_TObjectV8047c128_hp_change_local_only
  bl      send_and_handle_60
  b       on_TObjectV8047c128_hp_change_send_done
on_TObjectV8047c128_hp_change_local_only:
  bl      on_6xE4
on_TObjectV8047c128_hp_change_send_done:

  lwz     r3, [r1 + 0x08]
  lwz     r4, [r1 + 0x0C]
  lwz     r5, [r1 + 0x10]
  addi    r1, r1, 0x20
  lwz     r0, [r1 + 4]
  mtlr    r0

on_TObjectV8047c128_hp_change_skip_send:
  andi.   r0, r5, 1
  bne     on_TObjectV8047c128_hp_change_b_to_add
  b       TObjectV8047c128_subtract_hp
on_TObjectV8047c128_hp_change_b_to_add:
  b       TObjectV8047c128_add_hp



code_end:

  .data   0x00000000
  .data   0x00000000
