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
  .data  0x803DE468  # malloc9
  .data  0x8020286C  # get_character_file
  .data  0x80202848  # get_selected_character_file_index
  .data  0x805D5C70  # root_protocol (anchor: send_05)
  .data  0x803DE4C0  # free9
  .data  0x800786A0  # TProtocol_wait_send_drain
get_data_addr:
  .include GetExtendedPlayerInfoGC
