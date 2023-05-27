# This function returns the game version, with values more specific than can be
# detected by the sub_version field in the various login commands (e.g. 9D/9E).

# The returned value has the format SSGGRRVV, where:
#   S = 33 (which represents PSO GC)
#   G = game (4F (O) = Episodes 1&2, 53 (S) = Episode 3)
#   R = region (45 (E), 4A (J), or 50 (P))
#   V = minor version | 30 (30 = 1.00, 31 = 1.01, 32 = 1.02, etc.)
# This results in a 4-character ASCII-printable version code which encodes all
# of the above information. This value is called specific_version in the places
# where it's used by the server.

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
