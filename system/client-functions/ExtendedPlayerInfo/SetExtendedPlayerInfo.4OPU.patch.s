.meta hide_from_patches_menu
.meta name="SetExtendedPlayerInfo"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  .include SetExtendedPlayerInfoXB
data:
  .data  0x0063319C  # char_file_part1
  .data  0x00633240  # char_file_part2
  # Server adds a PSOXBCharacterFileCharacter here
