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
  .data     0x481AEC5D  # 8000B090 => bl        +0x001AEC5C /* 801B9CEC */
  .data     0x7FA3EB78  # 8000B094 => mr        r3, r29
  .data     0x481AEF2C  # 8000B098 => b         +0x001AEF2C /* 801B9FC4 */
  .data     0x881F0000  # 8000B09C => lbz       r0, [r31]
  .data     0x28090001  # 8000B0A0 => cmplwi    r9, 1
  .data     0x4082000C  # 8000B0A4 => bne       +0x0000000C /* 8000B0B0 */
  .data     0x881F0001  # 8000B0A8 => lbz       r0, [r31 + 0x0001]
  .data     0x3BFF0002  # 8000B0AC => addi      r31, r31, 0x0002
  .data     0x48100A44  # 8000B0B0 => b         +0x00100A44 /* 8010BAF4 */
  .data     0x39200000  # 8000B0B4 => li        r9, 0x0000
  .data     0x481009D5  # 8000B0B8 => bl        +0x001009D4 /* 8010BA8C */
  .data     0x7F43D378  # 8000B0BC => mr        r3, r26
  .data     0x7F64DB78  # 8000B0C0 => mr        r4, r27
  .data     0x7F85E378  # 8000B0C4 => mr        r5, r28
  .data     0x7FA6EB78  # 8000B0C8 => mr        r6, r29
  .data     0x7FC7F378  # 8000B0CC => mr        r7, r30
  .data     0x7FE8FB78  # 8000B0D0 => mr        r8, r31
  .data     0x39200001  # 8000B0D4 => li        r9, 0x0001
  .data     0x481009B5  # 8000B0D8 => bl        +0x001009B4 /* 8010BA8C */
  .data     0x48102E4C  # 8000B0DC => b         +0x00102E4C /* 8010DF28 */
  # region @ 8000B5C8 (20 bytes)
  .data     0x8000B5C8  # address
  .data     0x00000014  # size
  .data     0x80630098  # 8000B5C8 => lwz       r3, [r3 + 0x0098]
  .data     0x483D8D21  # 8000B5CC => bl        +0x003D8D20 /* 803E42EC */
  .data     0x807F042C  # 8000B5D0 => lwz       r3, [r31 + 0x042C]
  .data     0x809F0430  # 8000B5D4 => lwz       r4, [r31 + 0x0430]
  .data     0x48178D4C  # 8000B5D8 => b         +0x00178D4C /* 80184324 */
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
  .data     0x481654E4  # 8000BBEC => b         +0x001654E4 /* 801710D0 */
  # region @ 8000C3F8 (124 bytes)
  .data     0x8000C3F8  # address
  .data     0x0000007C  # size
  .data     0x28040000  # 8000C3F8 => cmplwi    r4, 0
  .data     0x4D820020  # 8000C3FC => beqlr
  .data     0x9421FFF0  # 8000C400 => stwu      [r1 - 0x0010], r1
  .data     0x481AD8EC  # 8000C404 => b         +0x001AD8EC /* 801B9CF0 */
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
  .data     0x480FEC5D  # 8000C43C => bl        +0x000FEC5C /* 8010B098 */
  .data     0x7F83E378  # 8000C440 => mr        r3, r28
  .data     0x38800001  # 8000C444 => li        r4, 0x0001
  .data     0x480FEDCD  # 8000C448 => bl        +0x000FEDCC /* 8010B214 */
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
  .data     0x4810C848  # 8000C650 => b         +0x0010C848 /* 80118E98 */
  # region @ 8000C6D0 (32 bytes)
  .data     0x8000C6D0  # address
  .data     0x00000020  # size
  .data     0x38000001  # 8000C6D0 => li        r0, 0x0001
  .data     0x901D0054  # 8000C6D4 => stw       [r29 + 0x0054], r0
  .data     0x807D0024  # 8000C6D8 => lwz       r3, [r29 + 0x0024]
  .data     0x48211FC4  # 8000C6DC => b         +0x00211FC4 /* 8021E6A0 */
  .data     0x38000001  # 8000C6E0 => li        r0, 0x0001
  .data     0x901F0378  # 8000C6E4 => stw       [r31 + 0x0378], r0
  .data     0x807F0024  # 8000C6E8 => lwz       r3, [r31 + 0x0024]
  .data     0x48215474  # 8000C6EC => b         +0x00215474 /* 80221B60 */
  # region @ 8000C8A0 (20 bytes)
  .data     0x8000C8A0  # address
  .data     0x00000014  # size
  .data     0x1C00000A  # 8000C8A0 => mulli     r0, r0, 10
  .data     0x57E407BD  # 8000C8A4 => rlwinm.   r4, r31, 0, 30, 30
  .data     0x41820008  # 8000C8A8 => beq       +0x00000008 /* 8000C8B0 */
  .data     0x7FA00734  # 8000C8AC => extsh     r0, r29
  .data     0x48105F44  # 8000C8B0 => b         +0x00105F44 /* 801127F4 */
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
  .data     0x482AF934  # 8000D990 => b         +0x002AF934 /* 802BD2C4 */
  # region @ 8000D9A0 (24 bytes)
  .data     0x8000D9A0  # address
  .data     0x00000018  # size
  .data     0xC042FC80  # 8000D9A0 => lfs       f2, [r2 - 0x0380]
  .data     0x807E0030  # 8000D9A4 => lwz       r3, [r30 + 0x0030]
  .data     0x70630020  # 8000D9A8 => andi.     r3, r3, 0x0020
  .data     0x41820008  # 8000D9AC => beq       +0x00000008 /* 8000D9B4 */
  .data     0xC042FC98  # 8000D9B0 => lfs       f2, [r2 - 0x0368]
  .data     0x483299EC  # 8000D9B4 => b         +0x003299EC /* 803373A0 */
  # region @ 8000E1E0 (28 bytes)
  .data     0x8000E1E0  # address
  .data     0x0000001C  # size
  .data     0x7FC802A6  # 8000E1E0 => mflr      r30
  .data     0x38A00000  # 8000E1E4 => li        r5, 0x0000
  .data     0x38C0001E  # 8000E1E8 => li        r6, 0x001E
  .data     0x38E00040  # 8000E1EC => li        r7, 0x0040
  .data     0x480786C5  # 8000E1F0 => bl        +0x000786C4 /* 800868B4 */
  .data     0x7FC803A6  # 8000E1F4 => mtlr      r30
  .data     0x4E800020  # 8000E1F8 => blr
  # region @ 8001304C (4 bytes)
  .data     0x8001304C  # address
  .data     0x00000004  # size
  .data     0x4BFFFCC0  # 8001304C => b         -0x00000340 /* 80012D0C */
  # region @ 800142BC (4 bytes)
  .data     0x800142BC  # address
  .data     0x00000004  # size
  .data     0x4BFF8605  # 800142BC => bl        -0x000079FC /* 8000C8C0 */
  # region @ 80015CE4 (4 bytes)
  .data     0x80015CE4  # address
  .data     0x00000004  # size
  .data     0x4BFF6BE1  # 80015CE4 => bl        -0x00009420 /* 8000C8C4 */
  # region @ 8009193C (8 bytes)
  .data     0x8009193C  # address
  .data     0x00000008  # size
  .data     0x4800024D  # 8009193C => bl        +0x0000024C /* 80091B88 */
  .data     0xB3C3032C  # 80091940 => sth       [r3 + 0x032C], r30
  # region @ 800BCB80 (4 bytes)
  .data     0x800BCB80  # address
  .data     0x00000004  # size
  .data     0x48000010  # 800BCB80 => b         +0x00000010 /* 800BCB90 */
  # region @ 80104CA4 (4 bytes)
  .data     0x80104CA4  # address
  .data     0x00000004  # size
  .data     0x4182000C  # 80104CA4 => beq       +0x0000000C /* 80104CB0 */
  # region @ 801075D4 (4 bytes)
  .data     0x801075D4  # address
  .data     0x00000004  # size
  .data     0x4800000C  # 801075D4 => b         +0x0000000C /* 801075E0 */
  # region @ 801075E8 (4 bytes)
  .data     0x801075E8  # address
  .data     0x00000004  # size
  .data     0x7C030378  # 801075E8 => mr        r3, r0
  # region @ 8010BAF0 (4 bytes)
  .data     0x8010BAF0  # address
  .data     0x00000004  # size
  .data     0x4BEFF5AC  # 8010BAF0 => b         -0x00100A54 /* 8000B09C */
  # region @ 8010DF24 (4 bytes)
  .data     0x8010DF24  # address
  .data     0x00000004  # size
  .data     0x4BEFD190  # 8010DF24 => b         -0x00102E70 /* 8000B0B4 */
  # region @ 801127F0 (4 bytes)
  .data     0x801127F0  # address
  .data     0x00000004  # size
  .data     0x4BEFA0B0  # 801127F0 => b         -0x00105F50 /* 8000C8A0 */
  # region @ 80114524 (4 bytes)
  .data     0x80114524  # address
  .data     0x00000004  # size
  .data     0x38000012  # 80114524 => li        r0, 0x0012
  # region @ 80118764 (4 bytes)
  .data     0x80118764  # address
  .data     0x00000004  # size
  .data     0x88040016  # 80118764 => lbz       r0, [r4 + 0x0016]
  # region @ 80118770 (4 bytes)
  .data     0x80118770  # address
  .data     0x00000004  # size
  .data     0x88040017  # 80118770 => lbz       r0, [r4 + 0x0017]
  # region @ 80118E94 (4 bytes)
  .data     0x80118E94  # address
  .data     0x00000004  # size
  .data     0x4BEF37AC  # 80118E94 => b         -0x0010C854 /* 8000C640 */
  # region @ 8011CC6C (12 bytes)
  .data     0x8011CC6C  # address
  .data     0x0000000C  # size
  .data     0x7C030378  # 8011CC6C => mr        r3, r0
  .data     0x3863FFFF  # 8011CC70 => subi      r3, r3, 0x0001
  .data     0x4BFFFFE8  # 8011CC74 => b         -0x00000018 /* 8011CC5C */
  # region @ 8011CD28 (12 bytes)
  .data     0x8011CD28  # address
  .data     0x0000000C  # size
  .data     0x7C030378  # 8011CD28 => mr        r3, r0
  .data     0x3863FFFF  # 8011CD2C => subi      r3, r3, 0x0001
  .data     0x4BFFFFE8  # 8011CD30 => b         -0x00000018 /* 8011CD18 */
  # region @ 8011CD78 (12 bytes)
  .data     0x8011CD78  # address
  .data     0x0000000C  # size
  .data     0x7C040378  # 8011CD78 => mr        r4, r0
  .data     0x3884FFFF  # 8011CD7C => subi      r4, r4, 0x0001
  .data     0x4BFFFFE8  # 8011CD80 => b         -0x00000018 /* 8011CD68 */
  # region @ 8016679C (8 bytes)
  .data     0x8016679C  # address
  .data     0x00000008  # size
  .data     0x3C604005  # 8016679C => lis       r3, 0x4005
  .data     0x4800009C  # 801667A0 => b         +0x0000009C /* 8016683C */
  # region @ 80166838 (4 bytes)
  .data     0x80166838  # address
  .data     0x00000004  # size
  .data     0x4800001C  # 80166838 => b         +0x0000001C /* 80166854 */
  # region @ 801710CC (4 bytes)
  .data     0x801710CC  # address
  .data     0x00000004  # size
  .data     0x4BE9AB04  # 801710CC => b         -0x001654FC /* 8000BBD0 */
  # region @ 801710EC (4 bytes)
  .data     0x801710EC  # address
  .data     0x00000004  # size
  .data     0x60800420  # 801710EC => ori       r0, r4, 0x0420
  # region @ 80184320 (4 bytes)
  .data     0x80184320  # address
  .data     0x00000004  # size
  .data     0x4BE872A8  # 80184320 => b         -0x00178D58 /* 8000B5C8 */
  # region @ 80184360 (4 bytes)
  .data     0x80184360  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80184360 => nop
  # region @ 80189EF0 (4 bytes)
  .data     0x80189EF0  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80189EF0 => nop
  # region @ 80193874 (4 bytes)
  .data     0x80193874  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80193874 => nop
  # region @ 801B9CEC (4 bytes)
  .data     0x801B9CEC  # address
  .data     0x00000004  # size
  .data     0x4BE5271C  # 801B9CEC => b         -0x001AD8E4 /* 8000C408 */
  # region @ 801B9FC0 (4 bytes)
  .data     0x801B9FC0  # address
  .data     0x00000004  # size
  .data     0x4BE510C8  # 801B9FC0 => b         -0x001AEF38 /* 8000B088 */
  # region @ 801C642C (4 bytes)
  .data     0x801C642C  # address
  .data     0x00000004  # size
  .data     0x389F02FC  # 801C642C => addi      r4, r31, 0x02FC
  # region @ 801CA7AC (4 bytes)
  .data     0x801CA7AC  # address
  .data     0x00000004  # size
  .data     0x48000010  # 801CA7AC => b         +0x00000010 /* 801CA7BC */
  # region @ 8021E69C (4 bytes)
  .data     0x8021E69C  # address
  .data     0x00000004  # size
  .data     0x4BDEE034  # 8021E69C => b         -0x00211FCC /* 8000C6D0 */
  # region @ 80221B5C (4 bytes)
  .data     0x80221B5C  # address
  .data     0x00000004  # size
  .data     0x4BDEAB84  # 80221B5C => b         -0x0021547C /* 8000C6E0 */
  # region @ 8022A990 (4 bytes)
  .data     0x8022A990  # address
  .data     0x00000004  # size
  .data     0x2C000001  # 8022A990 => cmpwi     r0, 1
  # region @ 8022B190 (4 bytes)
  .data     0x8022B190  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8022B190 => li        r4, 0xFFFFFF00
  # region @ 8022B1C0 (4 bytes)
  .data     0x8022B1C0  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 8022B1C0 => li        r4, 0xFFFFFE80
  # region @ 8022B1F0 (4 bytes)
  .data     0x8022B1F0  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 8022B1F0 => li        r4, 0xFFFFFDB0
  # region @ 8022DE8C (4 bytes)
  .data     0x8022DE8C  # address
  .data     0x00000004  # size
  .data     0x60000000  # 8022DE8C => nop
  # region @ 8022E5C0 (4 bytes)
  .data     0x8022E5C0  # address
  .data     0x00000004  # size
  .data     0x41810630  # 8022E5C0 => bgt       +0x00000630 /* 8022EBF0 */
  # region @ 8022F8E4 (4 bytes)
  .data     0x8022F8E4  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8022F8E4 => li        r4, 0xFFFFFF00
  # region @ 8022F914 (4 bytes)
  .data     0x8022F914  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 8022F914 => li        r4, 0xFFFFFE80
  # region @ 8022F944 (4 bytes)
  .data     0x8022F944  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 8022F944 => li        r4, 0xFFFFFDB0
  # region @ 802300F0 (4 bytes)
  .data     0x802300F0  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 802300F0 => li        r4, 0xFFFFFF00
  # region @ 80230120 (4 bytes)
  .data     0x80230120  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80230120 => li        r4, 0xFFFFFE80
  # region @ 80230150 (4 bytes)
  .data     0x80230150  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80230150 => li        r4, 0xFFFFFDB0
  # region @ 802316F4 (4 bytes)
  .data     0x802316F4  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 802316F4 => li        r4, 0xFFFFFF00
  # region @ 80231724 (4 bytes)
  .data     0x80231724  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80231724 => li        r4, 0xFFFFFE80
  # region @ 80231754 (4 bytes)
  .data     0x80231754  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80231754 => li        r4, 0xFFFFFDB0
  # region @ 80232464 (4 bytes)
  .data     0x80232464  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80232464 => li        r4, 0xFFFFFF00
  # region @ 80232494 (4 bytes)
  .data     0x80232494  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80232494 => li        r4, 0xFFFFFE80
  # region @ 802324C4 (4 bytes)
  .data     0x802324C4  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 802324C4 => li        r4, 0xFFFFFDB0
  # region @ 80232D58 (4 bytes)
  .data     0x80232D58  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80232D58 => li        r4, 0xFFFFFF00
  # region @ 80232D90 (4 bytes)
  .data     0x80232D90  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80232D90 => li        r4, 0xFFFFFE80
  # region @ 80232DC8 (4 bytes)
  .data     0x80232DC8  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80232DC8 => li        r4, 0xFFFFFDB0
  # region @ 80234E04 (4 bytes)
  .data     0x80234E04  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80234E04 => li        r4, 0xFFFFFF00
  # region @ 80234E34 (4 bytes)
  .data     0x80234E34  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80234E34 => li        r4, 0xFFFFFE80
  # region @ 80234E64 (4 bytes)
  .data     0x80234E64  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80234E64 => li        r4, 0xFFFFFDB0
  # region @ 80237430 (4 bytes)
  .data     0x80237430  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80237430 => li        r4, 0xFFFFFF00
  # region @ 8023746C (4 bytes)
  .data     0x8023746C  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 8023746C => li        r4, 0xFFFFFE80
  # region @ 802374A8 (4 bytes)
  .data     0x802374A8  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 802374A8 => li        r4, 0xFFFFFDB0
  # region @ 80237C08 (4 bytes)
  .data     0x80237C08  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80237C08 => li        r4, 0xFFFFFF00
  # region @ 80237C38 (4 bytes)
  .data     0x80237C38  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80237C38 => li        r4, 0xFFFFFE80
  # region @ 80237C68 (4 bytes)
  .data     0x80237C68  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80237C68 => li        r4, 0xFFFFFDB0
  # region @ 8023861C (4 bytes)
  .data     0x8023861C  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8023861C => li        r4, 0xFFFFFF00
  # region @ 8023864C (4 bytes)
  .data     0x8023864C  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 8023864C => li        r4, 0xFFFFFE80
  # region @ 8023867C (4 bytes)
  .data     0x8023867C  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 8023867C => li        r4, 0xFFFFFDB0
  # region @ 80238FF4 (4 bytes)
  .data     0x80238FF4  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80238FF4 => li        r4, 0xFFFFFF00
  # region @ 80239024 (4 bytes)
  .data     0x80239024  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80239024 => li        r4, 0xFFFFFE80
  # region @ 80239054 (4 bytes)
  .data     0x80239054  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80239054 => li        r4, 0xFFFFFDB0
  # region @ 8023C924 (4 bytes)
  .data     0x8023C924  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8023C924 => li        r4, 0xFFFFFF00
  # region @ 8023C954 (4 bytes)
  .data     0x8023C954  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 8023C954 => li        r4, 0xFFFFFE80
  # region @ 8023C984 (4 bytes)
  .data     0x8023C984  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 8023C984 => li        r4, 0xFFFFFDB0
  # region @ 802519A4 (4 bytes)
  .data     0x802519A4  # address
  .data     0x00000004  # size
  .data     0x60000000  # 802519A4 => nop
  # region @ 80269898 (4 bytes)
  .data     0x80269898  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80269898 => nop
  # region @ 8026F548 (4 bytes)
  .data     0x8026F548  # address
  .data     0x00000004  # size
  .data     0x3884AAFA  # 8026F548 => subi      r4, r4, 0x5506
  # region @ 8026F65C (4 bytes)
  .data     0x8026F65C  # address
  .data     0x00000004  # size
  .data     0x3863AAFA  # 8026F65C => subi      r3, r3, 0x5506
  # region @ 8026F6E4 (4 bytes)
  .data     0x8026F6E4  # address
  .data     0x00000004  # size
  .data     0x3883AAFA  # 8026F6E4 => subi      r4, r3, 0x5506
  # region @ 802BD2C0 (4 bytes)
  .data     0x802BD2C0  # address
  .data     0x00000004  # size
  .data     0x4BD506C0  # 802BD2C0 => b         -0x002AF940 /* 8000D980 */
  # region @ 802FDB6C (4 bytes)
  .data     0x802FDB6C  # address
  .data     0x00000004  # size
  .data     0x2C030001  # 802FDB6C => cmpwi     r3, 1
  # region @ 803037D0 (28 bytes)
  .data     0x803037D0  # address
  .data     0x0000001C  # size
  .data     0x48000020  # 803037D0 => b         +0x00000020 /* 803037F0 */
  .data     0x3863A830  # 803037D4 => subi      r3, r3, 0x57D0
  .data     0x800DB9B4  # 803037D8 => lwz       r0, [r13 - 0x464C]
  .data     0x2C000023  # 803037DC => cmpwi     r0, 35
  .data     0x40820008  # 803037E0 => bne       +0x00000008 /* 803037E8 */
  .data     0x3863FB28  # 803037E4 => subi      r3, r3, 0x04D8
  .data     0x4800008C  # 803037E8 => b         +0x0000008C /* 80303874 */
  # region @ 80303870 (4 bytes)
  .data     0x80303870  # address
  .data     0x00000004  # size
  .data     0x4BFFFF64  # 80303870 => b         -0x0000009C /* 803037D4 */
  # region @ 8033739C (4 bytes)
  .data     0x8033739C  # address
  .data     0x00000004  # size
  .data     0x4BCD6604  # 8033739C => b         -0x003299FC /* 8000D9A0 */
  # region @ 80358074 (4 bytes)
  .data     0x80358074  # address
  .data     0x00000004  # size
  .data     0x388001E8  # 80358074 => li        r4, 0x01E8
  # region @ 80358098 (4 bytes)
  .data     0x80358098  # address
  .data     0x00000004  # size
  .data     0x4BCB6149  # 80358098 => bl        -0x00349EB8 /* 8000E1E0 */
  # region @ 80358108 (4 bytes)
  .data     0x80358108  # address
  .data     0x00000004  # size
  .data     0x388001E8  # 80358108 => li        r4, 0x01E8
  # region @ 80358118 (4 bytes)
  .data     0x80358118  # address
  .data     0x00000004  # size
  .data     0x4BCB60C9  # 80358118 => bl        -0x00349F38 /* 8000E1E0 */
  # region @ 804B90B8 (8 bytes)
  .data     0x804B90B8  # address
  .data     0x00000008  # size
  .data     0x70808080  # 804B90B8 => andi.     r0, r4, 0x8080
  .data     0x60707070  # 804B90BC => ori       r16, r3, 0x7070
  # region @ 804CC90C (4 bytes)
  .data     0x804CC90C  # address
  .data     0x00000004  # size
  .data     0x0000001E  # 804CC90C => .invalid
  # region @ 804CC964 (4 bytes)
  .data     0x804CC964  # address
  .data     0x00000004  # size
  .data     0x00000028  # 804CC964 => .invalid
  # region @ 804CC990 (4 bytes)
  .data     0x804CC990  # address
  .data     0x00000004  # size
  .data     0x00000032  # 804CC990 => .invalid
  # region @ 804CC9BC (4 bytes)
  .data     0x804CC9BC  # address
  .data     0x00000004  # size
  .data     0x0000003C  # 804CC9BC => .invalid
  # region @ 804CC9CC (4 bytes)
  .data     0x804CC9CC  # address
  .data     0x00000004  # size
  .data     0x0018003C  # 804CC9CC => .invalid
  # region @ 804CCC24 (4 bytes)
  .data     0x804CCC24  # address
  .data     0x00000004  # size
  .data     0x00000028  # 804CCC24 => .invalid
  # region @ 804D1580 (4 bytes)
  .data     0x804D1580  # address
  .data     0x00000004  # size
  .data     0xFF0074EE  # 804D1580 => fsel      f24, f0, f14, f19
  # region @ 805DB1AC (4 bytes)
  .data     0x805DB1AC  # address
  .data     0x00000004  # size
  .data     0x435C0000  # 805DB1AC => bc        26, 28, +0x00000000 /* 805DB1AC */
  # region @ 805DCE48 (4 bytes)
  .data     0x805DCE48  # address
  .data     0x00000004  # size
  .data     0x46AFC800  # 805DCE48 => .invalid  sc
  # region @ 805DD0E8 (4 bytes)
  .data     0x805DD0E8  # address
  .data     0x00000004  # size
  .data     0x43480000  # 805DD0E8 => bc        26, 8, +0x00000000 /* 805DD0E8 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
