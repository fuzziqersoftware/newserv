.meta name="Decoction"
.meta description="Makes the Decoction\nitem reset your\nmaterial usage"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC
  # region @ 80352E54 (152 bytes)
  .data     0x80352E54  # address
  .data     0x00000098  # size
  .data     0x880300EE  # 80352E54 => lbz       r0, [r3 + 0x00EE]
  .data     0x2800000B  # 80352E58 => cmplwi    r0, 11
  .data     0x40820144  # 80352E5C => bne       +0x00000144 /* 80352FA0 */
  .data     0x83E300F0  # 80352E60 => lwz       r31, [r3 + 0x00F0]
  .data     0x38000000  # 80352E64 => li        r0, 0x0000
  .data     0x60000000  # 80352E68 => nop
  .data     0x38800374  # 80352E6C => li        r4, 0x0374
  .data     0x38A00D38  # 80352E70 => li        r5, 0x0D38
  .data     0x48000059  # 80352E74 => bl        +0x00000058 /* 80352ECC */
  .data     0x38A00D3A  # 80352E78 => li        r5, 0x0D3A
  .data     0x48000051  # 80352E7C => bl        +0x00000050 /* 80352ECC */
  .data     0x38A00D3C  # 80352E80 => li        r5, 0x0D3C
  .data     0x48000049  # 80352E84 => bl        +0x00000048 /* 80352ECC */
  .data     0x38A00D40  # 80352E88 => li        r5, 0x0D40
  .data     0x48000041  # 80352E8C => bl        +0x00000040 /* 80352ECC */
  .data     0x38A00D44  # 80352E90 => li        r5, 0x0D44
  .data     0x48000039  # 80352E94 => bl        +0x00000038 /* 80352ECC */
  .data     0x7FE3FB78  # 80352E98 => mr        r3, r31
  .data     0x4BE634AD  # 80352E9C => bl        -0x0019CB54 /* 801B6348 */
  .data     0xA01F032C  # 80352EA0 => lhz       r0, [r31 + 0x032C]
  .data     0xA07F02B8  # 80352EA4 => lhz       r3, [r31 + 0x02B8]
  .data     0x7C001840  # 80352EA8 => cmpl      r0, r3
  .data     0x40810008  # 80352EAC => ble       +0x00000008 /* 80352EB4 */
  .data     0xB07F032C  # 80352EB0 => sth       [r31 + 0x032C], r3
  .data     0xA01F032E  # 80352EB4 => lhz       r0, [r31 + 0x032E]
  .data     0xA07F02BA  # 80352EB8 => lhz       r3, [r31 + 0x02BA]
  .data     0x7C001840  # 80352EBC => cmpl      r0, r3
  .data     0x40810008  # 80352EC0 => ble       +0x00000008 /* 80352EC8 */
  .data     0xB07F032E  # 80352EC4 => sth       [r31 + 0x032E], r3
  .data     0x480000D8  # 80352EC8 => b         +0x000000D8 /* 80352FA0 */
  .data     0x7CDF20AE  # 80352ECC => lbzx      r6, [r31 + r4]
  .data     0x7CFF2A2E  # 80352ED0 => lhzx      r7, [r31 + r5]
  .data     0x54C6083C  # 80352ED4 => rlwinm    r6, r6, 1, 0, 30
  .data     0x7CE63850  # 80352ED8 => subf      r7, r6, r7
  .data     0x7CFF2B2E  # 80352EDC => sthx      [r31 + r5], r7
  .data     0x7C1F21AE  # 80352EE0 => stbx      [r31 + r4], r0
  .data     0x38840001  # 80352EE4 => addi      r4, r4, 0x0001
  .data     0x4E800020  # 80352EE8 => blr
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
