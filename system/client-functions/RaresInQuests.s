.meta visibility="all"
.meta name="Rares in quests"
.meta description="Disables logic that\nprevents items\nabove 8 stars and\nrares from dropping\nin quests."


entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocks



  .versions 1OEF 1OJ3 1OJ4 1OJF 1OPF

  .align    4
  .data     <VERS 0x8C1F1210 0x8C1EE044 0x8C1F0C10 0x8C1EE1F0 0x8C1F0FB4>
  .data     2
  mov       r0, 0

  .align    4
  .data     <VERS 0x8C1F1182 0x8C1EDFB6 0x8C1F0B82 0x8C1EE162 0x8C1F0F26>
  .data     2
  mov       r0, 0

  .align    4



  .versions 2OJ4 2OJ5 2OJF 2OEF 2OPF

  .align    4
  .data     <VERS 0x8C21A28C 0x8C21A28C 0x8C2192C8 0x8C21A28C 0x8C219E6C>
  .data     2
  nop

  .align    4
  .data     <VERS 0x8C21A300 0x8C21A300 0x8C219254 0x8C21A300 0x8C219DF8>
  .data     2
  nop

  .align    4



  .versions 2OJW

  .data     0x004DFC9A
  .data     2
  nop
  nop

  .data     0x004E03F4
  .data     2
  nop
  nop



  .all_versions

  .data     0x00000000
  .data     0x00000000
