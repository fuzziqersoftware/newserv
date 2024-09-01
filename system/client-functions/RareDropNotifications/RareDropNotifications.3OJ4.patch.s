.meta name="Rare alerts"
.meta description="Shows rare items on\nthe map and plays a\nsound when a rare\nitem drops"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC
  # region @ 8000C660 (40 bytes)
  .data     0x8000C660  # address
  .data     0x00000028  # size
  .data     0x881F00EF  # 8000C660 => lbz       r0, [r31 + 0x00EF]
  .data     0x28000004  # 8000C664 => cmplwi    r0, 4
  .data     0x40820018  # 8000C668 => bne       +0x00000018 /* 8000C680 */
  .data     0x387F0038  # 8000C66C => addi      r3, r31, 0x0038
  .data     0x3C80FFFF  # 8000C670 => lis       r4, 0xFFFF
  .data     0x38A00001  # 8000C674 => li        r5, 0x0001
  .data     0x38C00000  # 8000C678 => li        r6, 0x0000
  .data     0x481ED709  # 8000C67C => bl        +0x001ED708 /* 801F9D84 */
  .data     0x7FE3FB78  # 8000C680 => mr        r3, r31
  .data     0x480F6788  # 8000C684 => b         +0x000F6788 /* 80102E0C */
  # region @ 8000C690 (44 bytes)
  .data     0x8000C690  # address
  .data     0x0000002C  # size
  .data     0x28030000  # 8000C690 => cmplwi    r3, 0
  .data     0x41820020  # 8000C694 => beq       +0x00000020 /* 8000C6B4 */
  .data     0x880300EF  # 8000C698 => lbz       r0, [r3 + 0x00EF]
  .data     0x28000004  # 8000C69C => cmplwi    r0, 4
  .data     0x40820014  # 8000C6A0 => bne       +0x00000014 /* 8000C6B4 */
  .data     0x3C600005  # 8000C6A4 => lis       r3, 0x0005
  .data     0x60632813  # 8000C6A8 => ori       r3, r3, 0x2813
  .data     0x38800000  # 8000C6AC => li        r4, 0x0000
  .data     0x48027049  # 8000C6B0 => bl        +0x00027048 /* 800336F8 */
  .data     0x80010024  # 8000C6B4 => lwz       r0, [r1 + 0x0024]
  .data     0x4810EA38  # 8000C6B8 => b         +0x0010EA38 /* 8011B0F0 */
  # region @ 80102E08 (4 bytes)
  .data     0x80102E08  # address
  .data     0x00000004  # size
  .data     0x4BF09858  # 80102E08 => b         -0x000F67A8 /* 8000C660 */
  # region @ 8011B0EC (4 bytes)
  .data     0x8011B0EC  # address
  .data     0x00000004  # size
  .data     0x4BEF15A4  # 8011B0EC => b         -0x0010EA5C /* 8000C690 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
