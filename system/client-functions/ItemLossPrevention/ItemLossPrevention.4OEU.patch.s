.meta name="No item loss"
.meta description="Disables logic that\ndeletes items if\nyou don't log off\nnormally"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049
# Xbox port by fuzziqersoftware

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksXB
  .data     0x000D1BD5
  .data     0x00000001
  .binary   00
  .data     0x000D1C4C
  .data     0x00000002
  .binary   EB08
  .data     0x0020E805
  .data     0x00000001
  .binary   EB
  .data     0x00211A2A
  .data     0x00000002
  .binary   EB74
  .data     0x00229255
  .data     0x00000002
  .binary   9090
  .data     0x002292D7
  .data     0x00000002
  .binary   EB08
  .data     0x0022A2C2
  .data     0x00000002
  .binary   9090
  .data     0x0022A33B
  .data     0x00000002
  .binary   EB08
  .data     0x0022BFD5
  .data     0x00000001
  .binary   00
  .data     0x0022C00E
  .data     0x00000002
  .binary   EB08
  .data     0x0022C386
  .data     0x00000001
  .binary   00
  .data     0x00241A78
  .data     0x00000001
  .binary   00
  .data     0x00241AEC
  .data     0x00000002
  .binary   EB08
  .data     0x002A2B34
  .data     0x00000001
  .binary   00
  .data     0x002A2BAC
  .data     0x00000002
  .binary   EB08
  .data     0x002D6C8A
  .data     0x00000001
  .binary   00
  .data     0x002D6CFD
  .data     0x00000002
  .binary   EB08
  .data     0x002F0FCE
  .data     0x00000001
  .binary   EB
  .data     0x00000000
  .data     0x00000000
