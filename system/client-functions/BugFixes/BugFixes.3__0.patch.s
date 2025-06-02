.meta name="Bug fixes"
.meta description="Fixes many minor\ngameplay, sound,\nand graphical bugs"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

.versions 3OE0 3OE1 3OJ2

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC

  .data     0x8000B088
  .data     0x00000058
  .data     0x7FA3EB78
  .data     0x38800000
  .data     <VERS 0x481AEB11 0x481AEB11 0x481AE725>
  .data     0x7FA3EB78
  .data     <VERS 0x481AEDE0 0x481AEDE0 0x481AE9F4>
  .data     0x881F0000
  .data     0x28090001
  .data     0x4082000C
  .data     0x881F0001
  .data     0x3BFF0002
  .data     <VERS 0x48100B68 0x48100B68 0x481008C4>
  .data     0x39200000
  .data     <VERS 0x48100AF9 0x48100AF9 0x48100855>
  .data     0x7F43D378
  .data     0x7F64DB78
  .data     0x7F85E378
  .data     0x7FA6EB78
  .data     0x7FC7F378
  .data     0x7FE8FB78
  .data     0x39200001
  .data     <VERS 0x48100AD9 0x48100AD9 0x48100835>
  .data     <VERS 0x48102F64 0x48102F64 0x48102CC0>

  .data     0x8000B5C8
  .data     0x00000014
  .data     0x80630098
  .data     <VERS 0x483D5999 0x483D59F1 0x483D46F5>
  .data     0x807F042C
  .data     0x809F0430
  .data     <VERS 0x48178C7C 0x48178C7C 0x481788C0>

  .data     0x8000BBD0
  .data     0x00000020
  .data     0x809F0370
  .data     0x3884FC00
  .data     0x909F0370
  .data     0x807F0014
  .data     0x28030000
  .data     0x41820008
  .data     0x90830060
  .data     <VERS 0x48165428 0x48165428 0x4816506C>

  .data     0x8000C3F8
  .data     0x0000007C
  .data     0x28040000
  .data     0x4D820020
  .data     0x9421FFF0
  .data     <VERS 0x481AD7A0 0x481AD7A0 0x481AD3B4>
  .data     0x9421FFE0
  .data     0x7C0802A6
  .data     0x90010024
  .data     0xBF410008
  .data     0x7C7F1B78
  .data     0x4BFFFFDD
  .data     0x3BC00000
  .data     0x3BBF0D04
  .data     0x837F032C
  .data     0x839D0000
  .data     0x7F83E379
  .data     0x41820018
  .data     0x38800001
  .data     <VERS 0x480FED81 0x480FED81 0x480FEADD>
  .data     0x7F83E378
  .data     0x38800001
  .data     <VERS 0x480FEEF1 0x480FEEF1 0x480FEC4D>
  .data     0x3BBD0004
  .data     0x3BDE0001
  .data     0x2C1E000D
  .data     0x4180FFD4
  .data     0x937F032C
  .data     0xBB410008
  .data     0x80010024
  .data     0x7C0803A6
  .data     0x38210020
  .data     0x4E800020

  .data     0x8000C640
  .data     0x00000014
  .data     0x54800673
  .data     0x41820008
  .data     0x38800000
  .data     0x38040009
  .data     <VERS 0x4810C938 0x4810C938 0x4810C694>

  .data     0x8000C6D0
  .data     0x00000020
  .data     0x38000001
  .data     0x901D0054
  .data     0x807D0024
  .data     <VERS 0x48211244 0x48211244 0x482109C0>
  .data     0x38000001
  .data     0x901F0378
  .data     0x807F0024
  .data     <VERS 0x482146F4 0x482146F4 0x48165AA0>

  .data     0x8000C8A0
  .data     0x00000014
  .data     0x1C00000A
  .data     0x57E407BD
  .data     0x41820008
  .data     0x7FA00734
  .data     <VERS 0x4810605C 0x4810605C 0x48105DB8>

  .data     0x8000C8C0
  .data     0x00000010
  .data     0x7000000F
  .data     0x7000004F
  .data     0x2C000004
  .data     0x4E800020

  .data     0x8000D980
  .data     0x00000014
  .data     0x807C0000
  .data     0x2C030013
  .data     0x40820008
  .data     0x38600002
  .data     <VERS 0x482AE568 0x482AE5AC 0x482ADB24>

  .data     0x8000D9A0
  .data     0x00000018
  .data     <VERS 0xC042FC88 0xC042FC88 0xC042FC78>
  .data     0x807E0030
  .data     0x70630020
  .data     0x41820008
  .data     <VERS 0xC042FCA0 0xC042FCA0 0xC042FC90>
  .data     <VERS 0x483280A0 0x483280E4 0x483276B0>

  .data     0x8000E1E0
  .data     0x0000001C
  .data     0x7FC802A6
  .data     0x38A00000
  .data     0x38C0001E
  .data     0x38E00040
  .data     <VERS 0x4807853D 0x4807853D 0x480782B1>
  .data     0x7FC803A6
  .data     0x4E800020

  .data     <VERS 0x80013084 0x80013084 0x8001306C>
  .data     0x00000004
  .data     0x4BFFFCC0

  .data     <VERS 0x800142F4 0x800142F4 0x800142DC>
  .data     0x00000004
  .data     <VERS 0x4BFF85CD 0x4BFF85CD 0x4BFF85E5>

  .data     <VERS 0x80015D1C 0x80015D1C 0x80015D04>
  .data     0x00000004
  .data     <VERS 0x4BFF6BA9 0x4BFF6BA9 0x4BFF6BC1>

  .data     <VERS 0x800917B4 0x800917B4 0x80091528>
  .data     0x00000008
  .data     0x4800024D
  .data     0xB3C3032C

  .data     <VERS 0x800BC9E8 0x800BC9E8 0x800BC750>
  .data     0x00000004
  .data     0x48000010

  .data     <VERS 0x80101EB8 0x80101EB8 0x80101C14>
  .data     0x00000004
  .data     0x60000000

  .data     <VERS 0x80104DEC 0x80104DEC 0x80104B48>
  .data     0x00000004
  .data     0x4182000C

  .data     <VERS 0x8010771C 0x8010771C 0x80107478>
  .data     0x00000004
  .data     0x4800000C

  .data     <VERS 0x80107730 0x80107730 0x8010748C>
  .data     0x00000004
  .data     0x7C030378

  .data     <VERS 0x8010BC14 0x8010BC14 0x8010B970>
  .data     0x00000004
  .data     <VERS 0x4BEFF488 0x4BEFF488 0x4BEFF72C>

  .data     <VERS 0x8010E03C 0x8010E03C 0x8010DD98>
  .data     0x00000004
  .data     <VERS 0x4BEFD078 0x4BEFD078 0x4BEFD31C>

  .data     <VERS 0x80112908 0x80112908 0x80112664>
  .data     0x00000004
  .data     <VERS 0x4BEF9F98 0x4BEF9F98 0x4BEFA23C>

  .data     <VERS 0x8011461C 0x8011461C 0x80114378>
  .data     0x00000004
  .data     0x38000012

  .data     <VERS 0x80118854 0x80118854 0x801185B0>
  .data     0x00000004
  .data     0x88040016

  .data     <VERS 0x80118860 0x80118860 0x801185BC>
  .data     0x00000004
  .data     0x88040017

  .data     <VERS 0x80118F84 0x80118F84 0x80118CE0>
  .data     0x00000004
  .data     <VERS 0x4BEF36BC 0x4BEF36BC 0x4BEF3960>

  .data     <VERS 0x8011CD34 0x8011CD34 0x8011CA90>
  .data     0x0000000C
  .data     0x7C030378
  .data     0x3863FFFF
  .data     0x4BFFFFE8

  .data     <VERS 0x8011CDF0 0x8011CDF0 0x8011CB4C>
  .data     0x0000000C
  .data     0x7C030378
  .data     0x3863FFFF
  .data     0x4BFFFFE8

  .data     <VERS 0x8011CE40 0x8011CE40 0x8011CB9C>
  .data     0x0000000C
  .data     0x7C040378
  .data     0x3884FFFF
  .data     0x4BFFFFE8

  .data     <VERS 0x801666E0 0x801666E0 0x80166324>
  .data     0x00000008
  .data     0x3C604005
  .data     0x4800009C

  .data     <VERS 0x8016677C 0x8016677C 0x801663C0>
  .data     0x00000004
  .data     0x4800001C

  .data     <VERS 0x80171010 0x80171010 0x80170C54>
  .data     0x00000004
  .data     <VERS 0x4BE9ABC0 0x4BE9ABC0 0x4BE9AF7C>

  .data     <VERS 0x80171030 0x80171030 0x80170C74>
  .data     0x00000004
  .data     0x60800420

  .data     <VERS 0x80184250 0x80184250 0x80172188>
  .data     0x00000004
  .data     <VERS 0x4BE87378 0x4BE87378 0x4BE9A558>

  .data     <VERS 0x80184290 0x80184290 0x80183E94>
  .data     0x00000004
  .data     <VERS 0x60000000 0x60000000 0x4BE87734>

  .data     <VERS 0x80189E20 0x80189E20 0x80183ED4>
  .data     0x00000004
  .data     0x60000000

  .data     <VERS 0x801937A8 0x801937A8 0x80189A54>
  .data     0x00000004
  .data     0x60000000

  .data     <VERS 0x801B9BA0 0x801B9BA0 0x801933DC>
  .data     0x00000004
  .data     <VERS 0x4BE52868 0x4BE52868 0x60000000>

  .data     <VERS 0x801B9E74 0x801B9E74 0x801B97B4>
  .data     0x00000004
  .data     <VERS 0x4BE51214 0x4BE51214 0x4BE52C54>

  .data     <VERS 0x801C62C0 0x801C62C0 0x801B9A88>
  .data     0x00000004
  .data     <VERS 0x389F02FC 0x389F02FC 0x4BE51600>

  .data     <VERS 0x801CA610 0x801CA610 0x801C5EA4>
  .data     0x00000004
  .data     <VERS 0x48000010 0x48000010 0x389F02FC>

  .data     <VERS 0x8021D91C 0x8021D91C 0x801CA1F4>
  .data     0x00000004
  .data     <VERS 0x4BDEEDB4 0x4BDEEDB4 0x48000010>

  .data     <VERS 0x80220DDC 0x80220DDC 0x8021D098>
  .data     0x00000004
  .data     <VERS 0x4BDEB904 0x4BDEB904 0x4BDEF638>

  .data     <VERS 0x80229C10 0x80229C10 0x80229354>
  .data     0x00000004
  .data     0x2C000001

  .data     <VERS 0x8022A410 0x8022A410 0x80229B54>
  .data     0x00000004
  .data     0x3880FF00

  .data     <VERS 0x8022A440 0x8022A440 0x80229B84>
  .data     0x00000004
  .data     0x3880FE80

  .data     <VERS 0x8022A470 0x8022A470 0x80229BB4>
  .data     0x00000004
  .data     0x3880FDB0

  .data     <VERS 0x8022D10C 0x8022D10C 0x8022C850>
  .data     0x00000004
  .data     0x60000000

  .data     <VERS 0x8022D840 0x8022D840 0x8022CF84>
  .data     0x00000004
  .data     0x41810630

  .data     <VERS 0x8022DB34 0x8022DB34 0x8022D278>
  .data     0x00000004
  .data     0x4181033C

  .data     <VERS 0x8022DC28 0x8022DC28 0x8022D36C>
  .data     0x00000004
  .data     0x41810248

  .data     <VERS 0x8022EB64 0x8022EB64 0x8022E2A8>
  .data     0x00000004
  .data     0x3880FF00

  .data     <VERS 0x8022EB94 0x8022EB94 0x8022E2D8>
  .data     0x00000004
  .data     0x3880FE80

  .data     <VERS 0x8022EBC4 0x8022EBC4 0x8022E308>
  .data     0x00000004
  .data     0x3880FDB0

  .data     <VERS 0x8022F370 0x8022F370 0x8022EAB4>
  .data     0x00000004
  .data     0x3880FF00

  .data     <VERS 0x8022F3A0 0x8022F3A0 0x8022EAE4>
  .data     0x00000004
  .data     0x3880FE80

  .data     <VERS 0x8022F3D0 0x8022F3D0 0x8022EB14>
  .data     0x00000004
  .data     0x3880FDB0

  .data     <VERS 0x80230974 0x80230974 0x802300B8>
  .data     0x00000004
  .data     0x3880FF00

  .data     <VERS 0x802309A4 0x802309A4 0x802300E8>
  .data     0x00000004
  .data     0x3880FE80

  .data     <VERS 0x802309D4 0x802309D4 0x80230118>
  .data     0x00000004
  .data     0x3880FDB0

  .data     <VERS 0x802316E4 0x802316E4 0x80230E08>
  .data     0x00000004
  .data     0x3880FF00

  .data     <VERS 0x80231714 0x80231714 0x80230E38>
  .data     0x00000004
  .data     0x3880FE80

  .data     <VERS 0x80231744 0x80231744 0x80230E68>
  .data     0x00000004
  .data     0x3880FDB0

  .data     <VERS 0x80231FD8 0x80231FD8 0x802316FC>
  .data     0x00000004
  .data     0x3880FF00

  .data     <VERS 0x80232010 0x80232010 0x80231734>
  .data     0x00000004
  .data     0x3880FE80

  .data     <VERS 0x80232048 0x80232048 0x8023176C>
  .data     0x00000004
  .data     0x3880FDB0

  .data     <VERS 0x80234084 0x80234084 0x802337A8>
  .data     0x00000004
  .data     0x3880FF00

  .data     <VERS 0x802340B4 0x802340B4 0x802337D8>
  .data     0x00000004
  .data     0x3880FE80

  .data     <VERS 0x802340E4 0x802340E4 0x80233808>
  .data     0x00000004
  .data     0x3880FDB0

  .data     <VERS 0x802366B0 0x802366B0 0x80235DD4>
  .data     0x00000004
  .data     0x3880FF00

  .data     <VERS 0x802366EC 0x802366EC 0x80235E10>
  .data     0x00000004
  .data     0x3880FE80

  .data     <VERS 0x80236728 0x80236728 0x80235E4C>
  .data     0x00000004
  .data     0x3880FDB0

  .data     <VERS 0x80236E88 0x80236E88 0x802365AC>
  .data     0x00000004
  .data     0x3880FF00

  .data     <VERS 0x80236EB8 0x80236EB8 0x802365DC>
  .data     0x00000004
  .data     0x3880FE80

  .data     <VERS 0x80236EE8 0x80236EE8 0x8023660C>
  .data     0x00000004
  .data     0x3880FDB0

  .data     <VERS 0x8023789C 0x8023789C 0x80236FC0>
  .data     0x00000004
  .data     0x3880FF00

  .data     <VERS 0x802378CC 0x802378CC 0x80236FF0>
  .data     0x00000004
  .data     0x3880FE80

  .data     <VERS 0x802378FC 0x802378FC 0x80237020>
  .data     0x00000004
  .data     0x3880FDB0

  .data     <VERS 0x80238274 0x80238274 0x80237998>
  .data     0x00000004
  .data     0x3880FF00

  .data     <VERS 0x802382A4 0x802382A4 0x802379C8>
  .data     0x00000004
  .data     0x3880FE80

  .data     <VERS 0x802382D4 0x802382D4 0x802379F8>
  .data     0x00000004
  .data     0x3880FDB0

  .data     <VERS 0x8023BBA4 0x8023BBA4 0x8023B2C8>
  .data     0x00000004
  .data     0x3880FF00

  .data     <VERS 0x8023BBD4 0x8023BBD4 0x8023B2F8>
  .data     0x00000004
  .data     0x3880FE80

  .data     <VERS 0x8023BC04 0x8023BC04 0x8023B328>
  .data     0x00000004
  .data     0x3880FDB0

  .data     <VERS 0x80250AEC 0x80250AEC 0x80250264>
  .data     0x00000004
  .data     0x60000000

  .data     <VERS 0x80268788 0x80268788 0x80267DDC>
  .data     0x00000004
  .data     0x60000000

  .data     <VERS 0x8026E2D4 0x8026E2D4 0x8026DA74>
  .data     0x00000004
  .data     0x3884AAFA

  .data     <VERS 0x8026E3E8 0x8026E3E8 0x8026DB88>
  .data     0x00000004
  .data     0x3863AAFA

  .data     <VERS 0x8026E470 0x8026E470 0x8026DC10>
  .data     0x00000004
  .data     0x3883AAFA

  .data     <VERS 0x802BBEF4 0x802BBF38 0x802BB4B0>
  .data     0x00000004
  .data     <VERS 0x4BD51A8C 0x4BD51A48 0x4BD524D0>

  .data     <VERS 0x802FC2F4 0x802FC338 0x802FB99C>
  .data     0x00000004
  .data     0x2C030001

  .data     <VERS 0x80301F58 0x80301F9C 0x80301600>
  .data     0x0000001C
  .data     0x48000020
  .data     0x3863A830
  .data     <VERS 0x800DB9A4 0x800DB9A4 0x800DB98C>
  .data     0x2C000023
  .data     0x40820008
  .data     0x3863FB28
  .data     0x4800008C

  .data     <VERS 0x80301FF8 0x8030203C 0x803016A0>
  .data     0x00000004
  .data     0x4BFFFF64

  .data     <VERS 0x80335A50 0x80335A94 0x80335060>
  .data     0x00000004
  .data     <VERS 0x4BCD7F50 0x4BCD7F0C 0x4BCD8940>

  .data     <VERS 0x80356814 0x80356858 0x80355960>
  .data     0x00000004
  .data     0x388001E8

  .data     <VERS 0x80356838 0x8035687C 0x80355984>
  .data     0x00000004
  .data     <VERS 0x4BCB79A9 0x4BCB7965 0x4BCB885D>

  .data     <VERS 0x803568A8 0x803568EC 0x803559F4>
  .data     0x00000004
  .data     0x388001E8

  .data     <VERS 0x803568B8 0x803568FC 0x80355A04>
  .data     0x00000004
  .data     <VERS 0x4BCB7929 0x4BCB78E5 0x4BCB87DD>

  .data     <VERS 0x804B3EF0 0x804B43D0 0x804B3738>
  .data     0x00000008
  .data     0x70808080
  .data     0x60707070

  .data     <VERS 0x804C76B4 0x804C7B94 0x804C6EE4>
  .data     0x00000004
  .data     0x0000001E

  .data     <VERS 0x804C770C 0x804C7BEC 0x804C6F3C>
  .data     0x00000004
  .data     0x00000028

  .data     <VERS 0x804C7738 0x804C7C18 0x804C6F68>
  .data     0x00000004
  .data     0x00000032

  .data     <VERS 0x804C7764 0x804C7C44 0x804C6F94>
  .data     0x00000004
  .data     0x0000003C

  .data     <VERS 0x804C7774 0x804C7C54 0x804C6FA4>
  .data     0x00000004
  .data     0x0018003C

  .data     <VERS 0x804C79CC 0x804C7EAC 0x804C71FC>
  .data     0x00000004
  .data     0x00000028

  .data     <VERS 0x804CC310 0x804CC7F0 0x804CBB40>
  .data     0x00000004
  .data     0xFF0074EE

  .data     <VERS 0x805CA274 0x805D1294 0x805C996C>
  .data     0x00000004
  .data     0x435C0000

  .data     <VERS 0x805CBF10 0x805D2F30 0x805CB608>
  .data     0x00000004
  .data     0x46AFC800

  .data     <VERS 0x805CC1B0 0x805D31D0 0x805CB8A8>
  .data     0x00000004
  .data     0x43480000

  .data     0x00000000
  .data     0x00000000
