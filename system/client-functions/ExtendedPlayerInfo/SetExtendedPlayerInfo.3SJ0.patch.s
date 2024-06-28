.meta hide_from_patches_menu
.meta name="SetExtendedPlayerInfo"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  .include SetExtendedPlayerInfoGC
data:
  .data  0x80579878  # character_file
  .data  0x0000358C  # sizeof(*char_file_part2)
  # Server adds a PSOGCEp3CharacterFile::Character here
