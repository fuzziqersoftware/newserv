  mova    r0, [first_patch_header]
  mov     r7, r0       # r7 = read ptr
  xor     r3, r3
  dec     r3
  shl     r3, 2        # r3 = 0xFFFFFFFC (mask for aligning r7)
apply_patch:
  add     r7, 3
  and     r7, r3       # r7 = (r7 + 3) & (~3) (align to 4-byte boundary)
  mov.l   r4, [r7]+    # r4 = dest addr
  mov.l   r5, [r7]+
  add     r5, r4       # r5 = dest end ptr (dest addr + size)
  cmpeq   r4, r5       # if (size == 0) return
  bt      done

again:
  cmpeq   r4, r5
  bt      apply_patch  # if (r4 == r5) done with the patch; go to next header
  mov.b   r0, [r7]+
  mov.b   [r4], r0     # *(r4) = *(r7++);
  bs      again        # r4++; continue
  add     r4, 1

done:
  rets
  nop

  .align 4
first_patch_header:
