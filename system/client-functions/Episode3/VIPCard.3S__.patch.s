.meta name="VIP card"
.meta description="Gives you a VIP card"

.versions 3SJT 3SJ0 3SE0 3SP0

entry_ptr:
reloc0:
  .offsetof start

start:
  # Call seq_var_set(7000) - this gives the local player a VIP card
  li     r3, 7000
  lis    r0, 0x8010
  ori    r0, r0, <VERS 0xF410 0xBED8 0xBD18 0xC1A4>
  mtctr  r0
  bctr
