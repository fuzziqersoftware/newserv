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
  .binary   000D1B85 00000001 00
  .binary   000D1BFC 00000002 EB08
  .binary   0020E805 00000001 EB
  .binary   002119CA 00000002 EB74
  .binary   002291B5 00000002 9090
  .binary   00229237 00000002 EB08
  .binary   0022A222 00000002 9090
  .binary   0022A29B 00000002 EB08
  .binary   0022BF35 00000001 00
  .binary   0022BF6E 00000002 EB08
  .binary   0022C2E6 00000001 00
  .binary   002418E8 00000001 00
  .binary   0024195C 00000002 EB08
  .binary   002A2904 00000001 00
  .binary   002A297C 00000002 EB08
  .binary   002D677A 00000001 00
  .binary   002D67ED 00000002 EB08
  .binary   002F0E1E 00000001 EB
  .binary   00000000 00000000
