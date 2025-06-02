.meta name="Rare alerts"
.meta description="Shows rare items on\nthe map and plays a\nsound when a rare\nitem drops"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049
# Xbox port by fuzziqersoftware

.versions 4OED 4OEU 4OJB 4OJD 4OJU 4OPD 4OPU

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksXB



  # Map dot render hook

  .data     <VERS 0x00172350 0x00172360 0x00172220 0x00172310 0x001723F0 0x00172370 0x00172320>
  .deltaof  dot1_start, dot1_end
dot1_start:
  cmp       byte [esi + 0x000000EF], 0x4
  jne       +0x280  # skip_dot
  jmp       +0x16  # dot2_start
dot1_end:

  .data     <VERS 0x00172375 0x00172385 0x00172245 0x00172335 0x00172415 0x00172395 0x00172345>
  .deltaof  dot2_start, dot2_end
dot2_start:
  push      0x00
  push      0x01
  push      0xFFFFFFFF  # White
  jmp       +0x252  # dot3_start
dot2_end:

  .data     <VERS 0x001725D2 0x001725E2 0x001724A2 0x00172592 0x00172672 0x001725F2 0x001725A2>
  .deltaof  dot3_start, dot3_end
dot3_start:
  lea       edx, [esi + 0x38]
  call      <VERS +0x16E8C6 +0x16ED86 +0x16CA96 +0x16D526 +0x16ECD6 +0x16E8D6 +0x16EDF6>  # minimap_render_dot
  add       esp, 0xC
skip_dot:
  mov       eax, esi
  pop       esi
  # Falls through to the original tail-call-optimized target
dot3_end:



  # Notification sound hook

  .data     <VERS 0x00188538 0x00188578 0x00188388 0x00188548 0x00188648 0x00188558 0x00188538>
  .deltaof  sound1_start, sound1_end
sound1_start:
  pop       edi  # From original function; shorter replacement for add esp, 4
  pop       edi  # From original function
  pop       esi  # From original function
  add       esp, 0xC  # From original function
  test      eax, eax
  je        fail  # Item does not exist (was on a different floor)
  cmp       byte [eax + 0xEF], 0x4
  je        +0x503  # sound2_start
fail:
  ret
sound1_end:

  .data     <VERS 0x00188A52 0x00188A92 0x001888A2 0x00188A62 0x00188B62 0x00188A72 0x00188A52>
  .deltaof  sound2_start, sound2_end
sound2_start:
  xor       ecx, ecx
  push      ecx
  push      ecx
  push      ecx
  push      0x0000055E
  jmp       +0x33  # sound3_start
sound2_end:

  .data     <VERS 0x00188A91 0x00188AD1 0x001888E1 0x00188AA1 0x00188BA1 0x00188AB1 0x00188A91>
  .deltaof  sound3_start, sound3_end
sound3_start:
  call      <VERS +0x1626FA +0x1628DA +0x16094A +0x16130A +0x1627EA +0x16270A +0x16294A>  # play_sound(0x55E, nullptr, 0, 0);
  add       esp, 0x10
  ret
sound3_end:

  .data     0x00000000
  .data     0x00000000
