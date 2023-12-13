.meta name="Enemy HP bars"
.meta description="Show HP bars in\nenemy info windows"
# Original code by Ralf @ GC-Forever

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocks
  # region @ 802627A4 (4 bytes)
  .data     0x802627A4  # address
  .data     0x00000004  # size
  .data     0x4BFE12B1  # 802627A4 => bl        -0x0001ED50 /* 80243A54 */
  # region @ 804D0548 (4 bytes)
  .data     0x804D0548  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804D0548 => bc        19, 4, +0x00000000 /* 804D0548 */
  # region @ 804D0554 (4 bytes)
  .data     0x804D0554  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804D0554 => bc        19, 4, +0x00000000 /* 804D0554 */
  # region @ 804D0560 (4 bytes)
  .data     0x804D0560  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804D0560 => bc        19, 4, +0x00000000 /* 804D0560 */
  # region @ 804D056C (4 bytes)
  .data     0x804D056C  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804D056C => bc        19, 4, +0x00000000 /* 804D056C */
  # region @ 804D0608 (4 bytes)
  .data     0x804D0608  # address
  .data     0x00000004  # size
  .data     0x42300000  # 804D0608 => bdnz      cr4, +0x00000000 /* 804D0608 */
  # region @ 804D0624 (4 bytes)
  .data     0x804D0624  # address
  .data     0x00000004  # size
  .data     0xFF00FF15  # 804D0624 => .invalid  FC, 0
  # region @ 805D9344 (4 bytes)
  .data     0x805D9344  # address
  .data     0x00000004  # size
  .data     0x42A00000  # 805D9344 => b         +0x00000000 /* 805D9344 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
