.meta name="No item loss"
.meta description="Don't lose items if\nyou don't log off\nnormally"

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksDC

  .align    4
  .data     0x8C028276
  .data     6
  nop
  bs        +0x2C
  nop

  .align    4
  .data     0x8C16B50A
  .data     2
  sett

  .align    4
  .data     0x8C17E73A
  .data     2
  nop

  .align    4
  .data     0x8C17E816
  .data     2
  nop

  .align    4
  .data     0x8C18005C
  .data     2
  nop

  .align    4
  .data     0x8C180100
  .data     2
  nop

  .align    4
  .data     0x8C180ECA
  .data     2
  nop

  .align    4
  .data     0x8C180F96
  .data     2
  nop

  .align    4
  .data     0x8C181DC0
  .data     2
  nop

  .align    4
  .data     0x8C181DEC
  .data     2
  nop

  .align    4
  .data     0x8C1825F2
  .data     2
  nop

  .align    4
  .data     0x00000000
  .data     0x00000000
