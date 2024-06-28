.meta hide_from_patches_menu
.meta name="GetExtendedPlayerInfo"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  .include GetExtendedPlayerInfoGC
data:
  .data  0x8038C0EC  # malloc9
  .data  0x8057A6F0  # char_file_part1
  .data  0x8057A6F4  # char_file_part2
  .data  0x8057A150  # root_protocol (anchor: send_05)
  .data  0x8038C144  # free9
  .data  0x80026B88  # TProtocol_wait_send_drain
  .data  0x0000358C  # sizeof(*char_file_part2)
