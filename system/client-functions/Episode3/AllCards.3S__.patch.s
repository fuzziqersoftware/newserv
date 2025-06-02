# This patch gives you the maximum number of each card. It only works if used
# in-game, which means it must be used by running `$patch AllCards`.

.meta hide_from_patches_menu
.meta name="All cards"
.meta description="Gives you the\nmaximum number of\neach card."

.versions 3SJT 3SJ0 3SE0 3SP0

entry_ptr:
reloc0:
  .offsetof start

start:
  stwu   [r1 - 0x20], r1
  mflr   r0
  stw    [r1 + 0x24], r0
  stmw   [r1 + 0x0C], r27

  b      get_data
get_data_ret:
  mflr   r27

  # Ep3PlayerDataSegment* seg = get_player_data_segment(0)
  lwz    r3, [r27]
  mtctr  r3
  li     r3, 0
  bctrl
  mr     r31, r3

  # decrypt_ep3_player_data_segment(seg)
  lwz    r3, [r27 + 4]
  mtctr  r3
  mr     r3, r31
  bctrl

  # Ep3PlayerDataSegment_on_card_obtained(seg, card_id) for each card, 99 times
  lwz    r28, [r27 + 8]  # Ep3PlayerDataSegment_on_card_obtained
  li     r30, 1  # r30 = card_id
obtain_card_99times:
  li     r29, 99  # r29 = obtain count
obtain_card_again:
  mr     r3, r31
  mr     r4, r30
  mtctr  r28
  bctrl
  subi   r29, r29, 1
  cmplwi r29, 0
  bne    obtain_card_again
  addi   r30, r30, 1
  cmplwi r30, 0x2F0
  ble    obtain_card_99times

  # encrypt_ep3_player_data_segment(seg)
  lwz    r3, [r27 + 0x0C]  # encrypt_ep3_player_data_segment
  mtctr  r3
  mr     r3, r31
  bctrl

  lmw    r27, [r1 + 0x0C]
  lwz    r0, [r1 + 0x24]
  addi   r1, r1, 0x20
  mtlr   r0
  blr

get_data:
  bl     get_data_ret
  .data     <VERS 0x8029987C 0x802A1154 0x802A1BAC 0x802A25A4>  # get_player_data_segment
  .data     <VERS 0x802992A4 0x802A0B64 0x802A15BC 0x802A1FB4>  # decrypt_ep3_player_data_segment
  .data     <VERS 0x802994C0 0x802A0D54 0x802A17AC 0x802A21A4>  # Ep3PlayerDataSegment_on_card_obtained
  .data     <VERS 0x802992F8 0x802A0BB4 0x802A160C 0x802A2004>  # encrypt_ep3_player_data_segment
