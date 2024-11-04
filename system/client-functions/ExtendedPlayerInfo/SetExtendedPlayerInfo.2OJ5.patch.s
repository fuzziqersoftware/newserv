.meta hide_from_patches_menu
.meta name="SetExtendedPlayerInfo"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  .include SetExtendedPlayerInfoDC
data:
  .data  0x8C4EC4E0  # char_file_part1
  .data  0x8C4EC4E4  # char_file_part2
  # Server adds a PSODCV2CharacterFile::Character here
