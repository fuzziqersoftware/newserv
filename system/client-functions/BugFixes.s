# Most original codes by Ralf @ GC-Forever and Aleron Ives, except where noted
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049
# Xbox ports by fuzziqersoftware

.meta visibility="all"
.meta name="Bug fixes"
.meta description="Fixes many minor\ngameplay, sound,\nand graphical bugs"


entry_ptr:
reloc0:
  .data     start
start:
  .include  WriteCodeBlocks



  # Olga Flow Barta Bug Fix (makes barta work on ice weakness Olga Flow instead of damaging player)

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

  .label    ofb_hook_call, <VERS 0x802BB4B0 0x802BC3E0 0x802BD528 0x802BD2C0 0x802BBEF4 0x802BBF38 0x802BD474 0x802BCC08>
  .label    ofb_hook_loc, 0x8000D980
  .data     ofb_hook_loc
  .data     ofb_hook_end - ofb_hook_start
  .address  ofb_hook_loc
ofb_hook_start:
  lwz       r3, [r28]
  cmpwi     r3, 19
  bnelr
  li        r3, 2
  blr
ofb_hook_end:

  .data     ofb_hook_call
  .data     4
  .address  ofb_hook_call
  bl        ofb_hook_loc

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU

  .label    ofb_hook_call, <VERS 0x000970E0 0x000973F0 0x00097460 0x00097140 0x000970E0 0x00097160 0x00096FE0>
  .label    ofb_hook_loc, <VERS 0x00097124 0x00097434 0x000974A4 0x00097184 0x00097124 0x000971A4 0x00097024>
  .data     ofb_hook_call
  .data     6
  .address  ofb_hook_call
  mov       eax, esi
  cmp       al, 19
  jmp       ofb_hook_loc
ofb_hook_call_end:
  .data     ofb_hook_loc
  .data     ofb_hook_end - ofb_hook_start
  .address  ofb_hook_loc
ofb_hook_start:
  jne       ofb_hook_skip_replace_value
  mov       al, 2
ofb_hook_skip_replace_value:
  cmp       eax, [ebx + 0x440]  // Original opcode
  jmp       ofb_hook_call_end
ofb_hook_end:

  .versions 50YJ 59NJ 59NL

  .label    ofb_hook_call, <VERS 0x0049CD0A 0x0049D2D2 0x0049D176>
  .label    ofb_hook_loc, <VERS 0x0049CDA1 0x0049D369 0x0049D20D>
  .label    TObjectV00b441c0::v18_accept_hit, <VERS 0x0076D830 0x007748D8 0x00773AFC>

  .data     ofb_hook_call
  .data     6
  .address  ofb_hook_call
  call      ofb_hook_conditional_replace_value
  nop

  .data     ofb_hook_loc
  .data     (ofb_hook_end - ofb_hook_start)
  .assert   (ofb_hook_end - ofb_hook_start) <= 0x27
  .address  ofb_hook_loc
ofb_hook_start:
  mov       cl, 6
ofb_hook_again:
  push      dword [esp + 0x24]
  dec       cl
  jnz       ofb_hook_again
  mov       ecx, edi
  call      TObjectV00b441c0::v18_accept_hit
  add       esp, 8
  pop       edi
  ret       0x18
ofb_hook_conditional_replace_value:
  cmp       al, 19
  jne       ofb_hook_skip_replace_value
  mov       al, 2
ofb_hook_skip_replace_value:
  cmp       eax, dword [edi + 0x444]
  ret
ofb_hook_end:



  # Morfos Frozen Player Bug Fix (stops Morfos Laser multi-hitting when player is frozen)

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

  .label    mfp_hook_call, <VERS 0x80335060 0x803360CC 0x803375E8 0x8033739C 0x80335A50 0x80335A94 0x80337570 0x803369B4>
  .label    mfp_hook_loc, 0x8000D9A0
  .data     mfp_hook_loc
  .data     mfp_hook_end - mfp_hook_start
  .address  mfp_hook_loc
mfp_hook_start:
  lfs       f2, [r2 - <VERS 0x0388 0x0380 0x0380 0x0380 0x0378 0x0378 0x0378 0x0378>]
  lwz       r3, [r30 + 0x0030]
  andi.     r3, r3, 0x0020
  beqlr
  lfs       f2, [r2 - <VERS 0x0370 0x0368 0x0368 0x0368 0x0360 0x0360 0x0360 0x0360>]
  blr
mfp_hook_end:

  .data     mfp_hook_call
  .data     4
  .address  mfp_hook_call
  bl        mfp_hook_loc

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU

  .label    mfp_hook_call, <VERS 0x0012E257 0x0012E387 0x0012E4E7 0x0012E537 0x0012E567 0x0012E557 0x0012E5A7>
  .label    mfp_hook_loc1, <VERS 0x0012E5F4 0x0012E724 0x0012E884 0x0012E8D4 0x0012E904 0x0012E8F4 0x0012E944>
  .label    mfp_hook_loc2, <VERS 0x0012E622 0x0012E752 0x0012E8B2 0x0012E902 0x0012E932 0x0012E922 0x0012E972>
  .data     mfp_hook_call
  .data     6
  .address  mfp_hook_call
  call      mfp_hook_loc1
  nop
  .data     mfp_hook_loc1
  .data     mfp_hook_end1 - mfp_hook_start1
  .address  mfp_hook_loc1
mfp_hook_start1:
  fld1      st0  // st = [1.0, speed]
  fld1      st0  // st = [1.0, 1.0, speed]
  fadd      st0, st1  // st = [2.0, 1.0, speed]
  fdivp     st1, st0  // st = [0.5, speed]
  jmp       mfp_hook_loc2
mfp_hook_end1:

  .data     mfp_hook_loc2
  .data     mfp_hook_end2 - mfp_hook_start2
  .address  mfp_hook_loc2
mfp_hook_start2:
  test      byte [esi + 0x30], 0x20  // If not set, use 1.5; if set, use 0.5
  jnz       mfp_hook_entity_is_frozen
  fld1      st0  // st = [1, 0.5, speed]
  faddp     st1, st0  // st = [1.5, speed]
mfp_hook_entity_is_frozen:
  fmulp     st1, st0  // st = [((game_flags & 0x20) ? 0.5 : 1.5) * speed]
  ret
mfp_hook_end2:

  .versions 50YJ 59NJ 59NL

  .data     <VERS 0x0057C045 0x0057D131 0x0057D071>
  .data     (mfp_hook_end - mfp_hook_start)
  .assert   (mfp_hook_end - mfp_hook_start) == 0x25
mfp_hook_start:
  xor       eax, eax
  test      byte [edi + 0x30], 0x20
  cmovz     eax, [<VERS 0x00A67CE0 0x00A71C80 0x00A74100>]
  mov       edx, [edi]
  mov       [esp + 0x18], ecx
  mov       [esp + 0x1C], eax
  push      0xFFFFFFFF
  push      0xFFFFFFFF
  push      0
  push      eax
  push      ecx
  push      ebp
  mov       ecx, edi
  nop
  nop
  nop
mfp_hook_end:



  # Tiny Grass Assassins Bug Fix

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x800BC750 0x800BCA58 0x800BCBD0 0x800BCB80 0x800BC9E8 0x800BC9E8 0x800BCB90 0x800BCB58>
  .data     4
  b         +0x10

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x0016227A 0x0016238A 0x0016232A 0x0016240A 0x0016229A 0x0016242A 0x0016225A>
  .data     2
  .binary   EB0E

  .versions 50YJ 59NJ 59NL
  .data     <VERS 0x00521785 0x00522785 0x005226E9>
  .data     2
  .binary   EB0C



  # Bulclaw HP Bug Fix

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x80091528 0x80091814 0x8009198C 0x8009193C 0x800917B4 0x800917B4 0x8009194C 0x80091914>
  .data     8
  bl        +0x024C
  sth       [r3 + 0x032C], r30

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x000EAE63 0x000EAEC3 0x000EAF03 0x000EAF83 0x000EAFD3 0x000EAFA3 0x000EAFD3>
  .data     (bulclaw_hp_patch_end - bulclaw_hp_patch_start)
  .assert   (bulclaw_hp_patch_end - bulclaw_hp_patch_start) == 0x23
bulclaw_hp_patch_start:
  mov       edx, [ebx + 0x28]
  mov       [esi + 0x28], edx
  mov       edx, dword [esp + 0x18]
  mov       word [esi + 0x330], dx
  mov       edx, [edi]
  mov       [esi + 0x38], edx
  mov       edx, [edi + 4]
  mov       [esi + 0x3C], edx
  mov       edx, [edi + 8]
  mov       [esi + 0x40], edx
  nop
bulclaw_hp_patch_end:

  .versions 50YJ 59NJ 59NL
  .data     <VERS 0x0053471D 0x00535759 0x005356A5>
  .data     (bulclaw_hp_patch_end - bulclaw_hp_patch_start)
  .assert   (bulclaw_hp_patch_end - bulclaw_hp_patch_start) == 0x10
bulclaw_hp_patch_start:
  mov       esi, [eax + 0x28]
  mov       [ecx + 0x28], esi
  mov       eax, dword [ebp + 0x20]
  mov       word [ecx + 0x334], ax
bulclaw_hp_patch_end:



  # Control Tower: Delbiter Death SFX Bug Fix
  # TODO: Xbox and BB versions of this (note: 3OE1:80301F9C => 59NL:0054B26C)

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

  .label    ddsfx_patch_loc, <VERS 0x80301600 0x803025CC 0x80303A1C 0x803037D0 0x80301F58 0x80301F9C 0x8030398C 0x80302D64>
  .data     ddsfx_patch_loc
  .data     ddsfx_code_end - ddsfx_code_start
  .address  ddsfx_patch_loc
ddsfx_code_start:
  b         +0x20
  subi      r3, r3, 0x57D0
  lwz       r0, [r13 - <VERS 0x4674 0x466C 0x464C 0x464C 0x465C 0x465C 0x463C 0x45FC>]
  cmpwi     r0, 35
  bne       ddsfx_skip
  subi      r3, r3, 0x04D8
ddsfx_skip:
  b         +0x8C
ddsfx_code_end:

  .data     <VERS 0x803016A0 0x8030266C 0x80303ABC 0x80303870 0x80301FF8 0x8030203C 0x80303A2C 0x80302E04>
  .data     0x00000004
  b         -0x9C



  # Weapon Attributes Patch (allows attributes to work on minibosses and Olga Flow)

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

  .label    wa_hook_call1, <VERS 0x800142DC 0x8001430C 0x800146A4 0x800142BC 0x800142F4 0x800142F4 0x800142BC 0x80014334>
  .label    wa_hook_call2, <VERS 0x80015D04 0x80015D34 0x80016174 0x80015CE4 0x80015D1C 0x80015D1C 0x80015CE4 0x80015D5C>
  .label    wa_hook_loc, 0x8000C8C0
  .data     wa_hook_loc
  .data     wa_hook_end - wa_hook_start
  .address  wa_hook_loc
wa_hook_start:
  andi.     r0, r0, 0x000F
wa_hook_entry2:
  andi.     r0, r0, 0x004F
  cmpwi     r0, 4
  blr
wa_hook_end:

  .data     wa_hook_call1
  .data     4
  .address  wa_hook_call1
  bl        wa_hook_start

  .data     wa_hook_call2
  .data     4
  .address  wa_hook_call2
  bl        wa_hook_entry2

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU

  .data     <VERS 0x002A5D53 0x002A6833 0x002A7D03 0x002A7793 0x002A7973 0x002A77B3 0x002A7A33>
  .data     (wa_patch1_end - wa_patch1_start)
  .assert   (wa_patch1_end - wa_patch1_start) == 0x19
  .address  <VERS 0x002A5D53 0x002A6833 0x002A7D03 0x002A7793 0x002A7973 0x002A77B3 0x002A7A33>
wa_patch1_start:
  and       eax, 0x0F
  mov       esi, <VERS 0x00481DB4 0x004825EC 0x004895A4 0x00489CEC 0x004895B4 0x00489D04 0x0048A0C4>
  fmul      st, float [esi]
  fild      st, dword [esp + 0x34]
  fmul      st, float [esi]
  lea       esi, [eax - 1]
  lea       eax, [ebx + 0x111]
wa_patch1_end:

  .data     <VERS 0x002A5030 0x002A5B10 0x002A6FE0 0x002A6A60 0x002A6C50 0x002A6A80 0x002A6D10>
  .data     (wa_patch2_end - wa_patch2_start)
  .assert   (wa_patch2_end - wa_patch2_start) == 0x0F
  .address  <VERS 0x002A5030 0x002A5B10 0x002A6FE0 0x002A6A60 0x002A6C50 0x002A6A80 0x002A6D10>
wa_patch2_start:
  test      byte [eax + 0x33], 0x02
  jnz       <VERS 0x002A50A0 0x002A5B80 0x002A7050 0x002A6AD0 0x002A6CC0 0x002A6AF0 0x002A6D80>
  mov       eax, [eax + 0x2E8]
  and       eax, 0x4F
wa_patch2_end:

  .versions 50YJ 59NL 59NL

  .data     <VERS 0x0076D164 0x0077422C 0x00773450>
  .data     (wa_patch1_end - wa_patch1_start)
  .assert   (wa_patch1_end - wa_patch1_start) == 3
wa_patch1_start:
  and       al, 0x0F
  inc edx
wa_patch1_end:

  .data     <VERS 0x0076F010 0x007760B8 0x007752DC>
  .data     (wa_patch2_end - wa_patch2_start)
  .assert   (wa_patch2_end - wa_patch2_start) == 0x10
  .address  <VERS 0x0076F010 0x007760B8 0x007752DC>
wa_patch2_start:
  test      byte [ecx + 0x33], 0x02
  jnz       <VERS 0x0076F076 0x0077611E 0x00775342>
  xor       eax, eax
  mov       al, [ecx + 0x2E8]  # class_flags
  and       al, 0x4F
wa_patch2_end:



  # Ruins Laser Fence SFX Bug Fix
  # TODO: Xbox and BB versions of this

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x80166324 0x801666D8 0x80166848 0x8016679C 0x801666E0 0x801666E0 0x80166800 0x80166CC4>
  .data     8
  lis       r3, 0x4005
  b         +0x9C
  .data     <VERS 0x801663C0 0x80166774 0x801668E4 0x80166838 0x8016677C 0x8016677C 0x8016689C 0x80166D60>
  .data     4
  b         +0x1C



  # SFX Cancellation Distance Bug Fix
  # TODO: Xbox and BB versions of this

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x805CB608 0x805D5C08 0x805DD0A8 0x805DCE48 0x805CBF10 0x805D2F30 0x805DC750 0x805D8990>
  .data     4
  .data     encode_float(22500)
  .data     <VERS 0x805CB8A8 0x805D5EA8 0x805DD348 0x805DD0E8 0x805CC1B0 0x805D31D0 0x805DC9F0 0x805D8C30>
  .data     4
  .data     encode_float(200)



  # Foie SFX Pitch Bug Fix

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x8022E2A8 0x8022EC44 0x8022FB30 0x8022F8E4 0x8022EB64 0x8022EB64 0x8022FC18 0x8022F4B0>
  .data     4
  li        r4, 0xFFFFFF00
  .data     <VERS 0x8022E2D8 0x8022EC74 0x8022FB60 0x8022F914 0x8022EB94 0x8022EB94 0x8022FC48 0x8022F4E0>
  .data     4
  li        r4, 0xFFFFFE80
  .data     <VERS 0x8022E308 0x8022ECA4 0x8022FB90 0x8022F944 0x8022EBC4 0x8022EBC4 0x8022FC78 0x8022F510>
  .data     4
  li        r4, 0xFFFFFDB0

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x00246EBD 0x002471FD 0x002474ED 0x0024728D 0x0024732D 0x002472AD 0x0024745D>
  .data     4
  .data     0xFFFFFF00
  .data     <VERS 0x00246EE3 0x00247223 0x00247513 0x002472B3 0x00247353 0x002472D3 0x00247483>
  .data     4
  .data     0xFFFFFE80
  .data     <VERS 0x00246F09 0x00247249 0x00247539 0x002472D9 0x00247379 0x002472F9 0x002474A9>
  .data     4
  .data     0xFFFFFDB0

  .versions 50YJ 59NJ 59NL
  .data     <VERS 0x006DD030 0x006E3400 0x006E33CC>
  .data     4
  .data     0xFFFFFF00
  .data     <VERS 0x006DCFD3 0x006E33A3 0x006E336F>
  .data     4
  .data     0xFFFFFE80
  .data     <VERS 0x006DD002 0x006E33D2 0x006E339E>
  .data     4
  .data     0xFFFFFDB0



  # Gifoie SFX Pitch Bug Fix

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x802300B8 0x80230A54 0x80231940 0x802316F4 0x80230974 0x80230974 0x80231A28 0x802312C0>
  .data     4
  li        r4, 0xFFFFFF00
  .data     <VERS 0x802300E8 0x80230A84 0x80231970 0x80231724 0x802309A4 0x802309A4 0x80231A58 0x802312F0>
  .data     4
  li        r4, 0xFFFFFE80
  .data     <VERS 0x80230118 0x80230AB4 0x802319A0 0x80231754 0x802309D4 0x802309D4 0x80231A88 0x80231320>
  .data     4
  li        r4, 0xFFFFFDB0

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x002479FE 0x00247D3E 0x0024802E 0x00247DCE 0x00247E6E 0x00247DEE 0x00247F9E>
  .data     4
  .data     0xFFFFFF00
  .data     <VERS 0x00247A25 0x00247D65 0x00248055 0x00247DF5 0x00247E95 0x00247E15 0x00247FCA>
  .data     4
  .data     0xFFFFFE80
  .data     <VERS 0x00247A4C 0x00247D8C 0x0024807C 0x00247E1C 0x00247EBC 0x00247E3C 0x00247FEC>
  .data     4
  .data     0xFFFFFDB0

  .versions 50YJ 59NJ 59NL
  .data     <VERS 0x006DE811 0x006E4BAD 0x006E4B79>
  .data     4
  .data     0xFFFFFF00
  .data     <VERS 0x006DE79F 0x006E4B82 0x006E4B4E>
  .data     4
  .data     0xFFFFFE80
  .data     <VERS 0x006DE7CF 0x006E4C0F 0x006E4BDB>
  .data     4
  .data     0xFFFFFDB0



  # Rafoie SFX Pitch Bug Fix

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
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

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x0024D9EA 0x0024DDAA 0x0024E0EA 0x0024DDBA 0x0024DEDA 0x0024DDDA 0x0024E00A>
  .data     4
  .data     0xFFFFFF00
  .data     <VERS 0x0024DA10 0x0024DDD0 0x0024E110 0x0024DDE0 0x0024DF00 0x0024DE00 0x0024E030>
  .data     4
  .data     0xFFFFFE80
  .data     <VERS 0x0024DA36 0x0024DDF6 0x0024E136 0x0024DE06 0x0024DF26 0x0024DE26 0x0024E056>
  .data     4
  .data     0xFFFFFDB0
  .data     <VERS 0x0024D1C3 0x0024D583 0x0024D8C3 0x0024D593 0x0024D6B3 0x0024D5B3 0x0024D7E3>
  .data     4
  .data     0xFFFFFF00
  .data     <VERS 0x0024D1DF 0x0024D59F 0x0024D8DF 0x0024D5AF 0x0024D6CF 0x0024D5CF 0x0024D7FF>
  .data     4
  .data     0xFFFFFE80
  .data     <VERS 0x0024D1FB 0x0024D5BB 0x0024D8FB 0x0024D5CB 0x0024D6EB 0x0024D5EB 0x0024D81B>
  .data     4
  .data     0xFFFFFDB0

  .versions 50YJ 59NJ 59NL
  .data     <VERS 0x006E74DD 0x006ED901 0x006ED82D>
  .data     4
  .data     0xFFFFFF00
  .data     <VERS 0x006E7484 0x006ED8A8 0x006ED7D4>
  .data     4
  .data     0xFFFFFE80
  .data     <VERS 0x006E74B3 0x006ED8D7 0x006ED803>
  .data     4
  .data     0xFFFFFDB0
  .data     <VERS 0x006E7B9B 0x006EDF82 0x006EDEAE>
  .data     4
  .data     0xFFFFFF00
  .data     <VERS 0x006E7B34 0x006EDF54 0x006EDE80>
  .data     4
  .data     0xFFFFFE80
  .data     <VERS 0x006E7B67 0x006EDFD0 0x006EDEFC>
  .data     4
  .data     0xFFFFFDB0



  # Barta SFX Pitch Bug Fix

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x80229B54 0x8022A4F0 0x8022B3E0 0x8022B190 0x8022A410 0x8022A410 0x8022B4C4 0x8022AD5C>
  .data     4
  li        r4, 0xFFFFFF00
  .data     <VERS 0x80229B84 0x8022A520 0x8022B410 0x8022B1C0 0x8022A440 0x8022A440 0x8022B4F4 0x8022AD8C>
  .data     4
  li        r4, 0xFFFFFE80
  .data     <VERS 0x80229BB4 0x8022A550 0x8022B440 0x8022B1F0 0x8022A470 0x8022A470 0x8022B524 0x8022ADBC>
  .data     4
  li        r4, 0xFFFFFDB0

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x0024393D 0x00243BBD 0x00243EED 0x00243C4D 0x00243D2D 0x00243C6D 0x00243DBD>
  .data     4
  .data     0xFFFFFF00
  .data     <VERS 0x00243963 0x00243BE3 0x00243F13 0x00243C73 0x00243D53 0x00243C93 0x00243DE3>
  .data     4
  .data     0xFFFFFE80
  .data     <VERS 0x00243989 0x00243C09 0x00243F39 0x00243C99 0x00243D79 0x00243CB9 0x00243E09>
  .data     4
  .data     0xFFFFFDB0

  .versions 50YJ 59NJ 59NL
  .data     <VERS 0x006D7BB8 0x006DDD90 0x006DDD5C>
  .data     4
  .data     0xFFFFFF00
  .data     <VERS 0x006D7B5F 0x006DDD37 0x006DDD03>
  .data     4
  .data     0xFFFFFE80
  .data     <VERS 0x006D7B8E 0x006DDD66 0x006DDD32>
  .data     4
  .data     0xFFFFFDB0



  # Gibarta SFX Pitch Bug Fix

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x8022EAB4 0x8022F450 0x80230340 0x802300F0 0x8022F370 0x8022F370 0x80230424 0x8022FCBC>
  .data     4
  li        r4, 0xFFFFFF00
  .data     <VERS 0x8022EAE4 0x8022F480 0x80230370 0x80230120 0x8022F3A0 0x8022F3A0 0x80230454 0x8022FCEC>
  .data     4
  li        r4, 0xFFFFFE80
  .data     <VERS 0x8022EB14 0x8022F4B0 0x802303A0 0x80230150 0x8022F3D0 0x8022F3D0 0x80230484 0x8022FD1C>
  .data     4
  li        r4, 0xFFFFFDB0

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x0024775D 0x00247A9D 0x00247D8D 0x00247B2D 0x00247BCD 0x00247B4D 0x00247CFD>
  .data     4
  .data     0xFFFFFF00
  .data     <VERS 0x00247783 0x00247AC3 0x00247DB3 0x00247B53 0x00247BF3 0x00247B73 0x00247D23>
  .data     4
  .data     0xFFFFFE80
  .data     <VERS 0x002477A9 0x00247AE9 0x00247DD9 0x00247B79 0x00247C19 0x00247B99 0x00247D49>
  .data     4
  .data     0xFFFFFDB0

  .versions 50YJ 59NJ 59NL
  .data     <VERS 0x006DDC6C 0x006E403C 0x006E4008>
  .data     4
  .data     0xFFFFFF00
  .data     <VERS 0x006DDC13 0x006E3FE3 0x006E3FAF>
  .data     4
  .data     0xFFFFFE80
  .data     <VERS 0x006DDC42 0x006E4012 0x006E3FDE>
  .data     4
  .data     0xFFFFFDB0



  # Rabarta SFX Pitch Bug Fix

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x80235DD4 0x80236790 0x8023767C 0x80237430 0x802366B0 0x802366B0 0x80237764 0x80236FFC>
  .data     4
  li        r4, 0xFFFFFF00
  .data     <VERS 0x80235E10 0x802367CC 0x802376B8 0x8023746C 0x802366EC 0x802366EC 0x802377A0 0x80237038>
  .data     4
  li        r4, 0xFFFFFE80
  .data     <VERS 0x80235E4C 0x80236808 0x802376F4 0x802374A8 0x80236728 0x80236728 0x802377DC 0x80237074>
  .data     4
  li        r4, 0xFFFFFDB0

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x0024CCC2 0x0024D082 0x0024D372 0x0024D092 0x0024D1B2 0x0024D0B2 0x0024D2E2>
  .data     4
  .data     0xFFFFFF00
  .data     <VERS 0x0024CCF4 0x0024D0B4 0x0024D3A4 0x0024D0C4 0x0024D1E4 0x0024D0E4 0x0024D314>
  .data     4
  .data     0xFFFFFE80
  .data     <VERS 0x0024CD27 0x0024D0E7 0x0024D3D7 0x0024D0F7 0x0024D217 0x0024D117 0x0024D347>
  .data     4
  .data     0xFFFFFDB0

  .versions 50YJ 59NJ 59NL
  .data     <VERS 0x006E6532 0x006EC956 0x006EC882>
  .data     4
  .data     0xFFFFFF00
  .data     <VERS 0x006E64BF 0x006EC8E3 0x006EC80F>
  .data     4
  .data     0xFFFFFE80
  .data     <VERS 0x006E64FB 0x006EC91F 0x006EC84B>
  .data     4
  .data     0xFFFFFDB0



  # Zonde SFX Pitch Bug Fix

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x8023B2C8 0x8023BC84 0x8023CB70 0x8023C924 0x8023BBA4 0x8023BBA4 0x8023CC58 0x8023C4F0>
  .data     4
  li        r4, 0xFFFFFF00
  .data     <VERS 0x8023B2F8 0x8023BCB4 0x8023CBA0 0x8023C954 0x8023BBD4 0x8023BBD4 0x8023CC88 0x8023C520>
  .data     4
  li        r4, 0xFFFFFE80
  .data     <VERS 0x8023B328 0x8023BCE4 0x8023CBD0 0x8023C984 0x8023BC04 0x8023BC04 0x8023CCB8 0x8023C550>
  .data     4
  li        r4, 0xFFFFFDB0

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x00250BEF 0x00250E9F 0x0025122F 0x00250F2F 0x0025106F 0x00250F4F 0x002510FF>
  .data     4
  .data     0xFFFFFF00
  .data     <VERS 0x00250C0E 0x00250EBE 0x0025124E 0x00250F4E 0x0025108E 0x00250F6E 0x0025111E>
  .data     4
  .data     0xFFFFFE80
  .data     <VERS 0x00250C2D 0x00250EDD 0x0025126D 0x00250F6D 0x002510AD 0x00250F8D 0x0025113D>
  .data     4
  .data     0xFFFFFDB0

  .versions 50YJ 59NJ 59NL
  .data     <VERS 0x006EC9A6 0x006F2E1D 0x006F2DE9>
  .data     4
  .data     0xFFFFFF00
  .data     <VERS 0x006EC933 0x006F2DE8 0x006F2DB4>
  .data     4
  .data     0xFFFFFE80
  .data     <VERS 0x006EC971 0x006F2E6A 0x006F2E36>
  .data     4
  .data     0xFFFFFDB0



  # Gizonde SFX Pitch Bug Fix

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x80230E08 0x802317C4 0x802326B0 0x80232464 0x802316E4 0x802316E4 0x80232798 0x80232030>
  .data     4
  li        r4, 0xFFFFFF00
  .data     <VERS 0x80230E38 0x802317F4 0x802326E0 0x80232494 0x80231714 0x80231714 0x802327C8 0x80232060>
  .data     4
  li        r4, 0xFFFFFE80
  .data     <VERS 0x80230E68 0x80231824 0x80232710 0x802324C4 0x80231744 0x80231744 0x802327F8 0x80232090>
  .data     4
  li        r4, 0xFFFFFDB0

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x0024839A 0x002486DA 0x002489CA 0x0024876A 0x0024880A 0x0024878A 0x0024893A>
  .data     4
  .data     0xFFFFFF00
  .data     <VERS 0x002483B6 0x002486F6 0x002489E6 0x00248786 0x00248826 0x002487A6 0x00248956>
  .data     4
  .data     0xFFFFFE80
  .data     <VERS 0x002483D2 0x00248712 0x00248A02 0x002487A2 0x00248842 0x002487C2 0x00248972>
  .data     4
  .data     0xFFFFFDB0

  .versions 50YJ 59NJ 59NL
  .data     <VERS 0x006DF355 0x006E56F5 0x006E56C1>
  .data     4
  .data     0xFFFFFF00
  .data     <VERS 0x006DF2EA 0x006E56C2 0x006E568E>
  .data     4
  .data     0xFFFFFE80
  .data     <VERS 0x006DF322 0x006E5756 0x006E5722>
  .data     4
  .data     0xFFFFFDB0



  # Razonde SFX Pitch Bug Fix

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x80237998 0x80238354 0x80239240 0x80238FF4 0x80238274 0x80238274 0x80239328 0x80238BC0>
  .data     4
  li        r4, 0xFFFFFF00
  .data     <VERS 0x802379C8 0x80238384 0x80239270 0x80239024 0x802382A4 0x802382A4 0x80239358 0x80238BF0>
  .data     4
  li        r4, 0xFFFFFE80
  .data     <VERS 0x802379F8 0x802383B4 0x802392A0 0x80239054 0x802382D4 0x802382D4 0x80239388 0x80238C20>
  .data     4
  li        r4, 0xFFFFFDB0

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x0024DDE2 0x0024E1A2 0x0024E4E2 0x0024E1B2 0x0024E2D2 0x0024E1D2 0x0024E402>
  .data     4
  .data     0xFFFFFF00
  .data     <VERS 0x0024DE01 0x0024E1C1 0x0024E501 0x0024E1D1 0x0024E2F1 0x0024E1F1 0x0024E421>
  .data     4
  .data     0xFFFFFE80
  .data     <VERS 0x0024DE20 0x0024E1E0 0x0024E520 0x0024E1F0 0x0024E310 0x0024E210 0x0024E440>
  .data     4
  .data     0xFFFFFDB0

  .versions 50YJ 59NJ 59NL
  .data     <VERS 0x006E867E 0x006EEA69 0x006EE996>
  .data     4
  .data     0xFFFFFF00
  .data     <VERS 0x006E8613 0x006EEA37 0x006EE963>
  .data     4
  .data     0xFFFFFE80
  .data     <VERS 0x006E864B 0x006EEACB 0x006EE9F7>
  .data     4
  .data     0xFFFFFDB0



  # Grants SFX Pitch Bug Fix

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x802316FC 0x802320B8 0x80232FA4 0x80232D58 0x80231FD8 0x80231FD8 0x8023308C 0x80232924>
  .data     4
  li        r4, 0xFFFFFF00
  .data     <VERS 0x80231734 0x802320F0 0x80232FDC 0x80232D90 0x80232010 0x80232010 0x802330C4 0x8023295C>
  .data     4
  li        r4, 0xFFFFFE80
  .data     <VERS 0x8023176C 0x80232128 0x80233014 0x80232DC8 0x80232048 0x80232048 0x802330FC 0x80232994>
  .data     4
  li        r4, 0xFFFFFDB0

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x00248FDB 0x0024931B 0x0024960B 0x002493AB 0x0024944B 0x002493CB 0x0024957B>
  .data     4
  .data     0xFFFFFF00
  .data     <VERS 0x00249008 0x00249348 0x00249638 0x002493D8 0x00249478 0x002493F8 0x002495A8>
  .data     4
  .data     0xFFFFFE80
  .data     <VERS 0x00249035 0x00249375 0x00249665 0x00249405 0x002494A5 0x00249425 0x002495D5>
  .data     4
  .data     0xFFFFFDB0

  .versions 50YJ 59NJ 59NL
  .data     <VERS 0x006E0E66 0x006E7209 0x006E71D5>
  .data     4
  .data     0xFFFFFF00
  .data     <VERS 0x006E0DF9 0x006E71D5 0x006E71A1>
  .data     4
  .data     0xFFFFFE80
  .data     <VERS 0x006E0E32 0x006E725C 0x006E7228>
  .data     4
  .data     0xFFFFFDB0



  # Megid SFX Pitch Bug Fix

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x802337A8 0x80234164 0x80235050 0x80234E04 0x80234084 0x80234084 0x80235138 0x802349D0>
  .data     4
  li        r4, 0xFFFFFF00
  .data     <VERS 0x802337D8 0x80234194 0x80235080 0x80234E34 0x802340B4 0x802340B4 0x80235168 0x80234A00>
  .data     4
  li        r4, 0xFFFFFE80
  .data     <VERS 0x80233808 0x802341C4 0x802350B0 0x80234E64 0x802340E4 0x802340E4 0x80235198 0x80234A30>
  .data     4
  li        r4, 0xFFFFFDB0

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x0024B4DD 0x0024B89D 0x0024BB8D 0x0024B8AD 0x0024B9CD 0x0024B8CD 0x0024BAFD>
  .data     4
  .data     0xFFFFFF00
  .data     <VERS 0x0024B503 0x0024B8C3 0x0024BBB3 0x0024B8D3 0x0024B9F3 0x0024B8F3 0x0024BB23>
  .data     4
  .data     0xFFFFFE80
  .data     <VERS 0x0024B529 0x0024B8E9 0x0024BBD9 0x0024B8F9 0x0024BA19 0x0024B919 0x0024BB49>
  .data     4
  .data     0xFFFFFDB0

  .versions 50YJ 59NJ 59NL
  .data     <VERS 0x006E3A80 0x006E9E98 0x006E9DC4>
  .data     4
  .data     0xFFFFFF00
  .data     <VERS 0x006E3A27 0x006E9E3F 0x006E9D6B>
  .data     4
  .data     0xFFFFFE80
  .data     <VERS 0x006E3A56 0x006E9E6E 0x006E9D9A>
  .data     4
  .data     0xFFFFFDB0



  # Anti SFX Pitch Bug Fix
  # TODO: Xbox and BB versions of this

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x80229354 0x80229CF0 0x8022ABDC 0x8022A990 0x80229C10 0x80229C10 0x8022ACC4 0x8022A55C>
  .data     4
  cmpwi     r0, 1



  # Shield DFP/EVP Bug Fix (allows shields to reach true max DFP/EVP values)
  # This bug was fixed on BB; no patch is needed for BB

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x801185B0 0x801187CC 0x8011885C 0x80118764 0x80118854 0x80118854 0x80118774 0x8011894C>
  .data     4
  lbz       r0, [r4 + 0x0016]
  .data     <VERS 0x801185BC 0x801187D8 0x80118868 0x80118770 0x80118860 0x80118860 0x80118780 0x80118958>
  .data     4
  lbz       r0, [r4 + 0x0017]

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x00185D8E 0x00185F4E 0x0018600E 0x00185F0E 0x00185F6E 0x00185F2E 0x00185F2E>
  .data     0x00000001
  .binary   16
  .data     <VERS 0x00185D97 0x00185F57 0x00186017 0x00185F17 0x00185F77 0x00185F37 0x00185F37>
  .data     0x00000001
  .binary   17



  # VR Spaceship Item Drop Bug Fix (allows items to drop from enemies above a certain Y position)

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x805C996C 0x805D3F6C 0x805DB40C 0x805DB1AC 0x805CA274 0x805D1294 0x805DAAB4 0x805D6CF4>
  .data     4
  .data     encode_float(220)

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x00175D75 0x00175E55 0x00175F35 0x00175EC5 0x00175ED5 0x00175EE5 0x00175E95>
  .data     4
  .data     encode_float(220)

  .versions 50YJ 59NJ 59NL
  .data     <VERS 0x005C859B 0x005C9827 0x005C9823>
  .data     4
  .data     encode_float(220)



  # Invalid Items Bug Fix
  # This bug was fixed on BB; no patch is needed for BB

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
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

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x001894EB 0x0018966B 0x001897AB 0x0018969B 0x001896DB 0x001896BB 0x0018969B>
  .data     (ii_mag_end - ii_mag_start)
  .assert   (ii_mag_end - ii_mag_start) <= 0x15
ii_mag_start:
  lea       ecx, [ecx + ecx * 2 - 3]
  lea       eax, [edx + ecx * 8]
  ret
ii_mag_end:
  .data     <VERS 0x00189460 0x001895E0 0x00189720 0x00189610 0x00189650 0x00189630 0x00189610>
  .data     (ii_unit_end - ii_unit_start)
  .assert   (ii_unit_end - ii_unit_start) <= 0x10
ii_unit_start:
  dec       eax
  shl       eax, 0x4
  add       eax, dword [ecx + 4]
  ret
ii_unit_end:
  .data     <VERS 0x0018943E 0x001895BE 0x001896FE 0x001895EE 0x0018962E 0x0018960E 0x001895EE>
  .data     (ii_armor_end - ii_armor_start)
  .assert   (ii_armor_end - ii_armor_start) <= 9
ii_armor_start:
  dec       eax
  imul      eax, eax, 0x1C
  add       eax, dword [ecx + 4]
  ret
ii_armor_end:



  # Item Removal Maxed Stats Bug Fix
  # TODO: Xbox and BB versions of this (if needed)

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

  .label    irms_hook1_call, <VERS 0x801B9A88 0x801B9EF4 0x801BCF6C 0x801B9FC0 0x801B9E74 0x801B9E74 0x801BA024 0x801BA4E0>
  .label    irms_hook1_ret, <VERS 0x801B9A8C 0x801B9EF8 0x801BCF70 0x801B9FC4 0x801B9E78 0x801B9E78 0x801BA028 0x801BA4E4>
  .label    irms_hook2_call, <VERS 0x8010B970 0x8010BB70 0x8010BC04 0x8010BAF0 0x8010BC14 0x8010BC14 0x8010BB00 0x8010BCF0>
  .label    irms_hook3_call, <VERS 0x8010DD98 0x8010DF98 0x8010E0E4 0x8010DF24 0x8010E03C 0x8010E03C 0x8010DF34 0x8010E118>
  .label    irms_hook3_ret, <VERS 0x8010DD9C 0x8010DF9C 0x8010E0E8 0x8010DF28 0x8010E040 0x8010E040 0x8010DF38 0x8010E11C>
  .label    irms_hook3_apply_bonuses, <VERS 0x8010B90C 0x8010BB0C 0x8010BBA0 0x8010BA8C 0x8010BBB0 0x8010BBB0 0x8010BA9C 0x8010BC8C>
  .label    irms_hooks_loc, 0x8000B088
  .data     irms_hooks_loc
  .data     irms_hooks_end - irms_hook1_start
  .address  irms_hooks_loc
irms_hook1_start:
  mr        r3, r29
  li        r4, 0x0000
  bl        <VERS 0x801B97B4 0x801B9C20 0x801BCC98 0x801B9CEC 0x801B9BA0 0x801B9BA0 0x801B9D50 0x801BA20C>
  mr        r3, r29
  b         irms_hook1_ret
irms_hook2_start:
  lbz       r0, [r31]
  cmplwi    r9, 1
  bnelr
  lbz       r0, [r31 + 1]
  addi      r31, r31, 2
  blr
irms_hook3_start:
  li        r9, 0x0000
  bl        irms_hook3_apply_bonuses
  mr        r3, r26
  mr        r4, r27
  mr        r5, r28
  mr        r6, r29
  mr        r7, r30
  mr        r8, r31
  li        r9, 1
  bl        irms_hook3_apply_bonuses
  b         irms_hook3_ret
irms_hooks_end:

  .data     irms_hook1_call
  .data     4
  .address  irms_hook1_call
  b         irms_hook1_start

  .data     irms_hook2_call
  .data     4
  .address  irms_hook2_call
  bl        irms_hook2_start

  .data     irms_hook3_call
  .data     4
  .address  irms_hook3_call
  b         irms_hook3_start

  .label    irms_hook4_loc, 0x8000C3F8
  .label    irms_hook4_call, <VERS 0x801B97B4 0x801B9C20 0x801BCC98 0x801B9CEC 0x801B9BA0 0x801B9BA0 0x801B9D50 0x801BA20C>
  .label    irms_hook4_ret, <VERS 0x801B97B8 0x801B9C24 0x801BCC9C 0x801B9CF0 0x801B9BA4 0x801B9BA4 0x801B9D54 0x801BA210>
  .label    TItemEquipBase_v16, <VERS 0x8010B094 0x8010B294 0x8010B390 0x8010B214 0x8010B338 0x8010B338 0x8010B224 0x8010B414>
  .label    TItemEquipBase_v17, <VERS 0x8010AF18 0x8010B118 0x8010B204 0x8010B098 0x8010B1BC 0x8010B1BC 0x8010B0A8 0x8010B298>
  .data     irms_hook4_loc
  .data     irms_hook4_end - irms_hook4_start
  .address  irms_hook4_loc
irms_hook4_start:
  cmplwi    r4, 0
  beqlr
  stwu      [r1 - 0x0010], r1
  b         irms_hook4_ret
irms_hook4_entry:
  stwu      [r1 - 0x20], r1
  mflr      r0
  stw       [r1 + 0x24], r0
  stmw      [r1 + 0x08], r26
  mr        r31, r3
  bl        irms_hook4_start
  li        r30, 0
  addi      r29, r31, 0x0D04
  lwz       r27, [r31 + 0x032C]
irms_hook4_again:
  lwz       r28, [r29]
  mr.       r3, r28
  beq       irms_hook4_skip
  li        r4, 1
  bl        TItemEquipBase_v17
  mr        r3, r28
  li        r4, 1
  bl        TItemEquipBase_v16
irms_hook4_skip:
  addi      r29, r29, 4
  addi      r30, r30, 1
  cmpwi     r30, 13
  blt       irms_hook4_again
  stw       [r31 + 0x032C], r27
  lmw       r26, [r1 + 0x08]
  lwz       r0, [r1 + 0x24]
  mtlr      r0
  addi      r1, r1, 0x0020
  blr
irms_hook4_end:

  .data     irms_hook4_call
  .data     4
  .address  irms_hook4_call
  b         irms_hook4_entry



  # Unit Present Bug Fix

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .label    up_hook_loc, 0x8000C640
  .label    up_hook_call, <VERS 0x80118CE0 0x80118EFC 0x80118FD8 0x80118E94 0x80118F84 0x80118F84 0x80118EA4 0x8011907C>
  .data     up_hook_loc
  .data     up_hook_end - up_hook_start
  .address  up_hook_loc
up_hook_start:
  rlwinm.   r0, r4, 0, 25, 25
  beq       up_skip_replace
  li        r4, 0x0000
up_skip_replace:
  addi      r0, r4, 0x0009
  blr
up_hook_end:
  .data     up_hook_call
  .data     4
  .address  up_hook_call
  bl        up_hook_start

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x0018656A 0x0018672A 0x0018681A 0x0018671A 0x0018674A 0x0018673A 0x0018670A>
  .data     (up_patch_end - up_patch_start)
  .assert   (up_patch_end - up_patch_start) == 0x14
up_patch_start:
  xor       ecx, ecx
  movzx     eax, byte [edi + 4]
  test      al, 0x40
  cmovnz    eax, ecx
  add       eax, 9
  mov       [esi + 0x170], al
up_patch_end:

  .versions 50YJ 59NJ 59NL
  .data     <VERS 0x005E0019 0x005E2E7D 0x005E2EA1>
  .data     (up_patch_end - up_patch_start)
  .assert   (up_patch_end - up_patch_start) == 0x2B
up_patch_start:
  xor       edx, edx
  movzx     eax, byte [edi + 4]
  test      al, 0x40
  cmovnz    eax, edx
  add       eax, 9
  push      eax
  call      [<VERS 0x00B038C4 0x00B0DA64 0x00B0FA64>]
  mov       ebx, [ebp - 0x24]
  mov       ecx, edi
  xor       eax, eax
  mov       [ebx + 0x1B4], ax
  dec       eax
  mov       [ebx + 0x1E0], ax
up_patch_end:



  # Bank Item Stacking Bug Fix
  # TODO: Xbox and BB versions of this (if needed)

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

  .label    bis_hook1_loc, 0x8000C6D0
  .label    bis_hook1_call, <VERS 0x8021D098 0x8021D9FC 0x8021E8E8 0x8021E69C 0x8021D91C 0x8021D91C 0x8021E9D0 0x8021E268>
  .label    bis_hook2_call, <VERS 0x80220528 0x80220EBC 0x80221DA8 0x80221B5C 0x80220DDC 0x80220DDC 0x80221E90 0x80221728>
  .data     bis_hook1_loc
  .data     bis_hooks_end - bis_hook1_start
  .address  bis_hook1_loc
bis_hook1_start:
  li        r0, 1
  stw       [r29 + 0x54], r0
  lwz       r3, [r29 + 0x24]
  blr
bis_hook2_start:
  li        r0, 1
  stw       [r31 + 0x0378], r0
  lwz       r3, [r31 + 0x24]
  blr
bis_hooks_end:

  .data     bis_hook1_call
  .data     4
  .address  bis_hook1_call
  bl        bis_hook1_start

  .data     bis_hook2_call
  .data     4
  .address  bis_hook2_call
  bl        bis_hook2_start



  # Dropped Mag Color Bug Fix
  # This bug was fixed after the Xbox beta; no patch is needed for other Xbox versions or for BB

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x80114378 0x8011458C 0x80114634 0x80114524 0x8011461C 0x8011461C 0x80114534 0x8011470C>
  .data     4
  li        r0, 0x12

  .versions 4OJB
  .data     0x001759E6
  .data     1
  .binary   12
  .data     0x00180898
  .data     1
  .binary   12



  # Meseta Drop System Bug Fix
  # This isn't needed on BB since drops are controlled by the server

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x80107478 0x80107654 0x80107708 0x801075D4 0x8010771C 0x8010771C 0x801075E4 0x801077D4>
  .data     4
  b         +0x0C
  .data     <VERS 0x8010748C 0x80107668 0x8010771C 0x801075E8 0x80107730 0x80107730 0x801075F8 0x801077E8>
  .data     4
  mr        r3, r0

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x00177428 0x00177508 0x00177618 0x00177578 0x001775B8 0x00177598 0x00177578>
  .data     (mds_patch_end - mds_patch_start)
  .assert   (mds_patch_end - mds_patch_start) == 0x16
mds_patch_start:
  cmp       cx, ax
  jb        mds_patch_end
  pop       edi
  ret       0x08
  .binary   CCCCCCCCCCCCCCCCCCCCCCCCCC
mds_patch_end:



  # Present Color Bug Fix
  # This was fixed after the first few versions of GC; it's not needed on XB or BB

  .versions 3OJ2 3OE0 3OE1
  .data     <VERS 0x80101C14 0x80101EB8 0x80101EB8>
  .data     4
  nop



  # Offline Quests Drop Table Bug Fix

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x80104B48 0x80104D24 0x80104DE0 0x80104CA4 0x80104DEC 0x80104DEC 0x80104CB4 0x80104EA4>
  .data     4
  beq       +0x0C

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x001763AD 0x0017648D 0x0017656D 0x001764FD 0x0017650D 0x0017651D 0x001764CD>
  .data     2
  .binary   7405



  # Mag Revival Priority Bug Fix
  # TODO: Write Xbox version
  # TODO: 3OE1:80112908 => 59NL:005D92AC

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .label    mrp_hook_loc, 0x8000C8A0
  .label    mrp_hook_call, <VERS 0x80112664 0x80112864 0x80112A3C 0x801127F0 0x80112908 0x80112908 0x80112800 0x801129E4>
  .data     mrp_hook_loc
  .data     mrp_hook_end - mrp_hook_start
  .address  mrp_hook_loc
mrp_hook_start:
  mulli     r0, r0, 10
  rlwinm.   r4, r31, 0, 30, 30
  beqlr
  extsh     r0, r29
  blr
mrp_hook_end:
  .data     mrp_hook_call
  .data     4
  .address  mrp_hook_call
  bl        mrp_hook_start



  # Mag Revival Challenge & Quest Mode Bug Fix

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x801CA1F4 0x801CA6E0 0x801CB5EC 0x801CA7AC 0x801CA610 0x801CA610 0x801CA810 0x801CACCC>
  .data     4
  b         +0x10

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x00209F7D 0x0020A0FD 0x0020A34D 0x0020A1AD 0x0020A17D 0x0020A1CD 0x0020A20D>
  .data     2
  .binary   EB0D

  .versions 50YJ 59NJ 59NL
  .data     <VERS 0x0069870B 0x0069E2F7 0x0069E277>
  .data     2
  .binary   EB04



  # Chat Bubble Window TAB Bug Fix

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x80250264 0x80250CB0 0x80251CA4 0x802519A4 0x80250AEC 0x80250AEC 0x80251C68 0x802514B0>
  .data     4
  nop

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x0025D1DE 0x0025D58E 0x0025D93E 0x0025D68E 0x0025D7AE 0x0025D6AE 0x0025D7DE>
  .data     2
  .binary   EB02

  .versions 50YJ 59NJ 59NL
  .data     <VERS 0x0071A4F6 0x00720AA4 0x007209E0>
  .data     2
  .binary   EB03



  # Chat Log Window LF/Tab Bug Fix

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x80267DDC 0x80268A88 0x80269AE4 0x80269898 0x80268788 0x80268788 0x80269B5C 0x802693A4>
  .data     4
  nop

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x00270A99 0x00270DB9 0x00271179 0x00270E99 0x00270F89 0x00270EB9 0x00270FF9>
  .data     2
  .binary   EB02

  .versions 50YJ 59NJ 59NL
  .data     <VERS 0x007344B6 0x0073B02A 0x0073AFC6>
  .data     2
  .binary   EB03



  # Dark/Hell Special GFX Bug Fix (makes Dark/Hell display graphic on success like in PSO BB)

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

  .label    dhsg_hook_loc, 0x8000E1E0
  .label    dhsg_hook_call1, <VERS 0x80355984 0x80356D88 0x803582E4 0x80358098 0x80356838 0x8035687C 0x80358464 0x80357858>
  .label    dhsg_hook_call2, <VERS 0x80355A04 0x80356E08 0x80358364 0x80358118 0x803568B8 0x803568FC 0x803584E4 0x803578D8>
  .data     dhsg_hook_loc
  .data     dhsg_hook_end - dhsg_hook_start
  .address  dhsg_hook_loc
dhsg_hook_start:
  mflr      r30
  li        r5, 0x0000  # flags
  li        r6, 0x001E  # duration
  li        r7, 0x0040  # priority
  # TParticleCtrl___new___from_definition_index_and_params
  bl        <VERS 0x800864A0 0x8008678C 0x80086904 0x800868B4 0x8008672C 0x8008672C 0x800868C4 0x8008688C>
  mtlr      r30
  blr
dhsg_hook_end:
  .data     dhsg_hook_call1
  .data     4
  .address  dhsg_hook_call1
  bl        dhsg_hook_start
  .data     dhsg_hook_call2
  .data     4
  .address  dhsg_hook_call2
  bl        dhsg_hook_start
  .data     <VERS 0x80355960 0x80356D64 0x803582C0 0x80358074 0x80356814 0x80356858 0x80358440 0x80357834>
  .data     4
  li        r4, 0x01E8
  .data     <VERS 0x803559F4 0x80356DF8 0x80358354 0x80358108 0x803568A8 0x803568EC 0x803584D4 0x803578C8>
  .data     4
  li        r4, 0x01E8

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  # [std](const Vector3F* position @ edi, int32_t definition_index @ eax) -> void
  .label    TParticleCtrl___new___from_definition_index, <VERS 0x000DFE90 0x000DFED0 0x000DFE80 0x000DFF10 0x000DFF70 0x000DFF30 0x000DFF70>
  # [std](const Vector3F* position @ [esp], int32_t definition_index @ eax, uint32_t flags @ [esp + 4], uint32_t duration @ ebx, uint32_t priority @ edi) -> void
  .label    TParticleCtrl___new___from_definition_index_and_params, <VERS 0x000E0590 0x000E05D0 0x000E0580 0x000E0610 0x000E0670 0x000E0630 0x000E0670>
  .data     <VERS 0x0017A4F0 0x0017A5E0 0x0017A6E0 0x0017A650 0x0017A680 0x0017A670 0x0017A640>
  .data     (dhsg_patch_end - dhsg_fn1_start)
  .assert   (dhsg_patch_end - dhsg_fn1_start) <= 0xC0
  .assert   dhsg_fn2_start == (dhsg_fn1_start + 0x50)
dhsg_fn1_start:
  push      edi
  push      ebx
  cmp       dword [esp + 0x18], 1
  ja        dhsg_ret
  je        dhsg_fn1_use_custom_pos
dhsg_fn1_use_source_pos:
  mov       edi, [esp + 0x0C]
  add       edi, 0x300
  push      0x60
  pop       ebx
  lea       eax, [ebx + 0x66]  # eax = 0xC6
  call      TParticleCtrl___new___from_definition_index
  lea       eax, [ebx + 0x69]  # eax = 0xC9
  call      TParticleCtrl___new___from_definition_index
  jmp       dhsg_ret
dhsg_fn1_use_custom_pos:
  mov       edi, [esp + 0x14]
  mov       eax, 0x1E8
  call      TParticleCtrl___new___from_definition_index
  mov       eax, 0x1C8  # definition_index
  push      0  # flags
  push      edi  # position
  push      0x1E
  pop       ebx  # duration
  push      0x40
  pop       edi  # priority
  call      TParticleCtrl___new___from_definition_index_and_params
  pop       edi  # Argument space cleanup
  pop       edi  # Argument space cleanup
dhsg_ret:
  pop       ebx
  pop       edi
  ret
dhsg_fn2_start:
  push      edi
  push      ebx
  cmp       dword [esp + 0x18], 1
  ja        dhsg_ret
  jne       dhsg_fn1_use_source_pos
dhsg_fn2_use_custom_pos:
  mov       edi, [esp + 0x14]
  push      esi
  mov       esi, 0x40000000
  mov       eax, 0x1E8
  call      TParticleCtrl___new___from_definition_index
  test      eax, eax
  jz        dhsg_fn2_skip_update_params1
  mov       [eax + 0x40], esi
  mov       [eax + 0x44], esi
dhsg_fn2_skip_update_params1:
  mov       eax, 0x1C8  # definition_index
  push      0  # flags
  push      edi  # position
  push      0x1E
  pop       ebx  # duration
  push      0x40
  pop       edi  # priority
  call      TParticleCtrl___new___from_definition_index_and_params
  pop       edi  # Argument space cleanup
  pop       edi  # Argument space cleanup
  test      eax, eax
  jz        dhsg_fn2_skip_update_params2
  mov       [eax + 0x40], esi
  mov       [eax + 0x44], esi
dhsg_fn2_skip_update_params2:
  pop       esi
  jmp       dhsg_ret
dhsg_patch_end:



  # Gol Dragon Camera Bug Fix (makes the camera after Gol Dragon display "normally")

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x802FB99C 0x802FC968 0x802FDE60 0x802FDB6C 0x802FC2F4 0x802FC338 0x802FDD28 0x802FD100>
  .data     4
  cmpwi     r3, 1

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x000A8AE1 0x000A8C51 0x000A8BD1 0x000A89C1 0x000A8961 0x000A89E1 0x000A8921>
  .data     2
  .binary   01

  .versions 50YJ 59NJ 59NL
  .data     <VERS 0x004CB84A 0x004CBE12 0x004CBD5A>
  .data     1
  .binary   01



  # Box/Fence Fadeout Bug Fix (stops boxes and other environmental objects fading in and out as you approach)

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x80189A54 0x80189E2C 0x80189F90 0x80189EF0 0x80189E20 0x80189E20 0x80189F54 0x8018A418>
  .data     4
  nop
  .data     <VERS 0x801933DC 0x801937B0 0x80193914 0x80193874 0x801937A8 0x801937A8 0x801938D8 0x80193D9C>
  .data     4
  nop

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x001D229B 0x001D244B 0x001D295B 0x001D241B 0x001D26AB 0x001D243B 0x001D26DB>
  .data     2
  .binary   9090
  .data     <VERS 0x001DF7C4 0x001DF924 0x001DFD94 0x001DF964 0x001DFB04 0x001DF984 0x001DFA74>
  .data     2
  .binary   EB04

  .versions 50YJ 59NJ 59NL
  .data     <VERS 0x00648420 0x0064D838 0x0064D864>
  .data     1
  .binary   EB
  .data     <VERS 0x0065944F 0x0065E8E3 0x0065E8D3>
  .data     1
  .binary   EB



  # TP Bar Color Bug Fix
  # TODO: Write BB version of this patch

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
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

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x002779CE 0x00277C7E 0x0027808E 0x00277DAE 0x00277ECE 0x00277DCE 0x00277F9E>
  .data     0x00000004
  .data     0xFF00AAFA
  .data     <VERS 0x002779DE 0x00277C8E 0x0027809E 0x00277DBE 0x00277EDE 0x00277DDE 0x00277FAE>
  .data     0x00000004
  .data     0xFF00AAFA
  .data     <VERS 0x00277A24 0x00277CD4 0x002780E4 0x00277E04 0x00277F24 0x00277E24 0x00277FF4>
  .data     0x00000004
  .data     0xFF00AAFA
  .data     <VERS 0x0054543C 0x00545ACC 0x0054D5B4 0x0054AA34 0x0054A2D4 0x0054AA34 0x0054ADD4>
  .data     0x00000004
  .data     0xFF0074EE



  # Devil's and Demon's Special Damage Display Bug Fix
  # TODO: Are Xbox/BB versions of this patch needed?

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x8001306C 0x8001309C 0x80013364 0x8001304C 0x80013084 0x80013084 0x8001304C 0x800130C4>
  .data     4
  b         -0x0340



  # Christmas Trees Bug Fix
  # TODO: Xbox and BB versions

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

  .label    ct_hook_loc, 0x8000B5C8
  .label    ct_hook_call, <VERS 0x80183E94 0x8018425C 0x801843C0 0x80184320 0x80184250 0x80184250 0x80184384 0x80184848>
  .label    ct_hook_ret, <VERS 0x80183E98 0x80184260 0x801843C4 0x80184324 0x80184254 0x80184254 0x80184388 0x8018484C>
  .data     ct_hook_loc
  .data     ct_hook_end - ct_hook_start
  .address  ct_hook_loc
ct_hook_start:
  lwz       r3, [r3 + 0x98]
  bl        <VERS 0x803DFCC0 0x803E269C 0x803E453C 0x803E42EC 0x803E0F64 0x803E0FBC 0x803E46BC 0x803E31AC>
  lwz       r3, [r31 + 0x042C]
  lwz       r4, [r31 + 0x0430]
  b         ct_hook_ret
ct_hook_end:

  .data     ct_hook_call
  .data     4
  .address  ct_hook_call
  b         ct_hook_start

  .data     <VERS 0x80183ED4 0x8018429C 0x80184400 0x80184360 0x80184290 0x80184290 0x801843C4 0x80184888>
  .data     4
  nop



  # Rain Drops Color Bug Fix
  # TODO: BB version

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x804B3738 0x804B6E58 0x804B92F8 0x804B90B8 0x804B3EF0 0x804B43D0 0x804B8990 0x804B8E10>
  .data     8
  .data     0x70808080
  .data     0x60707070

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x0054D670 0x0054DD00 0x005557E8 0x00552C68 0x00552508 0x00552C68 0x00553008>
  .data     0x00000008
  .binary   7080808060707070



  # Reverser Target Lock Bug Fix
  # TODO: Xbox and BB versions

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x801C5EA4 0x801C6360 0x801C6604 0x801C642C 0x801C62C0 0x801C62C0 0x801C6490 0x801C694C>
  .data     4
  addi      r4, r31, 0x02FC



  # Deband/Shifta/Resta Target Bug Fix
  # TODO: Xbox and BB versions

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x8022CF84 0x8022D920 0x8022E85C 0x8022E5C0 0x8022D840 0x8022D840 0x8022E8F4 0x8022E18C>
  .data     4
  bgt       +0x0630

  .versions 3OJ2 3OE0 3OE1
  .data     <VERS 0x8022D278 0x8022DB34 0x8022DB34>
  .data     4
  bgt       +0x033C
  .data     <VERS 0x8022D36C 0x8022DC28 0x8022DC28>
  .data     4
  bgt       +0x0248



  # Tech Auto Targeting Bug Fix
  # TODO: Xbox and BB versions

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
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
  # TODO: Are Xbox and BB versions needed?

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

  .label    ta_hook_loc, 0x8000BBD0
  .label    ta_hook_call, <VERS 0x80170C54 0x80171008 0x80171260 0x801710CC 0x80171010 0x80171010 0x80171130 0x801715F4>
  .data     ta_hook_loc
  .data     ta_hook_end - ta_hook_start
  .address  ta_hook_loc
ta_hook_start:
  lwz       r4, [r31 + 0x0370]
  subi      r4, r4, 0x0400
  stw       [r31 + 0x0370], r4
  lwz       r3, [r31 + 0x14]
  cmplwi    r3, 0
  beqlr
  stw       [r3 + 0x0060], r4
  blr
ta_hook_end:

  .data     ta_hook_call
  .data     4
  .address  ta_hook_call
  bl        ta_hook_start

  .data     <VERS 0x80170C74 0x80171028 0x80171280 0x801710EC 0x80171030 0x80171030 0x80171150 0x80171614>
  .data     4
  ori       r0, r4, 0x0420



  # Belra arm bug fix (this part by fuzziqersoftware)
  # This was fixed after the first few versions of PSO GC; no patch is needed for Xbox or BB

  .versions 3OJ2 3OE0 3OE1

  .label    dba_hook1_call, <VERS 0x80095724 0x800959B0 0x800959B0>
  .label    dba_hook2_call, <VERS 0x80095734 0x800959C0 0x800959C0>
  .label    dba_hook_loc, 0x8000B06C
  .data     dba_hook_loc
  .data     dba_hook_end - dba_hook1_start
  .address  dba_hook_loc
dba_hook1_start:
  li        r0, 1
  stw       [r13 - <VERS 0x2E48 0x2E30 0x2E30>], r0  # Anchor: 80039388 @ 3OE1
  b         <VERS 0x803D3140 0x803D4410 0x803D4468>
dba_hook2_start:
  li        r4, 0
  stw       [r13 - <VERS 0x2E48 0x2E30 0x2E30>], r4
  lwz       r4, [r28 + 0x04]
  blr
dba_hook_end:

  .data     dba_hook1_call
  .data     4
  .address  dba_hook1_call
  bl        dba_hook1_start

  .data     dba_hook2_call
  .data     4
  .address  dba_hook2_call
  bl        dba_hook2_start



  # Tsumikiri J-Sword special attack + rapid weapon switch bug fix (this part by fuzziqersoftware)
  # TODO: Xbox and BB versions

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

  .label    tjs_switch_fix_hook_call, <VERS 0x8034CFA8 0x8034E3AC 0x8034F908 0x8034F6BC 0x8034DE5C 0x8034DEA0 0x8034FA88 0x8034EE7C>
  .label    tjs_switch_fix_hook_loc, 0x8000B050
  .data     tjs_switch_fix_hook_loc
  .data     tjs_switch_fix_hook_end - tjs_switch_fix_hook_start
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



  # Battle param reload bug fix (this part by fuzziqersoftware)
  # This was fixed after the first few versions of PSO GC; no patch is needed for Xbox or BB

  .versions 3OJ2 3OE0 3OE1
  .label    end_loading_screen, <VERS 0x8001C6D0 0x8001C8F0 0x8001C8F0>
  .label    load_battle_params, <VERS 0x8001DA48 0x8001DC68 0x8001DC68>
  .label    bp_reload_hook_loc, 0x8000E1BC
  .label    bp_reload_hook_call, <VERS 0x801A3A70 0x801A3E4C 0x801A3E4C>
  .data     bp_reload_hook_loc
  .data     bp_reload_hook_end - bp_reload_hook_start
  .address  bp_reload_hook_loc
bp_reload_hook_start:
  mflr      r0
  stwu      [r1 - 0x20], r1
  stw       [r1 + 0x24], r0
  bl        end_loading_screen
  bl        load_battle_params
  lwz       r0, [r1 + 0x24]
  addi      r1, r1, 0x20
  mtlr      r0
  blr
bp_reload_hook_end:
  .data     bp_reload_hook_call
  .data     4
  .address  bp_reload_hook_call
  bl        bp_reload_hook_start



  .all_versions
  .data     0
  .data     0
