# This patch gives you the maximum number of each card. It only works if used
# in-game, which means it must be used by running `$patch AllCards`.

.meta hide_from_patches_menu
.meta name="All cards"
.meta description="Gives you the\nmaximum number of\neach card."

entry_ptr:
reloc0:
  .offsetof start

start:
  .include  AllCards
  .data     0x802A1BAC  # get_player_data_segment
  .data     0x802A15BC  # decrypt_ep3_player_data_segment
  .data     0x802A17AC  # Ep3PlayerDataSegment_on_card_obtained
  .data     0x802A160C  # encrypt_ep3_player_data_segment
