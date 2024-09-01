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
  .data     0x000D1AF5
  .data     0x00000001
  .binary   00
  .data     0x000D1B6C
  .data     0x00000002
  .binary   EB08
  .data     0x0020E9D5
  .data     0x00000001
  .binary   EB
  .data     0x00211BFA
  .data     0x00000002
  .binary   EB74
  .data     0x00229415
  .data     0x00000002
  .binary   9090
  .data     0x00229497
  .data     0x00000002
  .binary   EB08
  .data     0x0022A482
  .data     0x00000002
  .binary   9090
  .data     0x0022A4FB
  .data     0x00000002
  .binary   EB08
  .data     0x0022C195
  .data     0x00000001
  .binary   00
  .data     0x0022C1CE
  .data     0x00000002
  .binary   EB08
  .data     0x0022C546
  .data     0x00000001
  .binary   00
  .data     0x00241BD8
  .data     0x00000001
  .binary   00
  .data     0x00241C4C
  .data     0x00000002
  .binary   EB08
  .data     0x002A2EC4
  .data     0x00000001
  .binary   00
  .data     0x002A2F3C
  .data     0x00000002
  .binary   EB08
  .data     0x002D6CBA
  .data     0x00000001
  .binary   00
  .data     0x002D6D2D
  .data     0x00000002
  .binary   EB08
  .data     0x002F0FCE
  .data     0x00000001
  .binary   EB
  .data     0x00000000
  .data     0x00000000
