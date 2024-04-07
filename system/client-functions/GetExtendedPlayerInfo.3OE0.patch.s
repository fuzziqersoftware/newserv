# .meta hide_from_patches_menu
.meta name="GetExtendedPlayerInfo"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  mflr   r12
  bl     get_data_addr
data:
  .data  0x803DB0E0  # malloc9
  .data  0x802021D0  # get_character_file
  .data  0x802021AC  # get_selected_character_file_index
  .data  0x805C4D80  # root_protocol (anchor: send_05)
  .data  0x803DB138  # free9
  .data  0x800787B0  # TProtocol_wait_send_drain
get_data_addr:
  .include GetExtendedPlayerInfoGC
