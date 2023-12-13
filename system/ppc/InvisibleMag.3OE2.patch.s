.meta name="Invisible MAG"
.meta description="Make MAGs invisible"
# Original code by Ralf @ GC-Forever

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocks
  # region @ 801150C0 (4 bytes)
  .data     0x801150C0  # address
  .data     0x00000004  # size
  .data     0x480000D4  # 801150C0 => b         +0x000000D4 /* 80115194 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
