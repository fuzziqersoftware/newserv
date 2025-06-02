.meta name="Item pickup"
.meta description="Prevents picking\nup items unless you\nhold the white or\nblack button"
# Original code by Ralf @ GC-Forever
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049
# Xbox port by fuzziqersoftware

.versions 4OED 4OEU 4OJB 4OJD 4OJU 4OPD 4OPU

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksXB
  .data     <VERS 0x001FDC99 0x001FDC99 0x001FDA89 0x001FDBE9 0x001FDE69 0x001FDCB9 0x001FDD29>
  .data     0x07
  .binary   E8880100009090
  .data     <VERS 0x001FDE26 0x001FDE26 0x001FDC16 0x001FDD76 0x001FDFF6 0x001FDE46 0x001FDEB6>
  .data     0x0A
  .binary   8B866C05000085C0EB46
  .data     <VERS 0x001FDE76 0x001FDE76 0x001FDC66 0x001FDDC6 0x001FE046 0x001FDE96 0x001FDF06>
  .data     0x09
  .binary   74038A40013408EB46
  .data     <VERS 0x001FDEC5 0x001FDEC5 0x001FDCB5 0x001FDE15 0x001FE095 0x001FDEE5 0x001FDF55>
  .data     0x0A
  .binary   7507F68624030000E0C3
  .data     <VERS 0x0025ADAD 0x0025AEED 0x0025A94D 0x0025ACCD 0x0025B07D 0x0025ADCD 0x0025AF1D>
  .data     0x01
  .binary   00
  .data     0x00000000
  .data     0x00000000
