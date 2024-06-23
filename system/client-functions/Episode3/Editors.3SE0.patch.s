# This patch enables the debug menus in PSO Episode 3 USA. Specifically, it
# causes them all to load, but only activates one (selected by uncommenting a
# line below). See the comments for more information. Most of these editors are
# present in PSO PC and PSOX as well, but not in GC Episodes 1 & 2. There are
# notes in the below comments that may help get these editors working on PSO PC.

# This patch must not be run from the Patches menu - it should only be run with
# the $patch command, since the client will likely crash if the player is not
# in a game or lobby when the patch runs.

.meta hide_from_patches_menu
.meta name="Editors"
.meta description="Enables the various\ndebug menus"

entry_ptr:
reloc0:
  .offsetof start

start:
  stwu   [r1 - 0x20], r1
  mflr   r0
  stw    [r1 + 0x24], r0
  stw    [r1 + 0x10], r31
  stw    [r1 + 0x0C], r30
  stw    [r1 + 0x08], r29

  # Write a short hook that updates our editors table when TEditor_destroy() is
  # called
  bl     get_TEditor_destroy_hook_addr
  mr     r4, r3
  bl     get_TEditor_destroy_hook_end
  sub    r5, r3, r4
  subi   r5, r5, 0x08
  lis    r3, 0x8000
  ori    r3, r3, 0xBD00
  bl     copy_code

  # Make TEditor_destroy call our hook immediately before returning
  bl     get_patch_branch_opcode
  mr     r4, r3
  lis    r3, 0x8002
  ori    r3, r3, 0xE554
  li     r5, 4
  bl     copy_code

  lis    r29, 0x8000
  ori    r29, r29, 0x17C4

construct_editors:
  # Call the constructors for all the editors and save the object pointers. If
  # an editor already exists, set its disable flags. (This behavior allows this
  # patch to run again to switch to a different editor without changing rooms.)
  # Note: In PSO PC (the version I have, at least) this table is at 00691FA8.
  lis    r30, 0x8043
  ori    r30, r30, 0x3760
  addi   r31, r30, 0xB4  # 15 entries * 12 bytes per entry
again:
  lwz    r3, [r29]
  mr.    r0, r3
  bne    editor_already_exists
  lwz    r0, [r30 + 0x08]
  mtctr  r0
  bctrl
  stw    [r29], r3
  mr.    r0, r3
  beq    editor_construction_failed
editor_already_exists:
  li     r0, 0x0014 # Flags: disable update, disable render
  # See comment below about the flags field on PSO PC.
  sth    [r3 + 0x04], r0
editor_construction_failed:
  addi   r30, r30, 0x0C
  addi   r29, r29, 4
  cmpl   r30, r31
  blt    again

activate_chosen_editor:
  # All of the editors have flags set at construction time that effectively
  # disable them (by disabling both the update and render functions). At the
  # time this code is executed, the flags are already set (and we set them again
  # in the above loop anyway), so we can unset the flags for whichever editor we
  # want to run by uncommenting the appropriate lwz opcode below.
  # Most of these tools expect input from the controller in port 3; the comments
  # below all refer to inputs from that port.

  li     r4, 0
  lis    r29, 0x8000
  ori    r29, r29, 0x17C4

  # lwz    r4, [r29 + 0x00] # TGroupSetEditor
  #   This editor is very similar to TGroupEnemySetEditor (see below).

  # lwz    r4, [r29 + 0x04] # TGroupEnemySetEditor
  #   This editor only works in a game; it crashes if loaded in the lobby.
  #   Use the D-pad to choose a value; hold X and use the D-pad to modify the
  #   selected value. Hold R to use the menu on the right.

  # lwz    r4, [r29 + 0x08] # TCameraEditor
  #   This editor displays a floating-point value at the bottom of the screen,
  #   which you can modify with C-left and C-right. It's not apparent what this
  #   value represents, though.

  # lwz    r4, [r29 + 0x0C] # TParticleEditor
  #   This editor has two modes. Hold A and press X to switch modes. In "MAIN
  #   MODE", use D-left/D-right to pick an effect. Hold L to make the effect
  #   picker manageable (instead of insanely fast). In "ELEMENT MODE", it seems
  #   that any of the displayed values can be modified, but the selector is very
  #   hard to see (the selected section is rendered in FFFFFF, while the others
  #   are rendered in F0F0F0 - very similar colors!). Hold A, Y, or X and use
  #   the D-pad to change a value in the selected section (each of A/Y/X
  #   corresponds to a specific field in the current section).

  # lwz    r4, [r29 + 0x10] # TFreeCamera
  #   This editor does nothing. Probably it was never implemented or the code
  #   was intentionally deleted (though if it was, it's not clear why only this
  #   editor's code was deleted).

  # lwz    r4, [r29 + 0x14] # TFogEditor
  #   Use L/R to pick a line, and the D-pad to modify the values. NO specifies
  #   which fog entry you're editing (0-127).

  # lwz    r4, [r29 + 0x18] # TLightEditor
  #   Used for testing character lighting. Use L to select a section and the
  #   D-pad to choose and modify values within that section. COLOR and DIR
  #   specify the properties of the highlight; AMBIENT specifies the color of
  #   the non-highlight lighting. It's not clear what the last section does.

  # lwz    r4, [r29 + 0x1C] # nothing (type table entry is blank)

  # lwz    r4, [r29 + 0x20] # TSeqVarsEdit
  #   Use L/R to change pages, use the D-pad to pick a flag, and use A to toggle
  #   it. There are 8192 flags in total (0x400 bytes).

  # lwz    r4, [r29 + 0x24] # TSetEvtScriptTest
  #   Use D-left/D-right to change the label value and D-up/D-down to move the
  #   menu selection. Two of the menu items appear to do nothing, and the last
  #   crashes. Maybe it works better on Episodes 1&2.

  # lwz    r4, [r29 + 0x28] # nothing (type table entry is blank)

  # lwz    r4, [r29 + 0x2C] # TQuestScriptChecker (quest debugger)
  #   Use L to change functions, and the D-pad to navigate within each function.
  #   If you set EVENT NO to a very high value, the editor can appear messed up;
  #   what actually happens is the value is shifted one decimal place to the
  #   right, but the cursor remains in the same position with incorrect
  #   highlighting. The value appears to be a signed 32-bit integer. On the
  #   registers page, use D-left/D-right to see more registers; hold X and use
  #   the D-pad to modify a register's value. Similarly, hold X and use the
  #   D-pad on the breakpoints page to change values.

  # lwz    r4, [r29 + 0x30] # TPlyPKEditor (battle mode options)
  #   Use the D-pad to move the cursor and set options. In Episode 3, it appears
  #   this debugger doesn't do anything. It's likely more functional in Episodes
  #   1 & 2.

  # lwz    r4, [r29 + 0x34] # TEffIndirectEditor
  # li     r0, 1
  # stw    [r4 + 0x38], r0
  #   This editor is missing in PSO PC, but is present in PSOX. It appears to be
  #   used for testing texture overlay effects, but it doesn't work properly in
  #   Episode 3 - none of the effects appear to do anything. All three lines
  #   above must be uncommented for it to load.

  # lwz    r4, [r29 + 0x38] # TCCScenarioDebug (movie/cutscene tests)
  #   This editor exists only in Episode 3 - it is neither in PSOPC nor PSOX.
  #   Nothing appears immediately after activating this debugger because the
  #   default page is blank. Use C-left and C-right to change major pages; use
  #   L/R to change minor pages (sets of 50 flags within each major page). Use
  #   the D-pad to pick a flag and A to toggle it. On the "STAFFROLL" page, use
  #   the D-pad to pick a movie, and R+A to play it. If you watch the movie to
  #   the end, you'll return to your game and things will work as normal, but
  #   the textures will likely have been overwritten with garbage data.

  li     r3, 0
  mr.    r0, r4
  beq    skip_enable_editor
  # Note: The PSO PC TObject structure is a bit different; the flags field is at
  # +8 instead of +4 (but it is still a 16-bit integer).
  sth    [r4 + 4], r3
skip_enable_editor:

skip_all:
  lwz    r29, [r1 + 0x08]
  lwz    r30, [r1 + 0x0C]
  lwz    r31, [r1 + 0x10]
  lwz    r0, [r1 + 0x24]
  addi   r1, r1, 0x20
  mtlr   r0
  blr



copy_code:
  .include CopyCode
  blr



get_addr_ret:
  mflr   r3
  mtlr   r0
  blr

get_TEditor_destroy_hook_addr:
  mflr   r0
  bl     get_addr_ret

TEditor_destroy_hook:
  li     r4, 0x0F
  mtctr  r4
  lis    r4, 0x8000
  ori    r4, r4, 0xBD40
  li     r0, 0
TEditor_destroy_hook_check_next:
  lwzu   r5, [r4 + 4]
  cmp    r5, r3
  bne    TEditor_destroy_hook_skip_clear
  stw    [r4], r0
TEditor_destroy_hook_skip_clear:
  bdnz   TEditor_destroy_hook_check_next
  blr

get_TEditor_destroy_hook_end:
get_patch_branch_opcode:
  mflr   r0
  bl     get_addr_ret

  .data  0x4BFDD7AC # (at 8002E554) b 8000BD00
