start:
    mflr     r7

    # If this patch has already been run, then the opcode that led here will
    # not be bctrl (4E800421). In that case, do nothing.
    lis      r3, 0x4E80
    ori      r3, r3, 0x0421
    lwz      r4, [r7 - 4]
    cmp      r3, r4
    beq      apply_patch
    blr
apply_patch:

    bl       patch_end
    .offsetof patch
    .offsetof patch_end
patch:
    mfctr    r6
    mr       r3, r6
    li       r4, 0x7C00
    .include FlushCachedCode-GC
    mtctr    r6
    bctr
patch_end:
    mflr     r4

    addi     r4, r4, 8
    lwz      r3, [r4 - 8]
    lwz      r5, [r4 - 4]
    sub      r5, r5, r3

    # At this point:
    # r4 = address of patch label
    # r5 = patch size in bytes
    # r7 = saved LR

    # Find a spot in the interrupt handlers with enough memory for the patch
    lis      r3, 0x8000
    ori      r3, r3, 0x0200
    sub      r3, r3, r5

check_location:
    rlwinm   r0, r5, 30, 2, 31
    mtctr    r0  # ctr = patch size in words
    subi     r8, r3, 4
check_location_next_word:
    lwzu     r0, [r8 + 4]
    cmpwi    r0, 0
    beq      check_location_word_ok
    addi     r3, r3, 0x0100
    rlwinm   r0, r3, 0, 16, 31
    cmpwi    r0, 0x1800
    blt      check_location
    # No suitable location was found - return null
    li       r3, 0
    mtlr     r7
    blr

check_location_word_ok:
    bdnz     check_location_next_word

location_ok:
    mr       r6, r3
    # Now:
    # r3 = destination location
    # r4 = patch src data
    # r5 = patch size in bytes
    # r6 = destination location
    # r7 = saved LR
    .include CopyCode-GC

setup_branch:
    # Replace the bctrl opcode that led to this call with a bl opcode that
    # leads to the copied patch code
    subi     r3, r7, 4
    sub      r4, r6, r3
    rlwinm   r4, r4, 0, 6, 31
    oris     r4, r4, 0x4800
    ori      r4, r4, 0x0001
    stw      [r3], r4
    dcbst    r0, r3
    sync
    icbi     r0, r3
    isync

    # Return the address that the patch was copied to
    mr       r3, r6
    mtlr     r7
    blr
