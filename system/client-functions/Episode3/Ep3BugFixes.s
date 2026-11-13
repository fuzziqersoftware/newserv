.meta visibility="all"
.meta name="Ep3 bugfixes"
.meta description=""

.versions 3SJ0 3SE0 3SP0

entry_ptr:
reloc0:
  .data   start

start:
  .include  WriteCodeBlocks

  # Fix 3rd opponent phase display position in 3v1 battles
  .data     <VERS 0x8047A930 0x8047B650 0x8047DA20>
  .data     4
  .data     0xB4

  .data     0x00000000
  .data     0x00000000
