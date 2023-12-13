.meta name="No item loss"
.meta description="Don't lose items if\nyou don't log off\nnormally"
# Original code by Ralf @ GC-Forever

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocks
  # region @ 801D38EC (4 bytes)
  .data     0x801D38EC  # address
  .data     0x00000004  # size
  .data     0x4800004C  # 801D38EC => b         +0x0000004C /* 801D3938 */
  # region @ 802C2F98 (4 bytes)
  .data     0x802C2F98  # address
  .data     0x00000004  # size
  .data     0x4800004C  # 802C2F98 => b         +0x0000004C /* 802C2FE4 */
  # region @ 802D1A58 (4 bytes)
  .data     0x802D1A58  # address
  .data     0x00000004  # size
  .data     0x48000020  # 802D1A58 => b         +0x00000020 /* 802D1A78 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
