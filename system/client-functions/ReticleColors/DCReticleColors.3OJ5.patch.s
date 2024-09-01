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
  # region @ 802AD184 (4 bytes)
  .data     0x802AD184  # address
  .data     0x00000004  # size
  .data     0x3C8000FF  # 802AD184 => lis       r4, 0x00FF
  # region @ 802AD198 (4 bytes)
  .data     0x802AD198  # address
  .data     0x00000004  # size
  .data     0x388000FF  # 802AD198 => li        r4, 0x00FF
  # region @ 802AD1AC (4 bytes)
  .data     0x802AD1AC  # address
  .data     0x00000004  # size
  .data     0x3884FF00  # 802AD1AC => subi      r4, r4, 0x0100
  # region @ 804A7898 (8 bytes)
  .data     0x804A7898  # address
  .data     0x00000008  # size
  .data     0x3F800000  # 804A7898 => lis       r28, 0x0000
  .data     0x00000000  # 804A789C => .invalid
  # region @ 804A78A8 (8 bytes)
  .data     0x804A78A8  # address
  .data     0x00000008  # size
  .data     0x3F800000  # 804A78A8 => lis       r28, 0x0000
  .data     0x00000000  # 804A78AC => .invalid
  # region @ 804A78B8 (12 bytes)
  .data     0x804A78B8  # address
  .data     0x0000000C  # size
  .data     0x3F800000  # 804A78B8 => lis       r28, 0x0000
  .data     0x3F800000  # 804A78BC => lis       r28, 0x0000
  .data     0x00000000  # 804A78C0 => .invalid
  # region @ 804A78C8 (4 bytes)
  .data     0x804A78C8  # address
  .data     0x00000004  # size
  .data     0x00000000  # 804A78C8 => .invalid
  # region @ 804A78D0 (4 bytes)
  .data     0x804A78D0  # address
  .data     0x00000004  # size
  .data     0x3F800000  # 804A78D0 => lis       r28, 0x0000
  # region @ 804A78D8 (12 bytes)
  .data     0x804A78D8  # address
  .data     0x0000000C  # size
  .data     0x3ECCCCCD  # 804A78D8 => subis     r22, r12, 0x3333
  .data     0x3DCCCCCD  # 804A78DC => subis     r14, r12, 0x3333
  .data     0x3DCCCCCD  # 804A78E0 => subis     r14, r12, 0x3333
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
