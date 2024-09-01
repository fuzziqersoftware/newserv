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
  # region @ 802ABDFC (4 bytes)
  .data     0x802ABDFC  # address
  .data     0x00000004  # size
  .data     0x3C8000FF  # 802ABDFC => lis       r4, 0x00FF
  # region @ 802ABE10 (4 bytes)
  .data     0x802ABE10  # address
  .data     0x00000004  # size
  .data     0x388000FF  # 802ABE10 => li        r4, 0x00FF
  # region @ 802ABE24 (4 bytes)
  .data     0x802ABE24  # address
  .data     0x00000004  # size
  .data     0x3884FF00  # 802ABE24 => subi      r4, r4, 0x0100
  # region @ 804A2BA8 (8 bytes)
  .data     0x804A2BA8  # address
  .data     0x00000008  # size
  .data     0x3F800000  # 804A2BA8 => lis       r28, 0x0000
  .data     0x00000000  # 804A2BAC => .invalid
  # region @ 804A2BB8 (8 bytes)
  .data     0x804A2BB8  # address
  .data     0x00000008  # size
  .data     0x3F800000  # 804A2BB8 => lis       r28, 0x0000
  .data     0x00000000  # 804A2BBC => .invalid
  # region @ 804A2BC8 (12 bytes)
  .data     0x804A2BC8  # address
  .data     0x0000000C  # size
  .data     0x3F800000  # 804A2BC8 => lis       r28, 0x0000
  .data     0x3F800000  # 804A2BCC => lis       r28, 0x0000
  .data     0x00000000  # 804A2BD0 => .invalid
  # region @ 804A2BD8 (4 bytes)
  .data     0x804A2BD8  # address
  .data     0x00000004  # size
  .data     0x00000000  # 804A2BD8 => .invalid
  # region @ 804A2BE0 (4 bytes)
  .data     0x804A2BE0  # address
  .data     0x00000004  # size
  .data     0x3F800000  # 804A2BE0 => lis       r28, 0x0000
  # region @ 804A2BE8 (12 bytes)
  .data     0x804A2BE8  # address
  .data     0x0000000C  # size
  .data     0x3ECCCCCD  # 804A2BE8 => subis     r22, r12, 0x3333
  .data     0x3DCCCCCD  # 804A2BEC => subis     r14, r12, 0x3333
  .data     0x3DCCCCCD  # 804A2BF0 => subis     r14, r12, 0x3333
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
