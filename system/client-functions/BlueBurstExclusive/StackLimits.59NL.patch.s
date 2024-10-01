# It would be a bad idea to remove `.meta hide_from_patches_menu` to make this
# patch an option for players to be able to select; either all players on the
# server should have this patch, or none should have it.

# If you change the stack limits in config.json away from the defaults, you
# should change the limits array below to match config.json and add this patch
# to the BBRequiredPatches list.

.meta name="Item stacks"
.meta description=""
.meta hide_from_patches_menu

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksBB

  # Patch 1: rewrite item_is_stackable
  .data     0x005C502C
  .deltaof  item_is_stackable_start, item_is_stackable_end

item_is_stackable_start:
  mov       eax, [esp + 4]
  cmp       al, 4
  je        return_1

  cmp       al, 3
  jne       return_0

  mov       ah, [esp + 8]
  push      eax
  mov       ecx, esp

  .binary   E8EC130100  # call max_stack_size_for_tool_start
  pop       ecx
  cmp       eax, 1
  jg        return_1
  # Fallthrough to return_0

return_0:
  xor       eax, eax
  ret
return_1:
  xor       eax, eax
  inc       eax
  ret
item_is_stackable_end:

  # Patch 2: rewrite max_stack_size_for_tool
  .data     0x005D6430
  .deltaof  max_stack_size_for_tool_start, max_stack_size_for_tool_end

max_stack_size_for_tool_start:
  xor       eax, eax
  inc       eax

  # if (data1[0] != 3) return 1
  cmp       byte [ecx], 3
  jne       not_tool2

  # declare return values array
  call   data_end
  # This array specifies the stack limits for each tool class. The array index
  # is the second byte of the item data (see names-v4.json for the values; for
  # e.g. tech disks this would be 02). For classes beyond 15, the value for 15
  # is used.
  # Index:  00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 10 11 12 13 14 15
  .binary   0A 0A 01 0A 0A 0A 0A 0A 0A 01 01 01 01 01 01 01 63 01 01 01 01 01
data_end:

  # eax = min<uint8_t>(data1[1], 0x15)
  mov       al, [ecx + 1]
  xor       edx, edx
  mov       dl, 0x15
  cmp       eax, edx
  cmovg     eax, edx

  # return data[eax]
  pop       edx
  mov       al, [edx + eax]
not_tool2:
  ret
max_stack_size_for_tool_end:

  .data     0x00000000
  .data     0x00000000
