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
  .binary   000D1B05 00000001 00
  .binary   000D1B7C 00000002 EB08
  .binary   0020E755 00000001 EB
  .binary   0021197A 00000002 EB74
  .binary   00229125 00000002 9090
  .binary   002291A7 00000002 EB08
  .binary   0022A192 00000002 9090
  .binary   0022A20B 00000002 EB08
  .binary   0022BEA5 00000001 00
  .binary   0022BEDE 00000002 EB08
  .binary   0022C256 00000001 00
  .binary   00241858 00000001 00
  .binary   002418CC 00000002 EB08
  .binary   002A19F4 00000001 00
  .binary   002A1A6C 00000002 EB08
  .binary   002D53DA 00000001 00
  .binary   002D544D 00000002 EB08
  .binary   002EF9CE 00000001 EB
  .binary   00000000 00000000
