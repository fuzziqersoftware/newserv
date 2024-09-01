.meta name="DC targets"
.meta description="Changes the target\nreticle colors to\nthose used on the\nDreamcast"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC
  # region @ 802AD338 (4 bytes)
  .data     0x802AD338  # address
  .data     0x00000004  # size
  .data     0x3C8000FF  # 802AD338 => lis       r4, 0x00FF
  # region @ 802AD34C (4 bytes)
  .data     0x802AD34C  # address
  .data     0x00000004  # size
  .data     0x388000FF  # 802AD34C => li        r4, 0x00FF
  # region @ 802AD360 (4 bytes)
  .data     0x802AD360  # address
  .data     0x00000004  # size
  .data     0x3884FF00  # 802AD360 => subi      r4, r4, 0x0100
  # region @ 804A7168 (8 bytes)
  .data     0x804A7168  # address
  .data     0x00000008  # size
  .data     0x3F800000  # 804A7168 => lis       r28, 0x0000
  .data     0x00000000  # 804A716C => .invalid
  # region @ 804A7178 (8 bytes)
  .data     0x804A7178  # address
  .data     0x00000008  # size
  .data     0x3F800000  # 804A7178 => lis       r28, 0x0000
  .data     0x00000000  # 804A717C => .invalid
  # region @ 804A7188 (12 bytes)
  .data     0x804A7188  # address
  .data     0x0000000C  # size
  .data     0x3F800000  # 804A7188 => lis       r28, 0x0000
  .data     0x3F800000  # 804A718C => lis       r28, 0x0000
  .data     0x00000000  # 804A7190 => .invalid
  # region @ 804A7198 (4 bytes)
  .data     0x804A7198  # address
  .data     0x00000004  # size
  .data     0x00000000  # 804A7198 => .invalid
  # region @ 804A71A0 (4 bytes)
  .data     0x804A71A0  # address
  .data     0x00000004  # size
  .data     0x3F800000  # 804A71A0 => lis       r28, 0x0000
  # region @ 804A71A8 (12 bytes)
  .data     0x804A71A8  # address
  .data     0x0000000C  # size
  .data     0x3ECCCCCD  # 804A71A8 => subis     r22, r12, 0x3333
  .data     0x3DCCCCCD  # 804A71AC => subis     r14, r12, 0x3333
  .data     0x3DCCCCCD  # 804A71B0 => subis     r14, r12, 0x3333
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
