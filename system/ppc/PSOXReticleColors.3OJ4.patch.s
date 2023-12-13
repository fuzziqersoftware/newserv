.meta name="Xbox/BB targets"
.meta description="Change the target\nreticle colors to\nthose used on the\nXbox and Blue Burst"
# Original code by Ralf @ GC-Forever

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocks
  # region @ 802AD3F8 (4 bytes)
  .data     0x802AD3F8  # address
  .data     0x00000004  # size
  .data     0x388000FF  # 802AD3F8 => li        r4, 0x00FF
  # region @ 804A7AF8 (12 bytes)
  .data     0x804A7AF8  # address
  .data     0x0000000C  # size
  .data     0x00000000  # 804A7AF8 => .invalid
  .data     0x00000000  # 804A7AFC => .invalid
  .data     0x3F800000  # 804A7B00 => lis       r28, 0x0000
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
