.meta name="Enemy HP bars"
.meta description="Shows HP bars in\nenemy info windows"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049

.versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksGC

  .label    hook_addr, 0x8000B650
  .label    sprintf, <VERS 0x80395EFC 0x80398904 0x8039A7A4 0x8039A554 0x803971CC 0x80397224 0x8039A924 0x80399414>

  .data     hook_addr
  .deltaof  hooks_start, hooks_end
  .address  hook_addr
hooks_start:
hook1:
  lis       r5, 0x8001
  lwz       r3, [r5 - 0x4944]
  mr        r30, r31
  lha       r6, [r30 + 0x032C]
  b         entry_merge
hook2:
  lha       r6, [r30 + 0x02B8]
  lis       r5, 0x8001
  stw       [r5 - 0x4944], r3
entry_merge:
  mflr      r0
  stw       [r5 - 0x4940], r0
  mr        r5, r3
  lha       r7, [r30 + 0x02B8]
  lis       r4, 0x8000
  ori       r4, r4, 0xB6AC  # r4 = &hp_format_str
  addi      r3, r4, 0x0018  # r3 = dest buffer
  crxor     crb6, crb6, crb6
  bl        sprintf
  lis       r4, 0x8000
  ori       r4, r4, 0xB6C4
  mr        r3, r28
  lwz       r0, [r4 - 0x0004]
  mtlr      r0
  blr
hp_format_str:
  .binary   "%s\n\nHP:%d/%d"00000000
hooks_end:

  .label    hook1_call, <VERS 0x80261260 0x80261E38 0x80262E80 0x80262C34 0x80261B38 0x80261B38 0x80262EF8 0x80262740>
  .data     hook1_call
  .data     0x00000004
  .address  hook1_call
  bl        hook1

  .label    flag_clear_call, <VERS 0x802612C4 0x80261E9C 0x80262EE4 0x80262C98 0x80261B9C 0x80261B9C 0x80262F5C 0x802627A4>
  .data     flag_clear_call
  .data     0x00000004
  .address  flag_clear_call
  bl        [<VERS 80242804 802431E4 80243548 80243ED8 802430E0 802430E0 8024420C 80243A54>]

  .label    hook2_call, <VERS 0x80261420 0x80261FF8 0x80263040 0x80262DF4 0x80261CF8 0x80261CF8 0x802630B8 0x80262900>
  .data     hook2_call
  .data     0x00000004
  .address  hook2_call
  bl        hook2

  .data     <VERS 0x804CAE40 0x804CE590 0x804D0AE0 0x804D0880 0x804CB610 0x804CBAF0 0x804D0158 0x804D0548>
  .data     0x00000004
  .float    75

  .data     <VERS 0x804CAE4C 0x804CE59C 0x804D0AEC 0x804D088C 0x804CB61C 0x804CBAFC 0x804D0164 0x804D0554>
  .data     0x00000004
  .float    75

  .data     <VERS 0x804CAE58 0x804CE5A8 0x804D0AF8 0x804D0898 0x804CB628 0x804CBB08 0x804D0170 0x804D0560>
  .data     0x00000004
  .float    75

  .data     <VERS 0x804CAE64 0x804CE5B4 0x804D0B04 0x804D08A4 0x804CB634 0x804CBB14 0x804D017C 0x804D056C>
  .data     0x00000004
  .float    75

  .data     <VERS 0x804CAE70 0x804CE5C0 0x804D0B10 0x804D08B0 0x804CB640 0x804CBB20 0x804D0188 0x804D0578>
  .data     0x00000004
  .float    75

  .data     <VERS 0x804CAEA0 0x804CE5F0 0x804D0B40 0x804D08E0 0x804CB670 0x804CBB50 0x804D01B8 0x804D05A8>
  .data     0x00000004
  .float    75

  .data     <VERS 0x804CAED0 0x804CE620 0x804D0B70 0x804D0910 0x804CB6A0 0x804CBB80 0x804D01E8 0x804D05D8>
  .data     0x00000004
  .float    75

  .data     <VERS 0x804CAF00 0x804CE650 0x804D0BA0 0x804D0940 0x804CB6D0 0x804CBBB0 0x804D0218 0x804D0608>
  .data     0x00000004
  .float    62

  .data     <VERS 0x804CAF1C 0x804CE66C 0x804D0BBC 0x804D095C 0x804CB6EC 0x804CBBCC 0x804D0234 0x804D0624>
  .data     0x00000004
  .data     0xFF00FF15

  .data     <VERS 0x805CBFBC 0x805D65BC 0x805DDA5C 0x805DD7FC 0x805CC8C4 0x805D38E4 0x805DD104 0x805D9344>
  .data     0x00000004
  .float    96

  .data     0x00000000
  .data     0x00000000
