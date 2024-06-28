.meta hide_from_patches_menu
.meta name="GetExtendedPlayerInfo"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  .include GetExtendedPlayerInfoGC
data:
  .data  0x803DE6B8  # malloc9
  .data  0x805D68B0  # char_file_part1
  .data  0x805D68B4  # char_file_part2
  .data  0x805D5ED0  # root_protocol (anchor: send_05)
  .data  0x803DE710  # free9
  .data  0x80078748  # TProtocol_wait_send_drain
  .data  0x00002370  # sizeof(*char_file_part2)
