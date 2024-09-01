.meta name="Movement"
.meta description="Allow backsteps and\nmovement when\nenemies are nearby"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC
  # region @ 801CE7AC (4 bytes)
  .data     0x801CE7AC  # address
  .data     0x00000004  # size
  .data     0x4800000C  # 801CE7AC => b         +0x0000000C /* 801CE7B8 */
  # region @ 801CF69C (4 bytes)
  .data     0x801CF69C  # address
  .data     0x00000004  # size
  .data     0x48000014  # 801CF69C => b         +0x00000014 /* 801CF6B0 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
