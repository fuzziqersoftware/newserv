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
  .data  0x803DE838  # malloc9
  .data  0x80202BA0  # get_character_file
  .data  0x80202B7C  # get_selected_character_file_index
  .data  0x805D5580  # root_protocol (anchor: send_05)
  .data  0x803DE890  # free9
  .data  0x8007889C  # TProtocol_wait_send_drain
get_data_addr:
  .include GetExtendedPlayerInfoGC
