.meta name="No item loss"
.meta description="Disables logic that\ndeletes items if\nyou don't log off\nnormally"

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksDC

  .align    4
  .data     0x8C0254B2
  .data     4
  bs        +0x38
  nop

  .align    4
  .data     0x8C150B2C
  .data     2
  sett

  .align    4
  .data     0x8C15F346
  .data     2
  and       r0, 0xFE

  .align    4
  .data     0x8C16053A
  .data     2
  and       r0, 0xFE

  .align    4
  .data     0x8C1617DA
  .data     2
  and       r0, 0xFE

  .align    4
  .data     0x8C15F3BA
  .data     2
  nop

  .align    4
  .data     0x8C1605A6
  .data     2
  nop

  .align    4
  .data     0x8C161808
  .data     2
  nop

  .align    4
  .data     0x00000000
  .data     0x00000000
