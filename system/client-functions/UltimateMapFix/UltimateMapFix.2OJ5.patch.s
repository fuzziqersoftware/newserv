.meta name="Ultimate map fix"
.meta description="Adds missing maps\nto Ultimate for\ncertain quests"

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksDC

# Modify location of Ultimate Cave 1 map pointer table to 8C008100 and make it have 6 entries
  .align    4
  .data     0x8C32FED0
  .data     6
  .binary   0081008C0600

# Modify location of Ultimate Cave 3 map pointer table to 8C008130 and make it have 6 entries
  .align    4
  .data     0x8C32FEE0
  .data     6
  .binary   3081008C0600

# Modify location of Ultimate Mine 1 map pointer table to 8C008160 and make it have 6 entries
  .align    4
  .data     0x8C32FEE8
  .data     6
  .binary   6081008C0600

# Modify location of Ultimate Mine 2 map pointer table to 8C008190 and make it have 6 entries
  .align    4
  .data     0x8C32FEF0
  .data     6
  .binary   9081008C0600

# New map pointer table for Cave 1
  .align    4
  .data     0x8C008100
  .data     48
  .binary   5704338C6304338C5704338C7204338C5704338C8104338C5704338C9004338C5704338C9F04338C5704338C0082008C

# New map pointer table for Cave 3
  .align    4
  .data     0x8C008130
  .data     48
  .binary   0505338C1105338C0505338C4D05338C0505338C2F05338C0505338C3E05338C0505338C4D05338C0505338C0F82008C

# New map pointer table for Mine 1
  .align    4
  .data     0x8C008160
  .data     48
  .binary   5C05338C6B05338C5C05338C7D05338C5C05338C8F05338C5C05338CA105338C5C05338CB305338C5C05338C1E82008C

# New map pointer table for Mine 2
  .align    4
  .data     0x8C008190
  .data     48
  .binary   C505338CD405338CC505338CE605338CC505338CF805338CC505338C0A06338CC505338C1C06338CC505338C3082008C

# Add missing map names: map_acave01_05, map_acave03_05, map_amachine01_05, map_amachine02_05
  .align    4
  .data     0x8C008200
  .data     66
  .binary   6D61705F616361766530315F3035006D61705F616361766530335F3035006D61705F616D616368696E6530315F3035006D61705F616D616368696E6530325F303500

  .align    4
  .data     0x00000000
  .data     0x00000000
