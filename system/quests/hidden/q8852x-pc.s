start:
  # Clear the fake shop text pointer that we created in the preamble
  mov     dword [0x007337D8], 0

  # Leave the "game" immediately by setting should_leave_game = 1 (sends 98)
  mov     dword [0x00700884], 1

  # Find VirtualProtect
  push    0x00673034
  call    [0x0064D114]  # GetModuleHandleA("KERNEL32")
  call    get_VirtualProtect_name
  .binary "VirtualProtect"00
get_VirtualProtect_name:
  push    eax
  call    [0x0064D128]  # GetProcAddress(kernel32_module, "VirtualProtect")

  # Push args to both VirtualProtect calls now
  push    0x00000000  # Buffer for prev_flags2 (it cannot be null)
  push    esp
  push    0x00000000  # Will be overwritten by first call
  mov     edx, esp
  push    0x0023ED70
  push    0x0040E000
  push    eax  # LATER: VirtualProtect(0x0040E000, 0x0023ED70, prev_flags, &prev_flags2)
  push    edx
  push    0x00000004
  push    0x0023ED70
  push    0x0040E000
  call    eax  # VirtualProtect(0x0040E000, 0x0023ED70, PAGE_READWRITE, &prev_flags)

  # Patch handle_B2 to actually run the server's code. The changes are:
  #   0058E1AC:
  #     EB D7          jmp     0x0058E185
  #   0058E185:
  #     83 C4 08       add     esp, 8  # Replaced opcode
  #     FF 10          call    [eax]
  #     89 44 24 0C    mov     [esp + 0x0C], eax  # Write return addr
  #     EB 1F          jmp     0x0058E1AF
  #   0058E1CA:
  #     EB 06          jmp     0x0058E1D2
  mov     word [0x0058E1AC], 0xD7EB
  mov     byte [0x0058E185], 0x83
  mov     word [0x0058E186], 0x08C4
  mov     dword [0x0058E188], 0x448910FF
  mov     dword [0x0058E18C], 0x1FEB0C24
  mov     word [0x0058E1CA], 0x06EB

  # Reset protection flags so we can return to the game's code
  pop     eax
  call    eax  # VirtualProtect call enqueued earlier

  add     esp, 4  # prev_flags2 (unused)
  mov     edx, 0x00590CE3
  jmp     edx
