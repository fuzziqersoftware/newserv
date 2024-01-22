# This code decompresses PRS-compressed data.
# Arguments:
#   r3 = destination pointer for decompressed data
#   r4 = source pointer for compressed data
#   r5 = destination buffer size
#   r6 = source data size
# Returns: number of bytes written to output buffer, or -1 on error
# Overwrites: r3, r4, r5, r6, r7, r8, r9, r10, r11, r12
prs_decompress__start:
  # r3 = dest ptr (used as write ptr)
  subi    r3, r3, 1
  # r4 = src ptr (used as read ptr)
  subi    r4, r4, 1
  # r5 = dest size (converted to ptr to last valid output byte)
  add     r5, r5, r3
  # r6 = src size (converted to ptr to last valid input byte)
  add     r6, r6, r4
  # r7 = control bits + guard bits
  li      r7, 0
  # r8 = temp for offset/count
  # r9 = original dest ptr - 1 (used for computing return value)
  mr      r9, r3
  # r10 = temp for reading/writing data
  # r11 = second-level saved LR, temp for offset/count
  # r12 = saved LR
  mflr    r12

prs_decompress__next_opcode:
  bl      prs_decompress__cmp_control_bit_and_return_in_r10
  beq     prs_decompress__control_0

prs_decompress__control_1:
  bl      prs_decompress__read_byte_to_r10
  bl      prs_decompress__write_byte_from_r10
  b       prs_decompress__next_opcode

prs_decompress__control_0:
  bl      prs_decompress__cmp_control_bit_and_return_in_r10
  beq     prs_decompress__control_00

prs_decompress__control_01:
  bl      prs_decompress__read_byte_to_r10
  rlwinm  r8, r10, 29, 27, 31  # low 5 bits of offset
  rlwinm  r11, r10, 0, 29, 31  # size
  addi    r11, r11, 2
  bl      prs_decompress__read_byte_to_r10
  rlwimi. r8, r10, 5, 19, 26  # high 8 bits of offset
  bne     prs_decompress__control_01_not_stop_opcode
  sub     r3, r3, r9
  mtlr    r12
  blr
prs_decompress__control_01_not_stop_opcode:
  ori     r8, r8, 0xE000
  cmplwi  r11, 2
  bne     prs_decompress__control_01_not_extended_count
  bl      prs_decompress__read_byte_to_r10
  addi    r11, r10, 1
prs_decompress__control_01_not_extended_count:
  mtctr   r11
  b       prs_decompress__control_00_01_copy

prs_decompress__control_00:
  bl      prs_decompress__cmp_control_bit_and_return_in_r10
  rlwinm  r11, r10, 1, 30, 30
  bl      prs_decompress__cmp_control_bit_and_return_in_r10
  rlwimi  r11, r10, 0, 31, 31
  addi    r11, r11, 2
  mtctr   r11
  bl      prs_decompress__read_byte_to_r10
  ori     r8, r10, 0xFF00

prs_decompress__control_00_01_copy:
  # r8 = src offset (negative 16-bit value)
  # ctr = byte count to copy
  oris    r8, r8, 0xFFFF
  add     r8, r8, r3  # r8 = copy src ptr (minus 1, for lbzu)
prs_decompress__control_00_01_copy_again:
  lbzu    r10, [r8 + 1]
  bl      prs_decompress__write_byte_from_r10
  bdnz    prs_decompress__control_00_01_copy_again
  b       prs_decompress__next_opcode

prs_decompress__cmp_control_bit_and_return_in_r10:
  andi.   r10, r7, 0x0100
  bne     prs_decompress__skip_read
  mflr    r8
  bl      prs_decompress__read_byte_to_r10
  mtlr    r8
  mr      r7, r10
  ori     r7, r7, 0xFF00
prs_decompress__skip_read:
  andi.   r10, r7, 1
  rlwinm  r7, r7, 31, 17, 31
  blr

prs_decompress__read_byte_to_r10:
  cmp     r4, r6
  bge     prs_decompress__return_error
  lbzu    r10, [r4 + 1]
  blr

prs_decompress__write_byte_from_r10:
  cmp     r3, r5
  bge     prs_decompress__return_error
  stbu    [r3 + 1], r10
  blr

prs_decompress__return_error:
  li      r3, -1
  mtlr    r12
  blr
