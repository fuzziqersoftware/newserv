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
  # region @ 80100A60 (4 bytes)
  .data     0x80100A60  # address
  .data     0x00000004  # size
  .data     0x4BF0D541  # 80100A60 => bl        -0x000F2AC0 /* 8000DFA0 */
  # region @ 80156BF8 (4 bytes)
  .data     0x80156BF8  # address
  .data     0x00000004  # size
  .data     0x4BEB73B5  # 80156BF8 => bl        -0x00148C4C /* 8000DFAC */
  # region @ 801A2164 (4 bytes)
  .data     0x801A2164  # address
  .data     0x00000004  # size
  .data     0x4BE6BE59  # 801A2164 => bl        -0x001941A8 /* 8000DFBC */
  # region @ 801A2364 (4 bytes)
  .data     0x801A2364  # address
  .data     0x00000004  # size
  .data     0x4BE6BC3D  # 801A2364 => bl        -0x001943C4 /* 8000DFA0 */
  # region @ 80206728 (4 bytes)
  .data     0x80206728  # address
  .data     0x00000004  # size
  .data     0x4BE078A1  # 80206728 => bl        -0x001F8760 /* 8000DFC8 */
  # region @ 80206ECC (4 bytes)
  .data     0x80206ECC  # address
  .data     0x00000004  # size
  .data     0x4BE070FD  # 80206ECC => bl        -0x001F8F04 /* 8000DFC8 */
  # region @ 805D94F0 (4 bytes)
  .data     0x805D94F0  # address
  .data     0x00000004  # size
  .data     0x47AFC800  # 805D94F0 => .invalid  sc
  # region @ 805DA39C (4 bytes)
  .data     0x805DA39C  # address
  .data     0x00000004  # size
  .data     0x47742400  # 805DA39C => .invalid  sc
  # region @ 805DA9C4 (4 bytes)
  .data     0x805DA9C4  # address
  .data     0x00000004  # size
  .data     0x491C4000  # 805DA9C4 => b         +0x011C4000 /* 8179E9C4 */
  # region @ 805DB850 (4 bytes)
  .data     0x805DB850  # address
  .data     0x00000004  # size
  .data     0x47AFC800  # 805DB850 => .invalid  sc
  # region @ 805DBDE0 (4 bytes)
  .data     0x805DBDE0  # address
  .data     0x00000004  # size
  .data     0x44AF0000  # 805DBDE0 => .invalid  sc
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
