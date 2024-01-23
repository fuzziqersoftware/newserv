.meta name="Enemy HP bars"
.meta description="Show HP bars in\nenemy info windows"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC
  # region @ 80262EE4 (4 bytes)
  .data     0x80262EE4  # address
  .data     0x00000004  # size
  .data     0x4BFE0665  # 80262EE4 => bl        -0x0001F99C /* 80243548 */
  # region @ 804D0AE0 (4 bytes)
  .data     0x804D0AE0  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804D0AE0 => bc        19, 4, +0x00000000 /* 804D0AE0 */
  # region @ 804D0AEC (4 bytes)
  .data     0x804D0AEC  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804D0AEC => bc        19, 4, +0x00000000 /* 804D0AEC */
  # region @ 804D0AF8 (4 bytes)
  .data     0x804D0AF8  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804D0AF8 => bc        19, 4, +0x00000000 /* 804D0AF8 */
  # region @ 804D0B04 (4 bytes)
  .data     0x804D0B04  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804D0B04 => bc        19, 4, +0x00000000 /* 804D0B04 */
  # region @ 804D0BA0 (4 bytes)
  .data     0x804D0BA0  # address
  .data     0x00000004  # size
  .data     0x42300000  # 804D0BA0 => bdnz      cr4, +0x00000000 /* 804D0BA0 */
  # region @ 804D0BBC (4 bytes)
  .data     0x804D0BBC  # address
  .data     0x00000004  # size
  .data     0xFF00FF15  # 804D0BBC => .invalid  FC, 0
  # region @ 805DDA5C (4 bytes)
  .data     0x805DDA5C  # address
  .data     0x00000004  # size
  .data     0x42A00000  # 805DDA5C => b         +0x00000000 /* 805DDA5C */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
