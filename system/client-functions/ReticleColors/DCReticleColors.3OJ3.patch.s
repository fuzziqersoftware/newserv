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
  # region @ 802AC2A4 (4 bytes)
  .data     0x802AC2A4  # address
  .data     0x00000004  # size
  .data     0x3C8000FF  # 802AC2A4 => lis       r4, 0x00FF
  # region @ 802AC2B8 (4 bytes)
  .data     0x802AC2B8  # address
  .data     0x00000004  # size
  .data     0x388000FF  # 802AC2B8 => li        r4, 0x00FF
  # region @ 802AC2CC (4 bytes)
  .data     0x802AC2CC  # address
  .data     0x00000004  # size
  .data     0x3884FF00  # 802AC2CC => subi      r4, r4, 0x0100
  # region @ 804A5638 (8 bytes)
  .data     0x804A5638  # address
  .data     0x00000008  # size
  .data     0x3F800000  # 804A5638 => lis       r28, 0x0000
  .data     0x00000000  # 804A563C => .invalid
  # region @ 804A5648 (8 bytes)
  .data     0x804A5648  # address
  .data     0x00000008  # size
  .data     0x3F800000  # 804A5648 => lis       r28, 0x0000
  .data     0x00000000  # 804A564C => .invalid
  # region @ 804A5658 (12 bytes)
  .data     0x804A5658  # address
  .data     0x0000000C  # size
  .data     0x3F800000  # 804A5658 => lis       r28, 0x0000
  .data     0x3F800000  # 804A565C => lis       r28, 0x0000
  .data     0x00000000  # 804A5660 => .invalid
  # region @ 804A5668 (4 bytes)
  .data     0x804A5668  # address
  .data     0x00000004  # size
  .data     0x00000000  # 804A5668 => .invalid
  # region @ 804A5670 (4 bytes)
  .data     0x804A5670  # address
  .data     0x00000004  # size
  .data     0x3F800000  # 804A5670 => lis       r28, 0x0000
  # region @ 804A5678 (12 bytes)
  .data     0x804A5678  # address
  .data     0x0000000C  # size
  .data     0x3ECCCCCD  # 804A5678 => subis     r22, r12, 0x3333
  .data     0x3DCCCCCD  # 804A567C => subis     r14, r12, 0x3333
  .data     0x3DCCCCCD  # 804A5680 => subis     r14, r12, 0x3333
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
