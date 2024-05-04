.meta name="Invisible MAG"
.meta description="Make MAGs invisible"

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksDC

  .align    4
  .data     0x8C1CA49C
  .data     0x00000004
  rets
  nop

  .align    4
  .data     0x00000000
  .data     0x00000000
