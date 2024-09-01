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
  .data     0x000D1BA5
  .data     0x00000001
  .binary   00
  .data     0x000D1C1C
  .data     0x00000002
  .binary   EB08
  .data     0x0020E825
  .data     0x00000001
  .binary   EB
  .data     0x002119EA
  .data     0x00000002
  .binary   EB74
  .data     0x002291D5
  .data     0x00000002
  .binary   9090
  .data     0x00229257
  .data     0x00000002
  .binary   EB08
  .data     0x0022A242
  .data     0x00000002
  .binary   9090
  .data     0x0022A2BB
  .data     0x00000002
  .binary   EB08
  .data     0x0022BF55
  .data     0x00000001
  .binary   00
  .data     0x0022BF8E
  .data     0x00000002
  .binary   EB08
  .data     0x0022C306
  .data     0x00000001
  .binary   00
  .data     0x00241908
  .data     0x00000001
  .binary   00
  .data     0x0024197C
  .data     0x00000002
  .binary   EB08
  .data     0x002A2924
  .data     0x00000001
  .binary   00
  .data     0x002A299C
  .data     0x00000002
  .binary   EB08
  .data     0x002D67AA
  .data     0x00000001
  .binary   00
  .data     0x002D681D
  .data     0x00000002
  .binary   EB08
  .data     0x002F0E4E
  .data     0x00000001
  .binary   EB
  .data     0x00000000
  .data     0x00000000
