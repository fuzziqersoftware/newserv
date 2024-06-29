  ret

  # Call table: 2 functions (on_window_created, on_hp_updated)
  jmp       on_window_created

on_hp_updated:
  call      rewrite_string
  movsx     ecx, word [ebp + 0x02BC] # Replaced opcode at callsite
  ret

on_window_created:
  mov       [0x00010C08], eax  # prev_desc
  push      ebp
  mov       ebp, ebx
  call      rewrite_string
  pop       ebp
  mov       dword [esp + 4], 0x00010C1C  # Change first argument to desc_buf
  jmp       [0x00010C04]  # Call original function

rewrite_string:
  movsx     eax, word [ebp + 0x02BC]  # max HP
  push      eax
  movsx     eax, word [ebp + 0x0330]  # current HP
  push      eax
  push      dword [0x00010C08]  # prev_desc
  push      0x00010C0C  # desc_template
  push      0x00010C1C  # desc_buf
  call      [0x00010C00]  # sprintf
  add       esp, 0x14
  ret
