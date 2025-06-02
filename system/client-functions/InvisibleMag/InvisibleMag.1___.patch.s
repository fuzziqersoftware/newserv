.meta name="Invisible MAG"
.meta description="Makes MAGs invisible"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049
# DC port by fuzziqersoftware

.versions 1OJ2 1OJ3 1OJ4 1OJF 1OEF 1OPF 2OJ5 2OJF 2OEF 2OPF

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksDC

  .align    4
  .data     <VERS 0x8C1AADD8 0x8C1C7408 0x8C1C9E9C 0x8C1C75B4 0x8C1CA49C 0x8C1CA240 0x8C1F27E8 0x8C1F17F0 0x8C1F27E8 0x8C1F2354>
  .data     0x00000004
  rets
  nop

  .align    4
  .data     0x00000000
  .data     0x00000000
