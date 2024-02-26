.meta name="Enemy HP bars"
.meta description="Show HP bars in\nenemy info windows"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049
# Xbox port by fuzziqersoftware

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksXB
  .data     0x0026B193
  .data     0x00000001
  .binary   A0
  .data     0x0026B19C
  .data     0x00000001
  .binary   DA
  .data     0x0026B396
  .data     0x00000004
  .binary   836004FD
  .data     0x0054A1CC
  .data     0x00000004
  .data     0x42640000
  .data     0x0054A1D8
  .data     0x00000004
  .data     0x42640000
  .data     0x0054A1E4
  .data     0x00000004
  .data     0x42640000
  .data     0x0054A1F0
  .data     0x00000004
  .data     0x42640000
  .data     0x0054A28C
  .data     0x00000004
  .data     0x42300000
  .data     0x0054A2A8
  .data     0x00000004
  .data     0xFF00FF15
  .data     0x00000000
  .data     0x00000000
