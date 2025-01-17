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
  .data     0x0026B083
  .data     0x00000001
  .binary   C0
  .data     0x0026B08C
  .data     0x00000001
  .binary   FA
  .data     0x0026B286
  .data     0x00000004
  .binary   836004FD
  .data     0x0054A92C
  .data     0x00000004
  .data     0x42960000
  .data     0x0054A95C
  .data     0x00000004
  .data     0x42960000
  .data     0x0054A98C
  .data     0x00000004
  .data     0x42960000
  .data     0x0054A9BC
  .data     0x00000004
  .data     0x42960000
  .data     0x0054A9EC
  .data     0x00000004
  .data     0x42780000
  .data     0x0054AA08
  .data     0x00000004
  .data     0xFF00FF15

  .data     0x00010C00
  .deltaof  str_data_start, str_data_end
str_data_start:
  .data     0x00318338  # sprintf
  .data     0x00264EA0  # Original function for on_window_created callsite
  .data     0x00000000
  .binary   "%s\n\nHP:%d/%d"
  .data     0x00000000
  .data     0x00000000
str_data_end:

  .data     0x002DB080
  .deltaof  new_code_start, new_code_end
new_code_start:
  .include  EnemyHPBars-TextHandlerXB
new_code_end:

  .data     0x0026B261
  .data     0x00000007
  nop
  nop
  .binary   E81BFE0600  # call 002DB083 (on_hp_updated)

  .data     0x0026B048
  .data     0x00000005
  .binary   E834000700  # call 002DB081 (on_window_created)

  .data     0x00000000
  .data     0x00000000
