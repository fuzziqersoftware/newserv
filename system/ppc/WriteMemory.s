# This example shows how to use newserv's send_function_call function for PSO
# GameCube clients. This code writes a variable-length block of data to a
# specified address in the client's memory.

# For example, to write the bytes 38 00 00 05 to the address 8010521C,
# send_function_call could be called like this:
#   auto fn = s->function_code_index->name_to_function.at("WriteMemory");
#   unordered_map<string, uint32_t label_writes(
#       {{"dest_addr", 0x8010521C}, {"size", 4}});
#   string suffix("\x38\x00\x00\x05", 4);
#   send_function_call(
#       c,  // Client to send function call to
#       fn,  // The function's code
#       label_writes,  // Variables to pass in to the function's code
#       suffix);  // Data to append after the code (not all functions use this)
# The meanings of label_writes and suffix are described in the comments below.

# A label newserv_id_XX tells newserv what value to use in the flag field when
# sending the B2 command. This is needed if the server needs to do something
# when the B3 response is received.
newserv_id_C0:

# The entry_ptr label is required. It should point to a .offsetof directive that
# itself points to the actual entrypoint.
entry_ptr:
# All labels starting with reloc signify that the following PPC word
# (be_uint32_t) is to be relocated at runtime. That is, when the code is run,
# the PPC word will contain the actual memory address relative to the running
# code instead of the offset that it holds at assembly time. The entry_ptr label
# should almost always have a reloc label next to it.
reloc0:
  .offsetof start

copy_block:
  # r8 = address to return to (LR, from start label)
  mflr    r6            # r6 = address of dest_addr label
  mtlr    r8
  lwz     r3, [r6]      # r3 = dest ptr
  subi    r3, r3, 1     # subtract 1 so we can use stbu
  lwz     r5, [r6 + 4]  # r5 = size (bytes remaining)
  add     r5, r5, r3    # r5 = dest end ptr
  addi    r4, r6, 7     # r4 = src ptr (starting at -1 so we can use lbzu)

copy_block__again:
  lbzu    r0, [r4 + 1]
  stbu    [r3 + 1], r0
  cmp     r3, r5
  bne     copy_block__again

  lwz     r3, [r6]      # r3 = dest ptr
  lwz     r4, [r6 + 4]  # r4 = size

  # Flush the data cache and clear the instruction cache at the written region
  lis     r5, 0xFFFF
  ori     r5, r5, 0xFFF1
  and     r5, r5, r3
  subf    r3, r5, r3
  add     r4, r4, r3
flush_cached_code_writes__again:
  dcbst   r0, r5
  sync
  icbi    r0, r5
  addic   r5, r5, 8
  subic.  r4, r4, 8
  bge     flush_cached_code_writes__again
  isync

  # Return 0 (this value appears in the B3 command)
  li      r3, 0
  blr

start:
  # We use a trick here to get the address of the dest_addr label: since bl puts
  # the immediately-following address into the link register, we "call"
  # copy_block and get the dest_addr pointer out of the LR. We then put r8 back
  # into the LR so copy_block can return normally.
  mflr    r8
  bl      copy_block

# These fields are filled in when the B2 command is generated. Specifically, the
# label_writes argument to send_function_call is responsible for this.
dest_addr:
  .zero
size:
  .zero

# The data to be written is appended here at B2 construction time via the suffix
# argument to send_function_call. (This label is for documentation purposes
# only; the suffix argument always appends data after the end of all the
# assembled code.)
data_to_write:
