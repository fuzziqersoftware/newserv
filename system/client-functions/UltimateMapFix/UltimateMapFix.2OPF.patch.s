.meta name="Ultimate map fix"
.meta description="Adds missing maps\nto Ultimate for\ncertain quests"

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksDC

# Modify location of Ultimate Cave 1 map pointer table to 8C008100 and make it have 6 entries
  .align    4
  .data     0x8C3244A8
  .data     6
  .binary   0081008C0600

# Modify location of Ultimate Cave 3 map pointer table to 8C008130 and make it have 6 entries
  .align    4
  .data     0x8C3244B8
  .data     6
  .binary   3081008C0600

# Modify location of Ultimate Mine 1 map pointer table to 8C008160 and make it have 6 entries
  .align    4
  .data     0x8C3244C0
  .data     6
  .binary   6081008C0600

# Modify location of Ultimate Mine 2 map pointer table to 8C008190 and make it have 6 entries
  .align    4
  .data     0x8C3244C8
  .data     6
  .binary   9081008C0600

# New map pointer table for Cave 1
  .align    4
  .data     0x8C008100
  .data     48
  .binary   2F4A328C3B4A328C2F4A328C4A4A328C2F4A328C594A328C2F4A328C684A328C2F4A328C774A328C2F4A328C0082008C

# New map pointer table for Cave 3
  .align    4
  .data     0x8C008130
  .data     48
  .binary   DD4A328CE94A328CDD4A328CF84A328CDD4A328C074B328CDD4A328C164B328CDD4A328C254B328CDD4A328C0F82008C

# New map pointer table for Mine 1
  .align    4
  .data     0x8C008160
  .data     48
  .binary   344B328C434B328C344B328C554B328C344B328C674B328C344B328C794B328C344B328C8B4B328C344B328C1E82008C

# New map pointer table for Mine 2
  .align    4
  .data     0x8C008190
  .data     48
  .binary   9D4B328CAC4B328C9D4B328CBE4B328C9D4B328CD04B328C9D4B328CE24B328C9D4B328CF44B328C9D4B328C3082008C

# Add missing map names: map_acave01_05, map_acave03_05, map_amachine01_05, map_amachine02_05
  .align    4
  .data     0x8C008200
  .data     66
  .binary   6D61705F616361766530315F3035006D61705F616361766530335F3035006D61705F616D616368696E6530315F3035006D61705F616D616368696E6530325F303500

  .align    4
  .data     0x00000000
  .data     0x00000000
