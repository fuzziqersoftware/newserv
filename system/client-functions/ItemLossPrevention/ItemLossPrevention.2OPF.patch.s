.meta name="No item loss"
.meta description="Don't lose items if\nyou don't log off\nnormally"

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksDC

  .align    4
  .data     0x8C0280AA
  .data     6
  nop
  bs        +0x2C
  nop

  .align    4
  .data     0x8C16BA22
  .data     2
  sett

  .align    4
  .data     0x8C17EC76
  .data     2
  nop

  .align    4
  .data     0x8C17ED52
  .data     2
  nop

  .align    4
  .data     0x8C1807A4
  .data     2
  nop

  .align    4
  .data     0x8C180848
  .data     2
  nop

  .align    4
  .data     0x8C18165E
  .data     2
  nop

  .align    4
  .data     0x8C18172A
  .data     2
  nop

  .align    4
  .data     0x8C182660
  .data     2
  nop

  .align    4
  .data     0x8C18268C
  .data     2
  nop

  .align    4
  .data     0x8C182F6A
  .data     2
  nop

  .align    4
  .data     0x00000000
  .data     0x00000000
