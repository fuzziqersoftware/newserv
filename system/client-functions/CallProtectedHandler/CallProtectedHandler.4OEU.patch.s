.meta hide_from_patches_menu
.meta name="CallProtectedHandler"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  CallProtectedHandlerXB
  .data  0x007237E8  # should_allow_protected_commands
  .data  0x002DE000  # handle_6x(void* data @ ecx, uint32_t size @ eax)
size:
  .data  0x00000000
data:
