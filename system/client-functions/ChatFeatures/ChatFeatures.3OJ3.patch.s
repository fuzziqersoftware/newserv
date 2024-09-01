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
  .data     0xA063C590  # 8000D6A4 => lhz       r3, [r3 - 0x3A70]
  .data     0x70600003  # 8000D6A8 => andi.     r0, r3, 0x0003
  .data     0x28000003  # 8000D6AC => cmplwi    r0, 3
  .data     0x41820008  # 8000D6B0 => beq       +0x00000008 /* 8000D6B8 */
  .data     0xD03C0084  # 8000D6B4 => stfs      [r28 + 0x0084], f1
  .data     0x4825B4C0  # 8000D6B8 => b         +0x0025B4C0 /* 80268B78 */
  # region @ 80268B74 (4 bytes)
  .data     0x80268B74  # address
  .data     0x00000004  # size
  .data     0x4BDA4B2C  # 80268B74 => b         -0x0025B4D4 /* 8000D6A0 */
  # region @ 803457AC (4 bytes)
  .data     0x803457AC  # address
  .data     0x00000004  # size
  .data     0x38600000  # 803457AC => li        r3, 0x0000
  # end sentinel
  .data     0x00000000  # address
  .data     0x00000000  # size
