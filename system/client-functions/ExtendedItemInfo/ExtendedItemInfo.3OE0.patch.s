.meta name="Ext item info"
.meta description="Shows more info\nbefore picking up\nan item"

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC

  .data     0x804CABA8
  .data     0x00000004
  .data     0x00000023  # Set bit 0x20 in TWindowHelpItem::spec.flags to suppress open/close sounds

  .data     0x80262270
  .data     0x00000004
  .address  0x80262270
  bl        set_window_state_on_switch_to_item

  .data     0x80262580
  .data     0x00000004
  .address  0x80262580
  b         set_window_state_on_lock_on_delete

  .data     0x8026224C
  .data     0x00000004
  .address  0x8026224C
  bl        set_window_state_on_switch_to_player

  .data     0x80262294
  .data     0x00000004
  .address  0x80262294
  bl        set_window_state_on_switch_to_enemy

  .data     0x8028747C
  .data     0x00000004
  .address  0x8028747C
  b         on_TWindowMainMenu1P_created

  .data     0x802873E0
  .data     0x00000004
  .address  0x802873E0
  b         on_TWindowMainMenu1P_destroyed

  .label    is_split_screen, 0x800196C8
  .label    malloc7, 0x8022953C
  .label    TWindow::close, 0x802438E8
  .label    TWindowHelpItem::init, 0x80254AF4
  .label    TWindowHelpItem::set_displayed_item_by_id, 0x80254678
  .label    TWindowLockOn::update_for_enemy, 0x80261C54
  .label    TWindowLockOn::update_for_item, 0x80261FB0
  .label    TWindowLockOn::update_for_other_player, 0x80262128

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
  lwz       r5, [r13 - 0x5280]  # local_client_id
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
