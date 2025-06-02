.meta name="Item pickup"
.meta description="Prevents picking\nup items unless you\nhold the Z button"
# Original code by Ralf @ GC-Forever
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

.versions 3OE0 3OE1 3OE2 3OJ2 3OJ3 3OJ4 3OJ5 3OP0

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC
  .data     0x8000B938
  .data     0x00000020
  .data     0x387C0550
  .data     0x38800100
  .data     <VERS 0x4834428D 0x483442D1 0x48345EB9 0x483433D9 0x483447DD 0x48345D45 0x48345AED 0x483452AD>
  .data     0x2C030000
  .data     0x4182000C
  .data     0x7F83E378
  .data     <VERS 0x481AA150 0x481AA150 0x481AA2E8 0x481A9D64 0x481AA1B8 0x481AC370 0x481AA284 0x481AA7A4>
  .data     <VERS 0x481AA15C 0x481AA15C 0x481AA2F4 0x481A9D70 0x481AA1C4 0x481AC37C 0x481AA290 0x481AA7B0>
  .data     <VERS 0x801B5A9C 0x801B5A9C 0x801B5C34 0x801B56B0 0x801B5B04 0x801B7CBC 0x801B5BD0 0x801B60F0>
  .data     0x00000004
  .data     <VERS 0x4BE55E9C 0x4BE55E9C 0x4BE55D04 0x4BE56288 0x4BE55E34 0x4BE53C7C 0x4BE55D68 0x4BE55848>
  .data     <VERS 0x8024CC0C 0x8024CC0C 0x8024DD88 0x8024C384 0x8024CDD0 0x8024DD28 0x8024DAC4 0x8024D5D0>
  .data     0x00000004
  .data     0x38800008
  .data     0x00000000
  .data     0x00000000
