.meta hide_from_patches_menu
.meta name="Player flags"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC

  .label    check_controller_button, 0x801A6C68  # [std](ControllerState* st, uint32_t flags) -> bool
  .label    TFogCtrl_change_fog, 0x800FB10C  # [std](TFogCtrl* this, uint32_t fog_num, uint32_t instant_transition) -> void
  .label    render_debug_printf, 0x803D4E3C  # [std](uint32_t coords, const char* fmt, ...) -> void
  .label    set_debug_text_color, 0x803D4990  # [](uint32_t color_argb) -> void
  .label    hook_call, 0x80228A38
  .label    hook_loc, 0x8000A000

  # Disable D-pad up + down chat shortcuts
  .data     0x80246314
  .data     4
  li        r4, 0
  .data     0x80246330
  .data     4
  li        r4, 0

  .data     hook_call
  .data     4
  .address  hook_call
  b         hook_loc

  .data     hook_loc
  .deltaof  hook_start, hook_end
  .address  hook_loc
hook_start:
  mflr      r0
  stwu      [r1 - 0x20], r1
  stw       [r1 + 0x24], r0
  stw       [r1 + 0x08], r30
  stw       [r1 + 0x0C], r31

  lis       r30, 0x804F
  ori       r30, r30, 0xC7A8
  lwz       r4, [r13 - 0x5280]  # local_client_id
  rlwinm    r4, r4, 2, 0, 29
  lwzx      r30, [r30 + r4]  # r30 = TFogCtrl_for_client_id[local_client_id]

  cmpwi     r30, 0
  beq       hook_skip_all

  lwz       r31, [r30 + 0x0184]  # Active slot number
  rlwinm    r31, r31, 2, 0, 29
  addi      r31, r31, 0x174
  lwzx      r31, [r30 + r31]  # Active slot pointer
  lwz       r31, [r31 + 0x40]  # Active fog number

  # Check for button presses to change fog
  lis       r3, 0x8050
  ori       r3, r3, 0x9848
  li        r4, 0x0010  # D-pad up
  bl        check_controller_button
  cmplwi    r3, 0
  beq       hook_skip_incr_fog
  mr        r3, r30
  addi      r4, r31, 1
  andi.     r4, r4, 0x007F
  li        r5, 1
  bl        TFogCtrl_change_fog
hook_skip_incr_fog:
  lis       r3, 0x8050
  ori       r3, r3, 0x9848
  li        r4, 0x0020  # D-pad down
  bl        check_controller_button
  cmplwi    r3, 0
  beq       hook_skip_decr_fog
  mr        r3, r30
  subi      r4, r31, 1
  andi.     r4, r4, 0x007F
  li        r5, 1
  bl        TFogCtrl_change_fog
hook_skip_decr_fog:

  # Show the current fog number
  lis       r3, 0xFFFF
  ori       r3, r3, 0x00FF
  bl        set_debug_text_color
  lis       r3, 0x0002
  ori       r3, r3, 0x000B
  bl        hook_get_fmt_string
  .binary   "Fog: %02X"000000
hook_get_fmt_string:
  mflr      r4
  mr        r5, r31
  bl        render_debug_printf

hook_skip_all:
  lwz       r31, [r1 + 0x0C]
  lwz       r30, [r1 + 0x08]
  lwz       r0, [r1 + 0x24]
  addi      r1, r1, 0x20
  mtlr      r0
  blr

hook_end:

  .data     0
  .data     0
