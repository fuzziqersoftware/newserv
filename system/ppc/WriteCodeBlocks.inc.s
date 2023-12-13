  mflr    r8
  b       get_patch_data_ptr
get_patch_data_ptr_ret:
  mflr    r7  # r7 = patch header
apply_patch:
  addi    r4, r7, 8  # r4 = start of patch data
  lwz     r3, [r4 - 8]  # r3 = patch dest address
  lwz     r5, [r4 - 4]  # r5 = patch data size
  or      r0, r3, r5
  cmplwi  r0, 0
  mtlr    r8
  beqlr
  add     r7, r4, r5  # r7 = next patch header
  .include CopyCode
  b       apply_patch

get_patch_data_ptr:
  bl      get_patch_data_ptr_ret

first_patch_header:
