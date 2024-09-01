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
  # region @ 802ABDB8 (4 bytes)
  .data     0x802ABDB8  # address
  .data     0x00000004  # size
  .data     0x3C8000FF  # 802ABDB8 => lis       r4, 0x00FF
  # region @ 802ABDCC (4 bytes)
  .data     0x802ABDCC  # address
  .data     0x00000004  # size
  .data     0x388000FF  # 802ABDCC => li        r4, 0x00FF
  # region @ 802ABDE0 (4 bytes)
  .data     0x802ABDE0  # address
  .data     0x00000004  # size
  .data     0x3884FF00  # 802ABDE0 => subi      r4, r4, 0x0100
  # region @ 804A26C8 (8 bytes)
  .data     0x804A26C8  # address
  .data     0x00000008  # size
  .data     0x3F800000  # 804A26C8 => lis       r28, 0x0000
  .data     0x00000000  # 804A26CC => .invalid
  # region @ 804A26D8 (8 bytes)
  .data     0x804A26D8  # address
  .data     0x00000008  # size
  .data     0x3F800000  # 804A26D8 => lis       r28, 0x0000
  .data     0x00000000  # 804A26DC => .invalid
  # region @ 804A26E8 (12 bytes)
  .data     0x804A26E8  # address
  .data     0x0000000C  # size
  .data     0x3F800000  # 804A26E8 => lis       r28, 0x0000
  .data     0x3F800000  # 804A26EC => lis       r28, 0x0000
  .data     0x00000000  # 804A26F0 => .invalid
  # region @ 804A26F8 (4 bytes)
  .data     0x804A26F8  # address
  .data     0x00000004  # size
  .data     0x00000000  # 804A26F8 => .invalid
  # region @ 804A2700 (4 bytes)
  .data     0x804A2700  # address
  .data     0x00000004  # size
  .data     0x3F800000  # 804A2700 => lis       r28, 0x0000
  # region @ 804A2708 (12 bytes)
  .data     0x804A2708  # address
  .data     0x0000000C  # size
  .data     0x3ECCCCCD  # 804A2708 => subis     r22, r12, 0x3333
  .data     0x3DCCCCCD  # 804A270C => subis     r14, r12, 0x3333
  .data     0x3DCCCCCD  # 804A2710 => subis     r14, r12, 0x3333
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
