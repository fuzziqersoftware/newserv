.meta name="Rare alerts"
.meta description="Show rare items on\nthe map and play a\nsound when a rare\nitem drops"
# Inspired by and adapted from the original patch for Ep1&2 made by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

.versions 2OJF 2OJ5 2OEF 2OPF

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksDC

  
  #replace function call from command 6x5f to call to custom code for sound
  .align    4
  .data     <VERS 0x8C1944E4 0x8C195478 0x8C195478 0x8C194F10>
  .data     4
  .data     <VERS 0x8C02DD1C 0x8C02DB60 0x8C02DB60 0x8C02DB60>
  
  #replace function call from TItem::update in the case when the item is on the ground
  .align    4
  .data     <VERS 0x8C0734A0 0x8C0739B0 0x8C0739B0 0x8C073654>
  .data     4
  .data     <VERS 0x8C02E19C 0x8C02DFD8 0x8C02DFD8 0x8C02DFD8>

  #custom code that goes to check if item is rare and in the area the player is in and plays sound if it is
  .align    4
  .data     <VERS 0x8C02DD1C 0x8C02DB60 0x8C02DB60 0x8C02DB60>
  .data     72
  .data     0x400BD00C
  .data     0x67E0E500
  .data     0x6000D00B
  .data     0x8B0C3700
  .data     0x65E3D00A
  .data     0x400B7510
  .data     0x88006552
  .data     0xD4088905
  .data     0xD008E700
  .data     0x400B6673
  .data     0x7F106573
  .data     0x000B4F26
  .data     0x00096EF6
  .data     <VERS 0x8C1629BC 0x8C1631C8 0x8C1631C8 0x8C162DE4>
  .data     <VERS 0x8C456324 0x8C45C904 0x8C45C904 0x8C44BE04>
  .data     <VERS 0x8C02E14C 0x8C02DF90 0x8C02DF90 0x8C02DF90>
  .data     0x00050013
  .data     <VERS 0x8C05FA44 0x8C05FF54 0x8C05FF54 0x8C05FBF8>
  
  
  
  
  #custom code for checking if an item is rare or not (from r5)
  .align    4
  .data     <VERS 0x8C02E14C 0x8C02DF90 0x8C02DF90 0x8C02DF90>
  .data     68
  .data     0x8800605C
  .data     0x45198B06
  .data     0xE60C605C
  .data     0x89173066
  .data     0xE000000B
  .data     0x8B0A8801
  .data     0x605C4519
  .data     0x89048803
  .data     0x605C4519
  .data     0x3066E616
  .data     0x000B890A
  .data     0x8803E000
  .data     0x45198B04
  .data     0xE60B605C
  .data     0x89013066
  .data     0xE000000B
  .data     0xE001000B
  
  
  #custom code for checking if item is rare, then showing a dot on the map with color FF0000 if it is
  .align    4
  .data     <VERS 0x8C02E19C 0x8C02DFD8 0x8C02DFD8 0x8C02DFD8>
  .data     88
  .data     0x767F66E3
  .data     0x763C767F
  .data     0xE7026560
  .data     0x45183678
  .data     0x250B6060
  .data     0x45183678
  .data     0x250B6060
  .data     0x45183678
  .data     0xD0096760
  .data     0x257B400B
  .data     0x89068800
  .data     0xD00764E3
  .data     0x743CD507
  .data     0x400BE700
  .data     0x64E3E601
  .data     0x400BD005
  .data     0x4F260009
  .data     0x6EF6000B
  .data     <VERS 0x8C02E14C 0x8C02DF90 0x8C02DF90 0x8C02DF90>
  .data     <VERS 0x8C1028F8 0x8C103098 0x8C103098 0x8C102CD0>
  .data     0xFFFF0000
  .data     <VERS 0x8C073688 0x8C073B98 0x8C073B98 0x8C07383C>


  .align    4
  .data     0x00000000
  .data     0x00000000
