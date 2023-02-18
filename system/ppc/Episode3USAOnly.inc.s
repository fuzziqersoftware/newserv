  lis    r4, 0x4750
  ori    r4, r4, 0x5345 # 'GPSE'
  lis    r5, 0x8000
  lwz    r5, [r5]
  li     r3, -1
  cmp    r4, r5
  bnelr
