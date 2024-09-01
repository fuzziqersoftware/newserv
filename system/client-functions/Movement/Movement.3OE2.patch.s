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
  # region @ 801CEDF0 (4 bytes)
  .data     0x801CEDF0  # address
  .data     0x00000004  # size
  .data     0x4800000C  # 801CEDF0 => b         +0x0000000C /* 801CEDFC */
  # region @ 801CFCE0 (4 bytes)
  .data     0x801CFCE0  # address
  .data     0x00000004  # size
  .data     0x48000014  # 801CFCE0 => b         +0x00000014 /* 801CFCF4 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
