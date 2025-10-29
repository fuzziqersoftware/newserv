.meta name="Fast tekker"
.meta description="Skips wind-up sound\nat tekker window"

.versions 3OJT 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC

  .data     <VERS 0x8026FAE8 0x8021F8CC 0x80220250 0x80221154 0x80220EF0 0x80220170 0x80220170 0x80221224 0x80220ABC>
  .data     4
  li        r0, 1

  .data     <VERS 0x8026FB10 0x8021F8F4 0x80220278 0x8022117C 0x80220F18 0x80220198 0x80220198 0x8022124C 0x80220AE4>
  .data     4
  nop

  .data     0x00000000
  .data     0x00000000
