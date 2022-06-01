# This macro clears the data and instruction caches at the beginning of each
# function. This is necessary because apparently some versions of PSO don't do
# this correctly by themselves.

# This macro expects to be run immediately at the entrypoint (usually the start
# label) for all functions. It returns the original return address in r12, and
# the address of the start label in r11.
  mflr   r12  # r12 = address to return to
  mfctr  r3  # r3 = address of start label (this code is called via bctrl)
  addi   r4, r3, 0x7C00  # r4 = end of relevant region
InitClearCaches__next_cache_block:
  dcbst  r0, r3
  sync
  icbi   r0, r3
  addi   r3, r3, 0x20
  cmpl   r3, r4
  blt    InitClearCaches__next_cache_block
  isync
