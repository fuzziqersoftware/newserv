.meta name="Chat"
.meta description="Enables extended\nWord Select and\nstops the Log\nWindow from\nscrolling with L+R"
# Original codes by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

.versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0 3SJT 3SJ0 3SE0 3SP0

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC

  # Extended Word Select Menu (PSO PCv2 Style)
  .data     <VERS 0x8034445C 0x803457AC 0x80346CCC 0x80346A80 0x8034525C 0x803452A0 0x80346E4C 0x8034627C 0x801D9B30 0x801C7CFC 0x801C7D88 0x801C83FC>
  .data     0x00000004
  li        r3, 0

  # Chat Log Window LF/TAB Bug Fix
  .data     <VERS 0x80267DDC 0x80268A88 0x80269AE4 0x80269898 0x80268788 0x80268788 0x80269B5C 0x802693A4 0x8017F434 0x8016FD00 0x8016FBB4 0x80170060>
  .data     0x00000004
  nop

  # Chat Bubble Window TAB Bug Fix
  .data     <VERS 0x80250264 0x80250CB0 0x80251CA4 0x802519A4 0x80250AEC 0x80250AEC 0x80251C68 0x802514B0 0x8016A77C 0x8015B1BC 0x8015B0CC 0x8015B578>
  .data     0x00000004
  nop

  # Chat Log Window: Scroll Lock (Hold L+R)
  .label    scroll_lock_hook_loc, 0x8000D6A0
  .data     scroll_lock_hook_loc
  .deltaof  scroll_lock_hook_start, scroll_lock_hook_end
  .address  scroll_lock_hook_loc
scroll_lock_hook_start:
  lis       r3, <VERS 0x8051 0x8051 0x8051 0x8051 0x8051 0x8051 0x8051 0x8051 0x8048 0x804A 0x804A 0x804A>
  lhz       r3, [r3 <VERS -0x7530 -0x3A70 -0x1430 -0x1690 -0x6C50 -0x6770 -0x1D90 -0x0D70 +0x1700 -0x08C0 +0x0560 +0x2980>]
  andi.     r0, r3, 0x0003
  cmplwi    r0, 3
  beqlr
  stfs      [r28 + 0x0084], f1
  blr
scroll_lock_hook_end:

  .label    scroll_lock_hook_call, <VERS 0x80267EC8 0x80268B74 0x80269BD0 0x80269984 0x80268874 0x80268874 0x80269C48 0x80269490 0x8017F51C 0x8016FDE8 0x8016FC9C 0x80170148>
  .data     scroll_lock_hook_call
  .data     0x00000004
  .address  scroll_lock_hook_call
  bl        scroll_lock_hook_start

  .data     0x00000000
  .data     0x00000000
