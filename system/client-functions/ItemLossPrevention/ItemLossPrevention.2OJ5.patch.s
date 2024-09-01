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
  .data     0x8C16BDFE
  .data     2
  sett

  .align    4
  .data     0x8C17F1DC
  .data     2
  and       r0, 0xFE

  .align    4
  .data     0x8C17F2BA
  .data     2
  nop

  .align    4
  .data     0x8C180D0A
  .data     2
  and       r0, 0xFE

  .align    4
  .data     0x8C180DB0
  .data     2
  nop

  .align    4
  .data     0x8C181BC4
  .data     2
  and       r0, 0xFE

  .align    4
  .data     0x8C181C92
  .data     2
  nop

  .align    4
  .data     0x8C182BC6
  .data     2
  and       r0, 0xFE

  .align    4
  .data     0x8C182BF4
  .data     2
  nop

  .align    4
  .data     0x8C1834D0
  .data     2
  and       r0, 0xFE

  .align    4
  .data     0x00000000
  .data     0x00000000
