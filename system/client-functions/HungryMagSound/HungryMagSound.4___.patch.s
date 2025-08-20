.meta name="MAG alert"
.meta description="Plays a sound when\nyour MAG is hungry"
# Original code by Ralf @ GC-Forever and Aleron Ives
# https://www.gc-forever.com/forums/viewtopic.php?t=2050
# https://www.gc-forever.com/forums/viewtopic.php?t=2049
# Xbox port by fuzziqersoftware

.versions 4OJB 4OJD 4OJU 4OED 4OEU 4OPD 4OPU

entry_ptr:
reloc0:
  .offsetof start
start:
  .include  WriteCodeBlocksXB

  .data     <VERS 0x00180EF5 0x00181075 0x00181125 0x00181065 0x00181095 0x00181085 0x00181055>
  .data     0x0A
  .address  <VERS 0x00180EF5 0x00181075 0x00181125 0x00181065 0x00181095 0x00181085 0x00181055>
hook_call:
  jmp       hook1
  int       3
  int       3
hook_ret_sound:
  add       esp, 0x10
hook_ret_no_sound:

  .data     <VERS 0x00181024 0x001811A4 0x00181254 0x00181194 0x001811C4 0x001811B4 0x00181184>
  .deltaof  hook1, hook1_end
  .address  <VERS 0x00181024 0x001811A4 0x00181254 0x00181194 0x001811C4 0x001811B4 0x00181184>
hook1:
  xor       eax, eax
  mov       dword [esi + 0x00000194], eax
  jmp       hook2
hook1_end:

  .data     <VERS 0x00181092 0x00181212 0x001812C2 0x00181202 0x00181232 0x00181222 0x001811F2>
  .deltaof  hook2, hook2_end
  .address  <VERS 0x00181092 0x00181212 0x001812C2 0x00181202 0x00181232 0x00181222 0x001811F2>
hook2:
  mov       edx, dword [esi + 0x000000F0]
  movzx     edx, word [edx + 0x0000001C]  # edx = this->owner_player->entity_id
  jmp       hook3
hook2_end:

  .data     <VERS 0x001810F1 0x00181271 0x00181321 0x00181261 0x00181291 0x00181281 0x00181251>
  .deltaof  hook3, hook3_end
  .address  <VERS 0x001810F1 0x00181271 0x00181321 0x00181261 0x00181291 0x00181281 0x00181251>
hook3:
  cmp       [<VERS 0x0071EFC0 0x0071F620 0x00727164 0x00724660 0x00723EE4 0x00724660 0x007249E4>], edx  # local_client_id
  jmp       hook4
hook3_end:

  .data     <VERS 0x001811D7 0x00181357 0x00181407 0x00181347 0x00181377 0x00181367 0x00181337>
  .deltaof  hook4, hook4_end
  .address  <VERS 0x001811D7 0x00181357 0x00181407 0x00181347 0x00181377 0x00181367 0x00181337>
hook4:
  jne       hook_ret_no_sound
  jmp       hook5
hook4_end:

  .data     <VERS 0x001811F3 0x00181373 0x00181423 0x00181363 0x00181393 0x00181383 0x00181353>
  .deltaof  hook5, hook5_end
  .address  <VERS 0x001811F3 0x00181373 0x00181423 0x00181363 0x00181393 0x00181383 0x00181353>
hook5:
  push      eax
  push      eax
  push      eax
  add       al, 0x8D
  push      eax
  mov       edx, <VERS 0x002E9230 0x002E9DB0 0x002EB390 0x002EB190 0x002EB3B0 0x002EB1C0 0x002EB3E0>  # play_sound
  jmp       hook6
hook5_end:

  .data     <VERS 0x00181211 0x00181391 0x00181441 0x00181381 0x001813B1 0x001813A1 0x00181371>
  .deltaof  hook6, hook6_end
  .address  <VERS 0x00181211 0x00181391 0x00181441 0x00181381 0x001813B1 0x001813A1 0x00181371>
hook6:
  call      edx
  jmp       hook_ret_sound
hook6_end:

  .data     0x00000000
  .data     0x00000000
