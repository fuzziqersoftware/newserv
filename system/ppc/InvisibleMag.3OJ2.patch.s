.meta name="Invisible MAG"
.meta description="Make MAGs invisible"
# Original code by Ralf @ GC-Forever

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocks
  # region @ 80114F04 (4 bytes)
  .data     0x80114F04  # address
  .data     0x00000004  # size
  .data     0x480000D4  # 80114F04 => b         +0x000000D4 /* 80114FD8 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
