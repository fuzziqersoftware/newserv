# This patch changes the number of BB character save slots from 4 to any number
# up to 127.

# This patch is for documentation purposes only; it works when used as a server
# patch via newserv, but is decidedly inconvenient to use via this method. This
# is because it affects logic that runs before any patches can be sent by the
# server, so the player has to connect once to get the patch, then disconnect
# and connect again to use the additional slots.

# As written, this patch changes the slot count from 4 to 12. To use a
# different slot count, first compute the following values:
#   slot count = your desired number of player slots (must be >= 4, <= 127)
#   total file size = (slot count * 0x2EA4) + 0x14
#   bgm_test_songs_unlocked offset = total file size - 0x10
#   save_count offset = total file size - 8
#   round2_seed offset = total file size - 4
# Then, for each of the above, search for the string to the left of the = sign
# and change the values used in all of the matching lines.

.meta name="More save slots"
.meta description=""
.meta hide_from_patches_menu

entry_ptr:
reloc0:
  .offsetof start

  # Include a few functions first
write_call_to_code:
  .include  WriteCallToCode-59NL
memcpy:
  .include  CopyData
  ret



start:
  # Apply all necessary patches
  call      apply_enable_scroll_patch
  call      apply_fix_scroll_patch1
  call      apply_fix_scroll_patch2
  call      apply_fix_file_index
  call      apply_preview_window_fix
  call      apply_static_patches
  # Rewrite the existing char file regions to have the appropriate size; this
  # must be done after the patches are applied because we call the checksum
  # function, which is patched by one of the above calls
  call      update_existing_char_file_list
  jmp       update_existing_char_file_list_memcard



apply_enable_scroll_patch:
  # This patch enables scrolling behavior within the character list
  push      -5          # Jump size (negative = jmp instead of call)
  push      0x00413B7F  # Jump address
  call      get_code_size_for_enable_scroll
  .deltaof  enable_scroll_start, enable_scroll_end
get_code_size_for_enable_scroll:
  pop       eax
  push      dword [eax]
  call      enable_scroll_end
enable_scroll_start:
  mov       eax, dword ptr [edi + 0x28]  # cursor = char_select_menu->cursor_obj (TAdSelectCurGC*)
  or        dword [eax + 0x01F8], 3  # cursor->flags |= 3  # Enable scrolling
  mov       eax, [0x00A3B050]  # scroll_bar = TAdScrollBarXb_objs[0]
  mov       ecx, [eax + 0xEC]  # ecx = scroll_bar->client_id
  imul      ecx, ecx, 0x24
  # Set up scroll bar graphics (in struct at scroll_bar + 0x1C)
  # TODO: Even though we set this up the same way PSO Xbox does, it still
  # doesn't render. Figure this out and fix it.
  mov       dword [eax + ecx + 0x1C], 0x439D0000
  mov       dword [eax + ecx + 0x20], 0x43360000
  mov       dword [eax + ecx + 0x24], 0x439D0000
  mov       dword [eax + ecx + 0x28], 0x4392AB85
  mov       dword [eax + ecx + 0x2C], 0x40400000
  mov       dword [eax + ecx + 0x30], 0x425EA3D7
  mov       dword [eax + ecx + 0x34], 0x00000008
  mov       dword [eax + ecx + 0x38], 0x00000000
  mov       dword [eax + ecx + 0x2C], 0x00000000
  or        dword [eax + 0xF0], 1  # scroll_bar->flags |= 1
  mov       ecx, [eax + 0xEC]
  shl       ecx, 4
  mov       dword [eax + ecx + 0xAC], 0  # scroll_bar->selection_state[client_id].scroll_offset = 0
  mov       dword [eax + ecx + 0xB0], 0  # scroll_bar->selection_state[client_id].selected_index = 0
  mov       dword [eax + ecx + 0xB4], 4  # scroll_bar->selection_state[client_id].num_items_in_view = 4
  mov       dword [eax + ecx + 0xB8], 0x0B  # scroll_bar->selection_state[client_id].last_item_index = (slot count - 1)
  pop       edi
  ret
enable_scroll_end:
  call      write_call_to_code
  ret



apply_fix_scroll_patch1:
  # This patch fixes character selection cursor object so it will take the
  # scroll offset into account
  push      6           # Call size
  push      0x00413C38  # Call address
  call      get_code_size_for_fix_scroll_patch1
  .deltaof  fix_scroll_patch1_start, fix_scroll_patch1_end
get_code_size_for_fix_scroll_patch1:
  pop       eax
  push      dword [eax]
  call      fix_scroll_patch1_end
fix_scroll_patch1_start:
  mov       edx, [edi + 0x28]  # cursor = this->ad_select_cur_obj (TAdSelectCurGC*)
  mov       ebp, [edx + 0x44]  # ebp = cursor->selected_index_within_view
  mov       eax, [0x00A3B050]  # scroll_bar = TAdScrollBarXb_objs[0]
  add       ebp, [eax + 0xAC]  # ebp += scroll_bar->selection_state[0].scroll_offset
  ret
fix_scroll_patch1_end:
  call      write_call_to_code
  ret



apply_fix_scroll_patch2:
  # This patch changes the TAdSinglePlyChrSelectGC::selected_index_within_view
  # to be the selected character's absolute index (including scroll_offset),
  # not the index only within to the displayed four characters
  push      6           # Call size
  push      0x00413CD8  # Call address
  call      get_code_size_for_fix_scroll_patch2
  .deltaof  fix_scroll_patch2_start, fix_scroll_patch2_end
get_code_size_for_fix_scroll_patch2:
  pop       eax
  push      dword [eax]
  call      fix_scroll_patch2_end
fix_scroll_patch2_start:
  mov       eax, [0x00A3B050]  # scroll_bar = TAdScrollBarXb_objs[0]
  mov       eax, [eax + 0xAC]  # eax = scroll_bar->selection_state[0].scroll_offset
  mov       edx, [edi + 0x28]  # cursor = this->ad_select_cur_obj (TAdSelectCurGC*)
  add       eax, [edx + 0x44]  # eax += cursor->selected_index_within_view
  ret
fix_scroll_patch2_end:
  call      write_call_to_code
  ret



apply_fix_file_index:
  # This patch fixes the character file indexing so it will account for the
  # scroll position
  push      5           # Call size
  push      0x00413CF0  # Call address
  call      get_code_size_for_selection_index_fix2
  .deltaof  selection_index_fix2_start, selection_index_fix2_end
get_code_size_for_selection_index_fix2:
  pop       eax
  push      dword [eax]
  call      selection_index_fix2_end
selection_index_fix2_start:
  mov       eax, [0x00A3B050]
  mov       eax, [eax + 0xAC]  # eax = TAdScrollBarXb_objs[0]->selection_state[0].scroll_offset
  add       ebp, eax  # arg0 += eax
  mov       [esp + 4], ebp
  mov       eax, 0x006C1A80
  jmp       eax  # set_current_char_slot
selection_index_fix2_end:
  call      write_call_to_code
  ret



apply_preview_window_fix:
  # This patch fixes the preview display so it will show the correct section
  # ID, etc.
  push      5           # Call size
  push      0x0040216C  # Call address
  call      get_code_size_for_preview_window_fix
  .deltaof  preview_window_fix_start, preview_window_fix_end
get_code_size_for_preview_window_fix:
  pop       eax
  push      dword [eax]
  call      preview_window_fix_end
preview_window_fix_start:
  mov       eax, [0x00A3B050]  # scroll_bar = TAdScrollBarXb_objs[0]
  mov       eax, [eax + 0xAC]  # eax = scroll_bar->selection_state[0].scroll_offset
  add       [esp + 4], eax
  mov       eax, 0x006C44D0  # get_player_preview_info
  jmp       eax
preview_window_fix_end:
  # This patch applies in two places, so push the second set of args now, then
  # apply it twice
  push      5                   # Call size
  push      0x00401842          # Call address
  push      dword [esp + 0x10]  # Code size
  push      dword [esp + 0x10]  # Code address
  call      write_call_to_code
  call      write_call_to_code
  ret



apply_static_patches:
  .include WriteCodeBlocksBB
  # These patches change various places where the character data size and slot
  # count are referenced
  .data    0x004751A4
  .data    0x00000001
  .binary  0C  # slot count; TDataProtocol::handle_E5
  .data    0x0047525B
  .data    0x00000001
  .binary  0C  # slot count; import_player_preview
  .data    0x004785E1
  .data    0x00000001
  .binary  0C  # slot count; TDataProtocol::handle_E4
  .data    0x0048242D
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C17BF
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C1CCB
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C1CFE
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C1D1C
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C1DD7
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C222E
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C226D
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C228E
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C229E
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C24DB
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C2643
  .data    0x00000004
  .data    0x00022FBC  # save_count offset
  .data    0x006C264D
  .data    0x00000004
  .data    0x00022FBC  # save_count offset
  .data    0x006C26EF
  .data    0x00000004
  .data    0x00022FBC  # save_count offset
  .data    0x006C2705
  .data    0x00000004
  .data    0x00022FC0  # round2_seed offset
  .data    0x006C2793
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C286C
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C3113
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C353F
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C357E
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C35AA
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C35B7
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C35D2
  .data    0x00000004
  .data    0x00022FBC  # save_count offset
  .data    0x006C35DB
  .data    0x00000004
  .data    0x00022FBC  # save_count offset
  .data    0x006C36E0
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C3B1E
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C4209
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C47EF
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C4826
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C4962
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C4999
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C4A81
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C4ABA
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C4C9A
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C4CD1
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C4DB9
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C4DF2
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C4F58
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C4F94
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C5181
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C51BD
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C5332
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C536C
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C5501
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C553D
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C56B2
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C56EC
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C5872
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C58AC
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C5A41
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C5A7D
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C5B6E
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C5BA8
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C5D2E
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C5D68
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C5EEE
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C5F28
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C60AE
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C60E8
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C6303
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C633D
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C64C1
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C64FD
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C65EE
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C6628
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C67AE
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C67E8
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C696E
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C69A8
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C6B43
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C6B74
  .data    0x00000004
  .data    0x0000005D  # memcard block count
  .data    0x006C6BF6
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C6C30
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C6E3E
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C6E78
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C7075
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C70AF
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C7A02
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C7D22
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x006C7D5E
  .data    0x00000001
  .binary  0C  # slot count
  .data    0x006C7D7C
  .data    0x00000004
  .data    0x00022FC4  # total file size
  .data    0x0077BE92
  .data    0x00000004
  .data    0x00022FB4  # bgm_test_songs_unlocked offset

  # Signature check on all save files (rewritten as loop)
  .data    0x006C1C2D
  .deltaof sig_check_begin, sig_check_end
sig_check_begin:
  mov      edx, 0xC87ED5B1   # Expected signature value
  add      eax, 0x04E8       # &char_file_list->chars[0].part2.signature
  mov      ecx, 0x0C         # slot count
again:
  cmp      dword [eax], 0    # signature == 0 (no char in slot)
  je       sig_ok
  cmp      dword [eax], edx  # signature == expected value
  jne      sig_bad
sig_ok:
  add      eax, 0x2EA4       # Advance to next slot
  dec      ecx
  jnz      again
  xor      eax, eax          # All signatures OK (eax = 0)
  jmp      sig_check_end
sig_bad:
  xor      eax, eax          # Bad signature (eax = 1)
  inc      eax
  jmp      sig_check_end
  .binary  CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC
sig_check_end:  # 006C1C76

  # Show slot number in each menu item
  .data    0x00401D57
  .deltaof show_slot_number_begin, show_slot_number_end
show_slot_number_begin:
  # Original call (sprintf(line_buf, "LV%d", preview_info->visual.disp.level + 1))
  lea      edx, [esp + 0x02C4]
  mov      ebx, [ebx + 8]
  inc      ebx
  push     ebx
  mov      ecx, esi
  push     edx
  mov      eax, 0x00402604
  call     eax
  # Find the end of the string
  lea      eax, [esp + 0x02C4]
show_slot_number_strend_again:
  cmp      word [eax], 0
  je       show_slot_number_strend_done
  add      eax, 2
  jmp      show_slot_number_strend_again
show_slot_number_strend_done:
  # Format the slot number and append it to the string
  mov      ecx, [0x00A3B050]  # scroll_bar = TAdScrollBarXb_objs[0]
  mov      ecx, [ecx + 0xAC]  # ecx = scroll_bar->selection_state[0].scroll_offset
  lea      ecx, [ecx + ebp + 1]
  push     ecx  # Slot number (scroll_offset + z)
  call     get_show_slot_number_suffix_fmt
  .binary  2000280023002500640029000000  # L" (#%d)"
get_show_slot_number_suffix_fmt:
  push     eax  # Destination buffer
  mov      eax, 0x00857E29  # _swprintf
  call     eax
  add      esp, 0x0C
  jmp      show_slot_number_end
  .zero    0x98
show_slot_number_end:  # 00401E4D

  # End static patches
  .data    0x00000000
  .data    0x00000000



update_existing_char_file_list:
  # Replace the existing character list with an appropriately-longer one. This
  # part does not need to be done if the patch is applied statically to the
  # executable; this is only necessary when used as a server patch because the
  # character list is already allocated at the time the patch is applied.
  push      0x00022FC4  # total file size
  mov       eax, 0x008581C5  # operator_new
  call      eax
  add       esp, 4
  mov       edx, [0x00A95E44]  # edx = old char_file_list
  mov       [0x00A95E44], eax
  mov       ecx, [edx + 0xBA94]  # Copy bgm_test_songs_unlocked_high to new file
  mov       [eax + 0x00022FB4], ecx
  mov       ecx, [edx + 0xBA98]  # Copy bgm_test_songs_unlocked_low to new file
  mov       [eax + 0x00022FB8], ecx
  mov       ecx, [edx + 0xBA9C]  # Copy save_count to new file
  mov       [eax + 0x00022FBC], ecx
  mov       ecx, [edx + 0xBAA0]  # Copy round2_seed to new file
  mov       [eax + 0x00022FC0], ecx
  add       eax, 4
  add       edx, 4
  mov       ecx, 0xBA90
  call      memcpy  # Copy the existing 4 characters over
  mov       eax, [0x00A95E44]
  add       eax, 0xBA94
  mov       ecx, 4
clear_next_char:
  cmp       ecx, 0x0C  # slot count
  jge       clear_next_char_done
  lea       edx, [eax + 0x2EA4]  # edx = ptr to next char (or footer)
clear_next_char_write_again:
  mov       dword [eax], 0
  add       eax, 4
  cmp       eax, edx
  jl        clear_next_char_write_again
clear_next_char_done:

  # Call eh_vector_constructor_iterator(
  #   &char_file_list.chars[4],
  #   sizeof(char_file_list.chars[0]),
  #   countof(char_file_list.chars) - 4,
  #   PSOCharacterFile::init,
  #   PSOCharacterFile::destroy)
  push      0x006C1940  # PSOCharacterFile::destroy
  push      0x006C17F0  # PSOCharacterFile::init
  push      0x08  # slot count - 4
  push      0x2EA4  # sizeof(PSOCharacterFile)
  mov       eax, [0x00A95E44]
  add       eax, 0xBA94
  push      eax
  mov       eax, 0x00858736
  call      eax

  # Fix the file's checksum
  mov       eax, [0x00A95E44]
  mov       ecx, 0x006C26FC
  jmp       ecx  # PSOBBCharacterFileList::checksum(char_file_list)



update_existing_char_file_list_memcard:
  # Allocate a new memory card file area and copy the data there too. It seems
  # Sega didn't fully strip out the local saving code from PSOBB; instead, they
  # just made it write to a heap-allocated buffer. Since the file is much
  # bigger now, we also have to make that heap-allocated buffer larger. We add
  # a few "blocks" on the end, since the original code in the game does that
  # too, but it's probably not strictly necessary.
  # Like the above, this part is not necessary if this patch is statically
  # applied to the executable.
  mov       eax, 0x00022FC4  # total file size
  add       eax, 0x0000FFFF
  and       eax, 0xFFFFC000
  push      eax
  mov       eax, 0x0082E940
  call      eax  # malloc10(total file size)
  add       esp, 4
  mov       [0x00A95E2C], eax
  mov       edx, [0x00A95E44]
  mov       ecx, 0x00022FC4  # total file size
  jmp       memcpy
