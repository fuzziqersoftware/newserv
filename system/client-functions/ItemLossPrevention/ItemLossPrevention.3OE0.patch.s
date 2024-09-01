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
  # region @ 801D381C (4 bytes)
  .data     0x801D381C  # address
  .data     0x00000004  # size
  .data     0x4800004C  # 801D381C => b         +0x0000004C /* 801D3868 */
  # region @ 801FF0FC (4 bytes)
  .data     0x801FF0FC  # address
  .data     0x00000004  # size
  .data     0x60000000  # 801FF0FC => nop
  # region @ 80200658 (4 bytes)
  .data     0x80200658  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80200658 => nop
  # region @ 802021C4 (4 bytes)
  .data     0x802021C4  # address
  .data     0x00000004  # size
  .data     0x38000000  # 802021C4 => li        r0, 0x0000
  # region @ 802C2A40 (4 bytes)
  .data     0x802C2A40  # address
  .data     0x00000004  # size
  .data     0x4800004C  # 802C2A40 => b         +0x0000004C /* 802C2A8C */
  # region @ 802D1480 (4 bytes)
  .data     0x802D1480  # address
  .data     0x00000004  # size
  .data     0x48000020  # 802D1480 => b         +0x00000020 /* 802D14A0 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
