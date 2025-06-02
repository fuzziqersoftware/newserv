.meta hide_from_patches_menu
.meta name="GetExtendedPlayerInfo"
.meta description=""

.versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0 3SJT 3SJ0 3SE0 3SP0

entry_ptr:
reloc0:
  .offsetof start
start:
  stwu   [r1 - 0x40], r1
  mflr   r0
  stw    [r1 + 0x44], r0
  stw    [r1 + 0x08], r31
  stw    [r1 + 0x0C], r30
  stw    [r1 + 0x10], r29
  stw    [r1 + 0x14], r28
  stw    [r1 + 0x18], r27

  b      get_data_ptr
get_data_ptr_ret:
  mflr   r30

  lwz    r27, [r30 + 0x18]  # sizeof(part2)

  addi   r3, r27, 0x42C  # sizeof(header) + sizeof(part1) + sizeof(part2) + sizeof(unused fields after part2)
  lwz    r0, [r30]
  mtctr  r0
  bctrl  # malloc9
  mr.    r31, r3
  beq    malloc9_failed

  li     r0, 0x3000
  sth    [r31], r0
  addi   r3, r27, 0x42C
  stb    [r31 + 2], r3
  rlwinm r3, r3, 24, 24, 31
  stb    [r31 + 3], r3  # header = 30 00 SS SS

  lwz    r4, [r30 + 0x04]
  lwz    r4, [r4]  # r4 = char_file_part1
  addi   r3, r31, 0x0004
  li     r5, 0x041C  # sizeof(part1)
  bl     memcpy

  lwz    r4, [r30 + 0x08]
  lwz    r4, [r4]  # r4 = char_file_part2
  addi   r3, r31, 0x0420
  mr     r5, r27
  bl     memcpy

  li     r0, 0
  add    r3, r27, r31
  addi   r3, r3, 0x420  # r3 = pointer to unused fields after part2
  stw    [r3 + 0], r0
  stw    [r3 + 4], r0
  stw    [r3 + 8], r0

  mr     r28, r31
  addi   r29, r27, 0x42C
send_again:
  lwz    r3, [r30 + 0x0C]
  lwz    r3, [r3]
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
  li     r3, 0

malloc9_failed:
  lwz    r27, [r1 + 0x18]
  lwz    r28, [r1 + 0x14]
  lwz    r29, [r1 + 0x10]
  lwz    r30, [r1 + 0x0C]
  lwz    r31, [r1 + 0x08]
  lwz    r0, [r1 + 0x44]
  addi   r1, r1, 0x40
  mtlr   r0
  blr

memcpy:
  .include CopyDataWords
  blr

get_data_ptr:
  bl     get_data_ptr_ret
data:
  .data  <VERS 0x803D9E38 0x803DC818 0x803DE6B8 0x803DE468 0x803DB0E0 0x803DB138 0x803DE838 0x803DD328 0x80358094 0x8038B09C 0x8038C0EC 0x8038CF94>  # malloc9
  .data  <VERS 0x805C4E68 0x805CF430 0x805D68B0 0x805D6650 0x805C5760 0x805CC740 0x805D5F60 0x805D21A0 0x8058B980 0x80579880 0x8057A6F0 0x8057CB10>  # char_file_part1
  .data  <VERS 0x805C4E6C 0x805CF434 0x805D68B4 0x805D6654 0x805C5764 0x805CC744 0x805D5F64 0x805D21A4 0x8058B984 0x80579884 0x8057A6F4 0x8057CB14>  # char_file_part2
  .data  <VERS 0x805C4488 0x805CEA50 0x805D5ED0 0x805D5C70 0x805C4D80 0x805CBD60 0x805D5580 0x805D17C0 0x8058B3A0 0x805792E0 0x8057A150 0x8057C570>  # root_protocol (anchor: send_05)
  .data  <VERS 0x803D9E90 0x803DC870 0x803DE710 0x803DE4C0 0x803DB138 0x803DB190 0x803DE890 0x803DD380 0x803580EC 0x8038B0F4 0x8038C144 0x8038CFEC>  # free9
  .data  <VERS 0x8007848C 0x800785F0 0x80078748 0x800786A0 0x800787B0 0x800787B0 0x8007889C 0x80078820 0x80026FE4 0x80026A04 0x80026B88 0x80026BB8>  # TProtocol_wait_send_drain
  .data  <VERS 0x00002370 0x00002370 0x00002370 0x00002370 0x00002370 0x00002370 0x00002370 0x00002370 0x000041F4 0x0000358C 0x0000358C 0x0000358C>  # sizeof(*char_file_part2)
