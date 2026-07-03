# Original code by Ralf @ GC-Forever
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049
# Xbox port by fuzziqersoftware

.meta visibility="all"
.meta name="Item pickup"



entry_ptr:
reloc0:
  .data   start
start:



  .versions 50YJ 59NJ 59NL

  .meta description="Prevents picking\nup items unless you\nhold the shift key"

  .label    imp_GetModuleHandleA, <VERS 0x008EC1B4 0x008F61B0 0x008F81F0>
  .label    imp_GetProcAddress, <VERS 0x008EC134 0x008F6130 0x008F812C>

  pop       ecx
  push      6
  push      <VERS 0x00683DC5 0x006893A9 0x0068933D>
  push      patch_code_end - patch_code
  call      patch_code_end

patch_code:  # [eax/](TObjPlayer* this @ ebp) -> flags @ edx
  push      eax

  call      patch_code_get_dll_name
  .binary   "user32"00
patch_code_get_dll_name:
  call      dword [imp_GetModuleHandleA]
  test      eax, eax
  jz        patch_code_flag_check

  call      patch_code_get_function_name
  .binary   "GetAsyncKeyState"00
patch_code_get_function_name:
  push      eax
  call      dword [imp_GetProcAddress]
  test      eax, eax
  jz        patch_code_flag_check

  push      0x10  # VK_SHIFT (see https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes)
  call      eax
  not       eax

patch_code_flag_check:
  # bit 0x8000 clear = key is down (or user32 missing, or GetAsyncKeyState missing)
  mov       dl, 0x20
  test      ax, 0x8000
  cmovz     edx, dword [ebp + 0x0328]
  pop       eax
  ret

patch_code_end:
  push      ecx
  .include  WriteCallToCode



  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

  .meta description="Prevents picking\nup items unless you\nhold the Z button"

  .include  WriteCodeBlocks

  .label    hook_loc, 0x8000B938
  .data     hook_loc
  .data     hook_end - hook_start
  .address  hook_loc
hook_start:
  addi      r3, r28, 0x0550
  li        r4, 0x0100
  bl        <VERS 0x8034ED18 0x8035011C 0x80351684 0x8035142C 0x8034FBCC 0x8034FC10 0x803517F8 0x80350BEC>
  cmpwi     r3, 0
  beq       skip
  mr        r3, r28
  b         <VERS 0x801B56B4 0x801B5B08 0x801B7CC0 0x801B5BD4 0x801B5AA0 0x801B5AA0 0x801B5C38 0x801B60F4>
skip:
  b         <VERS 0x801B56C4 0x801B5B18 0x801B7CD0 0x801B5BE4 0x801B5AB0 0x801B5AB0 0x801B5C48 0x801B6104>
hook_end:

  .label    hook_call, <VERS 0x801B56B0 0x801B5B04 0x801B7CBC 0x801B5BD0 0x801B5A9C 0x801B5A9C 0x801B5C34 0x801B60F0>
  .data     hook_call
  .data     0x00000004
  .address  hook_call
  b         hook_start

  # Prevent Z from opening the main menu
  .data     <VERS 0x8024C384 0x8024CDD0 0x8024DD28 0x8024DAC4 0x8024CC0C 0x8024CC0C 0x8024DD88 0x8024D5D0>
  .data     0x00000004
  li        r4, 0x0008



  .versions 4OED 4OEU 4OJB 4OJD 4OJU 4OPD 4OPU

  .meta description="Prevents picking\nup items unless you\nhold the black or\nwhite button"

  .include  WriteCodeBlocks

  .data     <VERS 0x001FDC99 0x001FDC99 0x001FDA89 0x001FDBE9 0x001FDE69 0x001FDCB9 0x001FDD29>
  .data     0x07
  .binary   E8880100009090

  .data     <VERS 0x001FDE26 0x001FDE26 0x001FDC16 0x001FDD76 0x001FDFF6 0x001FDE46 0x001FDEB6>
  .data     0x0A
  .binary   8B866C05000085C0EB46

  .data     <VERS 0x001FDE76 0x001FDE76 0x001FDC66 0x001FDDC6 0x001FE046 0x001FDE96 0x001FDF06>
  .data     0x09
  .binary   74038A40013408EB46

  .data     <VERS 0x001FDEC5 0x001FDEC5 0x001FDCB5 0x001FDE15 0x001FE095 0x001FDEE5 0x001FDF55>
  .data     0x0A
  .binary   7507F68624030000E0C3

  .data     <VERS 0x0025ADAD 0x0025AEED 0x0025A94D 0x0025ACCD 0x0025B07D 0x0025ADCD 0x0025AF1D>
  .data     0x01
  .binary   00



  .all_versions

  .data     0x00000000
  .data     0x00000000
