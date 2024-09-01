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
  # region @ 803530A0 (152 bytes)
  .data     0x803530A0  # address
  .data     0x00000098  # size
  .data     0x880300EE  # 803530A0 => lbz       r0, [r3 + 0x00EE]
  .data     0x2800000B  # 803530A4 => cmplwi    r0, 11
  .data     0x40820144  # 803530A8 => bne       +0x00000144 /* 803531EC */
  .data     0x83E300F0  # 803530AC => lwz       r31, [r3 + 0x00F0]
  .data     0x38000000  # 803530B0 => li        r0, 0x0000
  .data     0x60000000  # 803530B4 => nop
  .data     0x38800374  # 803530B8 => li        r4, 0x0374
  .data     0x38A00D38  # 803530BC => li        r5, 0x0D38
  .data     0x48000059  # 803530C0 => bl        +0x00000058 /* 80353118 */
  .data     0x38A00D3A  # 803530C4 => li        r5, 0x0D3A
  .data     0x48000051  # 803530C8 => bl        +0x00000050 /* 80353118 */
  .data     0x38A00D3C  # 803530CC => li        r5, 0x0D3C
  .data     0x48000049  # 803530D0 => bl        +0x00000048 /* 80353118 */
  .data     0x38A00D40  # 803530D4 => li        r5, 0x0D40
  .data     0x48000041  # 803530D8 => bl        +0x00000040 /* 80353118 */
  .data     0x38A00D44  # 803530DC => li        r5, 0x0D44
  .data     0x48000039  # 803530E0 => bl        +0x00000038 /* 80353118 */
  .data     0x7FE3FB78  # 803530E4 => mr        r3, r31
  .data     0x4BE654CD  # 803530E8 => bl        -0x0019AB34 /* 801B85B4 */
  .data     0xA01F032C  # 803530EC => lhz       r0, [r31 + 0x032C]
  .data     0xA07F02B8  # 803530F0 => lhz       r3, [r31 + 0x02B8]
  .data     0x7C001840  # 803530F4 => cmpl      r0, r3
  .data     0x40810008  # 803530F8 => ble       +0x00000008 /* 80353100 */
  .data     0xB07F032C  # 803530FC => sth       [r31 + 0x032C], r3
  .data     0xA01F032E  # 80353100 => lhz       r0, [r31 + 0x032E]
  .data     0xA07F02BA  # 80353104 => lhz       r3, [r31 + 0x02BA]
  .data     0x7C001840  # 80353108 => cmpl      r0, r3
  .data     0x40810008  # 8035310C => ble       +0x00000008 /* 80353114 */
  .data     0xB07F032E  # 80353110 => sth       [r31 + 0x032E], r3
  .data     0x480000D8  # 80353114 => b         +0x000000D8 /* 803531EC */
  .data     0x7CDF20AE  # 80353118 => lbzx      r6, [r31 + r4]
  .data     0x7CFF2A2E  # 8035311C => lhzx      r7, [r31 + r5]
  .data     0x54C6083C  # 80353120 => rlwinm    r6, r6, 1, 0, 30
  .data     0x7CE63850  # 80353124 => subf      r7, r6, r7
  .data     0x7CFF2B2E  # 80353128 => sthx      [r31 + r5], r7
  .data     0x7C1F21AE  # 8035312C => stbx      [r31 + r4], r0
  .data     0x38840001  # 80353130 => addi      r4, r4, 0x0001
  .data     0x4E800020  # 80353134 => blr
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
