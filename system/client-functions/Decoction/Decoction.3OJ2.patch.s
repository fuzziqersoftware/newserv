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
  # region @ 80350740 (152 bytes)
  .data     0x80350740  # address
  .data     0x00000098  # size
  .data     0x880300EE  # 80350740 => lbz       r0, [r3 + 0x00EE]
  .data     0x2800000B  # 80350744 => cmplwi    r0, 11
  .data     0x40820144  # 80350748 => bne       +0x00000144 /* 8035088C */
  .data     0x83E300F0  # 8035074C => lwz       r31, [r3 + 0x00F0]
  .data     0x38000000  # 80350750 => li        r0, 0x0000
  .data     0x60000000  # 80350754 => nop
  .data     0x38800374  # 80350758 => li        r4, 0x0374
  .data     0x38A00D38  # 8035075C => li        r5, 0x0D38
  .data     0x48000059  # 80350760 => bl        +0x00000058 /* 803507B8 */
  .data     0x38A00D3A  # 80350764 => li        r5, 0x0D3A
  .data     0x48000051  # 80350768 => bl        +0x00000050 /* 803507B8 */
  .data     0x38A00D3C  # 8035076C => li        r5, 0x0D3C
  .data     0x48000049  # 80350770 => bl        +0x00000048 /* 803507B8 */
  .data     0x38A00D40  # 80350774 => li        r5, 0x0D40
  .data     0x48000041  # 80350778 => bl        +0x00000040 /* 803507B8 */
  .data     0x38A00D44  # 8035077C => li        r5, 0x0D44
  .data     0x48000039  # 80350780 => bl        +0x00000038 /* 803507B8 */
  .data     0x7FE3FB78  # 80350784 => mr        r3, r31
  .data     0x4BE656A1  # 80350788 => bl        -0x0019A960 /* 801B5E28 */
  .data     0xA01F032C  # 8035078C => lhz       r0, [r31 + 0x032C]
  .data     0xA07F02B8  # 80350790 => lhz       r3, [r31 + 0x02B8]
  .data     0x7C001840  # 80350794 => cmpl      r0, r3
  .data     0x40810008  # 80350798 => ble       +0x00000008 /* 803507A0 */
  .data     0xB07F032C  # 8035079C => sth       [r31 + 0x032C], r3
  .data     0xA01F032E  # 803507A0 => lhz       r0, [r31 + 0x032E]
  .data     0xA07F02BA  # 803507A4 => lhz       r3, [r31 + 0x02BA]
  .data     0x7C001840  # 803507A8 => cmpl      r0, r3
  .data     0x40810008  # 803507AC => ble       +0x00000008 /* 803507B4 */
  .data     0xB07F032E  # 803507B0 => sth       [r31 + 0x032E], r3
  .data     0x480000D8  # 803507B4 => b         +0x000000D8 /* 8035088C */
  .data     0x7CDF20AE  # 803507B8 => lbzx      r6, [r31 + r4]
  .data     0x7CFF2A2E  # 803507BC => lhzx      r7, [r31 + r5]
  .data     0x54C6083C  # 803507C0 => rlwinm    r6, r6, 1, 0, 30
  .data     0x7CE63850  # 803507C4 => subf      r7, r6, r7
  .data     0x7CFF2B2E  # 803507C8 => sthx      [r31 + r5], r7
  .data     0x7C1F21AE  # 803507CC => stbx      [r31 + r4], r0
  .data     0x38840001  # 803507D0 => addi      r4, r4, 0x0001
  .data     0x4E800020  # 803507D4 => blr
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
