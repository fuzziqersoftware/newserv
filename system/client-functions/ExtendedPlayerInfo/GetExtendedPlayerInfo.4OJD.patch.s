.meta hide_from_patches_menu
.meta name="GetExtendedPlayerInfo"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  .include GetExtendedPlayerInfoXB
data:
  .data  0x002FD110  # malloc9(uint32_t size @ stack)
  .data  0x0062DDE4  # char_file_part1
  .data  0x0062DE88  # char_file_part2
  .data  0x0071F55C  # root_protocol
  .data  0x002FD1C0  # free9(void* ptr @ stack)
  .data  0x002AC910  # TProtocol::wait_send_drain(TProtocol* this @ esi)
