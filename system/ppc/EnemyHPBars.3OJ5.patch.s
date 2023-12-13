.meta name="Enemy HP bars"
.meta description="Show HP bars in\nenemy info windows"
# Original code by Ralf @ GC-Forever

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocks
  # region @ 80262C98 (4 bytes)
  .data     0x80262C98  # address
  .data     0x00000004  # size
  .data     0x4BFE1241  # 80262C98 => bl        -0x0001EDC0 /* 80243ED8 */
  # region @ 804D0880 (4 bytes)
  .data     0x804D0880  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804D0880 => bc        19, 4, +0x00000000 /* 804D0880 */
  # region @ 804D088C (4 bytes)
  .data     0x804D088C  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804D088C => bc        19, 4, +0x00000000 /* 804D088C */
  # region @ 804D0898 (4 bytes)
  .data     0x804D0898  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804D0898 => bc        19, 4, +0x00000000 /* 804D0898 */
  # region @ 804D08A4 (4 bytes)
  .data     0x804D08A4  # address
  .data     0x00000004  # size
  .data     0x42640000  # 804D08A4 => bc        19, 4, +0x00000000 /* 804D08A4 */
  # region @ 804D0940 (4 bytes)
  .data     0x804D0940  # address
  .data     0x00000004  # size
  .data     0x42300000  # 804D0940 => bdnz      cr4, +0x00000000 /* 804D0940 */
  # region @ 804D095C (4 bytes)
  .data     0x804D095C  # address
  .data     0x00000004  # size
  .data     0xFF00FF15  # 804D095C => .invalid  FC, 0
  # region @ 805DD7FC (4 bytes)
  .data     0x805DD7FC  # address
  .data     0x00000004  # size
  .data     0x42A00000  # 805DD7FC => b         +0x00000000 /* 805DD7FC */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
