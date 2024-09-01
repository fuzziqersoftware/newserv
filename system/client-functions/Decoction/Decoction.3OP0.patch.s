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
  # region @ 80352614 (152 bytes)
  .data     0x80352614  # address
  .data     0x00000098  # size
  .data     0x880300EE  # 80352614 => lbz       r0, [r3 + 0x00EE]
  .data     0x2800000B  # 80352618 => cmplwi    r0, 11
  .data     0x40820144  # 8035261C => bne       +0x00000144 /* 80352760 */
  .data     0x83E300F0  # 80352620 => lwz       r31, [r3 + 0x00F0]
  .data     0x38000000  # 80352624 => li        r0, 0x0000
  .data     0x60000000  # 80352628 => nop
  .data     0x38800374  # 8035262C => li        r4, 0x0374
  .data     0x38A00D38  # 80352630 => li        r5, 0x0D38
  .data     0x48000059  # 80352634 => bl        +0x00000058 /* 8035268C */
  .data     0x38A00D3A  # 80352638 => li        r5, 0x0D3A
  .data     0x48000051  # 8035263C => bl        +0x00000050 /* 8035268C */
  .data     0x38A00D3C  # 80352640 => li        r5, 0x0D3C
  .data     0x48000049  # 80352644 => bl        +0x00000048 /* 8035268C */
  .data     0x38A00D40  # 80352648 => li        r5, 0x0D40
  .data     0x48000041  # 8035264C => bl        +0x00000040 /* 8035268C */
  .data     0x38A00D44  # 80352650 => li        r5, 0x0D44
  .data     0x48000039  # 80352654 => bl        +0x00000038 /* 8035268C */
  .data     0x7FE3FB78  # 80352658 => mr        r3, r31
  .data     0x4BE6420D  # 8035265C => bl        -0x0019BDF4 /* 801B6868 */
  .data     0xA01F032C  # 80352660 => lhz       r0, [r31 + 0x032C]
  .data     0xA07F02B8  # 80352664 => lhz       r3, [r31 + 0x02B8]
  .data     0x7C001840  # 80352668 => cmpl      r0, r3
  .data     0x40810008  # 8035266C => ble       +0x00000008 /* 80352674 */
  .data     0xB07F032C  # 80352670 => sth       [r31 + 0x032C], r3
  .data     0xA01F032E  # 80352674 => lhz       r0, [r31 + 0x032E]
  .data     0xA07F02BA  # 80352678 => lhz       r3, [r31 + 0x02BA]
  .data     0x7C001840  # 8035267C => cmpl      r0, r3
  .data     0x40810008  # 80352680 => ble       +0x00000008 /* 80352688 */
  .data     0xB07F032E  # 80352684 => sth       [r31 + 0x032E], r3
  .data     0x480000D8  # 80352688 => b         +0x000000D8 /* 80352760 */
  .data     0x7CDF20AE  # 8035268C => lbzx      r6, [r31 + r4]
  .data     0x7CFF2A2E  # 80352690 => lhzx      r7, [r31 + r5]
  .data     0x54C6083C  # 80352694 => rlwinm    r6, r6, 1, 0, 30
  .data     0x7CE63850  # 80352698 => subf      r7, r6, r7
  .data     0x7CFF2B2E  # 8035269C => sthx      [r31 + r5], r7
  .data     0x7C1F21AE  # 803526A0 => stbx      [r31 + r4], r0
  .data     0x38840001  # 803526A4 => addi      r4, r4, 0x0001
  .data     0x4E800020  # 803526A8 => blr
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
