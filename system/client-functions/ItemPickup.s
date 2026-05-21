# Original code by Ralf @ GC-Forever
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049
# Xbox port by fuzziqersoftware

.meta visibility="all"
.meta name="Item pickup"
.meta description="Prevents picking\nup items unless you\nhold the Z button"


entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocks



  .versions 3OJ2 3OJ3 3OJ4 3OJ5 3OE0 3OE1 3OE2 3OP0

  .label    hook_loc, 0x8000B938
  .data     hook_loc
  .deltaof  hook_start, hook_end
  .address  hook_loc
hook_start:
  addi      r3, r28, 0x0550
  li        r4, 0x0100
  bl        [<VERS 8034ED18 8035011C 80351684 8035142C 8034FBCC 8034FC10 803517F8 80350BEC>]
  cmpwi     r3, 0
  beq       skip
  mr        r3, r28
  b         [<VERS 801B56B4 801B5B08 801B7CC0 801B5BD4 801B5AA0 801B5AA0 801B5C38 801B60F4>]
skip:
  b         [<VERS 801B56C4 801B5B18 801B7CD0 801B5BE4 801B5AB0 801B5AB0 801B5C48 801B6104>]
hook_end:

  .label    hook_call, <VERS 0x801B56B0 0x801B5B04 0x801B7CBC 0x801B5BD0 0x801B5A9C 0x801B5A9C 0x801B5C34 0x801B60F0>
  .data     hook_call
  .data     0x00000004
  .address  hook_call
  b         hook_start

  .data     <VERS 0x8024C384 0x8024CDD0 0x8024DD28 0x8024DAC4 0x8024CC0C 0x8024CC0C 0x8024DD88 0x8024D5D0>
  .data     0x00000004
  li        r4, 8



  .versions 4OED 4OEU 4OJB 4OJD 4OJU 4OPD 4OPU

  .data     <VERS 0x001FDC99 0x001FDC99 0x001FDA89 0x001FDBE9 0x001FDE69 0x001FDCB9 0x001FDD29>
  .data     0x07
  .binary   E8880100009090

  .data     <VERS 0x001FDE26 0x001FDE26 0x001FDC16 0x001FDD76 0x001FDFF6 0x001FDE46 0x001FDEB6>
  .data     0x0A
  .binary   8B866C05000085C0EB46

  .data     <VERS 0x001FDE76 0x001FDE76 0x001FDC66 0x001FDDC6 0x001FE046 0x001FDE96 0x001FDF06>
  .data     0x09
  .binary   74038A40013408EB46

  .data     <VERS 0x001FDEC5 0x001FDEC5 0x001FDCB5 0x001FDE15 0x001FE095 0x001FDEE5 0x001FDF55>
  .data     0x0A
  .binary   7507F68624030000E0C3

  .data     <VERS 0x0025ADAD 0x0025AEED 0x0025A94D 0x0025ACCD 0x0025B07D 0x0025ADCD 0x0025AF1D>
  .data     0x01
  .binary   00



  .all_versions

  .data     0x00000000
  .data     0x00000000
