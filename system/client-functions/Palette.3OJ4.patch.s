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
  .data     0x906DB964  # 8000B958 => stw       [r13 - 0x469C], r3
  .data     0x1C63003C  # 8000B95C => mulli     r3, r3, 60
  .data     0x808DB948  # 8000B960 => lwz       r4, [r13 - 0x46B8]
  .data     0x3C840001  # 8000B964 => addis     r4, r4, 0x0001
  .data     0x38840B80  # 8000B968 => addi      r4, r4, 0x0B80
  .data     0x7C841A14  # 8000B96C => add       r4, r4, r3
  .data     0x3C608000  # 8000B970 => lis       r3, 0x8000
  .data     0x6063CF40  # 8000B974 => ori       r3, r3, 0xCF40
  .data     0x38A0003C  # 8000B978 => li        r5, 0x003C
  .data     0x48002AA1  # 8000B97C => bl        +0x00002AA0 /* 8000E41C */
  .data     0x481F18EC  # 8000B980 => b         +0x001F18EC /* 801FD26C */
  .data     0x806DB964  # 8000B984 => lwz       r3, [r13 - 0x469C]
  .data     0x1C63003C  # 8000B988 => mulli     r3, r3, 60
  .data     0x808DB948  # 8000B98C => lwz       r4, [r13 - 0x46B8]
  .data     0x3C840001  # 8000B990 => addis     r4, r4, 0x0001
  .data     0x38840B80  # 8000B994 => addi      r4, r4, 0x0B80
  .data     0x7C641A14  # 8000B998 => add       r3, r4, r3
  .data     0x3C808000  # 8000B99C => lis       r4, 0x8000
  .data     0x6084CF40  # 8000B9A0 => ori       r4, r4, 0xCF40
  .data     0x38A0003C  # 8000B9A4 => li        r5, 0x003C
  .data     0x48002A75  # 8000B9A8 => bl        +0x00002A74 /* 8000E41C */
  .data     0x806DB948  # 8000B9AC => lwz       r3, [r13 - 0x46B8]
  .data     0x481F51E0  # 8000B9B0 => b         +0x001F51E0 /* 80200B90 */
  .data     0x806DB964  # 8000B9B4 => lwz       r3, [r13 - 0x469C]
  .data     0x1C63003C  # 8000B9B8 => mulli     r3, r3, 60
  .data     0x808DB948  # 8000B9BC => lwz       r4, [r13 - 0x46B8]
  .data     0x3C840001  # 8000B9C0 => addis     r4, r4, 0x0001
  .data     0x38840B80  # 8000B9C4 => addi      r4, r4, 0x0B80
  .data     0x7C641A14  # 8000B9C8 => add       r3, r4, r3
  .data     0x38800000  # 8000B9CC => li        r4, 0x0000
  .data     0x38A0003C  # 8000B9D0 => li        r5, 0x003C
  .data     0x48002961  # 8000B9D4 => bl        +0x00002960 /* 8000E334 */
  .data     0x48003F75  # 8000B9D8 => bl        +0x00003F74 /* 8000F94C */
  .data     0x481F4668  # 8000B9DC => b         +0x001F4668 /* 80200044 */
  # region @ 8000CA40 (64 bytes)
  .data     0x8000CA40  # address
  .data     0x00000040  # size
  .data     0x28030000  # 8000CA40 => cmplwi    r3, 0
  .data     0x40820008  # 8000CA44 => bne       +0x00000008 /* 8000CA4C */
  .data     0x3BE00000  # 8000CA48 => li        r31, 0x0000
  .data     0x7C00F800  # 8000CA4C => cmp       r0, r31
  .data     0x481CBA70  # 8000CA50 => b         +0x001CBA70 /* 801D84C0 */
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
  .data     0x481C0B84  # 8000CA7C => b         +0x001C0B84 /* 801CD600 */
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
  .data     0x481CB180  # 8000CD88 => b         +0x001CB180 /* 801D7F08 */
  .data     0x38600003  # 8000CD8C => li        r3, 0x0003
  .data     0x3C808001  # 8000CD90 => lis       r4, 0x8001
  .data     0xB064CF78  # 8000CD94 => sth       [r4 - 0x3088], r3
  .data     0x7FC3F378  # 8000CD98 => mr        r3, r30
  .data     0x4826B4C0  # 8000CD9C => b         +0x0026B4C0 /* 8027825C */
  .data     0x3D808045  # 8000CDA0 => lis       r12, 0x8045
  .data     0x618C27C0  # 8000CDA4 => ori       r12, r12, 0x27C0
  .data     0x80030000  # 8000CDA8 => lwz       r0, [r3]
  .data     0x7C006000  # 8000CDAC => cmp       r0, r12
  .data     0xA0030004  # 8000CDB0 => lhz       r0, [r3 + 0x0004]
  .data     0x40820018  # 8000CDB4 => bne       +0x00000018 /* 8000CDCC */
  .data     0x2C000000  # 8000CDB8 => cmpwi     r0, 0
  .data     0x40820010  # 8000CDBC => bne       +0x00000010 /* 8000CDCC */
  .data     0x38600001  # 8000CDC0 => li        r3, 0x0001
  .data     0x3D808001  # 8000CDC4 => lis       r12, 0x8001
  .data     0xB06CCF78  # 8000CDC8 => sth       [r12 - 0x3088], r3
  .data     0x482408D4  # 8000CDCC => b         +0x002408D4 /* 8024D6A0 */
  .data     0x3C608000  # 8000CDD0 => lis       r3, 0x8000
  .data     0x6063CF3E  # 8000CDD4 => ori       r3, r3, 0xCF3E
  .data     0x3800001C  # 8000CDD8 => li        r0, 0x001C
  .data     0x7C0903A6  # 8000CDDC => mtctr     r0
  .data     0x38000000  # 8000CDE0 => li        r0, 0x0000
  .data     0xB4030002  # 8000CDE4 => sthu      [r3 + 0x0002], r0
  .data     0x4200FFFC  # 8000CDE8 => bdnz      -0x00000004 /* 8000CDE4 */
  .data     0x4832B50C  # 8000CDEC => b         +0x0032B50C /* 803382F8 */
  # region @ 801B7BB8 (4 bytes)
  .data     0x801B7BB8  # address
  .data     0x00000004  # size
  .data     0x38600000  # 801B7BB8 => li        r3, 0x0000
  # region @ 801CD5FC (4 bytes)
  .data     0x801CD5FC  # address
  .data     0x00000004  # size
  .data     0x4BE3F458  # 801CD5FC => b         -0x001C0BA8 /* 8000CA54 */
  # region @ 801D7F04 (4 bytes)
  .data     0x801D7F04  # address
  .data     0x00000004  # size
  .data     0x4BE34DFC  # 801D7F04 => b         -0x001CB204 /* 8000CD00 */
  # region @ 801D84BC (4 bytes)
  .data     0x801D84BC  # address
  .data     0x00000004  # size
  .data     0x4BE34584  # 801D84BC => b         -0x001CBA7C /* 8000CA40 */
  # region @ 801FD268 (4 bytes)
  .data     0x801FD268  # address
  .data     0x00000004  # size
  .data     0x4BE0E6F0  # 801FD268 => b         -0x001F1910 /* 8000B958 */
  # region @ 80200040 (4 bytes)
  .data     0x80200040  # address
  .data     0x00000004  # size
  .data     0x4BE0B974  # 80200040 => b         -0x001F468C /* 8000B9B4 */
  # region @ 80200B8C (4 bytes)
  .data     0x80200B8C  # address
  .data     0x00000004  # size
  .data     0x4BE0ADF8  # 80200B8C => b         -0x001F5208 /* 8000B984 */
  # region @ 802484A8 (8 bytes)
  .data     0x802484A8  # address
  .data     0x00000008  # size
  .data     0xA01F004A  # 802484A8 => lhz       r0, [r31 + 0x004A]
  .data     0x54030637  # 802484AC => rlwinm.   r3, r0, 0, 24, 27
  # region @ 8024D69C (4 bytes)
  .data     0x8024D69C  # address
  .data     0x00000004  # size
  .data     0x4BDBF704  # 8024D69C => b         -0x002408FC /* 8000CDA0 */
  # region @ 80277C0C (4 bytes)
  .data     0x80277C0C  # address
  .data     0x00000004  # size
  .data     0x3803BAA0  # 80277C0C => subi      r0, r3, 0x4560
  # region @ 80278258 (4 bytes)
  .data     0x80278258  # address
  .data     0x00000004  # size
  .data     0x4BD94B34  # 80278258 => b         -0x0026B4CC /* 8000CD8C */
  # region @ 802782B8 (8 bytes)
  .data     0x802782B8  # address
  .data     0x00000008  # size
  .data     0xA01F004A  # 802782B8 => lhz       r0, [r31 + 0x004A]
  .data     0x54030637  # 802782BC => rlwinm.   r3, r0, 0, 24, 27
  # region @ 803382F4 (4 bytes)
  .data     0x803382F4  # address
  .data     0x00000004  # size
  .data     0x4BCD4ADC  # 803382F4 => b         -0x0032B524 /* 8000CDD0 */
  # region @ 8044FC34 (52 bytes)
  .data     0x8044FC34  # address
  .data     0x00000034  # size
  .data     0x0004000D  # 8044FC34 => .invalid
  .data     0x0004000E  # 8044FC38 => .invalid
  .data     0x00000000  # 8044FC3C => .invalid
  .data     0x0004000F  # 8044FC40 => .invalid
  .data     0x00040010  # 8044FC44 => .invalid
  .data     0x00000000  # 8044FC48 => .invalid
  .data     0x00050000  # 8044FC4C => .invalid
  .data     0x00050001  # 8044FC50 => .invalid
  .data     0x00050002  # 8044FC54 => .invalid
  .data     0x00050003  # 8044FC58 => .invalid
  .data     0x00050004  # 8044FC5C => .invalid
  .data     0x00050005  # 8044FC60 => .invalid
  .data     0x00080000  # 8044FC64 => .invalid
  # region @ 80471E4C (4 bytes)
  .data     0x80471E4C  # address
  .data     0x00000004  # size
  .data     0xFFFFFFFF  # 80471E4C => fnmadd.   f31, f31, f31, f31
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
