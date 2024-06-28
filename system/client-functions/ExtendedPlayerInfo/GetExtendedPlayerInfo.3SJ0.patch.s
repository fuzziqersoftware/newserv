.meta hide_from_patches_menu
.meta name="GetExtendedPlayerInfo"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  .include GetExtendedPlayerInfoGC
data:
  .data  0x8038B09C  # malloc9
  .data  0x80579880  # char_file_part1
  .data  0x80579884  # char_file_part2
  .data  0x805792E0  # root_protocol (anchor: send_05)
  .data  0x8038B0F4  # free9
  .data  0x80026A04  # TProtocol_wait_send_drain
  .data  0x0000358C  # sizeof(*char_file_part2)
