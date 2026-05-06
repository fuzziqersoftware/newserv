.meta name="PSO Peeps EP1 5x EXP"
.meta description="Sets EP1 enemy EXP\nto 5x for GC crossplay"

entry_ptr:
reloc0:
  .offsetof start

start:
  .include  WriteCodeBlocksGC

  # PSO Peeps GC Plus USA / 3OE1
  # Source table: BattleParamEntry_on.dat
  # Active online battle-param table loaded at 0x811AB7C0
  # EXP field offset within each 0x24-byte row is +0x1C
  # Generated from clean BattleParamEntry_on.dat; multiplier=5x

  .data     0x811AB7DC
  .data     4
  .binary   00000005

  .data     0x811AB800
  .data     4
  .binary   0000001e

  .data     0x811AB824
  .data     4
  .binary   00000019

  .data     0x811AB848
  .data     4
  .binary   00000023

  .data     0x811AB86C
  .data     4
  .binary   00000032

  .data     0x811AB890
  .data     4
  .binary   000002ee

  .data     0x811AB8B4
  .data     4
  .binary   0000007d

  .data     0x811AB8D8
  .data     4
  .binary   00000050

  .data     0x811AB8FC
  .data     4
  .binary   00000050

  .data     0x811AB920
  .data     4
  .binary   00000055

  .data     0x811AB944
  .data     4
  .binary   00000091

  .data     0x811AB968
  .data     4
  .binary   00000014

  .data     0x811AB98C
  .data     4
  .binary   00000014

  .data     0x811AB9B0
  .data     4
  .binary   000000af

  .data     0x811AB9D4
  .data     4
  .binary   000000af

  .data     0x811AB9F8
  .data     4
  .binary   00001162

  .data     0x811ABA1C
  .data     4
  .binary   00000032

  .data     0x811ABA40
  .data     4
  .binary   00000032

  .data     0x811ABA64
  .data     4
  .binary   000007d0

  .data     0x811ABA88
  .data     4
  .binary   0000008c

  .data     0x811ABB3C
  .data     4
  .binary   00000014

  .data     0x811ABB60
  .data     4
  .binary   000001f4

  .data     0x811ABB84
  .data     4
  .binary   0000004b

  .data     0x811ABBA8
  .data     4
  .binary   0000000f

  .data     0x811ABBCC
  .data     4
  .binary   0000005a

  .data     0x811ABBF0
  .data     4
  .binary   0000006e

  .data     0x811ABC14
  .data     4
  .binary   00000064

  .data     0x811ABC38
  .data     4
  .binary   00000087

  .data     0x811ABC5C
  .data     4
  .binary   0000001e

  .data     0x811ABD10
  .data     4
  .binary   0000186a

  .data     0x811ABE9C
  .data     4
  .binary   00000032

  .data     0x811ABEC0
  .data     4
  .binary   0000001e

  .data     0x811ABEE4
  .data     4
  .binary   00000014

  .data     0x811ABF08
  .data     4
  .binary   00000014

  .data     0x811ABF2C
  .data     4
  .binary   000002ee

  .data     0x811ABF50
  .data     4
  .binary   00000019

  .data     0x811ABF98
  .data     4
  .binary   00003a98

  .data     0x811ABFE0
  .data     4
  .binary   00000019

  .data     0x811AC220
  .data     4
  .binary   00000041

  .data     0x811AC244
  .data     4
  .binary   000001f4

  .data     0x811AC268
  .data     4
  .binary   00000019

  .data     0x811AC28C
  .data     4
  .binary   0000001e

  .data     0x811AC2B0
  .data     4
  .binary   00000023

  .data     0x811AC2D4
  .data     4
  .binary   00000064

  .data     0x811AC2F8
  .data     4
  .binary   00000032

  .data     0x811AC31C
  .data     4
  .binary   0000003c

  .data     0x811AC340
  .data     4
  .binary   00000046

  .data     0x811AC364
  .data     4
  .binary   00000096

  .data     0x811AC388
  .data     4
  .binary   0000006e

  .data     0x811AC3AC
  .data     4
  .binary   00000078

  .data     0x811AC3D0
  .data     4
  .binary   00000082

  .data     0x811AC55C
  .data     4
  .binary   000000b4

  .data     0x811AC580
  .data     4
  .binary   000000d7

  .data     0x811AC5A4
  .data     4
  .binary   000000d2

  .data     0x811AC5C8
  .data     4
  .binary   000000e1

  .data     0x811AC5EC
  .data     4
  .binary   000000f5

  .data     0x811AC610
  .data     4
  .binary   0000050f

  .data     0x811AC634
  .data     4
  .binary   0000015e

  .data     0x811AC658
  .data     4
  .binary   00000122

  .data     0x811AC67C
  .data     4
  .binary   00000122

  .data     0x811AC6A0
  .data     4
  .binary   0000012c

  .data     0x811AC6C4
  .data     4
  .binary   00000186

  .data     0x811AC6E8
  .data     4
  .binary   000000c8

  .data     0x811AC70C
  .data     4
  .binary   000000c8

  .data     0x811AC730
  .data     4
  .binary   000001e5

  .data     0x811AC754
  .data     4
  .binary   000001a4

  .data     0x811AC778
  .data     4
  .binary   00003e80

  .data     0x811AC79C
  .data     4
  .binary   0000007d

  .data     0x811AC7C0
  .data     4
  .binary   00000014

  .data     0x811AC7E4
  .data     4
  .binary   00002ee0

  .data     0x811AC808
  .data     4
  .binary   00000177

  .data     0x811AC8BC
  .data     4
  .binary   000000c8

  .data     0x811AC8E0
  .data     4
  .binary   00000398

  .data     0x811AC904
  .data     4
  .binary   0000011d

  .data     0x811AC928
  .data     4
  .binary   00000037

  .data     0x811AC94C
  .data     4
  .binary   00000131

  .data     0x811AC970
  .data     4
  .binary   0000014f

  .data     0x811AC994
  .data     4
  .binary   00000140

  .data     0x811AC9B8
  .data     4
  .binary   00000181

  .data     0x811AC9DC
  .data     4
  .binary   000000d7

  .data     0x811ACA90
  .data     4
  .binary   00004a38

  .data     0x811ACC1C
  .data     4
  .binary   000000f5

  .data     0x811ACC40
  .data     4
  .binary   00000082

  .data     0x811ACC64
  .data     4
  .binary   00000073

  .data     0x811ACC88
  .data     4
  .binary   00000073

  .data     0x811ACCAC
  .data     4
  .binary   0000050f

  .data     0x811ACCD0
  .data     4
  .binary   00000019

  .data     0x811ACD3C
  .data     4
  .binary   00009c40

  .data     0x811ACD60
  .data     4
  .binary   00000019

  .data     0x811ACFA0
  .data     4
  .binary   00000118

  .data     0x811ACFC4
  .data     4
  .binary   00000398

  .data     0x811ACFE8
  .data     4
  .binary   000000d2

  .data     0x811AD00C
  .data     4
  .binary   000000d7

  .data     0x811AD030
  .data     4
  .binary   000000e1

  .data     0x811AD054
  .data     4
  .binary   00000140

  .data     0x811AD078
  .data     4
  .binary   000000f5

  .data     0x811AD09C
  .data     4
  .binary   00000104

  .data     0x811AD0C0
  .data     4
  .binary   00000113

  .data     0x811AD0E4
  .data     4
  .binary   00000195

  .data     0x811AD108
  .data     4
  .binary   0000014f

  .data     0x811AD12C
  .data     4
  .binary   0000015e

  .data     0x811AD150
  .data     4
  .binary   0000016d

  .data     0x811AD2DC
  .data     4
  .binary   0000019a

  .data     0x811AD300
  .data     4
  .binary   000001cc

  .data     0x811AD324
  .data     4
  .binary   000001c2

  .data     0x811AD348
  .data     4
  .binary   000001d6

  .data     0x811AD36C
  .data     4
  .binary   000001f4

  .data     0x811AD390
  .data     4
  .binary   0000076c

  .data     0x811AD3B4
  .data     4
  .binary   000002ad

  .data     0x811AD3D8
  .data     4
  .binary   00000230

  .data     0x811AD3FC
  .data     4
  .binary   00000230

  .data     0x811AD420
  .data     4
  .binary   0000023a

  .data     0x811AD444
  .data     4
  .binary   000002e4

  .data     0x811AD468
  .data     4
  .binary   000001b8

  .data     0x811AD48C
  .data     4
  .binary   000001b8

  .data     0x811AD4B0
  .data     4
  .binary   000002ee

  .data     0x811AD4D4
  .data     4
  .binary   000002d0

  .data     0x811AD4F8
  .data     4
  .binary   0000a9ec

  .data     0x811AD51C
  .data     4
  .binary   00000096

  .data     0x811AD540
  .data     4
  .binary   00000028

  .data     0x811AD564
  .data     4
  .binary   00009bdc

  .data     0x811AD588
  .data     4
  .binary   000002cb

  .data     0x811AD63C
  .data     4
  .binary   000001b8

  .data     0x811AD660
  .data     4
  .binary   00000578

  .data     0x811AD684
  .data     4
  .binary   00000258

  .data     0x811AD6A8
  .data     4
  .binary   0000006e

  .data     0x811AD6CC
  .data     4
  .binary   00000244

  .data     0x811AD6F0
  .data     4
  .binary   0000026c

  .data     0x811AD714
  .data     4
  .binary   00000258

  .data     0x811AD738
  .data     4
  .binary   00000280

  .data     0x811AD75C
  .data     4
  .binary   000001cc

  .data     0x811AD810
  .data     4
  .binary   0000d8cc

  .data     0x811AD99C
  .data     4
  .binary   000001f4

  .data     0x811AD9C0
  .data     4
  .binary   00000104

  .data     0x811AD9E4
  .data     4
  .binary   000000f0

  .data     0x811ADA08
  .data     4
  .binary   000000f0

  .data     0x811ADA2C
  .data     4
  .binary   0000076c

  .data     0x811ADA50
  .data     4
  .binary   0000002d

  .data     0x811ADABC
  .data     4
  .binary   00013880

  .data     0x811ADAE0
  .data     4
  .binary   0000002d

  .data     0x811ADD20
  .data     4
  .binary   0000022b

  .data     0x811ADD44
  .data     4
  .binary   00000578

  .data     0x811ADD68
  .data     4
  .binary   000001c2

  .data     0x811ADD8C
  .data     4
  .binary   000001cc

  .data     0x811ADDB0
  .data     4
  .binary   000001d6

  .data     0x811ADDD4
  .data     4
  .binary   00000271

  .data     0x811ADDF8
  .data     4
  .binary   000001f4

  .data     0x811ADE1C
  .data     4
  .binary   00000208

  .data     0x811ADE40
  .data     4
  .binary   0000021c

  .data     0x811ADE64
  .data     4
  .binary   000002ee

  .data     0x811ADE88
  .data     4
  .binary   0000026c

  .data     0x811ADEAC
  .data     4
  .binary   00000280

  .data     0x811ADED0
  .data     4
  .binary   00000294

  .data     0x811AE05C
  .data     4
  .binary   000002d5

  .data     0x811AE080
  .data     4
  .binary   00000546

  .data     0x811AE0A4
  .data     4
  .binary   0000055f

  .data     0x811AE0C8
  .data     4
  .binary   00000578

  .data     0x811AE0EC
  .data     4
  .binary   000005c3

  .data     0x811AE110
  .data     4
  .binary   00001194

  .data     0x811AE134
  .data     4
  .binary   000006d6

  .data     0x811AE158
  .data     4
  .binary   00000640

  .data     0x811AE17C
  .data     4
  .binary   00000640

  .data     0x811AE1A0
  .data     4
  .binary   00000659

  .data     0x811AE1C4
  .data     4
  .binary   00000717

  .data     0x811AE1E8
  .data     4
  .binary   000001f4

  .data     0x811AE20C
  .data     4
  .binary   000001f4

  .data     0x811AE230
  .data     4
  .binary   00000785

  .data     0x811AE254
  .data     4
  .binary   00000744

  .data     0x811AE278
  .data     4
  .binary   00016f30

  .data     0x811AE29C
  .data     4
  .binary   0000012c

  .data     0x811AE2C0
  .data     4
  .binary   00000050

  .data     0x811AE2E4
  .data     4
  .binary   0001368c

  .data     0x811AE308
  .data     4
  .binary   000006b8

  .data     0x811AE3BC
  .data     4
  .binary   00000500

  .data     0x811AE3E0
  .data     4
  .binary   00000dac

  .data     0x811AE404
  .data     4
  .binary   00000640

  .data     0x811AE428
  .data     4
  .binary   00000127

  .data     0x811AE44C
  .data     4
  .binary   00000645

  .data     0x811AE470
  .data     4
  .binary   000006ef

  .data     0x811AE494
  .data     4
  .binary   00000686

  .data     0x811AE4B8
  .data     4
  .binary   00000686

  .data     0x811AE4DC
  .data     4
  .binary   00000555

  .data     0x811AE590
  .data     4
  .binary   0001cafc

  .data     0x811AE71C
  .data     4
  .binary   000005aa

  .data     0x811AE740
  .data     4
  .binary   000003cf

  .data     0x811AE764
  .data     4
  .binary   00000384

  .data     0x811AE788
  .data     4
  .binary   00000384

  .data     0x811AE7AC
  .data     4
  .binary   00001130

  .data     0x811AE7D0
  .data     4
  .binary   0000007d

  .data     0x811AE83C
  .data     4
  .binary   0003d090

  .data     0x811AE860
  .data     4
  .binary   00000064

  .data     0x811AEAA0
  .data     4
  .binary   000005dc

  .data     0x811AEAC4
  .data     4
  .binary   00000d7a

  .data     0x811AEAE8
  .data     4
  .binary   0000054b

  .data     0x811AEB0C
  .data     4
  .binary   00000564

  .data     0x811AEB30
  .data     4
  .binary   0000053c

  .data     0x811AEB54
  .data     4
  .binary   00000681

  .data     0x811AEB78
  .data     4
  .binary   000005c3

  .data     0x811AEB9C
  .data     4
  .binary   000005eb

  .data     0x811AEBC0
  .data     4
  .binary   00000636

  .data     0x811AEBE4
  .data     4
  .binary   00000753

  .data     0x811AEC08
  .data     4
  .binary   0000069f

  .data     0x811AEC2C
  .data     4
  .binary   000006bd

  .data     0x811AEC50
  .data     4
  .binary   000006f4

  .data     0
  .data     0
