#pragma once

#include <stdint.h>

#include "Client.hh"
#include "CommandFormats.hh"
#include "Lobby.hh"
#include "Map.hh"
#include "PSOProtocol.hh"
#include "ServerState.hh"

uint8_t translate_subcommand_number(Version to_version, Version from_version, uint8_t subcommand);

asio::awaitable<void> on_subcommand_multi(std::shared_ptr<Client> c, Channel::Message& msg);

void send_item_notification_if_needed(std::shared_ptr<Client> c, const ItemData& item, bool is_from_rare_table);

G_SpecializableItemDropRequest_6xA2 normalize_drop_request(const void* data, size_t size);

struct DropReconcileResult {
  std::shared_ptr<MapState::ObjectState> obj_st;
  // The ref ene_st is the one the client referenced in the drop request; the target ene_st is the one actually used
  // for drop computation (which may be the result of following an alias from the ref ene_st)
  std::shared_ptr<MapState::EnemyState> ref_ene_st;
  std::shared_ptr<MapState::EnemyState> target_ene_st;
  uint8_t effective_rt_index;
  bool should_drop;
  bool ignore_def;
};

DropReconcileResult reconcile_drop_request_with_map(
    std::shared_ptr<Client> c,
    G_SpecializableItemDropRequest_6xA2& cmd,
    Difficulty difficulty,
    uint8_t event,
    std::shared_ptr<MapState> map,
    bool mark_drop);

class Parsed6x70Data {
public:
  Version from_version;
  bool from_client_customization;
  Version item_version;

  G_6x70_Base_DCNTE base;
  uint32_t unknown_a5_nte = 0;
  uint32_t unknown_a6_nte = 0;
  uint16_t bonus_hp_from_materials = 0;
  uint16_t bonus_tp_from_materials = 0;
  parray<uint8_t, 0x10> unknown_a5_112000;
  StatusEffectState permanent_status_effect;
  StatusEffectState temporary_status_effect;
  StatusEffectState attack_status_effect;
  StatusEffectState defense_status_effect;
  StatusEffectState unused_status_effect;
  Language language = Language::JAPANESE;
  uint32_t player_tag = 0;
  uint32_t guild_card_number = 0;
  uint32_t unknown_a6 = 0;
  uint32_t battle_team_number = 0;
  G_6x70_Sub_Telepipe telepipe;
  uint32_t death_flags = 0;
  PlayerHoldState hold_state;
  uint32_t area = 0;
  uint32_t game_flags = 0;
  bool game_flags_is_v3 = false;
  parray<uint8_t, 0x14> technique_levels_v1 = 0xFF;
  PlayerVisualConfig visual;
  std::string name;
  PlayerStats stats;
  uint32_t num_items = 0;
  parray<PlayerInventoryItem, 0x1E> items;
  uint32_t floor = 0;
  uint64_t xb_user_id = 0;
  uint32_t xb_unknown_a16 = 0;

  Parsed6x70Data(
      const G_SyncPlayerDispAndInventory_DCNTE_6x70& cmd,
      uint32_t guild_card_number,
      Version from_version,
      bool from_client_customization);
  Parsed6x70Data(
      const G_SyncPlayerDispAndInventory_DC112000_6x70& cmd,
      uint32_t guild_card_number,
      Language language,
      Version from_version,
      bool from_client_customization);
  Parsed6x70Data(
      const G_SyncPlayerDispAndInventory_DC_PC_6x70& cmd,
      uint32_t guild_card_number,
      Version from_version,
      bool from_client_customization);
  Parsed6x70Data(
      const G_SyncPlayerDispAndInventory_GC_6x70& cmd,
      uint32_t guild_card_number,
      Version from_version,
      bool from_client_customization);
  Parsed6x70Data(
      const G_SyncPlayerDispAndInventory_XB_6x70& cmd,
      uint32_t guild_card_number,
      Version from_version,
      bool from_client_customization);
  Parsed6x70Data(
      const G_SyncPlayerDispAndInventory_BB_6x70& cmd,
      uint32_t guild_card_number,
      Version from_version,
      bool from_client_customization);

  G_SyncPlayerDispAndInventory_DCNTE_6x70 as_dc_nte(std::shared_ptr<ServerState> s) const;
  G_SyncPlayerDispAndInventory_DC112000_6x70 as_dc_112000(std::shared_ptr<ServerState> s) const;
  G_SyncPlayerDispAndInventory_DC_PC_6x70 as_dc_pc(std::shared_ptr<ServerState> s, Version to_version) const;
  G_SyncPlayerDispAndInventory_GC_6x70 as_gc_gcnte(std::shared_ptr<ServerState> s, Version to_version) const;
  G_SyncPlayerDispAndInventory_XB_6x70 as_xb(std::shared_ptr<ServerState> s) const;
  G_SyncPlayerDispAndInventory_BB_6x70 as_bb(std::shared_ptr<ServerState> s, Language language) const;

  uint64_t default_xb_user_id() const;
  void clear_v1_unused_item_fields();
  void clear_dc_protos_unused_item_fields();

protected:
  Parsed6x70Data(
      const G_6x70_Base_V1& base, uint32_t guild_card_number, Version from_version, bool from_client_customization);
  G_6x70_Base_V1 base_v1(bool is_v3) const;
  static uint32_t convert_game_flags(uint32_t game_flags, bool to_v3);
  uint32_t get_game_flags(bool is_v3) const;
};

bool validate_6xBB(G_SyncCardTradeServerState_Ep3_6xBB& cmd);
