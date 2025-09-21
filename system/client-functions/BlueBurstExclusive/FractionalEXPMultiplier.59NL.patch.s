# This patch changes the 6xDD command to support fractional multipliers.

.meta name="Fractional EXP multiplier"
.meta description=""
.meta hide_from_patches_menu

entry_ptr:
reloc0:
  .offsetof start
start:
  call      install_hook
  call      apply_static_patches
  fild      st0, dword [0x009F9EE0]
  fstp      dword [0x009F9EE0], st0
  ret



install_hook:
  pop       ecx
  push      7
  push      0x0078747E
  call      get_code_size
  .deltaof  hook_start, hook_end
get_code_size:
  pop       eax
  push      dword [eax]
  call      hook_end
hook_start:  # [eax, ebx]() -> void
  push      edx
  fild      st0, dword [esp]
  fld       st0, dword [0x009F9EE0]
  fmulp     st1, st0
  fistp     dword [esp], st0
  pop       edx
  ret
hook_end:
  push      ecx
  .include  WriteCallToCode-59NL



apply_static_patches:
  .include WriteCodeBlocksBB
  .data    0x00787998
  .deltaof handle_6xDD_start, handle_6xDD_end
handle_6xDD_start:  # [std](G_6xDD* cmd @ [esp + 4]) -> void
  mov      eax, [esp + 4]
  test     eax, eax
  je       handle_6xDD_ret
  cmp      byte [eax + 1], 1
  jg       handle_6xDD_use_float
  fild     st0, word [eax + 2]
  jmp      handle_6xDD_write_float
handle_6xDD_use_float:
  fld      st0, dword [eax + 4]
handle_6xDD_write_float:
  fstp     dword [0x009F9EE0], st0
handle_6xDD_ret:
  ret
handle_6xDD_end:
  .data    0x00000000
  .data    0x00000000
