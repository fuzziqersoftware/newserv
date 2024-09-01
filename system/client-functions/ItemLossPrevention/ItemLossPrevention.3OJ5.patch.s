.meta name="No item loss"
.meta description="Disables logic that\ndeletes items if\nyou don't log off\nnormally"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC
  # region @ 801D39B8 (4 bytes)
  .data     0x801D39B8  # address
  .data     0x00000004  # size
  .data     0x4800004C  # 801D39B8 => b         +0x0000004C /* 801D3A04 */
  # region @ 801FF710 (4 bytes)
  .data     0x801FF710  # address
  .data     0x00000004  # size
  .data     0x60000000  # 801FF710 => nop
  # region @ 80200C9C (4 bytes)
  .data     0x80200C9C  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80200C9C => nop
  # region @ 80202860 (4 bytes)
  .data     0x80202860  # address
  .data     0x00000004  # size
  .data     0x38000000  # 80202860 => li        r0, 0x0000
  # region @ 802C3E78 (4 bytes)
  .data     0x802C3E78  # address
  .data     0x00000004  # size
  .data     0x4800004C  # 802C3E78 => b         +0x0000004C /* 802C3EC4 */
  # region @ 802D2938 (4 bytes)
  .data     0x802D2938  # address
  .data     0x00000004  # size
  .data     0x48000020  # 802D2938 => b         +0x00000020 /* 802D2958 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
