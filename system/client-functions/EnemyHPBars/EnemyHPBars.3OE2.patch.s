.meta name="Enemy HP bars"
.meta description="Shows HP bars in\nenemy info windows"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC
  # region @ 8000B650 (108 bytes)
  .data     0x8000B650  # address
  .data     0x0000006C  # size
  .data     0x3CA08001  # 8000B650 => lis       r5, 0x8001
  .data     0x8065B6BC  # 8000B654 => lwz       r3, [r5 - 0x4944]
  .data     0x7FFEFB78  # 8000B658 => mr        r30, r31
  .data     0xA8DE032C  # 8000B65C => lha       r6, [r30 + 0x032C]
  .data     0x48000010  # 8000B660 => b         +0x00000010 /* 8000B670 */
  .data     0xA8DE02B8  # 8000B664 => lha       r6, [r30 + 0x02B8]
  .data     0x3CA08001  # 8000B668 => lis       r5, 0x8001
  .data     0x9065B6BC  # 8000B66C => stw       [r5 - 0x4944], r3
  .data     0x7C0802A6  # 8000B670 => mflr      r0
  .data     0x9005B6C0  # 8000B674 => stw       [r5 - 0x4940], r0
  .data     0x7C651B78  # 8000B678 => mr        r5, r3
  .data     0xA8FE02B8  # 8000B67C => lha       r7, [r30 + 0x02B8]
  .data     0x3C808000  # 8000B680 => lis       r4, 0x8000
  .data     0x6084B6AC  # 8000B684 => ori       r4, r4, 0xB6AC
  .data     0x38640018  # 8000B688 => addi      r3, r4, 0x0018
  .data     0x4CC63182  # 8000B68C => crxor     crb6, crb6, crb6
  .data     0x4838F295  # 8000B690 => bl        +0x0038F294 /* 8039A924 */
  .data     0x3C808000  # 8000B694 => lis       r4, 0x8000
  .data     0x6084B6C4  # 8000B698 => ori       r4, r4, 0xB6C4
  .data     0x7F83E378  # 8000B69C => mr        r3, r28
  .data     0x8004FFFC  # 8000B6A0 => lwz       r0, [r4 - 0x0004]
  .data     0x7C0803A6  # 8000B6A4 => mtlr      r0
  .data     0x4E800020  # 8000B6A8 => blr
  .data     0x25730A0A  # 8000B6AC => .invalid
  .data     0x48503A25  # 8000B6B0 => bl        +0x00503A24 /* 8050F0D4 */
  .data     0x642F2564  # 8000B6B4 => oris      r15, r1, 0x2564
  .data     0x00000000  # 8000B6B8 => .invalid
  # region @ 80262EF8 (4 bytes)
  .data     0x80262EF8  # address
  .data     0x00000004  # size
  .data     0x4BDA8759  # 80262EF8 => bl        -0x002578A8 /* 8000B650 */
  # region @ 80262F5C (4 bytes)
  .data     0x80262F5C  # address
  .data     0x00000004  # size
  .data     0x4BFE12B1  # 80262F5C => bl        -0x0001ED50 /* 8024420C */
  # region @ 802630B8 (4 bytes)
  .data     0x802630B8  # address
  .data     0x00000004  # size
  .data     0x4BDA85AD  # 802630B8 => bl        -0x00257A54 /* 8000B664 */
  # region @ 804D0158 (4 bytes)
  .data     0x804D0158  # address
  .data     0x00000004  # size
  .data     0x42960000  # 804D0158 => bc        20, 22, +0x00000000 /* 804D0158 */
  # region @ 804D0164 (4 bytes)
  .data     0x804D0164  # address
  .data     0x00000004  # size
  .data     0x42960000  # 804D0164 => bc        20, 22, +0x00000000 /* 804D0164 */
  # region @ 804D0170 (4 bytes)
  .data     0x804D0170  # address
  .data     0x00000004  # size
  .data     0x42960000  # 804D0170 => bc        20, 22, +0x00000000 /* 804D0170 */
  # region @ 804D017C (4 bytes)
  .data     0x804D017C  # address
  .data     0x00000004  # size
  .data     0x42960000  # 804D017C => bc        20, 22, +0x00000000 /* 804D017C */
  # region @ 804D0188 (4 bytes)
  .data     0x804D0188  # address
  .data     0x00000004  # size
  .data     0x42960000  # 804D0188 => bc        20, 22, +0x00000000 /* 804D0188 */
  .data     0x804D01B8
  .data     0x00000004
  .data     0x42960000
  .data     0x804D01E8
  .data     0x00000004
  .data     0x42960000
  # region @ 804D0218 (4 bytes)
  .data     0x804D0218  # address
  .data     0x00000004  # size
  .data     0x42780000  # 804D0218 => bc        19, 24, +0x00000000 /* 804D0218 */
  # region @ 804D0234 (4 bytes)
  .data     0x804D0234  # address
  .data     0x00000004  # size
  .data     0xFF00FF15  # 804D0234 => .invalid  FC, 0
  # region @ 805DD104 (4 bytes)
  .data     0x805DD104  # address
  .data     0x00000004  # size
  .data     0x42C00000  # 805DD104 => b         +0x00000000 /* 805DD104 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
