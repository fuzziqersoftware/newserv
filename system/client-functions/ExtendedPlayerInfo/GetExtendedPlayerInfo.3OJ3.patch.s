.meta hide_from_patches_menu
.meta name="GetExtendedPlayerInfo"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  .include GetExtendedPlayerInfoGC
data:
  .data  0x803DC818  # malloc9
  .data  0x805CF430  # char_file_part1
  .data  0x805CF434  # char_file_part2
  .data  0x805CEA50  # root_protocol (anchor: send_05)
  .data  0x803DC870  # free9
  .data  0x800785F0  # TProtocol_wait_send_drain
  .data  0x00002370  # sizeof(*char_file_part2)
