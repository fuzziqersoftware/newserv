.versions 1OJ3 1OJ4 1OJF 1OEF 1OPF

start:
  # This code runs at the time the jmp_on arguments decoder returns.

  # This script runs on the first frame during the quest loading procedure, but this procedure is started from the
  # lobby overview, not from a game! To make the result of loading a quest sane, we need to set some extra state that
  # will take effect when loading is done.


  # Clear the fake ShopMes pointer which was set in the preamble to make it not crash before the quest script starts.
  mov.l    r0, [ShopMes_addr]
  mov      r4, 0
  mov.l    [r0], r4

  # Set the area to load to the lobby. On later versions we use ba_initial_floor for this, but v1 doesn't have that
  # quest opcode, so we have to do it manually here instead.
  mov.l    r0, [area_config_addr]
  mov      r5, 0x0F
  mov.l    [r0], r5
  mov      r5, 0x10
  mov.l    [r0 + 0x04], r5
  mov.l    [r0 + 0x08], r4
  mov.l    [r0 + 0x0C], r4

  # Cause the client to leave the "game" immediately (sends 98). We don't have to save pr when calling another function
  # because we manually "return" to a specific address below
  mov.l    r0, [leave_game_addr]
  calls    [r0]
  nop



  # Copy handle_B2 over handle_0E
  bs       get_handle_B2_addr
  nop
get_handle_B2_addr_ret:
  sts      r1, pr
  mov.l    r2, [handle_B2_size]
  add      r2, r1  # r2 = handle_B2_end
  mov.l    r3, [handle_B2_dest_ptr]
copy_handle_B2_next_word:
  mov.w    r0, [r1]+
  mov.w    [r3], r0
  cmpeq    r1, r2
  bfs      copy_handle_B2_next_word
  add      r3, 2

  # Clear caches appropriately for the copied code
  calls    flush_all_caches
  nop

  # Replace command 0E with command B2. We don't need to change the code address since we overwrote handle_0E directly
  mov      r1, -0x4E
  extu.b   r1, r1
  mov.l    r0, [command_handler_entry_addr]
  mov.l    [r0], r1



  # The original return address has been destroyed to call this code, so we need to manually return to the right place
  mov.l    r0, [quest_call_return_addr]
  bs       [r0]
  nop

  .align   4
ShopMes_addr:
  .data    <VERS 0x8C4B8E20 0x8C4C6880 0x8C4C87E0 0x8C499880 0x8C499800>
leave_game_addr:
  .data    <VERS 0x8C1DD914 0x8C1E0468 0x8C1DDAC0 0x8C1E0A68 0x8C1E080C>
area_config_addr:
  .data    <VERS 0x8C447CE4 0x8C455744 0x8C4576A4 0x8C428744 0x8C4286C4>
quest_call_return_addr:
  .data    <VERS 0x8C1500D2 0x8C151222 0x8C1500F6 0x8C151692 0x8C15144E>
command_handler_entry_addr:
  .data    <VERS 0x8C2870DC 0x8C28AD7C 0x8C291B64 0x8C28B654 0x8C28B124>
handle_B2_dest_ptr:
  .data    <VERS 0x8C0289E8 0x8C029058 0x8C028930 0x8C029064 0x8C029078>
handle_B2_size:
  .data    (handle_B2_end - handle_B2)



get_handle_B2_addr:
  calls   get_handle_B2_addr_ret
  nop

handle_B2:  # [std](TProtocol* proto @ r4, void* data @ r5)
  mov.l   -[r15], r13
  mov.l   -[r15], r14
  sts.l   -[r15], pr
  mov     r13, r4
  mov     r14, r5

  # Checksum the requested range first
  mov     r0, -1  # Result value
  mov.l   r1, [crc32_xor_value]
  mov.l   r3, [r14 + 0x08]  # Address
  mov.l   r4, [r14 + 0x0C]  # Size
  add     r4, r3  # End address
crc32_next_byte:
  cmpeq   r3, r4
  bt      crc32_done
  mov.b   r5, [r3]+
  extu.b  r5, r5
  xor     r0, r5
  mov     r5, 8  # Bit count for xors below
crc32_next_bit:
  test    r0, 1
  bts     crc32_bit_zero
  shr     r0, 1
  xor     r0, r1
crc32_bit_zero:
  dec     r5
  bf      crc32_next_bit
  nop
  bs      crc32_next_byte
  nop
crc32_done:
  mov     r1, -1
  xor     r0, r1
  mov.l   -[r15], r0

  # Relocate the code segment and run it
  mov     r0, 0
  mov.l   -[r15], r0  # Default return value (if there's no code segment)
  mov.l   r1, [r14 + 4]  # Code size
  test    r1, r1
  bt      no_code_segment

  # Get the code addresses
  mov     r6, r14
  add     r6, 0x10  # r6 = code base address
  mov     r7, r6
  add     r7, r1
  add     r7, -0x20  # r7 = footer address (code base + code size - 0x20)

  mov.l   r3, [r7]
  add     r3, r6  # r3 = relocation list
  mov     r4, r6  # r4 = relocation target addr (starts at code base addr)
  mov.l   r2, [r7 + 4]  # r2 = relocation count
  test    r2, r2
next_relocation:
  bt      relocations_done
  mov.w   r0, [r3]+
  extu.w  r0, r0
  shl     r0, 2
  add     r4, r0  # Advance relocation pointer by next word
  mov.l   r0, [r4]
  add     r0, r6
  mov.l   [r4], r0  # Update value at target address
  bs      next_relocation
  dec     r2
relocations_done:

  # Ensure the code section is executable by flushing the data cache and invalidating the instruction cache
  mov.l   -[r15], r6
  mov.l   -[r15], r7
  calls   flush_all_caches
  nop
  mov.l   r7, [r15]+
  mov.l   r6, [r15]+

  # Call the code section
  mov.l   r0, [r7 + 0x10]
  add     r0, r6
  mov.l   r0, [r0]
  calls   [r0]  # (*footer->root)()
  nop
  mov.l   [r15], r0  # Return value
no_code_segment:

  # Push the command header (B3 XX 0C 00)
  mov.l   r1, [return_command_header]
  mov.b   r0, [r14 + 1]
  extu.b  r0, r0
  shl     r0, 8
  or      r1, r0
  mov.l   -[r15], r1

  # Send the response
  mov     r4, r13
  mov     r5, r15
  mov     r6, 0x0C
  mov.l   r0, [r13 + 0x18]
  mov.l   r0, [r0 + 0x2C]
  calls   [r0]
  nop

  # Clean up stack and return
  add     r15, 0x0C
  lds.l   pr, [r15]+
  mov.l   r14, [r15]+
  rets
  mov.l   r13, [r15]+

flush_all_caches:  # [std]() -> void; writes back entire data cache and flushes entire instruction cache
  # Write back the entire data cache
  sts.l   -[r15], pr
  mov.l   r0, [cache_writeback_all]  # []() -> void
  calls   [r0]
  nop

  # Save SR and mask interrupts, then switch to executing in uncached memory
  stc     r0, sr
  mov.l   -[r15], r0
  or      r0, 0xF0
  ldc     sr, r0
  mov.l   r0, [cache_p2_bit]
  bs      pc + r0
  nop

  # Invalidate the entire instruction cache
  mov.l   r0, [cache_invalidate_all]  # []() -> void
  calls   [r0]
  nop

  # Execute in uncached memory long enough after the CCR write (must be at least 8 instructions)
  mov     r2, 4
cache_sync_wait:
  dec     r2
  bf      cache_sync_wait

  # Restore SR and return to the caller
  ldc.l   sr, [r15]+
  lds.l   pr, [r15]+
  rets
  nop

  .align  4
cache_writeback_all:
  .data   <VERS 0x8C2DF92C 0x8C2ECDF4 0x8C2EB730 0x8C2ED250 0x8C2ED1CC>
cache_p2_bit:
  .data   0x20000000
cache_invalidate_all:
  .data   <VERS 0xAC300F04 0xAC30E3CC 0xAC30CD08 0xAC30E828 0xAC30E7A4>
crc32_xor_value:
  .data   0xEDB88320
return_command_header:
  .data   0x000C00B3
handle_B2_end:

  # Make sure handle_B2 doesn't overwrite anything outside of the original handle_0E
  .assert (handle_B2_end - handle_B2) <= 0x108
