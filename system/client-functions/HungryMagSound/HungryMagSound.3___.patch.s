.meta name="Mag alert"
.meta description="Plays a sound when\nyour Mag is hungry"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

.versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC

  .label    play_sound, <VERS 0x800336AC 0x800336DC 0x800336F8 0x8003368C 0x800338CC 0x800338CC 0x80033894 0x8003390C>

  .data     0x8000BF30
  .deltaof  code_start, code_end
  .address  0x8000BF30
code_start:  # [std] (TItemMag* this @ r3) -> void
  lwz       r4, [r3 + 0xF0]
  lhz       r4, [r4 + 0x1C]  # r4 = this->owner_player->entity_id
  lwz       r5, [r13 - <VERS 0x5298 0x5290 0x5270 0x5270 0x5280 0x5280 0x5260 0x5220>]  # local_client_id
  cmpl      r4, r5
  bnelr
  lis       r3, 0x0002
  ori       r3, r3, 0x2825
  li        r4, 0
  b         play_sound
code_end:

  .data     <VERS 0x80110D94 0x80110F94 0x80111080 0x80110F20 0x80111038 0x80111038 0x80110F30 0x80111114>
  .data     0x00000004
  .address  <VERS 0x80110D94 0x80110F94 0x80111080 0x80110F20 0x80111038 0x80111038 0x80110F30 0x80111114>
  b         code_start

  .data     0x00000000
  .data     0x00000000
