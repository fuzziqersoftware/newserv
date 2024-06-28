.meta hide_from_patches_menu
.meta name="GetExtendedPlayerInfo"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  .include GetExtendedPlayerInfoGC
data:
  .data  0x803DD328  # malloc9
  .data  0x805D21A0  # char_file_part1
  .data  0x805D21A4  # char_file_part2
  .data  0x805D17C0  # root_protocol (anchor: send_05)
  .data  0x803DD380  # free9
  .data  0x80078820  # TProtocol_wait_send_drain
  .data  0x00002370  # sizeof(*char_file_part2)
