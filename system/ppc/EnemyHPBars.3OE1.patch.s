.meta name="Enemy HP bars"
.meta description="Show HP bars in\nenemy info windows"
# Original code by Ralf @ GC-Forever

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocks
  # region @ 80261B9C (4 bytes)
  .data     0x80261B9C  # address
  .data     0x00000004  # size
  .data     0x4BFE1545  # 80261B9C => bl        -0x0001EABC /* 802430E0 */
  # region @ 804CBAF0 (4 bytes)
  .data     0x804CBAF0  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804CBAF0 => bc        19, 4, +0x00000000 /* 804CBAF0 */
  # region @ 804CBAFC (4 bytes)
  .data     0x804CBAFC  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804CBAFC => bc        19, 4, +0x00000000 /* 804CBAFC */
  # region @ 804CBB08 (4 bytes)
  .data     0x804CBB08  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804CBB08 => bc        19, 4, +0x00000000 /* 804CBB08 */
  # region @ 804CBB14 (4 bytes)
  .data     0x804CBB14  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804CBB14 => bc        19, 4, +0x00000000 /* 804CBB14 */
  # region @ 804CBBB0 (4 bytes)
  .data     0x804CBBB0  # address
  .data     0x00000004  # size
  .data     0x42300000  # 804CBBB0 => bdnz      cr4, +0x00000000 /* 804CBBB0 */
  # region @ 804CBBCC (4 bytes)
  .data     0x804CBBCC  # address
  .data     0x00000004  # size
  .data     0xFF00FF15  # 804CBBCC => .invalid  FC, 0
  # region @ 805D38E4 (4 bytes)
  .data     0x805D38E4  # address
  .data     0x00000004  # size
  .data     0x42A00000  # 805D38E4 => b         +0x00000000 /* 805D38E4 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
