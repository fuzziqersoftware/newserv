.meta name="MAG alert"
.meta description="Plays a sound when\nyour MAG is hungry"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC
  # region @ 8000BF30 (44 bytes)
  .data     0x8000BF30  # address
  .data     0x0000002C  # size
  .data     0x9421FFF0  # 8000BF30 => stwu      [r1 - 0x0010], r1
  .data     0x7C0802A6  # 8000BF34 => mflr      r0
  .data     0x90010014  # 8000BF38 => stw       [r1 + 0x0014], r0
  .data     0x3C600002  # 8000BF3C => lis       r3, 0x0002
  .data     0x60632825  # 8000BF40 => ori       r3, r3, 0x2825
  .data     0x38800000  # 8000BF44 => li        r4, 0x0000
  .data     0x480279C5  # 8000BF48 => bl        +0x000279C4 /* 8003390C */
  .data     0x80010014  # 8000BF4C => lwz       r0, [r1 + 0x0014]
  .data     0x7C0803A6  # 8000BF50 => mtlr      r0
  .data     0x38210010  # 8000BF54 => addi      r1, r1, 0x0010
  .data     0x4E800020  # 8000BF58 => blr
  # region @ 80111114 (4 bytes)
  .data     0x80111114  # address
  .data     0x00000004  # size
  .data     0x4BEFAE1C  # 80111114 => b         -0x001051E4 /* 8000BF30 */
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
