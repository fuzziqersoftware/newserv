start:
  call      process_stick_value
  push      esi
  sub       esi, 4
  call      process_stick_value
  pop       esi
  jmp       end

process_stick_value:
  mov       ax, [esp + 0x3C]    # ax = stick val y
  cwd                           # dx = FFFF if y < 0 else 0000
  xor       dh, dh              # dx = 00FF if y < 0 else 0000
  add       ax, dx              # adjust ax if y negative else don't
  sar       ax, 8               # eax = ----Y1Y2
  neg       ax
  shl       eax, 16             # eax = Y1Y2-----

  mov       ax, [esp + 0x3A]    # ax = stick val x
  cwd                           # dx = FFFF if x < 0 else 0000
  xor       dh, dh              # dx = 00FF if x < 0 else 0000
  add       ax, dx              # adjust ax if x negative else don't
  sar       ax, 8               # eax = Y1Y2X1X2

  # check for deadzone - if either value is outside of deadzone, allow movement
  xor       ecx, ecx            # ecx = 0
  lea       edx, [eax + 0x28]   # edx = eax + 0x28 (deadzone range is -0x28 through 0x28)
  cmp       dl, 0x50
  cmova     ecx, eax            # if X2 out of deadzone range, use eax
  mov       edx, eax
  bswap     edx                 # dh = Y2
  add       dh, 0x28            # dh = Y2 + 0x28
  cmp       dh, 0x50
  cmova     ecx, eax            # if Y2 out of deadzone range, use eax

  mov       [esi + 0x18], ecx   # set processed stick values
  ret

  .zero     0x56
end:
