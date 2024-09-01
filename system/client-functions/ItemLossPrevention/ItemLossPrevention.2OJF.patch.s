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
  .data     0x8C17E738
  .data     2
  and       r0, 0xFE

  .align    4
  .data     0x8C17E816
  .data     2
  nop

  .align    4
  .data     0x8C18005A
  .data     2
  and       r0, 0xFE

  .align    4
  .data     0x8C180100
  .data     2
  nop

  .align    4
  .data     0x8C180EC8
  .data     2
  and       r0, 0xFE

  .align    4
  .data     0x8C180F96
  .data     2
  nop

  .align    4
  .data     0x8C181DBE
  .data     2
  and       r0, 0xFE

  .align    4
  .data     0x8C181DEC
  .data     2
  nop

  .align    4
  .data     0x8C1825F0
  .data     2
  and       r0, 0xFE

  .align    4
  .data     0x00000000
  .data     0x00000000
