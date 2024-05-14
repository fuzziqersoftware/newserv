start:
  sts.l  -[r15], pr

  mova   r0, [data]  # r8 = data pointer
  mov    r7, r0

  # memcpy(char_file_part1, inbound_part1, sizeof(char_file_part1))
  mov.l  r4, [r7]
  mov.l  r4, [r4]
  mov    r5, r7
  mov.l  r6, [part1_size]  # This cannot be in the delay slot after `calls`
  calls  memcpy
  add    r5, 8

  # First, copy some important fields out of the saved character so that the
  # game won't consider the character corrupt. Note that r5 already points to
  # inbound_part2 here since it immediately follows inbound_part1.
  mov.l  r4, [r7 + 4]
  mov.l  r4, [r4]
  mov    r1, r4  # r1 = character_file_part2
  mov    r2, r5  # r2 = inbound_part2

  mov    r4, r2
  mov    r5, r1
  mov.l  r0, [r5 + 0x04]  # creation_timestamp
  mov.l  [r4 + 0x04], r0
  mov.l  r0, [r5 + 0x08]  # signature
  mov.l  [r4 + 0x08], r0
  add    r4, 0x14
  add    r5, 0x14
  calls  memcpy  # save_count, ppp_username, ppp_password (0x30 bytes total)
  mov    r6, 0x30

  mov    r4, r2
  mov    r5, r1
  mov.l  r0, [v1_creds_offset]
  add    r4, r0
  add    r5, r0
  calls  memcpy  # v1_serial_number, v1_access_key
  mov    r6, 0x20

  mov    r4, r2
  mov    r5, r1
  mov.l  r0, [v2_creds_offset]
  add    r4, r0
  add    r5, r0
  calls  memcpy  # v2_serial_number, v2_access_key
  mov    r6, 0x20

  # memcpy(char_file_part2, inbound_part2, sizeof(char_file_part2))
  mov    r4, r1
  mov.l  r6, [part2_size]  # This cannot be in the delay slot after `calls`
  calls  memcpy
  mov    r5, r2

  lds.l  pr, [r15]+
  rets
  mov    r0, 0

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
part1_size:
  .data  0x0000041C
part2_size:
  .data  0x000012D8
v1_creds_offset:
  .data  0x0000118C
v2_creds_offset:
  .data  0x000012B8

  .align 4
data:
