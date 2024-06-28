.meta hide_from_patches_menu
.meta name="GetExtendedPlayerInfo"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  .include GetExtendedPlayerInfoGC
data:
  .data  0x80358094  # malloc9
  .data  0x8058B980  # char_file_part1
  .data  0x8058B984  # char_file_part2
  .data  0x8058B3A0  # root_protocol (anchor: send_05)
  .data  0x803580EC  # free9
  .data  0x80026FE4  # TProtocol_wait_send_drain
  .data  0x000041F4  # sizeof(*char_file_part2)
