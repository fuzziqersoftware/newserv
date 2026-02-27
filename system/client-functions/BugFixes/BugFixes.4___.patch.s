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



  # TODO: Port the rest of the patches in the GC version of BugFixes:

  # Olga Flow Barta Bug Fix
  # Morfos Frozen Player Bug Fix
  # Bulclaw HP Bug Fix
  # Control Tower: Delbiter Death SFX Bug Fix
  # Weapon Attributes Patch
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
  # Invalid Items Bug Fix
  # Item Removal Maxed Stats Bug Fix
  # Unit Present Bug Fix
  # Bank Item Stacking Bug Fix
  # Dropped Mag Color Bug Fix
  # Meseta Drop System Bug Fix
  # Present Color Bug Fix
  # Offline Quests Drop Table Bug Fix
  # Mag Revival Priority Bug Fix
  # Mag Revival Challenge & Quest Mode Bug Fix
  # Chat Bubble Window TAB Bug Fix
  # Chat Log Window LF/Tab Bug Fix
  # Dark/Hell Special GFX Bug Fix
  # Box/Fence Fadeout Bug Fix
  # Devil's and Demon's Special Damage Display Bug Fix
  # Christmas Trees Bug Fix
  # Reverser Target Lock Bug Fix
  # Deband/Shifta/Resta Target Bug Fix
  # Tech Auto Targeting Bug Fix
  # Enable Trap Animations
  # Tsumikiri J-Sword special attack + rapid weapon switch bug fix



  .data     0x00000000
  .data     0x00000000
