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
  .data     0x481AEB11  # 8000B090 => bl        +0x001AEB10 /* 801B9BA0 */
  .data     0x7FA3EB78  # 8000B094 => mr        r3, r29
  .data     0x481AEDE0  # 8000B098 => b         +0x001AEDE0 /* 801B9E78 */
  .data     0x881F0000  # 8000B09C => lbz       r0, [r31]
  .data     0x28090001  # 8000B0A0 => cmplwi    r9, 1
  .data     0x4082000C  # 8000B0A4 => bne       +0x0000000C /* 8000B0B0 */
  .data     0x881F0001  # 8000B0A8 => lbz       r0, [r31 + 0x0001]
  .data     0x3BFF0002  # 8000B0AC => addi      r31, r31, 0x0002
  .data     0x48100B68  # 8000B0B0 => b         +0x00100B68 /* 8010BC18 */
  .data     0x39200000  # 8000B0B4 => li        r9, 0x0000
  .data     0x48100AF9  # 8000B0B8 => bl        +0x00100AF8 /* 8010BBB0 */
  .data     0x7F43D378  # 8000B0BC => mr        r3, r26
  .data     0x7F64DB78  # 8000B0C0 => mr        r4, r27
  .data     0x7F85E378  # 8000B0C4 => mr        r5, r28
  .data     0x7FA6EB78  # 8000B0C8 => mr        r6, r29
  .data     0x7FC7F378  # 8000B0CC => mr        r7, r30
  .data     0x7FE8FB78  # 8000B0D0 => mr        r8, r31
  .data     0x39200001  # 8000B0D4 => li        r9, 0x0001
  .data     0x48100AD9  # 8000B0D8 => bl        +0x00100AD8 /* 8010BBB0 */
  .data     0x48102F64  # 8000B0DC => b         +0x00102F64 /* 8010E040 */
  # region @ 8000B5C8 (20 bytes)
  .data     0x8000B5C8  # address
  .data     0x00000014  # size
  .data     0x80630098  # 8000B5C8 => lwz       r3, [r3 + 0x0098]
  .data     0x483D59F1  # 8000B5CC => bl        +0x003D59F0 /* 803E0FBC */
  .data     0x807F042C  # 8000B5D0 => lwz       r3, [r31 + 0x042C]
  .data     0x809F0430  # 8000B5D4 => lwz       r4, [r31 + 0x0430]
  .data     0x48178C7C  # 8000B5D8 => b         +0x00178C7C /* 80184254 */
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
  .data     0x48165428  # 8000BBEC => b         +0x00165428 /* 80171014 */
  # region @ 8000C3F8 (124 bytes)
  .data     0x8000C3F8  # address
  .data     0x0000007C  # size
  .data     0x28040000  # 8000C3F8 => cmplwi    r4, 0
  .data     0x4D820020  # 8000C3FC => beqlr
  .data     0x9421FFF0  # 8000C400 => stwu      [r1 - 0x0010], r1
  .data     0x481AD7A0  # 8000C404 => b         +0x001AD7A0 /* 801B9BA4 */
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
  .data     0x480FED81  # 8000C43C => bl        +0x000FED80 /* 8010B1BC */
  .data     0x7F83E378  # 8000C440 => mr        r3, r28
  .data     0x38800001  # 8000C444 => li        r4, 0x0001
  .data     0x480FEEF1  # 8000C448 => bl        +0x000FEEF0 /* 8010B338 */
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
  .data     0x4810C938  # 8000C650 => b         +0x0010C938 /* 80118F88 */
  # region @ 8000C6D0 (32 bytes)
  .data     0x8000C6D0  # address
  .data     0x00000020  # size
  .data     0x38000001  # 8000C6D0 => li        r0, 0x0001
  .data     0x901D0054  # 8000C6D4 => stw       [r29 + 0x0054], r0
  .data     0x807D0024  # 8000C6D8 => lwz       r3, [r29 + 0x0024]
  .data     0x48211244  # 8000C6DC => b         +0x00211244 /* 8021D920 */
  .data     0x38000001  # 8000C6E0 => li        r0, 0x0001
  .data     0x901F0378  # 8000C6E4 => stw       [r31 + 0x0378], r0
  .data     0x807F0024  # 8000C6E8 => lwz       r3, [r31 + 0x0024]
  .data     0x482146F4  # 8000C6EC => b         +0x002146F4 /* 80220DE0 */
  # region @ 8000C8A0 (20 bytes)
  .data     0x8000C8A0  # address
  .data     0x00000014  # size
  .data     0x1C00000A  # 8000C8A0 => mulli     r0, r0, 10
  .data     0x57E407BD  # 8000C8A4 => rlwinm.   r4, r31, 0, 30, 30
  .data     0x41820008  # 8000C8A8 => beq       +0x00000008 /* 8000C8B0 */
  .data     0x7FA00734  # 8000C8AC => extsh     r0, r29
  .data     0x4810605C  # 8000C8B0 => b         +0x0010605C /* 8011290C */
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
  .data     0x482AE5AC  # 8000D990 => b         +0x002AE5AC /* 802BBF3C */
  # region @ 8000D9A0 (24 bytes)
  .data     0x8000D9A0  # address
  .data     0x00000018  # size
  .data     0xC042FC88  # 8000D9A0 => lfs       f2, [r2 - 0x0378]
  .data     0x807E0030  # 8000D9A4 => lwz       r3, [r30 + 0x0030]
  .data     0x70630020  # 8000D9A8 => andi.     r3, r3, 0x0020
  .data     0x41820008  # 8000D9AC => beq       +0x00000008 /* 8000D9B4 */
  .data     0xC042FCA0  # 8000D9B0 => lfs       f2, [r2 - 0x0360]
  .data     0x483280E4  # 8000D9B4 => b         +0x003280E4 /* 80335A98 */
  # region @ 8000E1E0 (28 bytes)
  .data     0x8000E1E0  # address
  .data     0x0000001C  # size
  .data     0x7FC802A6  # 8000E1E0 => mflr      r30
  .data     0x38A00000  # 8000E1E4 => li        r5, 0x0000
  .data     0x38C0001E  # 8000E1E8 => li        r6, 0x001E
  .data     0x38E00040  # 8000E1EC => li        r7, 0x0040
  .data     0x4807853D  # 8000E1F0 => bl        +0x0007853C /* 8008672C */
  .data     0x7FC803A6  # 8000E1F4 => mtlr      r30
  .data     0x4E800020  # 8000E1F8 => blr
  # region @ 80013084 (4 bytes)
  .data     0x80013084  # address
  .data     0x00000004  # size
  .data     0x4BFFFCC0  # 80013084 => b         -0x00000340 /* 80012D44 */
  # region @ 800142F4 (4 bytes)
  .data     0x800142F4  # address
  .data     0x00000004  # size
  .data     0x4BFF85CD  # 800142F4 => bl        -0x00007A34 /* 8000C8C0 */
  # region @ 80015D1C (4 bytes)
  .data     0x80015D1C  # address
  .data     0x00000004  # size
  .data     0x4BFF6BA9  # 80015D1C => bl        -0x00009458 /* 8000C8C4 */
  # region @ 800917B4 (8 bytes)
  .data     0x800917B4  # address
  .data     0x00000008  # size
  .data     0x4800024D  # 800917B4 => bl        +0x0000024C /* 80091A00 */
  .data     0xB3C3032C  # 800917B8 => sth       [r3 + 0x032C], r30
  # region @ 800BC9E8 (4 bytes)
  .data     0x800BC9E8  # address
  .data     0x00000004  # size
  .data     0x48000010  # 800BC9E8 => b         +0x00000010 /* 800BC9F8 */
  # region @ 80101EB8 (4 bytes)
  .data     0x80101EB8  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80101EB8 => nop
  # region @ 80104DEC (4 bytes)
  .data     0x80104DEC  # address
  .data     0x00000004  # size
  .data     0x4182000C  # 80104DEC => beq       +0x0000000C /* 80104DF8 */
  # region @ 8010771C (4 bytes)
  .data     0x8010771C  # address
  .data     0x00000004  # size
  .data     0x4800000C  # 8010771C => b         +0x0000000C /* 80107728 */
  # region @ 80107730 (4 bytes)
  .data     0x80107730  # address
  .data     0x00000004  # size
  .data     0x7C030378  # 80107730 => mr        r3, r0
  # region @ 8010BC14 (4 bytes)
  .data     0x8010BC14  # address
  .data     0x00000004  # size
  .data     0x4BEFF488  # 8010BC14 => b         -0x00100B78 /* 8000B09C */
  # region @ 8010E03C (4 bytes)
  .data     0x8010E03C  # address
  .data     0x00000004  # size
  .data     0x4BEFD078  # 8010E03C => b         -0x00102F88 /* 8000B0B4 */
  # region @ 80112908 (4 bytes)
  .data     0x80112908  # address
  .data     0x00000004  # size
  .data     0x4BEF9F98  # 80112908 => b         -0x00106068 /* 8000C8A0 */
  # region @ 8011461C (4 bytes)
  .data     0x8011461C  # address
  .data     0x00000004  # size
  .data     0x38000012  # 8011461C => li        r0, 0x0012
  # region @ 80118854 (4 bytes)
  .data     0x80118854  # address
  .data     0x00000004  # size
  .data     0x88040016  # 80118854 => lbz       r0, [r4 + 0x0016]
  # region @ 80118860 (4 bytes)
  .data     0x80118860  # address
  .data     0x00000004  # size
  .data     0x88040017  # 80118860 => lbz       r0, [r4 + 0x0017]
  # region @ 80118F84 (4 bytes)
  .data     0x80118F84  # address
  .data     0x00000004  # size
  .data     0x4BEF36BC  # 80118F84 => b         -0x0010C944 /* 8000C640 */
  # region @ 8011CD34 (12 bytes)
  .data     0x8011CD34  # address
  .data     0x0000000C  # size
  .data     0x7C030378  # 8011CD34 => mr        r3, r0
  .data     0x3863FFFF  # 8011CD38 => subi      r3, r3, 0x0001
  .data     0x4BFFFFE8  # 8011CD3C => b         -0x00000018 /* 8011CD24 */
  # region @ 8011CDF0 (12 bytes)
  .data     0x8011CDF0  # address
  .data     0x0000000C  # size
  .data     0x7C030378  # 8011CDF0 => mr        r3, r0
  .data     0x3863FFFF  # 8011CDF4 => subi      r3, r3, 0x0001
  .data     0x4BFFFFE8  # 8011CDF8 => b         -0x00000018 /* 8011CDE0 */
  # region @ 8011CE40 (12 bytes)
  .data     0x8011CE40  # address
  .data     0x0000000C  # size
  .data     0x7C040378  # 8011CE40 => mr        r4, r0
  .data     0x3884FFFF  # 8011CE44 => subi      r4, r4, 0x0001
  .data     0x4BFFFFE8  # 8011CE48 => b         -0x00000018 /* 8011CE30 */
  # region @ 801666E0 (8 bytes)
  .data     0x801666E0  # address
  .data     0x00000008  # size
  .data     0x3C604005  # 801666E0 => lis       r3, 0x4005
  .data     0x4800009C  # 801666E4 => b         +0x0000009C /* 80166780 */
  # region @ 8016677C (4 bytes)
  .data     0x8016677C  # address
  .data     0x00000004  # size
  .data     0x4800001C  # 8016677C => b         +0x0000001C /* 80166798 */
  # region @ 80171010 (4 bytes)
  .data     0x80171010  # address
  .data     0x00000004  # size
  .data     0x4BE9ABC0  # 80171010 => b         -0x00165440 /* 8000BBD0 */
  # region @ 80171030 (4 bytes)
  .data     0x80171030  # address
  .data     0x00000004  # size
  .data     0x60800420  # 80171030 => ori       r0, r4, 0x0420
  # region @ 80184250 (4 bytes)
  .data     0x80184250  # address
  .data     0x00000004  # size
  .data     0x4BE87378  # 80184250 => b         -0x00178C88 /* 8000B5C8 */
  # region @ 80184290 (4 bytes)
  .data     0x80184290  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80184290 => nop
  # region @ 80189E20 (4 bytes)
  .data     0x80189E20  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80189E20 => nop
  # region @ 801937A8 (4 bytes)
  .data     0x801937A8  # address
  .data     0x00000004  # size
  .data     0x60000000  # 801937A8 => nop
  # region @ 801B9BA0 (4 bytes)
  .data     0x801B9BA0  # address
  .data     0x00000004  # size
  .data     0x4BE52868  # 801B9BA0 => b         -0x001AD798 /* 8000C408 */
  # region @ 801B9E74 (4 bytes)
  .data     0x801B9E74  # address
  .data     0x00000004  # size
  .data     0x4BE51214  # 801B9E74 => b         -0x001AEDEC /* 8000B088 */
  # region @ 801C62C0 (4 bytes)
  .data     0x801C62C0  # address
  .data     0x00000004  # size
  .data     0x389F02FC  # 801C62C0 => addi      r4, r31, 0x02FC
  # region @ 801CA610 (4 bytes)
  .data     0x801CA610  # address
  .data     0x00000004  # size
  .data     0x48000010  # 801CA610 => b         +0x00000010 /* 801CA620 */
  # region @ 8021D91C (4 bytes)
  .data     0x8021D91C  # address
  .data     0x00000004  # size
  .data     0x4BDEEDB4  # 8021D91C => b         -0x0021124C /* 8000C6D0 */
  # region @ 80220DDC (4 bytes)
  .data     0x80220DDC  # address
  .data     0x00000004  # size
  .data     0x4BDEB904  # 80220DDC => b         -0x002146FC /* 8000C6E0 */
  # region @ 80229C10 (4 bytes)
  .data     0x80229C10  # address
  .data     0x00000004  # size
  .data     0x2C000001  # 80229C10 => cmpwi     r0, 1
  # region @ 8022A410 (4 bytes)
  .data     0x8022A410  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8022A410 => li        r4, 0xFFFFFF00
  # region @ 8022A440 (4 bytes)
  .data     0x8022A440  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 8022A440 => li        r4, 0xFFFFFE80
  # region @ 8022A470 (4 bytes)
  .data     0x8022A470  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 8022A470 => li        r4, 0xFFFFFDB0
  # region @ 8022D10C (4 bytes)
  .data     0x8022D10C  # address
  .data     0x00000004  # size
  .data     0x60000000  # 8022D10C => nop
  # region @ 8022D840 (4 bytes)
  .data     0x8022D840  # address
  .data     0x00000004  # size
  .data     0x41810630  # 8022D840 => bgt       +0x00000630 /* 8022DE70 */
  # region @ 8022DB34 (4 bytes)
  .data     0x8022DB34  # address
  .data     0x00000004  # size
  .data     0x4181033C  # 8022DB34 => bgt       +0x0000033C /* 8022DE70 */
  # region @ 8022DC28 (4 bytes)
  .data     0x8022DC28  # address
  .data     0x00000004  # size
  .data     0x41810248  # 8022DC28 => bgt       +0x00000248 /* 8022DE70 */
  # region @ 8022EB64 (4 bytes)
  .data     0x8022EB64  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8022EB64 => li        r4, 0xFFFFFF00
  # region @ 8022EB94 (4 bytes)
  .data     0x8022EB94  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 8022EB94 => li        r4, 0xFFFFFE80
  # region @ 8022EBC4 (4 bytes)
  .data     0x8022EBC4  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 8022EBC4 => li        r4, 0xFFFFFDB0
  # region @ 8022F370 (4 bytes)
  .data     0x8022F370  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8022F370 => li        r4, 0xFFFFFF00
  # region @ 8022F3A0 (4 bytes)
  .data     0x8022F3A0  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 8022F3A0 => li        r4, 0xFFFFFE80
  # region @ 8022F3D0 (4 bytes)
  .data     0x8022F3D0  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 8022F3D0 => li        r4, 0xFFFFFDB0
  # region @ 80230974 (4 bytes)
  .data     0x80230974  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80230974 => li        r4, 0xFFFFFF00
  # region @ 802309A4 (4 bytes)
  .data     0x802309A4  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 802309A4 => li        r4, 0xFFFFFE80
  # region @ 802309D4 (4 bytes)
  .data     0x802309D4  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 802309D4 => li        r4, 0xFFFFFDB0
  # region @ 802316E4 (4 bytes)
  .data     0x802316E4  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 802316E4 => li        r4, 0xFFFFFF00
  # region @ 80231714 (4 bytes)
  .data     0x80231714  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80231714 => li        r4, 0xFFFFFE80
  # region @ 80231744 (4 bytes)
  .data     0x80231744  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80231744 => li        r4, 0xFFFFFDB0
  # region @ 80231FD8 (4 bytes)
  .data     0x80231FD8  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80231FD8 => li        r4, 0xFFFFFF00
  # region @ 80232010 (4 bytes)
  .data     0x80232010  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80232010 => li        r4, 0xFFFFFE80
  # region @ 80232048 (4 bytes)
  .data     0x80232048  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80232048 => li        r4, 0xFFFFFDB0
  # region @ 80234084 (4 bytes)
  .data     0x80234084  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80234084 => li        r4, 0xFFFFFF00
  # region @ 802340B4 (4 bytes)
  .data     0x802340B4  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 802340B4 => li        r4, 0xFFFFFE80
  # region @ 802340E4 (4 bytes)
  .data     0x802340E4  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 802340E4 => li        r4, 0xFFFFFDB0
  # region @ 802366B0 (4 bytes)
  .data     0x802366B0  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 802366B0 => li        r4, 0xFFFFFF00
  # region @ 802366EC (4 bytes)
  .data     0x802366EC  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 802366EC => li        r4, 0xFFFFFE80
  # region @ 80236728 (4 bytes)
  .data     0x80236728  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80236728 => li        r4, 0xFFFFFDB0
  # region @ 80236E88 (4 bytes)
  .data     0x80236E88  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80236E88 => li        r4, 0xFFFFFF00
  # region @ 80236EB8 (4 bytes)
  .data     0x80236EB8  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80236EB8 => li        r4, 0xFFFFFE80
  # region @ 80236EE8 (4 bytes)
  .data     0x80236EE8  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80236EE8 => li        r4, 0xFFFFFDB0
  # region @ 8023789C (4 bytes)
  .data     0x8023789C  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8023789C => li        r4, 0xFFFFFF00
  # region @ 802378CC (4 bytes)
  .data     0x802378CC  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 802378CC => li        r4, 0xFFFFFE80
  # region @ 802378FC (4 bytes)
  .data     0x802378FC  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 802378FC => li        r4, 0xFFFFFDB0
  # region @ 80238274 (4 bytes)
  .data     0x80238274  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80238274 => li        r4, 0xFFFFFF00
  # region @ 802382A4 (4 bytes)
  .data     0x802382A4  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 802382A4 => li        r4, 0xFFFFFE80
  # region @ 802382D4 (4 bytes)
  .data     0x802382D4  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 802382D4 => li        r4, 0xFFFFFDB0
  # region @ 8023BBA4 (4 bytes)
  .data     0x8023BBA4  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8023BBA4 => li        r4, 0xFFFFFF00
  # region @ 8023BBD4 (4 bytes)
  .data     0x8023BBD4  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 8023BBD4 => li        r4, 0xFFFFFE80
  # region @ 8023BC04 (4 bytes)
  .data     0x8023BC04  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 8023BC04 => li        r4, 0xFFFFFDB0
  # region @ 80250AEC (4 bytes)
  .data     0x80250AEC  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80250AEC => nop
  # region @ 80268788 (4 bytes)
  .data     0x80268788  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80268788 => nop
  # region @ 8026E2D4 (4 bytes)
  .data     0x8026E2D4  # address
  .data     0x00000004  # size
  .data     0x3884AAFA  # 8026E2D4 => subi      r4, r4, 0x5506
  # region @ 8026E3E8 (4 bytes)
  .data     0x8026E3E8  # address
  .data     0x00000004  # size
  .data     0x3863AAFA  # 8026E3E8 => subi      r3, r3, 0x5506
  # region @ 8026E470 (4 bytes)
  .data     0x8026E470  # address
  .data     0x00000004  # size
  .data     0x3883AAFA  # 8026E470 => subi      r4, r3, 0x5506
  # region @ 802BBF38 (4 bytes)
  .data     0x802BBF38  # address
  .data     0x00000004  # size
  .data     0x4BD51A48  # 802BBF38 => b         -0x002AE5B8 /* 8000D980 */
  # region @ 802FC338 (4 bytes)
  .data     0x802FC338  # address
  .data     0x00000004  # size
  .data     0x2C030001  # 802FC338 => cmpwi     r3, 1
  # region @ 80301F9C (28 bytes)
  .data     0x80301F9C  # address
  .data     0x0000001C  # size
  .data     0x48000020  # 80301F9C => b         +0x00000020 /* 80301FBC */
  .data     0x3863A830  # 80301FA0 => subi      r3, r3, 0x57D0
  .data     0x800DB9A4  # 80301FA4 => lwz       r0, [r13 - 0x465C]
  .data     0x2C000023  # 80301FA8 => cmpwi     r0, 35
  .data     0x40820008  # 80301FAC => bne       +0x00000008 /* 80301FB4 */
  .data     0x3863FB28  # 80301FB0 => subi      r3, r3, 0x04D8
  .data     0x4800008C  # 80301FB4 => b         +0x0000008C /* 80302040 */
  # region @ 8030203C (4 bytes)
  .data     0x8030203C  # address
  .data     0x00000004  # size
  .data     0x4BFFFF64  # 8030203C => b         -0x0000009C /* 80301FA0 */
  # region @ 80335A94 (4 bytes)
  .data     0x80335A94  # address
  .data     0x00000004  # size
  .data     0x4BCD7F0C  # 80335A94 => b         -0x003280F4 /* 8000D9A0 */
  # region @ 80356858 (4 bytes)
  .data     0x80356858  # address
  .data     0x00000004  # size
  .data     0x388001E8  # 80356858 => li        r4, 0x01E8
  # region @ 8035687C (4 bytes)
  .data     0x8035687C  # address
  .data     0x00000004  # size
  .data     0x4BCB7965  # 8035687C => bl        -0x0034869C /* 8000E1E0 */
  # region @ 803568EC (4 bytes)
  .data     0x803568EC  # address
  .data     0x00000004  # size
  .data     0x388001E8  # 803568EC => li        r4, 0x01E8
  # region @ 803568FC (4 bytes)
  .data     0x803568FC  # address
  .data     0x00000004  # size
  .data     0x4BCB78E5  # 803568FC => bl        -0x0034871C /* 8000E1E0 */
  # region @ 804B43D0 (8 bytes)
  .data     0x804B43D0  # address
  .data     0x00000008  # size
  .data     0x70808080  # 804B43D0 => andi.     r0, r4, 0x8080
  .data     0x60707070  # 804B43D4 => ori       r16, r3, 0x7070
  # region @ 804C7B94 (4 bytes)
  .data     0x804C7B94  # address
  .data     0x00000004  # size
  .data     0x0000001E  # 804C7B94 => .invalid
  # region @ 804C7BEC (4 bytes)
  .data     0x804C7BEC  # address
  .data     0x00000004  # size
  .data     0x00000028  # 804C7BEC => .invalid
  # region @ 804C7C18 (4 bytes)
  .data     0x804C7C18  # address
  .data     0x00000004  # size
  .data     0x00000032  # 804C7C18 => .invalid
  # region @ 804C7C44 (4 bytes)
  .data     0x804C7C44  # address
  .data     0x00000004  # size
  .data     0x0000003C  # 804C7C44 => .invalid
  # region @ 804C7C54 (4 bytes)
  .data     0x804C7C54  # address
  .data     0x00000004  # size
  .data     0x0018003C  # 804C7C54 => .invalid
  # region @ 804C7EAC (4 bytes)
  .data     0x804C7EAC  # address
  .data     0x00000004  # size
  .data     0x00000028  # 804C7EAC => .invalid
  # region @ 804CC7F0 (4 bytes)
  .data     0x804CC7F0  # address
  .data     0x00000004  # size
  .data     0xFF0074EE  # 804CC7F0 => fsel      f24, f0, f14, f19
  # region @ 805D1294 (4 bytes)
  .data     0x805D1294  # address
  .data     0x00000004  # size
  .data     0x435C0000  # 805D1294 => bc        26, 28, +0x00000000 /* 805D1294 */
  # region @ 805D2F30 (4 bytes)
  .data     0x805D2F30  # address
  .data     0x00000004  # size
  .data     0x46AFC800  # 805D2F30 => .invalid  sc
  # region @ 805D31D0 (4 bytes)
  .data     0x805D31D0  # address
  .data     0x00000004  # size
  .data     0x43480000  # 805D31D0 => bc        26, 8, +0x00000000 /* 805D31D0 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
