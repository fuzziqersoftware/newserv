.meta name="Fast tekker"
.meta description="Skips wind-up sound\nat tekker window"

.versions 1OJ1 1OJ2 1OJ3 1OJ4 1OJF 1OEF 1OPF 2OJ5 2OJF 2OEF 2OPF

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksDC

  .align 4
  .data     <VERS 0x8C15B0CA 0x8C162302 0x8C175E66 0x8C1780AE 0x8C17600E 0x8C17863E 0x8C1783FA 0x8C19BD4A 0x8C19ADB6 0x8C19BD4A 0x8C19B7E2>
  .data     0x00000002
  mov       r1, 1

  .align 4
  .data     <VERS 0x8C15B0E6 0x8C16231E 0x8C175E82 0x8C1780CA 0x8C17602A 0x8C17865A 0x8C178416 0x8C19BD66 0x8C19ADD2 0x8C19BD66 0x8C19B7FE>
  .data     0x00000002
  nop

  .align 4
  .data     0x00000000
  .data     0x00000000
