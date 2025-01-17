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
  .data     0x4838DD85  # 8000B690 => bl        +0x0038DD84 /* 80399414 */
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
  # region @ 80262740 (4 bytes)
  .data     0x80262740  # address
  .data     0x00000004  # size
  .data     0x4BDA8F11  # 80262740 => bl        -0x002570F0 /* 8000B650 */
  # region @ 802627A4 (4 bytes)
  .data     0x802627A4  # address
  .data     0x00000004  # size
  .data     0x4BFE12B1  # 802627A4 => bl        -0x0001ED50 /* 80243A54 */
  # region @ 80262900 (4 bytes)
  .data     0x80262900  # address
  .data     0x00000004  # size
  .data     0x4BDA8D65  # 80262900 => bl        -0x0025729C /* 8000B664 */
  # region @ 804D0548 (4 bytes)
  .data     0x804D0548  # address
  .data     0x00000004  # size
  .data     0x42960000  # 804D0548 => bc        20, 22, +0x00000000 /* 804D0548 */
  # region @ 804D0554 (4 bytes)
  .data     0x804D0554  # address
  .data     0x00000004  # size
  .data     0x42960000  # 804D0554 => bc        20, 22, +0x00000000 /* 804D0554 */
  # region @ 804D0560 (4 bytes)
  .data     0x804D0560  # address
  .data     0x00000004  # size
  .data     0x42960000  # 804D0560 => bc        20, 22, +0x00000000 /* 804D0560 */
  # region @ 804D056C (4 bytes)
  .data     0x804D056C  # address
  .data     0x00000004  # size
  .data     0x42960000  # 804D056C => bc        20, 22, +0x00000000 /* 804D056C */
  # region @ 804D0578 (4 bytes)
  .data     0x804D0578  # address
  .data     0x00000004  # size
  .data     0x42960000  # 804D0578 => bc        20, 22, +0x00000000 /* 804D0578 */
  .data     0x804D05A8
  .data     0x00000004
  .data     0x42960000
  .data     0x804D05D8
  .data     0x00000004
  .data     0x42960000
  # region @ 804D0608 (4 bytes)
  .data     0x804D0608  # address
  .data     0x00000004  # size
  .data     0x42780000  # 804D0608 => bc        19, 24, +0x00000000 /* 804D0608 */
  # region @ 804D0624 (4 bytes)
  .data     0x804D0624  # address
  .data     0x00000004  # size
  .data     0xFF00FF15  # 804D0624 => .invalid  FC, 0
  # region @ 805D9344 (4 bytes)
  .data     0x805D9344  # address
  .data     0x00000004  # size
  .data     0x42C00000  # 805D9344 => b         +0x00000000 /* 805D9344 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
