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
  .binary   000D1BA5 00000001 00
  .binary   000D1C1C 00000002 EB08
  .binary   0020E825 00000001 EB
  .binary   002119EA 00000002 EB74
  .binary   002291D5 00000002 9090
  .binary   00229257 00000002 EB08
  .binary   0022A242 00000002 9090
  .binary   0022A2BB 00000002 EB08
  .binary   0022BF55 00000001 00
  .binary   0022BF8E 00000002 EB08
  .binary   0022C306 00000001 00
  .binary   00241908 00000001 00
  .binary   0024197C 00000002 EB08
  .binary   002A2924 00000001 00
  .binary   002A299C 00000002 EB08
  .binary   002D67AA 00000001 00
  .binary   002D681D 00000002 EB08
  .binary   002F0E4E 00000001 EB
  .binary   00000000 00000000
