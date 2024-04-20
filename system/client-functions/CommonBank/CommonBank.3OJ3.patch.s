.meta name="Common bank"
.meta description="Hold L and open\nthe bank to use a\ncommon bank stored\nin temp character\n3's data"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC
  # region @ 8000BAB4 (156 bytes)
  .data     0x8000BAB4  # address
  .data     0x0000009C  # size
  .data     0x281B0002  # 8000BAB4 => cmplwi    r27, 2
  .data     0x40820018  # 8000BAB8 => bne       +0x00000018 /* 8000BAD0 */
  .data     0x3C008000  # 8000BABC => lis       r0, 0x8000
  .data     0x6000BAD8  # 8000BAC0 => ori       r0, r0, 0xBAD8
  .data     0x90030004  # 8000BAC4 => stw       [r3 + 0x0004], r0
  .data     0x38000000  # 8000BAC8 => li        r0, 0x0000
  .data     0x90030008  # 8000BACC => stw       [r3 + 0x0008], r0
  .data     0x807F0040  # 8000BAD0 => lwz       r3, [r31 + 0x0040]
  .data     0x4E800020  # 8000BAD4 => blr
  .data     0x434F4D4D  # 8000BAD8 => bdzl      cr3, +0x00004D4C /* 80010824 */
  .data     0x4F4E2042  # 8000BADC => crnor     crb26, crb14, crb4
  .data     0x414E4B00  # 8000BAE0 => bc        10, 14, +0x00004B00 /* 800105E0 */
  .data     0x800D8EB0  # 8000BAE4 => lwz       r0, [r13 - 0x7150]
  .data     0x28000001  # 8000BAE8 => cmplwi    r0, 1
  .data     0x40820040  # 8000BAEC => bne       +0x00000040 /* 8000BB2C */
  .data     0x3C808051  # 8000BAF0 => lis       r4, 0x8051
  .data     0xA084C590  # 8000BAF4 => lhz       r4, [r4 - 0x3A70]
  .data     0x70800002  # 8000BAF8 => andi.     r0, r4, 0x0002
  .data     0x41820028  # 8000BAFC => beq       +0x00000028 /* 8000BB24 */
  .data     0x800DB944  # 8000BB00 => lwz       r0, [r13 - 0x46BC]
  .data     0x28000006  # 8000BB04 => cmplwi    r0, 6
  .data     0x4182001C  # 8000BB08 => beq       +0x0000001C /* 8000BB24 */
  .data     0x806DB928  # 8000BB0C => lwz       r3, [r13 - 0x46D8]
  .data     0x28030000  # 8000BB10 => cmplwi    r3, 0
  .data     0x41820010  # 8000BB14 => beq       +0x00000010 /* 8000BB24 */
  .data     0x38000000  # 8000BB18 => li        r0, 0x0000
  .data     0x6000F1B0  # 8000BB1C => ori       r0, r0, 0xF1B0
  .data     0x7C630214  # 8000BB20 => add       r3, r3, r0
  .data     0x3C808001  # 8000BB24 => lis       r4, 0x8001
  .data     0x9064C32C  # 8000BB28 => stw       [r4 - 0x3CD4], r3
  .data     0x28030000  # 8000BB2C => cmplwi    r3, 0
  .data     0x48204894  # 8000BB30 => b         +0x00204894 /* 802103C4 */
  .data     0x800D8EB0  # 8000BB34 => lwz       r0, [r13 - 0x7150]
  .data     0x28000001  # 8000BB38 => cmplwi    r0, 1
  .data     0x4082000C  # 8000BB3C => bne       +0x0000000C /* 8000BB48 */
  .data     0x3C608001  # 8000BB40 => lis       r3, 0x8001
  .data     0x8063C32C  # 8000BB44 => lwz       r3, [r3 - 0x3CD4]
  .data     0x7C681B79  # 8000BB48 => mr.       r8, r3
  .data     0x48204804  # 8000BB4C => b         +0x00204804 /* 80210350 */
  # region @ 8021034C (4 bytes)
  .data     0x8021034C  # address
  .data     0x00000004  # size
  .data     0x4BDFB7E8  # 8021034C => b         -0x00204818 /* 8000BB34 */
  # region @ 802103C0 (4 bytes)
  .data     0x802103C0  # address
  .data     0x00000004  # size
  .data     0x4BDFB724  # 802103C0 => b         -0x002048DC /* 8000BAE4 */
  # region @ 8030BAA4 (4 bytes)
  .data     0x8030BAA4  # address
  .data     0x00000004  # size
  .data     0x4BD00011  # 8030BAA4 => bl        -0x002FFFF0 /* 8000BAB4 */
  # region @ 8030BAFC (4 bytes)
  .data     0x8030BAFC  # address
  .data     0x00000004  # size
  .data     0x4BCFFFB9  # 8030BAFC => bl        -0x00300048 /* 8000BAB4 */
  # region @ 8046FCEC (4 bytes)
  .data     0x8046FCEC  # address
  .data     0x00000004  # size
  .data     0xFFFFFFFF  # 8046FCEC => fnmadd.   f31, f31, f31, f31
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
