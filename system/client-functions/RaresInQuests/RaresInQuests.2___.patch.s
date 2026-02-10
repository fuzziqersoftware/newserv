.meta name="Rares in quests"
.meta description="Disables logic that\nprevents items\nabove 8 stars and\nrares from dropping\nin quests."

.versions 2OJ4 2OJ5 2OJF 2OEF 2OPF

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksDC

  .align    4
  .data     <VERS 0x8C21A28C 0x8C21A28C 0x8C2192C8 0x8C21A28C 0x8C219E6C>
  .data     2
  nop

  .align    4
  .data     <VERS 0x8C21A300 0x8C21A300 0x8C219254 0x8C21A300 0x8C219DF8>
  .data     2
  nop

  .align    4
  .data     0x00000000
  .data     0x00000000
