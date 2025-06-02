# (uint16_t entity_id @ eax) -> TObjectV00b441c0* @ eax
# Preserves all registers except eax
get_enemy_entity:
  push      esi
  push      edi
  push      edx
  push      ecx
  xor       edx, edx
  xchg      edx, eax
  cmp       edx, 0x1000
  jl        done
  cmp       edx, 0x4000
  jge       done

  mov       esi, [0x00AAE168]  # bs_low = next_player_entity_index
  mov       edi, [0x00AAE164]
  lea       edi, [edi + esi - 1]  # bs_high = next_player_entity_index + next_enemy_entity_index - 1
bs_again:
  cmp       esi, edi
  jge       bs_done
  lea       ecx, [esi + edi]
  shr       ecx, 1
  mov       eax, [ecx * 4 + 0x00AAD720]  # all_entities[ecx]
  cmp       [eax + 0x1C], dx
  jge       bs_not_less
  lea       esi, [ecx + 1]
  jmp       bs_again
bs_not_less:
  mov       edi, ecx
  jmp       bs_again
bs_done:

  mov       eax, [esi * 4 + 0x00AAD720]  # all_entities[bs_low]
  test      eax, eax
  je        done
  xor       ecx, ecx
  cmp       [eax + 0x1C], dx
  cmovne    eax, ecx

done:
  pop       ecx
  pop       edx
  pop       edi
  pop       esi
