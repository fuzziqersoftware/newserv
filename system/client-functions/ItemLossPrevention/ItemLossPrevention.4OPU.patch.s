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
  .data     0x0020E895
  .data     0x00000001
  .binary   EB
  .data     0x00211ABA
  .data     0x00000002
  .binary   EB74
  .data     0x002292E5
  .data     0x00000002
  .binary   9090
  .data     0x00229367
  .data     0x00000002
  .binary   EB08
  .data     0x0022A352
  .data     0x00000002
  .binary   9090
  .data     0x0022A3CB
  .data     0x00000002
  .binary   EB08
  .data     0x0022C065
  .data     0x00000001
  .binary   00
  .data     0x0022C09E
  .data     0x00000002
  .binary   EB08
  .data     0x0022C416
  .data     0x00000001
  .binary   00
  .data     0x00241B08
  .data     0x00000001
  .binary   00
  .data     0x00241B7C
  .data     0x00000002
  .binary   EB08
  .data     0x002A2BF4
  .data     0x00000001
  .binary   00
  .data     0x002A2C6C
  .data     0x00000002
  .binary   EB08
  .data     0x002D6D0A
  .data     0x00000001
  .binary   00
  .data     0x002D6D7D
  .data     0x00000002
  .binary   EB08
  .data     0x002F103E
  .data     0x00000001
  .binary   EB
  .data     0x00000000
  .data     0x00000000
