.meta name="Enemy HP bars"
.meta description="Shows HP bars in\nenemy info windows"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049
# Xbox port by fuzziqersoftware

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksXB
  .data     0x0026B193
  .data     0x00000001
  .binary   C0
  .data     0x0026B19C
  .data     0x00000001
  .binary   FA
  .data     0x0026B396
  .data     0x00000004
  .binary   836004FD
  .data     0x0054A1CC
  .data     0x00000004
  .data     0x42960000
  .data     0x0054A1FC
  .data     0x00000004
  .data     0x42960000
  .data     0x0054A22C
  .data     0x00000004
  .data     0x42960000
  .data     0x0054A25C
  .data     0x00000004
  .data     0x42960000
  .data     0x0054A28C
  .data     0x00000004
  .data     0x42780000
  .data     0x0054A2A8
  .data     0x00000004
  .data     0xFF00FF15

  .data     0x00010C00
  .deltaof  str_data_start, str_data_end
str_data_start:
  .data     0x00317D7A  # sprintf
  .data     0x00264F80  # Original function for on_window_created callsite
  .data     0x00000000
  .binary   "%s\n\nHP:%d/%d"
  .data     0x00000000
  .data     0x00000000
str_data_end:

  .data     0x002DB550
  .deltaof  new_code_start, new_code_end
new_code_start:
  .include  EnemyHPBars-TextHandlerXB
new_code_end:

  .data     0x0026B371
  .data     0x00000007
  nop
  nop
  .binary   E8DB010700  # call 002DB553 (on_hp_updated)

  .data     0x0026B158
  .data     0x00000005
  .binary   E8F4030700  # call 002DB551 (on_window_created)

  .data     0x00000000
  .data     0x00000000
