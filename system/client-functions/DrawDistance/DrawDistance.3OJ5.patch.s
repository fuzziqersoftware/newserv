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
  # region @ 80100A50 (4 bytes)
  .data     0x80100A50  # address
  .data     0x00000004  # size
  .data     0x4BF0D551  # 80100A50 => bl        -0x000F2AB0 /* 8000DFA0 */
  # region @ 80156B94 (4 bytes)
  .data     0x80156B94  # address
  .data     0x00000004  # size
  .data     0x4BEB7419  # 80156B94 => bl        -0x00148BE8 /* 8000DFAC */
  # region @ 801A2100 (4 bytes)
  .data     0x801A2100  # address
  .data     0x00000004  # size
  .data     0x4BE6BEBD  # 801A2100 => bl        -0x00194144 /* 8000DFBC */
  # region @ 801A2300 (4 bytes)
  .data     0x801A2300  # address
  .data     0x00000004  # size
  .data     0x4BE6BCA1  # 801A2300 => bl        -0x00194360 /* 8000DFA0 */
  # region @ 802063F4 (4 bytes)
  .data     0x802063F4  # address
  .data     0x00000004  # size
  .data     0x4BE07BD5  # 802063F4 => bl        -0x001F842C /* 8000DFC8 */
  # region @ 80206B98 (4 bytes)
  .data     0x80206B98  # address
  .data     0x00000004  # size
  .data     0x4BE07431  # 80206B98 => bl        -0x001F8BD0 /* 8000DFC8 */
  # region @ 805D9BE8 (4 bytes)
  .data     0x805D9BE8  # address
  .data     0x00000004  # size
  .data     0x47AFC800  # 805D9BE8 => .invalid  sc
  # region @ 805DAA94 (4 bytes)
  .data     0x805DAA94  # address
  .data     0x00000004  # size
  .data     0x47742400  # 805DAA94 => .invalid  sc
  # region @ 805DB0BC (4 bytes)
  .data     0x805DB0BC  # address
  .data     0x00000004  # size
  .data     0x491C4000  # 805DB0BC => b         +0x011C4000 /* 8179F0BC */
  # region @ 805DBF48 (4 bytes)
  .data     0x805DBF48  # address
  .data     0x00000004  # size
  .data     0x47AFC800  # 805DBF48 => .invalid  sc
  # region @ 805DC4D8 (4 bytes)
  .data     0x805DC4D8  # address
  .data     0x00000004  # size
  .data     0x44AF0000  # 805DC4D8 => .invalid  sc
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
