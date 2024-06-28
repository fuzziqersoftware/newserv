.meta hide_from_patches_menu
.meta name="GetExtendedPlayerInfo"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  .include GetExtendedPlayerInfoGC
data:
  .data  0x803DE468  # malloc9
  .data  0x805D6650  # char_file_part1
  .data  0x805D6654  # char_file_part2
  .data  0x805D5C70  # root_protocol (anchor: send_05)
  .data  0x803DE4C0  # free9
  .data  0x800786A0  # TProtocol_wait_send_drain
  .data  0x00002370  # sizeof(*char_file_part2)
