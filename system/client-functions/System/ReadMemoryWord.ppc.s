# This function is required for loading DOLs. If it's not present, newserv can't
# serve DOL files to GameCube clients.

.meta index=E0

entry_ptr:
reloc0:
  .offsetof start

start:
  mflr   r12
  bl     read
address:
  .zero
read:
  mflr   r3
  lwz    r3, [r3]
  lwz    r3, [r3]
  mtlr   r12
  blr
