.meta key="GetExtendedPlayerInfo"
.meta name="Get extended player info"
.meta description=""

.versions 3OJT 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0 3SJT 3SJ0 3SE0 3SP0

  .label   malloc9, <VERS 0x803F73A8 0x803D9E38 0x803DC818 0x803DE6B8 0x803DE468 0x803DB0E0 0x803DB138 0x803DE838 0x803DD328 0x80358094 0x8038B09C 0x8038C0EC 0x8038CF94>
  .label   free9, <VERS 0x803F7400 0x803D9E90 0x803DC870 0x803DE710 0x803DE4C0 0x803DB138 0x803DB190 0x803DE890 0x803DD380 0x803580EC 0x8038B0F4 0x8038C144 0x8038CFEC>  # free9
  .label   char_file_part1, <VERS 0x8065F560 0x805C4E68 0x805CF430 0x805D68B0 0x805D6650 0x805C5760 0x805CC740 0x805D5F60 0x805D21A0 0x8058B980 0x80579880 0x8057A6F0 0x8057CB10>
  .label   char_file_part1_size, 0x041C  # Same on all versions
  .label   char_file_part2, <VERS 0x8065F564 0x805C4E6C 0x805CF434 0x805D68B4 0x805D6654 0x805C5764 0x805CC744 0x805D5F64 0x805D21A4 0x8058B984 0x80579884 0x8057A6F4 0x8057CB14>
  .label   char_file_part2_size, <VERS 0x2268 0x2370 0x2370 0x2370 0x2370 0x2370 0x2370 0x2370 0x2370 0x41F4 0x358C 0x358C 0x358C>
  .label   root_protocol, <VERS 0x8065EB20 0x805C4488 0x805CEA50 0x805D5ED0 0x805D5C70 0x805C4D80 0x805CBD60 0x805D5580 0x805D17C0 0x8058B3A0 0x805792E0 0x8057A150 0x8057C570>
  .label   TProtocol_wait_send_drain, <VERS 0x800CA360 0x8007848C 0x800785F0 0x80078748 0x800786A0 0x800787B0 0x800787B0 0x8007889C 0x80078820 0x80026FE4 0x80026A04 0x80026B88 0x80026BB8>
  .label   TProtocol_send_vfunc_index, <VERS 0x0C 0x0A 0x0A 0x0A 0x0A 0x0A 0x0A 0x0A 0x0A 0x0A 0x0A 0x0A 0x0A>
  .label   command_size, (4 + char_file_part1_size + char_file_part2_size + 0x0C)  # sizeof(header) + sizeof(part1) + sizeof(part2) + sizeof(unused fields after part2)

entry_ptr:
reloc0:
  .data    start
start:
  stwu     [r1 - 0x20], r1
  mflr     r0
  stw      [r1 + 0x24], r0
  stw      [r1 + 0x08], r31
  stw      [r1 + 0x0C], r29
  stw      [r1 + 0x10], r28

  li       r3, command_size
  lis      r0, high_word(malloc9)
  ori      r0, r0, low_word(malloc9)
  mtctr    r0
  bctrl
  mr.      r31, r3
  beq      malloc9_failed

  lis      r0, 0x3000
  ori      r0, r0, bswap16(command_size)
  stw      [r31], r0  # header = 30 00 SS SS (size is little-endian)

  lis      r4, addr_high(char_file_part1)
  lwz      r4, [r4 + addr_low(char_file_part1)]  # r4 = char_file_part1
  addi     r3, r31, 4
  li       r5, char_file_part1_size
  bl       memcpy

  lis      r4, addr_high(char_file_part2)
  lwz      r4, [r4 + addr_low(char_file_part2)]
  addi     r3, r31, (4 + char_file_part1_size)
  li       r5, char_file_part2_size
  bl       memcpy

  li       r0, 0
  stw      [r31 + (command_size - 0x0C)], r0
  stw      [r31 + (command_size - 0x08)], r0
  stw      [r31 + (command_size - 0x04)], r0

  mr       r28, r31
  li       r29, command_size
send_again:
  lis      r0, high_word(TProtocol_wait_send_drain)
  ori      r0, r0, low_word(TProtocol_wait_send_drain)
  lis      r3, addr_high(root_protocol)
  lwz      r3, [r3 + addr_low(root_protocol)]
  mtctr    r0
  bctrl
  mr.      r0, r3
  bne      drain_failed

  lis      r3, addr_high(root_protocol)
  lwz      r3, [r3 + addr_low(root_protocol)]
  mr       r4, r28
  mr       r5, r29
  cmplwi   r5, 0x05B4
  ble      skip_adjust_size
  li       r5, 0x05B4
skip_adjust_size:
  add      r28, r28, r5
  sub      r29, r29, r5
  lwz      r12, [r3 + 0x18]  # root_protocol->vtable
  lwz      r12, [r12 + (4 * TProtocol_send_vfunc_index)]
  mtctr    r12
  bctrl    # root_protocol->send(&cmd, sizeof(cmd))
  cmplwi   r29, 0
  bne      send_again

drain_failed:
  mr       r3, r31
  lis      r0, high_word(free9)
  ori      r0, r0, low_word(free9)
  mtctr    r0
  bctrl    # free9
  li       r3, 0

malloc9_failed:
  lwz      r28, [r1 + 0x10]
  lwz      r29, [r1 + 0x0C]
  lwz      r31, [r1 + 0x08]
  lwz      r0, [r1 + 0x24]
  addi     r1, r1, 0x20
  mtlr     r0
  blr

memcpy:
  .include CopyDataWords
  blr
