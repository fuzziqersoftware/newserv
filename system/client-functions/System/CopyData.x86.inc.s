  # eax = dest ptr
  # edx = src ptr
  # ecx = size
  # Clobbers eax, ecx, edx
  push    ebx
again:
  test    ecx, ecx
  jz      done
  dec     ecx
  mov     bl, [edx + ecx]
  mov     [eax + ecx], bl
  jmp     again
done:
  pop     ebx
