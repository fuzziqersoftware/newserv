.meta name="Item pickup"
.meta description="Prevents picking\nup items unless you\nhold the white or\nblack button"
# Original code by Ralf @ GC-Forever
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049
# Xbox port by fuzziqersoftware

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksXB
  .data     0x001FDD29
  .data     0x07
  .binary   E8880100009090
  .data     0x001FDEB6
  .data     0x0A
  .binary   8B866C05000085C0EB46
  .data     0x001FDF06
  .data     0x09
  .binary   74038A40013408EB46
  .data     0x001FDF55
  .data     0x0A
  .binary   7507F68624030000E0C3
  .data     0x0025AF1D
  .data     0x01
  .binary   00
  .data     0x00000000
  .data     0x00000000
