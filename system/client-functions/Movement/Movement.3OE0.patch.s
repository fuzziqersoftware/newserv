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
  # region @ 801CEBF0 (4 bytes)
  .data     0x801CEBF0  # address
  .data     0x00000004  # size
  .data     0x4800000C  # 801CEBF0 => b         +0x0000000C /* 801CEBFC */
  # region @ 801CFAE0 (4 bytes)
  .data     0x801CFAE0  # address
  .data     0x00000004  # size
  .data     0x48000014  # 801CFAE0 => b         +0x00000014 /* 801CFAF4 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
