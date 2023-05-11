  # r3 = dest ptr
  # r4 = src ptr
  # r5 = size
  # Clobbers r0, r3, r4, r5
  addi    r5, r5, 3
  rlwinm  r5, r5, 30, 2, 31 # r5 = number of words to copy
  mtctr   r5
  subi    r3, r3, 4 # r3 = r3 - 4 (so we can use stwu)
  subi    r4, r4, 4 # r4 = r4 - 4 (so we can use lwzu)
copy_word_again:
  lwzu    r0, [r4 + 4]
  stwu    [r3 + 4], r0
  bdnz    copy_word_again

  rlwinm  r4, r5, 2, 0, 29
  addi    r3, r3, 4
  sub     r3, r3, r4
  .include FlushCachedCode
