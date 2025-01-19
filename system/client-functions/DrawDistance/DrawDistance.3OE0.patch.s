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
  .data     0x3C60804C  # 8000DFD8 => lis       r3, 0x804C
  .data     0x4E800020  # 8000DFDC => blr
  # region @ 80100B8C (4 bytes)
  .data     0x80100B8C  # address
  .data     0x00000004  # size
  .data     0x4BF0D415  # 80100B8C => bl        -0x000F2BEC /* 8000DFA0 */
  # region @ 80156AD8 (4 bytes)
  .data     0x80156AD8  # address
  .data     0x00000004  # size
  .data     0x4BEB74D5  # 80156AD8 => bl        -0x00148B2C /* 8000DFAC */
  # region @ 801A2040 (4 bytes)
  .data     0x801A2040  # address
  .data     0x00000004  # size
  .data     0x4BE6BF7D  # 801A2040 => bl        -0x00194084 /* 8000DFBC */
  # region @ 801A2240 (4 bytes)
  .data     0x801A2240  # address
  .data     0x00000004  # size
  .data     0x4BE6BD61  # 801A2240 => bl        -0x001942A0 /* 8000DFA0 */
  # region @ 80205840 (4 bytes)
  .data     0x80205840  # address
  .data     0x00000004  # size
  .data     0x4BE08789  # 80205840 => bl        -0x001F7878 /* 8000DFC8 */
  # region @ 80205FE4 (4 bytes)
  .data     0x80205FE4  # address
  .data     0x00000004  # size
  .data     0x4BE07FE5  # 80205FE4 => bl        -0x001F801C /* 8000DFC8 */
  # region @ 805C8CB0 (4 bytes)
  .data     0x805C8CB0  # address
  .data     0x00000004  # size
  .data     0x47AFC800  # 805C8CB0 => .invalid  sc
  # region @ 805C9B5C (4 bytes)
  .data     0x805C9B5C  # address
  .data     0x00000004  # size
  .data     0x47742400  # 805C9B5C => .invalid  sc
  # region @ 805CA184 (4 bytes)
  .data     0x805CA184  # address
  .data     0x00000004  # size
  .data     0x491C4000  # 805CA184 => b         +0x011C4000 /* 8178E184 */
  # region @ 805CB010 (4 bytes)
  .data     0x805CB010  # address
  .data     0x00000004  # size
  .data     0x47AFC800  # 805CB010 => .invalid  sc
  # region @ 805CB5A0 (4 bytes)
  .data     0x805CB5A0  # address
  .data     0x00000004  # size
  .data     0x44AF0000  # 805CB5A0 => .invalid  sc
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
