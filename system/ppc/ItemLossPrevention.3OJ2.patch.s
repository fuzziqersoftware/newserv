.meta name="No item loss"
.meta description="Don't lose items if\nyou don't log off\nnormally"
# Original code by Ralf @ GC-Forever

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocks
  # region @ 801D33E4 (4 bytes)
  .data     0x801D33E4  # address
  .data     0x00000004  # size
  .data     0x4800004C  # 801D33E4 => b         +0x0000004C /* 801D3430 */
  # region @ 802C2060 (4 bytes)
  .data     0x802C2060  # address
  .data     0x00000004  # size
  .data     0x4800004C  # 802C2060 => b         +0x0000004C /* 802C20AC */
  # region @ 802D0AA0 (4 bytes)
  .data     0x802D0AA0  # address
  .data     0x00000004  # size
  .data     0x48000020  # 802D0AA0 => b         +0x00000020 /* 802D0AC0 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
