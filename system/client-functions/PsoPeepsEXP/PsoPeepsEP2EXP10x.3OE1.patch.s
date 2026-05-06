.meta name="PSO Peeps EP2 10x EXP"
.meta description="Sets EP2 enemy EXP\nto 10x for GC crossplay"

entry_ptr:
reloc0:
  .offsetof start

start:
  .include  WriteCodeBlocksGC

  # PSO Peeps GC Plus USA / 3OE1
  # Source table: BattleParamEntry_lab_on.dat
  # Active online battle-param table loaded at 0x811AB7C0
  # EXP field offset within each 0x24-byte row is +0x1C
  # Generated from clean BattleParamEntry_lab_on.dat; multiplier=10x

  .data     0x811AB7DC
  .data     4
  .binary   0000000a

  .data     0x811AB800
  .data     4
  .binary   0000003c

  .data     0x811AB824
  .data     4
  .binary   00000064

  .data     0x811AB848
  .data     4
  .binary   00000078

  .data     0x811AB86C
  .data     4
  .binary   00000064

  .data     0x811AB890
  .data     4
  .binary   000005dc

  .data     0x811AB8B4
  .data     4
  .binary   000000be

  .data     0x811AB8D8
  .data     4
  .binary   00000064

  .data     0x811AB8FC
  .data     4
  .binary   000000a0

  .data     0x811AB920
  .data     4
  .binary   000000aa

  .data     0x811AB944
  .data     4
  .binary   000000aa

  .data     0x811AB968
  .data     4
  .binary   00000014

  .data     0x811AB98C
  .data     4
  .binary   00000014

  .data     0x811AB9B0
  .data     4
  .binary   00000190

  .data     0x811AB9D4
  .data     4
  .binary   00000096

  .data     0x811AB9F8
  .data     4
  .binary   000012c0

  .data     0x811ABA1C
  .data     4
  .binary   00000064

  .data     0x811ABA40
  .data     4
  .binary   0000001e

  .data     0x811ABA64
  .data     4
  .binary   0000251c

  .data     0x811ABA88
  .data     4
  .binary   000000d2

  .data     0x811ABB3C
  .data     4
  .binary   00000028

  .data     0x811ABB60
  .data     4
  .binary   00000a00

  .data     0x811ABB84
  .data     4
  .binary   000001fe

  .data     0x811ABBA8
  .data     4
  .binary   00000014

  .data     0x811ABBCC
  .data     4
  .binary   0000006e

  .data     0x811ABBF0
  .data     4
  .binary   000000be

  .data     0x811ABC14
  .data     4
  .binary   00003a98

  .data     0x811ABCC8
  .data     4
  .binary   00000550

  .data     0x811ABD10
  .data     4
  .binary   000001ea

  .data     0x811ABD34
  .data     4
  .binary   000001c2

  .data     0x811ABE0C
  .data     4
  .binary   000080e8

  .data     0x811ABE9C
  .data     4
  .binary   00000122

  .data     0x811ABEC0
  .data     4
  .binary   0000001e

  .data     0x811ABEE4
  .data     4
  .binary   00000046

  .data     0x811ABF08
  .data     4
  .binary   00000050

  .data     0x811AC004
  .data     4
  .binary   000001d6

  .data     0x811AC028
  .data     4
  .binary   00000078

  .data     0x811AC04C
  .data     4
  .binary   00000096

  .data     0x811AC070
  .data     4
  .binary   00000208

  .data     0x811AC0DC
  .data     4
  .binary   0000012c

  .data     0x811AC100
  .data     4
  .binary   00000078

  .data     0x811AC124
  .data     4
  .binary   00000014

  .data     0x811AC148
  .data     4
  .binary   00000140

  .data     0x811AC16C
  .data     4
  .binary   00000190

  .data     0x811AC190
  .data     4
  .binary   000001f4

  .data     0x811AC1B4
  .data     4
  .binary   00000226

  .data     0x811AC220
  .data     4
  .binary   00000064

  .data     0x811AC244
  .data     4
  .binary   000003e8

  .data     0x811AC268
  .data     4
  .binary   0000006e

  .data     0x811AC28C
  .data     4
  .binary   00000082

  .data     0x811AC2B0
  .data     4
  .binary   00000046

  .data     0x811AC2D4
  .data     4
  .binary   00000078

  .data     0x811AC2F8
  .data     4
  .binary   000000b4

  .data     0x811AC31C
  .data     4
  .binary   000000d2

  .data     0x811AC340
  .data     4
  .binary   0000008c

  .data     0x811AC364
  .data     4
  .binary   00000096

  .data     0x811AC388
  .data     4
  .binary   0000003c

  .data     0x811AC3AC
  .data     4
  .binary   00000046

  .data     0x811AC3D0
  .data     4
  .binary   00000050

  .data     0x811AC55C
  .data     4
  .binary   00000172

  .data     0x811AC580
  .data     4
  .binary   000001c2

  .data     0x811AC5A4
  .data     4
  .binary   00000208

  .data     0x811AC5C8
  .data     4
  .binary   00000226

  .data     0x811AC5EC
  .data     4
  .binary   00000208

  .data     0x811AC610
  .data     4
  .binary   00000b0e

  .data     0x811AC634
  .data     4
  .binary   00000294

  .data     0x811AC658
  .data     4
  .binary   00000208

  .data     0x811AC67C
  .data     4
  .binary   00000244

  .data     0x811AC6A0
  .data     4
  .binary   00000258

  .data     0x811AC6C4
  .data     4
  .binary   00000276

  .data     0x811AC6E8
  .data     4
  .binary   0000017c

  .data     0x811AC70C
  .data     4
  .binary   0000017c

  .data     0x811AC730
  .data     4
  .binary   000003f2

  .data     0x811AC754
  .data     4
  .binary   00000258

  .data     0x811AC778
  .data     4
  .binary   00007d00

  .data     0x811AC79C
  .data     4
  .binary   000000fa

  .data     0x811AC7C0
  .data     4
  .binary   00000028

  .data     0x811AC7E4
  .data     4
  .binary   00009858

  .data     0x811AC808
  .data     4
  .binary   00000294

  .data     0x811AC8BC
  .data     4
  .binary   000001a4

  .data     0x811AC8E0
  .data     4
  .binary   00001400

  .data     0x811AC904
  .data     4
  .binary   000004a6

  .data     0x811AC928
  .data     4
  .binary   00000064

  .data     0x811AC94C
  .data     4
  .binary   00000212

  .data     0x811AC970
  .data     4
  .binary   00000294

  .data     0x811AC994
  .data     4
  .binary   0000afc8

  .data     0x811ACA48
  .data     4
  .binary   00000a1e

  .data     0x811ACA90
  .data     4
  .binary   00000488

  .data     0x811ACAB4
  .data     4
  .binary   00000442

  .data     0x811ACB8C
  .data     4
  .binary   00012cc8

  .data     0x811ACC1C
  .data     4
  .binary   0000033e

  .data     0x811ACC40
  .data     4
  .binary   000001d6

  .data     0x811ACC64
  .data     4
  .binary   000001d6

  .data     0x811ACC88
  .data     4
  .binary   000001e0

  .data     0x811ACD84
  .data     4
  .binary   0000046a

  .data     0x811ACDA8
  .data     4
  .binary   00000226

  .data     0x811ACDCC
  .data     4
  .binary   00000258

  .data     0x811ACDF0
  .data     4
  .binary   000004ba

  .data     0x811ACE5C
  .data     4
  .binary   00000352

  .data     0x811ACE80
  .data     4
  .binary   00000226

  .data     0x811ACEA4
  .data     4
  .binary   000001c2

  .data     0x811ACEC8
  .data     4
  .binary   00000370

  .data     0x811ACEEC
  .data     4
  .binary   000003f2

  .data     0x811ACF10
  .data     4
  .binary   0000049c

  .data     0x811ACF34
  .data     4
  .binary   000004ec

  .data     0x811ACF7C
  .data     4
  .binary   00000032

  .data     0x811ACFA0
  .data     4
  .binary   00000208

  .data     0x811ACFC4
  .data     4
  .binary   000007d0

  .data     0x811ACFE8
  .data     4
  .binary   00000212

  .data     0x811AD00C
  .data     4
  .binary   00000230

  .data     0x811AD030
  .data     4
  .binary   000001c2

  .data     0x811AD054
  .data     4
  .binary   00000226

  .data     0x811AD078
  .data     4
  .binary   0000028a

  .data     0x811AD09C
  .data     4
  .binary   000002bc

  .data     0x811AD0C0
  .data     4
  .binary   00000226

  .data     0x811AD0E4
  .data     4
  .binary   00000258

  .data     0x811AD108
  .data     4
  .binary   000001c2

  .data     0x811AD12C
  .data     4
  .binary   000001d6

  .data     0x811AD150
  .data     4
  .binary   000001e0

  .data     0x811AD2DC
  .data     4
  .binary   00000366

  .data     0x811AD300
  .data     4
  .binary   000003ca

  .data     0x811AD324
  .data     4
  .binary   0000041a

  .data     0x811AD348
  .data     4
  .binary   00000442

  .data     0x811AD36C
  .data     4
  .binary   0000041a

  .data     0x811AD390
  .data     4
  .binary   00000f0a

  .data     0x811AD3B4
  .data     4
  .binary   0000047e

  .data     0x811AD3D8
  .data     4
  .binary   0000041a

  .data     0x811AD3FC
  .data     4
  .binary   00000460

  .data     0x811AD420
  .data     4
  .binary   00000474

  .data     0x811AD444
  .data     4
  .binary   000004a6

  .data     0x811AD468
  .data     4
  .binary   0000037a

  .data     0x811AD48C
  .data     4
  .binary   0000037a

  .data     0x811AD4B0
  .data     4
  .binary   00000672

  .data     0x811AD4D4
  .data     4
  .binary   0000047e

  .data     0x811AD4F8
  .data     4
  .binary   000157c0

  .data     0x811AD51C
  .data     4
  .binary   00000258

  .data     0x811AD540
  .data     4
  .binary   00000258

  .data     0x811AD564
  .data     4
  .binary   000186a0

  .data     0x811AD588
  .data     4
  .binary   000004ce

  .data     0x811AD63C
  .data     4
  .binary   000003a2

  .data     0x811AD660
  .data     4
  .binary   00001d88

  .data     0x811AD684
  .data     4
  .binary   0000074e

  .data     0x811AD6A8
  .data     4
  .binary   0000012c

  .data     0x811AD6CC
  .data     4
  .binary   0000042e

  .data     0x811AD6F0
  .data     4
  .binary   000004ce

  .data     0x811AD714
  .data     4
  .binary   0001e848

  .data     0x811AD7C8
  .data     4
  .binary   00000df2

  .data     0x811AD810
  .data     4
  .binary   00000726

  .data     0x811AD834
  .data     4
  .binary   000006d6

  .data     0x811AD90C
  .data     4
  .binary   000249f0

  .data     0x811AD99C
  .data     4
  .binary   00000596

  .data     0x811AD9C0
  .data     4
  .binary   0000038e

  .data     0x811AD9E4
  .data     4
  .binary   000003de

  .data     0x811ADA08
  .data     4
  .binary   000003f2

  .data     0x811ADB04
  .data     4
  .binary   000006fe

  .data     0x811ADB28
  .data     4
  .binary   00000442

  .data     0x811ADB4C
  .data     4
  .binary   0000047e

  .data     0x811ADB70
  .data     4
  .binary   00000762

  .data     0x811ADBDC
  .data     4
  .binary   000005aa

  .data     0x811ADC00
  .data     4
  .binary   00000442

  .data     0x811ADC24
  .data     4
  .binary   000003b6

  .data     0x811ADC48
  .data     4
  .binary   000005d2

  .data     0x811ADC6C
  .data     4
  .binary   00000672

  .data     0x811ADC90
  .data     4
  .binary   0000073a

  .data     0x811ADCB4
  .data     4
  .binary   0000079e

  .data     0x811ADCFC
  .data     4
  .binary   00000032

  .data     0x811ADD20
  .data     4
  .binary   0000041a

  .data     0x811ADD44
  .data     4
  .binary   00000b22

  .data     0x811ADD68
  .data     4
  .binary   0000042e

  .data     0x811ADD8C
  .data     4
  .binary   00000456

  .data     0x811ADDB0
  .data     4
  .binary   000003ac

  .data     0x811ADDD4
  .data     4
  .binary   00000442

  .data     0x811ADDF8
  .data     4
  .binary   000004ba

  .data     0x811ADE1C
  .data     4
  .binary   000004f6

  .data     0x811ADE40
  .data     4
  .binary   00000438

  .data     0x811ADE64
  .data     4
  .binary   0000047e

  .data     0x811ADE88
  .data     4
  .binary   000003ca

  .data     0x811ADEAC
  .data     4
  .binary   000003de

  .data     0x811ADED0
  .data     4
  .binary   000003f2

  .data     0x811AE05C
  .data     4
  .binary   000005dc

  .data     0x811AE080
  .data     4
  .binary   00000bf4

  .data     0x811AE0A4
  .data     4
  .binary   00000ce4

  .data     0x811AE0C8
  .data     4
  .binary   00000d5c

  .data     0x811AE0EC
  .data     4
  .binary   00000ce4

  .data     0x811AE110
  .data     4
  .binary   00002db4

  .data     0x811AE134
  .data     4
  .binary   00000e10

  .data     0x811AE158
  .data     4
  .binary   00000ce4

  .data     0x811AE17C
  .data     4
  .binary   00000be0

  .data     0x811AE1A0
  .data     4
  .binary   00000c08

  .data     0x811AE1C4
  .data     4
  .binary   00000e88

  .data     0x811AE1E8
  .data     4
  .binary   000003e8

  .data     0x811AE20C
  .data     4
  .binary   000003e8

  .data     0x811AE230
  .data     4
  .binary   000013ec

  .data     0x811AE254
  .data     4
  .binary   00000e10

  .data     0x811AE278
  .data     4
  .binary   0002bf20

  .data     0x811AE29C
  .data     4
  .binary   00000258

  .data     0x811AE2C0
  .data     4
  .binary   000000a0

  .data     0x811AE2E4
  .data     4
  .binary   00033450

  .data     0x811AE308
  .data     4
  .binary   00000f00

  .data     0x811AE3BC
  .data     4
  .binary   00000b7c

  .data     0x811AE3E0
  .data     4
  .binary   00002800

  .data     0x811AE404
  .data     4
  .binary   00001680

  .data     0x811AE428
  .data     4
  .binary   000001f4

  .data     0x811AE44C
  .data     4
  .binary   00000d20

  .data     0x811AE470
  .data     4
  .binary   00000f00

  .data     0x811AE494
  .data     4
  .binary   0003c4d8

  .data     0x811AE548
  .data     4
  .binary   00002a6c

  .data     0x811AE590
  .data     4
  .binary   00001608

  .data     0x811AE5B4
  .data     4
  .binary   00001518

  .data     0x811AE68C
  .data     4
  .binary   00073f78

  .data     0x811AE71C
  .data     4
  .binary   00001158

  .data     0x811AE740
  .data     4
  .binary   00000c30

  .data     0x811AE764
  .data     4
  .binary   00000c30

  .data     0x811AE788
  .data     4
  .binary   00000c6c

  .data     0x811AE884
  .data     4
  .binary   00000d98

  .data     0x811AE8A8
  .data     4
  .binary   00000d5c

  .data     0x811AE8CC
  .data     4
  .binary   00000e10

  .data     0x811AE8F0
  .data     4
  .binary   000016bc

  .data     0x811AE95C
  .data     4
  .binary   00001194

  .data     0x811AE980
  .data     4
  .binary   00000d5c

  .data     0x811AE9A4
  .data     4
  .binary   000007d0

  .data     0x811AE9C8
  .data     4
  .binary   0000120c

  .data     0x811AE9EC
  .data     4
  .binary   000013ec

  .data     0x811AEA10
  .data     4
  .binary   00001644

  .data     0x811AEA34
  .data     4
  .binary   00001770

  .data     0x811AEA7C
  .data     4
  .binary   00000032

  .data     0x811AEAA0
  .data     4
  .binary   00000ce4

  .data     0x811AEAC4
  .data     4
  .binary   000021fc

  .data     0x811AEAE8
  .data     4
  .binary   00000d20

  .data     0x811AEB0C
  .data     4
  .binary   00000d98

  .data     0x811AEB30
  .data     4
  .binary   00000a78

  .data     0x811AEB54
  .data     4
  .binary   00000d5c

  .data     0x811AEB78
  .data     4
  .binary   00000ec4

  .data     0x811AEB9C
  .data     4
  .binary   00000f78

  .data     0x811AEBC0
  .data     4
  .binary   00000b90

  .data     0x811AEBE4
  .data     4
  .binary   00000e10

  .data     0x811AEC08
  .data     4
  .binary   00000bf4

  .data     0x811AEC2C
  .data     4
  .binary   00000c30

  .data     0x811AEC50
  .data     4
  .binary   00000c6c

  .data     0
  .data     0
