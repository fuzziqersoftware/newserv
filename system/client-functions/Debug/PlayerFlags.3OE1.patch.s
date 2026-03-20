.meta hide_from_patches_menu
.meta name="Player flags"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC

  .label    TObjPlayer_for_client_id, 0x801BA59C  # [std](uint32_t client_id)
  .label    render_debug_printf, 0x803D4E3C  # [std](uint32_t coords, const char* fmt, ...);
  .label    set_debug_text_color, 0x803D4990  # [](uint32_t color_argb);
  .label    hook_call, 0x80228A38
  .label    hook_loc, 0x8000A000

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
  stw       [r1 + 8], r30
  li        r30, 0

hook_again:
  li        r6, 0
  mr        r3, r30
  bl        TObjPlayer_for_client_id
  cmplwi    r3, 0
  beq       hook_skip_player
  lwz       r6, [r3 + 0x0334]  # player_flags
  lwz       r4, [r13 - 0x5280]  # local_client_id
  cmp       r4, r30
  bne       hook_not_local_player
  lis       r3, 0xFFFF
  ori       r3, r3, 0x00FF
  b         hook_player_flags_ok
hook_not_local_player:
  lis       r3, 0xFFFF
  ori       r3, r3, 0xFFFF
hook_player_flags_ok:
  bl        set_debug_text_color

  lis       r3, 0x0002
  ori       r3, r3, 0x000B
  add       r3, r3, r30
  bl        hook_get_fmt_string
  .binary   "Player %2d: %08X"00000000
hook_get_fmt_string:
  mflr      r4
  mr        r5, r30
  bl        render_debug_printf

hook_skip_player:
  addi      r30, r30, 1
  cmplwi    r30, 0x0C
  blt       hook_again

  lwz       r30, [r1 + 8]
  lwz       r0, [r1 + 0x24]
  addi      r1, r1, 0x20
  mtlr      r0
  blr
hook_end:

  .data     0
  .data     0
