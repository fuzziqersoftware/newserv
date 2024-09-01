.meta name="VIP card"
.meta description="Gives you a VIP card"

entry_ptr:
reloc0:
  .offsetof start

start:
  # Call seq_var_set(7000) - this gives the local player a VIP card
  li     r3, 7000
  lis    r0, 0x8010
  ori    r0, r0, 0xF410
  mtctr  r0
  bctr
