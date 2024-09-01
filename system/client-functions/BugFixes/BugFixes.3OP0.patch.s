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
  .data     0x481AF17D  # 8000B090 => bl        +0x001AF17C /* 801BA20C */
  .data     0x7FA3EB78  # 8000B094 => mr        r3, r29
  .data     0x481AF44C  # 8000B098 => b         +0x001AF44C /* 801BA4E4 */
  .data     0x881F0000  # 8000B09C => lbz       r0, [r31]
  .data     0x28090001  # 8000B0A0 => cmplwi    r9, 1
  .data     0x4082000C  # 8000B0A4 => bne       +0x0000000C /* 8000B0B0 */
  .data     0x881F0001  # 8000B0A8 => lbz       r0, [r31 + 0x0001]
  .data     0x3BFF0002  # 8000B0AC => addi      r31, r31, 0x0002
  .data     0x48100C44  # 8000B0B0 => b         +0x00100C44 /* 8010BCF4 */
  .data     0x39200000  # 8000B0B4 => li        r9, 0x0000
  .data     0x48100BD5  # 8000B0B8 => bl        +0x00100BD4 /* 8010BC8C */
  .data     0x7F43D378  # 8000B0BC => mr        r3, r26
  .data     0x7F64DB78  # 8000B0C0 => mr        r4, r27
  .data     0x7F85E378  # 8000B0C4 => mr        r5, r28
  .data     0x7FA6EB78  # 8000B0C8 => mr        r6, r29
  .data     0x7FC7F378  # 8000B0CC => mr        r7, r30
  .data     0x7FE8FB78  # 8000B0D0 => mr        r8, r31
  .data     0x39200001  # 8000B0D4 => li        r9, 0x0001
  .data     0x48100BB5  # 8000B0D8 => bl        +0x00100BB4 /* 8010BC8C */
  .data     0x48103040  # 8000B0DC => b         +0x00103040 /* 8010E11C */
  # region @ 8000B5C8 (20 bytes)
  .data     0x8000B5C8  # address
  .data     0x00000014  # size
  .data     0x80630098  # 8000B5C8 => lwz       r3, [r3 + 0x0098]
  .data     0x483D7BE1  # 8000B5CC => bl        +0x003D7BE0 /* 803E31AC */
  .data     0x807F042C  # 8000B5D0 => lwz       r3, [r31 + 0x042C]
  .data     0x809F0430  # 8000B5D4 => lwz       r4, [r31 + 0x0430]
  .data     0x48179274  # 8000B5D8 => b         +0x00179274 /* 8018484C */
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
  .data     0x48165A0C  # 8000BBEC => b         +0x00165A0C /* 801715F8 */
  # region @ 8000C3F8 (124 bytes)
  .data     0x8000C3F8  # address
  .data     0x0000007C  # size
  .data     0x28040000  # 8000C3F8 => cmplwi    r4, 0
  .data     0x4D820020  # 8000C3FC => beqlr
  .data     0x9421FFF0  # 8000C400 => stwu      [r1 - 0x0010], r1
  .data     0x481ADE0C  # 8000C404 => b         +0x001ADE0C /* 801BA210 */
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
  .data     0x480FEE5D  # 8000C43C => bl        +0x000FEE5C /* 8010B298 */
  .data     0x7F83E378  # 8000C440 => mr        r3, r28
  .data     0x38800001  # 8000C444 => li        r4, 0x0001
  .data     0x480FEFCD  # 8000C448 => bl        +0x000FEFCC /* 8010B414 */
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
  .data     0x4810CA30  # 8000C650 => b         +0x0010CA30 /* 80119080 */
  # region @ 8000C6D0 (32 bytes)
  .data     0x8000C6D0  # address
  .data     0x00000020  # size
  .data     0x38000001  # 8000C6D0 => li        r0, 0x0001
  .data     0x901D0054  # 8000C6D4 => stw       [r29 + 0x0054], r0
  .data     0x807D0024  # 8000C6D8 => lwz       r3, [r29 + 0x0024]
  .data     0x48211B90  # 8000C6DC => b         +0x00211B90 /* 8021E26C */
  .data     0x38000001  # 8000C6E0 => li        r0, 0x0001
  .data     0x901F0378  # 8000C6E4 => stw       [r31 + 0x0378], r0
  .data     0x807F0024  # 8000C6E8 => lwz       r3, [r31 + 0x0024]
  .data     0x48215040  # 8000C6EC => b         +0x00215040 /* 8022172C */
  # region @ 8000C8A0 (20 bytes)
  .data     0x8000C8A0  # address
  .data     0x00000014  # size
  .data     0x1C00000A  # 8000C8A0 => mulli     r0, r0, 10
  .data     0x57E407BD  # 8000C8A4 => rlwinm.   r4, r31, 0, 30, 30
  .data     0x41820008  # 8000C8A8 => beq       +0x00000008 /* 8000C8B0 */
  .data     0x7FA00734  # 8000C8AC => extsh     r0, r29
  .data     0x48106138  # 8000C8B0 => b         +0x00106138 /* 801129E8 */
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
  .data     0x482AF27C  # 8000D990 => b         +0x002AF27C /* 802BCC0C */
  # region @ 8000D9A0 (24 bytes)
  .data     0x8000D9A0  # address
  .data     0x00000018  # size
  .data     0xC042FC88  # 8000D9A0 => lfs       f2, [r2 - 0x0378]
  .data     0x807E0030  # 8000D9A4 => lwz       r3, [r30 + 0x0030]
  .data     0x70630020  # 8000D9A8 => andi.     r3, r3, 0x0020
  .data     0x41820008  # 8000D9AC => beq       +0x00000008 /* 8000D9B4 */
  .data     0xC042FCA0  # 8000D9B0 => lfs       f2, [r2 - 0x0360]
  .data     0x48329004  # 8000D9B4 => b         +0x00329004 /* 803369B8 */
  # region @ 8000E1E0 (28 bytes)
  .data     0x8000E1E0  # address
  .data     0x0000001C  # size
  .data     0x7FC802A6  # 8000E1E0 => mflr      r30
  .data     0x38A00000  # 8000E1E4 => li        r5, 0x0000
  .data     0x38C0001E  # 8000E1E8 => li        r6, 0x001E
  .data     0x38E00040  # 8000E1EC => li        r7, 0x0040
  .data     0x4807869D  # 8000E1F0 => bl        +0x0007869C /* 8008688C */
  .data     0x7FC803A6  # 8000E1F4 => mtlr      r30
  .data     0x4E800020  # 8000E1F8 => blr
  # region @ 800130C4 (4 bytes)
  .data     0x800130C4  # address
  .data     0x00000004  # size
  .data     0x4BFFFCC0  # 800130C4 => b         -0x00000340 /* 80012D84 */
  # region @ 80014334 (4 bytes)
  .data     0x80014334  # address
  .data     0x00000004  # size
  .data     0x4BFF858D  # 80014334 => bl        -0x00007A74 /* 8000C8C0 */
  # region @ 80015D5C (4 bytes)
  .data     0x80015D5C  # address
  .data     0x00000004  # size
  .data     0x4BFF6B69  # 80015D5C => bl        -0x00009498 /* 8000C8C4 */
  # region @ 80091914 (8 bytes)
  .data     0x80091914  # address
  .data     0x00000008  # size
  .data     0x4800024D  # 80091914 => bl        +0x0000024C /* 80091B60 */
  .data     0xB3C3032C  # 80091918 => sth       [r3 + 0x032C], r30
  # region @ 800BCB58 (4 bytes)
  .data     0x800BCB58  # address
  .data     0x00000004  # size
  .data     0x48000010  # 800BCB58 => b         +0x00000010 /* 800BCB68 */
  # region @ 80104EA4 (4 bytes)
  .data     0x80104EA4  # address
  .data     0x00000004  # size
  .data     0x4182000C  # 80104EA4 => beq       +0x0000000C /* 80104EB0 */
  # region @ 801077D4 (4 bytes)
  .data     0x801077D4  # address
  .data     0x00000004  # size
  .data     0x4800000C  # 801077D4 => b         +0x0000000C /* 801077E0 */
  # region @ 801077E8 (4 bytes)
  .data     0x801077E8  # address
  .data     0x00000004  # size
  .data     0x7C030378  # 801077E8 => mr        r3, r0
  # region @ 8010BCF0 (4 bytes)
  .data     0x8010BCF0  # address
  .data     0x00000004  # size
  .data     0x4BEFF3AC  # 8010BCF0 => b         -0x00100C54 /* 8000B09C */
  # region @ 8010E118 (4 bytes)
  .data     0x8010E118  # address
  .data     0x00000004  # size
  .data     0x4BEFCF9C  # 8010E118 => b         -0x00103064 /* 8000B0B4 */
  # region @ 801129E4 (4 bytes)
  .data     0x801129E4  # address
  .data     0x00000004  # size
  .data     0x4BEF9EBC  # 801129E4 => b         -0x00106144 /* 8000C8A0 */
  # region @ 8011470C (4 bytes)
  .data     0x8011470C  # address
  .data     0x00000004  # size
  .data     0x38000012  # 8011470C => li        r0, 0x0012
  # region @ 8011894C (4 bytes)
  .data     0x8011894C  # address
  .data     0x00000004  # size
  .data     0x88040016  # 8011894C => lbz       r0, [r4 + 0x0016]
  # region @ 80118958 (4 bytes)
  .data     0x80118958  # address
  .data     0x00000004  # size
  .data     0x88040017  # 80118958 => lbz       r0, [r4 + 0x0017]
  # region @ 8011907C (4 bytes)
  .data     0x8011907C  # address
  .data     0x00000004  # size
  .data     0x4BEF35C4  # 8011907C => b         -0x0010CA3C /* 8000C640 */
  # region @ 8011CE54 (12 bytes)
  .data     0x8011CE54  # address
  .data     0x0000000C  # size
  .data     0x7C030378  # 8011CE54 => mr        r3, r0
  .data     0x3863FFFF  # 8011CE58 => subi      r3, r3, 0x0001
  .data     0x4BFFFFE8  # 8011CE5C => b         -0x00000018 /* 8011CE44 */
  # region @ 8011CF10 (12 bytes)
  .data     0x8011CF10  # address
  .data     0x0000000C  # size
  .data     0x7C030378  # 8011CF10 => mr        r3, r0
  .data     0x3863FFFF  # 8011CF14 => subi      r3, r3, 0x0001
  .data     0x4BFFFFE8  # 8011CF18 => b         -0x00000018 /* 8011CF00 */
  # region @ 8011CF60 (12 bytes)
  .data     0x8011CF60  # address
  .data     0x0000000C  # size
  .data     0x7C040378  # 8011CF60 => mr        r4, r0
  .data     0x3884FFFF  # 8011CF64 => subi      r4, r4, 0x0001
  .data     0x4BFFFFE8  # 8011CF68 => b         -0x00000018 /* 8011CF50 */
  # region @ 80166CC4 (8 bytes)
  .data     0x80166CC4  # address
  .data     0x00000008  # size
  .data     0x3C604005  # 80166CC4 => lis       r3, 0x4005
  .data     0x4800009C  # 80166CC8 => b         +0x0000009C /* 80166D64 */
  # region @ 80166D60 (4 bytes)
  .data     0x80166D60  # address
  .data     0x00000004  # size
  .data     0x4800001C  # 80166D60 => b         +0x0000001C /* 80166D7C */
  # region @ 801715F4 (4 bytes)
  .data     0x801715F4  # address
  .data     0x00000004  # size
  .data     0x4BE9A5DC  # 801715F4 => b         -0x00165A24 /* 8000BBD0 */
  # region @ 80171614 (4 bytes)
  .data     0x80171614  # address
  .data     0x00000004  # size
  .data     0x60800420  # 80171614 => ori       r0, r4, 0x0420
  # region @ 80184848 (4 bytes)
  .data     0x80184848  # address
  .data     0x00000004  # size
  .data     0x4BE86D80  # 80184848 => b         -0x00179280 /* 8000B5C8 */
  # region @ 80184888 (4 bytes)
  .data     0x80184888  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80184888 => nop
  # region @ 8018A418 (4 bytes)
  .data     0x8018A418  # address
  .data     0x00000004  # size
  .data     0x60000000  # 8018A418 => nop
  # region @ 80193D9C (4 bytes)
  .data     0x80193D9C  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80193D9C => nop
  # region @ 801BA20C (4 bytes)
  .data     0x801BA20C  # address
  .data     0x00000004  # size
  .data     0x4BE521FC  # 801BA20C => b         -0x001ADE04 /* 8000C408 */
  # region @ 801BA4E0 (4 bytes)
  .data     0x801BA4E0  # address
  .data     0x00000004  # size
  .data     0x4BE50BA8  # 801BA4E0 => b         -0x001AF458 /* 8000B088 */
  # region @ 801C694C (4 bytes)
  .data     0x801C694C  # address
  .data     0x00000004  # size
  .data     0x389F02FC  # 801C694C => addi      r4, r31, 0x02FC
  # region @ 801CACCC (4 bytes)
  .data     0x801CACCC  # address
  .data     0x00000004  # size
  .data     0x48000010  # 801CACCC => b         +0x00000010 /* 801CACDC */
  # region @ 8021E268 (4 bytes)
  .data     0x8021E268  # address
  .data     0x00000004  # size
  .data     0x4BDEE468  # 8021E268 => b         -0x00211B98 /* 8000C6D0 */
  # region @ 80221728 (4 bytes)
  .data     0x80221728  # address
  .data     0x00000004  # size
  .data     0x4BDEAFB8  # 80221728 => b         -0x00215048 /* 8000C6E0 */
  # region @ 8022A55C (4 bytes)
  .data     0x8022A55C  # address
  .data     0x00000004  # size
  .data     0x2C000001  # 8022A55C => cmpwi     r0, 1
  # region @ 8022AD5C (4 bytes)
  .data     0x8022AD5C  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8022AD5C => li        r4, 0xFFFFFF00
  # region @ 8022AD8C (4 bytes)
  .data     0x8022AD8C  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 8022AD8C => li        r4, 0xFFFFFE80
  # region @ 8022ADBC (4 bytes)
  .data     0x8022ADBC  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 8022ADBC => li        r4, 0xFFFFFDB0
  # region @ 8022DA58 (4 bytes)
  .data     0x8022DA58  # address
  .data     0x00000004  # size
  .data     0x60000000  # 8022DA58 => nop
  # region @ 8022E18C (4 bytes)
  .data     0x8022E18C  # address
  .data     0x00000004  # size
  .data     0x41810630  # 8022E18C => bgt       +0x00000630 /* 8022E7BC */
  # region @ 8022F4B0 (4 bytes)
  .data     0x8022F4B0  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8022F4B0 => li        r4, 0xFFFFFF00
  # region @ 8022F4E0 (4 bytes)
  .data     0x8022F4E0  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 8022F4E0 => li        r4, 0xFFFFFE80
  # region @ 8022F510 (4 bytes)
  .data     0x8022F510  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 8022F510 => li        r4, 0xFFFFFDB0
  # region @ 8022FCBC (4 bytes)
  .data     0x8022FCBC  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8022FCBC => li        r4, 0xFFFFFF00
  # region @ 8022FCEC (4 bytes)
  .data     0x8022FCEC  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 8022FCEC => li        r4, 0xFFFFFE80
  # region @ 8022FD1C (4 bytes)
  .data     0x8022FD1C  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 8022FD1C => li        r4, 0xFFFFFDB0
  # region @ 802312C0 (4 bytes)
  .data     0x802312C0  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 802312C0 => li        r4, 0xFFFFFF00
  # region @ 802312F0 (4 bytes)
  .data     0x802312F0  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 802312F0 => li        r4, 0xFFFFFE80
  # region @ 80231320 (4 bytes)
  .data     0x80231320  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80231320 => li        r4, 0xFFFFFDB0
  # region @ 80232030 (4 bytes)
  .data     0x80232030  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80232030 => li        r4, 0xFFFFFF00
  # region @ 80232060 (4 bytes)
  .data     0x80232060  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80232060 => li        r4, 0xFFFFFE80
  # region @ 80232090 (4 bytes)
  .data     0x80232090  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80232090 => li        r4, 0xFFFFFDB0
  # region @ 80232924 (4 bytes)
  .data     0x80232924  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80232924 => li        r4, 0xFFFFFF00
  # region @ 8023295C (4 bytes)
  .data     0x8023295C  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 8023295C => li        r4, 0xFFFFFE80
  # region @ 80232994 (4 bytes)
  .data     0x80232994  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80232994 => li        r4, 0xFFFFFDB0
  # region @ 802349D0 (4 bytes)
  .data     0x802349D0  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 802349D0 => li        r4, 0xFFFFFF00
  # region @ 80234A00 (4 bytes)
  .data     0x80234A00  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80234A00 => li        r4, 0xFFFFFE80
  # region @ 80234A30 (4 bytes)
  .data     0x80234A30  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80234A30 => li        r4, 0xFFFFFDB0
  # region @ 80236FFC (4 bytes)
  .data     0x80236FFC  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80236FFC => li        r4, 0xFFFFFF00
  # region @ 80237038 (4 bytes)
  .data     0x80237038  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80237038 => li        r4, 0xFFFFFE80
  # region @ 80237074 (4 bytes)
  .data     0x80237074  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80237074 => li        r4, 0xFFFFFDB0
  # region @ 802377D4 (4 bytes)
  .data     0x802377D4  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 802377D4 => li        r4, 0xFFFFFF00
  # region @ 80237804 (4 bytes)
  .data     0x80237804  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80237804 => li        r4, 0xFFFFFE80
  # region @ 80237834 (4 bytes)
  .data     0x80237834  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80237834 => li        r4, 0xFFFFFDB0
  # region @ 802381E8 (4 bytes)
  .data     0x802381E8  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 802381E8 => li        r4, 0xFFFFFF00
  # region @ 80238218 (4 bytes)
  .data     0x80238218  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80238218 => li        r4, 0xFFFFFE80
  # region @ 80238248 (4 bytes)
  .data     0x80238248  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80238248 => li        r4, 0xFFFFFDB0
  # region @ 80238BC0 (4 bytes)
  .data     0x80238BC0  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80238BC0 => li        r4, 0xFFFFFF00
  # region @ 80238BF0 (4 bytes)
  .data     0x80238BF0  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80238BF0 => li        r4, 0xFFFFFE80
  # region @ 80238C20 (4 bytes)
  .data     0x80238C20  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80238C20 => li        r4, 0xFFFFFDB0
  # region @ 8023C4F0 (4 bytes)
  .data     0x8023C4F0  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8023C4F0 => li        r4, 0xFFFFFF00
  # region @ 8023C520 (4 bytes)
  .data     0x8023C520  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 8023C520 => li        r4, 0xFFFFFE80
  # region @ 8023C550 (4 bytes)
  .data     0x8023C550  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 8023C550 => li        r4, 0xFFFFFDB0
  # region @ 802514B0 (4 bytes)
  .data     0x802514B0  # address
  .data     0x00000004  # size
  .data     0x60000000  # 802514B0 => nop
  # region @ 802693A4 (4 bytes)
  .data     0x802693A4  # address
  .data     0x00000004  # size
  .data     0x60000000  # 802693A4 => nop
  # region @ 8026EF44 (4 bytes)
  .data     0x8026EF44  # address
  .data     0x00000004  # size
  .data     0x3884AAFA  # 8026EF44 => subi      r4, r4, 0x5506
  # region @ 8026F058 (4 bytes)
  .data     0x8026F058  # address
  .data     0x00000004  # size
  .data     0x3863AAFA  # 8026F058 => subi      r3, r3, 0x5506
  # region @ 8026F0E0 (4 bytes)
  .data     0x8026F0E0  # address
  .data     0x00000004  # size
  .data     0x3883AAFA  # 8026F0E0 => subi      r4, r3, 0x5506
  # region @ 802BCC08 (4 bytes)
  .data     0x802BCC08  # address
  .data     0x00000004  # size
  .data     0x4BD50D78  # 802BCC08 => b         -0x002AF288 /* 8000D980 */
  # region @ 802FD100 (4 bytes)
  .data     0x802FD100  # address
  .data     0x00000004  # size
  .data     0x2C030001  # 802FD100 => cmpwi     r3, 1
  # region @ 80302D64 (28 bytes)
  .data     0x80302D64  # address
  .data     0x0000001C  # size
  .data     0x48000020  # 80302D64 => b         +0x00000020 /* 80302D84 */
  .data     0x3863A830  # 80302D68 => subi      r3, r3, 0x57D0
  .data     0x800DBA04  # 80302D6C => lwz       r0, [r13 - 0x45FC]
  .data     0x2C000023  # 80302D70 => cmpwi     r0, 35
  .data     0x40820008  # 80302D74 => bne       +0x00000008 /* 80302D7C */
  .data     0x3863FB28  # 80302D78 => subi      r3, r3, 0x04D8
  .data     0x4800008C  # 80302D7C => b         +0x0000008C /* 80302E08 */
  # region @ 80302E04 (4 bytes)
  .data     0x80302E04  # address
  .data     0x00000004  # size
  .data     0x4BFFFF64  # 80302E04 => b         -0x0000009C /* 80302D68 */
  # region @ 803369B4 (4 bytes)
  .data     0x803369B4  # address
  .data     0x00000004  # size
  .data     0x4BCD6FEC  # 803369B4 => b         -0x00329014 /* 8000D9A0 */
  # region @ 80357834 (4 bytes)
  .data     0x80357834  # address
  .data     0x00000004  # size
  .data     0x388001E8  # 80357834 => li        r4, 0x01E8
  # region @ 80357858 (4 bytes)
  .data     0x80357858  # address
  .data     0x00000004  # size
  .data     0x4BCB6989  # 80357858 => bl        -0x00349678 /* 8000E1E0 */
  # region @ 803578C8 (4 bytes)
  .data     0x803578C8  # address
  .data     0x00000004  # size
  .data     0x388001E8  # 803578C8 => li        r4, 0x01E8
  # region @ 803578D8 (4 bytes)
  .data     0x803578D8  # address
  .data     0x00000004  # size
  .data     0x4BCB6909  # 803578D8 => bl        -0x003496F8 /* 8000E1E0 */
  # region @ 804B8E10 (8 bytes)
  .data     0x804B8E10  # address
  .data     0x00000008  # size
  .data     0x70808080  # 804B8E10 => andi.     r0, r4, 0x8080
  .data     0x60707070  # 804B8E14 => ori       r16, r3, 0x7070
  # region @ 804CC5D4 (4 bytes)
  .data     0x804CC5D4  # address
  .data     0x00000004  # size
  .data     0x0000001E  # 804CC5D4 => .invalid
  # region @ 804CC62C (4 bytes)
  .data     0x804CC62C  # address
  .data     0x00000004  # size
  .data     0x00000028  # 804CC62C => .invalid
  # region @ 804CC658 (4 bytes)
  .data     0x804CC658  # address
  .data     0x00000004  # size
  .data     0x00000032  # 804CC658 => .invalid
  # region @ 804CC684 (4 bytes)
  .data     0x804CC684  # address
  .data     0x00000004  # size
  .data     0x0000003C  # 804CC684 => .invalid
  # region @ 804CC694 (4 bytes)
  .data     0x804CC694  # address
  .data     0x00000004  # size
  .data     0x0018003C  # 804CC694 => .invalid
  # region @ 804CC8EC (4 bytes)
  .data     0x804CC8EC  # address
  .data     0x00000004  # size
  .data     0x00000028  # 804CC8EC => .invalid
  # region @ 804D1248 (4 bytes)
  .data     0x804D1248  # address
  .data     0x00000004  # size
  .data     0xFF0074EE  # 804D1248 => fsel      f24, f0, f14, f19
  # region @ 805D6CF4 (4 bytes)
  .data     0x805D6CF4  # address
  .data     0x00000004  # size
  .data     0x435C0000  # 805D6CF4 => bc        26, 28, +0x00000000 /* 805D6CF4 */
  # region @ 805D8990 (4 bytes)
  .data     0x805D8990  # address
  .data     0x00000004  # size
  .data     0x46AFC800  # 805D8990 => .invalid  sc
  # region @ 805D8C30 (4 bytes)
  .data     0x805D8C30  # address
  .data     0x00000004  # size
  .data     0x43480000  # 805D8C30 => bc        26, 8, +0x00000000 /* 805D8C30 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
