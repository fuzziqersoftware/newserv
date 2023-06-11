entry_ptr:
reloc0:
    .offsetof start

start:
    mflr r7
    lis r3, 0x804D
    ori r3, r3, 0xDD98
    lis r4, 0x3f33
    ori r4, r4, 0x3333
    stw [r3], r4
    mtlr r7
    blr
