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
  # region @ 801150B0 (4 bytes)
  .data     0x801150B0  # address
  .data     0x00000004  # size
  .data     0x480000D4  # 801150B0 => b         +0x000000D4 /* 80115184 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
