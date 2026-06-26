.meta key="SetExtendedPlayerInfo"
.meta name="Set extended player info"
.meta description=""

.versions 3OJT 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0 3SJT 3SJ0 3SE0 3SP0

  .label   character_file, <VERS 0x8065F558 0x805C4E60 0x805CF428 0x805D68A8 0x805D6648 0x805C5758 0x805CC738 0x805D5F58 0x805D2198 0x8058B978 0x80579878 0x8057A6E8 0x8057CB08>
  .label   char_file_part1_size, 0x041C  # Same on all versions
  .label   char_file_part2_size, <VERS 0x2268 0x2370 0x2370 0x2370 0x2370 0x2370 0x2370 0x2370 0x2370 0x358C 0x358C 0x358C 0x358C>

entry_ptr:
reloc0:
  .data    start
start:
  mflr     r12
  bl       get_data_ptr
get_data_ptr_ret:
  mflr     r11

  lis      r10, high_word(character_file)
  ori      r10, r10, low_word(character_file)

  # Copy part1 data into place
  lwz      r3, [r10 + 0x08]
  mr       r4, r11
  li       r5, char_file_part1_size
  bl       memcpy

  # Copy part2 data into place, but retain the values of a few metadata fields so the game won't think it's corrupt
  lwz      r3, [r10 + 0x0C]
  lwz      r7, [r3 + 0x04]  # creation_timestamp
  lwz      r8, [r3 + 0x08]  # signature
  lwz      r9, [r3 + 0x14]  # save_count
  addi     r4, r11, char_file_part1_size
  li       r5, char_file_part2_size
  bl       memcpy
  lwz      r3, [r10 + 0x0C]
  stw      [r3 + 0x04], r7  # creation_timestamp
  stw      [r3 + 0x08], r8  # signature
  stw      [r3 + 0x14], r9  # save_count

  li       r3, 1
  mtlr     r12
  blr

memcpy:
  .include CopyDataWords
  blr

get_data_ptr:
  bl       get_data_ptr_ret
data:
  # Server adds PSOGCCharacterFile::Character, PSOGCNTECharacterFileCharacter, or PSOGCEp3CharacterFile::Character here
