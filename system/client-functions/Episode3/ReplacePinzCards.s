# This patch replaces the prices and contents of Pinz's Shop. It also documents how Pinz's Shop works internally.

# When you go to Pinz's Shop, the game generates a list of cards for each Card Capsule Machine from the lists below.
# Each card independently has a specific chance of appearing in the Card Capsule Machine at all; these are listed next
# to the cards' ranks. This list of cards is shown to you with the message "This time, cards like these have been put
# inside."

# For the normal Card Capsule Machines, there is a (price) / 6000 chance of getting a Rare Coin instead of a card, in
# which case the generated list of cards is ultimately ignored. (That is, Machine 1 gives a 50 / 6000 = 0.83% chance of
# a Rare Coin, Machine 2 gives a 100 / 6000 = 1.67% chance, and Machine 3 gives a 150 / 6000 = 2.5% chance, though for
# Machine 1 the chance is actually a bit higher, as described below). You can't get a Rare Coin from the Super Card
# Capsule Machine.

# If you play the game and don't get a Rare Coin, the game first chooses a result rank according to the following
# probabilities:
#                               N4  N3  N2  N1  R4  R3  R2  R1   S  SS  TOTAL
#   Card Capsule Machine 1      25  25  25  25  15                        115
#   Card Capsule Machine 2       5  25  25  25  25  25                    130
#   Card Capsule Machine 3       1  10  40  65  60      70  50            296
#   Super Card Capsule Machine              30  60  60  60  60  30        300
# These probabilities are all relative within each row; for example, Machine 1 chooses result rank R4 with probability
# 15 / (25 + 25 + 25 + 25 + 15) = 15 / 115 = 13.04%. After choosing a result rank, the game filters the card list so it
# contains only cards with that rank, then chooses one of those uniformly at random. If there are no cards of that
# rank, it chooses an N4 card from the list uniformly at random. If this happens and there are no N4 cards in the list
# (which is possible for Machine 1), it gives you a Rare Coin.

# So, if you see a specific card in the list before playing the game, the probability of getting that card is:
#   (rank probability from above table / sum of all rank probabilities in the same row) *
#     (1 / number of cards with same rank in the list of chosen cards)

# Uncomment the .meta visibility line to make this appear in the Patches menu.
# .meta visibility="all"
.meta name="New Pinz cards"
.meta description="Replaces the cards\navailable in Pinz's\nShop"

.versions 3SJ0 3SE0 3SP0

entry_ptr:
reloc0:
  .data   start

start:
  .include  WriteCodeBlocks

  # Meseta prices
  .data     <VERS 0x80487140 0x80487E80 0x8048A260>
  .data     0x00000010
  .data     50
  .data     100
  .data     150
  .data     0xFFFFFFFF

  # Probabilities of getting each rank for each machine
  .data     <VERS 0x80487150 0x80487E90 0x8048A270>
  .data     0x000000B0
  .data     25  # Machine 1 N4
  .data     25  # Machine 1 N3
  .data     25  # Machine 1 N2
  .data     25  # Machine 1 N1
  .data     15  # Machine 1 R4
  .data     0  # Machine 1 R3
  .data     0  # Machine 1 R2
  .data     0  # Machine 1 R1
  .data     0  # Machine 1 S
  .data     0  # Machine 1 SS
  .data     0xFFFFFFFF  # End of list
  .data     5  # Machine 2 N4
  .data     25  # Machine 2 N3
  .data     25  # Machine 2 N2
  .data     25  # Machine 2 N1
  .data     25  # Machine 2 R4
  .data     25  # Machine 2 R3
  .data     0  # Machine 2 R2
  .data     0  # Machine 2 R1
  .data     0  # Machine 2 S
  .data     0  # Machine 2 SS
  .data     0xFFFFFFFF  # End of list
  .data     1  # Machine 3 N4
  .data     10  # Machine 3 N3
  .data     40  # Machine 3 N2
  .data     65  # Machine 3 N1
  .data     60  # Machine 3 R4
  .data     0  # Machine 3 R3
  .data     70  # Machine 3 R2
  .data     50  # Machine 3 R1
  .data     0  # Machine 3 S
  .data     0  # Machine 3 SS
  .data     0xFFFFFFFF  # End of list
  .data     0  # Super machine N4
  .data     0  # Super machine N3
  .data     0  # Super machine N2
  .data     30  # Super machine N1
  .data     60  # Super machine R4
  .data     60  # Super machine R3
  .data     60  # Super machine R2
  .data     60  # Super machine R1
  .data     30  # Super machine S
  .data     0  # Super machine SS
  .data     0xFFFFFFFF  # End of list

  # Each entry is structured as follows:
  #   uint16_t card_id;
  #   int16_t min_clv; // -1 = limit doesn't apply
  #   int16_t max_clv; // -1 = limit doesn't apply
  #   uint16_t chance_to_appear_in_input; // In 0.01% increments, so 10000 = 100%
  # The values in the data below are the defaults.

  # Card Capsule Machine 1
  .data     <VERS 0x80487200 0x80487F40 0x8048A320>
  .data     0x00000078
  .binary   017C FFFF FFFF 1B58  # (70%;  N4) Visk-235W
  .binary   0173 FFFF FFFF 1B58  # (70%;  N4) Musashi
  .binary   0176 FFFF FFFF 1F40  # (80%;  N4) Yasha
  .binary   006A FFFF FFFF 2710  # (100%; N3) Migium
  .binary   01EB FFFF FFFF 1F40  # (80%;  N3) Meriltas
  .binary   01F1 FFFF FFFF 1770  # (60%;  N4) Recon
  .binary   020E FFFF FFFF 1770  # (60%;  N3) MC Attack
  .binary   0177 FFFF FFFF 1B58  # (70%;  N3) Lightning Partisan
  .binary   01AE FFFF FFFF 1770  # (60%;  N2) Bhirava
  .binary   028A FFFF FFFF 1770  # (60%;  N1) Mace Of Adaman
  .binary   01E8 FFFF FFFF 1770  # (60%;  N2) Zol Gibbon
  .binary   00A6 FFFF FFFF 1770  # (60%;  N1) HP Attack
  .binary   023D FFFF FFFF 1388  # (50%;  R2) Duel Guard (impossible to get, since probability of R2 is 0 for this machine)
  .binary   0208 FFFF FFFF 03E8  # (10%;  R4) Rage
  .binary   FFFF FFFF FFFF FFFF  # End of list (required)

  # Card Capsule Machine 2
  .data     <VERS 0x80487278 0x80487FB8 0x8048A398>
  .data     0x00000078
  .binary   017C FFFF FFFF 2710  # (100%; N4) Visk-235W (there are no R3 cards in the list, so this is also what you get if the game chooses R3)
  .binary   027E FFFF FFFF 1388  # (50%;  N3) HS25 Justice
  .binary   0075 FFFF FFFF 1388  # (50%;  N3) La Dimenian
  .binary   020E FFFF FFFF 1388  # (50%;  N3) MC Attack
  .binary   014D FFFF FFFF 1388  # (50%;  N3) Squeeze
  .binary   000F FFFF FFFF 1770  # (60%;  N2) DB's Saber
  .binary   0269 FFFF FFFF 1F40  # (80%;  N2) Asuka
  .binary   006D FFFF FFFF 1B58  # (70%;  N2) Dubchic
  .binary   0071 FFFF FFFF 1F40  # (80%;  N1) Sinow Beat
  .binary   00C3 FFFF FFFF 1F40  # (80%;  N2) Technique
  .binary   0208 FFFF FFFF 0BB8  # (30%;  R4) Rage
  .binary   0138 FFFF FFFF 1F40  # (80%;  N2) Territory
  .binary   0235 FFFF FFFF 1770  # (60%;  N1) Decline
  .binary   00E6 FFFF FFFF 03E8  # (10%;  R1) Shifta (impossible to get, since probability of R1 is 0 for this machine)
  .binary   FFFF FFFF FFFF FFFF  # End of list (required)

  # Card Capsule Machine 3
  .data     <VERS 0x804872F0 0x80488030 0x8048A410>
  .data     0x00000078
  .binary   01AE FFFF FFFF 1F40  # (80%;  N2) Bhirava
  .binary   014D FFFF FFFF 2328  # (90%;  N3) Squeeze
  .binary   00BA FFFF FFFF 2328  # (90%;  N3) Unit Blow
  .binary   00A5 FFFF FFFF 2710  # (100%; N4) TP Attack
  .binary   01E8 FFFF FFFF 1F40  # (80%;  N2) Zol Gibbon
  .binary   025D FFFF FFFF 1F40  # (80%;  N1) Slicer of Assassin
  .binary   028A FFFF FFFF 1F40  # (80%;  N1) Mace Of Adaman
  .binary   0249 FFFF FFFF 2328  # (90%;  N3) Victor Axe
  .binary   0071 FFFF FFFF 1F40  # (80%;  N1) Sinow Beat
  .binary   00B2 FFFF FFFF 1F40  # (80%;  N1) Ghost Blast
  .binary   0129 FFFF FFFF 1F40  # (80%;  N1) Shuffle Group
  .binary   01C1 FFFF FFFF 0BB8  # (30%;  R4) Sato
  .binary   0132 FFFF FFFF 0BB8  # (30%;  R2) Assist Return
  .binary   0148 FFFF FFFF 0BB8  # (30%;  R1) Support
  .binary   FFFF FFFF FFFF FFFF  # End of list (required)

  # Super Card Capsule Machine
  .data     <VERS 0x80487368 0x804880A8 0x8048A488>
  .data     0x00000070
  .binary   00A6 FFFF FFFF 2710  # (100%; N1) HP Attack
  .binary   01C1 FFFF FFFF 2710  # (100%; R4) Sato
  .binary   01FA FFFF FFFF 2710  # (100%;  S) Egg Rappy
  .binary   0208 FFFF FFFF 2710  # (100%; R4) Rage
  .binary   00E6 FFFF FFFF 2710  # (100%; R1) Shifta
  .binary   00FF FFFF FFFF 2710  # (100%; R2) Assistless
  .binary   0132 FFFF FFFF 2710  # (100%; R2) Assist Return
  .binary   013C FFFF FFFF 2710  # (100%;  S) Snail Pace
  .binary   0148 FFFF FFFF 2710  # (100%; R1) Support
  .binary   0198 FFFF FFFF 2710  # (100%; N2) Gal Wind (impossible to get, since probability of N2 is 0 for this machine)
  .binary   023D FFFF FFFF 2710  # (100%; R2) Duel Guard
  .binary   00CA FFFF FFFF 2710  # (100%; N4) Protection (there are no R3 cards in the list, so this is also what you get if the game chooses R3)
  .binary   00CF FFFF FFFF 2710  # (100%; N1) Companion
  .binary   FFFF FFFF FFFF FFFF  # End of list (required)

  .data     0x00000000
  .data     0x00000000
