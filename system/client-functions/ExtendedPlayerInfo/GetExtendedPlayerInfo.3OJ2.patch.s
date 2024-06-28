.meta hide_from_patches_menu
.meta name="GetExtendedPlayerInfo"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  .include GetExtendedPlayerInfoGC
data:
  .data  0x803D9E38  # malloc9
  .data  0x805C4E68  # char_file_part1
  .data  0x805C4E6C  # char_file_part2
  .data  0x805C4488  # root_protocol (anchor: send_05)
  .data  0x803D9E90  # free9
  .data  0x8007848C  # TProtocol_wait_send_drain
  .data  0x00002370  # sizeof(*char_file_part2)
