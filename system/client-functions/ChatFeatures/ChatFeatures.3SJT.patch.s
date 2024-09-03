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

  .data     0x8000D6A0
  .data     0x0000001C
  lis       r3, 0x8048
  lhz       r3, [r3 + 0x1238]
  andi.     r0, r3, 0x0003
  cmplwi    r0, 3
  beqlr
  stfs      [r28 + 0x0084], f1
  blr

  .data     0x8017F51C
  .data     0x00000004
  .data     0x4BE8E185

  .data     0x801D9B30
  .data     0x00000004
  li        r3, 0x0000

  .data     0x00000000
  .data     0x00000000
