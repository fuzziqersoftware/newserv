.meta name="Bug fixes (WIP)"
.meta description="Fixes many minor\ngameplay, sound,\nand graphical bugs"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049
# Xbox port by fuzziqersoftware

# This patch is a collection of many smaller patches, most of which are not yet
# ported. See the comments after the patch code.

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksXB

  # Tiny Grass Assassins Bug Fix
  .data     0x0016229A
  .data     0x00000002
  .binary   EB0E

  # Shield DFP/EVP Bug Fix (allows shields to reach true max DFP/EVP values)
  .data     0x00185F6E
  .data     0x00000001
  .binary   16
  .data     0x00185F77
  .data     0x00000001
  .binary   17

  # VR Spaceship Item Drop Bug Fix (allows items to drop from enemies above a certain Y position)
  .data     0x00175ED5
  .data     0x00000002
  .data     0x435C0000

  # Dropped Mag Colour Bug Fix
  # Not needed on Xbox

  # Gol Dragon Camera Bug Fix (makes the camera after Gol Dragon display "normally")
  .data     0x000A8961
  .data     0x00000002
  .binary   01

  # Rain Drops Colour Bug Fix
  .data     0x00552508
  .data     0x00000008
  .binary   7080808060707070

  # Olga Flow Barta Bug Fix (makes Barta work on ice weakness Olga Flow instead of damaging player)
  # 000970E0 8B8B40040000   mov    ecx, [ebx + 0x440]  # eax is zero when we get here; can use eax, ecx, edx
  # 000970E6 EB3C           jmp    00097124
  .data     0x000970E0
  .data     0x00000008
  .binary   8B8B40040000EB3C
  # 00097124 E83C020000     call   00097365
  # 00097129 74D3           je     000970FE
  # 0009712B EBBB           jmp    000970E8
  .data     0x00097124
  .data     0x00000009
  .binary   E83C02000074D3EBBB
  # 00097365 83F919         cmp    ecx, 0x19
  # 00097368 7502           jne    +2
  # 0009736A B102           mov    cl, 2
  # 0009736C 39CE           cmp    esi, ecx
  # 0009736E C3             ret
  .data     0x00097365
  .data     0x0000000A
  .binary   83F9197502B10239CEC3

  # TP Bar Colour Bug Fix
  .data     0x00277ECE
  .data     0x00000004
  .data     0xFF00AAFA
  .data     0x00277EDE
  .data     0x00000004
  .data     0xFF00AAFA
  .data     0x00277F24
  .data     0x00000004
  .data     0xFF00AAFA
  .data     0x0054A2D4
  .data     0x00000004
  .data     0xFF0074EE

  .data     0x00000000
  .data     0x00000000





# Currently-unported patches:

# Tech Auto Targetting Bug Fix
# 8022D10C 60000000 // TODO
# 0054C968 0000001E // done
# 0054C9C0 00000028 // done
# 0054C9EC 00000032 // done
# 0054CA18 0000003C // done
# 0054CA28 0018003C // done
# 0054CC80 00000028 // done

# Morfos Frozen Player Bug Fix (stops Morfos Laser multi-hitting when player is frozen)
# US11------------- DISASSEMBLY (US10)
# 8000D9A0 C042FC88 lfs       f2, [r2 - 0x0378]
# 8000D9A4 807E0030 lwz       r3, [r30 + 0x0030]
# 8000D9A8 70630020 andi.     r3, r3, 0x0020
# 8000D9AC 41820008 beq       +0x00000008 /* 8000D9B4 */
# 8000D9B0 C042FCA0 lfs       f2, [r2 - 0x0360]
# 8000D9B4 483280E4 b         +0x003280A0 /* 80335A54 */
# 80335A94 4BCD7F0C b         -0x003280B0 /* 8000D9A0 */

# Bulclaw HP Bug Fix
# US11------------- DISASSEMBLY (US10)
# 800917B4 4800024D bl        +0x0000024C /* 80091A00 */
# 800917B8 B3C3032C sth       [r3 + 0x032C], r30
# => 000EB350 on XBOX-US1, but param used on GC was optimized out

# Control Tower: Delbiter Death SFX Bug Fix
# US11------------- DISASSEMBLY (US10)
# 80301F9C 48000020 b         +0x00000020 /* 80301F78 */
# 80301FA0 3863A830 subi      r3, r3, 0x57D0
# 80301FA4 800DB9A4 lwz       r0, [r13 - 0x465C]
# 80301FA8 2C000023 cmpwi     r0, 35
# 80301FAC 40820008 bne       +0x00000008 /* 80301F70 */
# 80301FB0 3863FB28 subi      r3, r3, 0x04D8
# 80301FB4 4800008C b         +0x0000008C /* 80301FFC */
# 8030203C 4BFFFF64 b         -0x0000009C /* 80301F5C */

# Weapon Attributes Patch (allows attributes to work on minibosses and Olga Flow)
# US11------------- DISASSEMBLY (US10)
# 8000C8C0 7000000F andi.     r0, r0, 0x000F
# 8000C8C4 7000004F andi.     r0, r0, 0x004F
# 8000C8C8 2C000004 cmpwi     r0, 4
# 8000C8CC 4E800020 blr
# 800142F4 4BFF85CD bl        -0x00007A34 /* 8000C8C0 */
# 80015D1C 4BFF6BA9 bl        -0x00009458 /* 8000C8C4 */

# Ruins Laser Fence SFX Bug Fix
# US11------------- DISASSEMBLY (US10)
# 801666E0 3C604005 lis       r3, 0x4005
# 801666E4 4800009C b         +0x0000009C /* 80166780 */
# 8016677C 4800001C b         +0x0000001C /* 80166798 */

# SFX Cancellation Distance Bug Fix
# US11------------- DISASSEMBLY (US10)
# 805D2F30 46AFC800
# 805D31D0 43480000

# Foie SFX Pitch Bug Fix
# US11------------- DISASSEMBLY (US10)
# 8022EB64 3880FF00 li        r4, 0xFFFFFF00
# 8022EB94 3880FE80 li        r4, 0xFFFFFE80
# 8022EBC4 3880FDB0 li        r4, 0xFFFFFDB0

# Gifoie SFX Pitch Bug Fix
# US11------------- DISASSEMBLY (US10)
# 80230974 3880FF00 li        r4, 0xFFFFFF00
# 802309A4 3880FE80 li        r4, 0xFFFFFE80
# 802309D4 3880FDB0 li        r4, 0xFFFFFDB0

# Rafoie SFX Pitch Bug Fix
# US11------------- DISASSEMBLY (US10)
# 80236E88 3880FF00 li        r4, 0xFFFFFF00
# 80236EB8 3880FE80 li        r4, 0xFFFFFE80
# 80236EE8 3880FDB0 li        r4, 0xFFFFFDB0
# 8023789C 3880FF00 li        r4, 0xFFFFFF00
# 802378CC 3880FE80 li        r4, 0xFFFFFE80
# 802378FC 3880FDB0 li        r4, 0xFFFFFDB0

# Barta SFX Pitch Bug Fix
# US11------------- DISASSEMBLY (US10)
# 8022A410 3880FF00 li        r4, 0xFFFFFF00
# 8022A440 3880FE80 li        r4, 0xFFFFFE80
# 8022A470 3880FDB0 li        r4, 0xFFFFFDB0

# Gibarta SFX Pitch Bug Fix
# US11------------- DISASSEMBLY (US10)
# 8022F370 3880FF00 li        r4, 0xFFFFFF00
# 8022F3A0 3880FE80 li        r4, 0xFFFFFE80
# 8022F3D0 3880FDB0 li        r4, 0xFFFFFDB0

# Rabarta SFX Pitch Bug Fix
# US11------------- DISASSEMBLY (US10)
# 802366B0 3880FF00 li        r4, 0xFFFFFF00
# 802366EC 3880FE80 li        r4, 0xFFFFFE80
# 80236728 3880FDB0 li        r4, 0xFFFFFDB0

# Zonde SFX Pitch Bug Fix
# US11------------- DISASSEMBLY (US10)
# 8023BBA4 3880FF00 li        r4, 0xFFFFFF00
# 8023BBD4 3880FE80 li        r4, 0xFFFFFE80
# 8023BC04 3880FDB0 li        r4, 0xFFFFFDB0

# Gizonde SFX Pitch Bug Fix
# US11------------- DISASSEMBLY (US10)
# 802316E4 3880FF00 li        r4, 0xFFFFFF00
# 80231714 3880FE80 li        r4, 0xFFFFFE80
# 80231744 3880FDB0 li        r4, 0xFFFFFDB0

# Razonde SFX Pitch Bug Fix
# US11------------- DISASSEMBLY (US10)
# 80238274 3880FF00 li        r4, 0xFFFFFF00
# 802382A4 3880FE80 li        r4, 0xFFFFFE80
# 802382D4 3880FDB0 li        r4, 0xFFFFFDB0

# Grants SFX Pitch Bug Fix
# US11------------- DISASSEMBLY (US10)
# 80231FD8 3880FF00 li        r4, 0xFFFFFF00
# 80232010 3880FE80 li        r4, 0xFFFFFE80
# 80232048 3880FDB0 li        r4, 0xFFFFFDB0

# Megid SFX Pitch Bug Fix
# US11------------- DISASSEMBLY (US10)
# 80234084 3880FF00 li        r4, 0xFFFFFF00
# 802340B4 3880FE80 li        r4, 0xFFFFFE80
# 802340E4 3880FDB0 li        r4, 0xFFFFFDB0

# Anti SFX Pitch Bug Fix
# US11------------- DISASSEMBLY (US10)
# 80229C10 2C000001 cmpwi     r0, 1

# Invalid Items Bug Fix (something to do with making invalid items correctly display as ???? I think)
# US11------------- DISASSEMBLY (US10)
# 8011CD34 7C030378 mr        r3, r0
# 8011CD38 3863FFFF subi      r3, r3, 0x0001
# 8011CD3C 4BFFFFE8 b         -0x00000018 /* 8011CD24 */
# 8011CDF0 7C030378 mr        r3, r0
# 8011CDF4 3863FFFF subi      r3, r3, 0x0001
# 8011CDF8 4BFFFFE8 b         -0x00000018 /* 8011CDE0 */
# 8011CE40 7C040378 mr        r4, r0
# 8011CE44 3884FFFF subi      r4, r4, 0x0001
# 8011CE48 4BFFFFE8 b         -0x00000018 /* 8011CE30 */

# Item Removal Maxed Stats Bug Fix
# US11------------- DISASSEMBLY (US10)
# 8000B088 7FA3EB78 mr        r3, r29
# 8000B08C 38800000 li        r4, 0x0000
# 8000B090 481AEB11 bl        +0x001AEB10 /* 801B9BA0 */
# 8000B094 7FA3EB78 mr        r3, r29
# 8000B098 481AEDE0 b         +0x001AEDE0 /* 801B9E78 */
# 8000B09C 881F0000 lbz       r0, [r31]
# 8000B0A0 28090001 cmplwi    r9, 1
# 8000B0A4 4082000C bne       +0x0000000C /* 8000B0B0 */
# 8000B0A8 881F0001 lbz       r0, [r31 + 0x0001]
# 8000B0AC 3BFF0002 addi      r31, r31, 0x0002
# 8000B0B0 48100B68 b         +0x00100B68 /* 8010BC18 */
# 8000B0B4 39200000 li        r9, 0x0000
# 8000B0B8 48100AF9 bl        +0x00100AF8 /* 8010BBB0 */
# 8000B0BC 7F43D378 mr        r3, r26
# 8000B0C0 7F64DB78 mr        r4, r27
# 8000B0C4 7F85E378 mr        r5, r28
# 8000B0C8 7FA6EB78 mr        r6, r29
# 8000B0CC 7FC7F378 mr        r7, r30
# 8000B0D0 7FE8FB78 mr        r8, r31
# 8000B0D4 39200001 li        r9, 0x0001
# 8000B0D8 48100AD9 bl        +0x00100AD8 /* 8010BBB0 */
# 8000B0DC 48102F64 b         +0x00102F64 /* 8010E040 */
# 8000C3F8 28040000 cmplwi    r4, 0
# 8000C3FC 4D820020 beqlr
# 8000C400 9421FFF0 stwu      [r1 - 0x0010], r1
# 8000C404 481AD7A0 b         +0x001AD7A0 /* 801B9BA4 */
# 8000C408 9421FFE0 stwu      [r1 - 0x0020], r1
# 8000C40C 7C0802A6 mflr      r0
# 8000C410 90010024 stw       [r1 + 0x0024], r0
# 8000C414 BF410008 stmw      [r1 + 0x0008], r26
# 8000C418 7C7F1B78 mr        r31, r3
# 8000C41C 4BFFFFDD bl        -0x00000024 /* 8000C3F8 */
# 8000C420 3BC00000 li        r30, 0x0000
# 8000C424 3BBF0D04 addi      r29, r31, 0x0D04
# 8000C428 837F032C lwz       r27, [r31 + 0x032C]
# 8000C42C 839D0000 lwz       r28, [r29]
# 8000C430 7F83E379 mr.       r3, r28
# 8000C434 41820018 beq       +0x00000018 /* 8000C44C */
# 8000C438 38800001 li        r4, 0x0001
# 8000C43C 480FED81 bl        +0x000FED80 /* 8010B1BC */
# 8000C440 7F83E378 mr        r3, r28
# 8000C444 38800001 li        r4, 0x0001
# 8000C448 480FEEF1 bl        +0x000FEEF0 /* 8010B338 */
# 8000C44C 3BBD0004 addi      r29, r29, 0x0004
# 8000C450 3BDE0001 addi      r30, r30, 0x0001
# 8000C454 2C1E000D cmpwi     r30, 13
# 8000C458 4180FFD4 blt       -0x0000002C /* 8000C42C */
# 8000C45C 937F032C stw       [r31 + 0x032C], r27
# 8000C460 BB410008 lmw       r26, [r1 + 0x0008]
# 8000C464 80010024 lwz       r0, [r1 + 0x0024]
# 8000C468 7C0803A6 mtlr      r0
# 8000C46C 38210020 addi      r1, r1, 0x0020
# 8000C470 4E800020 blr
# 8010BC14 4BEFF488 b         -0x00100B78 /* 8000B09C */
# 8010E03C 4BEFD078 b         -0x00102F88 /* 8000B0B4 */
# 801B9BA0 4BE52868 b         -0x001AD798 /* 8000C408 */
# 801B9E74 4BE51214 b         -0x001AEDEC /* 8000B088 */

# Unit Present Bug Fix
# US11------------- DISASSEMBLY (US10)
# 8000C640 54800673 rlwinm.   r0, r4, 0, 25, 25
# 8000C644 41820008 beq       +0x00000008 /* 8000C64C */
# 8000C648 38800000 li        r4, 0x0000
# 8000C64C 38040009 addi      r0, r4, 0x0009
# 8000C650 4810C938 b         +0x0010C938 /* 80118F88 */
# 80118F84 4BEF36BC b         -0x0010C944 /* 8000C640 */

# Bank Item Stacking Bug Fix
# US11------------- DISASSEMBLY (US10)
# 8000C6D0 38000001 li        r0, 0x0001
# 8000C6D4 901D0054 stw       [r29 + 0x0054], r0
# 8000C6D8 807D0024 lwz       r3, [r29 + 0x0024]
# 8000C6DC 48211244 b         +0x00211244 /* 8021D920 */
# 8000C6E0 38000001 li        r0, 0x0001
# 8000C6E4 901F0378 stw       [r31 + 0x0378], r0
# 8000C6E8 807F0024 lwz       r3, [r31 + 0x0024]
# 8000C6EC 482146F4 b         +0x002146F4 /* 80220DE0 */
# 8021D91C 4BDEEDB4 b         -0x0021124C /* 8000C6D0 */
# 80220DDC 4BDEB904 b         -0x002146FC /* 8000C6E0 */

# Meseta Drop System Bug Fix
# US11------------- DISASSEMBLY (US10)
# 8010771C 4800000C b         +0x0000000C /* 80107728 */
# 80107730 7C030378 mr        r3, r0

# Present Colour Bug Fix (TODO: which versions need this?)
# US11------------- DISASSEMBLY (US10)
# 80101EB8 60000000 nop

# Offline Quests Drop Table Bug Fix
# US11------------- DISASSEMBLY (US10)
# 80104DEC 4182000C beq       +0x0000000C /* 80104DF8 */

# Mag Revival Priority Bug Fix
# US11------------- DISASSEMBLY (US10)
# 8000C8A0 1C00000A mulli     r0, r0, 10
# 8000C8A4 57E407BD rlwinm.   r4, r31, 0, 30, 30
# 8000C8A8 41820008 beq       +0x00000008 /* 8000C8B0 */
# 8000C8AC 7FA00734 extsh     r0, r29
# 8000C8B0 4810605C b         +0x0010605C /* 8011290C */
# 80112908 4BEF9F98 b         -0x00106068 /* 8000C8A0 */

# Mag Revival Challenge & Quest Mode Bug Fix
# US11------------- DISASSEMBLY (US10)
# 801CA610 48000010 b         +0x00000010 /* 801CA620 */

# Chat Bubble Window TAB Bug Fix
# US11------------- DISASSEMBLY (US10)
# 80250AEC 60000000 nop

# Chat Log Window LF/Tab Bug Fix
# US11------------- DISASSEMBLY (US10)
# 80268788 60000000 nop

# Dark/Hell Special GFX Bug Fix (makes Dark/Hell display graphic on success like in PSO BB)
# US11------------- DISASSEMBLY (US10)
# 8000E1E0 7FC802A6 mflr      r30
# 8000E1E4 38A00000 li        r5, 0x0000
# 8000E1E8 38C0001E li        r6, 0x001E
# 8000E1EC 38E00040 li        r7, 0x0040
# 8000E1F0 4807853D bl        +0x0007853C /* 8008672C */
# 8000E1F4 7FC803A6 mtlr      r30
# 8000E1F8 4E800020 blr
# 80356858 388001E8 li        r4, 0x01E8
# 8035687C 4BCB7965 bl        -0x00348658 /* 8000E1E0 */
# 803568EC 388001E8 li        r4, 0x01E8
# 803568FC 4BCB78E5 bl        -0x003486D8 /* 8000E1E0 */

# Box/Fence Fadeout Bug Fix (stops boxes and other environmental objects fading in and out as you approach)
# US11------------- DISASSEMBLY (US10)
# 80189E20 60000000 nop
# 801937A8 60000000 nop

# Devil's and Demon's Special Damage Display Bug Fix
# US11------------- DISASSEMBLY (US10)
# 80013084 4BFFFCC0 b         -0x00000340 /* 80012D44 */

# Christmas Trees Bug Fix
# US11------------- DISASSEMBLY (US10)
# 8000B5C8 80630098 lwz       r3, [r3 + 0x0098]
# 8000B5CC 483D59F1 bl        +0x003D5998 /* 803E0F64 */
# 8000B5D0 807F042C lwz       r3, [r31 + 0x042C]
# 8000B5D4 809F0430 lwz       r4, [r31 + 0x0430]
# 8000B5D8 48178C7C b         +0x00178C7C /* 80184254 */
# 80184250 4BE87378 b         -0x00178C88 /* 8000B5C8 */
# 80184290 60000000 nop

# Reverser Target Lock Bug Fix
# US11------------- DISASSEMBLY (US10)
# TODO: This changes an argument to a virtual function to use TObjPlayer->center_pos instead of a
# Vector3F from the stack. What is the correct offset on XB's TObjPlayer? Some later fields are
# offset by 4 (eg 320 on GC => 324 on XB), but earlier fields are not (60 on GC => 60 on XB).
# 801C62C0 389F02FC addi      r4, r31, 0x02FC

# Deband/Shifta/Resta Target Bug Fix
# US11------------- DISASSEMBLY (US10)
# 8022D840 41810630 bgt       +0x00000630 /* 8022DE70 */
# 8022DB34 4181033C bgt       +0x0000033C /* 8022DE70 */
# 8022DC28 41810248 bgt       +0x00000248 /* 8022DE70 */

# Enable Trap Animations
# US11------------- DISASSEMBLY (US10)
# 8000BBD0 809F0370 lwz       r4, [r31 + 0x0370]
# 8000BBD4 3884FC00 subi      r4, r4, 0x0400
# 8000BBD8 909F0370 stw       [r31 + 0x0370], r4
# 8000BBDC 807F0014 lwz       r3, [r31 + 0x0014]
# 8000BBE0 28030000 cmplwi    r3, 0
# 8000BBE4 41820008 beq       +0x00000008 /* 8000BBEC */
# 8000BBE8 90830060 stw       [r3 + 0x0060], r4
# 8000BBEC 48165428 b         +0x00165428 /* 80171014 */
# 80171010 4BE9ABC0 b         -0x00165440 /* 8000BBD0 */
# 80171030 60800420 ori       r0, r4, 0x0420
