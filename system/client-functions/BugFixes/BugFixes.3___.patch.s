.meta name="Bug fixes"
.meta description="Fixes many minor\ngameplay, sound,\nand graphical bugs"
# Most original codes by Ralf @ GC-Forever and Aleron Ives, except where noted
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

.versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC



  # Olga Flow Barta Bug Fix (makes barta work on ice weakness Olga Flow instead of damaging player)

  .label    g1_hook_call, <VERS 0x802BB4B0 0x802BC3E0 0x802BD528 0x802BD2C0 0x802BBEF4 0x802BBF38 0x802BD474 0x802BCC08>
  .label    g1_hook_loc, 0x8000D980
  .data     g1_hook_loc
  .deltaof  g1_hook_start, g1_hook_end
  .address  g1_hook_loc
g1_hook_start:
  lwz       r3, [r28]
  cmpwi     r3, 19
  bnelr
  li        r3, 0x0002
  blr
g1_hook_end:

  .data     g1_hook_call
  .data     4
  .address  g1_hook_call
  bl        g1_hook_loc



  # Morfos Frozen Player Bug Fix (stops Morfos Laser multi-hitting when player is frozen)

  .label    g2_hook_call, <VERS 0x80335060 0x803360CC 0x803375E8 0x8033739C 0x80335A50 0x80335A94 0x80337570 0x803369B4>
  .label    g2_hook_loc, 0x8000D9A0
  .data     g2_hook_loc
  .deltaof  g2_hook_start, g2_hook_end
  .address  g2_hook_loc
g2_hook_start:
  lfs       f2, [r2 - <VERS 0x0388 0x0380 0x0380 0x0380 0x0378 0x0378 0x0378 0x0378>]
  lwz       r3, [r30 + 0x0030]
  andi.     r3, r3, 0x0020
  beqlr
  lfs       f2, [r2 - <VERS 0x0370 0x0368 0x0368 0x0368 0x0360 0x0360 0x0360 0x0360>]
  blr
g2_hook_end:

  .data     g2_hook_call
  .data     4
  .address  g2_hook_call
  bl        g2_hook_loc



  # Tiny Grass Assassins Bug Fix

  .data     <VERS 0x800BC750 0x800BCA58 0x800BCBD0 0x800BCB80 0x800BC9E8 0x800BC9E8 0x800BCB90 0x800BCB58>
  .data     4
  b         +0x10



  # Bulclaw HP Bug Fix

  .data     <VERS 0x80091528 0x80091814 0x8009198C 0x8009193C 0x800917B4 0x800917B4 0x8009194C 0x80091914>
  .data     8
  bl        +0x024C
  sth       [r3 + 0x032C], r30



  # Control Tower: Delbiter Death SFX Bug Fix

  .label    g3_patch_loc, <VERS 0x80301600 0x803025CC 0x80303A1C 0x803037D0 0x80301F58 0x80301F9C 0x8030398C 0x80302D64>
  .data     g3_patch_loc
  .deltaof  g3_code_start, g3_code_end
  .address  g3_patch_loc
g3_code_start:
  b         +0x20
  subi      r3, r3, 0x57D0
  lwz       r0, [r13 - <VERS 0x4674 0x466C 0x464C 0x464C 0x465C 0x465C 0x463C 0x45FC>]
  cmpwi     r0, 35
  bne       g3_skip
  subi      r3, r3, 0x04D8
g3_skip:
  b         +0x8C
g3_code_end:

  .data     <VERS 0x803016A0 0x8030266C 0x80303ABC 0x80303870 0x80301FF8 0x8030203C 0x80303A2C 0x80302E04>
  .data     0x00000004
  b         -0x9C



  # Weapon Attributes Patch (allows attributes to work on minibosses and Olga Flow)

  .label    g4_hook_call1, <VERS 0x800142DC 0x8001430C 0x800146A4 0x800142BC 0x800142F4 0x800142F4 0x800142BC 0x80014334>
  .label    g4_hook_call2, <VERS 0x80015D04 0x80015D34 0x80016174 0x80015CE4 0x80015D1C 0x80015D1C 0x80015CE4 0x80015D5C>
  .label    g4_hook_loc, 0x8000C8C0
  .data     g4_hook_loc
  .deltaof  g4_hook_start, g4_hook_end
  .address  g4_hook_loc
g4_hook_start:
  andi.     r0, r0, 0x000F
g4_hook_entry2:
  andi.     r0, r0, 0x004F
  cmpwi     r0, 4
  blr
g4_hook_end:

  .data     g4_hook_call1
  .data     4
  .address  g4_hook_call1
  bl        g4_hook_start

  .data     g4_hook_call2
  .data     4
  .address  g4_hook_call2
  bl        g4_hook_entry2



  # Ruins Laser Fence SFX Bug Fix

  .data     <VERS 0x80166324 0x801666D8 0x80166848 0x8016679C 0x801666E0 0x801666E0 0x80166800 0x80166CC4>
  .data     8
  lis       r3, 0x4005
  b         +0x9C

  .data     <VERS 0x801663C0 0x80166774 0x801668E4 0x80166838 0x8016677C 0x8016677C 0x8016689C 0x80166D60>
  .data     4
  b         +0x1C



  # SFX Cancellation Distance Bug Fix

  .data     <VERS 0x805CB608 0x805D5C08 0x805DD0A8 0x805DCE48 0x805CBF10 0x805D2F30 0x805DC750 0x805D8990>
  .data     4
  .float    22500

  .data     <VERS 0x805CB8A8 0x805D5EA8 0x805DD348 0x805DD0E8 0x805CC1B0 0x805D31D0 0x805DC9F0 0x805D8C30>
  .data     4
  .float    200



  # Foie SFX Pitch Bug Fix

  .data     <VERS 0x8022E2A8 0x8022EC44 0x8022FB30 0x8022F8E4 0x8022EB64 0x8022EB64 0x8022FC18 0x8022F4B0>
  .data     4
  li        r4, 0xFFFFFF00

  .data     <VERS 0x8022E2D8 0x8022EC74 0x8022FB60 0x8022F914 0x8022EB94 0x8022EB94 0x8022FC48 0x8022F4E0>
  .data     4
  li        r4, 0xFFFFFE80

  .data     <VERS 0x8022E308 0x8022ECA4 0x8022FB90 0x8022F944 0x8022EBC4 0x8022EBC4 0x8022FC78 0x8022F510>
  .data     4
  li        r4, 0xFFFFFDB0



  # Gifoie SFX Pitch Bug Fix

  .data     <VERS 0x802300B8 0x80230A54 0x80231940 0x802316F4 0x80230974 0x80230974 0x80231A28 0x802312C0>
  .data     4
  li        r4, 0xFFFFFF00

  .data     <VERS 0x802300E8 0x80230A84 0x80231970 0x80231724 0x802309A4 0x802309A4 0x80231A58 0x802312F0>
  .data     4
  li        r4, 0xFFFFFE80

  .data     <VERS 0x80230118 0x80230AB4 0x802319A0 0x80231754 0x802309D4 0x802309D4 0x80231A88 0x80231320>
  .data     4
  li        r4, 0xFFFFFDB0



  # Rafoie SFX Pitch Bug Fix

  .data     <VERS 0x802365AC 0x80236F68 0x80237E54 0x80237C08 0x80236E88 0x80236E88 0x80237F3C 0x802377D4>
  .data     4
  li        r4, 0xFFFFFF00

  .data     <VERS 0x802365DC 0x80236F98 0x80237E84 0x80237C38 0x80236EB8 0x80236EB8 0x80237F6C 0x80237804>
  .data     4
  li        r4, 0xFFFFFE80

  .data     <VERS 0x8023660C 0x80236FC8 0x80237EB4 0x80237C68 0x80236EE8 0x80236EE8 0x80237F9C 0x80237834>
  .data     4
  li        r4, 0xFFFFFDB0

  .data     <VERS 0x80236FC0 0x8023797C 0x80238868 0x8023861C 0x8023789C 0x8023789C 0x80238950 0x802381E8>
  .data     4
  li        r4, 0xFFFFFF00

  .data     <VERS 0x80236FF0 0x802379AC 0x80238898 0x8023864C 0x802378CC 0x802378CC 0x80238980 0x80238218>
  .data     4
  li        r4, 0xFFFFFE80

  .data     <VERS 0x80237020 0x802379DC 0x802388C8 0x8023867C 0x802378FC 0x802378FC 0x802389B0 0x80238248>
  .data     4
  li        r4, 0xFFFFFDB0



  # Barta SFX Pitch Bug Fix

  .data     <VERS 0x80229B54 0x8022A4F0 0x8022B3E0 0x8022B190 0x8022A410 0x8022A410 0x8022B4C4 0x8022AD5C>
  .data     4
  li        r4, 0xFFFFFF00

  .data     <VERS 0x80229B84 0x8022A520 0x8022B410 0x8022B1C0 0x8022A440 0x8022A440 0x8022B4F4 0x8022AD8C>
  .data     4
  li        r4, 0xFFFFFE80

  .data     <VERS 0x80229BB4 0x8022A550 0x8022B440 0x8022B1F0 0x8022A470 0x8022A470 0x8022B524 0x8022ADBC>
  .data     4
  li        r4, 0xFFFFFDB0



  # Gibarta SFX Pitch Bug Fix

  .data     <VERS 0x8022EAB4 0x8022F450 0x80230340 0x802300F0 0x8022F370 0x8022F370 0x80230424 0x8022FCBC>
  .data     4
  li        r4, 0xFFFFFF00

  .data     <VERS 0x8022EAE4 0x8022F480 0x80230370 0x80230120 0x8022F3A0 0x8022F3A0 0x80230454 0x8022FCEC>
  .data     4
  li        r4, 0xFFFFFE80

  .data     <VERS 0x8022EB14 0x8022F4B0 0x802303A0 0x80230150 0x8022F3D0 0x8022F3D0 0x80230484 0x8022FD1C>
  .data     4
  li        r4, 0xFFFFFDB0



  # Rabarta SFX Pitch Bug Fix

  .data     <VERS 0x80235DD4 0x80236790 0x8023767C 0x80237430 0x802366B0 0x802366B0 0x80237764 0x80236FFC>
  .data     4
  li        r4, 0xFFFFFF00

  .data     <VERS 0x80235E10 0x802367CC 0x802376B8 0x8023746C 0x802366EC 0x802366EC 0x802377A0 0x80237038>
  .data     4
  li        r4, 0xFFFFFE80

  .data     <VERS 0x80235E4C 0x80236808 0x802376F4 0x802374A8 0x80236728 0x80236728 0x802377DC 0x80237074>
  .data     4
  li        r4, 0xFFFFFDB0



  # Zonde SFX Pitch Bug Fix

  .data     <VERS 0x8023B2C8 0x8023BC84 0x8023CB70 0x8023C924 0x8023BBA4 0x8023BBA4 0x8023CC58 0x8023C4F0>
  .data     4
  li        r4, 0xFFFFFF00

  .data     <VERS 0x8023B2F8 0x8023BCB4 0x8023CBA0 0x8023C954 0x8023BBD4 0x8023BBD4 0x8023CC88 0x8023C520>
  .data     4
  li        r4, 0xFFFFFE80

  .data     <VERS 0x8023B328 0x8023BCE4 0x8023CBD0 0x8023C984 0x8023BC04 0x8023BC04 0x8023CCB8 0x8023C550>
  .data     4
  li        r4, 0xFFFFFDB0



  # Gizonde SFX Pitch Bug Fix

  .data     <VERS 0x80230E08 0x802317C4 0x802326B0 0x80232464 0x802316E4 0x802316E4 0x80232798 0x80232030>
  .data     4
  li        r4, 0xFFFFFF00

  .data     <VERS 0x80230E38 0x802317F4 0x802326E0 0x80232494 0x80231714 0x80231714 0x802327C8 0x80232060>
  .data     4
  li        r4, 0xFFFFFE80

  .data     <VERS 0x80230E68 0x80231824 0x80232710 0x802324C4 0x80231744 0x80231744 0x802327F8 0x80232090>
  .data     4
  li        r4, 0xFFFFFDB0



  # Razonde SFX Pitch Bug Fix

  .data     <VERS 0x80237998 0x80238354 0x80239240 0x80238FF4 0x80238274 0x80238274 0x80239328 0x80238BC0>
  .data     4
  li        r4, 0xFFFFFF00

  .data     <VERS 0x802379C8 0x80238384 0x80239270 0x80239024 0x802382A4 0x802382A4 0x80239358 0x80238BF0>
  .data     4
  li        r4, 0xFFFFFE80

  .data     <VERS 0x802379F8 0x802383B4 0x802392A0 0x80239054 0x802382D4 0x802382D4 0x80239388 0x80238C20>
  .data     4
  li        r4, 0xFFFFFDB0



  # Grants SFX Pitch Bug Fix

  .data     <VERS 0x802316FC 0x802320B8 0x80232FA4 0x80232D58 0x80231FD8 0x80231FD8 0x8023308C 0x80232924>
  .data     4
  li        r4, 0xFFFFFF00

  .data     <VERS 0x80231734 0x802320F0 0x80232FDC 0x80232D90 0x80232010 0x80232010 0x802330C4 0x8023295C>
  .data     4
  li        r4, 0xFFFFFE80

  .data     <VERS 0x8023176C 0x80232128 0x80233014 0x80232DC8 0x80232048 0x80232048 0x802330FC 0x80232994>
  .data     4
  li        r4, 0xFFFFFDB0



  # Megid SFX Pitch Bug Fix

  .data     <VERS 0x802337A8 0x80234164 0x80235050 0x80234E04 0x80234084 0x80234084 0x80235138 0x802349D0>
  .data     4
  li        r4, 0xFFFFFF00

  .data     <VERS 0x802337D8 0x80234194 0x80235080 0x80234E34 0x802340B4 0x802340B4 0x80235168 0x80234A00>
  .data     4
  li        r4, 0xFFFFFE80

  .data     <VERS 0x80233808 0x802341C4 0x802350B0 0x80234E64 0x802340E4 0x802340E4 0x80235198 0x80234A30>
  .data     4
  li        r4, 0xFFFFFDB0



  # Anti SFX Pitch Bug Fix

  .data     <VERS 0x80229354 0x80229CF0 0x8022ABDC 0x8022A990 0x80229C10 0x80229C10 0x8022ACC4 0x8022A55C>
  .data     4
  cmpwi     r0, 1



  # Shield DFP/EVP Bug Fix (allows shields to reach true max DFP/EVP values)

  .data     <VERS 0x801185B0 0x801187CC 0x8011885C 0x80118764 0x80118854 0x80118854 0x80118774 0x8011894C>
  .data     4
  lbz       r0, [r4 + 0x0016]

  .data     <VERS 0x801185BC 0x801187D8 0x80118868 0x80118770 0x80118860 0x80118860 0x80118780 0x80118958>
  .data     4
  lbz       r0, [r4 + 0x0017]



  # VR Spaceship Item Drop Bug Fix (allows items to drop from enemies above a certain Y position)

  .data     <VERS 0x805C996C 0x805D3F6C 0x805DB40C 0x805DB1AC 0x805CA274 0x805D1294 0x805DAAB4 0x805D6CF4>
  .data     4
  .float    220



  # Invalid Items Bug Fix

  .data     <VERS 0x8011CA90 0x8011CCD4 0x8011CD0C 0x8011CC6C 0x8011CD34 0x8011CD34 0x8011CC7C 0x8011CE54>
  .data     0x0C
  mr        r3, r0
  subi      r3, r3, 0x0001
  b         -0x18

  .data     <VERS 0x8011CB4C 0x8011CD90 0x8011CDC8 0x8011CD28 0x8011CDF0 0x8011CDF0 0x8011CD38 0x8011CF10>
  .data     0x0C
  mr        r3, r0
  subi      r3, r3, 0x0001
  b         -0x18

  .data     <VERS 0x8011CB9C 0x8011CDE0 0x8011CE18 0x8011CD78 0x8011CE40 0x8011CE40 0x8011CD88 0x8011CF60>
  .data     0x0C
  mr        r4, r0
  subi      r4, r4, 0x0001
  b         -0x18



  # Item Removal Maxed Stats Bug Fix

  .label    g5_hook1_call, <VERS 0x801B9A88 0x801B9EF4 0x801BCF6C 0x801B9FC0 0x801B9E74 0x801B9E74 0x801BA024 0x801BA4E0>
  .label    g5_hook1_ret, <VERS 0x801B9A8C 0x801B9EF8 0x801BCF70 0x801B9FC4 0x801B9E78 0x801B9E78 0x801BA028 0x801BA4E4>
  .label    g5_hook2_call, <VERS 0x8010B970 0x8010BB70 0x8010BC04 0x8010BAF0 0x8010BC14 0x8010BC14 0x8010BB00 0x8010BCF0>
  .label    g5_hook3_call, <VERS 0x8010DD98 0x8010DF98 0x8010E0E4 0x8010DF24 0x8010E03C 0x8010E03C 0x8010DF34 0x8010E118>
  .label    g5_hook3_ret, <VERS 0x8010DD9C 0x8010DF9C 0x8010E0E8 0x8010DF28 0x8010E040 0x8010E040 0x8010DF38 0x8010E11C>
  .label    g5_hook3_apply_bonuses, <VERS 0x8010B90C 0x8010BB0C 0x8010BBA0 0x8010BA8C 0x8010BBB0 0x8010BBB0 0x8010BA9C 0x8010BC8C>
  .label    g5_hooks_loc, 0x8000B088
  .data     g5_hooks_loc
  .deltaof  g5_hook1_start, g5_hooks_end
  .address  g5_hooks_loc
g5_hook1_start:
  mr        r3, r29
  li        r4, 0x0000
  bl        [<VERS 801B97B4 801B9C20 801BCC98 801B9CEC 801B9BA0 801B9BA0 801B9D50 801BA20C>]
  mr        r3, r29
  b         g5_hook1_ret
g5_hook2_start:
  lbz       r0, [r31]
  cmplwi    r9, 1
  bnelr
  lbz       r0, [r31 + 1]
  addi      r31, r31, 2
  blr
g5_hook3_start:
  li        r9, 0x0000
  bl        g5_hook3_apply_bonuses
  mr        r3, r26
  mr        r4, r27
  mr        r5, r28
  mr        r6, r29
  mr        r7, r30
  mr        r8, r31
  li        r9, 1
  bl        g5_hook3_apply_bonuses
  b         g5_hook3_ret
g5_hooks_end:

  .data     g5_hook1_call
  .data     4
  .address  g5_hook1_call
  b         g5_hook1_start

  .data     g5_hook2_call
  .data     4
  .address  g5_hook2_call
  bl        g5_hook2_start

  .data     g5_hook3_call
  .data     4
  .address  g5_hook3_call
  b         g5_hook3_start

  .label    g5_hook4_loc, 0x8000C3F8
  .label    g5_hook4_call, <VERS 0x801B97B4 0x801B9C20 0x801BCC98 0x801B9CEC 0x801B9BA0 0x801B9BA0 0x801B9D50 0x801BA20C>
  .label    g5_hook4_ret, <VERS 0x801B97B8 0x801B9C24 0x801BCC9C 0x801B9CF0 0x801B9BA4 0x801B9BA4 0x801B9D54 0x801BA210>
  .label    TItemEquipBase_v16, <VERS 0x8010B094 0x8010B294 0x8010B390 0x8010B214 0x8010B338 0x8010B338 0x8010B224 0x8010B414>
  .label    TItemEquipBase_v17, <VERS 0x8010AF18 0x8010B118 0x8010B204 0x8010B098 0x8010B1BC 0x8010B1BC 0x8010B0A8 0x8010B298>
  .data     g5_hook4_loc
  .deltaof  g5_hook4_start, g5_hook4_end
  .address  g5_hook4_loc
g5_hook4_start:
  cmplwi    r4, 0
  beqlr
  stwu      [r1 - 0x0010], r1
  b         g5_hook4_ret
g5_hook4_entry:
  stwu      [r1 - 0x20], r1
  mflr      r0
  stw       [r1 + 0x24], r0
  stmw      [r1 + 0x08], r26
  mr        r31, r3
  bl        g5_hook4_start
  li        r30, 0
  addi      r29, r31, 0x0D04
  lwz       r27, [r31 + 0x032C]
g5_hook4_again:
  lwz       r28, [r29]
  mr.       r3, r28
  beq       g5_hook4_skip
  li        r4, 1
  bl        TItemEquipBase_v17
  mr        r3, r28
  li        r4, 1
  bl        TItemEquipBase_v16
g5_hook4_skip:
  addi      r29, r29, 4
  addi      r30, r30, 1
  cmpwi     r30, 13
  blt       g5_hook4_again
  stw       [r31 + 0x032C], r27
  lmw       r26, [r1 + 0x08]
  lwz       r0, [r1 + 0x24]
  mtlr      r0
  addi      r1, r1, 0x0020
  blr
g5_hook4_end:

  .data     g5_hook4_call
  .data     4
  .address  g5_hook4_call
  b         g5_hook4_entry



  # Unit Present Bug Fix

  .label    g6_hook_loc, 0x8000C640
  .label    g6_hook_call, <VERS 0x80118CE0 0x80118EFC 0x80118FD8 0x80118E94 0x80118F84 0x80118F84 0x80118EA4 0x8011907C>
  .data     g6_hook_loc
  .deltaof  g6_hook_start, g6_hook_end
  .address  g6_hook_loc
g6_hook_start:
  rlwinm.   r0, r4, 0, 25, 25
  beq       g6_hook_skip
  li        r4, 0x0000
g6_hook_skip:
  addi      r0, r4, 0x0009
  blr
g6_hook_end:

  .data     g6_hook_call
  .data     4
  .address  g6_hook_call
  bl        g6_hook_start



  # Bank Item Stacking Bug Fix

  .label    g7_hook1_loc, 0x8000C6D0
  .label    g7_hook1_call, <VERS 0x8021D098 0x8021D9FC 0x8021E8E8 0x8021E69C 0x8021D91C 0x8021D91C 0x8021E9D0 0x8021E268>
  .label    g7_hook2_call, <VERS 0x80220528 0x80220EBC 0x80221DA8 0x80221B5C 0x80220DDC 0x80220DDC 0x80221E90 0x80221728>
  .data     g7_hook1_loc
  .deltaof  g7_hook1_start, g7_hooks_end
  .address  g7_hook1_loc
g7_hook1_start:
  li        r0, 1
  stw       [r29 + 0x54], r0
  lwz       r3, [r29 + 0x24]
  blr
g7_hook2_start:
  li        r0, 1
  stw       [r31 + 0x0378], r0
  lwz       r3, [r31 + 0x24]
  blr
g7_hooks_end:

  .data     g7_hook1_call
  .data     4
  .address  g7_hook1_call
  bl        g7_hook1_start

  .data     g7_hook2_call
  .data     4
  .address  g7_hook2_call
  bl        g7_hook2_start



  # Dropped Mag Color Bug Fix

  .data     <VERS 0x80114378 0x8011458C 0x80114634 0x80114524 0x8011461C 0x8011461C 0x80114534 0x8011470C>
  .data     4
  li        r0, 0x12



  # Meseta Drop System Bug Fix

  .data     <VERS 0x80107478 0x80107654 0x80107708 0x801075D4 0x8010771C 0x8010771C 0x801075E4 0x801077D4>
  .data     4
  b         +0x0C

  .data     <VERS 0x8010748C 0x80107668 0x8010771C 0x801075E8 0x80107730 0x80107730 0x801075F8 0x801077E8>
  .data     4
  mr        r3, r0



  # Present Color Bug Fix

  .only_versions 3OJ2 3OE0 3OE1
  .data     <VERS 0x80101C14 0x80101EB8 0x80101EB8>
  .data     4
  nop
  .all_versions



  # Offline Quests Drop Table Bug Fix

  .data     <VERS 0x80104B48 0x80104D24 0x80104DE0 0x80104CA4 0x80104DEC 0x80104DEC 0x80104CB4 0x80104EA4>
  .data     4
  beq       +0x0C



  # Mag Revival Priority Bug Fix

  .label    g8_hook_loc, 0x8000C8A0
  .label    g8_hook_call, <VERS 0x80112664 0x80112864 0x80112A3C 0x801127F0 0x80112908 0x80112908 0x80112800 0x801129E4>
  .data     g8_hook_loc
  .deltaof  g8_hook_start, g8_hook_end
  .address  g8_hook_loc
g8_hook_start:
  mulli     r0, r0, 10
  rlwinm.   r4, r31, 0, 30, 30
  beqlr
  extsh     r0, r29
  blr
g8_hook_end:

  .data     g8_hook_call
  .data     4
  .address  g8_hook_call
  bl        g8_hook_start



  # Mag Revival Challenge & Quest Mode Bug Fix

  .data     <VERS 0x801CA1F4 0x801CA6E0 0x801CB5EC 0x801CA7AC 0x801CA610 0x801CA610 0x801CA810 0x801CACCC>
  .data     4
  b         +0x10



  # Chat Bubble Window TAB Bug Fix

  .data     <VERS 0x80250264 0x80250CB0 0x80251CA4 0x802519A4 0x80250AEC 0x80250AEC 0x80251C68 0x802514B0>
  .data     4
  nop



  # Chat Log Window LF/Tab Bug Fix

  .data     <VERS 0x80267DDC 0x80268A88 0x80269AE4 0x80269898 0x80268788 0x80268788 0x80269B5C 0x802693A4>
  .data     4
  nop



  # Dark/Hell Special GFX Bug Fix (makes Dark/Hell display graphic on success like in PSO BB)

  .label    g9_hook_loc, 0x8000E1E0
  .label    g9_hook_call1, <VERS 0x80355984 0x80356D88 0x803582E4 0x80358098 0x80356838 0x8035687C 0x80358464 0x80357858>
  .label    g9_hook_call2, <VERS 0x80355A04 0x80356E08 0x80358364 0x80358118 0x803568B8 0x803568FC 0x803584E4 0x803578D8>
  .data     g9_hook_loc
  .deltaof  g9_hook_start, g9_hook_end
  .address  g9_hook_loc
g9_hook_start:
  mflr      r30
  li        r5, 0x0000
  li        r6, 0x001E
  li        r7, 0x0040
  bl        [<VERS 800864A0 8008678C 80086904 800868B4 8008672C 8008672C 800868C4 8008688C>]
  mtlr      r30
  blr
g9_hook_end:

  .data     g9_hook_call1
  .data     4
  .address  g9_hook_call1
  bl        g9_hook_start

  .data     g9_hook_call2
  .data     4
  .address  g9_hook_call2
  bl        g9_hook_start

  .data     <VERS 0x80355960 0x80356D64 0x803582C0 0x80358074 0x80356814 0x80356858 0x80358440 0x80357834>
  .data     4
  li        r4, 0x01E8

  .data     <VERS 0x803559F4 0x80356DF8 0x80358354 0x80358108 0x803568A8 0x803568EC 0x803584D4 0x803578C8>
  .data     4
  li        r4, 0x01E8



  # Gol Dragon Camera Bug Fix (makes the camera after Gol Dragon display "normally")

  .data     <VERS 0x802FB99C 0x802FC968 0x802FDE60 0x802FDB6C 0x802FC2F4 0x802FC338 0x802FDD28 0x802FD100>
  .data     4
  cmpwi     r3, 1



  # Box/Fence Fadeout Bug Fix (stops boxes and other environmental objects fading in and out as you approach)

  .data     <VERS 0x80189A54 0x80189E2C 0x80189F90 0x80189EF0 0x80189E20 0x80189E20 0x80189F54 0x8018A418>
  .data     4
  nop

  .data     <VERS 0x801933DC 0x801937B0 0x80193914 0x80193874 0x801937A8 0x801937A8 0x801938D8 0x80193D9C>
  .data     4
  nop



  # TP Bar Color Bug Fix

  .data     <VERS 0x8026DA74 0x8026E738 0x8026F794 0x8026F548 0x8026E2D4 0x8026E2D4 0x8026F6FC 0x8026EF44>
  .data     4
  subi      r4, r4, 0x5506

  .data     <VERS 0x8026DB88 0x8026E84C 0x8026F8A8 0x8026F65C 0x8026E3E8 0x8026E3E8 0x8026F810 0x8026F058>
  .data     4
  subi      r3, r3, 0x5506

  .data     <VERS 0x8026DC10 0x8026E8D4 0x8026F930 0x8026F6E4 0x8026E470 0x8026E470 0x8026F898 0x8026F0E0>
  .data     4
  subi      r4, r3, 0x5506

  .data     <VERS 0x804CBB40 0x804CF290 0x804D17E0 0x804D1580 0x804CC310 0x804CC7F0 0x804D0E58 0x804D1248>
  .data     4
  .data     0xFF0074EE



  # Devil's and Demon's Special Damage Display Bug Fix

  .data     <VERS 0x8001306C 0x8001309C 0x80013364 0x8001304C 0x80013084 0x80013084 0x8001304C 0x800130C4>
  .data     4
  b         -0x0340



  # Christmas Trees Bug Fix

  .label    g10_hook_loc, 0x8000B5C8
  .label    g10_hook_call, <VERS 0x80183E94 0x8018425C 0x801843C0 0x80184320 0x80184250 0x80184250 0x80184384 0x80184848>
  .label    g10_hook_ret, <VERS 0x80183E98 0x80184260 0x801843C4 0x80184324 0x80184254 0x80184254 0x80184388 0x8018484C>
  .data     g10_hook_loc
  .deltaof  g10_hook_start, g10_hook_end
  .address  g10_hook_loc
g10_hook_start:
  lwz       r3, [r3 + 0x98]
  bl        [<VERS 803DFCC0 803E269C 803E453C 803E42EC 803E0F64 803E0FBC 803E46BC 803E31AC>]
  lwz       r3, [r31 + 0x042C]
  lwz       r4, [r31 + 0x0430]
  b         g10_hook_ret
g10_hook_end:

  .data     g10_hook_call
  .data     4
  .address  g10_hook_call
  b         g10_hook_start

  .data     <VERS 0x80183ED4 0x8018429C 0x80184400 0x80184360 0x80184290 0x80184290 0x801843C4 0x80184888>
  .data     4
  nop



  # Rain Drops Color Bug Fix

  .data     <VERS 0x804B3738 0x804B6E58 0x804B92F8 0x804B90B8 0x804B3EF0 0x804B43D0 0x804B8990 0x804B8E10>
  .data     8
  .data     0x70808080
  .data     0x60707070



  # Reverser Target Lock Bug Fix

  .data     <VERS 0x801C5EA4 0x801C6360 0x801C6604 0x801C642C 0x801C62C0 0x801C62C0 0x801C6490 0x801C694C>
  .data     4
  addi      r4, r31, 0x02FC



  # Deband/Shifta/Resta Target Bug Fix

  .data     <VERS 0x8022CF84 0x8022D920 0x8022E85C 0x8022E5C0 0x8022D840 0x8022D840 0x8022E8F4 0x8022E18C>
  .data     4
  bgt       +0x0630

  .only_versions 3OJ2 3OE0 3OE1
  .data     <VERS 0x8022D278 0x8022DB34 0x8022DB34>
  .data     4
  bgt       +0x033C

  .data     <VERS 0x8022D36C 0x8022DC28 0x8022DC28>
  .data     4
  bgt       +0x0248
  .all_versions



  # Tech Auto Targeting Bug Fix

  .data       <VERS 0x8022C850 0x8022D1EC 0x8022E128 0x8022DE8C 0x8022D10C 0x8022D10C 0x8022E1C0 0x8022DA58>
  .data       4
  nop

  .data       <VERS 0x804C6EE4 0x804CA61C 0x804CCB6C 0x804CC90C 0x804C76B4 0x804C7B94 0x804CC1E4 0x804CC5D4>
  .data       4
  .data       0x0000001E

  .data       <VERS 0x804C6F3C 0x804CA674 0x804CCBC4 0x804CC964 0x804C770C 0x804C7BEC 0x804CC23C 0x804CC62C>
  .data       4
  .data       0x00000028

  .data       <VERS 0x804C6F68 0x804CA6A0 0x804CCBF0 0x804CC990 0x804C7738 0x804C7C18 0x804CC268 0x804CC658>
  .data       4
  .data       0x00000032

  .data       <VERS 0x804C6F94 0x804CA6CC 0x804CCC1C 0x804CC9BC 0x804C7764 0x804C7C44 0x804CC294 0x804CC684>
  .data       4
  .data       0x0000003C

  .data       <VERS 0x804C6FA4 0x804CA6DC 0x804CCC2C 0x804CC9CC 0x804C7774 0x804C7C54 0x804CC2A4 0x804CC694>
  .data       4
  .data       0x0018003C

  .data       <VERS 0x804C71FC 0x804CA934 0x804CCE84 0x804CCC24 0x804C79CC 0x804C7EAC 0x804CC4FC 0x804CC8EC>
  .data       4
  .data       0x00000028



  # Enable Trap Animations

  .label    g11_hook_loc, 0x8000BBD0
  .label    g11_hook_call, <VERS 0x80170C54 0x80171008 0x80171260 0x801710CC 0x80171010 0x80171010 0x80171130 0x801715F4>
  .data     g11_hook_loc
  .deltaof  g11_hook_start, g11_hook_end
  .address  g11_hook_loc
g11_hook_start:
  lwz       r4, [r31 + 0x0370]
  subi      r4, r4, 0x0400
  stw       [r31 + 0x0370], r4
  lwz       r3, [r31 + 0x14]
  cmplwi    r3, 0
  beqlr
  stw       [r3 + 0x0060], r4
  blr
g11_hook_end:

  .data     g11_hook_call
  .data     4
  .address  g11_hook_call
  bl        g11_hook_start

  .data     <VERS 0x80170C74 0x80171028 0x80171280 0x801710EC 0x80171030 0x80171030 0x80171150 0x80171614>
  .data     4
  ori       r0, r4, 0x0420



  # Belra arm bug fix (this part by fuzziqersoftware)

  .only_versions 3OJ2 3OE0 3OE1
  .label    g12_hook1_call, <VERS 0x80095724 0x800959B0 0x800959B0>
  .label    g12_hook2_call, <VERS 0x80095734 0x800959C0 0x800959C0>
  .label    g12_hook_loc, 0x8000B06C
  .data     g12_hook_loc
  .deltaof  g12_hook1_start, g12_hook_end
  .address  g12_hook_loc
g12_hook1_start:
  li        r0, 1
  stw       [r13 - <VERS 0x2E48 0x2E30 0x2E30>], r0  # Anchor: 80039388 @ 3OE1
  b         [<VERS 803D3140 803D4410 803D4468>]
g12_hook2_start:
  li        r4, 0
  stw       [r13 - <VERS 0x2E48 0x2E30 0x2E30>], r4
  lwz       r4, [r28 + 0x04]
  blr
g12_hook_end:

  .data     g12_hook1_call
  .data     4
  .address  g12_hook1_call
  bl        g12_hook1_start

  .data     g12_hook2_call
  .data     4
  .address  g12_hook2_call
  bl        g12_hook2_start

  .all_versions



  # Tsumikiri J-Sword special attack + rapid weapon switch bug fix (this part
  # by fuzziqersoftware)

  .label    tjs_switch_fix_hook_call, <VERS 0x8034CFA8 0x8034E3AC 0x8034F908 0x8034F6BC 0x8034DE5C 0x8034DEA0 0x8034FA88 0x8034EE7C>
  .label    tjs_switch_fix_hook_loc, 0x8000B050
  .data     tjs_switch_fix_hook_loc
  .deltaof  tjs_switch_fix_hook_start, tjs_switch_fix_hook_end
  .address  tjs_switch_fix_hook_loc
tjs_switch_fix_hook_start:
  lwz       r0, [r3 + 0x0188]
  cmpwi     r0, 0
  bnelr
  mflr      r31
  addi      r31, r31, 0x100
  mtlr      r31
  blr
tjs_switch_fix_hook_end:

  .data     tjs_switch_fix_hook_call
  .data     8
  .address  tjs_switch_fix_hook_call
  beq       +0x108
  bl        tjs_switch_fix_hook_loc



  .data     0
  .data     0
