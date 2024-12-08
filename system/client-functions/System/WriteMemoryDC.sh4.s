.meta name="Write memory"
.meta description="Writes data to any location in memory"

entry_ptr:
reloc0:
  .offsetof start

start:
  mova    r0, [dest_addr]
  mov     r4, r0
  mov.l   r0, [r4]
  mov.l   r5, [r4 + 4]
  add     r4, 8
again:
  test    r5, r5
  bt      done
  mov.b   r6, [r4]
  mov.b   [r0], r6
  add     r4, 1
  add     r0, 1
  bs      again
  add     r5, -1
done:
  rets
  nop

  .align  4
dest_addr:
  .data   0
size:
  .data   0

data_to_write:
