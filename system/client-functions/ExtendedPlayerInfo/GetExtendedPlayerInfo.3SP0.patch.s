.meta hide_from_patches_menu
.meta name="GetExtendedPlayerInfo"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  .include GetExtendedPlayerInfoGC
data:
  .data  0x8038CF94  # malloc9
  .data  0x8057CB10  # char_file_part1
  .data  0x8057CB14  # char_file_part2
  .data  0x8057C570  # root_protocol (anchor: send_05)
  .data  0x8038CFEC  # free9
  .data  0x80026BB8  # TProtocol_wait_send_drain
  .data  0x0000358C  # sizeof(*char_file_part2)
