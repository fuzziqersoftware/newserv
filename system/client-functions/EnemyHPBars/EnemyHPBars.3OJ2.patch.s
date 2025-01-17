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
  .data     0x4838A86D  # 8000B690 => bl        +0x0038A86C /* 80395EFC */
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
  # region @ 80261260 (4 bytes)
  .data     0x80261260  # address
  .data     0x00000004  # size
  .data     0x4BDAA3F1  # 80261260 => bl        -0x00255C10 /* 8000B650 */
  # region @ 802612C4 (4 bytes)
  .data     0x802612C4  # address
  .data     0x00000004  # size
  .data     0x4BFE1541  # 802612C4 => bl        -0x0001EAC0 /* 80242804 */
  # region @ 80261420 (4 bytes)
  .data     0x80261420  # address
  .data     0x00000004  # size
  .data     0x4BDAA245  # 80261420 => bl        -0x00255DBC /* 8000B664 */
  # region @ 804CAE40 (4 bytes)
  .data     0x804CAE40  # address
  .data     0x00000004  # size
  .data     0x42960000  # 804CAE40 => bc        20, 22, +0x00000000 /* 804CAE40 */
  # region @ 804CAE4C (4 bytes)
  .data     0x804CAE4C  # address
  .data     0x00000004  # size
  .data     0x42960000  # 804CAE4C => bc        20, 22, +0x00000000 /* 804CAE4C */
  # region @ 804CAE58 (4 bytes)
  .data     0x804CAE58  # address
  .data     0x00000004  # size
  .data     0x42960000  # 804CAE58 => bc        20, 22, +0x00000000 /* 804CAE58 */
  # region @ 804CAE64 (4 bytes)
  .data     0x804CAE64  # address
  .data     0x00000004  # size
  .data     0x42960000  # 804CAE64 => bc        20, 22, +0x00000000 /* 804CAE64 */
  # region @ 804CAE70 (4 bytes)
  .data     0x804CAE70  # address
  .data     0x00000004  # size
  .data     0x42960000  # 804CAE70 => bc        20, 22, +0x00000000 /* 804CAE70 */
  .data     0x804CAEA0
  .data     0x00000004
  .data     0x42960000
  .data     0x804CAED0
  .data     0x00000004
  .data     0x42960000
  # region @ 804CAF00 (4 bytes)
  .data     0x804CAF00  # address
  .data     0x00000004  # size
  .data     0x42780000  # 804CAF00 => bc        19, 24, +0x00000000 /* 804CAF00 */
  # region @ 804CAF1C (4 bytes)
  .data     0x804CAF1C  # address
  .data     0x00000004  # size
  .data     0xFF00FF15  # 804CAF1C => .invalid  FC, 0
  # region @ 805CBFBC (4 bytes)
  .data     0x805CBFBC  # address
  .data     0x00000004  # size
  .data     0x42C00000  # 805CBFBC => b         +0x00000000 /* 805CBFBC */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
