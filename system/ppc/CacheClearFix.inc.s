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
    .include FlushCachedCode
    mtctr    r6
    bctr
patch_end:
    mflr     r4

    addi     r4, r4, 8
    lwz      r3, [r4 - 8]
    lwz      r5, [r4 - 4]
    sub      r5, r5, r3

    lis      r3, 0x8000
    ori      r3, r3, 0x01BC
    mr       r6, r3
    # At this point:
    # r3 = destination location (overwritten by CopyCode)
    # r4 = patch src data (overwritten by CopyCode)
    # r5 = patch size in bytes (overwritten by CopyCode)
    # r6 = destination location
    # r7 = saved LR
    .include CopyCode

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
