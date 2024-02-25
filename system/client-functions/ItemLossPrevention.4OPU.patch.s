.meta name="No item loss"
.meta description="Don't lose items if\nyou don't log off\nnormally"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049; Xbox port by fuzziqersoftware

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksXB
  .binary   000D1BD5 00000001 00
  .binary   000D1C4C 00000002 EB08
  .binary   0020E895 00000001 EB
  .binary   00211ABA 00000002 EB74
  .binary   002292E5 00000002 9090
  .binary   00229367 00000002 EB08
  .binary   0022A352 00000002 9090
  .binary   0022A3CB 00000002 EB08
  .binary   0022C065 00000001 00
  .binary   0022C09E 00000002 EB08
  .binary   0022C416 00000001 00
  .binary   00241B08 00000001 00
  .binary   00241B7C 00000002 EB08
  .binary   002A2BF4 00000001 00
  .binary   002A2C6C 00000002 EB08
  .binary   002D6D0A 00000001 00
  .binary   002D6D7D 00000002 EB08
  .binary   002F103E 00000001 EB
  .binary   00000000 00000000
