.meta name="No item loss"
.meta description="Disables logic that\ndeletes items if\nyou don't log off\nnormally"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049
# DCv2 port by fuzziqersoftware

.versions 2OEF 2OJ5 2OJF 2OPF

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksDC

  .align    4
  .data     <VERS 0x8C0280AA 0x8C0280AA 0x8C028276 0x8C0280AA>
  .data     6
  nop
  bs        +0x2C
  nop

  .align    4
  .data     <VERS 0x8C16BDFE 0x8C16BDFE 0x8C16B50A 0x8C16BA22>
  .data     2
  sett

  .align    4
  .data     <VERS 0x8C17F1DC 0x8C17F1DC 0x8C17E738 0x8C17EC74>
  .data     2
  and       r0, 0xFE

  .align    4
  .data     <VERS 0x8C17F2BA 0x8C17F2BA 0x8C17E816 0x8C17ED52>
  .data     2
  nop

  .align    4
  .data     <VERS 0x8C180D0A 0x8C180D0A 0x8C18005A 0x8C1807A2>
  .data     2
  and       r0, 0xFE

  .align    4
  .data     <VERS 0x8C180DB0 0x8C180DB0 0x8C180100 0x8C180848>
  .data     2
  nop

  .align    4
  .data     <VERS 0x8C181BC4 0x8C181BC4 0x8C180EC8 0x8C18165C>
  .data     2
  and       r0, 0xFE

  .align    4
  .data     <VERS 0x8C181C92 0x8C181C92 0x8C180F96 0x8C18172A>
  .data     2
  nop

  .align    4
  .data     <VERS 0x8C182BC6 0x8C182BC6 0x8C181DBE 0x8C18265E>
  .data     2
  and       r0, 0xFE

  .align    4
  .data     <VERS 0x8C182BF4 0x8C182BF4 0x8C181DEC 0x8C18268C>
  .data     2
  nop

  .align    4
  .data     <VERS 0x8C1834D0 0x8C1834D0 0x8C1825F0 0x8C182F68>
  .data     2
  and       r0, 0xFE

  .align    4
  .data     0x00000000
  .data     0x00000000
