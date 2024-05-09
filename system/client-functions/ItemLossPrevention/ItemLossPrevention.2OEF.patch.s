.meta name="No item loss"
.meta description="Don't lose items if\nyou don't log off\nnormally"

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksDC

  .align    4
  .data     0x8C16BDFE
  .data     2
  sett

  .align    4
  .data     0x8C17F1DE
  .data     2
  nop

  .align    4
  .data     0x8C180DB0
  .data     2
  nop

  .align    4
  .data     0x8C181C92
  .data     2
  nop

  .align    4
  .data     0x8C1834D2
  .data     2
  nop

  .align    4
  .data     0x8C182BC8
  .data     2
  nop

  .align    4
  .data     0x8C180D0C
  .data     2
  nop

  .align    4
  .data     0x8C181BC6
  .data     2
  nop

  .align    4
  .data     0x8C0280AA
  .data     6
  .binary   090014A00900
  # nop
  # bs        +0x2C  # 8C0280D8
  # nop

  .align    4
  .data     0x00000000
  .data     0x00000000
