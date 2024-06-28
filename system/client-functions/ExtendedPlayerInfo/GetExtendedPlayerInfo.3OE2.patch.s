.meta hide_from_patches_menu
.meta name="GetExtendedPlayerInfo"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  .include GetExtendedPlayerInfoGC
data:
  .data  0x803DE838  # malloc9
  .data  0x805D5F60  # char_file_part1
  .data  0x805D5F64  # char_file_part2
  .data  0x805D5580  # root_protocol (anchor: send_05)
  .data  0x803DE890  # free9
  .data  0x8007889C  # TProtocol_wait_send_drain
  .data  0x00002370  # sizeof(*char_file_part2)
