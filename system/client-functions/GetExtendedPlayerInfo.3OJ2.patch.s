.meta hide_from_patches_menu
.meta name="GetExtendedPlayerInfo"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  mflr   r12
  bl     get_data_addr
data:
  .data  0x803D9E38  # malloc9
  .data  0x802019D4  # get_character_file
  .data  0x802019B0  # get_selected_character_file_index
  .data  0x805C4488  # root_protocol (anchor: send_05)
  .data  0x803D9E90  # free9
  .data  0x8007848C  # TProtocol_wait_send_drain
get_data_addr:
  .include GetExtendedPlayerInfoGC
