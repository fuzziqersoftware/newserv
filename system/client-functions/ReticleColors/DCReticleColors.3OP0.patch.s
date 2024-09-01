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
  # region @ 802ACACC (4 bytes)
  .data     0x802ACACC  # address
  .data     0x00000004  # size
  .data     0x3C8000FF  # 802ACACC => lis       r4, 0x00FF
  # region @ 802ACAE0 (4 bytes)
  .data     0x802ACAE0  # address
  .data     0x00000004  # size
  .data     0x388000FF  # 802ACAE0 => li        r4, 0x00FF
  # region @ 802ACAF4 (4 bytes)
  .data     0x802ACAF4  # address
  .data     0x00000004  # size
  .data     0x3884FF00  # 802ACAF4 => subi      r4, r4, 0x0100
  # region @ 804A75E8 (8 bytes)
  .data     0x804A75E8  # address
  .data     0x00000008  # size
  .data     0x3F800000  # 804A75E8 => lis       r28, 0x0000
  .data     0x00000000  # 804A75EC => .invalid
  # region @ 804A75F8 (8 bytes)
  .data     0x804A75F8  # address
  .data     0x00000008  # size
  .data     0x3F800000  # 804A75F8 => lis       r28, 0x0000
  .data     0x00000000  # 804A75FC => .invalid
  # region @ 804A7608 (12 bytes)
  .data     0x804A7608  # address
  .data     0x0000000C  # size
  .data     0x3F800000  # 804A7608 => lis       r28, 0x0000
  .data     0x3F800000  # 804A760C => lis       r28, 0x0000
  .data     0x00000000  # 804A7610 => .invalid
  # region @ 804A7618 (4 bytes)
  .data     0x804A7618  # address
  .data     0x00000004  # size
  .data     0x00000000  # 804A7618 => .invalid
  # region @ 804A7620 (4 bytes)
  .data     0x804A7620  # address
  .data     0x00000004  # size
  .data     0x3F800000  # 804A7620 => lis       r28, 0x0000
  # region @ 804A7628 (12 bytes)
  .data     0x804A7628  # address
  .data     0x0000000C  # size
  .data     0x3ECCCCCD  # 804A7628 => subis     r22, r12, 0x3333
  .data     0x3DCCCCCD  # 804A762C => subis     r14, r12, 0x3333
  .data     0x3DCCCCCD  # 804A7630 => subis     r14, r12, 0x3333
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
