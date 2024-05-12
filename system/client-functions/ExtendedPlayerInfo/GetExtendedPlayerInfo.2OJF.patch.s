.meta hide_from_patches_menu
.meta name="GetExtendedPlayerInfo"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  .include GetExtendedPlayerInfoDC
data:
  .data  0x8C3772AE  # malloc9
  .data  0x8C4E5F80  # char_file_part1 (anchor: send_61)
  .data  0x8C4E5F84  # char_file_part2 (anchor: send_61)
  .data  0x8C422F80  # root_protocol (anchor: send_61)
  .data  0x8C37737C  # free9
  .data  0x8C010A1C  # TProtocol_wait_send_drain
