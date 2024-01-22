.meta name="Enemy HP bars"
.meta description="Show HP bars in\nenemy info windows"
# Original code by Ralf @ GC-Forever

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC
  # region @ 80261B9C (4 bytes)
  .data     0x80261B9C  # address
  .data     0x00000004  # size
  .data     0x4BFE1545  # 80261B9C => bl        -0x0001EABC /* 802430E0 */
  # region @ 804CB610 (4 bytes)
  .data     0x804CB610  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804CB610 => bc        19, 4, +0x00000000 /* 804CB610 */
  # region @ 804CB61C (4 bytes)
  .data     0x804CB61C  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804CB61C => bc        19, 4, +0x00000000 /* 804CB61C */
  # region @ 804CB628 (4 bytes)
  .data     0x804CB628  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804CB628 => bc        19, 4, +0x00000000 /* 804CB628 */
  # region @ 804CB634 (4 bytes)
  .data     0x804CB634  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804CB634 => bc        19, 4, +0x00000000 /* 804CB634 */
  # region @ 804CB6D0 (4 bytes)
  .data     0x804CB6D0  # address
  .data     0x00000004  # size
  .data     0x42300000  # 804CB6D0 => bdnz      cr4, +0x00000000 /* 804CB6D0 */
  # region @ 804CB6EC (4 bytes)
  .data     0x804CB6EC  # address
  .data     0x00000004  # size
  .data     0xFF00FF15  # 804CB6EC => .invalid  FC, 0
  # region @ 805CC8C4 (4 bytes)
  .data     0x805CC8C4  # address
  .data     0x00000004  # size
  .data     0x42A00000  # 805CC8C4 => b         +0x00000000 /* 805CC8C4 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
