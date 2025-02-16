.meta name="Rares in quests"
.meta description="Disables logic that\nprevents items\nabove 8 stars and\nrares from dropping\nin quests."

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksDC

  .align    4
  .data     0x8C1F0FB4
  .data     2
  mov       r0, 0

  .align    4
  .data     0x8C1F0F26
  .data     2
  mov       r0, 0

  .align    4
  .data     0x00000000
  .data     0x00000000
