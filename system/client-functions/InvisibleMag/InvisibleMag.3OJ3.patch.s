.meta name="Invisible MAG"
.meta description="Makes MAGs invisible"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC
  # region @ 80115118 (4 bytes)
  .data     0x80115118  # address
  .data     0x00000004  # size
  .data     0x480000D4  # 80115118 => b         +0x000000D4 /* 801151EC */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
