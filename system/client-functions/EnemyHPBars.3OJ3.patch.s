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
  # region @ 80261E9C (4 bytes)
  .data     0x80261E9C  # address
  .data     0x00000004  # size
  .data     0x4BFE1349  # 80261E9C => bl        -0x0001ECB8 /* 802431E4 */
  # region @ 804CE590 (4 bytes)
  .data     0x804CE590  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804CE590 => bc        19, 4, +0x00000000 /* 804CE590 */
  # region @ 804CE59C (4 bytes)
  .data     0x804CE59C  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804CE59C => bc        19, 4, +0x00000000 /* 804CE59C */
  # region @ 804CE5A8 (4 bytes)
  .data     0x804CE5A8  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804CE5A8 => bc        19, 4, +0x00000000 /* 804CE5A8 */
  # region @ 804CE5B4 (4 bytes)
  .data     0x804CE5B4  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804CE5B4 => bc        19, 4, +0x00000000 /* 804CE5B4 */
  # region @ 804CE650 (4 bytes)
  .data     0x804CE650  # address
  .data     0x00000004  # size
  .data     0x42300000  # 804CE650 => bdnz      cr4, +0x00000000 /* 804CE650 */
  # region @ 804CE66C (4 bytes)
  .data     0x804CE66C  # address
  .data     0x00000004  # size
  .data     0xFF00FF15  # 804CE66C => .invalid  FC, 0
  # region @ 805D65BC (4 bytes)
  .data     0x805D65BC  # address
  .data     0x00000004  # size
  .data     0x42A00000  # 805D65BC => b         +0x00000000 /* 805D65BC */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
