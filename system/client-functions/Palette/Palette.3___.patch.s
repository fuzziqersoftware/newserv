.meta name="Palette"
.meta description="Use C stick to\nuse 4 customize\nconfigurations\ninstead of just one"
# Original codes by Ralf @ GC-Forever
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

.versions 3OE0 3OE1 3OE2 3OJ2 3OJ3 3OJ4 3OJ5 3OP0

# This code will let you have up to four different palettes of action buttons.
# Battle Screen Control Keys
#   C-Stick/D-Pad Left  = Select Palette 1
#   C-Stick/D-Pad Down  = Select Palette 2
#   C-Stick/D-Pad Right = Select Palette 3
#   C-Stick/D-Pad Up    = Select Palette 4
# Customize Menu Control Keys
#   C-Stick/D-Pad Left  = Load Palette 1 As Active Button Selection
#   C-Stick/D-Pad Down  = Load Palette 2 As Active Button Selection
#   C-Stick/D-Pad Right = Load Palette 3 As Active Button Selection
#   C-Stick/D-Pad Up    = Load Palette 4 As Active Button Selection
# Hold L+R and press ...
#   C-Stick/D-Pad Left  = Save Active Button Selection As Palette 1
#   C-Stick/D-Pad Down  = Save Active Button Selection As Palette 2
#   C-Stick/D-Pad Right = Save Active Button Selection As Palette 3
#   C-Stick/D-Pad Up    = Save Active Button Selection As Palette 4

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC

  .label    memcpy, 0x8000E41C
  .label    memset, 0x8000E334
  .label    get_main_phase, <VERS 0x8000F948 0x8000F948 0x8000F8FC 0x8000F948 0x8000F94C 0x8000F94C 0x8000F8FC 0x8000F970>

  .data     0x8000B958  # Save Extra Palettes To Memory Card (Temp Slot 3)
  .deltaof  save_to_memcard_hook1, save_to_memcard_end
  .address  0x8000B958
save_to_memcard_hook1:
  stw       [r13 - <VERS 0x46AC 0x46AC 0x468C 0x46C4 0x46BC 0x469C 0x469C 0x464C>], r3
  mulli     r3, r3, 60
  lwz       r4, [r13 - <VERS 0x46C8 0x46C8 0x46A8 0x46E0 0x46D8 0x46B8 0x46B8 0x4668>]
  addis     r4, r4, 0x0001
  addi      r4, r4, 0x0B80
  add       r4, r4, r3
  lis       r3, 0x8000
  ori       r3, r3, 0xCF40
  li        r5, 0x003C
  b         memcpy
save_to_memcard_hook2:
  stw       [r4], r3
  lwz       r3, [r13 - <VERS 0x46AC 0x46AC 0x468C 0x46C4 0x46BC 0x469C 0x469C 0x464C>]
  mulli     r3, r3, 60
  lwz       r4, [r13 - <VERS 0x46C8 0x46C8 0x46A8 0x46E0 0x46D8 0x46B8 0x46B8 0x4668>]
  addis     r4, r4, 0x0001
  addi      r4, r4, 0x0B80
  add       r3, r4, r3
  lis       r4, 0x8000
  ori       r4, r4, 0xCF40
  li        r5, 0x003C
  b         memcpy
save_to_memcard_hook3:
  lwz       r3, [r13 - <VERS 0x46AC 0x46AC 0x468C 0x46C4 0x46BC 0x469C 0x469C 0x464C>]
  mulli     r3, r3, 60
  lwz       r4, [r13 - <VERS 0x46C8 0x46C8 0x46A8 0x46E0 0x46D8 0x46B8 0x46B8 0x4668>]
  addis     r4, r4, 0x0001
  addi      r4, r4, 0x0B80
  add       r3, r4, r3
  li        r4, 0x0000
  li        r5, 0x003C
  bl        memset
  bl        get_main_phase
  b         [<VERS 801FF034 801FF034 801FF97C 801FE838 801FF0AC 80200044 801FF648 801FF918>]
save_to_memcard_end:

  .data     0x8000CA40  # Full Action List (Incl. Photon Blasts & Traps)
  .deltaof  full_action_list_hook1, full_action_list_end
  .address  0x8000CA40
full_action_list_hook1:
  cmplwi    r3, 0
  bne       full_action_list_hook1_r3_nonzero
  li        r31, 0x0000
full_action_list_hook1_r3_nonzero:
  cmp       r0, r31
  blr
full_action_list_hook2:
  li        r0, 0x0003
  mtctr     r0
  ori       r4, r30, 0x0500
  addi      r5, r31, 0x0538
full_action_list_hook2_next:
  lhzu      r0, [r5 + 0x0004]
  cmp       r4, r0
  beq       full_action_list_hook2_ret
  bdnz      full_action_list_hook2_next
  li        r3, 0x0000
full_action_list_hook2_ret:
  cmpwi     r3, 0
  blr
full_action_list_end:

  .data     0x8000CD00  # Have Four Action Button Palettes
  .deltaof  four_palettes_hook1, four_palettes_end
  .address  0x8000CD00
four_palettes_hook1:
  lis       r4, 0x8000
  ori       r4, r4, 0xCF3E
  li        r31, 0x0000
  lhz       r6, [r4 + 0x003A]
  cmpwi     r6, 0
  beqlr
  sth       [r4 + 0x003A], r31
  lis       r3, 0x8051
  # D-pad version: lhz       r0, [r3 - <VERS 0x6C4A 0x676A 0x1D8A 0x752A 0x3A6A 0x142A 0x168A 0x0D6A>]
  lhz       r0, [r3 - <VERS 0x6C4C 0x676C 0x1D8C 0x752C 0x3A6C 0x142C 0x168C 0x0D6C>]
  lhz       r5, [r3 - <VERS 0x6C50 0x6770 0x1D90 0x7530 0x3A70 0x1430 0x1690 0x0D70>]
  and       r5, r5, r6
  andi.     r0, r0, 0x3C00  # D-pad version: andi.     r0, r0, 0x00F0
  beqlr
  rlwinm.   r3, r0, 0, 21, 21  # D-pad version: rlwinm.   r3, r0, 0, 27, 27
  beq       four_palettes_hook1_control_check1
  li        r30, 0x002A
four_palettes_hook1_control_check1:
  rlwinm.   r3, r0, 0, 18, 18  # D-pad version: rlwinm.   r3, r0, 0, 24, 24
  beq       four_palettes_hook1_control_check2
  li        r30, 0x001C
four_palettes_hook1_control_check2:
  rlwinm.   r3, r0, 0, 20, 20  # D-pad version: rlwinm.   r3, r0, 0, 26, 26
  beq       four_palettes_hook1_control_check3
  li        r30, 0x000E
four_palettes_hook1_control_check3:
  add       r4, r4, r30
  li        r0, 0x0007
  mtctr     r0
  addi      r3, r28, 0x0504
four_palettes_hook1_again:
  cmpwi     r5, 3
  bne       four_palettes_hook1_skip
  lhz       r0, [r3 + 0x0004]
  sth       [r4 + 0x0002], r0
four_palettes_hook1_skip:
  lhzu      r0, [r4 + 0x0002]
  sthu      [r3 + 0x0004], r0
  bdnz      four_palettes_hook1_again
  li        r30, 0x0000
  blr
four_palettes_hook2:
  li        r3, 0x0003
  lis       r4, 0x8001
  sth       [r4 - 0x3088], r3
  mr        r3, r30
  blr
four_palettes_hook3:
  lis       r12, 0x8044
  ori       r12, r12, <VERS 0xA858 0xACD8 0xE5D0 0x9AB8 0xC8D8 0xE940 0xE708 0xE3D0>
  lwz       r4, [r3]
  cmp       r4, r12
  rlwinm    r3, r0, 0, 29, 29  # Original opcode
  bnelr
  cmpwi     r0, 0
  bnelr
  li        r4, 0x0001
  lis       r12, 0x8001
  sth       [r12 - 0x3088], r4
  blr
four_palettes_hook4:
  lis       r3, 0x8000
  ori       r3, r3, 0xCF3C
  li        r0, 0x000E
  mtctr     r0
  li        r0, 0
four_palettes_hook4_again:
  stwu      [r3 + 4], r0
  bdnz      four_palettes_hook4_again
  blr
four_palettes_end:

  # Disable Photon Blast Palette Switching
  .data     <VERS 0x801B59E4 0x801B59E4 0x801B5B7C 0x801B55F8 0x801B5A4C 0x801B7BB8 0x801B5B18 0x801B6038>
  .data     0x00000004
  .address  <VERS 0x801B59E4 0x801B59E4 0x801B5B7C 0x801B55F8 0x801B5A4C 0x801B7BB8 0x801B5B18 0x801B6038>
  li        r3, 0x0000

  # Full Action List (Incl. Photon Blasts & Traps)
  .data     <VERS 0x801D8230 0x801D8230 0x801D8430 0x801D7DF8 0x801D8300 0x801D84BC 0x801D83CC 0x801D88EC>
  .data     0x00000004
  .address  <VERS 0x801D8230 0x801D8230 0x801D8430 0x801D7DF8 0x801D8300 0x801D84BC 0x801D83CC 0x801D88EC>
  bl        full_action_list_hook1
  .data     <VERS 0x801CC038 0x801CC038 0x801CC238 0x801CBC1C 0x801CC108 0x801CD5FC 0x801CC1D4 0x801CC6F4>
  .data     0x00000004
  .address  <VERS 0x801CC038 0x801CC038 0x801CC238 0x801CBC1C 0x801CC108 0x801CD5FC 0x801CC1D4 0x801CC6F4>
  bl        full_action_list_hook2

  # Save Extra Palettes To Memory Card (Temp Slot 3)
  .data     <VERS 0x801FC2D8 0x801FC2D8 0x801FCB58 0x801FBC74 0x801FC380 0x801FD268 0x801FC8EC 0x801FCA54>
  .data     0x00000004
  .address  <VERS 0x801FC2D8 0x801FC2D8 0x801FCB58 0x801FBC74 0x801FC380 0x801FD268 0x801FC8EC 0x801FCA54>
  bl        save_to_memcard_hook1
  .data     <VERS 0x801FFB14 0x801FFB14 0x8020048C 0x801FF318 0x801FFB8C 0x80200B88 0x80200158 0x802003F8>
  .data     0x00000004
  .address  <VERS 0x801FFB14 0x801FFB14 0x8020048C 0x801FF318 0x801FFB8C 0x80200B88 0x80200158 0x802003F8>
  bl        save_to_memcard_hook2
  .data     <VERS 0x801FF030 0x801FF030 0x801FF978 0x801FE834 0x801FF0A8 0x80200040 0x801FF644 0x801FF914>
  .data     0x00000004
  .address  <VERS 0x801FF030 0x801FF030 0x801FF978 0x801FE834 0x801FF0A8 0x80200040 0x801FF644 0x801FF914>
  b         save_to_memcard_hook3

  # Have Four Action Button Palettes
  .data     <VERS 0x801D7A78 0x801D7A78 0x801D7C78 0x801D7640 0x801D7B48 0x801D7CC4 0x801D7C14 0x801D8134>
  .data     0x00000004
  .address  <VERS 0x801D7A78 0x801D7A78 0x801D7C78 0x801D7640 0x801D7B48 0x801D7CC4 0x801D7C14 0x801D8134>
  bl        four_palettes_hook1
  .data     <VERS 0x802758C8 0x8027590C 0x80276D44 0x80275034 0x80275D70 0x80276DDC 0x80276B90 0x8027658C>
  .data     0x00000004
  .address  <VERS 0x802758C8 0x8027590C 0x80276D44 0x80275034 0x80275D70 0x80276DDC 0x80276B90 0x8027658C>
  bl        four_palettes_hook2
  .data     <VERS 0x8024B440 0x8024B440 0x8024C59C 0x8024ABB8 0x8024B5E4 0x8024C1B0 0x8024C2D8 0x8024BDE4>
  .data     0x00000004
  .address  <VERS 0x8024B440 0x8024B440 0x8024C59C 0x8024ABB8 0x8024B5E4 0x8024C1B0 0x8024C2D8 0x8024BDE4>
  b         four_palettes_hook3
  .data     <VERS 0x80334C3C 0x80334C80 0x8033675C 0x8033424C 0x803352B8 0x803367E0 0x80336588 0x80335BA0>
  .data     0x00000004
  .address  <VERS 0x80334C3C 0x80334C80 0x8033675C 0x8033424C 0x803352B8 0x803367E0 0x80336588 0x80335BA0>
  bl        four_palettes_hook4
  .data     <VERS 0x802462C8 0x802462C8 0x802473F4 0x80245A7C 0x8024643C 0x80247510 0x80247130 0x80246C3C>
  .data     0x00000008
  .address  <VERS 0x802462C8 0x802462C8 0x802473F4 0x80245A7C 0x8024643C 0x80247510 0x80247130 0x80246C3C>
  lhz       r0, [r31 + 0x004A]  # D-pad version: lhz       r0, [r31 + 0x0048]
  rlwinm.   r3, r0, 0, 24, 27  # D-pad version: rlwinm.   r3, r0, 0, 18, 21
  .data     <VERS 0x80275928 0x8027596C 0x80276DA4 0x80275094 0x80275DD0 0x80276E3C 0x80276BF0 0x802765EC>
  .data     0x00000008
  .address  <VERS 0x80275928 0x8027596C 0x80276DA4 0x80275094 0x80275DD0 0x80276E3C 0x80276BF0 0x802765EC>
  lhz       r0, [r31 + 0x004A]  # D-pad version: lhz       r0, [r31 + 0x0048]
  rlwinm.   r3, r0, 0, 24, 27  # D-pad version: rlwinm.   r3, r0, 0, 18, 21

  # Full Action List (Incl. Photon Blasts & Traps)
  .data     <VERS 0x802766F8 0x8027673C 0x80277B74 0x80275E64 0x80276BA0 0x80277C0C 0x802779C0 0x802773BC>
  .data     0x00000004
  .address  <VERS 0x802766F8 0x8027673C 0x80277B74 0x80275E64 0x80276BA0 0x80277C0C 0x802779C0 0x802773BC>
  subi      r0, r3, 0x07E8
  .data     <VERS 0x8044BB3C 0x8044BFBC 0x8044F8B4 0x8044ADAC 0x8044DBCC 0x8044FC34 0x8044F9FC 0x8044F6B4>
  .data     0x00000034
  .data     0x0004000D
  .data     0x0004000E
  .data     0x00000000
  .data     0x0004000F
  .data     0x00040010
  .data     0x00000000
  .data     0x00050000
  .data     0x00050001
  .data     0x00050002
  .data     0x00050003
  .data     0x00050004
  .data     0x00050005
  .data     0x00080000

  # Save Extra Palettes To Memory Card (Temp Slot 3)
  .data     <VERS 0x8046DC5C 0x8046E0DC 0x80471ACC 0x8046CECC 0x8046FCEC 0x80471E4C 0x80471C14 0x80471804>
  .data     0x00000004
  .data     0xFFFFFFFF

  .data     0x00000000
  .data     0x00000000
