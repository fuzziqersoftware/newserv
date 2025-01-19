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
  .data     0x3C60804D  # 8000DFD8 => lis       r3, 0x804D
  .data     0x4E800020  # 8000DFDC => blr
  # region @ 80100B74 (4 bytes)
  .data     0x80100B74  # address
  .data     0x00000004  # size
  .data     0x4BF0D42D  # 80100B74 => bl        -0x000F2BD4 /* 8000DFA0 */
  # region @ 80156C34 (4 bytes)
  .data     0x80156C34  # address
  .data     0x00000004  # size
  .data     0x4BEB7379  # 80156C34 => bl        -0x00148C88 /* 8000DFAC */
  # region @ 801A21A0 (4 bytes)
  .data     0x801A21A0  # address
  .data     0x00000004  # size
  .data     0x4BE6BE1D  # 801A21A0 => bl        -0x001941E4 /* 8000DFBC */
  # region @ 801A23A0 (4 bytes)
  .data     0x801A23A0  # address
  .data     0x00000004  # size
  .data     0x4BE6BC01  # 801A23A0 => bl        -0x00194400 /* 8000DFA0 */
  # region @ 80206640 (4 bytes)
  .data     0x80206640  # address
  .data     0x00000004  # size
  .data     0x4BE07989  # 80206640 => bl        -0x001F8678 /* 8000DFC8 */
  # region @ 80206DE4 (4 bytes)
  .data     0x80206DE4  # address
  .data     0x00000004  # size
  .data     0x4BE071E5  # 80206DE4 => bl        -0x001F8E1C /* 8000DFC8 */
  # region @ 805D9E48 (4 bytes)
  .data     0x805D9E48  # address
  .data     0x00000004  # size
  .data     0x47AFC800  # 805D9E48 => .invalid  sc
  # region @ 805DACF4 (4 bytes)
  .data     0x805DACF4  # address
  .data     0x00000004  # size
  .data     0x47742400  # 805DACF4 => .invalid  sc
  # region @ 805DB31C (4 bytes)
  .data     0x805DB31C  # address
  .data     0x00000004  # size
  .data     0x491C4000  # 805DB31C => b         +0x011C4000 /* 8179F31C */
  # region @ 805DC1A8 (4 bytes)
  .data     0x805DC1A8  # address
  .data     0x00000004  # size
  .data     0x47AFC800  # 805DC1A8 => .invalid  sc
  # region @ 805DC738 (4 bytes)
  .data     0x805DC738  # address
  .data     0x00000004  # size
  .data     0x44AF0000  # 805DC738 => .invalid  sc
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
