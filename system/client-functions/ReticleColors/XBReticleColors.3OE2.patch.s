.meta name="Xbox/BB targets"
.meta description="Changes the target\nreticle colors to\nthose used on the\nXbox and Blue Burst"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC
  # region @ 802AD360 (4 bytes)
  .data     0x802AD360  # address
  .data     0x00000004  # size
  .data     0x388000FF  # 802AD360 => li        r4, 0x00FF
  # region @ 804A7188 (12 bytes)
  .data     0x804A7188  # address
  .data     0x0000000C  # size
  .data     0x00000000  # 804A7188 => .invalid
  .data     0x00000000  # 804A718C => .invalid
  .data     0x3F800000  # 804A7190 => lis       r28, 0x0000
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
