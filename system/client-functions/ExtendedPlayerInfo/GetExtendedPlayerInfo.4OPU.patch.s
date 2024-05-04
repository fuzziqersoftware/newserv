.meta hide_from_patches_menu
.meta name="GetExtendedPlayerInfo"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  .include GetExtendedPlayerInfoXB
data:
  .data  0x002FE770  # malloc9(uint32_t size @ stack)
  .data  0x0063319C  # char_file_part1
  .data  0x00633240  # char_file_part2
  .data  0x00724920  # root_protocol
  .data  0x002FE820  # free9(void* ptr @ stack)
  .data  0x002ADB10  # TProtocol::wait_send_drain(TProtocol* this @ esi)
