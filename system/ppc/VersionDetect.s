# This function returns the game version, with values more specific than can be
# detected by the sub_version field in various login commands.

# The returned value has the format 03GGRRVV, where:
#   G = game (Ox4F (O) = Episodes 1&2, 0x53 (S) = Episode 3)
#   R = region (0x45 (E), 0x4A (J), 0x50 (P))
#   V = minor version (0 = 1.00, 1 = 1.01, 2 = 1.02, etc.)

newserv_index_E3:

entry_ptr:
reloc0:
  .offsetof start

start:
  lis    r3, 0x8000
  lwz    r4, [r3]
  lbz    r5, [r3 + 7]
  li     r3, -1

  rlwinm r0, r4, 16, 16, 31
  cmplwi r0, 0x4750
  bnelr

  lis    r3, 0x3300
  rlwimi r3, r4, 8, 8, 23
  rlwimi r3, r5, 0, 24, 31
  ori    r3, r3, 0x0030

  blr
