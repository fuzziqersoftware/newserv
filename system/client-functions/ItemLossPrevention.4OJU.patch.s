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
  .binary   000D1AF5 00000001 00
  .binary   000D1B6C 00000002 EB08
  .binary   0020E9D5 00000001 EB
  .binary   00211BFA 00000002 EB74
  .binary   00229415 00000002 9090
  .binary   00229497 00000002 EB08
  .binary   0022A482 00000002 9090
  .binary   0022A4FB 00000002 EB08
  .binary   0022C195 00000001 00
  .binary   0022C1CE 00000002 EB08
  .binary   0022C546 00000001 00
  .binary   00241BD8 00000001 00
  .binary   00241C4C 00000002 EB08
  .binary   002A2EC4 00000001 00
  .binary   002A2F3C 00000002 EB08
  .binary   002D6CBA 00000001 00
  .binary   002D6D2D 00000002 EB08
  .binary   002F0FCE 00000001 EB
  .binary   00000000 00000000
