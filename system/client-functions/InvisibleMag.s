# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049
# DC and Xbox ports by fuzziqersoftware

.meta visibility="all"
.meta name="Invisible MAG"
.meta description="Makes MAGs invisible"


entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocks



  .versions 1OJ2 1OJ3 1OJ4 1OJF 1OEF 1OPF 2OJ4 2OJ5 2OJF 2OEF 2OPF
  .align    4
  .data     <VERS 0x8C1AADD8 0x8C1C7408 0x8C1C9E9C 0x8C1C75B4 0x8C1CA49C 0x8C1CA240 0x8C1F27E8 0x8C1F27E8 0x8C1F17F0 0x8C1F27E8 0x8C1F2354>
  .data     0x00000004
  rets
  nop
  .align    4



  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x80114F04 0x80115118 0x8011521C 0x801150B0 0x801151A8 0x801151A8 0x801150C0 0x80115298>
  .data     0x00000004
  .data     0x480000D4



  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x001837C1 0x00183951 0x00183A01 0x00183941 0x00183971 0x00183961 0x00183931>
  .data     0x00000002
  .binary   90E9



  .all_versions
  .data     0x00000000
  .data     0x00000000
