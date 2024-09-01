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
  .data     0x481AE725  # 8000B090 => bl        +0x001AE724 /* 801B97B4 */
  .data     0x7FA3EB78  # 8000B094 => mr        r3, r29
  .data     0x481AE9F4  # 8000B098 => b         +0x001AE9F4 /* 801B9A8C */
  .data     0x881F0000  # 8000B09C => lbz       r0, [r31]
  .data     0x28090001  # 8000B0A0 => cmplwi    r9, 1
  .data     0x4082000C  # 8000B0A4 => bne       +0x0000000C /* 8000B0B0 */
  .data     0x881F0001  # 8000B0A8 => lbz       r0, [r31 + 0x0001]
  .data     0x3BFF0002  # 8000B0AC => addi      r31, r31, 0x0002
  .data     0x481008C4  # 8000B0B0 => b         +0x001008C4 /* 8010B974 */
  .data     0x39200000  # 8000B0B4 => li        r9, 0x0000
  .data     0x48100855  # 8000B0B8 => bl        +0x00100854 /* 8010B90C */
  .data     0x7F43D378  # 8000B0BC => mr        r3, r26
  .data     0x7F64DB78  # 8000B0C0 => mr        r4, r27
  .data     0x7F85E378  # 8000B0C4 => mr        r5, r28
  .data     0x7FA6EB78  # 8000B0C8 => mr        r6, r29
  .data     0x7FC7F378  # 8000B0CC => mr        r7, r30
  .data     0x7FE8FB78  # 8000B0D0 => mr        r8, r31
  .data     0x39200001  # 8000B0D4 => li        r9, 0x0001
  .data     0x48100835  # 8000B0D8 => bl        +0x00100834 /* 8010B90C */
  .data     0x48102CC0  # 8000B0DC => b         +0x00102CC0 /* 8010DD9C */
  # region @ 8000B5C8 (20 bytes)
  .data     0x8000B5C8  # address
  .data     0x00000014  # size
  .data     0x80630098  # 8000B5C8 => lwz       r3, [r3 + 0x0098]
  .data     0x483D46F5  # 8000B5CC => bl        +0x003D46F4 /* 803DFCC0 */
  .data     0x807F042C  # 8000B5D0 => lwz       r3, [r31 + 0x042C]
  .data     0x809F0430  # 8000B5D4 => lwz       r4, [r31 + 0x0430]
  .data     0x481788C0  # 8000B5D8 => b         +0x001788C0 /* 80183E98 */
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
  .data     0x4816506C  # 8000BBEC => b         +0x0016506C /* 80170C58 */
  # region @ 8000C3F8 (124 bytes)
  .data     0x8000C3F8  # address
  .data     0x0000007C  # size
  .data     0x28040000  # 8000C3F8 => cmplwi    r4, 0
  .data     0x4D820020  # 8000C3FC => beqlr
  .data     0x9421FFF0  # 8000C400 => stwu      [r1 - 0x0010], r1
  .data     0x481AD3B4  # 8000C404 => b         +0x001AD3B4 /* 801B97B8 */
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
  .data     0x480FEADD  # 8000C43C => bl        +0x000FEADC /* 8010AF18 */
  .data     0x7F83E378  # 8000C440 => mr        r3, r28
  .data     0x38800001  # 8000C444 => li        r4, 0x0001
  .data     0x480FEC4D  # 8000C448 => bl        +0x000FEC4C /* 8010B094 */
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
  .data     0x4810C694  # 8000C650 => b         +0x0010C694 /* 80118CE4 */
  # region @ 8000C6D0 (32 bytes)
  .data     0x8000C6D0  # address
  .data     0x00000020  # size
  .data     0x38000001  # 8000C6D0 => li        r0, 0x0001
  .data     0x901D0054  # 8000C6D4 => stw       [r29 + 0x0054], r0
  .data     0x807D0024  # 8000C6D8 => lwz       r3, [r29 + 0x0024]
  .data     0x482109C0  # 8000C6DC => b         +0x002109C0 /* 8021D09C */
  .data     0x38000001  # 8000C6E0 => li        r0, 0x0001
  .data     0x901F0378  # 8000C6E4 => stw       [r31 + 0x0378], r0
  .data     0x807F0024  # 8000C6E8 => lwz       r3, [r31 + 0x0024]
  .data     0x48165AA0  # 8000C6EC => b         +0x00165AA0 /* 8017218C */
  # region @ 8000C8A0 (20 bytes)
  .data     0x8000C8A0  # address
  .data     0x00000014  # size
  .data     0x1C00000A  # 8000C8A0 => mulli     r0, r0, 10
  .data     0x57E407BD  # 8000C8A4 => rlwinm.   r4, r31, 0, 30, 30
  .data     0x41820008  # 8000C8A8 => beq       +0x00000008 /* 8000C8B0 */
  .data     0x7FA00734  # 8000C8AC => extsh     r0, r29
  .data     0x48105DB8  # 8000C8B0 => b         +0x00105DB8 /* 80112668 */
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
  .data     0x482ADB24  # 8000D990 => b         +0x002ADB24 /* 802BB4B4 */
  # region @ 8000D9A0 (24 bytes)
  .data     0x8000D9A0  # address
  .data     0x00000018  # size
  .data     0xC042FC78  # 8000D9A0 => lfs       f2, [r2 - 0x0388]
  .data     0x807E0030  # 8000D9A4 => lwz       r3, [r30 + 0x0030]
  .data     0x70630020  # 8000D9A8 => andi.     r3, r3, 0x0020
  .data     0x41820008  # 8000D9AC => beq       +0x00000008 /* 8000D9B4 */
  .data     0xC042FC90  # 8000D9B0 => lfs       f2, [r2 - 0x0370]
  .data     0x483276B0  # 8000D9B4 => b         +0x003276B0 /* 80335064 */
  # region @ 8000E1E0 (28 bytes)
  .data     0x8000E1E0  # address
  .data     0x0000001C  # size
  .data     0x7FC802A6  # 8000E1E0 => mflr      r30
  .data     0x38A00000  # 8000E1E4 => li        r5, 0x0000
  .data     0x38C0001E  # 8000E1E8 => li        r6, 0x001E
  .data     0x38E00040  # 8000E1EC => li        r7, 0x0040
  .data     0x480782B1  # 8000E1F0 => bl        +0x000782B0 /* 800864A0 */
  .data     0x7FC803A6  # 8000E1F4 => mtlr      r30
  .data     0x4E800020  # 8000E1F8 => blr
  # region @ 8001306C (4 bytes)
  .data     0x8001306C  # address
  .data     0x00000004  # size
  .data     0x4BFFFCC0  # 8001306C => b         -0x00000340 /* 80012D2C */
  # region @ 800142DC (4 bytes)
  .data     0x800142DC  # address
  .data     0x00000004  # size
  .data     0x4BFF85E5  # 800142DC => bl        -0x00007A1C /* 8000C8C0 */
  # region @ 80015D04 (4 bytes)
  .data     0x80015D04  # address
  .data     0x00000004  # size
  .data     0x4BFF6BC1  # 80015D04 => bl        -0x00009440 /* 8000C8C4 */
  # region @ 80091528 (8 bytes)
  .data     0x80091528  # address
  .data     0x00000008  # size
  .data     0x4800024D  # 80091528 => bl        +0x0000024C /* 80091774 */
  .data     0xB3C3032C  # 8009152C => sth       [r3 + 0x032C], r30
  # region @ 800BC750 (4 bytes)
  .data     0x800BC750  # address
  .data     0x00000004  # size
  .data     0x48000010  # 800BC750 => b         +0x00000010 /* 800BC760 */
  # region @ 80101C14 (4 bytes)
  .data     0x80101C14  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80101C14 => nop
  # region @ 80104B48 (4 bytes)
  .data     0x80104B48  # address
  .data     0x00000004  # size
  .data     0x4182000C  # 80104B48 => beq       +0x0000000C /* 80104B54 */
  # region @ 80107478 (4 bytes)
  .data     0x80107478  # address
  .data     0x00000004  # size
  .data     0x4800000C  # 80107478 => b         +0x0000000C /* 80107484 */
  # region @ 8010748C (4 bytes)
  .data     0x8010748C  # address
  .data     0x00000004  # size
  .data     0x7C030378  # 8010748C => mr        r3, r0
  # region @ 8010B970 (4 bytes)
  .data     0x8010B970  # address
  .data     0x00000004  # size
  .data     0x4BEFF72C  # 8010B970 => b         -0x001008D4 /* 8000B09C */
  # region @ 8010DD98 (4 bytes)
  .data     0x8010DD98  # address
  .data     0x00000004  # size
  .data     0x4BEFD31C  # 8010DD98 => b         -0x00102CE4 /* 8000B0B4 */
  # region @ 80112664 (4 bytes)
  .data     0x80112664  # address
  .data     0x00000004  # size
  .data     0x4BEFA23C  # 80112664 => b         -0x00105DC4 /* 8000C8A0 */
  # region @ 80114378 (4 bytes)
  .data     0x80114378  # address
  .data     0x00000004  # size
  .data     0x38000012  # 80114378 => li        r0, 0x0012
  # region @ 801185B0 (4 bytes)
  .data     0x801185B0  # address
  .data     0x00000004  # size
  .data     0x88040016  # 801185B0 => lbz       r0, [r4 + 0x0016]
  # region @ 801185BC (4 bytes)
  .data     0x801185BC  # address
  .data     0x00000004  # size
  .data     0x88040017  # 801185BC => lbz       r0, [r4 + 0x0017]
  # region @ 80118CE0 (4 bytes)
  .data     0x80118CE0  # address
  .data     0x00000004  # size
  .data     0x4BEF3960  # 80118CE0 => b         -0x0010C6A0 /* 8000C640 */
  # region @ 8011CA90 (12 bytes)
  .data     0x8011CA90  # address
  .data     0x0000000C  # size
  .data     0x7C030378  # 8011CA90 => mr        r3, r0
  .data     0x3863FFFF  # 8011CA94 => subi      r3, r3, 0x0001
  .data     0x4BFFFFE8  # 8011CA98 => b         -0x00000018 /* 8011CA80 */
  # region @ 8011CB4C (12 bytes)
  .data     0x8011CB4C  # address
  .data     0x0000000C  # size
  .data     0x7C030378  # 8011CB4C => mr        r3, r0
  .data     0x3863FFFF  # 8011CB50 => subi      r3, r3, 0x0001
  .data     0x4BFFFFE8  # 8011CB54 => b         -0x00000018 /* 8011CB3C */
  # region @ 8011CB9C (12 bytes)
  .data     0x8011CB9C  # address
  .data     0x0000000C  # size
  .data     0x7C040378  # 8011CB9C => mr        r4, r0
  .data     0x3884FFFF  # 8011CBA0 => subi      r4, r4, 0x0001
  .data     0x4BFFFFE8  # 8011CBA4 => b         -0x00000018 /* 8011CB8C */
  # region @ 80166324 (8 bytes)
  .data     0x80166324  # address
  .data     0x00000008  # size
  .data     0x3C604005  # 80166324 => lis       r3, 0x4005
  .data     0x4800009C  # 80166328 => b         +0x0000009C /* 801663C4 */
  # region @ 801663C0 (4 bytes)
  .data     0x801663C0  # address
  .data     0x00000004  # size
  .data     0x4800001C  # 801663C0 => b         +0x0000001C /* 801663DC */
  # region @ 80170C54 (4 bytes)
  .data     0x80170C54  # address
  .data     0x00000004  # size
  .data     0x4BE9AF7C  # 80170C54 => b         -0x00165084 /* 8000BBD0 */
  # region @ 80170C74 (4 bytes)
  .data     0x80170C74  # address
  .data     0x00000004  # size
  .data     0x60800420  # 80170C74 => ori       r0, r4, 0x0420
  # region @ 80172188 (4 bytes)
  .data     0x80172188  # address
  .data     0x00000004  # size
  .data     0x4BE9A558  # 80172188 => b         -0x00165AA8 /* 8000C6E0 */
  # region @ 80183E94 (4 bytes)
  .data     0x80183E94  # address
  .data     0x00000004  # size
  .data     0x4BE87734  # 80183E94 => b         -0x001788CC /* 8000B5C8 */
  # region @ 80183ED4 (4 bytes)
  .data     0x80183ED4  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80183ED4 => nop
  # region @ 80189A54 (4 bytes)
  .data     0x80189A54  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80189A54 => nop
  # region @ 801933DC (4 bytes)
  .data     0x801933DC  # address
  .data     0x00000004  # size
  .data     0x60000000  # 801933DC => nop
  # region @ 801B97B4 (4 bytes)
  .data     0x801B97B4  # address
  .data     0x00000004  # size
  .data     0x4BE52C54  # 801B97B4 => b         -0x001AD3AC /* 8000C408 */
  # region @ 801B9A88 (4 bytes)
  .data     0x801B9A88  # address
  .data     0x00000004  # size
  .data     0x4BE51600  # 801B9A88 => b         -0x001AEA00 /* 8000B088 */
  # region @ 801C5EA4 (4 bytes)
  .data     0x801C5EA4  # address
  .data     0x00000004  # size
  .data     0x389F02FC  # 801C5EA4 => addi      r4, r31, 0x02FC
  # region @ 801CA1F4 (4 bytes)
  .data     0x801CA1F4  # address
  .data     0x00000004  # size
  .data     0x48000010  # 801CA1F4 => b         +0x00000010 /* 801CA204 */
  # region @ 8021D098 (4 bytes)
  .data     0x8021D098  # address
  .data     0x00000004  # size
  .data     0x4BDEF638  # 8021D098 => b         -0x002109C8 /* 8000C6D0 */
  # region @ 80229354 (4 bytes)
  .data     0x80229354  # address
  .data     0x00000004  # size
  .data     0x2C000001  # 80229354 => cmpwi     r0, 1
  # region @ 80229B54 (4 bytes)
  .data     0x80229B54  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80229B54 => li        r4, 0xFFFFFF00
  # region @ 80229B84 (4 bytes)
  .data     0x80229B84  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80229B84 => li        r4, 0xFFFFFE80
  # region @ 80229BB4 (4 bytes)
  .data     0x80229BB4  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80229BB4 => li        r4, 0xFFFFFDB0
  # region @ 8022C850 (4 bytes)
  .data     0x8022C850  # address
  .data     0x00000004  # size
  .data     0x60000000  # 8022C850 => nop
  # region @ 8022CF84 (4 bytes)
  .data     0x8022CF84  # address
  .data     0x00000004  # size
  .data     0x41810630  # 8022CF84 => bgt       +0x00000630 /* 8022D5B4 */
  # region @ 8022D278 (4 bytes)
  .data     0x8022D278  # address
  .data     0x00000004  # size
  .data     0x4181033C  # 8022D278 => bgt       +0x0000033C /* 8022D5B4 */
  # region @ 8022D36C (4 bytes)
  .data     0x8022D36C  # address
  .data     0x00000004  # size
  .data     0x41810248  # 8022D36C => bgt       +0x00000248 /* 8022D5B4 */
  # region @ 8022E2A8 (4 bytes)
  .data     0x8022E2A8  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8022E2A8 => li        r4, 0xFFFFFF00
  # region @ 8022E2D8 (4 bytes)
  .data     0x8022E2D8  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 8022E2D8 => li        r4, 0xFFFFFE80
  # region @ 8022E308 (4 bytes)
  .data     0x8022E308  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 8022E308 => li        r4, 0xFFFFFDB0
  # region @ 8022EAB4 (4 bytes)
  .data     0x8022EAB4  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8022EAB4 => li        r4, 0xFFFFFF00
  # region @ 8022EAE4 (4 bytes)
  .data     0x8022EAE4  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 8022EAE4 => li        r4, 0xFFFFFE80
  # region @ 8022EB14 (4 bytes)
  .data     0x8022EB14  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 8022EB14 => li        r4, 0xFFFFFDB0
  # region @ 802300B8 (4 bytes)
  .data     0x802300B8  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 802300B8 => li        r4, 0xFFFFFF00
  # region @ 802300E8 (4 bytes)
  .data     0x802300E8  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 802300E8 => li        r4, 0xFFFFFE80
  # region @ 80230118 (4 bytes)
  .data     0x80230118  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80230118 => li        r4, 0xFFFFFDB0
  # region @ 80230E08 (4 bytes)
  .data     0x80230E08  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80230E08 => li        r4, 0xFFFFFF00
  # region @ 80230E38 (4 bytes)
  .data     0x80230E38  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80230E38 => li        r4, 0xFFFFFE80
  # region @ 80230E68 (4 bytes)
  .data     0x80230E68  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80230E68 => li        r4, 0xFFFFFDB0
  # region @ 802316FC (4 bytes)
  .data     0x802316FC  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 802316FC => li        r4, 0xFFFFFF00
  # region @ 80231734 (4 bytes)
  .data     0x80231734  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80231734 => li        r4, 0xFFFFFE80
  # region @ 8023176C (4 bytes)
  .data     0x8023176C  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 8023176C => li        r4, 0xFFFFFDB0
  # region @ 802337A8 (4 bytes)
  .data     0x802337A8  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 802337A8 => li        r4, 0xFFFFFF00
  # region @ 802337D8 (4 bytes)
  .data     0x802337D8  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 802337D8 => li        r4, 0xFFFFFE80
  # region @ 80233808 (4 bytes)
  .data     0x80233808  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80233808 => li        r4, 0xFFFFFDB0
  # region @ 80235DD4 (4 bytes)
  .data     0x80235DD4  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80235DD4 => li        r4, 0xFFFFFF00
  # region @ 80235E10 (4 bytes)
  .data     0x80235E10  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80235E10 => li        r4, 0xFFFFFE80
  # region @ 80235E4C (4 bytes)
  .data     0x80235E4C  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80235E4C => li        r4, 0xFFFFFDB0
  # region @ 802365AC (4 bytes)
  .data     0x802365AC  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 802365AC => li        r4, 0xFFFFFF00
  # region @ 802365DC (4 bytes)
  .data     0x802365DC  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 802365DC => li        r4, 0xFFFFFE80
  # region @ 8023660C (4 bytes)
  .data     0x8023660C  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 8023660C => li        r4, 0xFFFFFDB0
  # region @ 80236FC0 (4 bytes)
  .data     0x80236FC0  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80236FC0 => li        r4, 0xFFFFFF00
  # region @ 80236FF0 (4 bytes)
  .data     0x80236FF0  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80236FF0 => li        r4, 0xFFFFFE80
  # region @ 80237020 (4 bytes)
  .data     0x80237020  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80237020 => li        r4, 0xFFFFFDB0
  # region @ 80237998 (4 bytes)
  .data     0x80237998  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80237998 => li        r4, 0xFFFFFF00
  # region @ 802379C8 (4 bytes)
  .data     0x802379C8  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 802379C8 => li        r4, 0xFFFFFE80
  # region @ 802379F8 (4 bytes)
  .data     0x802379F8  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 802379F8 => li        r4, 0xFFFFFDB0
  # region @ 8023B2C8 (4 bytes)
  .data     0x8023B2C8  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8023B2C8 => li        r4, 0xFFFFFF00
  # region @ 8023B2F8 (4 bytes)
  .data     0x8023B2F8  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 8023B2F8 => li        r4, 0xFFFFFE80
  # region @ 8023B328 (4 bytes)
  .data     0x8023B328  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 8023B328 => li        r4, 0xFFFFFDB0
  # region @ 80250264 (4 bytes)
  .data     0x80250264  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80250264 => nop
  # region @ 80267DDC (4 bytes)
  .data     0x80267DDC  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80267DDC => nop
  # region @ 8026DA74 (4 bytes)
  .data     0x8026DA74  # address
  .data     0x00000004  # size
  .data     0x3884AAFA  # 8026DA74 => subi      r4, r4, 0x5506
  # region @ 8026DB88 (4 bytes)
  .data     0x8026DB88  # address
  .data     0x00000004  # size
  .data     0x3863AAFA  # 8026DB88 => subi      r3, r3, 0x5506
  # region @ 8026DC10 (4 bytes)
  .data     0x8026DC10  # address
  .data     0x00000004  # size
  .data     0x3883AAFA  # 8026DC10 => subi      r4, r3, 0x5506
  # region @ 802BB4B0 (4 bytes)
  .data     0x802BB4B0  # address
  .data     0x00000004  # size
  .data     0x4BD524D0  # 802BB4B0 => b         -0x002ADB30 /* 8000D980 */
  # region @ 802FB99C (4 bytes)
  .data     0x802FB99C  # address
  .data     0x00000004  # size
  .data     0x2C030001  # 802FB99C => cmpwi     r3, 1
  # region @ 80301600 (28 bytes)
  .data     0x80301600  # address
  .data     0x0000001C  # size
  .data     0x48000020  # 80301600 => b         +0x00000020 /* 80301620 */
  .data     0x3863A830  # 80301604 => subi      r3, r3, 0x57D0
  .data     0x800DB98C  # 80301608 => lwz       r0, [r13 - 0x4674]
  .data     0x2C000023  # 8030160C => cmpwi     r0, 35
  .data     0x40820008  # 80301610 => bne       +0x00000008 /* 80301618 */
  .data     0x3863FB28  # 80301614 => subi      r3, r3, 0x04D8
  .data     0x4800008C  # 80301618 => b         +0x0000008C /* 803016A4 */
  # region @ 803016A0 (4 bytes)
  .data     0x803016A0  # address
  .data     0x00000004  # size
  .data     0x4BFFFF64  # 803016A0 => b         -0x0000009C /* 80301604 */
  # region @ 80335060 (4 bytes)
  .data     0x80335060  # address
  .data     0x00000004  # size
  .data     0x4BCD8940  # 80335060 => b         -0x003276C0 /* 8000D9A0 */
  # region @ 80355960 (4 bytes)
  .data     0x80355960  # address
  .data     0x00000004  # size
  .data     0x388001E8  # 80355960 => li        r4, 0x01E8
  # region @ 80355984 (4 bytes)
  .data     0x80355984  # address
  .data     0x00000004  # size
  .data     0x4BCB885D  # 80355984 => bl        -0x003477A4 /* 8000E1E0 */
  # region @ 803559F4 (4 bytes)
  .data     0x803559F4  # address
  .data     0x00000004  # size
  .data     0x388001E8  # 803559F4 => li        r4, 0x01E8
  # region @ 80355A04 (4 bytes)
  .data     0x80355A04  # address
  .data     0x00000004  # size
  .data     0x4BCB87DD  # 80355A04 => bl        -0x00347824 /* 8000E1E0 */
  # region @ 804B3738 (8 bytes)
  .data     0x804B3738  # address
  .data     0x00000008  # size
  .data     0x70808080  # 804B3738 => andi.     r0, r4, 0x8080
  .data     0x60707070  # 804B373C => ori       r16, r3, 0x7070
  # region @ 804C6EE4 (4 bytes)
  .data     0x804C6EE4  # address
  .data     0x00000004  # size
  .data     0x0000001E  # 804C6EE4 => .invalid
  # region @ 804C6F3C (4 bytes)
  .data     0x804C6F3C  # address
  .data     0x00000004  # size
  .data     0x00000028  # 804C6F3C => .invalid
  # region @ 804C6F68 (4 bytes)
  .data     0x804C6F68  # address
  .data     0x00000004  # size
  .data     0x00000032  # 804C6F68 => .invalid
  # region @ 804C6F94 (4 bytes)
  .data     0x804C6F94  # address
  .data     0x00000004  # size
  .data     0x0000003C  # 804C6F94 => .invalid
  # region @ 804C6FA4 (4 bytes)
  .data     0x804C6FA4  # address
  .data     0x00000004  # size
  .data     0x0018003C  # 804C6FA4 => .invalid
  # region @ 804C71FC (4 bytes)
  .data     0x804C71FC  # address
  .data     0x00000004  # size
  .data     0x00000028  # 804C71FC => .invalid
  # region @ 804CBB40 (4 bytes)
  .data     0x804CBB40  # address
  .data     0x00000004  # size
  .data     0xFF0074EE  # 804CBB40 => fsel      f24, f0, f14, f19
  # region @ 805C996C (4 bytes)
  .data     0x805C996C  # address
  .data     0x00000004  # size
  .data     0x435C0000  # 805C996C => bc        26, 28, +0x00000000 /* 805C996C */
  # region @ 805CB608 (4 bytes)
  .data     0x805CB608  # address
  .data     0x00000004  # size
  .data     0x46AFC800  # 805CB608 => .invalid  sc
  # region @ 805CB8A8 (4 bytes)
  .data     0x805CB8A8  # address
  .data     0x00000004  # size
  .data     0x43480000  # 805CB8A8 => bc        26, 8, +0x00000000 /* 805CB8A8 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
