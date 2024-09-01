.meta name="Rare alerts"
.meta description="Shows rare items on\nthe map and plays a\nsound when a rare\nitem drops"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049
# Xbox port by fuzziqersoftware

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksXB



  # Map dot render hook

  .data     0x00172350
  .deltaof  dot1_start, dot1_end
dot1_start:
  cmp       byte [esi + 0x000000EF], 0x4
  jne       +0x280  # skip_dot
  jmp       +0x16  # dot2_start
dot1_end:

  .data     0x00172375
  .deltaof  dot2_start, dot2_end
dot2_start:
  push      0x00
  push      0x01
  push      0xFFFFFFFF  # White
  jmp       +0x252  # dot3_start
dot2_end:

  .data     0x001725D2
  .deltaof  dot3_start, dot3_end
dot3_start:
  lea       edx, [esi + 0x38]
  call      +0x16E8C6  # 002E0EA0 = minimap_render_dot
  add       esp, 0xC
skip_dot:
  mov       eax, esi
  pop       esi
  # Falls through to the original tail-call-optimized target (001725E0)
dot3_end:



  # Notification sound hook

  .data     0x00188538
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

  .data     0x00188A52
  .deltaof  sound2_start, sound2_end
sound2_start:
  xor       ecx, ecx
  push      ecx
  push      ecx
  push      ecx
  push      0x0000055E
  jmp       +0x33  # sound3_start
sound2_end:

  .data     0x00188A91
  .deltaof  sound3_start, sound3_end
sound3_start:
  call      +0x1626FA  # 002EB190 => play_sound(0x55E, nullptr, 0, 0);
  add       esp, 0x10
  ret
sound3_end:

  .data     0x00000000
  .data     0x00000000
