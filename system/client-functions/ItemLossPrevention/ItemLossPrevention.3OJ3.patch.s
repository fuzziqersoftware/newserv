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
  # region @ 801D38EC (4 bytes)
  .data     0x801D38EC  # address
  .data     0x00000004  # size
  .data     0x4800004C  # 801D38EC => b         +0x0000004C /* 801D3938 */
  # region @ 801FF174 (4 bytes)
  .data     0x801FF174  # address
  .data     0x00000004  # size
  .data     0x60000000  # 801FF174 => nop
  # region @ 802006D0 (4 bytes)
  .data     0x802006D0  # address
  .data     0x00000004  # size
  .data     0x60000000  # 802006D0 => nop
  # region @ 8020223C (4 bytes)
  .data     0x8020223C  # address
  .data     0x00000004  # size
  .data     0x38000000  # 8020223C => li        r0, 0x0000
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
