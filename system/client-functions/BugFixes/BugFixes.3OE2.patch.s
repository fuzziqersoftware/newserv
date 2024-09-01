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
  .data     0x481AECC1  # 8000B090 => bl        +0x001AECC0 /* 801B9D50 */
  .data     0x7FA3EB78  # 8000B094 => mr        r3, r29
  .data     0x481AEF90  # 8000B098 => b         +0x001AEF90 /* 801BA028 */
  .data     0x881F0000  # 8000B09C => lbz       r0, [r31]
  .data     0x28090001  # 8000B0A0 => cmplwi    r9, 1
  .data     0x4082000C  # 8000B0A4 => bne       +0x0000000C /* 8000B0B0 */
  .data     0x881F0001  # 8000B0A8 => lbz       r0, [r31 + 0x0001]
  .data     0x3BFF0002  # 8000B0AC => addi      r31, r31, 0x0002
  .data     0x48100A54  # 8000B0B0 => b         +0x00100A54 /* 8010BB04 */
  .data     0x39200000  # 8000B0B4 => li        r9, 0x0000
  .data     0x481009E5  # 8000B0B8 => bl        +0x001009E4 /* 8010BA9C */
  .data     0x7F43D378  # 8000B0BC => mr        r3, r26
  .data     0x7F64DB78  # 8000B0C0 => mr        r4, r27
  .data     0x7F85E378  # 8000B0C4 => mr        r5, r28
  .data     0x7FA6EB78  # 8000B0C8 => mr        r6, r29
  .data     0x7FC7F378  # 8000B0CC => mr        r7, r30
  .data     0x7FE8FB78  # 8000B0D0 => mr        r8, r31
  .data     0x39200001  # 8000B0D4 => li        r9, 0x0001
  .data     0x481009C5  # 8000B0D8 => bl        +0x001009C4 /* 8010BA9C */
  .data     0x48102E5C  # 8000B0DC => b         +0x00102E5C /* 8010DF38 */
  # region @ 8000B5C8 (20 bytes)
  .data     0x8000B5C8  # address
  .data     0x00000014  # size
  .data     0x80630098  # 8000B5C8 => lwz       r3, [r3 + 0x0098]
  .data     0x483D90F1  # 8000B5CC => bl        +0x003D90F0 /* 803E46BC */
  .data     0x807F042C  # 8000B5D0 => lwz       r3, [r31 + 0x042C]
  .data     0x809F0430  # 8000B5D4 => lwz       r4, [r31 + 0x0430]
  .data     0x48178DB0  # 8000B5D8 => b         +0x00178DB0 /* 80184388 */
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
  .data     0x48165548  # 8000BBEC => b         +0x00165548 /* 80171134 */
  # region @ 8000C3F8 (124 bytes)
  .data     0x8000C3F8  # address
  .data     0x0000007C  # size
  .data     0x28040000  # 8000C3F8 => cmplwi    r4, 0
  .data     0x4D820020  # 8000C3FC => beqlr
  .data     0x9421FFF0  # 8000C400 => stwu      [r1 - 0x0010], r1
  .data     0x481AD950  # 8000C404 => b         +0x001AD950 /* 801B9D54 */
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
  .data     0x480FEC6D  # 8000C43C => bl        +0x000FEC6C /* 8010B0A8 */
  .data     0x7F83E378  # 8000C440 => mr        r3, r28
  .data     0x38800001  # 8000C444 => li        r4, 0x0001
  .data     0x480FEDDD  # 8000C448 => bl        +0x000FEDDC /* 8010B224 */
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
  .data     0x4810C858  # 8000C650 => b         +0x0010C858 /* 80118EA8 */
  # region @ 8000C6D0 (32 bytes)
  .data     0x8000C6D0  # address
  .data     0x00000020  # size
  .data     0x38000001  # 8000C6D0 => li        r0, 0x0001
  .data     0x901D0054  # 8000C6D4 => stw       [r29 + 0x0054], r0
  .data     0x807D0024  # 8000C6D8 => lwz       r3, [r29 + 0x0024]
  .data     0x482122F8  # 8000C6DC => b         +0x002122F8 /* 8021E9D4 */
  .data     0x38000001  # 8000C6E0 => li        r0, 0x0001
  .data     0x901F0378  # 8000C6E4 => stw       [r31 + 0x0378], r0
  .data     0x807F0024  # 8000C6E8 => lwz       r3, [r31 + 0x0024]
  .data     0x482157A8  # 8000C6EC => b         +0x002157A8 /* 80221E94 */
  # region @ 8000C8A0 (20 bytes)
  .data     0x8000C8A0  # address
  .data     0x00000014  # size
  .data     0x1C00000A  # 8000C8A0 => mulli     r0, r0, 10
  .data     0x57E407BD  # 8000C8A4 => rlwinm.   r4, r31, 0, 30, 30
  .data     0x41820008  # 8000C8A8 => beq       +0x00000008 /* 8000C8B0 */
  .data     0x7FA00734  # 8000C8AC => extsh     r0, r29
  .data     0x48105F54  # 8000C8B0 => b         +0x00105F54 /* 80112804 */
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
  .data     0x482AFAE8  # 8000D990 => b         +0x002AFAE8 /* 802BD478 */
  # region @ 8000D9A0 (24 bytes)
  .data     0x8000D9A0  # address
  .data     0x00000018  # size
  .data     0xC042FC88  # 8000D9A0 => lfs       f2, [r2 - 0x0378]
  .data     0x807E0030  # 8000D9A4 => lwz       r3, [r30 + 0x0030]
  .data     0x70630020  # 8000D9A8 => andi.     r3, r3, 0x0020
  .data     0x41820008  # 8000D9AC => beq       +0x00000008 /* 8000D9B4 */
  .data     0xC042FCA0  # 8000D9B0 => lfs       f2, [r2 - 0x0360]
  .data     0x48329BC0  # 8000D9B4 => b         +0x00329BC0 /* 80337574 */
  # region @ 8000E1E0 (28 bytes)
  .data     0x8000E1E0  # address
  .data     0x0000001C  # size
  .data     0x7FC802A6  # 8000E1E0 => mflr      r30
  .data     0x38A00000  # 8000E1E4 => li        r5, 0x0000
  .data     0x38C0001E  # 8000E1E8 => li        r6, 0x001E
  .data     0x38E00040  # 8000E1EC => li        r7, 0x0040
  .data     0x480786D5  # 8000E1F0 => bl        +0x000786D4 /* 800868C4 */
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
  # region @ 8009194C (8 bytes)
  .data     0x8009194C  # address
  .data     0x00000008  # size
  .data     0x4800024D  # 8009194C => bl        +0x0000024C /* 80091B98 */
  .data     0xB3C3032C  # 80091950 => sth       [r3 + 0x032C], r30
  # region @ 800BCB90 (4 bytes)
  .data     0x800BCB90  # address
  .data     0x00000004  # size
  .data     0x48000010  # 800BCB90 => b         +0x00000010 /* 800BCBA0 */
  # region @ 80104CB4 (4 bytes)
  .data     0x80104CB4  # address
  .data     0x00000004  # size
  .data     0x4182000C  # 80104CB4 => beq       +0x0000000C /* 80104CC0 */
  # region @ 801075E4 (4 bytes)
  .data     0x801075E4  # address
  .data     0x00000004  # size
  .data     0x4800000C  # 801075E4 => b         +0x0000000C /* 801075F0 */
  # region @ 801075F8 (4 bytes)
  .data     0x801075F8  # address
  .data     0x00000004  # size
  .data     0x7C030378  # 801075F8 => mr        r3, r0
  # region @ 8010BB00 (4 bytes)
  .data     0x8010BB00  # address
  .data     0x00000004  # size
  .data     0x4BEFF59C  # 8010BB00 => b         -0x00100A64 /* 8000B09C */
  # region @ 8010DF34 (4 bytes)
  .data     0x8010DF34  # address
  .data     0x00000004  # size
  .data     0x4BEFD180  # 8010DF34 => b         -0x00102E80 /* 8000B0B4 */
  # region @ 80112800 (4 bytes)
  .data     0x80112800  # address
  .data     0x00000004  # size
  .data     0x4BEFA0A0  # 80112800 => b         -0x00105F60 /* 8000C8A0 */
  # region @ 80114534 (4 bytes)
  .data     0x80114534  # address
  .data     0x00000004  # size
  .data     0x38000012  # 80114534 => li        r0, 0x0012
  # region @ 80118774 (4 bytes)
  .data     0x80118774  # address
  .data     0x00000004  # size
  .data     0x88040016  # 80118774 => lbz       r0, [r4 + 0x0016]
  # region @ 80118780 (4 bytes)
  .data     0x80118780  # address
  .data     0x00000004  # size
  .data     0x88040017  # 80118780 => lbz       r0, [r4 + 0x0017]
  # region @ 80118EA4 (4 bytes)
  .data     0x80118EA4  # address
  .data     0x00000004  # size
  .data     0x4BEF379C  # 80118EA4 => b         -0x0010C864 /* 8000C640 */
  # region @ 8011CC7C (12 bytes)
  .data     0x8011CC7C  # address
  .data     0x0000000C  # size
  .data     0x7C030378  # 8011CC7C => mr        r3, r0
  .data     0x3863FFFF  # 8011CC80 => subi      r3, r3, 0x0001
  .data     0x4BFFFFE8  # 8011CC84 => b         -0x00000018 /* 8011CC6C */
  # region @ 8011CD38 (12 bytes)
  .data     0x8011CD38  # address
  .data     0x0000000C  # size
  .data     0x7C030378  # 8011CD38 => mr        r3, r0
  .data     0x3863FFFF  # 8011CD3C => subi      r3, r3, 0x0001
  .data     0x4BFFFFE8  # 8011CD40 => b         -0x00000018 /* 8011CD28 */
  # region @ 8011CD88 (12 bytes)
  .data     0x8011CD88  # address
  .data     0x0000000C  # size
  .data     0x7C040378  # 8011CD88 => mr        r4, r0
  .data     0x3884FFFF  # 8011CD8C => subi      r4, r4, 0x0001
  .data     0x4BFFFFE8  # 8011CD90 => b         -0x00000018 /* 8011CD78 */
  # region @ 80166800 (8 bytes)
  .data     0x80166800  # address
  .data     0x00000008  # size
  .data     0x3C604005  # 80166800 => lis       r3, 0x4005
  .data     0x4800009C  # 80166804 => b         +0x0000009C /* 801668A0 */
  # region @ 8016689C (4 bytes)
  .data     0x8016689C  # address
  .data     0x00000004  # size
  .data     0x4800001C  # 8016689C => b         +0x0000001C /* 801668B8 */
  # region @ 80171130 (4 bytes)
  .data     0x80171130  # address
  .data     0x00000004  # size
  .data     0x4BE9AAA0  # 80171130 => b         -0x00165560 /* 8000BBD0 */
  # region @ 80171150 (4 bytes)
  .data     0x80171150  # address
  .data     0x00000004  # size
  .data     0x60800420  # 80171150 => ori       r0, r4, 0x0420
  # region @ 80184384 (4 bytes)
  .data     0x80184384  # address
  .data     0x00000004  # size
  .data     0x4BE87244  # 80184384 => b         -0x00178DBC /* 8000B5C8 */
  # region @ 801843C4 (4 bytes)
  .data     0x801843C4  # address
  .data     0x00000004  # size
  .data     0x60000000  # 801843C4 => nop
  # region @ 80189F54 (4 bytes)
  .data     0x80189F54  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80189F54 => nop
  # region @ 801938D8 (4 bytes)
  .data     0x801938D8  # address
  .data     0x00000004  # size
  .data     0x60000000  # 801938D8 => nop
  # region @ 801B9D50 (4 bytes)
  .data     0x801B9D50  # address
  .data     0x00000004  # size
  .data     0x4BE526B8  # 801B9D50 => b         -0x001AD948 /* 8000C408 */
  # region @ 801BA024 (4 bytes)
  .data     0x801BA024  # address
  .data     0x00000004  # size
  .data     0x4BE51064  # 801BA024 => b         -0x001AEF9C /* 8000B088 */
  # region @ 801C6490 (4 bytes)
  .data     0x801C6490  # address
  .data     0x00000004  # size
  .data     0x389F02FC  # 801C6490 => addi      r4, r31, 0x02FC
  # region @ 801CA810 (4 bytes)
  .data     0x801CA810  # address
  .data     0x00000004  # size
  .data     0x48000010  # 801CA810 => b         +0x00000010 /* 801CA820 */
  # region @ 8021E9D0 (4 bytes)
  .data     0x8021E9D0  # address
  .data     0x00000004  # size
  .data     0x4BDEDD00  # 8021E9D0 => b         -0x00212300 /* 8000C6D0 */
  # region @ 80221E90 (4 bytes)
  .data     0x80221E90  # address
  .data     0x00000004  # size
  .data     0x4BDEA850  # 80221E90 => b         -0x002157B0 /* 8000C6E0 */
  # region @ 8022ACC4 (4 bytes)
  .data     0x8022ACC4  # address
  .data     0x00000004  # size
  .data     0x2C000001  # 8022ACC4 => cmpwi     r0, 1
  # region @ 8022B4C4 (4 bytes)
  .data     0x8022B4C4  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8022B4C4 => li        r4, 0xFFFFFF00
  # region @ 8022B4F4 (4 bytes)
  .data     0x8022B4F4  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 8022B4F4 => li        r4, 0xFFFFFE80
  # region @ 8022B524 (4 bytes)
  .data     0x8022B524  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 8022B524 => li        r4, 0xFFFFFDB0
  # region @ 8022E1C0 (4 bytes)
  .data     0x8022E1C0  # address
  .data     0x00000004  # size
  .data     0x60000000  # 8022E1C0 => nop
  # region @ 8022E8F4 (4 bytes)
  .data     0x8022E8F4  # address
  .data     0x00000004  # size
  .data     0x41810630  # 8022E8F4 => bgt       +0x00000630 /* 8022EF24 */
  # region @ 8022FC18 (4 bytes)
  .data     0x8022FC18  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8022FC18 => li        r4, 0xFFFFFF00
  # region @ 8022FC48 (4 bytes)
  .data     0x8022FC48  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 8022FC48 => li        r4, 0xFFFFFE80
  # region @ 8022FC78 (4 bytes)
  .data     0x8022FC78  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 8022FC78 => li        r4, 0xFFFFFDB0
  # region @ 80230424 (4 bytes)
  .data     0x80230424  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80230424 => li        r4, 0xFFFFFF00
  # region @ 80230454 (4 bytes)
  .data     0x80230454  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80230454 => li        r4, 0xFFFFFE80
  # region @ 80230484 (4 bytes)
  .data     0x80230484  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80230484 => li        r4, 0xFFFFFDB0
  # region @ 80231A28 (4 bytes)
  .data     0x80231A28  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80231A28 => li        r4, 0xFFFFFF00
  # region @ 80231A58 (4 bytes)
  .data     0x80231A58  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80231A58 => li        r4, 0xFFFFFE80
  # region @ 80231A88 (4 bytes)
  .data     0x80231A88  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80231A88 => li        r4, 0xFFFFFDB0
  # region @ 80232798 (4 bytes)
  .data     0x80232798  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80232798 => li        r4, 0xFFFFFF00
  # region @ 802327C8 (4 bytes)
  .data     0x802327C8  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 802327C8 => li        r4, 0xFFFFFE80
  # region @ 802327F8 (4 bytes)
  .data     0x802327F8  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 802327F8 => li        r4, 0xFFFFFDB0
  # region @ 8023308C (4 bytes)
  .data     0x8023308C  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8023308C => li        r4, 0xFFFFFF00
  # region @ 802330C4 (4 bytes)
  .data     0x802330C4  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 802330C4 => li        r4, 0xFFFFFE80
  # region @ 802330FC (4 bytes)
  .data     0x802330FC  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 802330FC => li        r4, 0xFFFFFDB0
  # region @ 80235138 (4 bytes)
  .data     0x80235138  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80235138 => li        r4, 0xFFFFFF00
  # region @ 80235168 (4 bytes)
  .data     0x80235168  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80235168 => li        r4, 0xFFFFFE80
  # region @ 80235198 (4 bytes)
  .data     0x80235198  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80235198 => li        r4, 0xFFFFFDB0
  # region @ 80237764 (4 bytes)
  .data     0x80237764  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80237764 => li        r4, 0xFFFFFF00
  # region @ 802377A0 (4 bytes)
  .data     0x802377A0  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 802377A0 => li        r4, 0xFFFFFE80
  # region @ 802377DC (4 bytes)
  .data     0x802377DC  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 802377DC => li        r4, 0xFFFFFDB0
  # region @ 80237F3C (4 bytes)
  .data     0x80237F3C  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80237F3C => li        r4, 0xFFFFFF00
  # region @ 80237F6C (4 bytes)
  .data     0x80237F6C  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80237F6C => li        r4, 0xFFFFFE80
  # region @ 80237F9C (4 bytes)
  .data     0x80237F9C  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80237F9C => li        r4, 0xFFFFFDB0
  # region @ 80238950 (4 bytes)
  .data     0x80238950  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80238950 => li        r4, 0xFFFFFF00
  # region @ 80238980 (4 bytes)
  .data     0x80238980  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80238980 => li        r4, 0xFFFFFE80
  # region @ 802389B0 (4 bytes)
  .data     0x802389B0  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 802389B0 => li        r4, 0xFFFFFDB0
  # region @ 80239328 (4 bytes)
  .data     0x80239328  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80239328 => li        r4, 0xFFFFFF00
  # region @ 80239358 (4 bytes)
  .data     0x80239358  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80239358 => li        r4, 0xFFFFFE80
  # region @ 80239388 (4 bytes)
  .data     0x80239388  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80239388 => li        r4, 0xFFFFFDB0
  # region @ 8023CC58 (4 bytes)
  .data     0x8023CC58  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8023CC58 => li        r4, 0xFFFFFF00
  # region @ 8023CC88 (4 bytes)
  .data     0x8023CC88  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 8023CC88 => li        r4, 0xFFFFFE80
  # region @ 8023CCB8 (4 bytes)
  .data     0x8023CCB8  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 8023CCB8 => li        r4, 0xFFFFFDB0
  # region @ 80251C68 (4 bytes)
  .data     0x80251C68  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80251C68 => nop
  # region @ 80269B5C (4 bytes)
  .data     0x80269B5C  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80269B5C => nop
  # region @ 8026F6FC (4 bytes)
  .data     0x8026F6FC  # address
  .data     0x00000004  # size
  .data     0x3884AAFA  # 8026F6FC => subi      r4, r4, 0x5506
  # region @ 8026F810 (4 bytes)
  .data     0x8026F810  # address
  .data     0x00000004  # size
  .data     0x3863AAFA  # 8026F810 => subi      r3, r3, 0x5506
  # region @ 8026F898 (4 bytes)
  .data     0x8026F898  # address
  .data     0x00000004  # size
  .data     0x3883AAFA  # 8026F898 => subi      r4, r3, 0x5506
  # region @ 802BD474 (4 bytes)
  .data     0x802BD474  # address
  .data     0x00000004  # size
  .data     0x4BD5050C  # 802BD474 => b         -0x002AFAF4 /* 8000D980 */
  # region @ 802FDD28 (4 bytes)
  .data     0x802FDD28  # address
  .data     0x00000004  # size
  .data     0x2C030001  # 802FDD28 => cmpwi     r3, 1
  # region @ 8030398C (28 bytes)
  .data     0x8030398C  # address
  .data     0x0000001C  # size
  .data     0x48000020  # 8030398C => b         +0x00000020 /* 803039AC */
  .data     0x3863A830  # 80303990 => subi      r3, r3, 0x57D0
  .data     0x800DB9C4  # 80303994 => lwz       r0, [r13 - 0x463C]
  .data     0x2C000023  # 80303998 => cmpwi     r0, 35
  .data     0x40820008  # 8030399C => bne       +0x00000008 /* 803039A4 */
  .data     0x3863FB28  # 803039A0 => subi      r3, r3, 0x04D8
  .data     0x4800008C  # 803039A4 => b         +0x0000008C /* 80303A30 */
  # region @ 80303A2C (4 bytes)
  .data     0x80303A2C  # address
  .data     0x00000004  # size
  .data     0x4BFFFF64  # 80303A2C => b         -0x0000009C /* 80303990 */
  # region @ 80337570 (4 bytes)
  .data     0x80337570  # address
  .data     0x00000004  # size
  .data     0x4BCD6430  # 80337570 => b         -0x00329BD0 /* 8000D9A0 */
  # region @ 80358440 (4 bytes)
  .data     0x80358440  # address
  .data     0x00000004  # size
  .data     0x388001E8  # 80358440 => li        r4, 0x01E8
  # region @ 80358464 (4 bytes)
  .data     0x80358464  # address
  .data     0x00000004  # size
  .data     0x4BCB5D7D  # 80358464 => bl        -0x0034A284 /* 8000E1E0 */
  # region @ 803584D4 (4 bytes)
  .data     0x803584D4  # address
  .data     0x00000004  # size
  .data     0x388001E8  # 803584D4 => li        r4, 0x01E8
  # region @ 803584E4 (4 bytes)
  .data     0x803584E4  # address
  .data     0x00000004  # size
  .data     0x4BCB5CFD  # 803584E4 => bl        -0x0034A304 /* 8000E1E0 */
  # region @ 804B8990 (8 bytes)
  .data     0x804B8990  # address
  .data     0x00000008  # size
  .data     0x70808080  # 804B8990 => andi.     r0, r4, 0x8080
  .data     0x60707070  # 804B8994 => ori       r16, r3, 0x7070
  # region @ 804CC1E4 (4 bytes)
  .data     0x804CC1E4  # address
  .data     0x00000004  # size
  .data     0x0000001E  # 804CC1E4 => .invalid
  # region @ 804CC23C (4 bytes)
  .data     0x804CC23C  # address
  .data     0x00000004  # size
  .data     0x00000028  # 804CC23C => .invalid
  # region @ 804CC268 (4 bytes)
  .data     0x804CC268  # address
  .data     0x00000004  # size
  .data     0x00000032  # 804CC268 => .invalid
  # region @ 804CC294 (4 bytes)
  .data     0x804CC294  # address
  .data     0x00000004  # size
  .data     0x0000003C  # 804CC294 => .invalid
  # region @ 804CC2A4 (4 bytes)
  .data     0x804CC2A4  # address
  .data     0x00000004  # size
  .data     0x0018003C  # 804CC2A4 => .invalid
  # region @ 804CC4FC (4 bytes)
  .data     0x804CC4FC  # address
  .data     0x00000004  # size
  .data     0x00000028  # 804CC4FC => .invalid
  # region @ 804D0E58 (4 bytes)
  .data     0x804D0E58  # address
  .data     0x00000004  # size
  .data     0xFF0074EE  # 804D0E58 => fsel      f24, f0, f14, f19
  # region @ 805DAAB4 (4 bytes)
  .data     0x805DAAB4  # address
  .data     0x00000004  # size
  .data     0x435C0000  # 805DAAB4 => bc        26, 28, +0x00000000 /* 805DAAB4 */
  # region @ 805DC750 (4 bytes)
  .data     0x805DC750  # address
  .data     0x00000004  # size
  .data     0x46AFC800  # 805DC750 => .invalid  sc
  # region @ 805DC9F0 (4 bytes)
  .data     0x805DC9F0  # address
  .data     0x00000004  # size
  .data     0x43480000  # 805DC9F0 => bc        26, 8, +0x00000000 /* 805DC9F0 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
