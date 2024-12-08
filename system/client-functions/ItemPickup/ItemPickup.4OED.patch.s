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
  .data     0x001FDC99
  .data     0x07
  .binary   E8880100009090
  .data     0x001FDE26
  .data     0x0A
  .binary   8B866C05000085C0EB46
  .data     0x001FDE76
  .data     0x09
  .binary   74038A40013408EB46
  .data     0x001FDEC5
  .data     0x0A
  .binary   7507F68624030000E0C3
  .data     0x0025ADAD
  .data     0x01
  .binary   00
  .data     0x00000000
  .data     0x00000000
