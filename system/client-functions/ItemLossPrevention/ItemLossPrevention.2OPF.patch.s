.meta name="No item loss"
.meta description="Disables logic that\ndeletes items if\nyou don't log off\nnormally"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049
# DCv2 port by fuzziqersoftware

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
  .data     0x8C17EC74
  .data     2
  and       r0, 0xFE

  .align    4
  .data     0x8C17ED52
  .data     2
  nop

  .align    4
  .data     0x8C1807A2
  .data     2
  and       r0, 0xFE

  .align    4
  .data     0x8C180848
  .data     2
  nop

  .align    4
  .data     0x8C18165C
  .data     2
  and       r0, 0xFE

  .align    4
  .data     0x8C18172A
  .data     2
  nop

  .align    4
  .data     0x8C18265E
  .data     2
  and       r0, 0xFE

  .align    4
  .data     0x8C18268C
  .data     2
  nop

  .align    4
  .data     0x8C182F68
  .data     2
  and       r0, 0xFE

  .align    4
  .data     0x00000000
  .data     0x00000000
