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
  # region @ 80351638 (152 bytes)
  .data     0x80351638  # address
  .data     0x00000098  # size
  .data     0x880300EE  # 80351638 => lbz       r0, [r3 + 0x00EE]  # data1_2
  .data     0x2800000B  # 8035163C => cmplwi    r0, 11
  .data     0x40820144  # 80351640 => bne       +0x00000144 /* 80351784 */
  .data     0x83E300F0  # 80351644 => lwz       r31, [r3 + 0x00F0]  # r31 = owner_player
  .data     0x38000000  # 80351648 => li        r0, 0x0000
  .data     0x60000000  # 8035164C => nop
  .data     0x38800374  # 80351650 => li        r4, 0x0374  # material_usage
  .data     0x38A00D38  # 80351654 => li        r5, 0x0D38  # stats.char_stats.atp
  .data     0x48000059  # 80351658 => bl        +0x00000058 /* 803516B0 */
  .data     0x38A00D3A  # 8035165C => li        r5, 0x0D3A  # stats.char_stats.mst
  .data     0x48000051  # 80351660 => bl        +0x00000050 /* 803516B0 */
  .data     0x38A00D3C  # 80351664 => li        r5, 0x0D3C  # stats.char_stats.evp
  .data     0x48000049  # 80351668 => bl        +0x00000048 /* 803516B0 */
  .data     0x38A00D40  # 8035166C => li        r5, 0x0D40  # stats.char_stats.dfp
  .data     0x48000041  # 80351670 => bl        +0x00000040 /* 803516B0 */
  .data     0x38A00D44  # 80351674 => li        r5, 0x0D44  # stats.char_stats.lck
  .data     0x48000039  # 80351678 => bl        +0x00000038 /* 803516B0 */
  .data     0x7FE3FB78  # 8035167C => mr        r3, r31
  .data     0x4BE64B95  # 80351680 => bl        -0x0019B46C /* 801B6214 */
  .data     0xA01F032C  # 80351684 => lhz       r0, [r31 + 0x032C]
  .data     0xA07F02B8  # 80351688 => lhz       r3, [r31 + 0x02B8]
  .data     0x7C001840  # 8035168C => cmpl      r0, r3
  .data     0x40810008  # 80351690 => ble       +0x00000008 /* 80351698 */
  .data     0xB07F032C  # 80351694 => sth       [r31 + 0x032C], r3
  .data     0xA01F032E  # 80351698 => lhz       r0, [r31 + 0x032E]
  .data     0xA07F02BA  # 8035169C => lhz       r3, [r31 + 0x02BA]
  .data     0x7C001840  # 803516A0 => cmpl      r0, r3
  .data     0x40810008  # 803516A4 => ble       +0x00000008 /* 803516AC */
  .data     0xB07F032E  # 803516A8 => sth       [r31 + 0x032E], r3
  .data     0x480000D8  # 803516AC => b         +0x000000D8 /* 80351784 */
  .data     0x7CDF20AE  # 803516B0 => lbzx      r6, [r31 + r4]
  .data     0x7CFF2A2E  # 803516B4 => lhzx      r7, [r31 + r5]
  .data     0x54C6083C  # 803516B8 => rlwinm    r6, r6, 1, 0, 30
  .data     0x7CE63850  # 803516BC => subf      r7, r6, r7
  .data     0x7CFF2B2E  # 803516C0 => sthx      [r31 + r5], r7
  .data     0x7C1F21AE  # 803516C4 => stbx      [r31 + r4], r0
  .data     0x38840001  # 803516C8 => addi      r4, r4, 0x0001
  .data     0x4E800020  # 803516CC => blr
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
