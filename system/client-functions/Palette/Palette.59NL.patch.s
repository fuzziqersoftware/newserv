# Original patch by Soly, in Blue Burst Patch Project
# https://github.com/Solybum/Blue-Burst-Patch-Project

.meta name="Palette"
.meta description="Enables the alternate action\npalette for number keys"

entry_ptr:
reloc0:
  .offsetof start

write_call_func:
  .include  WriteCallToCode-59NL

start:
  mov       al, 0xEB
  mov       [0x0068A739], al                  # SecondaryPaletteAttack1
  xor       al, al
  mov       [0x006A114F], al                  # SecondaryPaletteAttack2
  mov       [0x006A0C4F], al                  # SecondaryPaletteAttack3

  call      patch_func_1                      # GetCurrentPalette
  call      patch_func_2                      # CheckHotkey1_1
  call      patch_func_3                      # CheckHotkey1_2
  call      patch_func_4                      # CheckHotkey2_1
  call      patch_func_5                      # CheckHotkey2_2
  call      patch_func_6                      # CheckHotkey3_1
  call      patch_func_7                      # CheckHotkey3_2
  jmp       write_code_blocks                 # UnsetHotkey1, UnsetHotkey2, SetHotkey

# GetCurrentPalette
patch_func_1:
  pop       ecx
  push      8
  push      0x00748944
  call      get_code_size1
  .deltaof  patch_code1, patch_code_end1
get_code_size1:
  pop       eax
  push      dword [eax]
  call      patch_code_end1
patch_code1:
  mov       edx, [ebp - 0x14]
  mov       edx, [edx + 0x2C]
  movzx     edx, byte [edx + 0x62]
  test      edx, edx
  setnz     byte [0x00748ACF]
  mov       edx, edi
  and       edx, 0xFF
  ret
patch_code_end1:
  push      ecx
  jmp       write_call_func

# CheckHotkey1_1
patch_func_2:
  pop       ecx
  push      5
  push      0x00748992
  call      get_code_size2
  .deltaof  patch_code2, patch_code_end2
get_code_size2:
  pop       eax
  push      dword [eax]
  call      patch_code_end2
patch_code2:
  cmp       byte [0x00748ACF], 0
  jnz       +0x06
  movzx     edx, byte [eax + esi * 4 + 0x04]  # main palette
  ret
  movzx     edx, byte [eax + esi * 4 + 0x3C]  # alt palette
  ret
patch_code_end2:
  push      ecx
  jmp       write_call_func

# CheckHotkey1_2
patch_func_3:
  pop       ecx
  push      5
  push      0x007489A1
  call      get_code_size3
  .deltaof  patch_code3, patch_code_end3
get_code_size3:
  pop       eax
  push      dword [eax]
  call      patch_code_end3
patch_code3:
  cmp       byte [0x00748ACF], 0
  jnz       +0x06
  movzx     ecx, byte [eax + ecx * 2 + 0x05]  # main palette
  ret
  movzx     ecx, byte [eax + ecx * 2 + 0x3D]  # alt palette
  ret
patch_code_end3:
  push      ecx
  jmp       write_call_func

# CheckHotkey2_1
patch_func_4:
  pop       ecx
  push      5
  push      0x00748A3C
  call      get_code_size4
  .deltaof  patch_code4, patch_code_end4
get_code_size4:
  pop       eax
  push      dword [eax]
  call      patch_code_end4
patch_code4:
  cmp       byte [0x00748ACF], 0
  jnz       +0x06
  movzx     edx, byte [edx + ebx * 4 + 0x04]  # main palette
  ret
  movzx     edx, byte [edx + ebx * 4 + 0x3C]  # alt palette
  ret
patch_code_end4:
  push      ecx
  jmp       write_call_func

# CheckHotkey2_2
patch_func_5:
  pop       ecx
  push      5
  push      0x00748A4B
  call      get_code_size5
  .deltaof  patch_code5, patch_code_end5
get_code_size5:
  pop       eax
  push      dword [eax]
  call      patch_code_end5
patch_code5:
  cmp       byte [0x00748ACF], 0
  jnz       +0x06
  movzx     ecx, byte [edx + eax * 2 + 0x05]  # main palette
  ret
  movzx     ecx, byte [edx + eax * 2 + 0x3D]  # alt palette
  ret
patch_code_end5:
  push      ecx
  jmp       write_call_func

# CheckHotkey3_1
patch_func_6:
  pop       ecx
  push      5
  push      0x007103B7
  call      get_code_size6
  .deltaof  patch_code6, patch_code_end6
get_code_size6:
  pop       eax
  push      dword [eax]
  call      patch_code_end6
patch_code6:
  cmp       byte [0x00748ACF], 0
  jnz       +0x06
  movzx     ecx, byte [eax + edx * 4 + 0x04]  # main palette
  ret
  movzx     ecx, byte [eax + edx * 4 + 0x3C]  # alt palette
  ret
patch_code_end6:
  push      ecx
  jmp       write_call_func

# CheckHotkey3_2
patch_func_7:
  pop       ecx
  push      5
  push      0x007103C0
  call      get_code_size7
  .deltaof  patch_code7, patch_code_end7
get_code_size7:
  pop       eax
  push      dword [eax]
  call      patch_code_end7
patch_code7:
  cmp       byte [0x00748ACF], 0
  jnz       +0x06
  movzx     ecx, byte [eax + edx * 4 + 0x05]  # main palette
  ret
  movzx     ecx, byte [eax + edx * 4 + 0x3D]  # alt palette
  ret
patch_code_end7:
  push      ecx
  jmp       write_call_func

write_code_blocks:
  .include  WriteCodeBlocksBB

  .data     0x007489B9
  .deltaof  code_block1_start, code_block1_end

# UnsetHotkey1
code_block1_start:
  push      dword [0x00748ACF]
  push      eax
  mov       eax, 0x0068CDE0                   # SetPaletteHotkey
  call      eax
  .binary   909090909090909090
code_block1_end:
  .data     0x00748A5F
  .deltaof  code_block2_start, code_block2_end

# UnsetHotkey2
code_block2_start:
  push      dword [0x00748ACF]
  push      eax
  mov       eax, 0x0068CDE0                   # SetPaletteHotkey
  call      eax
  .binary   909090909090909090
code_block2_end:
  .data     0x00748ABE
  .deltaof  code_block3_start, code_block3_end

# SetHotkey
code_block3_start:
  mov       eax, [ebp - 0x24]
  mov       ecx, [ebp - 0x28]
  movzx     ebx, word [eax]
  movzx     edx, word [eax + 0x02]
  push      edx
  push      ebx
  push      esi
  .binary   6800000000                        # tmpCurrentPalette = 0x00748ACF
  push      0
  mov       eax, 0x0068CDE0                   # SetPaletteHotkey
  call      eax
  .binary   90909090909090909090909090909090
code_block3_end:
  .data     0x00000000
  .data     0x00000000
