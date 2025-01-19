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
  .data     0xC3C2C1F8  # 8000DFA0 => lfs       f30, [r2 - 0x3E08]
  .data     0xEFDE0072  # 8000DFA4 => fmuls     f30, f30, f1
  .data     0x4E800020  # 8000DFA8 => blr
  .data     0xC042C1F8  # 8000DFAC => lfs       f2, [r2 - 0x3E08]
  .data     0xC01E001C  # 8000DFB0 => lfs       f0, [r30 + 0x001C]
  .data     0xEC0000B2  # 8000DFB4 => fmuls     f0, f0, f2
  .data     0x4E800020  # 8000DFB8 => blr
  .data     0xC382C1F8  # 8000DFBC => lfs       f28, [r2 - 0x3E08]
  .data     0xEF9C00B2  # 8000DFC0 => fmuls     f28, f28, f2
  .data     0x4E800020  # 8000DFC4 => blr
  .data     0xC002C1F8  # 8000DFC8 => lfs       f0, [r2 - 0x3E08]
  .data     0xC023000C  # 8000DFCC => lfs       f1, [r3 + 0x000C]
  .data     0xEC000072  # 8000DFD0 => fmuls     f0, f0, f1
  .data     0xD003000C  # 8000DFD4 => stfs      [r3 + 0x000C], f0
  .data     0x3C60804C  # 8000DFD8 => lis       r3, 0x804C
  .data     0x4E800020  # 8000DFDC => blr
  # region @ 801008E8 (4 bytes)
  .data     0x801008E8  # address
  .data     0x00000004  # size
  .data     0x4BF0D6B9  # 801008E8 => bl        -0x000F2948 /* 8000DFA0 */
  # region @ 8015671C (4 bytes)
  .data     0x8015671C  # address
  .data     0x00000004  # size
  .data     0x4BEB7891  # 8015671C => bl        -0x00148770 /* 8000DFAC */
  # region @ 801A1C64 (4 bytes)
  .data     0x801A1C64  # address
  .data     0x00000004  # size
  .data     0x4BE6C359  # 801A1C64 => bl        -0x00193CA8 /* 8000DFBC */
  # region @ 801A1E64 (4 bytes)
  .data     0x801A1E64  # address
  .data     0x00000004  # size
  .data     0x4BE6C13D  # 801A1E64 => bl        -0x00193EC4 /* 8000DFA0 */
  # region @ 80205044 (4 bytes)
  .data     0x80205044  # address
  .data     0x00000004  # size
  .data     0x4BE08F85  # 80205044 => bl        -0x001F707C /* 8000DFC8 */
  # region @ 802057E8 (4 bytes)
  .data     0x802057E8  # address
  .data     0x00000004  # size
  .data     0x4BE087E1  # 802057E8 => bl        -0x001F7820 /* 8000DFC8 */
  # region @ 805C83A8 (4 bytes)
  .data     0x805C83A8  # address
  .data     0x00000004  # size
  .data     0x47AFC800  # 805C83A8 => .invalid  sc
  # region @ 805C9254 (4 bytes)
  .data     0x805C9254  # address
  .data     0x00000004  # size
  .data     0x47742400  # 805C9254 => .invalid  sc
  # region @ 805C987C (4 bytes)
  .data     0x805C987C  # address
  .data     0x00000004  # size
  .data     0x491C4000  # 805C987C => b         +0x011C4000 /* 8178D87C */
  # region @ 805CA708 (4 bytes)
  .data     0x805CA708  # address
  .data     0x00000004  # size
  .data     0x47AFC800  # 805CA708 => .invalid  sc
  # region @ 805CAC98 (4 bytes)
  .data     0x805CAC98  # address
  .data     0x00000004  # size
  .data     0x44AF0000  # 805CAC98 => .invalid  sc
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
