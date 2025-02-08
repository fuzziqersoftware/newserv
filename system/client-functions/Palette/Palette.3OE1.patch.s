.meta name="Palette"
.meta description="Use C stick to\nuse 4 customize\nconfigurations\ninstead of just one"
# Original codes by Ralf @ GC-Forever
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

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
  .label    get_main_phase, 0x8000F948

  .data     0x8000B958  # Save Extra Palettes To Memory Card (Temp Slot 3)
  .deltaof  save_to_memcard_hook1, save_to_memcard_end
  .address  0x8000B958
save_to_memcard_hook1:
  stw       [r13 - 0x46AC], r3
  mulli     r3, r3, 60
  lwz       r4, [r13 - 0x46C8]
  addis     r4, r4, 0x0001
  addi      r4, r4, 0x0B80
  add       r4, r4, r3
  lis       r3, 0x8000
  ori       r3, r3, 0xCF40
  li        r5, 0x003C
  b         memcpy
save_to_memcard_hook2:
  stw       [r4], r3
  lwz       r3, [r13 - 0x46AC]
  mulli     r3, r3, 60
  lwz       r4, [r13 - 0x46C8]
  addis     r4, r4, 0x0001
  addi      r4, r4, 0x0B80
  add       r3, r4, r3
  lis       r4, 0x8000
  ori       r4, r4, 0xCF40
  li        r5, 0x003C
  b         memcpy
save_to_memcard_hook3:
  lwz       r3, [r13 - 0x46AC]
  mulli     r3, r3, 60
  lwz       r4, [r13 - 0x46C8]
  addis     r4, r4, 0x0001
  addi      r4, r4, 0x0B80
  add       r3, r4, r3
  li        r4, 0x0000
  li        r5, 0x003C
  bl        memset
  bl        get_main_phase
  b         [801FF034]
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
  lhz       r0, [r3 - 0x676C]  # D-pad version: lhz       r0, [r3 - 0x676A]
  lhz       r5, [r3 - 0x6770]
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
  ori       r12, r12, 0xACD8
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

  .data     0x801B59E4  # Disable Photon Blast Palette Switching
  .data     0x00000004
  .address  0x801B59E4
  li        r3, 0x0000

  .data     0x801D8230  # Full Action List (Incl. Photon Blasts & Traps)
  .data     0x00000004
  .address  0x801D8230
  bl        full_action_list_hook1
  .data     0x801CC038  # Full Action List (Incl. Photon Blasts & Traps)
  .data     0x00000004
  .address  0x801CC038
  bl        full_action_list_hook2

  .data     0x801FC2D8  # Save Extra Palettes To Memory Card (Temp Slot 3)
  .data     0x00000004
  .address  0x801FC2D8
  bl        save_to_memcard_hook1
  .data     0x801FFB14  # Save Extra Palettes To Memory Card (Temp Slot 3)
  .data     0x00000004
  .address  0x801FFB14
  bl        save_to_memcard_hook2
  .data     0x801FF030  # Save Extra Palettes To Memory Card (Temp Slot 3)
  .data     0x00000004
  .address  0x801FF030
  b         save_to_memcard_hook3

  .data     0x801D7A78  # Have Four Action Button Palettes
  .data     0x00000004
  .address  0x801D7A78
  bl        four_palettes_hook1
  .data     0x8027590C  # Have Four Action Button Palettes
  .data     0x00000004
  .address  0x8027590C
  bl        four_palettes_hook2
  .data     0x8024B440  # Have Four Action Button Palettes
  .data     0x00000004
  .address  0x8024B440
  b         four_palettes_hook3
  .data     0x80334C80  # Have Four Action Button Palettes
  .data     0x00000004
  .address  0x80334C80
  bl        four_palettes_hook4

  .data     0x802462C8  # Have Four Action Button Palettes
  .data     0x00000008
  .address  0x802462C8
  lhz       r0, [r31 + 0x004A]  # D-pad version: lhz       r0, [r31 + 0x0048]
  rlwinm.   r3, r0, 0, 24, 27  # D-pad version: rlwinm.   r3, r0, 0, 18, 21

  .data     0x8027596C  # Have Four Action Button Palettes
  .data     0x00000008
  .address  0x8027596C
  lhz       r0, [r31 + 0x004A]  # D-pad version: lhz       r0, [r31 + 0x0048]
  rlwinm.   r3, r0, 0, 24, 27  # D-pad version: rlwinm.   r3, r0, 0, 18, 21

  .data     0x8027673C  # Full Action List (Incl. Photon Blasts & Traps)
  .data     0x00000004
  .address  0x8027673C
  subi      r0, r3, 0x07E8

  .data     0x8044BFBC  # Full Action List (Incl. Photon Blasts & Traps)
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

  .data     0x8046E0DC  # Save Extra Palettes To Memory Card (Temp Slot 3)
  .data     0x00000004
  .data     0xFFFFFFFF

  .data     0x00000000
  .data     0x00000000
