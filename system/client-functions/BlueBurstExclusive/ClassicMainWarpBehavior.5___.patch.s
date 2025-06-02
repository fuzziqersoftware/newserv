# This patch disables the logic that causes all unlockable areas to be open by
# default for all players, instead restoring the logic that checks quest flags
# to open areas (as previous PSO versions used).

# This patch is intended to be used in the BBRequiredPatches field in
# config.json if you want the classic behavior, hence the presence of the
# hide_from_patches_menu directive here.

.meta name="Classic main warp behavior"
.meta description=""
.meta hide_from_patches_menu

.versions 59NJ 59NL

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksBB
  .data     <VERS 0x0064A642 0x0064A5DE>  # Episode 1
  .data     1
  .binary   01
  .data     <VERS 0x0064A4AC 0x0064A448>  # Episode 2
  .data     2
  .binary   0100
  .data     <VERS 0x0064A58D 0x0064A529>  # Episode 4
  .data     1
  .binary   01
  .data     <VERS 0x0064A6BC 0x0064A658>  # Non-Normal difficulty check
  .data     2
  nop
  nop
  .data     0
  .data     0
