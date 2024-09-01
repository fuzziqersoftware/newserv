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
  .data     0x481AEB91  # 8000B090 => bl        +0x001AEB90 /* 801B9C20 */
  .data     0x7FA3EB78  # 8000B094 => mr        r3, r29
  .data     0x481AEE60  # 8000B098 => b         +0x001AEE60 /* 801B9EF8 */
  .data     0x881F0000  # 8000B09C => lbz       r0, [r31]
  .data     0x28090001  # 8000B0A0 => cmplwi    r9, 1
  .data     0x4082000C  # 8000B0A4 => bne       +0x0000000C /* 8000B0B0 */
  .data     0x881F0001  # 8000B0A8 => lbz       r0, [r31 + 0x0001]
  .data     0x3BFF0002  # 8000B0AC => addi      r31, r31, 0x0002
  .data     0x48100AC4  # 8000B0B0 => b         +0x00100AC4 /* 8010BB74 */
  .data     0x39200000  # 8000B0B4 => li        r9, 0x0000
  .data     0x48100A55  # 8000B0B8 => bl        +0x00100A54 /* 8010BB0C */
  .data     0x7F43D378  # 8000B0BC => mr        r3, r26
  .data     0x7F64DB78  # 8000B0C0 => mr        r4, r27
  .data     0x7F85E378  # 8000B0C4 => mr        r5, r28
  .data     0x7FA6EB78  # 8000B0C8 => mr        r6, r29
  .data     0x7FC7F378  # 8000B0CC => mr        r7, r30
  .data     0x7FE8FB78  # 8000B0D0 => mr        r8, r31
  .data     0x39200001  # 8000B0D4 => li        r9, 0x0001
  .data     0x48100A35  # 8000B0D8 => bl        +0x00100A34 /* 8010BB0C */
  .data     0x48102EC0  # 8000B0DC => b         +0x00102EC0 /* 8010DF9C */
  # region @ 8000B5C8 (20 bytes)
  .data     0x8000B5C8  # address
  .data     0x00000014  # size
  .data     0x80630098  # 8000B5C8 => lwz       r3, [r3 + 0x0098]
  .data     0x483D70D1  # 8000B5CC => bl        +0x003D70D0 /* 803E269C */
  .data     0x807F042C  # 8000B5D0 => lwz       r3, [r31 + 0x042C]
  .data     0x809F0430  # 8000B5D4 => lwz       r4, [r31 + 0x0430]
  .data     0x48178C88  # 8000B5D8 => b         +0x00178C88 /* 80184260 */
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
  .data     0x48165420  # 8000BBEC => b         +0x00165420 /* 8017100C */
  # region @ 8000C3F8 (124 bytes)
  .data     0x8000C3F8  # address
  .data     0x0000007C  # size
  .data     0x28040000  # 8000C3F8 => cmplwi    r4, 0
  .data     0x4D820020  # 8000C3FC => beqlr
  .data     0x9421FFF0  # 8000C400 => stwu      [r1 - 0x0010], r1
  .data     0x481AD820  # 8000C404 => b         +0x001AD820 /* 801B9C24 */
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
  .data     0x480FECDD  # 8000C43C => bl        +0x000FECDC /* 8010B118 */
  .data     0x7F83E378  # 8000C440 => mr        r3, r28
  .data     0x38800001  # 8000C444 => li        r4, 0x0001
  .data     0x480FEE4D  # 8000C448 => bl        +0x000FEE4C /* 8010B294 */
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
  .data     0x4810C8B0  # 8000C650 => b         +0x0010C8B0 /* 80118F00 */
  # region @ 8000C6D0 (32 bytes)
  .data     0x8000C6D0  # address
  .data     0x00000020  # size
  .data     0x38000001  # 8000C6D0 => li        r0, 0x0001
  .data     0x901D0054  # 8000C6D4 => stw       [r29 + 0x0054], r0
  .data     0x807D0024  # 8000C6D8 => lwz       r3, [r29 + 0x0024]
  .data     0x48211324  # 8000C6DC => b         +0x00211324 /* 8021DA00 */
  .data     0x38000001  # 8000C6E0 => li        r0, 0x0001
  .data     0x901F0378  # 8000C6E4 => stw       [r31 + 0x0378], r0
  .data     0x807F0024  # 8000C6E8 => lwz       r3, [r31 + 0x0024]
  .data     0x482147D4  # 8000C6EC => b         +0x002147D4 /* 80220EC0 */
  # region @ 8000C8A0 (20 bytes)
  .data     0x8000C8A0  # address
  .data     0x00000014  # size
  .data     0x1C00000A  # 8000C8A0 => mulli     r0, r0, 10
  .data     0x57E407BD  # 8000C8A4 => rlwinm.   r4, r31, 0, 30, 30
  .data     0x41820008  # 8000C8A8 => beq       +0x00000008 /* 8000C8B0 */
  .data     0x7FA00734  # 8000C8AC => extsh     r0, r29
  .data     0x48105FB8  # 8000C8B0 => b         +0x00105FB8 /* 80112868 */
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
  .data     0x482AEA54  # 8000D990 => b         +0x002AEA54 /* 802BC3E4 */
  # region @ 8000D9A0 (24 bytes)
  .data     0x8000D9A0  # address
  .data     0x00000018  # size
  .data     0xC042FC80  # 8000D9A0 => lfs       f2, [r2 - 0x0380]
  .data     0x807E0030  # 8000D9A4 => lwz       r3, [r30 + 0x0030]
  .data     0x70630020  # 8000D9A8 => andi.     r3, r3, 0x0020
  .data     0x41820008  # 8000D9AC => beq       +0x00000008 /* 8000D9B4 */
  .data     0xC042FC98  # 8000D9B0 => lfs       f2, [r2 - 0x0368]
  .data     0x4832871C  # 8000D9B4 => b         +0x0032871C /* 803360D0 */
  # region @ 8000E1E0 (28 bytes)
  .data     0x8000E1E0  # address
  .data     0x0000001C  # size
  .data     0x7FC802A6  # 8000E1E0 => mflr      r30
  .data     0x38A00000  # 8000E1E4 => li        r5, 0x0000
  .data     0x38C0001E  # 8000E1E8 => li        r6, 0x001E
  .data     0x38E00040  # 8000E1EC => li        r7, 0x0040
  .data     0x4807859D  # 8000E1F0 => bl        +0x0007859C /* 8008678C */
  .data     0x7FC803A6  # 8000E1F4 => mtlr      r30
  .data     0x4E800020  # 8000E1F8 => blr
  # region @ 8001309C (4 bytes)
  .data     0x8001309C  # address
  .data     0x00000004  # size
  .data     0x4BFFFCC0  # 8001309C => b         -0x00000340 /* 80012D5C */
  # region @ 8001430C (4 bytes)
  .data     0x8001430C  # address
  .data     0x00000004  # size
  .data     0x4BFF85B5  # 8001430C => bl        -0x00007A4C /* 8000C8C0 */
  # region @ 80015D34 (4 bytes)
  .data     0x80015D34  # address
  .data     0x00000004  # size
  .data     0x4BFF6B91  # 80015D34 => bl        -0x00009470 /* 8000C8C4 */
  # region @ 80091814 (8 bytes)
  .data     0x80091814  # address
  .data     0x00000008  # size
  .data     0x4800024D  # 80091814 => bl        +0x0000024C /* 80091A60 */
  .data     0xB3C3032C  # 80091818 => sth       [r3 + 0x032C], r30
  # region @ 800BCA58 (4 bytes)
  .data     0x800BCA58  # address
  .data     0x00000004  # size
  .data     0x48000010  # 800BCA58 => b         +0x00000010 /* 800BCA68 */
  # region @ 80104D24 (4 bytes)
  .data     0x80104D24  # address
  .data     0x00000004  # size
  .data     0x4182000C  # 80104D24 => beq       +0x0000000C /* 80104D30 */
  # region @ 80107654 (4 bytes)
  .data     0x80107654  # address
  .data     0x00000004  # size
  .data     0x4800000C  # 80107654 => b         +0x0000000C /* 80107660 */
  # region @ 80107668 (4 bytes)
  .data     0x80107668  # address
  .data     0x00000004  # size
  .data     0x7C030378  # 80107668 => mr        r3, r0
  # region @ 8010BB70 (4 bytes)
  .data     0x8010BB70  # address
  .data     0x00000004  # size
  .data     0x4BEFF52C  # 8010BB70 => b         -0x00100AD4 /* 8000B09C */
  # region @ 8010DF98 (4 bytes)
  .data     0x8010DF98  # address
  .data     0x00000004  # size
  .data     0x4BEFD11C  # 8010DF98 => b         -0x00102EE4 /* 8000B0B4 */
  # region @ 80112864 (4 bytes)
  .data     0x80112864  # address
  .data     0x00000004  # size
  .data     0x4BEFA03C  # 80112864 => b         -0x00105FC4 /* 8000C8A0 */
  # region @ 8011458C (4 bytes)
  .data     0x8011458C  # address
  .data     0x00000004  # size
  .data     0x38000012  # 8011458C => li        r0, 0x0012
  # region @ 801187CC (4 bytes)
  .data     0x801187CC  # address
  .data     0x00000004  # size
  .data     0x88040016  # 801187CC => lbz       r0, [r4 + 0x0016]
  # region @ 801187D8 (4 bytes)
  .data     0x801187D8  # address
  .data     0x00000004  # size
  .data     0x88040017  # 801187D8 => lbz       r0, [r4 + 0x0017]
  # region @ 80118EFC (4 bytes)
  .data     0x80118EFC  # address
  .data     0x00000004  # size
  .data     0x4BEF3744  # 80118EFC => b         -0x0010C8BC /* 8000C640 */
  # region @ 8011CCD4 (12 bytes)
  .data     0x8011CCD4  # address
  .data     0x0000000C  # size
  .data     0x7C030378  # 8011CCD4 => mr        r3, r0
  .data     0x3863FFFF  # 8011CCD8 => subi      r3, r3, 0x0001
  .data     0x4BFFFFE8  # 8011CCDC => b         -0x00000018 /* 8011CCC4 */
  # region @ 8011CD90 (12 bytes)
  .data     0x8011CD90  # address
  .data     0x0000000C  # size
  .data     0x7C030378  # 8011CD90 => mr        r3, r0
  .data     0x3863FFFF  # 8011CD94 => subi      r3, r3, 0x0001
  .data     0x4BFFFFE8  # 8011CD98 => b         -0x00000018 /* 8011CD80 */
  # region @ 8011CDE0 (12 bytes)
  .data     0x8011CDE0  # address
  .data     0x0000000C  # size
  .data     0x7C040378  # 8011CDE0 => mr        r4, r0
  .data     0x3884FFFF  # 8011CDE4 => subi      r4, r4, 0x0001
  .data     0x4BFFFFE8  # 8011CDE8 => b         -0x00000018 /* 8011CDD0 */
  # region @ 801666D8 (8 bytes)
  .data     0x801666D8  # address
  .data     0x00000008  # size
  .data     0x3C604005  # 801666D8 => lis       r3, 0x4005
  .data     0x4800009C  # 801666DC => b         +0x0000009C /* 80166778 */
  # region @ 80166774 (4 bytes)
  .data     0x80166774  # address
  .data     0x00000004  # size
  .data     0x4800001C  # 80166774 => b         +0x0000001C /* 80166790 */
  # region @ 80171008 (4 bytes)
  .data     0x80171008  # address
  .data     0x00000004  # size
  .data     0x4BE9ABC8  # 80171008 => b         -0x00165438 /* 8000BBD0 */
  # region @ 80171028 (4 bytes)
  .data     0x80171028  # address
  .data     0x00000004  # size
  .data     0x60800420  # 80171028 => ori       r0, r4, 0x0420
  # region @ 8018425C (4 bytes)
  .data     0x8018425C  # address
  .data     0x00000004  # size
  .data     0x4BE8736C  # 8018425C => b         -0x00178C94 /* 8000B5C8 */
  # region @ 8018429C (4 bytes)
  .data     0x8018429C  # address
  .data     0x00000004  # size
  .data     0x60000000  # 8018429C => nop
  # region @ 80189E2C (4 bytes)
  .data     0x80189E2C  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80189E2C => nop
  # region @ 801937B0 (4 bytes)
  .data     0x801937B0  # address
  .data     0x00000004  # size
  .data     0x60000000  # 801937B0 => nop
  # region @ 801B9C20 (4 bytes)
  .data     0x801B9C20  # address
  .data     0x00000004  # size
  .data     0x4BE527E8  # 801B9C20 => b         -0x001AD818 /* 8000C408 */
  # region @ 801B9EF4 (4 bytes)
  .data     0x801B9EF4  # address
  .data     0x00000004  # size
  .data     0x4BE51194  # 801B9EF4 => b         -0x001AEE6C /* 8000B088 */
  # region @ 801C6360 (4 bytes)
  .data     0x801C6360  # address
  .data     0x00000004  # size
  .data     0x389F02FC  # 801C6360 => addi      r4, r31, 0x02FC
  # region @ 801CA6E0 (4 bytes)
  .data     0x801CA6E0  # address
  .data     0x00000004  # size
  .data     0x48000010  # 801CA6E0 => b         +0x00000010 /* 801CA6F0 */
  # region @ 8021D9FC (4 bytes)
  .data     0x8021D9FC  # address
  .data     0x00000004  # size
  .data     0x4BDEECD4  # 8021D9FC => b         -0x0021132C /* 8000C6D0 */
  # region @ 80220EBC (4 bytes)
  .data     0x80220EBC  # address
  .data     0x00000004  # size
  .data     0x4BDEB824  # 80220EBC => b         -0x002147DC /* 8000C6E0 */
  # region @ 80229CF0 (4 bytes)
  .data     0x80229CF0  # address
  .data     0x00000004  # size
  .data     0x2C000001  # 80229CF0 => cmpwi     r0, 1
  # region @ 8022A4F0 (4 bytes)
  .data     0x8022A4F0  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8022A4F0 => li        r4, 0xFFFFFF00
  # region @ 8022A520 (4 bytes)
  .data     0x8022A520  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 8022A520 => li        r4, 0xFFFFFE80
  # region @ 8022A550 (4 bytes)
  .data     0x8022A550  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 8022A550 => li        r4, 0xFFFFFDB0
  # region @ 8022D1EC (4 bytes)
  .data     0x8022D1EC  # address
  .data     0x00000004  # size
  .data     0x60000000  # 8022D1EC => nop
  # region @ 8022D920 (4 bytes)
  .data     0x8022D920  # address
  .data     0x00000004  # size
  .data     0x41810630  # 8022D920 => bgt       +0x00000630 /* 8022DF50 */
  # region @ 8022EC44 (4 bytes)
  .data     0x8022EC44  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8022EC44 => li        r4, 0xFFFFFF00
  # region @ 8022EC74 (4 bytes)
  .data     0x8022EC74  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 8022EC74 => li        r4, 0xFFFFFE80
  # region @ 8022ECA4 (4 bytes)
  .data     0x8022ECA4  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 8022ECA4 => li        r4, 0xFFFFFDB0
  # region @ 8022F450 (4 bytes)
  .data     0x8022F450  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8022F450 => li        r4, 0xFFFFFF00
  # region @ 8022F480 (4 bytes)
  .data     0x8022F480  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 8022F480 => li        r4, 0xFFFFFE80
  # region @ 8022F4B0 (4 bytes)
  .data     0x8022F4B0  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 8022F4B0 => li        r4, 0xFFFFFDB0
  # region @ 80230A54 (4 bytes)
  .data     0x80230A54  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80230A54 => li        r4, 0xFFFFFF00
  # region @ 80230A84 (4 bytes)
  .data     0x80230A84  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80230A84 => li        r4, 0xFFFFFE80
  # region @ 80230AB4 (4 bytes)
  .data     0x80230AB4  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80230AB4 => li        r4, 0xFFFFFDB0
  # region @ 802317C4 (4 bytes)
  .data     0x802317C4  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 802317C4 => li        r4, 0xFFFFFF00
  # region @ 802317F4 (4 bytes)
  .data     0x802317F4  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 802317F4 => li        r4, 0xFFFFFE80
  # region @ 80231824 (4 bytes)
  .data     0x80231824  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80231824 => li        r4, 0xFFFFFDB0
  # region @ 802320B8 (4 bytes)
  .data     0x802320B8  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 802320B8 => li        r4, 0xFFFFFF00
  # region @ 802320F0 (4 bytes)
  .data     0x802320F0  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 802320F0 => li        r4, 0xFFFFFE80
  # region @ 80232128 (4 bytes)
  .data     0x80232128  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80232128 => li        r4, 0xFFFFFDB0
  # region @ 80234164 (4 bytes)
  .data     0x80234164  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80234164 => li        r4, 0xFFFFFF00
  # region @ 80234194 (4 bytes)
  .data     0x80234194  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80234194 => li        r4, 0xFFFFFE80
  # region @ 802341C4 (4 bytes)
  .data     0x802341C4  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 802341C4 => li        r4, 0xFFFFFDB0
  # region @ 80236790 (4 bytes)
  .data     0x80236790  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80236790 => li        r4, 0xFFFFFF00
  # region @ 802367CC (4 bytes)
  .data     0x802367CC  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 802367CC => li        r4, 0xFFFFFE80
  # region @ 80236808 (4 bytes)
  .data     0x80236808  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80236808 => li        r4, 0xFFFFFDB0
  # region @ 80236F68 (4 bytes)
  .data     0x80236F68  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80236F68 => li        r4, 0xFFFFFF00
  # region @ 80236F98 (4 bytes)
  .data     0x80236F98  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80236F98 => li        r4, 0xFFFFFE80
  # region @ 80236FC8 (4 bytes)
  .data     0x80236FC8  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 80236FC8 => li        r4, 0xFFFFFDB0
  # region @ 8023797C (4 bytes)
  .data     0x8023797C  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8023797C => li        r4, 0xFFFFFF00
  # region @ 802379AC (4 bytes)
  .data     0x802379AC  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 802379AC => li        r4, 0xFFFFFE80
  # region @ 802379DC (4 bytes)
  .data     0x802379DC  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 802379DC => li        r4, 0xFFFFFDB0
  # region @ 80238354 (4 bytes)
  .data     0x80238354  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 80238354 => li        r4, 0xFFFFFF00
  # region @ 80238384 (4 bytes)
  .data     0x80238384  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 80238384 => li        r4, 0xFFFFFE80
  # region @ 802383B4 (4 bytes)
  .data     0x802383B4  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 802383B4 => li        r4, 0xFFFFFDB0
  # region @ 8023BC84 (4 bytes)
  .data     0x8023BC84  # address
  .data     0x00000004  # size
  .data     0x3880FF00  # 8023BC84 => li        r4, 0xFFFFFF00
  # region @ 8023BCB4 (4 bytes)
  .data     0x8023BCB4  # address
  .data     0x00000004  # size
  .data     0x3880FE80  # 8023BCB4 => li        r4, 0xFFFFFE80
  # region @ 8023BCE4 (4 bytes)
  .data     0x8023BCE4  # address
  .data     0x00000004  # size
  .data     0x3880FDB0  # 8023BCE4 => li        r4, 0xFFFFFDB0
  # region @ 80250CB0 (4 bytes)
  .data     0x80250CB0  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80250CB0 => nop
  # region @ 80268A88 (4 bytes)
  .data     0x80268A88  # address
  .data     0x00000004  # size
  .data     0x60000000  # 80268A88 => nop
  # region @ 8026E738 (4 bytes)
  .data     0x8026E738  # address
  .data     0x00000004  # size
  .data     0x3884AAFA  # 8026E738 => subi      r4, r4, 0x5506
  # region @ 8026E84C (4 bytes)
  .data     0x8026E84C  # address
  .data     0x00000004  # size
  .data     0x3863AAFA  # 8026E84C => subi      r3, r3, 0x5506
  # region @ 8026E8D4 (4 bytes)
  .data     0x8026E8D4  # address
  .data     0x00000004  # size
  .data     0x3883AAFA  # 8026E8D4 => subi      r4, r3, 0x5506
  # region @ 802BC3E0 (4 bytes)
  .data     0x802BC3E0  # address
  .data     0x00000004  # size
  .data     0x4BD515A0  # 802BC3E0 => b         -0x002AEA60 /* 8000D980 */
  # region @ 802FC968 (4 bytes)
  .data     0x802FC968  # address
  .data     0x00000004  # size
  .data     0x2C030001  # 802FC968 => cmpwi     r3, 1
  # region @ 803025CC (28 bytes)
  .data     0x803025CC  # address
  .data     0x0000001C  # size
  .data     0x48000020  # 803025CC => b         +0x00000020 /* 803025EC */
  .data     0x3863A830  # 803025D0 => subi      r3, r3, 0x57D0
  .data     0x800DB994  # 803025D4 => lwz       r0, [r13 - 0x466C]
  .data     0x2C000023  # 803025D8 => cmpwi     r0, 35
  .data     0x40820008  # 803025DC => bne       +0x00000008 /* 803025E4 */
  .data     0x3863FB28  # 803025E0 => subi      r3, r3, 0x04D8
  .data     0x4800008C  # 803025E4 => b         +0x0000008C /* 80302670 */
  # region @ 8030266C (4 bytes)
  .data     0x8030266C  # address
  .data     0x00000004  # size
  .data     0x4BFFFF64  # 8030266C => b         -0x0000009C /* 803025D0 */
  # region @ 803360CC (4 bytes)
  .data     0x803360CC  # address
  .data     0x00000004  # size
  .data     0x4BCD78D4  # 803360CC => b         -0x0032872C /* 8000D9A0 */
  # region @ 80356D64 (4 bytes)
  .data     0x80356D64  # address
  .data     0x00000004  # size
  .data     0x388001E8  # 80356D64 => li        r4, 0x01E8
  # region @ 80356D88 (4 bytes)
  .data     0x80356D88  # address
  .data     0x00000004  # size
  .data     0x4BCB7459  # 80356D88 => bl        -0x00348BA8 /* 8000E1E0 */
  # region @ 80356DF8 (4 bytes)
  .data     0x80356DF8  # address
  .data     0x00000004  # size
  .data     0x388001E8  # 80356DF8 => li        r4, 0x01E8
  # region @ 80356E08 (4 bytes)
  .data     0x80356E08  # address
  .data     0x00000004  # size
  .data     0x4BCB73D9  # 80356E08 => bl        -0x00348C28 /* 8000E1E0 */
  # region @ 804B6E58 (8 bytes)
  .data     0x804B6E58  # address
  .data     0x00000008  # size
  .data     0x70808080  # 804B6E58 => andi.     r0, r4, 0x8080
  .data     0x60707070  # 804B6E5C => ori       r16, r3, 0x7070
  # region @ 804CA61C (4 bytes)
  .data     0x804CA61C  # address
  .data     0x00000004  # size
  .data     0x0000001E  # 804CA61C => .invalid
  # region @ 804CA674 (4 bytes)
  .data     0x804CA674  # address
  .data     0x00000004  # size
  .data     0x00000028  # 804CA674 => .invalid
  # region @ 804CA6A0 (4 bytes)
  .data     0x804CA6A0  # address
  .data     0x00000004  # size
  .data     0x00000032  # 804CA6A0 => .invalid
  # region @ 804CA6CC (4 bytes)
  .data     0x804CA6CC  # address
  .data     0x00000004  # size
  .data     0x0000003C  # 804CA6CC => .invalid
  # region @ 804CA6DC (4 bytes)
  .data     0x804CA6DC  # address
  .data     0x00000004  # size
  .data     0x0018003C  # 804CA6DC => .invalid
  # region @ 804CA934 (4 bytes)
  .data     0x804CA934  # address
  .data     0x00000004  # size
  .data     0x00000028  # 804CA934 => .invalid
  # region @ 804CF290 (4 bytes)
  .data     0x804CF290  # address
  .data     0x00000004  # size
  .data     0xFF0074EE  # 804CF290 => fsel      f24, f0, f14, f19
  # region @ 805D3F6C (4 bytes)
  .data     0x805D3F6C  # address
  .data     0x00000004  # size
  .data     0x435C0000  # 805D3F6C => bc        26, 28, +0x00000000 /* 805D3F6C */
  # region @ 805D5C08 (4 bytes)
  .data     0x805D5C08  # address
  .data     0x00000004  # size
  .data     0x46AFC800  # 805D5C08 => .invalid  sc
  # region @ 805D5EA8 (4 bytes)
  .data     0x805D5EA8  # address
  .data     0x00000004  # size
  .data     0x43480000  # 805D5EA8 => bc        26, 8, +0x00000000 /* 805D5EA8 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
