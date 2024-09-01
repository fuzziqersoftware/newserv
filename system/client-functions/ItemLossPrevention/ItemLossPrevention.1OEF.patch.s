.meta name="No item loss"
.meta description="Disables logic that\ndeletes items if\nyou don't log off\nnormally"

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksDC

  .align    4
  .data     0x8C0254BE
  .data     4
  bs        +0x38
  nop

  .align    4
  .data     0x8C150F9C
  .data     2
  sett

  .align    4
  .data     0x8C15F856
  .data     2
  and       r0, 0xFE

  .align    4
  .data     0x8C160A4A
  .data     2
  and       r0, 0xFE

  .align    4
  .data     0x8C161D6A
  .data     2
  and       r0, 0xFE

  .align    4
  .data     0x8C15F8CA
  .data     2
  nop

  .align    4
  .data     0x8C160AB6
  .data     2
  nop

  .align    4
  .data     0x8C161D98
  .data     2
  nop

  .align    4
  .data     0x00000000
  .data     0x00000000
