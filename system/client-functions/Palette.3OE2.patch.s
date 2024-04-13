.meta name="Palette"
.meta description="Press Z to cycle\nthrough 4 customize\nconfigurations\ninstead of just one"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC
  # region @ 8000B958 (136 bytes)
  .data     0x8000B958  # address
  .data     0x00000088  # size
  .data     0x906DB974  # 8000B958 => stw       [r13 - 0x468C], r3
  .data     0x1C63003C  # 8000B95C => mulli     r3, r3, 60
  .data     0x808DB958  # 8000B960 => lwz       r4, [r13 - 0x46A8]
  .data     0x3C840001  # 8000B964 => addis     r4, r4, 0x0001
  .data     0x38840B80  # 8000B968 => addi      r4, r4, 0x0B80
  .data     0x7C841A14  # 8000B96C => add       r4, r4, r3
  .data     0x3C608000  # 8000B970 => lis       r3, 0x8000
  .data     0x6063CF40  # 8000B974 => ori       r3, r3, 0xCF40
  .data     0x38A0003C  # 8000B978 => li        r5, 0x003C
  .data     0x48002AA1  # 8000B97C => bl        +0x00002AA0 /* 8000E41C */
  .data     0x481F11DC  # 8000B980 => b         +0x001F11DC /* 801FCB5C */
  .data     0x806DB974  # 8000B984 => lwz       r3, [r13 - 0x468C]
  .data     0x1C63003C  # 8000B988 => mulli     r3, r3, 60
  .data     0x808DB958  # 8000B98C => lwz       r4, [r13 - 0x46A8]
  .data     0x3C840001  # 8000B990 => addis     r4, r4, 0x0001
  .data     0x38840B80  # 8000B994 => addi      r4, r4, 0x0B80
  .data     0x7C641A14  # 8000B998 => add       r3, r4, r3
  .data     0x3C808000  # 8000B99C => lis       r4, 0x8000
  .data     0x6084CF40  # 8000B9A0 => ori       r4, r4, 0xCF40
  .data     0x38A0003C  # 8000B9A4 => li        r5, 0x003C
  .data     0x48002A75  # 8000B9A8 => bl        +0x00002A74 /* 8000E41C */
  .data     0x806DB958  # 8000B9AC => lwz       r3, [r13 - 0x46A8]
  .data     0x481F4AE4  # 8000B9B0 => b         +0x001F4AE4 /* 80200494 */
  .data     0x806DB974  # 8000B9B4 => lwz       r3, [r13 - 0x468C]
  .data     0x1C63003C  # 8000B9B8 => mulli     r3, r3, 60
  .data     0x808DB958  # 8000B9BC => lwz       r4, [r13 - 0x46A8]
  .data     0x3C840001  # 8000B9C0 => addis     r4, r4, 0x0001
  .data     0x38840B80  # 8000B9C4 => addi      r4, r4, 0x0B80
  .data     0x7C641A14  # 8000B9C8 => add       r3, r4, r3
  .data     0x38800000  # 8000B9CC => li        r4, 0x0000
  .data     0x38A0003C  # 8000B9D0 => li        r5, 0x003C
  .data     0x48002961  # 8000B9D4 => bl        +0x00002960 /* 8000E334 */
  .data     0x48003F25  # 8000B9D8 => bl        +0x00003F24 /* 8000F8FC */
  .data     0x481F3FA0  # 8000B9DC => b         +0x001F3FA0 /* 801FF97C */
  # region @ 8000CA40 (64 bytes)
  .data     0x8000CA40  # address
  .data     0x00000040  # size
  .data     0x28030000  # 8000CA40 => cmplwi    r3, 0
  .data     0x40820008  # 8000CA44 => bne       +0x00000008 /* 8000CA4C */
  .data     0x3BE00000  # 8000CA48 => li        r31, 0x0000
  .data     0x7C00F800  # 8000CA4C => cmp       r0, r31
  .data     0x481CB9E4  # 8000CA50 => b         +0x001CB9E4 /* 801D8434 */
  .data     0x38000003  # 8000CA54 => li        r0, 0x0003
  .data     0x7C0903A6  # 8000CA58 => mtctr     r0
  .data     0x63C40500  # 8000CA5C => ori       r4, r30, 0x0500
  .data     0x38BF0538  # 8000CA60 => addi      r5, r31, 0x0538
  .data     0xA4050004  # 8000CA64 => lhzu      r0, [r5 + 0x0004]
  .data     0x7C040000  # 8000CA68 => cmp       r4, r0
  .data     0x4182000C  # 8000CA6C => beq       +0x0000000C /* 8000CA78 */
  .data     0x4200FFF4  # 8000CA70 => bdnz      -0x0000000C /* 8000CA64 */
  .data     0x38600000  # 8000CA74 => li        r3, 0x0000
  .data     0x2C030000  # 8000CA78 => cmpwi     r3, 0
  .data     0x481BF7C0  # 8000CA7C => b         +0x001BF7C0 /* 801CC23C */
  # region @ 8000CD00 (240 bytes)
  .data     0x8000CD00  # address
  .data     0x000000F0  # size
  .data     0x3C808000  # 8000CD00 => lis       r4, 0x8000
  .data     0x6084CF3E  # 8000CD04 => ori       r4, r4, 0xCF3E
  .data     0x3BE00000  # 8000CD08 => li        r31, 0x0000
  .data     0xA0C4003A  # 8000CD0C => lhz       r6, [r4 + 0x003A]
  .data     0x2C060000  # 8000CD10 => cmpwi     r6, 0
  .data     0x41820074  # 8000CD14 => beq       +0x00000074 /* 8000CD88 */
  .data     0xB3E4003A  # 8000CD18 => sth       [r4 + 0x003A], r31
  .data     0x3C608051  # 8000CD1C => lis       r3, 0x8051
  .data     0xA003E274  # 8000CD20 => lhz       r0, [r3 - 0x1D8C]
  .data     0xA0A3E270  # 8000CD24 => lhz       r5, [r3 - 0x1D90]
  .data     0x7CA53038  # 8000CD28 => and       r5, r5, r6
  .data     0x70003C00  # 8000CD2C => andi.     r0, r0, 0x3C00
  .data     0x41820058  # 8000CD30 => beq       +0x00000058 /* 8000CD88 */
  .data     0x5403056B  # 8000CD34 => rlwinm.   r3, r0, 0, 21, 21
  .data     0x41820008  # 8000CD38 => beq       +0x00000008 /* 8000CD40 */
  .data     0x3BC0002A  # 8000CD3C => li        r30, 0x002A
  .data     0x540304A5  # 8000CD40 => rlwinm.   r3, r0, 0, 18, 18
  .data     0x41820008  # 8000CD44 => beq       +0x00000008 /* 8000CD4C */
  .data     0x3BC0001C  # 8000CD48 => li        r30, 0x001C
  .data     0x54030529  # 8000CD4C => rlwinm.   r3, r0, 0, 20, 20
  .data     0x41820008  # 8000CD50 => beq       +0x00000008 /* 8000CD58 */
  .data     0x3BC0000E  # 8000CD54 => li        r30, 0x000E
  .data     0x7C84F214  # 8000CD58 => add       r4, r4, r30
  .data     0x38000007  # 8000CD5C => li        r0, 0x0007
  .data     0x7C0903A6  # 8000CD60 => mtctr     r0
  .data     0x387C0504  # 8000CD64 => addi      r3, r28, 0x0504
  .data     0x2C050003  # 8000CD68 => cmpwi     r5, 3
  .data     0x4082000C  # 8000CD6C => bne       +0x0000000C /* 8000CD78 */
  .data     0xA0030004  # 8000CD70 => lhz       r0, [r3 + 0x0004]
  .data     0xB0040002  # 8000CD74 => sth       [r4 + 0x0002], r0
  .data     0xA4040002  # 8000CD78 => lhzu      r0, [r4 + 0x0002]
  .data     0xB4030004  # 8000CD7C => sthu      [r3 + 0x0004], r0
  .data     0x4200FFE8  # 8000CD80 => bdnz      -0x00000018 /* 8000CD68 */
  .data     0x3BC00000  # 8000CD84 => li        r30, 0x0000
  .data     0x481CB0F4  # 8000CD88 => b         +0x001CB0F4 /* 801D7E7C */
  .data     0x38600003  # 8000CD8C => li        r3, 0x0003
  .data     0x3C808001  # 8000CD90 => lis       r4, 0x8001
  .data     0xB064CF78  # 8000CD94 => sth       [r4 - 0x3088], r3
  .data     0x7FC3F378  # 8000CD98 => mr        r3, r30
  .data     0x4826B428  # 8000CD9C => b         +0x0026B428 /* 802781C4 */
  .data     0x3D808045  # 8000CDA0 => lis       r12, 0x8045
  .data     0x618C2440  # 8000CDA4 => ori       r12, r12, 0x2440
  .data     0x80030000  # 8000CDA8 => lwz       r0, [r3]
  .data     0x7C006000  # 8000CDAC => cmp       r0, r12
  .data     0xA0030004  # 8000CDB0 => lhz       r0, [r3 + 0x0004]
  .data     0x40820018  # 8000CDB4 => bne       +0x00000018 /* 8000CDCC */
  .data     0x2C000000  # 8000CDB8 => cmpwi     r0, 0
  .data     0x40820010  # 8000CDBC => bne       +0x00000010 /* 8000CDCC */
  .data     0x38600001  # 8000CDC0 => li        r3, 0x0001
  .data     0x3D808001  # 8000CDC4 => lis       r12, 0x8001
  .data     0xB06CCF78  # 8000CDC8 => sth       [r12 - 0x3088], r3
  .data     0x4824094C  # 8000CDCC => b         +0x0024094C /* 8024D718 */
  .data     0x3C608000  # 8000CDD0 => lis       r3, 0x8000
  .data     0x6063CF3E  # 8000CDD4 => ori       r3, r3, 0xCF3E
  .data     0x3800001C  # 8000CDD8 => li        r0, 0x001C
  .data     0x7C0903A6  # 8000CDDC => mtctr     r0
  .data     0x38000000  # 8000CDE0 => li        r0, 0x0000
  .data     0xB4030002  # 8000CDE4 => sthu      [r3 + 0x0002], r0
  .data     0x4200FFFC  # 8000CDE8 => bdnz      -0x00000004 /* 8000CDE4 */
  .data     0x4832B494  # 8000CDEC => b         +0x0032B494 /* 80338280 */
  # region @ 801B5B7C (4 bytes)
  .data     0x801B5B7C  # address
  .data     0x00000004  # size
  .data     0x38600000  # 801B5B7C => li        r3, 0x0000
  # region @ 801CC238 (4 bytes)
  .data     0x801CC238  # address
  .data     0x00000004  # size
  .data     0x4BE4081C  # 801CC238 => b         -0x001BF7E4 /* 8000CA54 */
  # region @ 801D7E78 (4 bytes)
  .data     0x801D7E78  # address
  .data     0x00000004  # size
  .data     0x4BE34E88  # 801D7E78 => b         -0x001CB178 /* 8000CD00 */
  # region @ 801D8430 (4 bytes)
  .data     0x801D8430  # address
  .data     0x00000004  # size
  .data     0x4BE34610  # 801D8430 => b         -0x001CB9F0 /* 8000CA40 */
  # region @ 801FCB58 (4 bytes)
  .data     0x801FCB58  # address
  .data     0x00000004  # size
  .data     0x4BE0EE00  # 801FCB58 => b         -0x001F1200 /* 8000B958 */
  # region @ 801FF978 (4 bytes)
  .data     0x801FF978  # address
  .data     0x00000004  # size
  .data     0x4BE0C03C  # 801FF978 => b         -0x001F3FC4 /* 8000B9B4 */
  # region @ 80200490 (4 bytes)
  .data     0x80200490  # address
  .data     0x00000004  # size
  .data     0x4BE0B4F4  # 80200490 => b         -0x001F4B0C /* 8000B984 */
  # region @ 80248520 (8 bytes)
  .data     0x80248520  # address
  .data     0x00000008  # size
  .data     0xA01F004A  # 80248520 => lhz       r0, [r31 + 0x004A]
  .data     0x54030637  # 80248524 => rlwinm.   r3, r0, 0, 24, 27
  # region @ 8024D714 (4 bytes)
  .data     0x8024D714  # address
  .data     0x00000004  # size
  .data     0x4BDBF68C  # 8024D714 => b         -0x00240974 /* 8000CDA0 */
  # region @ 80277B74 (4 bytes)
  .data     0x80277B74  # address
  .data     0x00000004  # size
  .data     0x3803BAA0  # 80277B74 => subi      r0, r3, 0x4560
  # region @ 802781C0 (4 bytes)
  .data     0x802781C0  # address
  .data     0x00000004  # size
  .data     0x4BD94BCC  # 802781C0 => b         -0x0026B434 /* 8000CD8C */
  # region @ 80278220 (8 bytes)
  .data     0x80278220  # address
  .data     0x00000008  # size
  .data     0xA01F004A  # 80278220 => lhz       r0, [r31 + 0x004A]
  .data     0x54030637  # 80278224 => rlwinm.   r3, r0, 0, 24, 27
  # region @ 8033827C (4 bytes)
  .data     0x8033827C  # address
  .data     0x00000004  # size
  .data     0x4BCD4B54  # 8033827C => b         -0x0032B4AC /* 8000CDD0 */
  # region @ 8044F8B4 (52 bytes)
  .data     0x8044F8B4  # address
  .data     0x00000034  # size
  .data     0x0004000D  # 8044F8B4 => .invalid
  .data     0x0004000E  # 8044F8B8 => .invalid
  .data     0x00000000  # 8044F8BC => .invalid
  .data     0x0004000F  # 8044F8C0 => .invalid
  .data     0x00040010  # 8044F8C4 => .invalid
  .data     0x00000000  # 8044F8C8 => .invalid
  .data     0x00050000  # 8044F8CC => .invalid
  .data     0x00050001  # 8044F8D0 => .invalid
  .data     0x00050002  # 8044F8D4 => .invalid
  .data     0x00050003  # 8044F8D8 => .invalid
  .data     0x00050004  # 8044F8DC => .invalid
  .data     0x00050005  # 8044F8E0 => .invalid
  .data     0x00080000  # 8044F8E4 => .invalid
  # region @ 80471ACC (4 bytes)
  .data     0x80471ACC  # address
  .data     0x00000004  # size
  .data     0xFFFFFFFF  # 80471ACC => fnmadd.   f31, f31, f31, f31
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
