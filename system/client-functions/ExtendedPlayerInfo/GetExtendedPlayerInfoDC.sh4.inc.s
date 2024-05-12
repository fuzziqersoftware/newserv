start:
  sts.l  -[r15], pr
  mov.l  -[r15], r8
  mov.l  -[r15], r9
  mov.l  -[r15], r10
  mov.l  -[r15], r11

  mova   r0, [data]  # r8 = data pointer
  mov    r8, r0

  # outbound_cmd = malloc9(0x16F8)
  mov    r0, 0x16
  shl    r0, 8
  or     r0, 0xF8
  mov    r4, r0
  mov    r11, r0
  mov.l  r0, [r8]
  calls  [r0]
  nop
  cmpeq  r0, 0
  bt     malloc9_failed
  mov    r9, r0

  # header = 30 00 F8 16
  # r11 = 0x16F8 (size used later in send loop)
  mov    r0, 0x30
  mov.w  [r9], r0
  mov    r0, r11
  mov.w  [r9 + 2], r0

  # memcpy(outbound_cmd.part1, char_file_part1, sizeof(char_file_part1))
  mov    r4, r9
  add    r4, 4
  mov.l  r5, [r8 + 4]
  mov.l  r5, [r5]
  mov    r0, 0x04
  shl    r0, 8
  or     r0, 0x1C
  calls  memcpy
  mov    r6, r0

  # memcpy(outbound_cmd.part2, char_file_part2, sizeof(char_file_part2))
  mov    r0, 0x04
  shl    r0, 8
  or     r0, 0x20
  mov    r4, r9
  add    r4, r0
  mov.l  r5, [r8 + 8]
  mov.l  r5, [r5]
  mov    r0, 0x12
  shl    r0, 8
  or     r0, 0xD8
  calls  memcpy
  mov    r6, r0

  # r10 = send ptr, r11 = send bytes remaining (already set earlier)
  mov    r10, r9
send_again:
  # root_protocol->wait_send_drain()
  mov.l  r4, [r8 + 0x0C]
  mov.l  r4, [r4]
  mov.l  r0, [r8 + 0x14]
  calls  [r0]
  nop
  cmpeq  r0, 0
  bf     drain_failed

  # root_protocol->send(send_ptr, min(send_bytes_remaining, 0x5B4))
  mov.l  r4, [r8 + 0x0C]
  mov.l  r4, [r4]
  mov    r5, r10
  mov    r6, r11
  mov    r0, 0x05
  shl    r0, 8
  or     r0, 0xB4
  cmpge  r0, r6
  bt     skip_adjust_size
  mov    r6, r0
skip_adjust_size:
  add    r10, r6  # adjust send_ptr
  sub    r11, r6  # adjust send_bytes_remaining
  mov.l  r0, [r4 + 0x18]
  mov.l  r0, [r0 + 0x2C]
  calls  [r0]
  nop

  cmpgt  r11, 0
  bt     send_again

drain_failed:
  # free(outbound_cmd)
  mov    r4, r9
  mov.l  r0, [r8 + 0x10]
  calls  [r0]
  nop
  mov    r0, 0

malloc9_failed:
  mov.l  r11, [r15]+
  mov.l  r10, [r15]+
  mov.l  r9, [r15]+
  mov.l  r8, [r15]+
  lds.l  pr, [r15]+
  rets
  nop

memcpy:
  test   r6, r6
  bt     memcpy_done
  mov.l  r0, [r5]+
  mov.l  [r4], r0
  add    r4, 4
  bs     memcpy
  add    r6, -4
memcpy_done:
  rets
  nop

  .align 4
data:
