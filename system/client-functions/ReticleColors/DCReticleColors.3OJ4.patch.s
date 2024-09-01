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
  # region @ 802AD3D0 (4 bytes)
  .data     0x802AD3D0  # address
  .data     0x00000004  # size
  .data     0x3C8000FF  # 802AD3D0 => lis       r4, 0x00FF
  # region @ 802AD3E4 (4 bytes)
  .data     0x802AD3E4  # address
  .data     0x00000004  # size
  .data     0x388000FF  # 802AD3E4 => li        r4, 0x00FF
  # region @ 802AD3F8 (4 bytes)
  .data     0x802AD3F8  # address
  .data     0x00000004  # size
  .data     0x3884FF00  # 802AD3F8 => subi      r4, r4, 0x0100
  # region @ 804A7AD8 (8 bytes)
  .data     0x804A7AD8  # address
  .data     0x00000008  # size
  .data     0x3F800000  # 804A7AD8 => lis       r28, 0x0000
  .data     0x00000000  # 804A7ADC => .invalid
  # region @ 804A7AE8 (8 bytes)
  .data     0x804A7AE8  # address
  .data     0x00000008  # size
  .data     0x3F800000  # 804A7AE8 => lis       r28, 0x0000
  .data     0x00000000  # 804A7AEC => .invalid
  # region @ 804A7AF8 (12 bytes)
  .data     0x804A7AF8  # address
  .data     0x0000000C  # size
  .data     0x3F800000  # 804A7AF8 => lis       r28, 0x0000
  .data     0x3F800000  # 804A7AFC => lis       r28, 0x0000
  .data     0x00000000  # 804A7B00 => .invalid
  # region @ 804A7B08 (4 bytes)
  .data     0x804A7B08  # address
  .data     0x00000004  # size
  .data     0x00000000  # 804A7B08 => .invalid
  # region @ 804A7B10 (4 bytes)
  .data     0x804A7B10  # address
  .data     0x00000004  # size
  .data     0x3F800000  # 804A7B10 => lis       r28, 0x0000
  # region @ 804A7B18 (12 bytes)
  .data     0x804A7B18  # address
  .data     0x0000000C  # size
  .data     0x3ECCCCCD  # 804A7B18 => subis     r22, r12, 0x3333
  .data     0x3DCCCCCD  # 804A7B1C => subis     r14, r12, 0x3333
  .data     0x3DCCCCCD  # 804A7B20 => subis     r14, r12, 0x3333
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
