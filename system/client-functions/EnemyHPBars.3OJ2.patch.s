.meta name="Enemy HP bars"
.meta description="Show HP bars in\nenemy info windows"
# Original code by Ralf @ GC-Forever

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC
  # region @ 802612C4 (4 bytes)
  .data     0x802612C4  # address
  .data     0x00000004  # size
  .data     0x4BFE1541  # 802612C4 => bl        -0x0001EAC0 /* 80242804 */
  # region @ 804CAE40 (4 bytes)
  .data     0x804CAE40  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804CAE40 => bc        19, 4, +0x00000000 /* 804CAE40 */
  # region @ 804CAE4C (4 bytes)
  .data     0x804CAE4C  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804CAE4C => bc        19, 4, +0x00000000 /* 804CAE4C */
  # region @ 804CAE58 (4 bytes)
  .data     0x804CAE58  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804CAE58 => bc        19, 4, +0x00000000 /* 804CAE58 */
  # region @ 804CAE64 (4 bytes)
  .data     0x804CAE64  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804CAE64 => bc        19, 4, +0x00000000 /* 804CAE64 */
  # region @ 804CAF00 (4 bytes)
  .data     0x804CAF00  # address
  .data     0x00000004  # size
  .data     0x42300000  # 804CAF00 => bdnz      cr4, +0x00000000 /* 804CAF00 */
  # region @ 804CAF1C (4 bytes)
  .data     0x804CAF1C  # address
  .data     0x00000004  # size
  .data     0xFF00FF15  # 804CAF1C => .invalid  FC, 0
  # region @ 805CBFBC (4 bytes)
  .data     0x805CBFBC  # address
  .data     0x00000004  # size
  .data     0x42A00000  # 805CBFBC => b         +0x00000000 /* 805CBFBC */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
