.meta name="Movement"
.meta description="Fix movement dead\nzone thresholds"

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksXB

  .data     0x003067D8
  .deltaof  code_start, code_end

code_start:
  .include  MovementXB
code_end:
