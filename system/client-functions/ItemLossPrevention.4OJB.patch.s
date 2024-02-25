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
  .binary   000D1A35 00000001 00
  .binary   000D1AAC 00000002 EB08
  .binary   0020E5D5 00000001 EB
  .binary   0021170A 00000002 EB74
  .binary   00228F15 00000002 9090
  .binary   00228F97 00000002 EB08
  .binary   00229F82 00000002 9090
  .binary   00229FFB 00000002 EB08
  .binary   0022BC95 00000001 00
  .binary   0022BCCE 00000002 EB08
  .binary   0022C046 00000001 00
  .binary   00241608 00000001 00
  .binary   0024167C 00000002 EB08
  .binary   002A0FA4 00000001 00
  .binary   002A101C 00000002 EB08
  .binary   002D481A 00000001 00
  .binary   002D488D 00000002 EB08
  .binary   002EEEBE 00000001 EB
  .binary   00000000 00000000
