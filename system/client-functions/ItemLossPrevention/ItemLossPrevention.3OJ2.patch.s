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
  # region @ 801D33E4 (4 bytes)
  .data     0x801D33E4  # address
  .data     0x00000004  # size
  .data     0x4800004C  # 801D33E4 => b         +0x0000004C /* 801D3430 */
  # region @ 801FE900 (4 bytes)
  .data     0x801FE900  # address
  .data     0x00000004  # size
  .data     0x60000000  # 801FE900 => nop
  # region @ 801FFE5C (4 bytes)
  .data     0x801FFE5C  # address
  .data     0x00000004  # size
  .data     0x60000000  # 801FFE5C => nop
  # region @ 802019C8 (4 bytes)
  .data     0x802019C8  # address
  .data     0x00000004  # size
  .data     0x38000000  # 802019C8 => li        r0, 0x0000
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
