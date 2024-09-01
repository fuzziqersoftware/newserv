.meta name="Bug fixes"
.meta description="Fixes many minor\ngameplay, sound,\nand graphical bugs"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC
  # region @ 8000B088 (88 bytes)
  .data     0x8000B088  # address
  .data     0x00000058  # size
  .data     0x7FA3EB78  # 8000B088 => mr        r3, r29
  .data     0x38800000  # 8000B08C => li        r4, 0x0000
  .data     0x481B1C09  # 8000B090 => bl        +0x001B1C08 /* 801BCC98 */
  .data     0x7FA3EB78  # 8000B094 => mr        r3, r29
  .data     0x481B1ED8  # 8000B098 => b         +0x001B1ED8 /* 801BCF70 */
  .data     0x881F0000  # 8000B09C => lbz       r0, [r31]
  .data     0x28090001  # 8000B0A0 => cmplwi    r9, 1
  .data     0x4082000C  # 8000B0A4 => bne       +0x0000000C /* 8000B0B0 */
  .data     0x881F0001  # 8000B0A8 => lbz       r0, [r31 + 0x0001]
  .data     0x3BFF0002  # 8000B0AC => addi      r31, r31, 0x0002
  .data     0x48100B58  # 8000B0B0 => b         +0x00100B58 /* 8010BC08 */
  .data     0x39200000  # 8000B0B4 => li        r9, 0x0000
  .data     0x48100AE9  # 8000B0B8 => bl        +0x00100AE8 /* 8010BBA0 */
  .data     0x7F43D378  # 8000B0BC => mr        r3, r26
  .data     0x7F64DB78  # 8000B0C0 => mr        r4, r27
  .data     0x7F85E378  # 8000B0C4 => mr        r5, r28
  .data     0x7FA6EB78  # 8000B0C8 => mr        r6, r29
  .data     0x7FC7F378  # 8000B0CC => mr        r7, r30
  .data     0x7FE8FB78  # 8000B0D0 => mr        r8, r31
  .data     0x39200001  # 8000B0D4 => li        r9, 0x0001
  .data     0x48100AC9  # 8000B0D8 => bl        +0x00100AC8 /* 8010BBA0 */
  .data     0x4810300C  # 8000B0DC => b         +0x0010300C /* 8010E0E8 */
  # region @ 8000B5C8 (20 bytes)
  .data     0x8000B5C8  # address
  .data     0x00000014  # size
  .data     0x80630098  # 8000B5C8 => lwz       r3, [r3 + 0x0098]
  .data     0x483D8F71  # 8000B5CC => bl        +0x003D8F70 /* 803E453C */
  .data     0x807F042C  # 8000B5D0 => lwz       r3, [r31 + 0x042C]
  .data     0x809F0430  # 8000B5D4 => lwz       r4, [r31 + 0x0430]
  .data     0x48178DEC  # 8000B5D8 => b         +0x00178DEC /* 801843C4 */
  # region @ 8000BBD0 (32 bytes)
  .data     0x8000BBD0  # address
  .data     0x00000020  # size
  .data     0x809F0370  # 8000BBD0 => lwz       r4, [r31 + 0x0370]
  .data     0x3884FC00  # 8000BBD4 => subi      r4, r4, 0x0400
  .data     0x909F0370  # 8000BBD8 => stw       [r31 + 0x0370], r4
  .data     0x807F0014  # 8000BBDC => lwz       r3, [r31 + 0x0014]
  .data     0x28030000  # 8000BBE0 => cmplwi    r3, 0
  .data     0x41820008  # 8000BBE4 => beq       +0x00000008 /* 8000BBEC */
  .data     0x90830060  # 8000BBE8 => stw       [r3 + 0x0060], r4
  .data     0x48165678  # 8000BBEC => b         +0x00165678 /* 80171264 */
  # region @ 8000C3F8 (124 bytes)
  .data     0x8000C3F8  # address
  .data     0x0000007C  # size
  .data     0x28040000  # 8000C3F8 => cmplwi    r4, 0
  .data     0x4D820020  # 8000C3FC => beqlr
  .data     0x9421FFF0  # 8000C400 => stwu      [r1 - 0x0010], r1
  .data     0x481B0898  # 8000C404 => b         +0x001B0898 /* 801BCC9C */
  .data     0x9421FFE0  # 8000C408 => stwu      [r1 - 0x0020], r1
  .data     0x7C0802A6  # 8000C40C => mflr      r0
  .data     0x90010024  # 8000C410 => stw       [r1 + 0x0024], r0
  .data     0xBF410008  # 8000C414 => stmw      [r1 + 0x0008], r26
  .data     0x7C7F1B78  # 8000C418 => mr        r31, r3
  .data     0x4BFFFFDD  # 8000C41C => bl        -0x00000024 /* 8000C3F8 */
  .data     0x3BC00000  # 8000C420 => li        r30, 0x0000
  .data     0x3BBF0D04  # 8000C424 => addi      r29, r31, 0x0D04
  .data     0x837F032C  # 8000C428 => lwz       r27, [r31 + 0x032C]
  .data     0x839D0000  # 8000C42C => lwz       r28, [r29]
  .data     0x7F83E379  # 8000C430 => mr.       r3, r28
  .data     0x41820018  # 8000C434 => beq       +0x00000018 /* 8000C44C */
  .data     0x38800001  # 8000C438 => li        r4, 0x0001
  .data     0x480FEDC9  # 8000C43C => bl        +0x000FEDC8 /* 8010B204 */
  .data     0x7F83E378  # 8000C440 => mr        r3, r28
  .data     0x38800001  # 8000C444 => li        r4, 0x0001
  .data     0x480FEF49  # 8000C448 => bl        +0x000FEF48 /* 8010B390 */
  .data     0x3BBD0004  # 8000C44C => addi      r29, r29, 0x0004
  .data     0x3BDE0001  # 8000C450 => addi      r30, r30, 0x0001
  .data     0x2C1E000D  # 8000C454 => cmpwi     r30, 13
  .data     0x4180FFD4  # 8000C458 => blt       -0x0000002C /* 8000C42C */
  .data     0x937F032C  # 8000C45C => stw       [r31 + 0x032C], r27
  .data     0xBB410008  # 8000C460 => lmw       r26, [r1 + 0x0008]
  .data     0x80010024  # 8000C464 => lwz       r0, [r1 + 0x0024]
  .data     0x7C0803A6  # 8000C468 => mtlr      r0
  .data     0x38210020  # 8000C46C => addi      r1, r1, 0x0020
  .data     0x4E800020  # 8000C470 => blr
  # region @ 8000C640 (20 bytes)
  .data     0x8000C640  # address
  .data     0x00000014  # size
  .data     0x54800673  # 8000C640 => rlwinm.   r0, r4, 0, 25, 25
  .data     0x41820008  # 8000C644 => beq       +0x00000008 /* 8000C64C */
  .data     0x38800000  # 8000C648 => li        r4, 0x0000
  .data     0x38040009  # 8000C64C => addi      r0, r4, 0x0009
  .data     0x4810C98C  # 8000C650 => b         +0x0010C98C /* 80118FDC */
  # region @ 8000C6D0 (32 bytes)
  .data     0x8000C6D0  # address
  .data     0x00000020  # size
  .data     0x38000001  # 8000C6D0 => li        r0, 0x0001
  .data     0x901D0054  # 8000C6D4 => stw       [r29 + 0x0054], r0
  .data     0x807D0024  # 8000C6D8 => lwz       r3, [r29 + 0x0024]
  .data     0x48212210  # 8000C6DC => b         +0x00212210 /* 8021E8EC */
  .data     0x38000001  # 8000C6E0 => li        r0, 0x0001
  .data     0x901F0378  # 8000C6E4 => stw       [r31 + 0x0378], r0
  .data     0x807F0024  # 8000C6E8 => lwz       r3, [r31 + 0x0024]
  .data     0x482156C0  # 8000C6EC => b         +0x002156C0 /* 80221DAC */
  # region @ 8000C8A0 (20 bytes)
  .data     0x8000C8A0  # address
  .data     0x00000014  # size
  .data     0x1C00000A  # 8000C8A0 => mulli     r0, r0, 10
  .data     0x57E407BD  # 8000C8A4 => rlwinm.   r4, r31, 0, 30, 30
  .data     0x41820008  # 8000C8A8 => beq       +0x00000008 /* 8000C8B0 */
  .data     0x7FA00734  # 8000C8AC => extsh     r0, r29
  .data     0x48106190  # 8000C8B0 => b         +0x00106190 /* 80112A40 */
  # region @ 8000C8C0 (16 bytes)
  .data     0x8000C8C0  # address
  .data     0x00000010  # size
  .data     0x7000000F  # 8000C8C0 => andi.     r0, r0, 0x000F
  .data     0x7000004F  # 8000C8C4 => andi.     r0, r0, 0x004F
  .data     0x2C000004  # 8000C8C8 => cmpwi     r0, 4
  .data     0x4E800020  # 8000C8CC => blr
  # region @ 8000D980 (20 bytes)
  .data     0x8000D980  # address
  .data     0x00000014  # size
  .data     0x807C0000  # 8000D980 => lwz       r3, [r28]
  .data     0x2C030013  # 8000D984 => cmpwi     r3, 19
  .data     0x40820008  # 8000D988 => bne       +0x00000008 /* 8000D990 */
  .data     0x38600002  # 8000D98C => li        r3, 0x0002
  .data     0x482AFB9C  # 8000D990 => b         +0x002AFB9C /* 802BD52C */
  # region @ 8000D9A0 (24 bytes)
  .data     0x8000D9A0  # address
  .data     0x00000018  # size
  .data     0xC042FC80  # 8000D9A0 => lfs       f2, [r2 - 0x0380]
  .data     0x807E0030  # 8000D9A4 => lwz       r3, [r30 + 0x0030]
  .data     0x70630020  # 8000D9A8 => andi.     r3, r3, 0x0020
  .data     0x41820008  # 8000D9AC => beq       +0x00000008 /* 8000D9B4 */
  .data     0xC042FC98  # 8000D9B0 => lfs       f2, [r2 - 0x0368]
  .data     0x48329C38  # 8000D9B4 => b         +0x00329C38 /* 803375EC */
  # region @ 8000E1E0 (28 bytes)
  .data     0x8000E1E0  # address
  .data     0x0000001C  # size
  .data     0x7FC802A6  # 8000E1E0 => mflr      r30
  .data     0x38A00000  # 8000E1E4 => li        r5, 0x0000
  .data     0x38C0001E  # 8000E1E8 => li        r6, 0x001E
  .data     0x38E00040  # 8000E1EC => li        r7, 0x0040
  .data     0x48078715  # 8000E1F0 => bl        +0x00078714 /* 80086904 */
  .data     0x7FC803A6  # 8000E1F4 => mtlr      r30
  .data     0x4E800020  # 8000E1F8 => blr
  # region @ 80013364 (4 bytes)
  .data     0x80013364  # address
  .data     0x00000004  # size
  .data     0x4BFFFCC0  # 80013364 => b         -0x00000340 /* 80013024 */
  # region @ 800146A4 (4 bytes)
  .data     0x800146A4  # address
  .data     0x00000004  # size
  .data     0x4BFF821D  # 800146A4 => bl        -0x00007DE4 /* 8000C8C0 */
  # region @ 80016174 (4 bytes)
  .data     0x80016174  # address
  .data     0x00000004  # size
  .data     0x4BFF6751  # 80016174 => bl        -0x000098B0 /* 8000C8C4 */
  # region @ 8009198C (8 bytes)
  .data     0x8009198C  # address
  .data     0x00000008  # size
  .data     0x4800024D  # 8009198C => bl        +0x0000024C /* 80091BD8 */
  .data     0xB3C3032C  # 80091990 => sth       [r3 + 0x032C], r30
  # region @ 800BCBD0 (4 bytes)
  .data     0x800BCBD0  # address
  .data     0x00000004  # size
  .data     0x48000010  # 800BCBD0 => b         +0x00000010 /* 800BCBE0 */
  # region @ 80104DE0 (4 bytes)
  .data     0x80104DE0  # address
  .data     0x00000004  # size
  .data     0x4182000C  # 80104DE0 => beq       +0x0000000C /* 80104DEC */
  # region @ 80107708 (4 bytes)
  .data     0x80107708  # address
  .data     0x00000004  # size
  .data     0x4800000C  # 80107708 => b         +0x0000000C /* 80107714 */
  # region @ 8010771C (4 bytes)
  .data     0x8010771C  # address
  .data     0x00000004  # size
  .data     0x7C030378  # 8010771C => mr        r3, r0
  # region @ 8010BC04 (4 bytes)
  .data     0x8010BC04  # address
  .data     0x00000004  # size
  .data     0x4BEFF498  # 8010BC04 => b         -0x00100B68 /* 8000B09C */
  # region @ 8010E0E4 (4 bytes)
  .data     0x8010E0E4  # address
  .data     0x00000004  # size
  .data     0x4BEFCFD0  # 8010E0E4 => b         -0x00103030 /* 8000B0B4 */
  # region @ 80112A3C (4 bytes)
  .data     0x80112A3C  # address
  .data     0x00000004  # size
  .data     0x4BEF9E64  # 80112A3C => b         -0x0010619C /* 8000C8A0 */
  # region @ 80114634 (4 bytes)
  .data     0x80114634  # address
  .data     0x00000004  # size
  .data     0x38000012  # 80114634 => li        r0, 0x0012
  # region @ 8011885C (4 bytes)
  .data     0x8011885C  # address
  .data     0x00000004  # size
  .data     0x88040016  # 8011885C => lbz       r0, [r4 + 0x0016]
  # region @ 80118868 (4 bytes)
  .data     0x80118868  # address
  .data     0x00000004  # size
  .data     0x88040017  # 80118868 => lbz       r0, [r4 + 0x0017]
  # region @ 80118FD8 (4 bytes)
  .data     0x80118FD8  # address
  .data     0x00000004  # size
  .data     0x4BEF3668  # 80118FD8 => b         -0x0010C998 /* 8000C640 */
  # region @ 8011CD0C (12 bytes)
  .data     0x8011CD0C  # address
  .data     0x0000000C  # size
  .data     0x7C030378  # 8011CD0C => mr        r3, r0
  .data     0x3863FFFF  # 8011CD10 => subi      r3, r3, 0x0001
  .data     0x4BFFFFE8  # 8011CD14 => b         -0x00000018 /* 8011CCFC */
  # region @ 8011CDC8 (12 bytes)
  .data     0x8011CDC8  # address
  .data     0x0000000C  # size
  .data     0x7C030378  # 8011CDC8 => mr        r3, r0
  .data     0x3863FFFF  # 8011CDCC => subi      r3, r3, 0x0001
  .data     0x4BFFFFE8  # 8011CDD0 => b         -0x00000018 /* 8011CDB8 */
  # region @ 8011CE18 (12 bytes)
  .data     0x8011CE18  # address
  .data     0x0000000C  # size
  .data     0x7C040378  # 8011CE18 => mr        r4, r0
  .data     0x3884FFFF  # 8011CE1C => subi      r4, r4, 0x0001
  .data     0x4BFFFFE8  # 8011CE20 => b         -0x00000018 /* 8011CE08 */
  # region @ 80166848 (8 bytes)
  .data     0x80166848  # address
  .data     0x00000008  # size
  .data     0x3C604005  # 80166848 => lis       r3, 0x4005
  .data     0x4800009C  # 8016684C => b         +0x0000009C /* 801668E8 */
  # region @ 801668E4 (4 bytes)
  .data     0x801668E4  # address
  .data     0x00000004  # size
  .data     0x4800001C  # 801668E4 => b         +0x0000001C /* 80166900 */
  # region @ 80171260 (4 bytes)
  .data     0x80171260  # address
  .data     0x00000004  # size
  .data     0x4BE9A970  # 80171260 => b         -0x00165690 /* 8000BBD0 */
  # region @ 80171280 (4 bytes)
  .data     0x80171280  # address
  .data     0x00000004  # size
  .data     0x60800420  # 80171280 => ori       r0, r4, 0x0420
  # region @ 801843C0 (4 bytes)
  .data     0x801843C0  # address
  .data     0x00000004  # size
  .data     0x4BE87208  # 801843C0 => b         -0x00178DF8 /* 8000B5C8 */
  # region @ 80184400 (4 bytes)
  .data     0x80184400  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80184400 => nop
  # region @ 80189F90 (4 bytes)
  .data     0x80189F90  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80189F90 => nop
  # region @ 80193914 (4 bytes)
  .data     0x80193914  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80193914 => nop
  # region @ 801BCC98 (4 bytes)
  .data     0x801BCC98  # address
  .data     0x00000004  # size
  .data     0x4BE4F770  # 801BCC98 => b         -0x001B0890 /* 8000C408 */
  # region @ 801BCF6C (4 bytes)
  .data     0x801BCF6C  # address
  .data     0x00000004  # size
  .data     0x4BE4E11C  # 801BCF6C => b         -0x001B1EE4 /* 8000B088 */
  # region @ 801C6604 (4 bytes)
  .data     0x801C6604  # address
  .data     0x00000004  # size
  .data     0x389F02FC  # 801C6604 => addi      r4, r31, 0x02FC
  # region @ 801CB5EC (4 bytes)
  .data     0x801CB5EC  # address
  .data     0x00000004  # size
  .data     0x48000010  # 801CB5EC => b         +0x00000010 /* 801CB5FC */
  # region @ 8021E8E8 (4 bytes)
  .data     0x8021E8E8  # address
  .data     0x00000004  # size
  .data     0x4BDEDDE8  # 8021E8E8 => b         -0x00212218 /* 8000C6D0 */
  # region @ 80221DA8 (4 bytes)
  .data     0x80221DA8  # address
  .data     0x00000004  # size
  .data     0x4BDEA938  # 80221DA8 => b         -0x002156C8 /* 8000C6E0 */
  # region @ 8022ABDC (4 bytes)
  .data     0x8022ABDC  # address
  .data     0x00000004  # size
  .data     0x2C000001  # 8022ABDC => cmpwi     r0, 1
  # region @ 8022B3E0 (4 bytes)
  .data     0x8022B3E0  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8022B3E0 => li        r4, 0xFFFFFF00
  # region @ 8022B410 (4 bytes)
  .data     0x8022B410  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 8022B410 => li        r4, 0xFFFFFE80
  # region @ 8022B440 (4 bytes)
  .data     0x8022B440  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 8022B440 => li        r4, 0xFFFFFDB0
  # region @ 8022E128 (4 bytes)
  .data     0x8022E128  # address
  .data     0x00000004  # size
  .data     0x60000000  # 8022E128 => nop
  # region @ 8022E85C (4 bytes)
  .data     0x8022E85C  # address
  .data     0x00000004  # size
  .data     0x41810630  # 8022E85C => bgt       +0x00000630 /* 8022EE8C */
  # region @ 8022FB30 (4 bytes)
  .data     0x8022FB30  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8022FB30 => li        r4, 0xFFFFFF00
  # region @ 8022FB60 (4 bytes)
  .data     0x8022FB60  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 8022FB60 => li        r4, 0xFFFFFE80
  # region @ 8022FB90 (4 bytes)
  .data     0x8022FB90  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 8022FB90 => li        r4, 0xFFFFFDB0
  # region @ 80230340 (4 bytes)
  .data     0x80230340  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80230340 => li        r4, 0xFFFFFF00
  # region @ 80230370 (4 bytes)
  .data     0x80230370  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80230370 => li        r4, 0xFFFFFE80
  # region @ 802303A0 (4 bytes)
  .data     0x802303A0  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 802303A0 => li        r4, 0xFFFFFDB0
  # region @ 80231940 (4 bytes)
  .data     0x80231940  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80231940 => li        r4, 0xFFFFFF00
  # region @ 80231970 (4 bytes)
  .data     0x80231970  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80231970 => li        r4, 0xFFFFFE80
  # region @ 802319A0 (4 bytes)
  .data     0x802319A0  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 802319A0 => li        r4, 0xFFFFFDB0
  # region @ 802326B0 (4 bytes)
  .data     0x802326B0  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 802326B0 => li        r4, 0xFFFFFF00
  # region @ 802326E0 (4 bytes)
  .data     0x802326E0  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 802326E0 => li        r4, 0xFFFFFE80
  # region @ 80232710 (4 bytes)
  .data     0x80232710  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80232710 => li        r4, 0xFFFFFDB0
  # region @ 80232FA4 (4 bytes)
  .data     0x80232FA4  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80232FA4 => li        r4, 0xFFFFFF00
  # region @ 80232FDC (4 bytes)
  .data     0x80232FDC  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80232FDC => li        r4, 0xFFFFFE80
  # region @ 80233014 (4 bytes)
  .data     0x80233014  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80233014 => li        r4, 0xFFFFFDB0
  # region @ 80235050 (4 bytes)
  .data     0x80235050  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80235050 => li        r4, 0xFFFFFF00
  # region @ 80235080 (4 bytes)
  .data     0x80235080  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80235080 => li        r4, 0xFFFFFE80
  # region @ 802350B0 (4 bytes)
  .data     0x802350B0  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 802350B0 => li        r4, 0xFFFFFDB0
  # region @ 8023767C (4 bytes)
  .data     0x8023767C  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8023767C => li        r4, 0xFFFFFF00
  # region @ 802376B8 (4 bytes)
  .data     0x802376B8  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 802376B8 => li        r4, 0xFFFFFE80
  # region @ 802376F4 (4 bytes)
  .data     0x802376F4  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 802376F4 => li        r4, 0xFFFFFDB0
  # region @ 80237E54 (4 bytes)
  .data     0x80237E54  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80237E54 => li        r4, 0xFFFFFF00
  # region @ 80237E84 (4 bytes)
  .data     0x80237E84  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80237E84 => li        r4, 0xFFFFFE80
  # region @ 80237EB4 (4 bytes)
  .data     0x80237EB4  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80237EB4 => li        r4, 0xFFFFFDB0
  # region @ 80238868 (4 bytes)
  .data     0x80238868  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80238868 => li        r4, 0xFFFFFF00
  # region @ 80238898 (4 bytes)
  .data     0x80238898  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80238898 => li        r4, 0xFFFFFE80
  # region @ 802388C8 (4 bytes)
  .data     0x802388C8  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 802388C8 => li        r4, 0xFFFFFDB0
  # region @ 80239240 (4 bytes)
  .data     0x80239240  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80239240 => li        r4, 0xFFFFFF00
  # region @ 80239270 (4 bytes)
  .data     0x80239270  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80239270 => li        r4, 0xFFFFFE80
  # region @ 802392A0 (4 bytes)
  .data     0x802392A0  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 802392A0 => li        r4, 0xFFFFFDB0
  # region @ 8023CB70 (4 bytes)
  .data     0x8023CB70  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8023CB70 => li        r4, 0xFFFFFF00
  # region @ 8023CBA0 (4 bytes)
  .data     0x8023CBA0  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 8023CBA0 => li        r4, 0xFFFFFE80
  # region @ 8023CBD0 (4 bytes)
  .data     0x8023CBD0  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 8023CBD0 => li        r4, 0xFFFFFDB0
  # region @ 80251CA4 (4 bytes)
  .data     0x80251CA4  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80251CA4 => nop
  # region @ 80269AE4 (4 bytes)
  .data     0x80269AE4  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80269AE4 => nop
  # region @ 8026F794 (4 bytes)
  .data     0x8026F794  # address
  .data     0x00000004  # size
  .data     0x3884AAFA  # 8026F794 => subi      r4, r4, 0x5506
  # region @ 8026F8A8 (4 bytes)
  .data     0x8026F8A8  # address
  .data     0x00000004  # size
  .data     0x3863AAFA  # 8026F8A8 => subi      r3, r3, 0x5506
  # region @ 8026F930 (4 bytes)
  .data     0x8026F930  # address
  .data     0x00000004  # size
  .data     0x3883AAFA  # 8026F930 => subi      r4, r3, 0x5506
  # region @ 802BD528 (4 bytes)
  .data     0x802BD528  # address
  .data     0x00000004  # size
  .data     0x4BD50458  # 802BD528 => b         -0x002AFBA8 /* 8000D980 */
  # region @ 802FDE60 (4 bytes)
  .data     0x802FDE60  # address
  .data     0x00000004  # size
  .data     0x2C030001  # 802FDE60 => cmpwi     r3, 1
  # region @ 80303A1C (28 bytes)
  .data     0x80303A1C  # address
  .data     0x0000001C  # size
  .data     0x48000020  # 80303A1C => b         +0x00000020 /* 80303A3C */
  .data     0x3863A830  # 80303A20 => subi      r3, r3, 0x57D0
  .data     0x800DB9B4  # 80303A24 => lwz       r0, [r13 - 0x464C]
  .data     0x2C000023  # 80303A28 => cmpwi     r0, 35
  .data     0x40820008  # 80303A2C => bne       +0x00000008 /* 80303A34 */
  .data     0x3863FB28  # 80303A30 => subi      r3, r3, 0x04D8
  .data     0x4800008C  # 80303A34 => b         +0x0000008C /* 80303AC0 */
  # region @ 80303ABC (4 bytes)
  .data     0x80303ABC  # address
  .data     0x00000004  # size
  .data     0x4BFFFF64  # 80303ABC => b         -0x0000009C /* 80303A20 */
  # region @ 803375E8 (4 bytes)
  .data     0x803375E8  # address
  .data     0x00000004  # size
  .data     0x4BCD63B8  # 803375E8 => b         -0x00329C48 /* 8000D9A0 */
  # region @ 803582C0 (4 bytes)
  .data     0x803582C0  # address
  .data     0x00000004  # size
  .data     0x388001E8  # 803582C0 => li        r4, 0x01E8
  # region @ 803582E4 (4 bytes)
  .data     0x803582E4  # address
  .data     0x00000004  # size
  .data     0x4BCB5EFD  # 803582E4 => bl        -0x0034A104 /* 8000E1E0 */
  # region @ 80358354 (4 bytes)
  .data     0x80358354  # address
  .data     0x00000004  # size
  .data     0x388001E8  # 80358354 => li        r4, 0x01E8
  # region @ 80358364 (4 bytes)
  .data     0x80358364  # address
  .data     0x00000004  # size
  .data     0x4BCB5E7D  # 80358364 => bl        -0x0034A184 /* 8000E1E0 */
  # region @ 804B92F8 (8 bytes)
  .data     0x804B92F8  # address
  .data     0x00000008  # size
  .data     0x70808080  # 804B92F8 => andi.     r0, r4, 0x8080
  .data     0x60707070  # 804B92FC => ori       r16, r3, 0x7070
  # region @ 804CCB6C (4 bytes)
  .data     0x804CCB6C  # address
  .data     0x00000004  # size
  .data     0x0000001E  # 804CCB6C => .invalid
  # region @ 804CCBC4 (4 bytes)
  .data     0x804CCBC4  # address
  .data     0x00000004  # size
  .data     0x00000028  # 804CCBC4 => .invalid
  # region @ 804CCBF0 (4 bytes)
  .data     0x804CCBF0  # address
  .data     0x00000004  # size
  .data     0x00000032  # 804CCBF0 => .invalid
  # region @ 804CCC1C (4 bytes)
  .data     0x804CCC1C  # address
  .data     0x00000004  # size
  .data     0x0000003C  # 804CCC1C => .invalid
  # region @ 804CCC2C (4 bytes)
  .data     0x804CCC2C  # address
  .data     0x00000004  # size
  .data     0x0018003C  # 804CCC2C => .invalid
  # region @ 804CCE84 (4 bytes)
  .data     0x804CCE84  # address
  .data     0x00000004  # size
  .data     0x00000028  # 804CCE84 => .invalid
  # region @ 804D17E0 (4 bytes)
  .data     0x804D17E0  # address
  .data     0x00000004  # size
  .data     0xFF0074EE  # 804D17E0 => fsel      f24, f0, f14, f19
  # region @ 805DB40C (4 bytes)
  .data     0x805DB40C  # address
  .data     0x00000004  # size
  .data     0x435C0000  # 805DB40C => bc        26, 28, +0x00000000 /* 805DB40C */
  # region @ 805DD0A8 (4 bytes)
  .data     0x805DD0A8  # address
  .data     0x00000004  # size
  .data     0x46AFC800  # 805DD0A8 => .invalid  sc
  # region @ 805DD348 (4 bytes)
  .data     0x805DD348  # address
  .data     0x00000004  # size
  .data     0x43480000  # 805DD348 => bc        26, 8, +0x00000000 /* 805DD348 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
