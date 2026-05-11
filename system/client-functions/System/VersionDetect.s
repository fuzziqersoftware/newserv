# This function returns the game version, with values more specific than can be detected by the sub_version field in
# the various login commands (e.g. 9D/9E). We call this value specific_version in the codebase.

# The returned value has the format SSGGRRVV, where:
#   S = 31 = PSOv1, 32 = PSOv2, 33 = PSOv3, 34 = Xbox, 35 = BB
#   G = game (4F (O) = non-Ep3, 53 (S) = Ep3)
#   R = region (45 (E), 4A (J), or 50 (P))
#   V = minor version (meaning varies by major version)
# This results in a 4-character ASCII-printable version code which encodes all of the above information.

entry_ptr:
reloc0:
  .offsetof start



.versions SH4

start:
  mova    r0, [data_start]
  mov     r1, r0
  mov.l   r2, [r1]+  # target value
again:
  mov.l   r0, [r1]+  # candidate address
  cmpeq   r0, 0
  bt      done       # return 0 if no version matched
  mov.l   r0, [r0]   # value from candidate address
  cmpeq   r0, r2
  mov.l   r0, [r1]+  # specific_version from this match
  bf      again
  nop

done:
  rets
  nop

  .align  4
data_start:
  .data   0x61657244
  .data   0x8C239D78  # v1 NTE
  .data   0x314F4A31  # 1OJ1
  .data   0x8C24CA24  # v1 11/2000
  .data   0x314F4A32  # 1OJ2
  .data   0x8C2873AC  # v1 12/2000
  .data   0x314F4A33  # 1OJ3
  .data   0x8C28B04C  # v1 01/2001
  .data   0x314F4A34  # 1OJ4
  .data   0x8C291E34  # v1 JP
  .data   0x314F4A46  # 1OJF
  .data   0x8C28B924  # v1 USA
  .data   0x314F4546  # 1OEF
  .data   0x8C28B3F4  # v1 EU
  .data   0x314F5046  # 1OPF
  .data   0x8C2F3748  # v2 08/06/2001 AND v2 08/22/2001 (TODO: Find a way to tell these apart)
  .data   0x324F4A35  # 2OJ5
  .data   0x8C2F11D0  # v2 JP
  .data   0x324F4A46  # 2OJF
  .data   0x8C2F3738  # v2 USA
  .data   0x324F4546  # 2OEF
  .data   0x8C2E7CE0  # v2 EU
  .data   0x324F5046  # 2OPF
  .data   0x00000000  # end sentinel



.versions PPC

start:
  lis    r3, 0x8000
  lwz    r4, [r3]

  # For Trial Editions, set the V field to 54; for other versions, set it to 0x30 | disc_version
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



.versions X86

start:
  .include GetVersionInfoXB

  test     eax, eax
  jz       version_not_found
  mov      eax, [eax]
  ret

version_not_found:
  mov      eax, 0x344F0000
  ret
