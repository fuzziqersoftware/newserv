# Implements the function:
# const PSOXBVersionInfo* @ eax [/none] get_version();
# The return opcode is not included in this fragment.
#
# The returned structure is:
# struct PSOXBVersionInfo {
#   /* 00 */ uint32_t specific_version;
#   /* 04 */ void* check_addr;
#   /* 08 */ void (**MmSetAddressProtect)(void* addr @ [esp+4], uint32_t size @ [esp + 8], uint32_t flags @ [esp+0xC]);
#   /* 0C */ uint32_t @ eax (**MmQueryAddressProtect)(void* addr @ [esp+4]);
#   /* 10 */ void* @ eax (*malloc7)(uint32_t size @ ecx, AllocatorInstance* instance @ edx);
#   /* 14 */ AllocatorInstance** malloc7_instance; // For use with malloc7
# };
#
# NOTE: Cxbx-Reloaded defines the protection flags as:
#   XBOX_PAGE_NOACCESS          = 0x00000001
#   XBOX_PAGE_READONLY          = 0x00000002
#   XBOX_PAGE_READWRITE         = 0x00000004
#   XBOX_PAGE_WRITECOPY         = 0x00000008
#   XBOX_PAGE_EXECUTE           = 0x00000010
#   XBOX_PAGE_EXECUTE_READ      = 0x00000020
#   XBOX_PAGE_EXECUTE_READWRITE = 0x00000040
#   XBOX_PAGE_EXECUTE_WRITECOPY = 0x00000080
#   XBOX_PAGE_GUARD             = 0x00000100
#   XBOX_PAGE_NOCACHE           = 0x00000200
#   XBOX_PAGE_WRITECOMBINE      = 0x00000400

start:
  push   ecx
  push   edx
  call   get_data

  .data  0x344F4A42  # 4OJB
  .data  0x0043D460
  .data  0x00400578
  .data  0x0040057C
  .data  0x002C63F0
  .data  0x006305E0

  .data  0x344F4A44  # 4OJD
  .data  0x0043D7D0
  .data  0x00400918
  .data  0x0040091C
  .data  0x002C6F40
  .data  0x00630C40

  .data  0x344F4A55  # 4OJU
  .data  0x00440FE0
  .data  0x00403E3C
  .data  0x00403E40
  .data  0x002C84E0
  .data  0x0063878C

  .data  0x344F4544  # 4OED
  .data  0x0044174C
  .data  0x00404518
  .data  0x0040451C
  .data  0x002C8030
  .data  0x00635C74

  .data  0x344F4555  # 4OEU
  .data  0x00440FEC
  .data  0x00403E3C
  .data  0x00403E40
  .data  0x002C8210
  .data  0x0063550C

  .data  0x344F5044  # 4OPD
  .data  0x00441768
  .data  0x00404538
  .data  0x0040453C
  .data  0x002C8060
  .data  0x00635C74

  .data  0x344F5055  # 4OPU
  .data  0x00441AF8
  .data  0x0040491C
  .data  0x00404920
  .data  0x002C8330
  .data  0x0063600C

  .data  0x00000000
  .data  0x00000000

get_data:
  pop    edx
  xor    eax, eax
  sub    edx, 0x18

check_next_version:
  add    edx, 0x18
  mov    ecx, [edx + 0x04]
  test   ecx, ecx
  jz     done
  cmp    dword [ecx], 0x61657244
  jne    check_next_version
  mov    eax, edx

done:
  pop    edx
  pop    ecx
