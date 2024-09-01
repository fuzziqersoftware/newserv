.meta name="Chat"
.meta description="Enables extended\nWord Select and\nstops the Log\nWindow from\nscrolling with L+R"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC
  # region @ 8000D6A0 (28 bytes)
  .data     0x8000D6A0  # address
  .data     0x0000001C  # size
  .data     0x3C608051  # 8000D6A0 => lis       r3, 0x8051
  .data     0xA0638AD0  # 8000D6A4 => lhz       r3, [r3 - 0x7530]
  .data     0x70600003  # 8000D6A8 => andi.     r0, r3, 0x0003
  .data     0x28000003  # 8000D6AC => cmplwi    r0, 3
  .data     0x41820008  # 8000D6B0 => beq       +0x00000008 /* 8000D6B8 */
  .data     0xD03C0084  # 8000D6B4 => stfs      [r28 + 0x0084], f1
  .data     0x4825A814  # 8000D6B8 => b         +0x0025A814 /* 80267ECC */
  # region @ 80267EC8 (4 bytes)
  .data     0x80267EC8  # address
  .data     0x00000004  # size
  .data     0x4BDA57D8  # 80267EC8 => b         -0x0025A828 /* 8000D6A0 */
  # region @ 8034445C (4 bytes)
  .data     0x8034445C  # address
  .data     0x00000004  # size
  .data     0x38600000  # 8034445C => li        r3, 0x0000
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
