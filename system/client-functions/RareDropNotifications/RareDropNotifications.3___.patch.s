.meta name="Rare alerts"
.meta description="Shows rare items on\nthe map and plays a\nsound when a rare\nitem drops"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

.versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC

  .label    hook_code, 0x8000C660



  # Called from TItem::update in the case when the item is on the ground
  .label    minimap_hook_call, <VERS 0x8010261C 0x801027F8 0x80102E08 0x80102778 0x801028C0 0x801028C0 0x80102788 0x80102978>
  .label    minimap_hook_ret, <VERS 0x80102620 0x801027FC 0x80102E0C 0x8010277C 0x801028C4 0x801028C4 0x8010278C 0x8010297C>
  .label    minimap_render_dot, <VERS 0x801F9490 0x801F9B2C 0x801F9D84 0x801F9B38 0x801F99FC 0x801F99FC 0x801F9B8C 0x801FA108>

  .data     hook_code
  .deltaof  hooks_start, hooks_end
  .address  hook_code
hooks_start:
minimap_hook:
  lbz       r0, [r31 + 0x00EF]
  cmplwi    r0, 4
  bne       minimap_hook_skip_render  # if (item->box_type != ItemBoxType::RARE) return;
  addi      r3, r31, 0x0038
  lis       r4, 0xFFFF
  li        r5, 0x0001
  li        r6, 0x0000
  bl        minimap_render_dot  # minimap_render_dot(&item->location, RED_COLOR, 1, 0);
minimap_hook_skip_render:
  mr        r3, r31
  b         minimap_hook_ret



  # Called from handle_6x5F immediately after the item is constructed
  .label    sound_hook_call, <VERS 0x8011AD00 0x8011AF1C 0x8011B0EC 0x8011AEB4 0x8011AFA4 0x8011AFA4 0x8011AEC4 0x8011B09C>
  .label    sound_hook_ret, <VERS 0x8011AD04 0x8011AF20 0x8011B0F0 0x8011AEB8 0x8011AFA8 0x8011AFA8 0x8011AEC8 0x8011B0A0>
  .label    play_sound_at_location, <VERS 0x800336AC 0x800336DC 0x800336F8 0x8003368C 0x800338CC 0x800338CC 0x80033894 0x8003390C>

sound_hook:
  cmplwi    r3, 0
  beq       sound_hook_skip_play  # if (item == nullptr) return;
  lbz       r0, [r3 + 0x00EF]
  cmplwi    r0, 4
  bne       sound_hook_skip_play  # if (item->box_type != ItemBoxType::RARE) return;
  lis       r3, 0x0005
  ori       r3, r3, 0x2813
  li        r4, 0x0000
  bl        play_sound_at_location  # play_sound_at_location(RARE_JINGLE, nullptr);
sound_hook_skip_play:
  lwz       r0, [r1 + 0x0024]
  b         sound_hook_ret
hooks_end:



  .data     minimap_hook_call
  .data     4
  .address  minimap_hook_call
  b         minimap_hook

  .data     sound_hook_call
  .data     4
  .address  sound_hook_call
  b         sound_hook



  .data     0x00000000
  .data     0x00000000
