# This function returns the game version, with values more specific than can be
# detected by the sub_version field in the various login commands (e.g. 93/9D).

# The returned value has the format SSPPRRVV, where:
#   S = version (31 = PSOv1, 32 = PSOv2)
#   G = game (4F = PSO)
#   R = region (45 = E, 4A = J, 50 = P)
#   V = minor version (31 = NTE, 32 = 11/2000, 33 = 12/2000, 24 = 01/2001,
#       35 = 08/2001, 46 = not a prototype)
# This results in a 4-character ASCII-printable version code which encodes all
# of the above information. This value is called specific_version in the places
# where it's used by the server.

entry_ptr:
reloc0:
  .offsetof start

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
  .data   0x8C2F3748  # v2 08/2001
  .data   0x324F4A35  # 2OJ5
  .data   0x8C2F11D0  # v2 JP
  .data   0x324F4A46  # 2OJF
  .data   0x8C2F3738  # v2 USA
  .data   0x324F4546  # 2OEF
  .data   0x8C2E7CE0  # v2 EU
  .data   0x324F5046  # 2OPF
  .data   0x00000000  # end sentinel
