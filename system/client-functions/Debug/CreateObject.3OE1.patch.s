.meta hide_from_patches_menu
.meta name="CreateObject"
.meta description=""

entry_ptr:
reloc0:
  .offsetof start
start:
  mflr    r0
  b       get_data
get_data_ret:
  mflr    r3
  mtlr    r0
  lwz     r0, [r3]
  mtctr   r0
  addi    r3, r3, 4
  bctr

get_data:
  bl      get_data_ret
  .data   0x8020C158  # construct_dat_object_from_args
base_type_high:
  .data   0xFFFF0000  # base_type, set_flags
floor_low:
  .data   0x0000FFFF  # index, floor
  .data   0x00000000  # entity_id, group
  .data   0x00000000  # room, unknown_a3
pos_x:
  .float  0.0  # pos.x
pos_y:
  .float  0.0  # pos.y
pos_z:
  .float  0.0  # pos.z
angle_x:
  .data   0x00000000  # angle.x
angle_y:
  .data   0x00000000  # angle.y
angle_z:
  .data   0x00000000  # angle.z
param1:
  .float  0.0  # param1
param2:
  .float  0.0  # param2
param3:
  .float  0.0  # param3
param4:
  .data   0  # param4
param5:
  .data   0  # param5
param6:
  .data   0  # param6
  .data   0  # unused_obj_ptr
