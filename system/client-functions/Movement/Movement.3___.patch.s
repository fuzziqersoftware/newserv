.meta name="Movement"
.meta description="Allow backsteps and\nmovement when\nenemies are nearby"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

.versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC

  .data     <VERS 0x801CE7AC 0x801CECC0 0x801D0D10 0x801CED8C 0x801CEBF0 0x801CEBF0 0x801CEDF0 0x801CF2AC>
  .data     0x00000004
  b         +0x0C

  .data     <VERS 0x801CF69C 0x801CFBB0 0x801D1CEC 0x801CFC7C 0x801CFAE0 0x801CFAE0 0x801CFCE0 0x801D019C>
  .data     0x00000004
  b         +0x14

  .data     0x00000000
  .data     0x00000000
