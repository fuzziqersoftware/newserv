.meta name="Enemy HP bars"
.meta description="Shows HP bars in\nenemy info windows"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049
# Xbox port by fuzziqersoftware

.versions 4OED 4OEU 4OJB 4OJD 4OJU 4OPD 4OPU

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksXB
  .data     <VERS 0x0026B063 0x0026B193 0x0026ABA3 0x0026AF13 0x0026B2F3 0x0026B083 0x0026B193>
  .data     0x00000001
  .binary   C0
  .data     <VERS 0x0026B06C 0x0026B19C 0x0026ABAC 0x0026AF1C 0x0026B2FC 0x0026B08C 0x0026B19C>
  .data     0x00000001
  .binary   FA
  .data     <VERS 0x0026B266 0x0026B396 0x0026ADA6 0x0026B116 0x0026B4F6 0x0026B286 0x0026B396>
  .data     0x00000004
  .binary   836004FD
  .data     <VERS 0x0054A92C 0x0054A1CC 0x00545334 0x005459C4 0x0054D4AC 0x0054A92C 0x0054ACCC>
  .data     0x00000004
  .data     0x42960000
  .data     <VERS 0x0054A95C 0x0054A1FC 0x00545364 0x005459F4 0x0054D4DC 0x0054A95C 0x0054ACFC>
  .data     0x00000004
  .data     0x42960000
  .data     <VERS 0x0054A98C 0x0054A22C 0x00545394 0x00545A24 0x0054D50C 0x0054A98C 0x0054AD2C>
  .data     0x00000004
  .data     0x42960000
  .data     <VERS 0x0054A9BC 0x0054A25C 0x005453C4 0x00545A54 0x0054D53C 0x0054A9BC 0x0054AD5C>
  .data     0x00000004
  .data     0x42960000
  .data     <VERS 0x0054A9EC 0x0054A28C 0x005453F4 0x00545A84 0x0054D56C 0x0054A9EC 0x0054AD8C>
  .data     0x00000004
  .data     0x42780000
  .data     <VERS 0x0054AA08 0x0054A2A8 0x00545410 0x00545AA0 0x0054D588 0x0054AA08 0x0054ADA8>
  .data     0x00000004
  .data     0xFF00FF15

  .data     0x00010C00
  .deltaof  str_data_start, str_data_end
str_data_start:
  .data     <VERS 0x00318308 0x00317D7A 0x00313B22 0x00314722 0x00317D7A 0x00318338 0x00318858>  # sprintf
  .data     <VERS 0x00264E80 0x00264F80 0x002649C0 0x00264D80 0x00265130 0x00264EA0 0x00264FD0>  # Original function for on_window_created callsite
  .data     0x00000000
  .binary   "%s\n\nHP:%d/%d"
  .data     0x00000000
  .data     0x00000000
str_data_end:

  # WARNING: FlickeringStatusIcons patch starts immediately after this segment;
  # if the size of this is changed, that patch will have to be changed too
  .data     <VERS 0x002DB050 0x002DB550 0x002D90E0 0x002D9CB0 0x002DB580 0x002DB080 0x002DB5D0>
  .deltaof  new_code_start, new_code_end
new_code_start:
  # Replacement for handle_0E (do nothing)
  ret

  # Call table: 2 functions (on_window_created, on_hp_updated)
  jmp       on_window_created

on_hp_updated:
  call      rewrite_string
  movsx     ecx, word [ebp + 0x02BC] # Replaced opcode at callsite
  ret

on_window_created:
  mov       [0x00010C08], eax  # prev_desc
  push      ebp
  mov       ebp, ebx
  call      rewrite_string
  pop       ebp
  mov       dword [esp + 4], 0x00010C1C  # Change first argument to desc_buf
  jmp       [0x00010C04]  # Call original function

rewrite_string:
  movsx     eax, word [ebp + 0x02BC]  # max HP
  push      eax
  movsx     eax, word [ebp + 0x0330]  # current HP
  push      eax
  push      dword [0x00010C08]  # prev_desc
  push      0x00010C0C  # desc_template
  push      0x00010C1C  # desc_buf
  call      [0x00010C00]  # sprintf
  add       esp, 0x14
  ret
new_code_end:

  .data     <VERS 0x0026B241 0x0026B371 0x0026AD81 0x0026B0F1 0x0026B4D1 0x0026B261 0x0026B371>
  .data     0x00000007
  nop
  nop
  .binary   <VERS E80BFE0600 E8DB010700 E85BE30600 E8BBEB0600 E8AB000700 E81BFE0600 E85B020700>  # call on_hp_updated

  .data     <VERS 0x0026B028 0x0026B158 0x0026AB68 0x0026AED8 0x0026B2B8 0x0026B048 0x0026B158>
  .data     0x00000005
  .binary   <VERS E824000700 E8F4030700 E874E50600 E8D4ED0600 E8C4020700 E834000700 E874040700>  # call on_window_created

  .data     0x00000000
  .data     0x00000000
