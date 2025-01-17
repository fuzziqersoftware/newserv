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
  .data     0x0026ABA3
  .data     0x00000001
  .binary   C0
  .data     0x0026ABAC
  .data     0x00000001
  .binary   FA
  .data     0x0026ADA6
  .data     0x00000004
  .binary   836004FD
  .data     0x00545334
  .data     0x00000004
  .data     0x42960000
  .data     0x00545364
  .data     0x00000004
  .data     0x42960000
  .data     0x00545394
  .data     0x00000004
  .data     0x42960000
  .data     0x005453C4
  .data     0x00000004
  .data     0x42960000
  .data     0x005453F4
  .data     0x00000004
  .data     0x42780000
  .data     0x00545410
  .data     0x00000004
  .data     0xFF00FF15

  .data     0x00010C00
  .deltaof  str_data_start, str_data_end
str_data_start:
  .data     0x00313B22  # sprintf
  .data     0x002649C0  # Original function for on_window_created callsite
  .data     0x00000000
  .binary   "%s\n\nHP:%d/%d"
  .data     0x00000000
  .data     0x00000000
str_data_end:

  .data     0x002D90E0
  .deltaof  new_code_start, new_code_end
new_code_start:
  .include  EnemyHPBars-TextHandlerXB
new_code_end:

  .data     0x0026AD81
  .data     0x00000007
  nop
  nop
  .binary   E85BE30600  # call 002D90E3 (on_hp_updated)

  .data     0x0026AB68
  .data     0x00000005
  .binary   E874E50600  # call 002D90E1 (on_window_created)

  .data     0x00000000
  .data     0x00000000
