.meta hide_from_patches_menu
.meta name="GetExtendedPlayerInfo"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  .include GetExtendedPlayerInfoDC
data:
  .data  0x8C36CD2A  # malloc9
  .data  0x8C4DB9E0  # char_file_part1 (anchor: send_61)
  .data  0x8C4DB9E4  # char_file_part2 (anchor: send_61)
  .data  0x8C418A00  # root_protocol (anchor: send_61)
  .data  0x8C36CDF8  # free9
  .data  0x8C010A1C  # TProtocol_wait_send_drain
