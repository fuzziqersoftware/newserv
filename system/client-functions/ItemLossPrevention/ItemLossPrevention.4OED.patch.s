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
  .data     0x000D1B85
  .data     0x00000001
  .binary   00
  .data     0x000D1BFC
  .data     0x00000002
  .binary   EB08
  .data     0x0020E805
  .data     0x00000001
  .binary   EB
  .data     0x002119CA
  .data     0x00000002
  .binary   EB74
  .data     0x002291B5
  .data     0x00000002
  .binary   9090
  .data     0x00229237
  .data     0x00000002
  .binary   EB08
  .data     0x0022A222
  .data     0x00000002
  .binary   9090
  .data     0x0022A29B
  .data     0x00000002
  .binary   EB08
  .data     0x0022BF35
  .data     0x00000001
  .binary   00
  .data     0x0022BF6E
  .data     0x00000002
  .binary   EB08
  .data     0x0022C2E6
  .data     0x00000001
  .binary   00
  .data     0x002418E8
  .data     0x00000001
  .binary   00
  .data     0x0024195C
  .data     0x00000002
  .binary   EB08
  .data     0x002A2904
  .data     0x00000001
  .binary   00
  .data     0x002A297C
  .data     0x00000002
  .binary   EB08
  .data     0x002D677A
  .data     0x00000001
  .binary   00
  .data     0x002D67ED
  .data     0x00000002
  .binary   EB08
  .data     0x002F0E1E
  .data     0x00000001
  .binary   EB
  .data     0x00000000
  .data     0x00000000
