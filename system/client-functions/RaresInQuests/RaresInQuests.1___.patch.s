.meta name="Rares in quests"
.meta description="Disables logic that\nprevents items\nabove 8 stars and\nrares from dropping\nin quests."

.versions 1OEF 1OJ3 1OJ4 1OJF 1OPF

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksDC

  .align    4
  .data     <VERS 0x8C1F1210 0x8C1EE044 0x8C1F0C10 0x8C1EE1F0 0x8C1F0FB4>
  .data     2
  mov       r0, 0

  .align    4
  .data     <VERS 0x8C1F1182 0x8C1EDFB6 0x8C1F0B82 0x8C1EE162 0x8C1F0F26>
  .data     2
  mov       r0, 0

  .align    4
  .data     0x00000000
  .data     0x00000000
