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
  .data     0x000D1B05
  .data     0x00000001
  .binary   00
  .data     0x000D1B7C
  .data     0x00000002
  .binary   EB08
  .data     0x0020E755
  .data     0x00000001
  .binary   EB
  .data     0x0021197A
  .data     0x00000002
  .binary   EB74
  .data     0x00229125
  .data     0x00000002
  .binary   9090
  .data     0x002291A7
  .data     0x00000002
  .binary   EB08
  .data     0x0022A192
  .data     0x00000002
  .binary   9090
  .data     0x0022A20B
  .data     0x00000002
  .binary   EB08
  .data     0x0022BEA5
  .data     0x00000001
  .binary   00
  .data     0x0022BEDE
  .data     0x00000002
  .binary   EB08
  .data     0x0022C256
  .data     0x00000001
  .binary   00
  .data     0x00241858
  .data     0x00000001
  .binary   00
  .data     0x002418CC
  .data     0x00000002
  .binary   EB08
  .data     0x002A19F4
  .data     0x00000001
  .binary   00
  .data     0x002A1A6C
  .data     0x00000002
  .binary   EB08
  .data     0x002D53DA
  .data     0x00000001
  .binary   00
  .data     0x002D544D
  .data     0x00000002
  .binary   EB08
  .data     0x002EF9CE
  .data     0x00000001
  .binary   EB
  .data     0x00000000
  .data     0x00000000
