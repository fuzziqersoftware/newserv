.meta hide_from_patches_menu
.meta name="GetExtendedPlayerInfo"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  .include GetExtendedPlayerInfoXB
data:
  .data  0x002FE5A0  # malloc9(uint32_t size @ stack)
  .data  0x00632E04  # char_file_part1
  .data  0x00632EA8  # char_file_part2
  .data  0x0072459C  # root_protocol
  .data  0x002FE650  # free9(void* ptr @ stack)
  .data  0x002AD870  # TProtocol::wait_send_drain(TProtocol* this @ esi)
