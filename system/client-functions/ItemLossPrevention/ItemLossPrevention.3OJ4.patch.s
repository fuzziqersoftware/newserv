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
  # region @ 801D3CC4 (4 bytes)
  .data     0x801D3CC4  # address
  .data     0x00000004  # size
  .data     0x4800004C  # 801D3CC4 => b         +0x0000004C /* 801D3D10 */
  # region @ 801FD944 (4 bytes)
  .data     0x801FD944  # address
  .data     0x00000004  # size
  .data     0x38000000  # 801FD944 => li        r0, 0x0000
  # region @ 8020010C (4 bytes)
  .data     0x8020010C  # address
  .data     0x00000004  # size
  .data     0x60000000  # 8020010C => nop
  # region @ 802016CC (4 bytes)
  .data     0x802016CC  # address
  .data     0x00000004  # size
  .data     0x60000000  # 802016CC => nop
  # region @ 802C42E4 (4 bytes)
  .data     0x802C42E4  # address
  .data     0x00000004  # size
  .data     0x4800004C  # 802C42E4 => b         +0x0000004C /* 802C4330 */
  # region @ 802D2C10 (4 bytes)
  .data     0x802D2C10  # address
  .data     0x00000004  # size
  .data     0x48000020  # 802D2C10 => b         +0x00000020 /* 802D2C30 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
