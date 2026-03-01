.meta name="Bug fixes"
.meta description="Fixes many minor\ngameplay, sound,\nand graphical bugs"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049
# Xbox port by fuzziqersoftware

# This patch is a collection of many smaller patches, most of which are not yet ported.

.versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksXB



  # Tiny Grass Assassins Bug Fix

  .data     <VERS 0x0016227A 0x0016238A 0x0016232A 0x0016240A 0x0016229A 0x0016242A 0x0016225A>
  .data     0x00000002
  .binary   EB0E



  # Shield DFP/EVP Bug Fix (allows shields to reach true max DFP/EVP values)

  .data     <VERS 0x00185D8E 0x00185F4E 0x0018600E 0x00185F0E 0x00185F6E 0x00185F2E 0x00185F2E>
  .data     0x00000001
  .binary   16
  .data     <VERS 0x00185D97 0x00185F57 0x00186017 0x00185F17 0x00185F77 0x00185F37 0x00185F37>
  .data     0x00000001
  .binary   17



  # VR Spaceship Item Drop Bug Fix (allows items to drop from enemies above a certain Y position)

  .data     <VERS 0x00175D75 0x00175E55 0x00175F35 0x00175EC5 0x00175ED5 0x00175EE5 0x00175E95>
  .data     0x00000002
  .data     0x435C0000



  # Gol Dragon Camera Bug Fix (makes the camera after Gol Dragon display "normally")

  .data     <VERS 0x000A8AE1 0x000A8C51 0x000A8BD1 0x000A89C1 0x000A8961 0x000A89E1 0x000A8921>
  .data     0x00000002
  .binary   01



  # Rain Drops Color Bug Fix

  .data     <VERS 0x0054D670 0x0054DD00 0x005557E8 0x00552C68 0x00552508 0x00552C68 0x00553008>
  .data     0x00000008
  .binary   7080808060707070



  # TP Bar Color Bug Fix

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



  # Olga Flow Barta Bug Fix

  .label    g1_hook_call, <VERS 0x000970E0 0x000973F0 0x00097460 0x00097140 0x000970E0 0x00097160 0x00096FE0>
  .label    g1_hook_loc, <VERS 0x00097124 0x00097434 0x000974A4 0x00097184 0x00097124 0x000971A4 0x00097024>
  .data     g1_hook_call
  .data     6
  .address  g1_hook_call
  mov       eax, esi
  cmp       al, 19
  jmp       g1_hook_loc
g1_hook_call_end:
  .data     g1_hook_loc
  .deltaof  g1_hook_start, g1_hook_end
  .address  g1_hook_loc
g1_hook_start:
  jne       g1_hook_skip_replace_value
  mov       al, 2
g1_hook_skip_replace_value:
  cmp       eax, [ebx + 0x440]  // Original opcode
  jmp       g1_hook_call_end
g1_hook_end:



  # Morfos Frozen Player Bug Fix

  .label    g2_hook_call, <VERS 0x0012E257 0x0012E387 0x0012E4E7 0x0012E537 0x0012E567 0x0012E557 0x0012E5A7>
  .label    g2_hook_loc1, <VERS 0x0012E5F4 0x0012E724 0x0012E884 0x0012E8D4 0x0012E904 0x0012E8F4 0x0012E944>
  .label    g2_hook_loc2, <VERS 0x0012E622 0x0012E752 0x0012E8B2 0x0012E902 0x0012E932 0x0012E922 0x0012E972>
  .data     g2_hook_call
  .data     6
  .address  g2_hook_call
  call      g2_hook_loc1
  nop
  .data     g2_hook_loc1
  .deltaof  g2_hook_start1, g2_hook_end1
  .address  g2_hook_loc1
g2_hook_start1:
  fld1      st0  // st = [1.0, speed]
  fld1      st0  // st = [1.0, 1.0, speed]
  fadd      st0, st1  // st = [2.0, 1.0, speed]
  fdivp     st1, st0  // st = [0.5, speed]
  jmp       g2_hook_loc2
g2_hook_end1:

  .data     g2_hook_loc2
  .deltaof  g2_hook_start2, g2_hook_end2
  .address  g2_hook_loc2
g2_hook_start2:
  test      byte [esi + 0x30], 0x20  // If not set, use 1.5; if set, use 0.5
  jnz       g2_hook_entity_is_frozen
  fld1      st0  // st = [1, 0.5, speed]
  faddp     st1, st0  // st = [1.5, speed]
g2_hook_entity_is_frozen:
  fmulp     st1, st0  // st = [((game_flags & 0x20) ? 0.5 : 1.5) * speed]
  ret
g2_hook_end2:



  # Dropped Mag Color Bug Fix (only needed on beta version)

  .only_versions 4OJB
  .data     0x001759E6
  .data     1
  .binary   12
  .data     0x00180898
  .data     1
  .binary   12
  .all_versions



  # Box/Fence Fadeout Bug Fix

  .data     <VERS 0x001D229B 0x001D244B 0x001D295B 0x001D241B 0x001D26AB 0x001D243B 0x001D26DB>
  .data     2
  nop
  nop

  .data     <VERS 0x001DF7C4 0x001DF924 0x001DFD94 0x001DF964 0x001DFB04 0x001DF984 0x001DFA74>
  .data     6
  nop
  nop
  nop
  nop
  nop
  nop



  # TODO: Port the rest of the patches in the GC version of BugFixes:

  # Bulclaw HP Bug Fix
  # Weapon Attributes Patch
  # Invalid Items Bug Fix
  # Item Removal Maxed Stats Bug Fix
  # Unit Present Bug Fix
  # Bank Item Stacking Bug Fix
  # Meseta Drop System Bug Fix
  # Offline Quests Drop Table Bug Fix
  # Mag Revival Priority Bug Fix
  # Mag Revival Challenge & Quest Mode Bug Fix
  # Reverser Target Lock Bug Fix
  # Deband/Shifta/Resta Target Bug Fix
  # Tech Auto Targeting Bug Fix
  # Enable Trap Animations
  # Tsumikiri J-Sword special attack + rapid weapon switch bug fix

  # Control Tower: Delbiter Death SFX Bug Fix
  # Ruins Laser Fence SFX Bug Fix
  # SFX Cancellation Distance Bug Fix
  # Foie SFX Pitch Bug Fix
  # Gifoie SFX Pitch Bug Fix
  # Rafoie SFX Pitch Bug Fix
  # Barta SFX Pitch Bug Fix
  # Gibarta SFX Pitch Bug Fix
  # Rabarta SFX Pitch Bug Fix
  # Zonde SFX Pitch Bug Fix
  # Gizonde SFX Pitch Bug Fix
  # Razonde SFX Pitch Bug Fix
  # Grants SFX Pitch Bug Fix
  # Megid SFX Pitch Bug Fix
  # Anti SFX Pitch Bug Fix
  # Present Color Bug Fix
  # Chat Bubble Window TAB Bug Fix
  # Chat Log Window LF/Tab Bug Fix
  # Dark/Hell Special GFX Bug Fix
  # Devil's and Demon's Special Damage Display Bug Fix
  # Christmas Trees Bug Fix


  .data     0x00000000
  .data     0x00000000
