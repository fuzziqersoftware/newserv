.meta name="No item loss"
.meta description="Disables logic that\ndeletes items if\nyou don't log off\nnormally"

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksDC

  .align    4
  .data     0x8C0254D2
  .data     4
  bs        +0x38
  nop

  .align    4
  .data     0x8C150D58
  .data     2
  sett

  .align    4
  .data     0x8C15F612
  .data     2
  and       r0, 0xFE

  .align    4
  .data     0x8C160806
  .data     2
  and       r0, 0xFE

  .align    4
  .data     0x8C161B26
  .data     2
  and       r0, 0xFE

  .align    4
  .data     0x8C15F686
  .data     2
  nop

  .align    4
  .data     0x8C160872
  .data     2
  nop

  .align    4
  .data     0x8C161B54
  .data     2
  nop

  .align    4
  .data     0x00000000
  .data     0x00000000
