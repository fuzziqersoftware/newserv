# This patch can't be used currently because the EU version of Episode 3 doesn't
# natively support B2, and there is no buffer-overflow exploit (yet) to address
# this.

.meta name="Get VIP card"
.meta description="Gives you a VIP card"

entry_ptr:
reloc0:
  .offsetof start

start:
  # Call seq_var_set(7000) - this gives the local player a VIP card
  li     r3, 7000
  lis    r0, 0x8010
  ori    r0, r0, 0xC1A4
  mtctr  r0
  bctr
