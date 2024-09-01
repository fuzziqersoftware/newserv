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
  # region @ 803515F4 (152 bytes)
  .data     0x803515F4  # address
  .data     0x00000098  # size
  .data     0x880300EE  # 803515F4 => lbz       r0, [r3 + 0x00EE]
  .data     0x2800000B  # 803515F8 => cmplwi    r0, 11
  .data     0x40820144  # 803515FC => bne       +0x00000144 /* 80351740 */
  .data     0x83E300F0  # 80351600 => lwz       r31, [r3 + 0x00F0]
  .data     0x38000000  # 80351604 => li        r0, 0x0000
  .data     0x60000000  # 80351608 => nop
  .data     0x38800374  # 8035160C => li        r4, 0x0374
  .data     0x38A00D38  # 80351610 => li        r5, 0x0D38
  .data     0x48000059  # 80351614 => bl        +0x00000058 /* 8035166C */
  .data     0x38A00D3A  # 80351618 => li        r5, 0x0D3A
  .data     0x48000051  # 8035161C => bl        +0x00000050 /* 8035166C */
  .data     0x38A00D3C  # 80351620 => li        r5, 0x0D3C
  .data     0x48000049  # 80351624 => bl        +0x00000048 /* 8035166C */
  .data     0x38A00D40  # 80351628 => li        r5, 0x0D40
  .data     0x48000041  # 8035162C => bl        +0x00000040 /* 8035166C */
  .data     0x38A00D44  # 80351630 => li        r5, 0x0D44
  .data     0x48000039  # 80351634 => bl        +0x00000038 /* 8035166C */
  .data     0x7FE3FB78  # 80351638 => mr        r3, r31
  .data     0x4BE64BD9  # 8035163C => bl        -0x0019B428 /* 801B6214 */
  .data     0xA01F032C  # 80351640 => lhz       r0, [r31 + 0x032C]
  .data     0xA07F02B8  # 80351644 => lhz       r3, [r31 + 0x02B8]
  .data     0x7C001840  # 80351648 => cmpl      r0, r3
  .data     0x40810008  # 8035164C => ble       +0x00000008 /* 80351654 */
  .data     0xB07F032C  # 80351650 => sth       [r31 + 0x032C], r3
  .data     0xA01F032E  # 80351654 => lhz       r0, [r31 + 0x032E]
  .data     0xA07F02BA  # 80351658 => lhz       r3, [r31 + 0x02BA]
  .data     0x7C001840  # 8035165C => cmpl      r0, r3
  .data     0x40810008  # 80351660 => ble       +0x00000008 /* 80351668 */
  .data     0xB07F032E  # 80351664 => sth       [r31 + 0x032E], r3
  .data     0x480000D8  # 80351668 => b         +0x000000D8 /* 80351740 */
  .data     0x7CDF20AE  # 8035166C => lbzx      r6, [r31 + r4]
  .data     0x7CFF2A2E  # 80351670 => lhzx      r7, [r31 + r5]
  .data     0x54C6083C  # 80351674 => rlwinm    r6, r6, 1, 0, 30
  .data     0x7CE63850  # 80351678 => subf      r7, r6, r7
  .data     0x7CFF2B2E  # 8035167C => sthx      [r31 + r5], r7
  .data     0x7C1F21AE  # 80351680 => stbx      [r31 + r4], r0
  .data     0x38840001  # 80351684 => addi      r4, r4, 0x0001
  .data     0x4E800020  # 80351688 => blr
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
