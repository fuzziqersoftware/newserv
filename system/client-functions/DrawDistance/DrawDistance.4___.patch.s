.meta name="Draw Distance"
.meta description="Extends the draw\ndistance of many\nobjects"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049
# Xbox port by fuzziqersoftware

.versions 4OED 4OEU 4OJB 4OJD 4OJU 4OPD 4OPU

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksXB

  .data     <VERS 0x001737C2 0x001737D2 0x00173692 0x00173782 0x00173862 0x001737E2 0x00173792>  # From 3OE1:80100B8C
  .deltaof  p1_1s, p1_1e
p1_1s:
  .binary   E87B020000  # call p1_2s
  nop
p1_1e:
  .data     <VERS 0x00173A42 0x00173A52 0x00173912 0x00173A02 0x00173AE2 0x00173A62 0x00173A12>
  .deltaof  p1_2s, p1_2e
p1_2s:
  fld       st0, dword [esp + 0x1C]
  fadd      st0, st0
  fchs      st0
  ret
p1_2e:

  .data     <VERS 0x001A3DEF 0x001A3EEF 0x001A3BBF 0x001A3DBF 0x001A3FDF 0x001A3E0F 0x001A3ECF>  # From 3OE1:80156AD8
  .deltaof  p2_1s, p2_1e
p2_1s:
  .binary   E844000000  # call p2_2s
p2_1e:
  .data     <VERS 0x001A3E38 0x001A3F38 0x001A3C08 0x001A3E08 0x001A4028 0x001A3E58 0x001A3F18>
  .deltaof  p2_2s, p2_2e
p2_2s:
  fld       st0, dword [ecx + 0x1C]
  fadd      st0, st0
  fld       st0, st1
  ret
p2_2e:

  .data     <VERS 0x002D2DC8 0x002D3148 0x002D0E68 0x002D1A28 0x002D32F8 0x002D2DF8 0x002D31C8>  # From 3OE1:801A2040
  .deltaof  p3_1s, p3_1e
p3_1s:
  .binary   E8DA000000  # call p3_2s
  nop
p3_1e:
  .data     <VERS 0x002D2EA7 0x002D3227 0x002D0F47 0x002D1B07 0x002D33D7 0x002D2ED7 0x002D32A7>
  .deltaof  p3_2s, p3_2e
p3_2s:
  fld       st0, dword [esp + 0x24]
  fadd      st0, st0
  fchs      st0
  ret
p3_2e:

  .data     <VERS 0x00156AC8 0x002D32A8 0x001569E8 0x00156A78 0x00156AB8 0x00156AE8 0x002D3328>  # From 3OE1:801A2240
  .deltaof  p4_1s, p4_1e
p4_1s:
  .binary   <VERS E877010000 E807010000 E877010000 E877010000 E877010000 E877010000 E807010000>  # call p4_2s
  nop
p4_1e:
  .data     <VERS 0x00156C44 0x002D33B4 0x00156B64 0x00156BF4 0x00156C34 0x00156C64 0x002D3434>
  .deltaof  p4_2s, p4_2e
p4_2s:
  fld       st0, dword [esp + 0x28]
  fadd      st0, st0
  fchs      st0
  ret
p4_2e:

  .data     <VERS 0x002E2B93 0x002E2E8C 0x002E0C33 0x002E17B3 0x002E2E6C 0x002E2BC3 0x002E2EBC>  # From 3OE1:80205840
  .deltaof  p5_1s, p5_1e
p5_1s:
  .binary   <VERS E8EA000000 E840010000 E8EA000000 E8EA000000 E840010000 E8EA000000 E840010000>  # call p5_3s
p5_1e:
  .data     <VERS 0x002E1FD1 0x002E2404 0x002E0071 0x002E0BF1 0x002E23E4 0x002E2001 0x002E2434>  # From 3OE1:80205FE4
  .deltaof  p5_2s, p5_2e
p5_2s:
  .binary   <VERS E8AC0C0000 E8C80B0000 E8AC0C0000 E8AC0C0000 E8C80B0000 E8AC0C0000 E8C80B0000>  # call p5_3s
p5_2e:
  .data     <VERS 0x002E2C82 0x002E2FD1 0x002E0D22 0x002E18A2 0x002E2FB1 0x002E2CB2 0x002E3001>
  .deltaof  p5_3s, p5_3e
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
