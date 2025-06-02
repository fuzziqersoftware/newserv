.meta name="Blinking SD"
.meta description="Makes the Shifta\nand Deband status\nicons flicker when\nthey are about\nto run out"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049
# Xbox port by fuzziqersoftware

.versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksXB

  .data     <VERS 0x0027782A 0x00277ADA 0x00277EEA 0x00277C0A 0x00277D2A 0x00277C2A 0x00277DFA>
  .data     5
  .binary   <VERS E807190600 E827220600 E8E7360600 E897340600 E877380600 E8A7340600 E827380600>

  .data     <VERS 0x002D9136 0x002D9D06 0x002DB5D6 0x002DB0A6 0x002DB5A6 0x002DB0D6 0x002DB626>
  .deltaof  code_start, code_end
code_start:
  push      eax
  mov       ecx, [esp + 0x20]
  test      cl, 0x02  # Slot 2 or 3
  jz        skip
  mov       eax, [eax + 0x28]
  test      eax, eax
  jz        skip
  mov       eax, [eax]
  test      eax, eax
  jz        skip

  mov       edx, [esp + 0x24]  # client ID
  mov       edx, dword [<VERS 0x0062CCFC 0x0062D29C 0x00634DD4 0x006322BC 0x00631B54 0x006322BC 0x00632654> + edx * 4]
  imul      ecx, ecx, 0x0C
  mov       edx, [edx + ecx + 0x264]
  cmp       edx, 450
  jb        decr_alpha

  mov       byte [eax + 0x002F], 0xFF
  jmp       skip

decr_alpha:
  xor       edx, edx
  lea       ecx, [edx - 1]
  mov       dl, [eax + 0x002F]
  sub       edx, 8
  cmovl     edx, ecx
  mov       [eax + 0x002F], dl

skip:
  pop       eax
  pop       edx
  lea       ecx, [edx + 0x20]
  cmp       dword [eax + 0x6C], ebp
  cmovz     edx, ecx
  jmp       edx
code_end:

  .data     0
  .data     0
