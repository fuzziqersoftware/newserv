entry_ptr:
reloc0:
    .offsetof start

start:
    mflr r7
    lis r1, 0x804D
    ori r1, r1, 0xDD98
    li r2, 0x3f33
    ori r2, r2, 0x3333
    stw [r1], r2
    mtlr r7
    blr
