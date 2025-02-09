.meta name="Chat"
.meta description="Enables extended\nWord Select and\nstops the Log\nWindow from\nscrolling with L+R"
# Original codes by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC

  .data     0x801C7CFC  # Extended Word Select Menu (PSO PCv2 Style)
  .data     0x00000004
  .address  0x801C7CFC
  li        r3, 0

  .data     0x8016FD00  # Chat Log Window LF/TAB Bug Fix
  .data     0x00000004
  .address  0x8016FD00
  nop

  .data     0x8015B1BC  # Chat Bubble Window TAB Bug Fix
  .data     0x00000004
  .address  0x8015B1BC
  nop

  .data     0x8000D6A0  # Chat Log Window: Scroll Lock (Hold L+R)
  .deltaof  scroll_lock_hook, scroll_lock_hook_end
  .address  0x8000D6A0
scroll_lock_hook:
  lis       r3, 0x804A
  lhz       r3, [r3 - 0x08C0]
  andi.     r0, r3, 0x0003
  cmplwi    r0, 3
  beqlr
  stfs      [r28 + 0x0084], f1
  blr
scroll_lock_hook_end:

  .data     0x8016FDE8  # Chat Log Window: Scroll Lock (Hold L+R)
  .data     0x00000004
  .address  0x8016FDE8
  bl        scroll_lock_hook

  .data     0x00000000
  .data     0x00000000
