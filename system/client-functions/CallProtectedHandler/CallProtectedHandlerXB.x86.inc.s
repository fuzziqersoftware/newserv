  jmp    get_data_addr
resume:
  xchg   ebx, [esp]

  mov    edx, [ebx]
  mov    dword [edx], 1

  mov    edx, [ebx + 4]
  lea    ecx, [ebx + 0x0C]
  mov    eax, [ebx + 8]
  call   edx

  mov    edx, [ebx]
  mov    dword [edx], 0

  pop    ebx
  ret

get_data_addr:
  call   resume
