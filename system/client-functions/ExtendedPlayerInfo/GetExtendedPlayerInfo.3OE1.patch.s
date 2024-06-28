.meta hide_from_patches_menu
.meta name="GetExtendedPlayerInfo"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  .include GetExtendedPlayerInfoGC
data:
  .data  0x803DB138  # malloc9
  .data  0x805CC740  # char_file_part1
  .data  0x805CC744  # char_file_part2
  .data  0x805CBD60  # root_protocol (anchor: send_05)
  .data  0x803DB190  # free9
  .data  0x800787B0  # TProtocol_wait_send_drain
  .data  0x00002370  # sizeof(*char_file_part2)
