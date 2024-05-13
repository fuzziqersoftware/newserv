.meta hide_from_patches_menu
.meta name="SetExtendedPlayerInfo"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  .include SetExtendedPlayerInfoXB
data:
  .data  0x0062DDE4  # char_file_part1
  .data  0x0062DE88  # char_file_part2
  # Server adds a PSOXBCharacterFileCharacter here
