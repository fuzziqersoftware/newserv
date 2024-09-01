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
  # region @ 80353220 (152 bytes)
  .data     0x80353220  # address
  .data     0x00000098  # size
  .data     0x880300EE  # 80353220 => lbz       r0, [r3 + 0x00EE]
  .data     0x2800000B  # 80353224 => cmplwi    r0, 11
  .data     0x40820144  # 80353228 => bne       +0x00000144 /* 8035336C */
  .data     0x83E300F0  # 8035322C => lwz       r31, [r3 + 0x00F0]
  .data     0x38000000  # 80353230 => li        r0, 0x0000
  .data     0x60000000  # 80353234 => nop
  .data     0x38800374  # 80353238 => li        r4, 0x0374
  .data     0x38A00D38  # 8035323C => li        r5, 0x0D38
  .data     0x48000059  # 80353240 => bl        +0x00000058 /* 80353298 */
  .data     0x38A00D3A  # 80353244 => li        r5, 0x0D3A
  .data     0x48000051  # 80353248 => bl        +0x00000050 /* 80353298 */
  .data     0x38A00D3C  # 8035324C => li        r5, 0x0D3C
  .data     0x48000049  # 80353250 => bl        +0x00000048 /* 80353298 */
  .data     0x38A00D40  # 80353254 => li        r5, 0x0D40
  .data     0x48000041  # 80353258 => bl        +0x00000040 /* 80353298 */
  .data     0x38A00D44  # 8035325C => li        r5, 0x0D44
  .data     0x48000039  # 80353260 => bl        +0x00000038 /* 80353298 */
  .data     0x7FE3FB78  # 80353264 => mr        r3, r31
  .data     0x4BE63145  # 80353268 => bl        -0x0019CEBC /* 801B63AC */
  .data     0xA01F032C  # 8035326C => lhz       r0, [r31 + 0x032C]
  .data     0xA07F02B8  # 80353270 => lhz       r3, [r31 + 0x02B8]
  .data     0x7C001840  # 80353274 => cmpl      r0, r3
  .data     0x40810008  # 80353278 => ble       +0x00000008 /* 80353280 */
  .data     0xB07F032C  # 8035327C => sth       [r31 + 0x032C], r3
  .data     0xA01F032E  # 80353280 => lhz       r0, [r31 + 0x032E]
  .data     0xA07F02BA  # 80353284 => lhz       r3, [r31 + 0x02BA]
  .data     0x7C001840  # 80353288 => cmpl      r0, r3
  .data     0x40810008  # 8035328C => ble       +0x00000008 /* 80353294 */
  .data     0xB07F032E  # 80353290 => sth       [r31 + 0x032E], r3
  .data     0x480000D8  # 80353294 => b         +0x000000D8 /* 8035336C */
  .data     0x7CDF20AE  # 80353298 => lbzx      r6, [r31 + r4]
  .data     0x7CFF2A2E  # 8035329C => lhzx      r7, [r31 + r5]
  .data     0x54C6083C  # 803532A0 => rlwinm    r6, r6, 1, 0, 30
  .data     0x7CE63850  # 803532A4 => subf      r7, r6, r7
  .data     0x7CFF2B2E  # 803532A8 => sthx      [r31 + r5], r7
  .data     0x7C1F21AE  # 803532AC => stbx      [r31 + r4], r0
  .data     0x38840001  # 803532B0 => addi      r4, r4, 0x0001
  .data     0x4E800020  # 803532B4 => blr
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
