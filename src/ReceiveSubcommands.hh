#pragma once

#include <stdint.h>

#include "Client.hh"
#include "CommandFormats.hh"
#include "Lobby.hh"
#include "PSOProtocol.hh"
#include "ServerState.hh"

void on_subcommand_multi(std::shared_ptr<Client> c, uint8_t command, uint8_t flag, std::string& data);
bool subcommand_is_implemented(uint8_t which);

void send_item_notification_if_needed(
    std::shared_ptr<ServerState> s,
    Channel& ch,
    const Client::Config& config,
    const ItemData& item,
    bool is_from_rare_table);

G_SpecializableItemDropRequest_6xA2 normalize_drop_request(const void* data, size_t size);

struct DropReconcileResult {
  uint8_t effective_rt_index;
  bool is_box;
  bool should_drop;
  bool ignore_def;
};

DropReconcileResult reconcile_drop_request_with_map(
    PrefixedLogger& log,
    Channel& client_channel,
    G_SpecializableItemDropRequest_6xA2& cmd,
    Version version,
    Episode episode,
    const Client::Config& config,
    std::shared_ptr<Map> map,
    bool mark_drop);
