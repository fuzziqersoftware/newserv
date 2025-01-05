#pragma once

#include <stdint.h>

#include "Client.hh"
#include "CommandFormats.hh"
#include "Lobby.hh"
#include "Map.hh"
#include "PSOProtocol.hh"
#include "ServerState.hh"

void on_subcommand_multi(std::shared_ptr<Client> c, uint8_t command, uint8_t flag, std::string& data);

void send_item_notification_if_needed(
    std::shared_ptr<ServerState> s,
    Channel& ch,
    const Client::Config& config,
    const ItemData& item,
    bool is_from_rare_table);

G_SpecializableItemDropRequest_6xA2 normalize_drop_request(const void* data, size_t size);

struct DropReconcileResult {
  std::shared_ptr<MapState::ObjectState> obj_st;
  std::shared_ptr<MapState::EnemyState> ene_st;
  uint8_t effective_rt_index;
  bool should_drop;
  bool ignore_def;
};

DropReconcileResult reconcile_drop_request_with_map(
    phosg::PrefixedLogger& log,
    Channel& client_channel,
    G_SpecializableItemDropRequest_6xA2& cmd,
    Episode episode,
    uint8_t event,
    const Client::Config& config,
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
  G_6x70_StatusEffectState permanent_status_effect;
  G_6x70_StatusEffectState temporary_status_effect;
  G_6x70_StatusEffectState attack_status_effect;
  G_6x70_StatusEffectState defense_status_effect;
  G_6x70_StatusEffectState unused_status_effect;
  uint32_t language = 0;
  uint32_t player_tag = 0;
  uint32_t guild_card_number = 0;
  uint32_t unknown_a6 = 0;
  uint32_t battle_team_number = 0;
  G_6x70_Sub_Telepipe telepipe;
  uint32_t unknown_a8 = 0;
  parray<uint8_t, 0x10> unknown_a9_nte_112000;
  G_6x70_Sub_UnknownA1 unknown_a9_final;
  uint32_t area = 0;
  uint32_t flags2_value = 0;
  bool flags2_is_v3 = false;
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
      uint8_t language,
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
  G_SyncPlayerDispAndInventory_BB_6x70 as_bb(std::shared_ptr<ServerState> s, uint8_t language) const;

  uint64_t default_xb_user_id() const;
  void clear_v1_unused_item_fields();
  void clear_dc_protos_unused_item_fields();

protected:
  Parsed6x70Data(
      const G_6x70_Base_V1& base,
      uint32_t guild_card_number,
      Version from_version,
      bool from_client_customization);
  G_6x70_Base_V1 base_v1(bool is_v3) const;
  uint32_t get_flags2(bool is_v3) const;
};

bool validate_6xBB(G_SyncCardTradeServerState_Ep3_6xBB& cmd);
