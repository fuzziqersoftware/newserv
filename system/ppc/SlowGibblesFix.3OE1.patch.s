entry_ptr:
reloc0:
    .offsetof start

start:
    mflr r7
    lis r4, 0x804D
    ori r4, r4, 0xE278
    li r5, 0x3f33
    ori r5, r5, 0x3333
    stw [r1], r2
    mtlr r7
    blr
