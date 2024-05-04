.meta hide_from_patches_menu
.meta name="GetExtendedPlayerInfo"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  .include GetExtendedPlayerInfoXB
data:
  .data  0x002FC5C0  # malloc9(uint32_t size @ stack)
  .data  0x0062D844  # char_file_part1
  .data  0x0062D8E8  # char_file_part2
  .data  0x0071EEFC  # root_protocol
  .data  0x002FC670  # free9(void* ptr @ stack)
  .data  0x002ABE30  # TProtocol::wait_send_drain(TProtocol* this @ esi)
