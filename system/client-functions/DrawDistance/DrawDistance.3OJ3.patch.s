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
  # region @ 80100AD0 (4 bytes)
  .data     0x80100AD0  # address
  .data     0x00000004  # size
  .data     0x4BF0D4D1  # 80100AD0 => bl        -0x000F2B30 /* 8000DFA0 */
  # region @ 80156AD0 (4 bytes)
  .data     0x80156AD0  # address
  .data     0x00000004  # size
  .data     0x4BEB74DD  # 80156AD0 => bl        -0x00148B24 /* 8000DFAC */
  # region @ 801A203C (4 bytes)
  .data     0x801A203C  # address
  .data     0x00000004  # size
  .data     0x4BE6BF81  # 801A203C => bl        -0x00194080 /* 8000DFBC */
  # region @ 801A223C (4 bytes)
  .data     0x801A223C  # address
  .data     0x00000004  # size
  .data     0x4BE6BD65  # 801A223C => bl        -0x0019429C /* 8000DFA0 */
  # region @ 802058B8 (4 bytes)
  .data     0x802058B8  # address
  .data     0x00000004  # size
  .data     0x4BE08711  # 802058B8 => bl        -0x001F78F0 /* 8000DFC8 */
  # region @ 8020605C (4 bytes)
  .data     0x8020605C  # address
  .data     0x00000004  # size
  .data     0x4BE07F6D  # 8020605C => bl        -0x001F8094 /* 8000DFC8 */
  # region @ 805D29A8 (4 bytes)
  .data     0x805D29A8  # address
  .data     0x00000004  # size
  .data     0x47AFC800  # 805D29A8 => .invalid  sc
  # region @ 805D3854 (4 bytes)
  .data     0x805D3854  # address
  .data     0x00000004  # size
  .data     0x47742400  # 805D3854 => .invalid  sc
  # region @ 805D3E7C (4 bytes)
  .data     0x805D3E7C  # address
  .data     0x00000004  # size
  .data     0x491C4000  # 805D3E7C => b         +0x011C4000 /* 81797E7C */
  # region @ 805D4D08 (4 bytes)
  .data     0x805D4D08  # address
  .data     0x00000004  # size
  .data     0x47AFC800  # 805D4D08 => .invalid  sc
  # region @ 805D5298 (4 bytes)
  .data     0x805D5298  # address
  .data     0x00000004  # size
  .data     0x44AF0000  # 805D5298 => .invalid  sc
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
