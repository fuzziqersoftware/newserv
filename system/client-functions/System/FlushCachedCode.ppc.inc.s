# This code flushes the data cache and invalidates the instruction cache for a
# block of newly-written code in memory.
# Arguments:
#   r3 = address of written code
#   r4 = number of bytes
# Returns: nothing
# Overwrites: r3, r4, r5
  lis     r5, 0xFFFF
  ori     r5, r5, 0xFFF1
  and     r5, r5, r3
  subf    r3, r5, r3
  add     r4, r4, r3
flush_cached_code_writes__again:
  dcbst   r0, r5
  sync
  icbi    r0, r5
  addic   r5, r5, 8
  subic.  r4, r4, 8
  bge     flush_cached_code_writes__again
  isync
