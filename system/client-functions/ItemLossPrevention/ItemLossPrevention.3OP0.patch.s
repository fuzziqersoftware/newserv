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
  # region @ 801D3ED8 (4 bytes)
  .data     0x801D3ED8  # address
  .data     0x00000004  # size
  .data     0x4800004C  # 801D3ED8 => b         +0x0000004C /* 801D3F24 */
  # region @ 801FF9E0 (4 bytes)
  .data     0x801FF9E0  # address
  .data     0x00000004  # size
  .data     0x60000000  # 801FF9E0 => nop
  # region @ 80200F3C (4 bytes)
  .data     0x80200F3C  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80200F3C => nop
  # region @ 80202AA8 (4 bytes)
  .data     0x80202AA8  # address
  .data     0x00000004  # size
  .data     0x38000000  # 80202AA8 => li        r0, 0x0000
  # region @ 802C37C0 (4 bytes)
  .data     0x802C37C0  # address
  .data     0x00000004  # size
  .data     0x4800004C  # 802C37C0 => b         +0x0000004C /* 802C380C */
  # region @ 802D2280 (4 bytes)
  .data     0x802D2280  # address
  .data     0x00000004  # size
  .data     0x48000020  # 802D2280 => b         +0x00000020 /* 802D22A0 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
