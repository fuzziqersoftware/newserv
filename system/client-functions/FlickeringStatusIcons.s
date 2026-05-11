# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

.meta visibility="all"
.meta name="Blinking SD"
.meta description="Makes the Shifta\nand Deband status\nicons flicker when\nthey are about\nto run out"


entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocks



  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

  .data     0x8000B86C
  .data     0x00000054
  .address  0x8000B86C
code_start:
  mr        r3, r0
  andi.     r0, r31, 2
  beqlr
  lwz       r4, [r3 + 0x0028]
  cmplwi    r4, 0
  beqlr
  lwz       r4, [r4]
  cmplwi    r4, 0
  beqlr
  mulli     r0, r31, 12
  add       r5, r29, r0
  lwz       r6, [r5 + 0x025C]
  cmplwi    r6, 450
  bge       full_intensity
  lbz       r6, [r4 + 0x002C]
  subi      r6, r6, 0x0008
  cmpwi     r6, 0
  bge       not_full_intensity
full_intensity:
  li        r6, 0x00FF
not_full_intensity:
  stb       [r4 + 0x002C], r6
  blr
code_end:

  .data     <VERS 0x8026DF94 0x8026EC58 0x8026FCB4 0x8026FA68 0x8026E7F4 0x8026E7F4 0x8026FC1C 0x8026F464>
  .data     4
  .address  <VERS 0x8026DF94 0x8026EC58 0x8026FCB4 0x8026FA68 0x8026E7F4 0x8026E7F4 0x8026FC1C 0x8026F464>
  bl        code_start



  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU

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



  .all_versions

  .data     0
  .data     0
