# Returns the client specific_version in eax and the address of the
# MmSetAddressProtect function pointer in edx, which is immediately followed by
# the MmQueryAddressProtect function pointer.

start:
  mov   ecx, 0x61657244

  # JP beta
  mov   eax, 0x344F4A42
  mov   edx, 0x00400578
  cmp   [0x0043D460], ecx
  je    done

  # JP disc
  mov   eax, 0x344F4A44
  mov   edx, 0x00400918
  cmp   [0x0043D7D0], ecx
  je    done

  # JP title update
  mov   eax, 0x344F4A55
  mov   edx, 0x00403E3C
  cmp   [0x00440FE0], ecx
  je    done

  # US disc
  mov   eax, 0x344F4544
  mov   edx, 0x00404518
  cmp   [0x0044174C], ecx
  je    done

  # US title update
  mov   eax, 0x344F4555
  mov   edx, 0x00403E3C
  cmp   [0x00440FEC], ecx
  je    done

  # EU disc
  mov   eax, 0x344F5044
  mov   edx, 0x00404538
  cmp   [0x00441768], ecx
  je done

  # EU title update
  mov   eax, 0x344F5055
  mov   edx, 0x0040491C
  cmp   [0x00441AF8], ecx
  je    done

  # Unknown version
  mov   eax, 0x344F0000
  xor   edx, edx

done:
