# This function is required for loading DOLs. If it's not present, newserv can't serve DOL files to GameCube clients.

# This is also the file I've chosen to document how to write code for newserv's functions subsystem. Client functions
# are assembly snippets written in the native language of the client, which can be sent to the client with the B2
# command. This is done at login time if the server administrator has enabled automatic patches in config.json or if
# the client has enabled certain patches in the Patches menu. Client functions can also be sent at any time with the
# $patch chat command, if they include .meta visibility (see below).

# This file is a general function (it does not appear in the Patches menu). General functions are used to implement
# various server operations; this one is used to write arbitrary data to the client's memory space. For example, to use
# this function to write the bytes 38 00 00 05 to the address 8010521C, send_function_call could be called like this:
#   auto fn = s->client_functions->get("WriteMemory", c->specific_version);
#   co_await send_function_call(
#       c,  // Client to send function call to
#       fn,  // The function's code
#       {{"dest_addr", 0x8010521C}, {"size", 4}},  // Variables to pass in to the function's code (see below)
#       "\x38\x00\x00\x05",  // Data to append after the code (not all functions use this)
#       4);  // Size of data to append after the code
# The meanings of label_writes and suffix are described in the comments below.

# The .versions directive is required for all client functions that can be called by the server or the player. This
# directive specifies which architectures or specific versions of the game the client function is compatible with. The
# version tokens may be specific game versions (e.g. 3OE1, 59NL) or architectures (PPC, X86, or SH4); in the latter
# case, the source applies to all versions which use that architecture. All lines after a .versions directive apply
# only to the specified versions; this set of "active" versions can be changed with another .versions
# directive later in the file, thereby splitting the file into different sections that apply to different sets of
# versions. Any lines in the file the appear before the first .versions directive apply to all versions. After a
# .versions directive, expressions like "VERS value1 value2 ..." (but with <> instead of "") can be used to specialize
# the patch for each version. In a VERS expression, the number of values must match the number of versions given in the
# .versions directive, and the values must appear in the same order. This function is implemented on all versions and
# all architecture, so we specify all architectures here. Later on, the implementations for each architecture are
# segregated via further .versions directives.
.versions SH4 PPC X86

# This directive controls where the function appears. The values are (note that the quotes are required):
#   visibility="hidden" (default): this function does not appear in the Patches menu and cannot be used via $patch
#   visibility="cheat": this function doesn't appear in the Patches menu but can be used via $patch if cheat mode is on
#   visibility="chat": this function doesn't appear in the Patches menu but can be used via $patch
#   visibility="menu": this function appears in the Patches menu but can't be used via $patch
#   visibility="all": this function appears in the Patches menu and can be used via $patch
# Note that if the client has $debug enabled, then all functions can be run via $patch regardless if this setting.
# .meta visibility="menu"

# This directive specifies what the function's internal name is. This is the name that can be used in config.json to
# require the patch for all clients, and is also the name used with the $patch command. If not specified, the
# function's internal name is the same as its filename without the .s extension.
# .meta key="WriteMemory"

# These directives tell newserv what to show to the player in the Patches menu. Neither of them is required; if the
# name is omitted, the filename is used instead. These have no real effect for this function (since .meta visibility is
# not used), so this is primarily for documentation purposes.
.meta name="Write memory"
.meta description="Writes data to any location in memory"

# When used for debugging purposes, it may be useful to see the value returned by the client function when run via the
# $patch chat command. This directive causes the server to tell you the return value in-game after running it.
# .meta show_return_value

# The entry_ptr label is required for all functions. It should generally point to a .offsetof directive that itself
# points to the actual entrypoint.
entry_ptr:
# All labels starting with reloc signify that the following PPC word (big-endian 32-bit value) is to be relocated at
# runtime. That is, when the code runs on the client, the PPC word will contain the actual memory address relative to
# the running code instead of the offset that it holds at assembly time. The entry_ptr label should almost always have
# a reloc label next to it.
reloc0:
  .offsetof start



# Everything following this directive (until the next .versions directive) applies only to PowerPC architectures. When
# this function is compiled for other architectures, this section will be ignored.
.versions PPC

start:
  mflr    r12
  bl      get_block_ptr
  mr      r6, r3        # r6 = address of dest_addr label

copy_block:
  lwz     r3, [r6]      # r3 = dest ptr
  subi    r3, r3, 1     # subtract 1 so we can use stbu
  lwz     r5, [r6 + 4]  # r5 = size (bytes remaining)
  add     r5, r5, r3    # r5 = dest end ptr (last byte to be written)
  addi    r4, r6, 7     # r4 = src ptr (starting at -1 so we can use lbzu)
copy_block__again:
  lbzu    r0, [r4 + 1]
  stbu    [r3 + 1], r0
  cmp     r3, r5
  bne     copy_block__again

  # Flush the data cache and clear the instruction cache at the written region
  lwz     r3, [r6]      # r3 = dest ptr
  lwz     r4, [r6 + 4]  # r4 = size
  # A .include directive essentially pastes in the code from the referenced file. Here, we use the code from the file
  # FlushCachedCode.inc.s. When compiling includes, newserv first looks in the same directory as the function's source,
  # then looks in system/client-functions/System.
  .include FlushCachedCode

  # Return the address after the last byte written. The value returned in r3 from the function is sent back to the
  # server in a B3 command. newserv uses the return value during DOL loading to know which section of the DOL file to
  # send next, or to send the RunDOL function if all sections have been loaded.
  lwz     r3, [r6]      # r3 = dest ptr
  lwz     r4, [r6 + 4]  # r4 = size
  add     r3, r3, r4
  mtlr    r12
  blr

get_block_ptr__ret:
  mflr    r3
  mtlr    r10
  blr
get_block_ptr:
  # We use a trick here to get the address of the dest_addr label: since bl puts the immediately-following address into
  # the link register, we "call" get_block_ptr__ret and get the dest_addr pointer out of the LR. We then put r10 back
  # into the LR so get_block_ptr__ret returns to the caller.
  mflr    r10
  bl      get_block_ptr__ret



.versions SH4

start:
  mova    r0, [dest_addr]
  mov     r4, r0
  mov.l   r0, [r4]
  mov.l   r5, [r4 + 4]
  add     r4, 8
again:
  test    r5, r5
  bt      done
  mov.b   r6, [r4]
  mov.b   [r0], r6
  add     r4, 1
  add     r0, 1
  bs      again
  add     r5, -1
done:
  rets
  nop

  .align  4



.versions X86

start:
  jmp     get_block_ptr
get_block_ptr_ret:
  xchg    ebx, [esp]
  mov     eax, [ebx]
  mov     ecx, [ebx + 4]
  add     ebx, 8

again:
  test    ecx, ecx
  jz      done
  mov     dl, [ebx]
  mov     [eax], dl
  inc     ebx
  inc     eax
  dec     ecx
  jmp     again

done:
  pop     ebx
  ret

get_block_ptr:
  call    get_block_ptr_ret



# This last section applies to all architectures, so we re-enable all versions again. This directive also disables the
# use of VERS tokens.
.all_versions

# These fields are filled in right before the command is sent to the client. Specifically, the label_writes argument to
# send_function_call is responsible for this. The label_writes argument is a map of label name to value, and it simply
# writes the given values to the locations of the given labels before sending the function to the client. (Notice that
# these label names match the keys in the map passed in the example at the beginning of this file.) This is a way to
# pass arbitrary arguments to a function at call time.
dest_addr:
  .data   0  # There must be space (32 bits) allocated for the actual value after the label, hence these placeholders
size:
  .data   0

# Finally, we use the suffix argument to instruct send_function_call to append the data we want to write to memory
# immediately after the assembled code. (The data_to_write label here is for documentation purposes only; the suffix
# argument always appends data after the end of all the assembled code.)
data_to_write:
