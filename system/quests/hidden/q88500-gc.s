// This function copies an inline handler for the B2 command (function call)
// to an unused area of memory, and inserts it into the game's command handler
// table, thus making the B2 command fully functional as it is on most other
// versions of the game.

// We could do the code copy and callsite modification directly in the quest
// script, but that would restrict us to only using addresses that end in 00.
// Furthermore, doing it this way provides an example of how to embed native
// code in a quest script and run it from within the script.

start:
  mflr    r11
  bl      get_handle_B2_ptr

handle_B2:
  # Arguments:
  # r3 = TProtocol* proto (we use this to call the send function)
  # r4 = void* data
  # Returns: void

  mflr    r0
  stwu    [r1 - 0x40], r1
  stw     [r1 + 0x44], r0

  # Stack:
  # [r1+08] = B3 XX 0C 00
  # [r1+0C] = code section's return value
  # [r1+10] = checksum
  # [r1+14] = saved ctx argument
  # [r1+18] = saved data argument
  stw     [r1 + 0x14], r3
  stw     [r1 + 0x18], r4

  # Set up the reply header (B3 XX 0C 00, where XX comes from the B2 command)
  lbz     r5, [r4 + 1]
  rlwinm  r5, r5, 16, 8, 15
  oris    r5, r5, 0xB300
  ori     r5, r5, 0x0C00
  stw     [r1 + 0x08], r5

  # If there's no code section, skip it. We also write the code section size to
  # the return value field (which will be overwritten later if the size is not
  # zero). This is because I'm lazy and this gives the behavior we want: the
  # code return value is always zero if the code section size is zero.
  li      r6, 4
  lwbrx   r5, [r4 + r6] # r5 = code_size
  stw     [r1 + 0x0C], r5 # response.code_return_value = code_size
  cmplwi  r5, 0
  beq     handle_B2_skip_code

  # Get the code section base and footer addresses
  addi    r6, r4, 0x10 # r6 = code base address
  add     r7, r6, r5
  subi    r7, r7, 0x20 # r7 = footer address (code base + code size - 0x20)

  # Check if there are relocations to do
  lwz     r8, [r7 + 4] # r8 = num relocations
  cmplwi  r8, 0
  beq     handle_B2_skip_relocations

  # Execute the relocations
  mtctr   r8
  lwz     r8, [r7] # r8 = relocations list offset
  add     r8, r8, r6 # r8 = relocations list address
  subi    r8, r8, 2 # Back up one space so we can use lhzu in the loop
  mr      r10, r6 # relocation pointer = code base address
handle_B2_relocate_again:
  lhzu    r9, [r8 + 2]
  rlwinm  r9, r9, 2, 0, 29 # r9 = next_relocation_offset * 4
  add     r10, r10, r9 # relocation pointer += next_relocation_offset * 4
  lwz     r9, [r10]
  add     r9, r9, r6
  stw     [r10], r9 # (*relocation pointer) += code base address
  bdnz    handle_B2_relocate_again
handle_B2_skip_relocations:

  # Invalidate the caches appropriately for the newly-copied code
  lis     r0, 0x8000
  ori     r0, r0, 0xC274
  mr      r3, r6
  mr      r4, r5
  bl      call_flush_code  # flush_code(code_base_addr, code_section_size)

  # Call the code section and put the return value (byteswapped) on the stack
  # Note: flush_code only uses r3, r4, and r5, so we don't need to reload r7
  # after the above call
  lwz     r8, [r7 + 0x10]
  lwzx    r8, [r8 + r6]
  mtctr   r8
  bctrl
  li      r8, 0x0C
  stwbrx  [r1 + r8], r3
handle_B2_skip_code:

  # Get the checksum function args
  lwz     r4, [r1 + 0x18]
  li      r5, 0x08
  lwbrx   r3, [r4 + r5] # checksum addr
  li      r5, 0x0C
  lwbrx   r4, [r4 + r5] # checksum size
  bl      crc32   # crc32(checksum_addr, checksum_size)
  li      r8, 0x10
  stwbrx  [r1 + r8], r3

  # Send the response (B3 command)
  lwz     r3, [r1 + 0x14]
  lwz     r4, [r3 + 0x18]
  lwz     r4, [r4 + 0x28]
  mtctr   r4
  addi    r4, r1, 0x08
  li      r5, 0x0C
  bctrl   # TProtocol::send_command(ctx, &reply_data, 0x0C)

  # Clean up stack and return
  lwz     r0, [r1 + 0x44]
  addi    r1, r1, 0x40
  mtlr    r0
  blr

crc32:
  subi    r3, r3, 1  # So we can use lbzu
  add     r4, r3, r4  # r4 = end ptr (also adjusted for lbzu, implicitly)
  li      r5, -1  # r5 = result value (0xFFFFFFFF initially)
  lis     r7, 0xEDB8
  ori     r7, r7, 0x8320  # 1-bit xor value
  li      r8, 8  # Number of bits per byte

crc32_again:
  cmpl    r3, r4
  beq     crc32_done

  lbzu    r9, [r3 + 1]
  xor     r5, r5, r9  # result ^= next_input_value

  mtctr   r8
crc32_next_bit:
  rlwinm  r6, r5, 0, 31, 31  # r6 = low bit of result
  rlwinm  r5, r5, 31, 1, 31  # result >>= 1
  neg     r6, r6
  and     r6, r6, r7
  xor     r5, r5, r6  # result ^= (0xEDB88320 if low bit was 1, else 0)
  bdnz    crc32_next_bit
  b       crc32_again

crc32_done:
  xoris   r3, r5, 0xFFFF
  xori    r3, r3, 0xFFFF
  blr     # return (result ^ 0xFFFFFFFF)

call_flush_code:
  lis     r5, 0x8000
  ori     r5, r5, 0xC274
  mtctr   r5
  lhz     r0, [r5 + 6]
  cmplwi  r0, 0xFFF1
  beqctr
  addi    r5, r5, 0xB0  # 8000C324
  mtctr   r5
  bctr

get_handle_B2_ptr:
  mflr    r9  # r9 = &handle_B2
  bl      get_handle_B2_end_ptr
get_handle_B2_end_ptr:
  mflr    r10
  subi    r10, r10, 8  # r10 = pointer to end of handle_B2

  # Copy handle_B2 to 8000B0E0, which is normally unused by the game
  lis     r12, 0x8000
  ori     r12, r12, 0xB0E0  # r12 = 0x8000B0E0
  sub     r7, r10, r9
  rlwinm  r7, r7, 30, 2, 31  # r7 = number of words to copy
  mtctr   r7
  subi    r8, r12, 4  # r8 = r12 - 4 (so we can use stwu)
  subi    r9, r9, 4  # r9 = r9 - 4 (so we can use lwzu)
copy_handle_B2_word_again:
  lwzu    r0, [r9 + 4]
  stwu    [r8 + 4], r0
  bdnz    copy_handle_B2_word_again

  # Invalidate the caches appropriately for the newly-copied code
  mr      r3, r12
  rlwinm  r4, r7, 30, 2, 31
  bl      call_flush_code  # flush_code(copied_B2_handler, copied_B2_handler_bytes)

  # Replace the command handler table entry for command 0E (which is an unused
  # legacy command and has very broken behavior) with our B2 implementation
  li      r0, 0x00B2
  lis     r6, 0x804C
  ori     r5, r6, 0x4E08  # US v1.2
  lwz     r3, [r5]
  cmplwi  r3, 0x000E
  beq     patch_main_handlers_write
  ori     r5, r6, 0x5530  # JP v1.5
  lwz     r3, [r5]
  cmplwi  r3, 0x000E
  beq     patch_main_handlers_write
  lis     r6, 0x8045
  subi    r5, r6, 0x097C  # US Ep3
  lwz     r3, [r5]
  cmplwi  r3, 0x000E
  beq     patch_main_handlers_write
  ori     r5, r6, 0x1A3C  # EU Ep3
  lwz     r3, [r5]
  cmplwi  r3, 0x000E
  bne     done

patch_main_handlers_write:
  stw     [r5], r0
  stw     [r5 + 0x0C], r12

done:
  mtlr    r11
  blr
