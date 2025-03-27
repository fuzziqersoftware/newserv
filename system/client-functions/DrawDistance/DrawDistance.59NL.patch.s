# Currently beta quality, map objects that fade like boxes, and Pioneer's
# background billboards and elevators still have regular draw distance.
# TODO: 90% of stuff is included, bring home the last 10%.

.meta name="Draw Distance"
.meta description="Extends the draw\ndistance of many\nobjects"

entry_ptr:
reloc0:
  .offsetof start

write_call_func:
  .include  WriteCallToCode-59NL

start:
  mov       eax, 0x41800000        # Environment clip distance mod 16.0f
  mov       [0x0097F1B8], eax      # This affects mostly static map objects
  mov       [0x0097F1BC], eax
  mov       [0x0097F1C0], eax

  mov       ax, 0x9090
  mov       [0x00689B5B], ax       # Players draw distance 10000.0f always
  mov       eax, 0x41000000        # Use newly acquired skipped branch room
  mov       [0x00689B65], eax      # to store our float multiplier

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
  push      0x005C5267
  call      get_code_size1
  .deltaof  patch_code1, patch_code_end1
get_code_size1:
  pop       eax
  push      dword [eax]
  call      patch_code_end1
patch_code1:
  mov       edx, [esp + 0x18]
  fld       st0, dword [0x00689B65]
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
  push      0x007BA472
  call      get_code_size2
  .deltaof  patch_code2, patch_code_end2
get_code_size2:
  pop       eax
  push      dword [eax]
  call      patch_code_end2
patch_code2:
  test      eax, 0x400
  fld       st0, dword [0x00689B65]
  fld       st0, dword [esp + 0x2C]
  fmulp     st1, st0
  ret
patch_code_end2:
  push      ecx
  jmp       write_call_func

# Duplicate function from above, reuse same hook
patch_func_3:
  mov       eax, dword [0x007BA473]
  add       eax, 0x002A1C74
  mov       dword [0x005187FF], eax
  mov       byte [0x005187FE], 0xE8
  mov       dword [0x00518803], 0x90909090
  ret

# TODO: Which objects this affects?
patch_func_4:
  pop       ecx
  push      7
  push      0x00616FFC
  call      get_code_size4
  .deltaof  patch_code4, patch_code_end4
get_code_size4:
  pop       eax
  push      dword [eax]
  call      patch_code_end4
patch_code4:
  lea       edx, [edi + 0x38]
  fld       st0, dword [0x00689B65]
  fld       st0, dword [esp + 0x14]
  fmulp     st1, st0
  ret
patch_code_end4:
  push      ecx
  jmp       write_call_func

# TODO: This one too?
patch_func_5:
  pop       ecx
  push      6
  push      0x0064394C
  call      get_code_size5
  .deltaof  patch_code5, patch_code_end5
get_code_size5:
  pop       eax
  push      dword [eax]
  call      patch_code_end5
patch_code5:
  fld       st0, dword [0x00689B65]
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
  push      0x0065B985
  call      get_code_size6
  .deltaof  patch_code6, patch_code_end6
get_code_size6:
  pop       eax
  push      dword [eax]
  call      patch_code_end6
patch_code6:
  mov       ebp, ecx
  fld       st0, dword [0x00689B65]
  fld       st0, dword [esp + 0x30]
  fmulp     st1, st0
  ret
patch_code_end6:
  push      ecx
  jmp       write_call_func
