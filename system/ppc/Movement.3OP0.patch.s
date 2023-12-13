.meta name="Movement"
.meta description="Allow backsteps and\nmovement when enemies\nare nearby"
# Original code by Ralf @ GC-Forever

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocks
  # region @ 801CF2AC (4 bytes)
  .data     0x801CF2AC  # address
  .data     0x00000004  # size
  .data     0x4800000C  # 801CF2AC => b         +0x0000000C /* 801CF2B8 */
  # region @ 801D019C (4 bytes)
  .data     0x801D019C  # address
  .data     0x00000004  # size
  .data     0x48000014  # 801D019C => b         +0x00000014 /* 801D01B0 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
