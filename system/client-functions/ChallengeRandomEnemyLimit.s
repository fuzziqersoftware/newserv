# This patch changes the Challenge mode random enemy location table limit from 32 to any number up to 127. As written,
# it changes the limit to 100; to use a different limit, change the value of the count label below.

.meta visibility="hidden"
.meta name="Challenge random enemy limit"

entry_ptr:
reloc0:
  .data   start
start:
  .include  WriteCodeBlocks

  .label    count, 100



  # TODO: Make a 2OJ4 version of this patch
  .versions 2OJ5 2OJF 2OEF 2OPF
  .align    4
  .data     <VERS 0x8C2501B2 0x8C24E98E 0x8C2501A2 0x8C244C7E>
  .data     2
  mov       r5, count
  .align    4

  .versions 2OJW 2OJZ 50YJ 59NJ 59NL
  .data     <VERS 0x005AA2FE 0x005A908E 0x008075C3 0x0080FA3F 0x0080ECB7>
  .data     4
  .data     (count * 4)
  .data     <VERS 0x005AA30C 0x005A909D 0x008075DC 0x0080FA58 0x0080ECD0>
  .data     4
  .data     count

  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0
  .data     <VERS 0x80208C4C 0x802094C0 0x8020A5A8 0x80209FFC 0x80209448 0x80209448 0x8020A330 0x80209D2C>
  .data     4
  li        r4, count

  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU
  .data     <VERS 0x002E527C 0x002E5DFC 0x002E740C 0x002E71DC 0x002E742C 0x002E720C 0x002E745C>
  .data     4
  .data     count



  .all_versions

  .data     0x00000000
  .data     0x00000000
