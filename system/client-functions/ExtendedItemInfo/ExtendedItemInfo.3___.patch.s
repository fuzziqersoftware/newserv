.meta name="Ext item info"
.meta description="Shows more info\nbefore picking up\nan item"

.versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC

  .data     <VERS 0x804CA3D8 0x804CDB28 0x804D0078 0x804CFE18 0x804CABA8 0x804CB088 0x804CF6F0 0x804CFAE0>
  .data     0x00000004
  .data     0x00000023  # Set bit 0x20 in TWindowHelpItem::spec.flags to suppress open/close sounds

  .data     <VERS 0x80261998 0x80262570 0x802635B8 0x8026336C 0x80262270 0x80262270 0x80263630 0x80262E78>
  .data     0x00000004
  .address  <VERS 0x80261998 0x80262570 0x802635B8 0x8026336C 0x80262270 0x80262270 0x80263630 0x80262E78>
  bl        set_window_state_on_switch_to_item

  .data     <VERS 0x80261CA8 0x80262880 0x802638C8 0x8026367C 0x80262580 0x80262580 0x80263940 0x80263188>
  .data     0x00000004
  .address  <VERS 0x80261CA8 0x80262880 0x802638C8 0x8026367C 0x80262580 0x80262580 0x80263940 0x80263188>
  b         set_window_state_on_lock_on_delete

  .data     <VERS 0x80261974 0x8026254C 0x80263594 0x80263348 0x8026224C 0x8026224C 0x8026360C 0x80262E54>
  .data     0x00000004
  .address  <VERS 0x80261974 0x8026254C 0x80263594 0x80263348 0x8026224C 0x8026224C 0x8026360C 0x80262E54>
  bl        set_window_state_on_switch_to_player

  .data     <VERS 0x802619BC 0x80262594 0x802635DC 0x80263390 0x80262294 0x80262294 0x80263654 0x80262E9C>
  .data     0x00000004
  .address  <VERS 0x802619BC 0x80262594 0x802635DC 0x80263390 0x80262294 0x80262294 0x80263654 0x80262E9C>
  bl        set_window_state_on_switch_to_enemy

  .data     <VERS 0x80286BB0 0x80287924 0x802889D4 0x80288788 0x8028747C 0x802874C0 0x8028893C 0x80288140>
  .data     0x00000004
  .address  <VERS 0x80286BB0 0x80287924 0x802889D4 0x80288788 0x8028747C 0x802874C0 0x8028893C 0x80288140>
  b         on_TWindowMainMenu1P_created

  .data     <VERS 0x80286B14 0x80287888 0x80288938 0x802886EC 0x802873E0 0x80287424 0x802888A0 0x802880A4>
  .data     0x00000004
  .address  <VERS 0x80286B14 0x80287888 0x80288938 0x802886EC 0x802873E0 0x80287424 0x802888A0 0x802880A4>
  b         on_TWindowMainMenu1P_destroyed

  .label    is_split_screen, <VERS 0x800196B0 0x800196E0 0x800111BC 0x80019690 0x800196C8 0x800196C8 0x80019690 0x80019708>
  .label    malloc7, <VERS 0x80228C80 0x8022961C 0x8022A544 0x8022A2BC 0x8022953C 0x8022953C 0x8022A5F0 0x80229E88>
  .label    TWindow::close, <VERS 0x8024302C 0x802439EC 0x80244940 0x802446E0 0x802438E8 0x802438E8 0x80244A14 0x8024425C>
  .label    TWindowHelpItem::init, <VERS 0x8025426C 0x80254D14 0x80255CC8 0x80255A08 0x80254AF4 0x80254AF4 0x80255CCC 0x80255514>
  .label    TWindowHelpItem::set_displayed_item_by_id, <VERS 0x80253DF0 0x80254898 0x8025584C 0x8025558C 0x80254678 0x80254678 0x80255850 0x80255098>
  .label    TWindowLockOn::update_for_enemy, <VERS 0x8026137C 0x80261F54 0x80262F9C 0x80262D50 0x80261C54 0x80261C54 0x80263014 0x8026285C>
  .label    TWindowLockOn::update_for_item, <VERS 0x802616D8 0x802622B0 0x802632F8 0x802630AC 0x80261FB0 0x80261FB0 0x80263370 0x80262BB8>
  .label    TWindowLockOn::update_for_other_player, <VERS 0x80261850 0x80262428 0x80263470 0x80263224 0x80262128 0x80262128 0x802634E8 0x80262D30>

  .data     0x80004000
  .deltaof  code_start, code_end
  .address  0x80004000

code_start:
  .data     0x00000000  # Placeholder for active window pointer
  .data     0x00000000  # Placeholder for TWindowMainMenu1P object
  .data     0x41F00000  # TWindowHelpItem x position
  .data     0x43480000  # TWindowHelpItem y position

on_TWindowMainMenu1P_created:
  lis       r4, 0x8000
  stw       [r4 + 0x4004], r3
  blr

on_TWindowMainMenu1P_destroyed:
  lis       r4, 0x8000
  lwz       r0, [r4 + 0x4004]
  cmpl      r0, r3
  bne       on_TWindowMainMenu1P_destroyed_different_object
  li        r0, 0
  stw       [r4 + 0x4004], r0
on_TWindowMainMenu1P_destroyed_different_object:
  blr

set_window_state_on_lock_on_delete:
  stwu      [r1 - 0x20], r1
  mflr      r0
  stw       [r1 + 0x24], r0
  b         close_window_tail
set_window_state_on_switch_to_player:
  stwu      [r1 - 0x20], r1
  mflr      r0
  stw       [r1 + 0x24], r0
  bl        TWindowLockOn::update_for_other_player
  b         close_window_tail
set_window_state_on_switch_to_enemy:
  stwu      [r1 - 0x20], r1
  mflr      r0
  stw       [r1 + 0x24], r0
  bl        TWindowLockOn::update_for_enemy
close_window_tail:
  li        r3, 0
  bl        set_window_state
skip_close_window:
  lwz       r0, [r1 + 0x24]
  mtlr      r0
  addi      r1, r1, 0x20
  blr

set_window_state_on_switch_to_item:
  stwu      [r1 - 0x20], r1
  mflr      r0
  stw       [r1 + 0x24], r0
  stw       [r1 + 0x08], r4
  stw       [r1 + 0x0C], r31
  bl        TWindowLockOn::update_for_item

  lwz       r3, [r1 + 0x08]
  bl        set_window_state

skip_all:
  lwz       r31, [r1 + 0x0C]
  lwz       r0, [r1 + 0x24]
  addi      r1, r1, 0x20
  mtlr      r0
  blr

set_window_state:  # (TItem* item: r3) -> void
  stwu      [r1 - 0x20], r1
  mflr      r0
  stw       [r1 + 0x24], r0
  stw       [r1 + 0x08], r3
  stw       [r1 + 0x0C], r31
  lis       r31, 0x8000

  # If item is null, hide the window
  cmplwi    r3, 0
  beq       window_should_not_exist

  # Only show the window for weapons, armors, shields, and mags (not for
  # units, tools, or meseta)
  lhz       r4, [r3 + 0xEC]  # data[0] and data[1]
  cmplwi    r4, 0x0103
  beq       window_should_not_exist
  cmplwi    r4, 0x0300
  bge       window_should_not_exist

  # Don't show the window in split-screen mode
  bl        is_split_screen
  cmplwi    r3, 0
  bne       window_should_not_exist

  # If the TWindowMainMenu1P exists and is visible, the TWindowHelpItem should
  # not be visible
  lis       r3, 0x8000
  lwz       r3, [r3 + 0x4004]
  cmplwi    r3, 0
  beq       window_should_exist  # TWindowMainMenu does not exist
  lwz       r3, [r3 + 0x4C]
  rlwinm.   r3, r3, 0, 31, 31
  bne       window_should_not_exist  # TWindowMainMenu exists and is visible

window_should_exist:
  # Check if the window already exists
  lwz       r3, [r31 + 0x4000]
  cmplwi    r3, 0
  bne       window_already_exists

  # The window does not exist; create it
  li        r3, 0xBC
  bl        malloc7
  stw       [r31 + 0x4000], r3

  # Call the constructor if malloc7 succeeded
  cmplwi    r3, 0
  beq       set_window_state_return
  ori       r3, r31, 0x4008
  mr        r4, r3
  lwz       r3, [r31 + 0x4000]
  lwz       r5, [r13 - <VERS 0x5298 0x5290 0x5270 0x5270 0x5280 0x5280 0x5260 0x5220>]  # local_client_id
  bl        TWindowHelpItem::init
window_already_exists:

  # Set flag 0x20, which suppresses the open/close sound
  lwz       r3, [r31 + 0x4000]
  lwz       r0, [r3 + 0x2C]
  ori       r0, r0, 0x20
  stw       [r3 + 0x2C], r0

  # Call TWindowHelpItem::set_displayed_item_by_id(window, item->item_id)
  lwz       r4, [r1 + 0x08]  # item
  lwz       r4, [r4 + 0xD8]  # item->item_id
  bl        TWindowHelpItem::set_displayed_item_by_id

  b         set_window_state_return

window_should_not_exist:
  lwz       r3, [r31 + 0x4000]
  cmplwi    r3, 0
  beq       set_window_state_return
  li        r0, 0
  stw       [r31 + 0x4000], r0
  bl        TWindow::close

set_window_state_return:
  lwz       r31, [r1 + 0x0C]
  lwz       r0, [r1 + 0x24]
  addi      r1, r1, 0x20
  mtlr      r0
  blr

code_end:

  .data     0x00000000
  .data     0x00000000
