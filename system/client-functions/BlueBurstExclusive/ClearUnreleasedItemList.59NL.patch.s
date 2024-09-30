# It would be a bad idea to remove `.meta hide_from_patches_menu` to make this
# patch an option for players to be able to select; either all players on the
# server should have this patch, or none should have it.

# This patch clears the list of unreleased items on the client, so the client
# never creates buggy items when the server generates an item that wasn't
# released on the official servers.

.meta name="Clear unreleased item list"
.meta description=""
.meta hide_from_patches_menu

entry_ptr:
reloc0:
  .offsetof start
start:
  xor       eax, eax
  mov       edx, esp
  mov       esp, 0x009F81B0
  mov       ecx, 0x3C
again:
  push      0
  dec       ecx
  jnz       again
  mov       esp, edx
  ret
