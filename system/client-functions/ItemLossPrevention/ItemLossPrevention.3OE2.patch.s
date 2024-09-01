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
  # region @ 801D3A1C (4 bytes)
  .data     0x801D3A1C  # address
  .data     0x00000004  # size
  .data     0x4800004C  # 801D3A1C => b         +0x0000004C /* 801D3A68 */
  # region @ 801FFA44 (4 bytes)
  .data     0x801FFA44  # address
  .data     0x00000004  # size
  .data     0x60000000  # 801FFA44 => nop
  # region @ 80200FD0 (4 bytes)
  .data     0x80200FD0  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80200FD0 => nop
  # region @ 80202B94 (4 bytes)
  .data     0x80202B94  # address
  .data     0x00000004  # size
  .data     0x38000000  # 80202B94 => li        r0, 0x0000
  # region @ 802C402C (4 bytes)
  .data     0x802C402C  # address
  .data     0x00000004  # size
  .data     0x4800004C  # 802C402C => b         +0x0000004C /* 802C4078 */
  # region @ 802D2AEC (4 bytes)
  .data     0x802D2AEC  # address
  .data     0x00000004  # size
  .data     0x48000020  # 802D2AEC => b         +0x00000020 /* 802D2B0C */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
