#pragma once

#include <stdint.h>

#include <memory>
#include <vector>

#include "DataIndexes.hh"
#include "DeckState.hh"
#include "PlayerState.hh"

namespace Episode3 {

class Server;

extern const std::vector<uint16_t> ALL_ASSIST_CARD_IDS;

AssistEffect assist_effect_number_for_card_id(uint16_t card_id);

class AssistServer {
public:
  explicit AssistServer(std::shared_ptr<Server> server);
  std::shared_ptr<Server> server();
  std::shared_ptr<const Server> server() const;

  uint16_t card_id_for_card_ref(uint16_t card_ref) const;
  std::shared_ptr<const CardIndex::CardEntry> definition_for_card_id(uint16_t card_id) const;

  uint32_t compute_num_assist_effects_for_client(uint16_t client_id);
  uint32_t compute_num_assist_effects_for_team(uint32_t team_id);

  bool should_block_assist_effects_for_client(uint16_t client_id) const;
  AssistEffect get_active_assist_by_index(size_t index) const;

  void populate_effects();
  void recompute_effects();

private:
  std::weak_ptr<Server> w_server;

public:
  parray<AssistEffect, 4> assist_effects;
  std::shared_ptr<const CardIndex::CardEntry> assist_card_defs[4];
  uint32_t num_assist_cards_set;
  parray<uint8_t, 4> client_ids_with_assists;
  parray<AssistEffect, 4> active_assist_effects;
  std::shared_ptr<const CardIndex::CardEntry> active_assist_card_defs[4];
  uint32_t num_active_assists;
  std::shared_ptr<HandAndEquipState> hand_and_equip_states[4];
  std::shared_ptr<parray<CardShortStatus, 0x10>> card_short_statuses[4];
  std::shared_ptr<DeckEntry> deck_entries[4];
  std::shared_ptr<parray<ActionChainWithConds, 9>> set_card_action_chains[4];
  std::shared_ptr<parray<ActionMetadata, 9>> set_card_action_metadatas[4];
};

} // namespace Episode3
