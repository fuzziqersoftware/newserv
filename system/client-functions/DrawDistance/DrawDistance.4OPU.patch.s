.meta name="Draw Distance"
.meta description="Extends the draw\ndistance of many\nobjects"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049
# Xbox port by fuzziqersoftware

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksXB

  .data     0x00173792  # From 3OE1:80100B8C
  .deltaof  p1_1s, p1_1e
p1_1s:
  .binary   E87B020000  # call p1_2s
  nop
p1_1e:
  .data     0x00173A12
  .deltaof  p1_2s, p1_2e
p1_2s:
  fld       st0, dword [esp + 0x20]
  fadd      st0, st0
  fchs      st0
  ret
p1_2e:

  .data     0x001A3ECF  # From 3OE1:80156AD8
  .deltaof  p2_1s, p2_1e
p2_1s:
  .binary   E844000000  # call p2_2s
p2_1e:
  .data     0x001A3F18
  .deltaof  p2_2s, p2_2e
p2_2s:
  fld       st0, dword [ecx + 0x1C]
  fadd      st0, st0
  fld       st0, st1
  ret
p2_2e:

  .data     0x002D31C8  # From 3OE1:801A2040
  .deltaof  p3_1s, p3_1e
p3_1s:
  .binary   E8DA000000  # call p3_2s
  nop
p3_1e:
  .data     0x002D32A7
  .deltaof  p3_2s, p3_2e
p3_2s:
  fld       st0, dword [esp + 0x24]
  fadd      st0, st0
  fchs      st0
  ret
p3_2e:

  .data     0x002D3328  # From 3OE1:801A2240
  .deltaof  p4_1s, p4_1e
p4_1s:
  .binary   E807010000  # call p4_2s
  nop
p4_1e:
  .data     0x002D3434
  .deltaof  p4_2s, p4_2e
p4_2s:
  fld       st0, dword [esp + 0x28]
  fadd      st0, st0
  fchs      st0
  ret
p4_2e:

  .data     0x002E2EBC  # From 3OE1:80205840
  .deltaof  p5_1s, p5_1e
p5_1s:
  .binary   E840010000  # call p5_3s
p5_1e:
  .data     0x002E2434  # From 3OE1:80205FE4
  .deltaof  p5_2s, p5_2e
p5_2s:
  .binary   E8C80B0000  # call p5_3s
p5_2e:
  .data     0x002E3001
  .deltaof  p5_3s, p5_3e
p5_3s:
  fld       st0, dword [eax + 0x0C]
  fadd      st0, st0
  fstp      dword [eax + 0x0C], st0
  mov       eax, [0x0053AD6C]
  ret
p5_3e:

  .data     0x00492440  # From 3OE1:805CFCD0
  .data     0x00000004
  .data     0x47AFC800

  .data     0x0042D450  # From 3OE1:805D0B7C
  .data     0x00000004
  .data     0x437A0000

  .data     0x004925CC  # From 3OE1:805D11A4
  .data     0x00000004
  .data     0x491C4000

  .data     0x0042BBE8  # From 3OE1:805D2030
  .data     0x00000004
  .data     0x47AFC800

  .data     0x001D98A6  # From 3OE1:805D25C0
  .data     0x00000004
  .data     0x44AF0000

  .data     0x001D98B8  # From 3OE1:805D25C0
  .data     0x00000004
  .data     0x44AF0000

  .data     0x00000000
  .data     0x00000000
