# This function returns the game version, with values more specific than can be
# detected by the sub_version field in the various login commands (e.g. 9D/9E).

# The returned value has the format SSGGRRVV, where:
#   S = 33 (which represents PSO GC)
#   G = game (4F (O) = Ep1&2, 53 (S) = Ep3)
#   R = region (45 (E), 4A (J), or 50 (P))
#   V = minor version | 30 (30 = 1.0, 31 = 1.1, 32 = 1.2, etc.), or 54 for NTE
# This results in a 4-character ASCII-printable version code which encodes all
# of the above information. This value is called specific_version in the places
# where it's used by the server.

entry_ptr:
reloc0:
  .offsetof start

start:
  lis    r3, 0x8000
  lwz    r4, [r3]

  # For Trial Editions, set the V field to 54; for other versions, set it to
  # 0x30 | disc_version
  rlwinm r0, r4, 8, 24, 31
  cmplwi r0, 0x47  # Check if high byte of game ID is 'G'
  beq    not_trial
  cmplwi r0, 0x44  # Check if high byte of game ID is 'D'
  beq    is_nte
  li     r3, 0
  blr
is_nte:
  li     r3, 0x0054
  b      end_trial_check
not_trial:
  lbz    r3, [r3 + 7]
  ori    r3, r3, 0x0030
end_trial_check:
  oris   r3, r3, 0x3300  # Set high byte ('3')
  rlwimi r3, r4, 8, 8, 23  # Set middle two bytes to last two bytes of game ID
  blr
