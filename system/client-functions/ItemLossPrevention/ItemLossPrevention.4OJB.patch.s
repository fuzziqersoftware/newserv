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
  .data     0x000D1A35
  .data     0x00000001
  .binary   00
  .data     0x000D1AAC
  .data     0x00000002
  .binary   EB08
  .data     0x0020E5D5
  .data     0x00000001
  .binary   EB
  .data     0x0021170A
  .data     0x00000002
  .binary   EB74
  .data     0x00228F15
  .data     0x00000002
  .binary   9090
  .data     0x00228F97
  .data     0x00000002
  .binary   EB08
  .data     0x00229F82
  .data     0x00000002
  .binary   9090
  .data     0x00229FFB
  .data     0x00000002
  .binary   EB08
  .data     0x0022BC95
  .data     0x00000001
  .binary   00
  .data     0x0022BCCE
  .data     0x00000002
  .binary   EB08
  .data     0x0022C046
  .data     0x00000001
  .binary   00
  .data     0x00241608
  .data     0x00000001
  .binary   00
  .data     0x0024167C
  .data     0x00000002
  .binary   EB08
  .data     0x002A0FA4
  .data     0x00000001
  .binary   00
  .data     0x002A101C
  .data     0x00000002
  .binary   EB08
  .data     0x002D481A
  .data     0x00000001
  .binary   00
  .data     0x002D488D
  .data     0x00000002
  .binary   EB08
  .data     0x002EEEBE
  .data     0x00000001
  .binary   EB
  .data     0x00000000
  .data     0x00000000
