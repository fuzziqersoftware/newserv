.meta name="PSO Peeps Dragon HP"
.meta description="Sets Normal Dragon HP\nto 1800 for V2 crossplay"

entry_ptr:
reloc0:
  .offsetof start

start:
  .include  WriteCodeBlocksGC

  # GC Plus USA / 3OE1
  # BattleParamEntry_on.dat loaded Normal Dragon row at 0x811ABA48
  # HP field is row + 0x06 = 0x811ABA4E
  # 2500 = 09 C4; 1800 = 07 08
  .data     0x811ABA4E
  .data     2
  .binary   0708

  .data     0
  .data     0
