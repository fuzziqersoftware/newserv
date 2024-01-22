# This function is required for loading DOLs. If it's not present, newserv can't
# serve DOL files to GameCube clients.

.meta index=E2

entry_ptr:
reloc0:
  .offsetof start

start:
disable_interrupts:
  mfmsr   r3
  rlwinm  r3, r3, 0, 17, 15
  mtmsr   r3

  bl      get_current_addr
dol_base_ptr:
  .zero
get_current_addr:
  mflr    r31
  # TODO: It'd be nice to be able to use an expression for the immediate value
  # here - something like (dol_base_ptr - start), for example
  subi    r31, r31, 0x10  # r31 = base of data to copy to low memory (start label)

  # If this code is not running from low memory (80001800-80003000), then copy
  # it there and branch to it
  lis     r3, 0x8000
  ori     r3, r3, 0x3000
  cmp     r31, r3
  blt     run_dol

copy_code_to_low_memory:
  bl      get_end_ptr
  sub     r30, r3, r31  # r30 = size of code to copy (for cache flushing later)
  subi    r5, r3, 4  # r5 = end ptr
  subi    r4, r31, 4
  lis     r3, 0x8000
  ori     r3, r3, 0x17FC
copy_code_to_low_memory__again:
  lwzu    r0, [r4 + 4]
  stwu    [r3 + 4], r0
  cmp     r4, r5
  bne     copy_code_to_low_memory__again

  # Flush the data cache and clear the instruction cache before running the
  # moved code
  lis     r3, 0x8000
  ori     r3, r3, 0x1800
  mr      r4, r30
  mtlr    r3
  b       flush_cached_code_writes



run_dol:
  lwz     r30, [r31 + 0x10]  # r30 = data base ptr

  # Decompress the file first. If the compressed size is zero, then skip this
  # step (the file is not compressed). The header consists of two fields:
  # compressed size followed by decompressed size.
  lwz     r6, [r30]
  cmplwi  r6, 0
  beq     run_dol__not_compressed
  lwz     r5, [r30 + 4]
  addi    r4, r30, 8  # Compressed data immediately follows the 2 header fields
  sub     r3, r30, r5  # Decompress to immediately before the compressed data
  mr      r30, r3  # Save DOL header pointer for after decompression
  bl      prs_decompress
  b       run_dol__decompressed

run_dol__not_compressed:
  addi    r30, r30, 8

run_dol__decompressed:
  # DOL files are very simple: they have up to 7 text sections, up to 11 data
  # sections, and a BSS section and an entrypoint. No imports or other fancy
  # things to do - we just have to move a bunch of bytes around.
  mr      r29, r30  # r29 = DOL header iterator
  addi    r28, r29, 0x48  # r28 = DOL header iterator end value

run_dol__move_section:
  lwz     r4, [r29]  # r4 = file offset of section data
  add     r4, r4, r30  # r4 = address of section data
  lwz     r3, [r29 + 0x48]  # r3 = dest address of section data
  lwz     r5, [r29 + 0x90]  # r5 = number of bytes to move
  cmplwi  r5, 0  # If size is 0, skip the section entirely
  beq     skip_section
  subi    r3, r3, 1
  subi    r4, r4, 1
  add     r5, r4, r5  # r5 = source end pointer
run_dol__move_section_data__again:
  # TODO: We probably should implement memmove-like semantics here, in case the
  # DOL loads at an unusually late address. This is probably very rare.
  lbzu    r0, [r4 + 1]
  stbu    [r3 + 1], r0
  cmp     r4, r5
  bne     run_dol__move_section_data__again

  # Flush the data cache and invalidate the instruction cache after copying the
  # section data. Technically we don't have to do this for data sections, but
  # I'm lazy and it doesn't take too long.
  lwz     r3, [r29 + 0x48]  # r3 = dest address of section data
  lwz     r4, [r29 + 0x90]  # r4 = size of section data
  bl      flush_cached_code_writes

skip_section:
  # Move to the next section
  addi    r29, r29, 4
  cmp     r29, r28
  bne     run_dol__move_section

run_dol__zero_bss:
  lwz     r3, [r30 + 0xD8]  # r3 = BSS address
  lwz     r4, [r30 + 0xDC]  # r4 = BSS size
  cmplwi  r4, 0
  beq     run_dol__skip_zero_bss
  add     r4, r3, r4  # r4 = BSS end address
  subi    r3, r3, 1
  li      r0, 0
run_dol__zero_bss__again:
  stbu    [r3 + 1], r0
  cmp     r3, r4
  bne     run_dol__zero_bss__again
run_dol__skip_zero_bss:

run_dol__go_to_entrypoint:
  lwz     r0, [r30 + 0xE0]  # r30 = entrypoint
  mtctr   r0
  bctr



flush_cached_code_writes:
  .include FlushCachedCode
  blr



prs_decompress:
  .include PRSDecompress



return_end_ptr:
  mflr    r3
  bctr
get_end_ptr:
  mflr    r0
  mtctr   r0
  bl      return_end_ptr
