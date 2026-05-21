# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

.meta visibility="all"
.meta name="Movement"
.meta description="Allow backsteps and\nmovement when\nenemies are nearby"


entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocks



  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

  .data     <VERS 0x801CE7AC 0x801CECC0 0x801D0D10 0x801CED8C 0x801CEBF0 0x801CEBF0 0x801CEDF0 0x801CF2AC>
  .data     0x00000004
  b         +0x0C

  .data     <VERS 0x801CF69C 0x801CFBB0 0x801D1CEC 0x801CFC7C 0x801CFAE0 0x801CFAE0 0x801CFCE0 0x801D019C>
  .data     0x00000004
  b         +0x14



  .versions 4OED 4OEU 4OJB 4OJD 4OJU 4OPD 4OPU

  .data     <VERS 0x00308E88 0x00308F08 0x003067D8 0x003073D8 0x00308F08 0x00308EB8 0x00309078>
  .deltaof  code_start, code_end
code_start:
  call      process_stick_value
  push      esi
  sub       esi, 4
  call      process_stick_value
  pop       esi
  jmp       code_end

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
code_end:



  .all_versions

  .data     0x00000000
  .data     0x00000000
