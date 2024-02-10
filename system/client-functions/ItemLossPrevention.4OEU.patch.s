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
  .binary   0020E805 00000001 EB
  .binary   00211A2A 00000002 EB74
  .binary   00229255 00000002 9090
  .binary   002292D7 00000002 EB08
  .binary   0022A2C2 00000002 9090
  .binary   0022A33B 00000002 EB08
  .binary   0022BFD5 00000001 00
  .binary   0022C00E 00000002 EB08
  .binary   0022C386 00000001 00
  .binary   00241A78 00000001 00
  .binary   00241AEC 00000002 EB08
  .binary   002A2B34 00000001 00
  .binary   002A2BAC 00000002 EB08
  .binary   002D6C8A 00000001 00
  .binary   002D6CFD 00000002 EB08
  .binary   002F0FCE 00000001 EB
  .binary   00000000 00000000
