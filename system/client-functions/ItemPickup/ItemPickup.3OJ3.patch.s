.meta name="Item pickup"
.meta description="Prevents picking\nup items unless you\nhold the Z button"
# Original code by Ralf @ GC-Forever
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC
  .data     0x8000B938
  .data     0x00000020
  .data     0x387C0550
  .data     0x38800100
  .data     0x483447DD
  .data     0x2C030000
  .data     0x4182000C
  .data     0x7F83E378
  .data     0x481AA1B8
  .data     0x481AA1C4
  .data     0x801B5B04
  .data     0x00000004
  .data     0x4BE55E34
  .data     0x8024CDD0
  .data     0x00000004
  .data     0x38800008
  .data     0x00000000
  .data     0x00000000
