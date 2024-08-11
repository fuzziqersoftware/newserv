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
