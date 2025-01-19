.meta name="Draw Distance"
.meta description="Extends the draw\ndistance of many\nobjects"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC
  # region @ 8000DFA0 (64 bytes)
  .data     0x8000DFA0  # address
  .data     0x00000040  # size
  .data     0xC3C2C200  # 8000DFA0 => lfs       f30, [r2 - 0x3E00]
  .data     0xEFDE0072  # 8000DFA4 => fmuls     f30, f30, f1
  .data     0x4E800020  # 8000DFA8 => blr
  .data     0xC042C200  # 8000DFAC => lfs       f2, [r2 - 0x3E00]
  .data     0xC01E001C  # 8000DFB0 => lfs       f0, [r30 + 0x001C]
  .data     0xEC0000B2  # 8000DFB4 => fmuls     f0, f0, f2
  .data     0x4E800020  # 8000DFB8 => blr
  .data     0xC382C200  # 8000DFBC => lfs       f28, [r2 - 0x3E00]
  .data     0xEF9C00B2  # 8000DFC0 => fmuls     f28, f28, f2
  .data     0x4E800020  # 8000DFC4 => blr
  .data     0xC002C200  # 8000DFC8 => lfs       f0, [r2 - 0x3E00]
  .data     0xC023000C  # 8000DFCC => lfs       f1, [r3 + 0x000C]
  .data     0xEC000072  # 8000DFD0 => fmuls     f0, f0, f1
  .data     0xD003000C  # 8000DFD4 => stfs      [r3 + 0x000C], f0
  .data     0x3C60804D  # 8000DFD8 => lis       r3, 0x804D
  .data     0x4E800020  # 8000DFDC => blr
  # region @ 80100C50 (4 bytes)
  .data     0x80100C50  # address
  .data     0x00000004  # size
  .data     0x4BF0D351  # 80100C50 => bl        -0x000F2CB0 /* 8000DFA0 */
  # region @ 801570BC (4 bytes)
  .data     0x801570BC  # address
  .data     0x00000004  # size
  .data     0x4BEB6EF1  # 801570BC => bl        -0x00149110 /* 8000DFAC */
  # region @ 801A2628 (4 bytes)
  .data     0x801A2628  # address
  .data     0x00000004  # size
  .data     0x4BE6B995  # 801A2628 => bl        -0x0019466C /* 8000DFBC */
  # region @ 801A2828 (4 bytes)
  .data     0x801A2828  # address
  .data     0x00000004  # size
  .data     0x4BE6B779  # 801A2828 => bl        -0x00194888 /* 8000DFA0 */
  # region @ 80206124 (4 bytes)
  .data     0x80206124  # address
  .data     0x00000004  # size
  .data     0x4BE07EA5  # 80206124 => bl        -0x001F815C /* 8000DFC8 */
  # region @ 802068C8 (4 bytes)
  .data     0x802068C8  # address
  .data     0x00000004  # size
  .data     0x4BE07701  # 802068C8 => bl        -0x001F8900 /* 8000DFC8 */
  # region @ 805D5730 (4 bytes)
  .data     0x805D5730  # address
  .data     0x00000004  # size
  .data     0x47AFC800  # 805D5730 => .invalid  sc
  # region @ 805D65DC (4 bytes)
  .data     0x805D65DC  # address
  .data     0x00000004  # size
  .data     0x47742400  # 805D65DC => .invalid  sc
  # region @ 805D6C04 (4 bytes)
  .data     0x805D6C04  # address
  .data     0x00000004  # size
  .data     0x491C4000  # 805D6C04 => b         +0x011C4000 /* 8179AC04 */
  # region @ 805D7A90 (4 bytes)
  .data     0x805D7A90  # address
  .data     0x00000004  # size
  .data     0x47AFC800  # 805D7A90 => .invalid  sc
  # region @ 805D8020 (4 bytes)
  .data     0x805D8020  # address
  .data     0x00000004  # size
  .data     0x44AF0000  # 805D8020 => .invalid  sc
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
