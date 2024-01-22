.meta name="Enemy HP bars"
.meta description="Show HP bars in\nenemy info windows"
# Original code by Ralf @ GC-Forever

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC
  # region @ 80262F5C (4 bytes)
  .data     0x80262F5C  # address
  .data     0x00000004  # size
  .data     0x4BFE12B1  # 80262F5C => bl        -0x0001ED50 /* 8024420C */
  # region @ 804D0158 (4 bytes)
  .data     0x804D0158  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804D0158 => bc        19, 4, +0x00000000 /* 804D0158 */
  # region @ 804D0164 (4 bytes)
  .data     0x804D0164  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804D0164 => bc        19, 4, +0x00000000 /* 804D0164 */
  # region @ 804D0170 (4 bytes)
  .data     0x804D0170  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804D0170 => bc        19, 4, +0x00000000 /* 804D0170 */
  # region @ 804D017C (4 bytes)
  .data     0x804D017C  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804D017C => bc        19, 4, +0x00000000 /* 804D017C */
  # region @ 804D0218 (4 bytes)
  .data     0x804D0218  # address
  .data     0x00000004  # size
  .data     0x42300000  # 804D0218 => bdnz      cr4, +0x00000000 /* 804D0218 */
  # region @ 804D0234 (4 bytes)
  .data     0x804D0234  # address
  .data     0x00000004  # size
  .data     0xFF00FF15  # 804D0234 => .invalid  FC, 0
  # region @ 805DD104 (4 bytes)
  .data     0x805DD104  # address
  .data     0x00000004  # size
  .data     0x42A00000  # 805DD104 => b         +0x00000000 /* 805DD104 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
