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
  # region @ 80351B44 (152 bytes)
  .data     0x80351B44  # address
  .data     0x00000098  # size
  .data     0x880300EE  # 80351B44 => lbz       r0, [r3 + 0x00EE]
  .data     0x2800000B  # 80351B48 => cmplwi    r0, 11
  .data     0x40820144  # 80351B4C => bne       +0x00000144 /* 80351C90 */
  .data     0x83E300F0  # 80351B50 => lwz       r31, [r3 + 0x00F0]
  .data     0x38000000  # 80351B54 => li        r0, 0x0000
  .data     0x60000000  # 80351B58 => nop
  .data     0x38800374  # 80351B5C => li        r4, 0x0374
  .data     0x38A00D38  # 80351B60 => li        r5, 0x0D38
  .data     0x48000059  # 80351B64 => bl        +0x00000058 /* 80351BBC */
  .data     0x38A00D3A  # 80351B68 => li        r5, 0x0D3A
  .data     0x48000051  # 80351B6C => bl        +0x00000050 /* 80351BBC */
  .data     0x38A00D3C  # 80351B70 => li        r5, 0x0D3C
  .data     0x48000049  # 80351B74 => bl        +0x00000048 /* 80351BBC */
  .data     0x38A00D40  # 80351B78 => li        r5, 0x0D40
  .data     0x48000041  # 80351B7C => bl        +0x00000040 /* 80351BBC */
  .data     0x38A00D44  # 80351B80 => li        r5, 0x0D44
  .data     0x48000039  # 80351B84 => bl        +0x00000038 /* 80351BBC */
  .data     0x7FE3FB78  # 80351B88 => mr        r3, r31
  .data     0x4BE646F1  # 80351B8C => bl        -0x0019B910 /* 801B627C */
  .data     0xA01F032C  # 80351B90 => lhz       r0, [r31 + 0x032C]
  .data     0xA07F02B8  # 80351B94 => lhz       r3, [r31 + 0x02B8]
  .data     0x7C001840  # 80351B98 => cmpl      r0, r3
  .data     0x40810008  # 80351B9C => ble       +0x00000008 /* 80351BA4 */
  .data     0xB07F032C  # 80351BA0 => sth       [r31 + 0x032C], r3
  .data     0xA01F032E  # 80351BA4 => lhz       r0, [r31 + 0x032E]
  .data     0xA07F02BA  # 80351BA8 => lhz       r3, [r31 + 0x02BA]
  .data     0x7C001840  # 80351BAC => cmpl      r0, r3
  .data     0x40810008  # 80351BB0 => ble       +0x00000008 /* 80351BB8 */
  .data     0xB07F032E  # 80351BB4 => sth       [r31 + 0x032E], r3
  .data     0x480000D8  # 80351BB8 => b         +0x000000D8 /* 80351C90 */
  .data     0x7CDF20AE  # 80351BBC => lbzx      r6, [r31 + r4]
  .data     0x7CFF2A2E  # 80351BC0 => lhzx      r7, [r31 + r5]
  .data     0x54C6083C  # 80351BC4 => rlwinm    r6, r6, 1, 0, 30
  .data     0x7CE63850  # 80351BC8 => subf      r7, r6, r7
  .data     0x7CFF2B2E  # 80351BCC => sthx      [r31 + r5], r7
  .data     0x7C1F21AE  # 80351BD0 => stbx      [r31 + r4], r0
  .data     0x38840001  # 80351BD4 => addi      r4, r4, 0x0001
  .data     0x4E800020  # 80351BD8 => blr
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
