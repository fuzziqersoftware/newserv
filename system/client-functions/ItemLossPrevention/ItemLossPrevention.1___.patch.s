.meta name="No item loss"
.meta description="Disables logic that\ndeletes items if\nyou don't log off\nnormally"

.versions 1OJ4 1OEF 1OPF

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksDC

  .align    4
  .data     <VERS 0x8C0254B2 0x8C0254BE 0x8C0254D2>
  .data     4
  bs        +0x38
  nop

  .align    4
  .data     <VERS 0x8C150B2C 0x8C150F9C 0x8C150D58>
  .data     2
  sett

  .align    4
  .data     <VERS 0x8C15F346 0x8C15F856 0x8C15F612>
  .data     2
  and       r0, 0xFE

  .align    4
  .data     <VERS 0x8C16053A 0x8C160A4A 0x8C160806>
  .data     2
  and       r0, 0xFE

  .align    4
  .data     <VERS 0x8C1617DA 0x8C161D6A 0x8C161B26>
  .data     2
  and       r0, 0xFE

  .align    4
  .data     <VERS 0x8C15F3BA 0x8C15F8CA 0x8C15F686>
  .data     2
  nop

  .align    4
  .data     <VERS 0x8C1605A6 0x8C160AB6 0x8C160872>
  .data     2
  nop

  .align    4
  .data     <VERS 0x8C161808 0x8C161D98 0x8C161B54>
  .data     2
  nop

  .align    4
  .data     0x00000000
  .data     0x00000000
