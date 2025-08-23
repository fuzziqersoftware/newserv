# This patch changes the amount of items and Meseta that can be stored in the
# bank. If the bank item limit is increased beyond 200, this patch requires
# server support for extended bank data stored outside of the player's data.
# newserv has support for this, but you must set the BBBankItemLimit and
# BBBankMesetaLimit values in config.json to match the values used here.

# As written, this changes the meseta limit to 2000000000 and the item limit to
# 1000. The meseta limit can be any value up to 2147483647, and the item limit
# can be any value up to 1321. To use different values than the defaults, first
# compute the data size as ((slot count * 0x18) + 8), then replace each value
# below appropriately.

.meta name="More bank slots"
.meta description=""
.meta hide_from_patches_menu

entry_ptr:
reloc0:
  .offsetof start

start:
  .include WriteCodeBlocksBB

  .data    0x006C8C0F
  .data    4
  .data    1000  # slot count
  .data    0x006C8C4D
  .data    4
  .data    1000  # slot count
  .data    0x006C8B54
  .data    4
  .data    999  # slot count - 1
  .data    0x006C8B94
  .data    4
  .data    0x5DC0  # data size - 8
  .data    0x006C8D16
  .data    4
  .data    999  # slot count - 1
  .data    0x006C8E5E
  .data    4
  .data    999  # slot count - 1
  .data    0x006C8F2C
  .data    4
  .data    999  # slot count - 1
  .data    0x006C9016
  .data    4
  .data    0x5DB0  # data size - 0x18
  .data    0x006C9034
  .data    4
  .data    0x5DC0  # data size - 8
  .data    0x006C910D
  .data    4
  .data    0x5DB0  # data size - 0x18
  .data    0x006C9129
  .data    4
  .data    0x5DC8  # data size
  .data    0x006C9236
  .data    4
  .data    1000  # slot count
  .data    0x006C924C
  .data    4
  .data    999  # slot count - 1
  .data    0x006C9286
  .data    4
  .data    999  # slot count - 1
  .data    0x006C92FA
  .data    4
  .data    1000  # slot count
  .data    0x006C9883
  .data    4
  .data    1000  # slot count
  .data    0x006C9A22
  .data    4
  .data    2000000000  # max meseta
  .data    0x006CA2DB
  .data    4
  .data    0x5DC8  # data size
  .data    0x006CA303
  .data    4
  .data    1000  # slot count
  .data    0x006CA37F
  .data    4
  .data    0x5DC8  # data size
  .data    0x006D7DAC
  .data    4
  .data    1000  # slot count
  .data    0x006D7DBD
  .data    4
  .data    1000  # slot count
  .data    0x006D7E14
  .data    4
  .data    1000  # slot count
  .data    0x006D7BF5
  .data    4
  .data    1000  # slot count

  .data    0x006C8DBF
  .data    2
  jmp      +0x27

  .data    0
  .data    0
