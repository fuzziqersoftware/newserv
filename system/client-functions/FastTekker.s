.meta visibility="all"
.meta name="Fast tekker"
.meta description="Skips wind-up sound\nat tekker window"


entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocks



  .versions 1OJ1 1OJ2 1OJ3 1OJ4 1OJF 1OEF 1OPF 2OJ4 2OJ5 2OJF 2OEF 2OPF

  .align 4
  .data     <VERS 0x8C15B0CA 0x8C162302 0x8C175E66 0x8C1780AE 0x8C17600E 0x8C17863E 0x8C1783FA 0x8C19BD4A 0x8C19BD4A 0x8C19ADB6 0x8C19BD4A 0x8C19B7E2>
  .data     0x00000002
  mov       r1, 1

  .align 4
  .data     <VERS 0x8C15B0E6 0x8C16231E 0x8C175E82 0x8C1780CA 0x8C17602A 0x8C17865A 0x8C178416 0x8C19BD66 0x8C19BD66 0x8C19ADD2 0x8C19BD66 0x8C19B7FE>
  .data     0x00000002
  nop

  .align 4



  .versions 3OJT 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

  .data     <VERS 0x8026FAE8 0x8021F8CC 0x80220250 0x80221154 0x80220EF0 0x80220170 0x80220170 0x80221224 0x80220ABC>
  .data     4
  li        r0, 1

  .data     <VERS 0x8026FB10 0x8021F8F4 0x80220278 0x8022117C 0x80220F18 0x80220198 0x80220198 0x8022124C 0x80220AE4>
  .data     4
  nop



  .versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU

  .data     <VERS 0x0023EC5C 0x0023EEAC 0x0023F21C 0x0023EF3C 0x0023F0BC 0x0023EF5C 0x0023F14C>
  .deltaof  patch1_start, patch1_end
patch1_start:
  mov       dword [ebp + 0x14C], 1
patch1_end:

  .data     <VERS 0x0023EC77 0x0023EEC7 0x0023F237 0x0023EF57 0x0023F0D7 0x0023EF77 0x0023F167>
  .deltaof  patch2_start, patch2_end
patch2_start:
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
patch2_end:



  .versions 59NJ 59NL

  .data     <VERS 0x006DA14B 0x006DA113>
  .deltaof  patch1_start, patch1_end
patch1_start:
  mov       dword [edi + 0x14C], 1
patch1_end:

  .data     <VERS 0x006DA168 0x006DA130>
  .deltaof  patch2_start, patch2_end
patch2_start:
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  nop
patch2_end:



  .all_versions

  .data     0x00000000
  .data     0x00000000
