# This function returns the game version, with values more specific than can be
# detected by the sub_version field in the various login commands (e.g. 9D/9E).

# The returned value has the format SSSSRRVV, where:
#   S = 344F (which represents PSO Xbox)
#   R = region (45 (E), 4A (J), or 50 (P))
#   V = version (42 (B) for beta, 44 (D) for disc, 55 (U) for title update)
# This results in a 4-character ASCII-printable version code which encodes all
# of the above information. This value is called specific_version in the places
# where it's used by the server.

entry_ptr:
reloc0:
  .offsetof start

start:
  .include VersionDetectWithPatchFunctionsXB
  ret
