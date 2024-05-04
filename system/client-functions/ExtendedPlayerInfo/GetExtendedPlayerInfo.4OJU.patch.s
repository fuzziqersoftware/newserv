.meta hide_from_patches_menu
.meta name="GetExtendedPlayerInfo"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  .include GetExtendedPlayerInfoXB
data:
  .data  0x002FE700  # malloc9(uint32_t size @ stack)
  .data  0x0063591C  # char_file_part1
  .data  0x006359C0  # char_file_part2
  .data  0x007270A0  # root_protocol
  .data  0x002FE7B0  # free9(void* ptr @ stack)
  .data  0x002ADDE0  # TProtocol::wait_send_drain(TProtocol* this @ esi)
