.meta hide_from_patches_menu
.meta name="SetExtendedPlayerInfo"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  .include SetExtendedPlayerInfoGC
data:
  .data  0x805D5F58  # character_file
  .data  0x00002370  # sizeof(part2)
  # Server adds a PSOGCCharacterFile::Character here
