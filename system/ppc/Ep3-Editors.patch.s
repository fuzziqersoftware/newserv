# This patch enables the debug menus in PSO Episode 3 USA. Specifically, it
# causes them all to load, but only activates one (selected by uncommenting a
# line below). See the comments for more information.

# This patch is only for PSO Episode 3 USA, which means it requires the
# EnableEpisode3SendFunctionCall option to be enabled in config.json. If that
# option is disabled, the Patches menu won't appear for the client. If this
# patch is run on a different client version, it will do nothing.

# When you run this patch (via the $patch chat command, since it must be done
# when already in a lobby or game), the screen will go mostly black for a few
# seconds while the editors are constructed. It's unclear why this takes so
# long, but it doesn't seem to cause any other issues.

# This patch automatically does nothing if it has been run already in the
# current session, since some of the things it does have unclear lifecycles.
# This means it can only be used once per power-on; in the future, we could hook
# into the destroy functions of the various editors to know when it's OK to
# construct them again. (We'd also have to manage some hackish bookkeeping that
# the TEditor base class does so its list doesn't overflow, which would cause
# any new editors to be deleted immediately.)

entry_ptr:
reloc0:
  .offsetof start

start:
  # Note: We don't actually need this for Episode 3, since all Episode 3
  # versions correctly clear the caches before running code from a B2 command.
  # But we leave it in to be consistent with patches for Episodes 1&2.
  .include InitClearCaches

  stwu   [r1 - 0x20], r1
  mflr   r0
  stw    [r1 + 0x24], r0
  stw    [r1 + 0x10], r31
  stw    [r1 + 0x0C], r30
  stw    [r1 + 0x08], r29

  # First, make sure this is actually Episode 3 USA; if not, do nothing
  lis    r4, 0x4750
  ori    r4, r4, 0x5345 # 'GPSE'
  lis    r5, 0x8000
  lwz    r5, [r5]
  li     r3, -1
  cmp    r4, r5
  bne    skip_all

  # Running this patch multiple times will likely crash the client, so do
  # nothing if we detect the patch has already run.
  lis    r29, 0x8000
  ori    r29, r29, 0xBD44
  lwz    r3, [r29 - 4]
  mr.    r0, r3
  bne    skip_all
  li     r0, -1
  stw    [r29 - 4], r0

setup_editors:
  # This function sets up various things that the editors require. Most editors
  # will crash in update() if this isn't called before construction time.
  lis    r0, 0x8002
  ori    r0, r0, 0x9D88
  mtctr  r0
  bctrl

construct_editors:
  # Call the constructors for all the editors and save the object pointers
  lis    r30, 0x8043
  ori    r30, r30, 0x3760
  addi   r31, r30, 0xB4  # 15 entries * 12 bytes per entry
again:
  lwz    r0, [r30 + 0x08]
  mtctr  r0
  bctrl
  addi   r30, r30, 0x0C
  stw    [r29], r3
  addi   r29, r29, 4
  cmpl   r30, r31
  blt    again

activate_chosen_editor:
  # All of the editors have flags set at construction time that effectively
  # disable them (by disabling both the update and render functions). At the
  # time this code is executed, the flags are already set, so we can unset them
  # for whichever editor we want to run by uncommenting the appropriate lwz
  # opcode below.
  # Most of these tools expect input from the controller in port 3; the comments
  # below all refer to inputs from that port.

  li     r4, 0
  lis    r29, 0x8000
  ori    r29, r29, 0xBD44

  # lwz    r4, [r29 + 0x00] # TGroupSetEditor
  #   This editor is very similar to TGroupEnemySetEditor (see below).

  # lwz    r4, [r29 + 0x04] # TGroupEnemySetEditor
  #   TODO: This editor crashes on update. It sort of works if you ignore
  #   invalid memory accesses in Dolphin, but this is not a good solution.
  #   Figure out if this is an Ep3-specific issue and fix it if possible.
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
  #   TODO: This doesn't appear to do anything, despite having a lot of code
  #   that checks various buttons on the controller. Figure this out and make it
  #   work.

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

  # lwz    r4, [r29 + 0x34] # TEffIndirectEditor (no visible effects)
  #   TODO: It's not apparent what this editor does, or if it even survives to
  #   the update/render phase. Further research is needed here.

  # lwz    r4, [r29 + 0x38] # TCCScenarioDebug (movie/cutscene tests)
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
