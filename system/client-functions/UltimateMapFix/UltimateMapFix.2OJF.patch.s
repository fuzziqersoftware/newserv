.meta name="Ultimate map fix"
.meta description="Adds missing maps\nto Ultimate for\ncertain quests"

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksDC

# Modify location of Ultimate Cave 1 map pointer table to 8C008100 and make it have 6 entries
  .align    4
  .data     0x8C32D638
  .data     6
  .binary   0081008C0600

# Modify location of Ultimate Cave 3 map pointer table to 8C008130 and make it have 6 entries
  .align    4
  .data     0x8C32D648
  .data     6
  .binary   3081008C0600

# Modify location of Ultimate Mine 1 map pointer table to 8C008160 and make it have 6 entries
  .align    4
  .data     0x8C32D650
  .data     6
  .binary   6081008C0600

# Modify location of Ultimate Mine 2 map pointer table to 8C008190 and make it have 6 entries
  .align    4
  .data     0x8C32D658
  .data     6
  .binary   9081008C0600

# New map pointer table for Cave 1
  .align    4
  .data     0x8C008100
  .data     48
  .binary   BFDB328CCBDB328CBFDB328CDADB328CBFDB328CE9DB328CBFDB328CF8DB328CBFDB328C07DC328CBFDB328C0082008C

# New map pointer table for Cave 3
  .align    4
  .data     0x8C008130
  .data     48
  .binary   6DDC328C79DC328C6DDC328C88DC328C6DDC328C97DC328C6DDC328CA6DC328C6DDC328CB5DC328C6DDC328C0F82008C

# New map pointer table for Mine 1
  .align    4
  .data     0x8C008160
  .data     48
  .binary   C4DC328CD3DC328CC4DC328CE5DC328CC4DC328CF7DC328CC4DC328C09DD328CC4DC328C1BDD328CC4DC328C1E82008C

# New map pointer table for Mine 2
  .align    4
  .data     0x8C008190
  .data     48
  .binary   2DDD328C3CDD328C2DDD328C4EDD328C2DDD328C60DD328C2DDD328C72DD328C2DDD328C84DD328C2DDD328C3082008C

# Add missing map names: map_acave01_05, map_acave03_05, map_amachine01_05, map_amachine02_05
  .align    4
  .data     0x8C008200
  .data     66
  .binary   6D61705F616361766530315F3035006D61705F616361766530335F3035006D61705F616D616368696E6530315F3035006D61705F616D616368696E6530325F303500

  .align    4
  .data     0x00000000
  .data     0x00000000
