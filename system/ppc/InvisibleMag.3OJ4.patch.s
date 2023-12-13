.meta name="Invisible MAG"
.meta description="Make MAGs invisible"
# Original code by Ralf @ GC-Forever

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocks
  # region @ 8011521C (4 bytes)
  .data     0x8011521C  # address
  .data     0x00000004  # size
  .data     0x480000D4  # 8011521C => b         +0x000000D4 /* 801152F0 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
