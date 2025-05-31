# This patch replaces the prices and contents of Pinz's Shop.
# Each entry is structured as follows:
#   uint16_t card_id;
#   int16_t min_clv; // -1 = limit doesn't apply
#   int16_t max_clv; // -1 = limit doesn't apply
#   uint16_t relative_chance;
# The values in the patch data below are the defaults.

.meta name="New Pinz cards"
.meta description="Replaces the cards\navailable in Pinz's\nShop"

entry_ptr:
reloc0:
  .offsetof start

start:
  .include  WriteCodeBlocksGC

  # Meseta prices
  .data     0x80487140
  .data     0x00000010
  .data     50
  .data     100
  .data     150
  .data     0xFFFFFFFF

  # Card Capsule Machine 1
  .data     0x80487200
  .data     0x00000078
  .binary   017C FFFF FFFF 1B58
  .binary   0173 FFFF FFFF 1B58
  .binary   0176 FFFF FFFF 1F40
  .binary   006A FFFF FFFF 2710
  .binary   01EB FFFF FFFF 1F40
  .binary   01F1 FFFF FFFF 1770
  .binary   020E FFFF FFFF 1770
  .binary   0177 FFFF FFFF 1B58
  .binary   01AE FFFF FFFF 1770
  .binary   028A FFFF FFFF 1770
  .binary   01E8 FFFF FFFF 1770
  .binary   00A6 FFFF FFFF 1770
  .binary   023D FFFF FFFF 1388
  .binary   0208 FFFF FFFF 03E8
  .binary   FFFF FFFF FFFF FFFF

  # Card Capsule Machine 2
  .data     0x80487278
  .data     0x00000078
  .binary   017C FFFF FFFF 2710
  .binary   027E FFFF FFFF 1388
  .binary   0075 FFFF FFFF 1388
  .binary   020E FFFF FFFF 1388
  .binary   014D FFFF FFFF 1388
  .binary   000F FFFF FFFF 1770
  .binary   0269 FFFF FFFF 1F40
  .binary   006D FFFF FFFF 1B58
  .binary   0071 FFFF FFFF 1F40
  .binary   00C3 FFFF FFFF 1F40
  .binary   0208 FFFF FFFF 0BB8
  .binary   0138 FFFF FFFF 1F40
  .binary   0235 FFFF FFFF 1770
  .binary   00E6 FFFF FFFF 03E8
  .binary   FFFF FFFF FFFF FFFF

  # Card Capsule Machine 3
  .data     0x804872F0
  .data     0x00000078
  .binary   01AE FFFF FFFF 1F40
  .binary   014D FFFF FFFF 2328
  .binary   00BA FFFF FFFF 2328
  .binary   00A5 FFFF FFFF 2710
  .binary   01E8 FFFF FFFF 1F40
  .binary   025D FFFF FFFF 1F40
  .binary   028A FFFF FFFF 1F40
  .binary   0249 FFFF FFFF 2328
  .binary   0071 FFFF FFFF 1F40
  .binary   00B2 FFFF FFFF 1F40
  .binary   0129 FFFF FFFF 1F40
  .binary   01C1 FFFF FFFF 0BB8
  .binary   0132 FFFF FFFF 0BB8
  .binary   0148 FFFF FFFF 0BB8
  .binary   FFFF FFFF FFFF FFFF

  # Coin machine
  .data     0x80487368
  .data     0x00000070
  .binary   00A6 FFFF FFFF 2710
  .binary   01C1 FFFF FFFF 2710
  .binary   01FA FFFF FFFF 2710
  .binary   0208 FFFF FFFF 2710
  .binary   00E6 FFFF FFFF 2710
  .binary   00FF FFFF FFFF 2710
  .binary   0132 FFFF FFFF 2710
  .binary   013C FFFF FFFF 2710
  .binary   0148 FFFF FFFF 2710
  .binary   0198 FFFF FFFF 2710
  .binary   023D FFFF FFFF 2710
  .binary   00CA FFFF FFFF 2710
  .binary   00CF FFFF FFFF 2710
  .binary   FFFF FFFF FFFF FFFF

  .data     0x00000000
  .data     0x00000000
