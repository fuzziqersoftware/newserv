  stwu   [r1 - 0x20], r1
  stw    [r1 + 0x24], r12
  stw    [r1 + 0x08], r31
  stw    [r1 + 0x0C], r30
  stw    [r1 + 0x10], r29
  stw    [r1 + 0x14], r28
  mflr   r30

  li     r3, 0x279C
  lwz    r0, [r30]
  mtctr  r0
  bctrl  # malloc9
  mr.    r31, r3
  beq    malloc9_failed

  lis    r0, 0x3000
  ori    r0, r0, 0x9C27
  stw    [r31], r0  # header = 30 00 9C 27

  lwz    r0, [r30 + 0x04]
  mtctr  r0
  bctrl  # get_character_file
  mr     r28, r3

  lwz    r0, [r30 + 0x08]
  mtctr  r0
  bctrl  # get_selected_character_file_index
  mulli  r3, r3, 0x2798
  addi   r3, r3, 4
  add    r4, r3, r28  # r4 = &character_file->characters[selected_char_file_index]
  addi   r3, r31, 4
  li     r5, 0x2798
  bl     memcpy

  mr     r28, r31
  li     r29, 0x279C
send_again:
  lwz    r3, [r30 + 0x0C]
  lwz    r0, [r30 + 0x14]
  mtctr  r0
  bctrl  # TProtocol_wait_send_drain(root_protocol)
  mr.    r0, r3
  bne    drain_failed

  lwz    r3, [r30 + 0x0C]
  lwz    r3, [r3]  # root_protocol
  mr     r4, r28
  mr     r5, r29
  cmplwi r5, 0x05B4
  ble    skip_adjust_size
  li     r5, 0x05B4
skip_adjust_size:
  add    r28, r28, r5
  sub    r29, r29, r5
  lwz    r12, [r3 + 0x18]
  lwz    r12, [r12 + 0x28]
  mtctr  r12
  bctrl  # root_protocol->send(&cmd, sizeof(cmd))
  cmplwi r29, 0
  bne    send_again

drain_failed:
  mr     r3, r31
  lwz    r0, [r30 + 0x10]
  mtctr  r0
  bctrl  # free9
  li     r3, 1

malloc9_failed:
  lwz    r28, [r1 + 0x14]
  lwz    r29, [r1 + 0x10]
  lwz    r30, [r1 + 0x0C]
  lwz    r31, [r1 + 0x08]
  lwz    r0, [r1 + 0x24]
  addi   r1, r1, 0x20
  mtlr   r0
  blr

memcpy:
  .include CopyDataWords
  blr
