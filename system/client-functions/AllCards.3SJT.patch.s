# This patch gives you the maximum number of each card. It only works if used
# in-game, which means it must be used by running `$patch AllCards`.

.meta hide_from_patches_menu
.meta name="Get all cards"
.meta description="This patch gives you\nthe maximum number\nof each card."

entry_ptr:
reloc0:
  .offsetof start

start:
  stwu   [r1 - 0x20], r1
  mflr   r0
  stw    [r1 + 0x24], r0
  stw    [r1 + 0x10], r31
  stw    [r1 + 0x14], r30
  stw    [r1 + 0x18], r29
  stw    [r1 + 0x1C], r28

  # Ep3PlayerDataSegment* seg = get_player_data_segment(0)
  lis    r3, 0x8029
  ori    r3, r3, 0x987C
  mtctr  r3
  li     r3, 0
  bctrl
  mr     r31, r3

  # decrypt_ep3_player_data_segment(seg)
  lis    r3, 0x8029
  ori    r3, r3, 0x92A4
  mtctr  r3
  mr     r3, r31
  bctrl

  # Ep3PlayerDataSegment_on_card_obtained(seg, card_id) for each card, 99 times
  lis    r28, 0x8029
  ori    r28, r28, 0x94C0
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
  lis    r3, 0x8029
  ori    r3, r3, 0x92F8
  mtctr  r3
  mr     r3, r31
  bctrl

  lwz    r31, [r1 + 0x10]
  lwz    r30, [r1 + 0x14]
  lwz    r29, [r1 + 0x18]
  lwz    r28, [r1 + 0x1C]
  lwz    r0, [r1 + 0x24]
  addi   r1, r1, 0x20
  mtlr   r0
  blr
