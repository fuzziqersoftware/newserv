# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049
# Xbox port by fuzziqersoftware

# BB notes:
# Currently beta quality, map objects that fade like boxes, and Pioneer's background billboards and elevators still
# have regular draw distance.
# TODO: 90% of stuff is included, bring home the last 10%.

.meta visibility="all"
.meta name="Draw Distance"
.meta description="Extends the draw\ndistance of many\nobjects"

entry_ptr:
reloc0:
  .offsetof start



.versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

start:
  .include  WriteCodeBlocks

  .data     0x8000DFA0
  .deltaof  hook_start, hook_end
  .address  0x8000DFA0
hook_start:
hook1:
  lfs       f30, [r2 - <VERS 0x3E08 0x3E08 0x3E08 0x3E08 0x3E00 0x3E00 0x3E00 0x3E00>]
  fmuls     f30, f30, f1
  blr
hook2:
  lfs       f2, [r2 - <VERS 0x3E08 0x3E08 0x3E08 0x3E08 0x3E00 0x3E00 0x3E00 0x3E00>]
  lfs       f0, [r30 + 0x001C]
  fmuls     f0, f0, f2
  blr
hook3:
  lfs       f28, [r2 - <VERS 0x3E08 0x3E08 0x3E08 0x3E08 0x3E00 0x3E00 0x3E00 0x3E00>]
  fmuls     f28, f28, f2
  blr
hook4:
  lfs       f0, [r2 - <VERS 0x3E08 0x3E08 0x3E08 0x3E08 0x3E00 0x3E00 0x3E00 0x3E00>]
  lfs       f1, [r3 + 0x000C]
  fmuls     f0, f0, f1
  stfs      [r3 + 0x000C], f0
  lis       r3, <VERS 0x804C 0x804C 0x804D 0x804D 0x804C 0x804C 0x804D 0x804D>
  blr
hook_end:

  .data     <VERS 0x801008E8 0x80100AD0 0x80100B74 0x80100A50 0x80100B8C 0x80100B8C 0x80100A60 0x80100C50>
  .data     0x00000004
  .address  <VERS 0x801008E8 0x80100AD0 0x80100B74 0x80100A50 0x80100B8C 0x80100B8C 0x80100A60 0x80100C50>
  bl        hook1

  .data     <VERS 0x8015671C 0x80156AD0 0x80156C34 0x80156B94 0x80156AD8 0x80156AD8 0x80156BF8 0x801570BC>
  .data     0x00000004
  .address  <VERS 0x8015671C 0x80156AD0 0x80156C34 0x80156B94 0x80156AD8 0x80156AD8 0x80156BF8 0x801570BC>
  bl        hook2

  .data     <VERS 0x801A1C64 0x801A203C 0x801A21A0 0x801A2100 0x801A2040 0x801A2040 0x801A2164 0x801A2628>
  .data     0x00000004
  .address  <VERS 0x801A1C64 0x801A203C 0x801A21A0 0x801A2100 0x801A2040 0x801A2040 0x801A2164 0x801A2628>
  bl        hook3

  .data     <VERS 0x801A1E64 0x801A223C 0x801A23A0 0x801A2300 0x801A2240 0x801A2240 0x801A2364 0x801A2828>
  .data     0x00000004
  .address  <VERS 0x801A1E64 0x801A223C 0x801A23A0 0x801A2300 0x801A2240 0x801A2240 0x801A2364 0x801A2828>
  bl        hook1

  .data     <VERS 0x80205044 0x802058B8 0x80206640 0x802063F4 0x80205840 0x80205840 0x80206728 0x80206124>
  .data     0x00000004
  .address  <VERS 0x80205044 0x802058B8 0x80206640 0x802063F4 0x80205840 0x80205840 0x80206728 0x80206124>
  bl        hook4

  .data     <VERS 0x802057E8 0x8020605C 0x80206DE4 0x80206B98 0x80205FE4 0x80205FE4 0x80206ECC 0x802068C8>
  .data     0x00000004
  .address  <VERS 0x802057E8 0x8020605C 0x80206DE4 0x80206B98 0x80205FE4 0x80205FE4 0x80206ECC 0x802068C8>
  bl        hook4

  .data     <VERS 0x805C83A8 0x805D29A8 0x805D9E48 0x805D9BE8 0x805C8CB0 0x805CFCD0 0x805D94F0 0x805D5730>
  .data     0x00000004
  .float    90000

  .data     <VERS 0x805C9254 0x805D3854 0x805DACF4 0x805DAA94 0x805C9B5C 0x805D0B7C 0x805DA39C 0x805D65DC>
  .data     0x00000004
  .float    62500

  .data     <VERS 0x805C987C 0x805D3E7C 0x805DB31C 0x805DB0BC 0x805CA184 0x805D11A4 0x805DA9C4 0x805D6C04>
  .data     0x00000004
  .float    640000

  .data     <VERS 0x805CA708 0x805D4D08 0x805DC1A8 0x805DBF48 0x805CB010 0x805D2030 0x805DB850 0x805D7A90>
  .data     0x00000004
  .float    90000

  .data     <VERS 0x805CAC98 0x805D5298 0x805DC738 0x805DC4D8 0x805CB5A0 0x805D25C0 0x805DBDE0 0x805D8020>
  .data     0x00000004
  .float    1400

  .data     0x00000000
  .data     0x00000000



.versions 4OED 4OEU 4OJB 4OJD 4OJU 4OPD 4OPU

start:
  .include  WriteCodeBlocks

  .data     <VERS 0x001737C2 0x001737D2 0x00173692 0x00173782 0x00173862 0x001737E2 0x00173792>  # From 3OE1:80100B8C
  .deltaof  p1_1s, p1_1e
  .address  <VERS 0x001737C2 0x001737D2 0x00173692 0x00173782 0x00173862 0x001737E2 0x00173792>  # From 3OE1:80100B8C
p1_1s:
  call      p1_2s
  nop
p1_1e:
  .data     <VERS 0x00173A42 0x00173A52 0x00173912 0x00173A02 0x00173AE2 0x00173A62 0x00173A12>
  .deltaof  p1_2s, p1_2e
  .address  <VERS 0x00173A42 0x00173A52 0x00173912 0x00173A02 0x00173AE2 0x00173A62 0x00173A12>
p1_2s:
  fld       st0, dword [esp + 0x1C]
  fadd      st0, st0
  fchs      st0
  ret
p1_2e:

  .data     <VERS 0x001A3DEF 0x001A3EEF 0x001A3BBF 0x001A3DBF 0x001A3FDF 0x001A3E0F 0x001A3ECF>  # From 3OE1:80156AD8
  .deltaof  p2_1s, p2_1e
  .address  <VERS 0x001A3DEF 0x001A3EEF 0x001A3BBF 0x001A3DBF 0x001A3FDF 0x001A3E0F 0x001A3ECF>  # From 3OE1:80156AD8
p2_1s:
  call      p2_2s
p2_1e:
  .data     <VERS 0x001A3E38 0x001A3F38 0x001A3C08 0x001A3E08 0x001A4028 0x001A3E58 0x001A3F18>
  .deltaof  p2_2s, p2_2e
  .address  <VERS 0x001A3E38 0x001A3F38 0x001A3C08 0x001A3E08 0x001A4028 0x001A3E58 0x001A3F18>
p2_2s:
  fld       st0, dword [ecx + 0x1C]
  fadd      st0, st0
  fld       st0, st1
  ret
p2_2e:

  .data     <VERS 0x002D2DC8 0x002D3148 0x002D0E68 0x002D1A28 0x002D32F8 0x002D2DF8 0x002D31C8>  # From 3OE1:801A2040
  .deltaof  p3_1s, p3_1e
  .address  <VERS 0x002D2DC8 0x002D3148 0x002D0E68 0x002D1A28 0x002D32F8 0x002D2DF8 0x002D31C8>  # From 3OE1:801A2040
p3_1s:
  call      p3_2s
  nop
p3_1e:
  .data     <VERS 0x002D2EA7 0x002D3227 0x002D0F47 0x002D1B07 0x002D33D7 0x002D2ED7 0x002D32A7>
  .deltaof  p3_2s, p3_2e
  .address  <VERS 0x002D2EA7 0x002D3227 0x002D0F47 0x002D1B07 0x002D33D7 0x002D2ED7 0x002D32A7>
p3_2s:
  fld       st0, dword [esp + 0x24]
  fadd      st0, st0
  fchs      st0
  ret
p3_2e:

  .data     <VERS 0x00156AC8 0x002D32A8 0x001569E8 0x00156A78 0x00156AB8 0x00156AE8 0x002D3328>  # From 3OE1:801A2240
  .deltaof  p4_1s, p4_1e
  .address  <VERS 0x00156AC8 0x002D32A8 0x001569E8 0x00156A78 0x00156AB8 0x00156AE8 0x002D3328>  # From 3OE1:801A2240
p4_1s:
  call      p4_2s
  nop
p4_1e:
  .data     <VERS 0x00156C44 0x002D33B4 0x00156B64 0x00156BF4 0x00156C34 0x00156C64 0x002D3434>
  .deltaof  p4_2s, p4_2e
  .address  <VERS 0x00156C44 0x002D33B4 0x00156B64 0x00156BF4 0x00156C34 0x00156C64 0x002D3434>
p4_2s:
  fld       st0, dword [esp + 0x28]
  fadd      st0, st0
  fchs      st0
  ret
p4_2e:

  .data     <VERS 0x002E2B93 0x002E2E8C 0x002E0C33 0x002E17B3 0x002E2E6C 0x002E2BC3 0x002E2EBC>  # From 3OE1:80205840
  .deltaof  p5_1s, p5_1e
  .address  <VERS 0x002E2B93 0x002E2E8C 0x002E0C33 0x002E17B3 0x002E2E6C 0x002E2BC3 0x002E2EBC>  # From 3OE1:80205840
p5_1s:
  call      p5_3s
p5_1e:
  .data     <VERS 0x002E1FD1 0x002E2404 0x002E0071 0x002E0BF1 0x002E23E4 0x002E2001 0x002E2434>  # From 3OE1:80205FE4
  .deltaof  p5_2s, p5_2e
  .address  <VERS 0x002E1FD1 0x002E2404 0x002E0071 0x002E0BF1 0x002E23E4 0x002E2001 0x002E2434>  # From 3OE1:80205FE4
p5_2s:
  call      p5_3s
p5_2e:
  .data     <VERS 0x002E2C82 0x002E2FD1 0x002E0D22 0x002E18A2 0x002E2FB1 0x002E2CB2 0x002E3001>
  .deltaof  p5_3s, p5_3e
  .address  <VERS 0x002E2C82 0x002E2FD1 0x002E0D22 0x002E18A2 0x002E2FB1 0x002E2CB2 0x002E3001>
p5_3s:
  fld       st0, dword [eax + 0x0C]
  fadd      st0, st0
  fstp      dword [eax + 0x0C], st0
  mov       eax, [<VERS 0x0053A9CC 0x0053A26C 0x00535BAC 0x0053622C 0x0053D54C 0x0053A9CC 0x0053AD6C>]
  ret
p5_3e:

  .data     <VERS 0x004920A0 0x00491940 0x0048D4F0 0x0048DC88 0x00494C30 0x004920A8 0x00492440>  # From 3OE1:805CFCD0
  .data     0x00000004
  .data     0x47AFC800

  .data     <VERS 0x0042D0A0 0x0042C940 0x00428DC0 0x00429130 0x0042C940 0x0042D0C0 0x0042D450>  # From 3OE1:805D0B7C
  .data     0x00000004
  .data     0x437A0000

  .data     <VERS 0x0049222C 0x00491ACC 0x0048D67C 0x0048DE14 0x00494DBC 0x00492234 0x004925CC>  # From 3OE1:805D11A4
  .data     0x00000004
  .data     0x491C4000

  .data     <VERS 0x0042B838 0x0042B0D8 0x00427558 0x004278C8 0x0042B0D8 0x0042B858 0x0042BBE8>  # From 3OE1:805D2030
  .data     0x00000004
  .data     0x47AFC800

  .data     <VERS 0x001D9736 0x001D9936 0x001D95F6 0x001D9746 0x001D9BC6 0x001D9756 0x001D98A6>  # From 3OE1:805D25C0
  .data     0x00000004
  .data     0x44AF0000

  .data     <VERS 0x001D9748 0x001D9948 0x001D9608 0x001D9758 0x001D9BD8 0x001D9768 0x001D98B8>  # From 3OE1:805D25C0
  .data     0x00000004
  .data     0x44AF0000

  .data     0x00000000
  .data     0x00000000



.versions 59NJ 59NL

write_call_func:
  .include  WriteCallToCode

start:
  mov       eax, 0x41800000        # Environment clip distance mod 16.0f
  mov       [<VERS 0x0097D198 0x0097F1B8>], eax      # This affects mostly static map objects
  mov       [<VERS 0x0097D19C 0x0097F1BC>], eax
  mov       [<VERS 0x0097D1A0 0x0097F1C0>], eax

  mov       ax, 0x9090
  mov       [<VERS 0x00689BC7 0x00689B5B>], ax       # Players draw distance 10000.0f always
  mov       eax, 0x41000000                          # Use newly acquired skipped branch room
  mov       [<VERS 0x00689BD1 0x00689B65>], eax      # to store our float multiplier

  call      patch_func_1           # Floor items
  call      patch_func_2           # Whole bunch of stuff, including NPCs
  call      patch_func_3           # Duplicate function from above, reuse same hook
  call      patch_func_4           # TODO: Which objects this affects?
  call      patch_func_5           # TODO: This one too?
  call      patch_func_6           # TODO: And this one?
  ret

# Floor items
patch_func_1:
  pop       ecx
  push      8
  push      <VERS 0x005C525B 0x005C5267>
  call      get_code_size1
  .deltaof  patch_code1, patch_code_end1
get_code_size1:
  pop       eax
  push      dword [eax]
  call      patch_code_end1
patch_code1:
  mov       edx, [esp + 0x18]
  fld       st0, dword [<VERS 0x00689BD1 0x00689B65>]
  fld       st0, dword [esp + 0x14]
  fmulp     st1, st0
  ret
patch_code_end1:
  push      ecx
  jmp       write_call_func

# Whole bunch of stuff, including NPCs
patch_func_2:
  pop       ecx
  push      9
  push      <VERS 0x007BB21E 0x007BA472>
  call      get_code_size2
  .deltaof  patch_code2, patch_code_end2
get_code_size2:
  pop       eax
  push      dword [eax]
  call      patch_code_end2
patch_code2:
  test      eax, 0x400
  fld       st0, dword [<VERS 0x00689BD1 0x00689B65>]
  fld       st0, dword [esp + 0x2C]
  fmulp     st1, st0
  ret
patch_code_end2:
  push      ecx
  jmp       write_call_func

# Duplicate function from above, reuse same hook
patch_func_3:
  mov       eax, dword [<VERS 0x007BB21F 0x007BA473>]
  add       eax, 0x002A1C74
  mov       dword [<VERS 0x00518843 0x005187FF>], eax
  mov       byte [<VERS 0x00518842 0x005187FE>], 0xE8
  mov       dword [<VERS 0x00518847 0x00518803>], 0x90909090
  ret

# TOComputerMachine01
patch_func_4:
  pop       ecx
  push      7
  push      <VERS 0x00616FF4 0x00616FFC>
  call      get_code_size4
  .deltaof  patch_code4, patch_code_end4
get_code_size4:
  pop       eax
  push      dword [eax]
  call      patch_code_end4
patch_code4:
  lea       edx, [edi + 0x38]
  fld       st0, dword [<VERS 0x00689BD1 0x00689B65>]
  fld       st0, dword [esp + 0x14]
  fmulp     st1, st0
  ret
patch_code_end4:
  push      ecx
  jmp       write_call_func

# TObjCamera
patch_func_5:
  pop       ecx
  push      6
  push      <VERS 0x006439A8 0x0064394C>
  call      get_code_size5
  .deltaof  patch_code5, patch_code_end5
get_code_size5:
  pop       eax
  push      dword [eax]
  call      patch_code_end5
patch_code5:
  fld       st0, dword [<VERS 0x00689BD1 0x00689B65>]
  fld       st0, dword [esp + 0x28]
  fmulp     st1, st0
  fchs      st0
  ret
patch_code_end5:
  push      ecx
  jmp       write_call_func

# TODO: And this one?
patch_func_6:
  pop       ecx
  push      6
  push      <VERS 0x0065B959 0x0065B985>
  call      get_code_size6
  .deltaof  patch_code6, patch_code_end6
get_code_size6:
  pop       eax
  push      dword [eax]
  call      patch_code_end6
patch_code6:
  mov       ebp, ecx
  fld       st0, dword [<VERS 0x00689BD1 0x00689B65>]
  fld       st0, dword [esp + 0x30]
  fmulp     st1, st0
  ret
patch_code_end6:
  push      ecx
  jmp       write_call_func
