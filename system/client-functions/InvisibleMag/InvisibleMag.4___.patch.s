.meta name="Invisible MAG"
.meta description="Makes MAGs invisible"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049
# Xbox port by fuzziqersoftware

.versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksXB
  .data     <VERS 0x001837C1 0x00183951 0x00183A01 0x00183941 0x00183971 0x00183961 0x00183931>
  .data     0x00000002
  .binary   90E9
  .data     0x00000000
  .data     0x00000000
