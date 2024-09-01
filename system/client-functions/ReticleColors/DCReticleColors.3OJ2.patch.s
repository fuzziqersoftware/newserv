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
  # region @ 802AB3FC (4 bytes)
  .data     0x802AB3FC  # address
  .data     0x00000004  # size
  .data     0x3C8000FF  # 802AB3FC => lis       r4, 0x00FF
  # region @ 802AB410 (4 bytes)
  .data     0x802AB410  # address
  .data     0x00000004  # size
  .data     0x388000FF  # 802AB410 => li        r4, 0x00FF
  # region @ 802AB424 (4 bytes)
  .data     0x802AB424  # address
  .data     0x00000004  # size
  .data     0x3884FF00  # 802AB424 => subi      r4, r4, 0x0100
  # region @ 804A1F18 (8 bytes)
  .data     0x804A1F18  # address
  .data     0x00000008  # size
  .data     0x3F800000  # 804A1F18 => lis       r28, 0x0000
  .data     0x00000000  # 804A1F1C => .invalid
  # region @ 804A1F28 (8 bytes)
  .data     0x804A1F28  # address
  .data     0x00000008  # size
  .data     0x3F800000  # 804A1F28 => lis       r28, 0x0000
  .data     0x00000000  # 804A1F2C => .invalid
  # region @ 804A1F38 (12 bytes)
  .data     0x804A1F38  # address
  .data     0x0000000C  # size
  .data     0x3F800000  # 804A1F38 => lis       r28, 0x0000
  .data     0x3F800000  # 804A1F3C => lis       r28, 0x0000
  .data     0x00000000  # 804A1F40 => .invalid
  # region @ 804A1F48 (4 bytes)
  .data     0x804A1F48  # address
  .data     0x00000004  # size
  .data     0x00000000  # 804A1F48 => .invalid
  # region @ 804A1F50 (4 bytes)
  .data     0x804A1F50  # address
  .data     0x00000004  # size
  .data     0x3F800000  # 804A1F50 => lis       r28, 0x0000
  # region @ 804A1F58 (12 bytes)
  .data     0x804A1F58  # address
  .data     0x0000000C  # size
  .data     0x3ECCCCCD  # 804A1F58 => subis     r22, r12, 0x3333
  .data     0x3DCCCCCD  # 804A1F5C => subis     r14, r12, 0x3333
  .data     0x3DCCCCCD  # 804A1F60 => subis     r14, r12, 0x3333
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
