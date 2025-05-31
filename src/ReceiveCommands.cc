#include "ReceiveCommands.hh"

#include <inttypes.h>
#include <string.h>

#include <memory>
#include <phosg/Filesystem.hh>
#include <phosg/Hash.hh>
#include <phosg/Network.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>
#include <phosg/Time.hh>

#include "ChatCommands.hh"
#include "Compression.hh"
#include "Episode3/Tournament.hh"
#include "FileContentsCache.hh"
#include "GameServer.hh"
#include "IPStackSimulator.hh"
#include "ItemCreator.hh"
#include "Loggers.hh"
#include "PSOProtocol.hh"
#include "ProxyCommands.hh"
#include "ReceiveSubcommands.hh"
#include "SendCommands.hh"
#include "StaticGameData.hh"
#include "Text.hh"

using namespace std;

const char* BATTLE_TABLE_DISCONNECT_HOOK_NAME = "battle_table_state";
const char* QUEST_BARRIER_DISCONNECT_HOOK_NAME = "quest_barrier";
const char* ADD_NEXT_CLIENT_DISCONNECT_HOOK_NAME = "add_next_game_client";

asio::awaitable<void> on_connect(std::shared_ptr<Client> c) {
  auto s = c->require_server_state();
  if (s->default_switch_assist_enabled) {
    c->set_flag(Client::Flag::SWITCH_ASSIST_ENABLED);
  }

  switch (c->server_behavior) {
    case ServerBehavior::PC_CONSOLE_DETECT: {
      uint16_t pc_port = s->name_to_port_config.at("pc")->port;
      uint16_t console_port = s->name_to_port_config.at("gc-us3")->port;
      send_pc_console_split_reconnect(c, s->connect_address_for_client(c), pc_port, console_port);
      // TODO: There appears to be a bug that occurs rarely when a client
      // connects to this port; sometimes it disconnects before receiving the
      // data it needs. My hypothesis is that there's either a bug in Channel
      // where the data isn't being sent before the RST, or there's a bug in
      // AVE-TCP where it doesn't forward the last data to the app if the RST
      // is received on the same frame as the last PSH. In either case, waiting
      // a short amount of time here should mitigate it.
      co_await async_sleep(std::chrono::seconds(2));
      c->channel->disconnect();
      break;
    }
    case ServerBehavior::GAME_SERVER:
      send_server_init(c, SendServerInitFlag::IS_INITIAL_CONNECTION);
      break;
    case ServerBehavior::PATCH_SERVER_PC:
      c->channel->version = Version::PC_PATCH;
      send_server_init(c, 0);
      break;
    case ServerBehavior::PATCH_SERVER_BB:
      c->channel->version = Version::BB_PATCH;
      send_server_init(c, 0);
      break;
    default:
      c->log.error_f("Unimplemented behavior: {}", static_cast<int64_t>(c->server_behavior));
  }
  co_return;
}

asio::awaitable<void> on_disconnect(shared_ptr<Client> c) {
  // If the client was in a lobby, remove them and notify the other clients
  auto l = c->lobby.lock();
  if (l) {
    auto server = c->server.lock();
    if (server) {
      server->get_state()->remove_client_from_lobby(c);
    }
  }

  // Note: The client's GameData destructor should save their player data
  // shortly after this point
  co_return;
}

static void send_main_menu(shared_ptr<Client> c) {
  auto s = c->require_server_state();

  auto main_menu = make_shared<Menu>(MenuID::MAIN, s->name);
  main_menu->items.emplace_back(
      MainMenuItemID::GO_TO_LOBBY, "Go to lobby",
      [wc = weak_ptr<Client>(c)]() -> string {
        auto c = wc.lock();
        if (!c) {
          return "";
        }
        auto s = c->require_server_state();

        size_t num_players = 0;
        size_t num_games = 0;
        size_t num_compatible_games = 0;
        for (const auto& it : s->id_to_lobby) {
          const auto& l = it.second;
          if (l->is_game()) {
            num_games++;
            if (l->version_is_allowed(c->version()) && (l->is_ep3() == is_ep3(c->version()))) {
              num_compatible_games++;
            }
          }
          for (const auto& c : l->clients) {
            if (c) {
              num_players++;
            }
          }
        }
        return std::format(
            "$C6{}$C7 players online\n$C6{}$C7 games\n$C6{}$C7 compatible games",
            num_players, num_games, num_compatible_games);
      },
      0);
  main_menu->items.emplace_back(MainMenuItemID::INFORMATION, "Information",
      "View server\ninformation", MenuItem::Flag::INVISIBLE_ON_DC_PROTOS | MenuItem::Flag::REQUIRES_MESSAGE_BOXES);

  uint32_t proxy_destinations_menu_item_flags =
      (s->proxy_destinations_dc.empty() ? MenuItem::Flag::INVISIBLE_ON_DC : 0) |
      (s->proxy_destinations_pc.empty() ? MenuItem::Flag::INVISIBLE_ON_PC : 0) |
      (s->proxy_destinations_gc.empty() ? MenuItem::Flag::INVISIBLE_ON_GC : 0) |
      (s->proxy_destinations_xb.empty() ? MenuItem::Flag::INVISIBLE_ON_XB : 0) |
      MenuItem::Flag::INVISIBLE_ON_BB;
  main_menu->items.emplace_back(MainMenuItemID::PROXY_DESTINATIONS, "Proxy server",
      "Connect to another\nserver through the\nproxy", proxy_destinations_menu_item_flags);

  main_menu->items.emplace_back(MainMenuItemID::DOWNLOAD_QUESTS, "Download quests",
      "Download quests", MenuItem::Flag::INVISIBLE_ON_DC_PROTOS | MenuItem::Flag::INVISIBLE_ON_PC_NTE | MenuItem::Flag::INVISIBLE_ON_BB);
  if (!s->function_code_index->patch_menu_empty(c->specific_version)) {
    main_menu->items.emplace_back(MainMenuItemID::PATCH_SWITCHES, "Patches",
        "Change game\nbehaviors", MenuItem::Flag::REQUIRES_SEND_FUNCTION_CALL_RUNS_CODE);
  }
  if (!s->dol_file_index->empty()) {
    main_menu->items.emplace_back(MainMenuItemID::PROGRAMS, "Programs",
        "Run GameCube\nprograms", MenuItem::Flag::GC_ONLY | MenuItem::Flag::REQUIRES_SEND_FUNCTION_CALL_RUNS_CODE | MenuItem::Flag::REQUIRES_SAVE_DISABLED);
  }
  main_menu->items.emplace_back(MainMenuItemID::DISCONNECT, "Disconnect", "Disconnect", 0);
  main_menu->items.emplace_back(MainMenuItemID::CLEAR_LICENSE, "Clear license",
      "Disconnect with an\ninvalid license error\nso you can enter a\ndifferent serial\nnumber, access key,\nor password",
      MenuItem::Flag::INVISIBLE_ON_DC_PROTOS | MenuItem::Flag::INVISIBLE_ON_PC_NTE | MenuItem::Flag::INVISIBLE_ON_XB | MenuItem::Flag::INVISIBLE_ON_BB);

  send_menu(c, main_menu);
}

static void send_proxy_destinations_menu(shared_ptr<Client> c) {
  auto s = c->require_server_state();
  send_menu(c, s->proxy_destinations_menu(c->version()));
}

static shared_ptr<const Menu> proxy_options_menu_for_client(shared_ptr<const Client> c) {
  if (!c->login) {
    throw logic_error("client is not logged in");
  }
  auto s = c->require_server_state();

  auto ret = make_shared<Menu>(MenuID::PROXY_OPTIONS, "Proxy options");
  ret->items.emplace_back(ProxyOptionsMenuItemID::GO_BACK, "Go back", "Return to the\nProxy Server menu", 0);

  auto add_bool_option = [&](uint32_t item_id, bool is_enabled, const char* text, const char* description) -> void {
    string option = is_enabled ? "* " : "- ";
    option += text;
    ret->items.emplace_back(item_id, option, description, 0);
  };
  auto add_flag_option = [&](uint32_t item_id, Client::Flag flag, const char* text, const char* description) -> void {
    add_bool_option(item_id, c->check_flag(flag), text, description);
  };

  if (c->can_use_chat_commands()) {
    add_flag_option(ProxyOptionsMenuItemID::CHAT_COMMANDS, Client::Flag::PROXY_CHAT_COMMANDS_ENABLED,
        "Chat commands", "Enable chat\ncommands");
  }
  add_flag_option(ProxyOptionsMenuItemID::PLAYER_NOTIFICATIONS, Client::Flag::PROXY_PLAYER_NOTIFICATIONS_ENABLED,
      "Player notifs", "Show a message\nwhen other players\njoin or leave");
  static const char* item_drop_notifs_description = "Enable item drop\nnotifications:\n- No: no notifs\n- Rare: rares only\n- Item: all items\nbut not Meseta\n- Every: everything";
  if (!is_ep3(c->version())) {
    switch (c->get_drop_notification_mode()) {
      case Client::ItemDropNotificationMode::NOTHING:
        ret->items.emplace_back(ProxyOptionsMenuItemID::DROP_NOTIFICATIONS, "No drop notifs", item_drop_notifs_description, 0);
        break;
      case Client::ItemDropNotificationMode::RARES_ONLY:
        ret->items.emplace_back(ProxyOptionsMenuItemID::DROP_NOTIFICATIONS, "Rare drop notifs", item_drop_notifs_description, 0);
        break;
      case Client::ItemDropNotificationMode::ALL_ITEMS:
        ret->items.emplace_back(ProxyOptionsMenuItemID::DROP_NOTIFICATIONS, "Item drop notifs", item_drop_notifs_description, 0);
        break;
      case Client::ItemDropNotificationMode::ALL_ITEMS_INCLUDING_MESETA:
        ret->items.emplace_back(ProxyOptionsMenuItemID::DROP_NOTIFICATIONS, "Every drop notif", item_drop_notifs_description, 0);
        break;
    }
  }
  add_bool_option(ProxyOptionsMenuItemID::BLOCK_EVENTS, (c->override_lobby_event != 0xFF),
      "Block events", "Disable seasonal\nevents in the lobby\nand in games");
  add_flag_option(ProxyOptionsMenuItemID::BLOCK_PATCHES, Client::Flag::PROXY_BLOCK_FUNCTION_CALLS,
      "Block patches", "Disable patches sent\nby the remote server");
  if (!is_ep3(c->version())) {
    add_flag_option(ProxyOptionsMenuItemID::SWITCH_ASSIST, Client::Flag::SWITCH_ASSIST_ENABLED,
        "Switch assist", "Automatically unlock\nmulti-player doors\nwhen you step on\nany of the door\'s\nswitches");
  }
  if ((s->cheat_mode_behavior != ServerState::BehaviorSwitch::OFF) ||
      c->login->account->check_flag(Account::Flag::CHEAT_ANYWHERE)) {
    if (!is_ep3(c->version())) {
      add_flag_option(ProxyOptionsMenuItemID::INFINITE_HP, Client::Flag::INFINITE_HP_ENABLED,
          "Infinite HP", "Enable automatic HP\nrestoration when\nyou are hit by an\nenemy or trap\n\nCannot revive you\nfrom one-hit kills");
      add_flag_option(ProxyOptionsMenuItemID::INFINITE_TP, Client::Flag::INFINITE_TP_ENABLED,
          "Infinite TP", "Enable automatic TP\nrestoration when\nyou cast any\ntechnique");
    } else {
      add_flag_option(ProxyOptionsMenuItemID::EP3_INFINITE_MESETA, Client::Flag::PROXY_EP3_INFINITE_MESETA_ENABLED,
          "Inf Meseta", "Fix Meseta value\nat 1,000,000");
      add_flag_option(ProxyOptionsMenuItemID::EP3_INFINITE_TIME, Client::Flag::PROXY_EP3_INFINITE_TIME_ENABLED,
          "Infinite time", "Disable overall and\nper-phase time limits\nin battle");
      add_flag_option(ProxyOptionsMenuItemID::EP3_UNMASK_WHISPERS, Client::Flag::PROXY_EP3_UNMASK_WHISPERS,
          "Show whispers", "Show contents of\nwhisper messages\neven if they are not\nfor you");
    }
  }
  if (s->proxy_allow_save_files) {
    add_flag_option(ProxyOptionsMenuItemID::SAVE_FILES, Client::Flag::PROXY_SAVE_FILES,
        "Save files", "Save local copies of\nfiles from the\nremote server\n(quests, etc.)");
  }
  return ret;
}

static asio::awaitable<void> send_auto_patches_if_needed(shared_ptr<Client> c) {
  auto s = c->require_server_state();

  if (c->login->account->auto_patches_enabled.empty() &&
      ((c->version() != Version::BB_V4) || s->bb_required_patches.empty())) {
    c->set_flag(Client::Flag::HAS_AUTO_PATCHES);
  }

  if (!c->check_flag(Client::Flag::HAS_AUTO_PATCHES) && c->check_flag(Client::Flag::HAS_SEND_FUNCTION_CALL)) {
    c->set_flag(Client::Flag::HAS_AUTO_PATCHES);
    co_await prepare_client_for_patches(c);

    vector<shared_ptr<const CompiledFunctionCode>> functions_to_send;
    if (c->version() == Version::BB_V4) {
      for (const auto& patch_name : s->bb_required_patches) {
        try {
          functions_to_send.emplace_back(s->function_code_index->get_patch(patch_name, c->specific_version));
        } catch (const out_of_range&) {
          string message = std::format(
              "Your client is not compatible with a\nrequired patch on this server.\n\nClient version: {:08X}\nPatch name: {}", c->specific_version, patch_name);
          send_message_box(c, message);
          c->channel->disconnect();
          co_return;
        }
      }
    }
    for (const auto& patch_name : c->login->account->auto_patches_enabled) {
      try {
        functions_to_send.emplace_back(s->function_code_index->get_patch(patch_name, c->specific_version));
      } catch (const out_of_range&) {
        c->log.warning_f("Client has auto patch {} enabled, but it is not available for specific_version {:08X}",
            patch_name, c->specific_version);
      }
    }

    co_await send_function_call_multi(c, functions_to_send);
  }
}

asio::awaitable<void> start_login_server_procedure(shared_ptr<Client> c) {
  auto s = c->require_server_state();

  if (c->lobby.lock()) {
    send_self_leave_notification(c);
    s->remove_client_from_lobby(c);
  }

  if (s->pre_lobby_event && (!is_ep3(c->version()) || s->ep3_menu_song < 0)) {
    send_change_event(c, s->pre_lobby_event);
  }

  send_server_time(c);

  if (c->version() == Version::BB_V4) {
    // This implicitly loads the client's account and player data from disk
    send_complete_player_bb(c);
    c->should_update_play_time = true;
  } else if (!is_pre_v1(c->version())) {
    // send_get_player_info is required to differentiate Ep3 from Ep3 NTE. We
    // send it early in the login procedure because the contents of the card
    // list update and the tournament entry depend on this difference.
    co_await send_get_player_info(c, false);
  }

  if (is_ep3(c->version())) {
    send_ep3_rank_update(c);
    if (!c->check_flag(Client::Flag::HAS_EP3_CARD_DEFS)) {
      send_ep3_card_list_update(c);
      c->set_flag(Client::Flag::HAS_EP3_CARD_DEFS);
    }
    if ((c->version() != Version::GC_EP3_NTE) && !c->check_flag(Client::Flag::HAS_EP3_MEDIA_UPDATES)) {
      for (const auto& banner : s->ep3_lobby_banners) {
        send_ep3_media_update(c, banner.type, banner.which, banner.data);
        c->set_flag(Client::Flag::HAS_EP3_MEDIA_UPDATES);
      }
    }
    s->ep3_tournament_index->link_client(c);
  }

  if (s->welcome_message.empty() ||
      c->check_flag(Client::Flag::NO_D6) ||
      !c->check_flag(Client::Flag::AT_WELCOME_MESSAGE)) {
    c->clear_flag(Client::Flag::AT_WELCOME_MESSAGE);
    send_main_menu(c);
  } else {
    send_message_box(c, s->welcome_message);
  }
  co_return;
}

static asio::awaitable<void> on_login_complete(shared_ptr<Client> c) {
  auto s = c->require_server_state();

  c->convert_account_to_temporary_if_nte();

  if (!is_v4(c->version())) {
    send_set_guild_card_number(c);
  }

  if (c->check_flag(Client::Flag::CAN_RECEIVE_ENABLE_B2_QUEST) &&
      (!c->check_flag(Client::Flag::HAS_SEND_FUNCTION_CALL) ||
          !c->check_flag(Client::Flag::SEND_FUNCTION_CALL_ACTUALLY_RUNS_CODE))) {
    shared_ptr<const Quest> q;
    try {
      int64_t quest_num = s->enable_send_function_call_quest_numbers.at(c->specific_version);
      q = s->default_quest_index->get(quest_num);
    } catch (const out_of_range&) {
    }
    if (!q) {
      c->log.info_f("There is no quest to enable server function calls for specific version {:08X}", c->specific_version);
    } else if (q) {
      auto vq = q->version(is_ep3(c->version()) ? Version::GC_V3 : c->version(), 1);
      if (vq) {
        c->set_flag(Client::Flag::HAS_SEND_FUNCTION_CALL);
        c->set_flag(Client::Flag::SEND_FUNCTION_CALL_ACTUALLY_RUNS_CODE);
        c->set_flag(Client::Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH);
        c->set_flag(Client::Flag::AWAITING_ENABLE_B2_QUEST);

        // PCv2 will crash if it receives an online quest file while it's
        // not in a game, so we have to put it into a fake game first
        if (c->version() == Version::PC_V2) {
          S_JoinGame_PC_64 cmd;
          auto& lobby_data = cmd.lobby_data[0];
          lobby_data.player_tag = 0x00010000;
          lobby_data.guild_card_number = c->login->account->account_id;
          send_command_t(c, 0x64, 0x01, cmd);
        } else {
          c->log.info_f("Sending {} version of quest \"{}\"", char_for_language_code(vq->language), vq->name);
          string bin_filename = vq->bin_filename();
          string dat_filename = vq->dat_filename();
          string xb_filename = vq->xb_filename();
          send_open_quest_file(c, bin_filename, bin_filename, xb_filename, vq->quest_number, QuestFileType::ONLINE, vq->bin_contents);
          send_open_quest_file(c, dat_filename, dat_filename, xb_filename, vq->quest_number, QuestFileType::ONLINE, vq->dat_contents);

          if (!is_v1_or_v2(c->version())) {
            send_command(c, 0xAC, 0x00);
          }
        }
      }
    }
  }

  // TODO: It'd be nice to use response_futures to wait for the eventual 84
  // command instead of using flags and calling start_login_server_procedure in
  // on_84 too.
  if (!c->check_flag(Client::Flag::AWAITING_ENABLE_B2_QUEST)) {
    co_await start_login_server_procedure(c);
  }
  co_return;
}

static asio::awaitable<void> enable_save_if_needed(shared_ptr<Client> c) {
  if (c->enable_save_promise) {
    throw logic_error("Enable save promise is already present");
  }
  if (!c->channel->connected()) {
    throw runtime_error("Client has already disconnected");
  }

  if (c->check_flag(Client::Flag::SHOULD_SEND_ENABLE_SAVE)) {
    c->set_flag(Client::Flag::SAVE_ENABLED);
    c->clear_flag(Client::Flag::SHOULD_SEND_ENABLE_SAVE);
    // DC NTE and 11/2000 crash if they receive 97, so we do nothing on those
    // versions
    if (!is_pre_v1(c->version())) {
      send_command(c, 0x97, 0x01);
      auto promise = make_shared<AsyncPromise<void>>();
      c->enable_save_promise = promise;
      co_await promise->get();
    }
  }
}

asio::awaitable<void> start_proxy_session(shared_ptr<Client> c, const string& host, uint16_t port, bool use_persistent_config) {
  if (c->proxy_session) {
    throw logic_error("cannot start a proxy session while one is already active");
  }

  auto s = c->require_server_state();

  // We don't send any preamble commands for BB clients since proxy sessions
  // are created at connection time instead of during the main session
  if (!is_patch(c->version()) && !is_v4(c->version())) {
    if (!c->login) {
      throw logic_error("Cannot start proxy session with preamble on logged-out client");
    }
    co_await send_auto_patches_if_needed(c);
    co_await enable_save_if_needed(c);
  }

  if (!c->can_use_chat_commands()) {
    c->clear_flag(Client::Flag::PROXY_CHAT_COMMANDS_ENABLED);
  }
  if (!s->proxy_allow_save_files) {
    c->clear_flag(Client::Flag::PROXY_SAVE_FILES);
  }
  if (c->version() == Version::GC_EP3) {
    send_ep3_media_update(c, 4, 0, "");
    c->clear_flag(Client::Flag::HAS_EP3_MEDIA_UPDATES);
  }

  string netloc_str = std::format("{}:{}", host, port);
  c->log.info_f("Connecting to {}", netloc_str);
  unique_ptr<asio::ip::tcp::socket> sock;
  try {
    sock = make_unique<asio::ip::tcp::socket>(co_await async_connect_tcp(host, port));
  } catch (const exception& e) {
    auto msg = std::format("Failed to connect to\n{}\n\n{}", netloc_str, e.what());
    if (is_patch(c->version()) || is_v4(c->version())) {
      send_message_box(c, msg);
    } else {
      send_ship_info(c, msg);
    }
  }

  if (!sock) {
    if (is_patch(c->version()) || is_v4(c->version())) {
      c->channel->disconnect();
    } else {
      send_proxy_destinations_menu(c);
    }
  } else {
    // Get persistent config if available
    ProxySession::PersistentConfig* pc = nullptr;
    if (use_persistent_config) {
      try {
        pc = &s->proxy_persistent_configs.at(c->login->account->account_id);
        if (pc->expire_timer) {
          pc->expire_timer->cancel();
          pc->expire_timer.reset();
        }
        c->log.info_f("Found persistent config for proxy session");
      } catch (const out_of_range&) {
        c->log.info_f("No persistent config exists for proxy session");
      }
    }

    shared_ptr<Channel> channel = SocketChannel::create(
        s->io_context,
        std::move(sock),
        c->channel->version,
        c->channel->language,
        std::format("C-{} proxy remote server at {}", c->id, netloc_str),
        phosg::TerminalFormat::FG_YELLOW,
        phosg::TerminalFormat::FG_RED);
    c->proxy_session = make_shared<ProxySession>(channel, pc);

    c->log.info_f("Server channel connected");
    asio::co_spawn(*s->io_context, handle_proxy_server_commands(c, c->proxy_session, channel), asio::detached);
  }
}

asio::awaitable<void> end_proxy_session(shared_ptr<Client> c, const std::string& error_message) {
  if (!c->proxy_session) {
    co_return;
  }

  // Set expiration timer for persistent config
  auto s = c->require_server_state();
  try {
    auto& pc = s->proxy_persistent_configs.at(c->login->account->account_id);
    pc.expire_timer = make_unique<asio::steady_timer>(*s->io_context);
    pc.expire_timer->expires_after(std::chrono::seconds(30));
    pc.expire_timer->async_wait([ws = weak_ptr<ServerState>(s), account_id = c->login->account->account_id](std::error_code ec) -> void {
      if (!ec) {
        auto s = ws.lock();
        if (s) {
          server_log.info_f("Deleting proxy persistent config for account {:08X}", account_id);
          s->proxy_persistent_configs.erase(account_id);
        }
      }
    });
  } catch (const out_of_range&) {
  }

  // Delete all the other players
  for (size_t x = 0; x < c->proxy_session->lobby_players.size(); x++) {
    if (c->proxy_session->lobby_players[x].guild_card_number == 0) {
      continue;
    }
    uint8_t leaving_id = x;
    uint8_t leader_id = c->lobby_client_id;
    S_LeaveLobby_66_69_Ep3_E9 cmd = {leaving_id, leader_id, 1, 0};
    c->channel->send(c->proxy_session->is_in_game ? 0x66 : 0x69, leaving_id, &cmd, sizeof(cmd));
  }

  bool is_in_game = c->proxy_session->is_in_game;
  c->proxy_session->server_channel->disconnect();
  c->proxy_session.reset();

  if (is_v4(c->version())) {
    c->channel->disconnect();
    co_return;
  }

  if (is_in_game) {
    string msg = std::format("You cannot return\nto $C6{}$C7\nwhile in a game.\n\n{}",
        s->name, error_message);
    send_ship_info(c, msg);
    c->channel->disconnect();
  } else {
    string msg = std::format("You\'ve returned to\n$C6{}$C7\n\n{}", s->name, error_message);
    send_ship_info(c, msg);
    co_await start_login_server_procedure(c);
  }
}

////////////////////////////////////////////////////////////////////////////////

static asio::awaitable<void> on_02_U(shared_ptr<Client> c, Channel::Message& msg) {
  check_size_v(msg.data.size(), 0);
  c->channel->send(0x04, 0x00); // This requests the user's login information
  co_return;
}

static asio::awaitable<void> on_04_U(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& cmd = msg.check_size_t<C_Login_Patch_04>();

  c->username = cmd.username.decode();
  c->password = cmd.password.decode();
  c->email_address = cmd.email_address.decode();

  // There are 3 cases here:
  // - No login information at all: just proceed without checking credentials
  // - Username: check that account exists if allow_unregistered_users is off
  // - Username and password: call verify_bb
  uint8_t result_code = 0x00;
  auto s = c->require_server_state();
  if (!c->username.empty() && !c->password.empty()) {
    try {
      s->account_index->from_bb_credentials(c->username, &c->password, false);
    } catch (const AccountIndex::incorrect_password& e) {
      result_code = 0x03;
    } catch (const AccountIndex::missing_account& e) {
      if (!s->allow_unregistered_users) {
        result_code = 0x08;
      }
    }
  } else if (!c->username.empty() && !s->allow_unregistered_users) {
    try {
      s->account_index->from_bb_credentials(c->username, nullptr, false);
    } catch (const AccountIndex::missing_account& e) {
      result_code = 0x08;
    }
  }
  if (result_code) {
    c->channel->send(0x15, result_code);
    c->channel->disconnect();
    co_return;
  }

  // Switch to proxy session if there's a destination configured
  if (is_patch(c->version()) && s->proxy_destination_patch.has_value()) {
    const auto& [host, port] = *s->proxy_destination_patch;
    co_await start_proxy_session(c, host, port, false);

  } else {
    // No proxy destination; continue with normal patch logic
    bool is_bb = (c->version() == Version::BB_PATCH);
    const string& message = is_bb ? s->bb_patch_server_message : s->pc_patch_server_message;
    if (!message.empty()) {
      send_message_box(c, message);
    }

    auto index = is_bb ? s->bb_patch_file_index : s->pc_patch_file_index;
    if (index.get()) {
      c->channel->send(0x0B, 0x00); // Start patch session; go to root directory

      vector<string> path_directories;
      for (const auto& file : index->all_files()) {
        send_patch_change_to_directory(c, path_directories, file->path_directories);

        S_FileChecksumRequest_Patch_0C req = {c->patch_file_checksum_requests.size(), {file->name, 1}};
        c->channel->send(0x0C, 0x00, req);
        c->patch_file_checksum_requests.emplace_back(file);
      }
      send_patch_change_to_directory(c, path_directories, {});

      c->channel->send(0x0D, 0x00); // End of checksum requests

    } else {
      // No patch index present: just do something that will satisfy the client
      // without actually checking or downloading any files
      send_patch_enter_directory(c, ".");
      send_patch_enter_directory(c, "data");
      send_patch_enter_directory(c, "scene");
      c->channel->send(0x0A, 0x00);
      c->channel->send(0x0A, 0x00);
      c->channel->send(0x0A, 0x00);
      c->channel->send(0x12, 0x00);
    }
  }
}

asio::awaitable<void> on_0F_U(shared_ptr<Client> c, Channel::Message& msg) {
  auto& cmd = msg.check_size_t<C_FileInformation_Patch_0F>();
  auto& req = c->patch_file_checksum_requests.at(cmd.request_id);
  req.crc32 = cmd.checksum;
  req.size = cmd.size;
  req.response_received = true;
  co_return;
}

asio::awaitable<void> on_10_U(shared_ptr<Client> c, Channel::Message&) {
  S_StartFileDownloads_Patch_11 start_cmd = {0, 0};
  for (const auto& req : c->patch_file_checksum_requests) {
    if (!req.response_received) {
      throw runtime_error("client did not respond to checksum request");
    }
    if (req.needs_update()) {
      c->log.info_f("File {} needs update (CRC: {:08X}/{:08X}, size: {}/{})",
          req.file->name, req.file->crc32, req.crc32, req.file->size, req.size);
      start_cmd.total_bytes += req.file->size;
      start_cmd.num_files++;
    } else {
      c->log.info_f("File {} is up to date", req.file->name);
    }
  }

  if (start_cmd.num_files) {
    c->channel->send(0x11, 0x00, start_cmd);
    vector<string> path_directories;
    for (const auto& req : c->patch_file_checksum_requests) {
      if (req.needs_update()) {
        send_patch_change_to_directory(c, path_directories, req.file->path_directories);

        S_OpenFile_Patch_06 open_cmd = {0, req.file->size, {req.file->name, 1}};
        c->channel->send(0x06, 0x00, open_cmd);

        for (size_t x = 0; x < req.file->chunk_crcs.size(); x++) {
          auto data = req.file->load_data();
          size_t chunk_size = min<uint32_t>(req.file->size - (x * 0x4000), 0x4000);

          vector<pair<const void*, size_t>> blocks;
          S_WriteFileHeader_Patch_07 cmd_header = {x, req.file->chunk_crcs[x], chunk_size};
          blocks.emplace_back(&cmd_header, sizeof(cmd_header));
          blocks.emplace_back(data->data() + (x * 0x4000), chunk_size);
          c->channel->send(0x07, 0x00, blocks);
        }

        S_CloseCurrentFile_Patch_08 close_cmd = {0};
        c->channel->send(0x08, 0x00, close_cmd);
      }
    }
    send_patch_change_to_directory(c, path_directories, {});
  }

  c->channel->send(0x12, 0x00);
  co_return;
}

////////////////////////////////////////////////////////////////////////////////

static asio::awaitable<void> on_1D(shared_ptr<Client> c, Channel::Message&) {
  if (c->ping_start_time) {
    uint64_t ping_usecs = phosg::now() - c->ping_start_time;
    c->ping_start_time = 0;
    double ping_ms = static_cast<double>(ping_usecs) / 1000.0;
    send_text_message_fmt(c, "To server: {:g}ms", ping_ms);
  }

  // See the comment on the 6x6D command in CommandFormats.hh to understand why
  // we do this.
  if (c->game_join_command_queue) {
    c->log.info_f("Sending {} queued command(s)", c->game_join_command_queue->size());
    while (!c->game_join_command_queue->empty()) {
      const auto& cmd = c->game_join_command_queue->front();
      send_command(c, cmd.command, cmd.flag, cmd.data);
      c->game_join_command_queue->pop_front();
    }
    c->game_join_command_queue.reset();
  }

  if (!is_ep3(c->version())) {
    if (c->check_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_ITEM_STATE)) {
      c->clear_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_ITEM_STATE);
      send_game_item_state(c); // 6x6D
    }
    if (c->check_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_OBJECT_STATE)) {
      c->clear_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_OBJECT_STATE);
      send_game_object_state(c); // 6x6C
    }
    if (c->check_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_ENEMY_AND_SET_STATE)) {
      c->clear_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_ENEMY_AND_SET_STATE);
      send_game_enemy_state(c); // 6x6B
      send_game_set_state(c); // 6x6E
    }
  }
  if (c->check_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_FLAG_STATE)) {
    c->clear_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_FLAG_STATE);
    send_game_flag_state(c); // 6x6F
  }
  if (c->check_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_PLAYER_STATES)) {
    c->clear_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_PLAYER_STATES);
    auto l = c->require_lobby();
    if (l->is_game()) {
      for (auto lc : l->clients) {
        // If we haven't received a 6x70 from this client, maybe they're VERY
        // far behind and wil lstll send it, or maybe they'll time out and be
        // disconnected soon - either way, we shouldn't fail if it's missing.
        if (lc && (lc != c) && lc->last_reported_6x70) {
          send_game_player_state(c, lc, true); // 6x70
        }
      }
    }
  }
  co_return;
}

static asio::awaitable<void> on_05_XB(shared_ptr<Client> c, Channel::Message&) {
  // The Xbox Live service doesn't close the TCP connection when the player
  // chooses Quit Game, so we manually disconnect the client when they send this
  // command instead. We could let the idle timeout take care of it, but this is
  // cleaner overall.
  c->channel->disconnect();
  co_return;
}

static void set_console_client_flags(shared_ptr<Client> c, uint32_t sub_version) {
  if (c->channel->crypt_in->type() == PSOEncryption::Type::V2) {
    if (sub_version <= 0x24) {
      c->channel->version = Version::DC_V1;
      c->log.info_f("Game version changed to DC_V1");
      if (specific_version_is_indeterminate(c->specific_version) ||
          c->specific_version == SPECIFIC_VERSION_DC_11_2000_PROTOTYPE) {
        c->specific_version = SPECIFIC_VERSION_DC_V1_INDETERMINATE;
      }
    } else if (sub_version <= 0x28) {
      c->channel->version = Version::DC_V2;
      c->log.info_f("Game version changed to DC_V2");
      if (specific_version_is_indeterminate(c->specific_version)) {
        c->specific_version = SPECIFIC_VERSION_DC_V2_INDETERMINATE;
      }
    } else if (is_v3(c->version())) {
      c->channel->version = Version::GC_NTE;
      c->log.info_f("Game version changed to GC_NTE");
      c->specific_version = SPECIFIC_VERSION_GC_NTE;
    }
  } else {
    if (sub_version >= 0x40 && !is_ep3(c->version())) {
      c->channel->version = Version::GC_EP3;
      c->log.info_f("Game version changed to GC_EP3");
      if (specific_version_is_indeterminate(c->specific_version)) {
        c->specific_version = SPECIFIC_VERSION_GC_EP3_INDETERMINATE;
      }
    }
  }
  c->set_flags_for_version(c->version(), sub_version);
  c->sub_version = sub_version;
  if (specific_version_is_indeterminate(c->specific_version)) {
    c->specific_version = default_specific_version_for_version(c->version(), sub_version);
  }
}

static asio::awaitable<void> on_DB_V3(shared_ptr<Client> c, Channel::Message& msg) {
  if (c->channel->crypt_in->type() == PSOEncryption::Type::V2) {
    throw runtime_error("GC trial edition client sent V3 verify account command");
  }

  const auto& cmd = check_size_t<C_VerifyAccount_V3_DB>(msg.data);
  c->v1_serial_number = cmd.v1_serial_number.decode();
  c->v1_access_key = cmd.v1_access_key.decode();
  c->serial_number = cmd.serial_number.decode();
  c->access_key = cmd.access_key.decode();
  c->hardware_id = cmd.hardware_id;
  set_console_client_flags(c, cmd.sub_version);
  c->serial_number2 = cmd.serial_number2.decode();
  c->access_key2 = cmd.access_key2.decode();
  c->password = cmd.password.decode();

  uint32_t serial_number = stoul(c->serial_number, nullptr, 16);
  try {
    auto s = c->require_server_state();
    c->set_login(s->account_index->from_gc_credentials(
        serial_number, c->access_key, &c->password, "", s->allow_unregistered_users));
    send_command(c, 0x9A, 0x02);

  } catch (const AccountIndex::no_username& e) {
    c->log.info_f("Login failed (no username)");
    send_command(c, 0x9A, 0x03);
  } catch (const AccountIndex::incorrect_access_key& e) {
    c->log.info_f("Login failed (incorrect access key)");
    send_command(c, 0x9A, 0x03);
  } catch (const AccountIndex::incorrect_password& e) {
    c->log.info_f("Login failed (incorrect password)");
    send_command(c, 0x9A, 0x01);
  } catch (const AccountIndex::missing_account& e) {
    c->log.info_f("Login failed (missing account)");
    send_command(c, 0x9A, 0x04);
  } catch (const AccountIndex::account_banned& e) {
    c->log.info_f("Login failed (account banned)");
    send_command(c, 0x9A, 0x0F);
  }

  if (!c->login) {
    c->channel->disconnect();
  }
  co_return;
}

static asio::awaitable<void> on_88_DCNTE(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& cmd = check_size_t<C_Login_DCNTE_88>(msg.data);
  c->serial_number = cmd.serial_number.decode();
  c->access_key = cmd.access_key.decode();
  c->channel->version = Version::DC_NTE;
  c->specific_version = SPECIFIC_VERSION_DC_NTE;
  c->set_flags_for_version(c->version(), -1);
  c->log.info_f("Game version changed to DC_NTE");

  try {
    auto s = c->require_server_state();
    c->set_login(s->account_index->from_dc_nte_credentials(c->serial_number, c->access_key, s->allow_unregistered_users));
    send_command(c, 0x88, 0x00);

  } catch (const AccountIndex::no_username& e) {
    c->log.info_f("Login failed (no username)");
    send_message_box(c, "Incorrect serial number");
  } catch (const AccountIndex::incorrect_access_key& e) {
    c->log.info_f("Login failed (incorrect access key)");
    send_message_box(c, "Incorrect access key");
  } catch (const AccountIndex::missing_account& e) {
    c->log.info_f("Login failed (missing account)");
    send_message_box(c, "Incorrect serial number");
  } catch (const AccountIndex::account_banned& e) {
    c->log.info_f("Login failed (account banned)");
    send_message_box(c, "Account is banned");
  }

  if (!c->login) {
    c->channel->disconnect();
  }
  co_return;
}

static asio::awaitable<void> on_8B_DCNTE(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& cmd = check_size_t<C_Login_DCNTE_8B>(msg.data, sizeof(C_LoginExtended_DCNTE_8B));
  c->channel->version = Version::DC_NTE;
  c->hardware_id = cmd.hardware_id;
  c->sub_version = cmd.sub_version;
  c->channel->language = cmd.language;
  c->serial_number = cmd.serial_number.decode();
  c->access_key = cmd.access_key.decode();
  c->username = cmd.username.decode();
  c->password = cmd.password.decode();
  c->login_character_name = cmd.login_character_name.decode();
  c->set_flags_for_version(c->version(), c->sub_version);
  c->specific_version = SPECIFIC_VERSION_DC_NTE;
  c->log.info_f("Game version changed to DC_NTE");

  try {
    auto s = c->require_server_state();
    c->set_login(s->account_index->from_dc_nte_credentials(c->serial_number, c->access_key, s->allow_unregistered_users));
  } catch (const AccountIndex::no_username& e) {
    c->log.info_f("Login failed (no username)");
    send_message_box(c, "Incorrect serial number");
  } catch (const AccountIndex::incorrect_access_key& e) {
    c->log.info_f("Login failed (incorrect access key)");
    send_message_box(c, "Incorrect access key");
  } catch (const AccountIndex::missing_account& e) {
    c->log.info_f("Login failed (missing account)");
    send_message_box(c, "Incorrect serial number");
  } catch (const AccountIndex::account_banned& e) {
    c->log.info_f("Login failed (account banned)");
    send_message_box(c, "Account is banned");
  }

  if (!c->login) {
    c->channel->disconnect();
  } else {
    if (cmd.is_extended) {
      const auto& ext_cmd = check_size_t<C_LoginExtended_DCNTE_8B>(msg.data);
      if (ext_cmd.extension.lobby_refs[0].menu_id == MenuID::LOBBY) {
        c->preferred_lobby_id = ext_cmd.extension.lobby_refs[0].item_id;
      }
    }
    co_await on_login_complete(c);
  }
}

static asio::awaitable<void> on_90_DC(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& cmd = check_size_t<C_LoginV1_DC_PC_V3_90>(msg.data, 0xFFFF);
  c->serial_number = cmd.serial_number.decode();
  c->access_key = cmd.access_key.decode();
  c->channel->version = Version::DC_V1;
  c->set_flags_for_version(c->version(), -1);
  if (specific_version_is_indeterminate(c->specific_version) ||
      (c->specific_version == SPECIFIC_VERSION_DC_11_2000_PROTOTYPE)) {
    c->specific_version = SPECIFIC_VERSION_DC_V1_INDETERMINATE;
  }
  c->log.info_f("Game version changed to DC_V1");

  uint32_t serial_number = 0;
  try {
    auto s = c->require_server_state();
    if (c->serial_number.size() > 8 || c->access_key.size() > 8) {
      c->set_login(s->account_index->from_dc_nte_credentials(c->serial_number, c->access_key, s->allow_unregistered_users));
    } else {
      serial_number = stoull(c->serial_number, nullptr, 16);
      c->set_login(s->account_index->from_dc_credentials(serial_number, c->access_key, "", s->allow_unregistered_users));
    }
    if (c->log.should_log(phosg::LogLevel::L_INFO)) {
      string login_str = c->login->str();
      c->log.info_f("Received login: {}", login_str);
    }
    send_command(c, 0x90, 0x01);

  } catch (const AccountIndex::no_username& e) {
    c->log.info_f("Login failed (no username)");
    send_command(c, 0x90, 0x03);
  } catch (const AccountIndex::incorrect_access_key& e) {
    c->log.info_f("Login failed (incorrect access key)");
    send_command(c, 0x90, 0x03);
  } catch (const AccountIndex::missing_account& e) {
    c->log.info_f("Login failed (no account)");
    send_command(c, 0x90, 0x03);
  } catch (const AccountIndex::account_banned& e) {
    c->log.info_f("Login failed (account banned)");
    send_command(c, 0x90, 0x0F);
  }
  if (!c->login) {
    c->channel->disconnect();
  }
  co_return;
}

static asio::awaitable<void> on_92_DC(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& cmd = check_size_t<C_RegisterV1_DC_92>(msg.data);
  c->hardware_id = cmd.hardware_id;
  c->channel->language = cmd.language;
  c->serial_number2 = cmd.serial_number2.decode();
  c->access_key2 = cmd.access_key2.decode();
  c->email_address = cmd.email_address.decode();
  // It appears that in response to 90 01, 11/2000 sends 93 rather than 92, so
  // we use the presence of a 92 command to determine that the client is
  // actually DCv1 and not the prototype.
  c->set_flag(Client::Flag::CHECKED_FOR_DC_V1_PROTOTYPE);
  c->channel->version = Version::DC_V1;
  if (specific_version_is_indeterminate(c->specific_version) ||
      (c->specific_version == SPECIFIC_VERSION_DC_11_2000_PROTOTYPE)) {
    c->specific_version = SPECIFIC_VERSION_DC_V1_INDETERMINATE;
  }
  set_console_client_flags(c, cmd.sub_version);
  c->log.info_f("Game version changed to DC_V1");
  send_command(c, 0x92, 0x01);
  co_return;
}

static asio::awaitable<void> on_93_DC(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& cmd = check_size_t<C_LoginV1_DC_93>(msg.data, sizeof(C_LoginExtendedV1_DC_93));
  auto s = c->require_server_state();
  c->hardware_id = cmd.hardware_id;
  c->channel->language = cmd.language;
  c->serial_number = cmd.serial_number.decode();
  c->access_key = cmd.access_key.decode();
  c->serial_number2 = cmd.serial_number2.decode();
  c->access_key2 = cmd.access_key2.decode();
  c->login_character_name = cmd.login_character_name.decode();
  if (!c->check_flag(Client::Flag::CHECKED_FOR_DC_V1_PROTOTYPE)) {
    set_console_client_flags(c, cmd.sub_version);
  }

  uint32_t serial_number = 0;
  try {
    if (c->serial_number.size() > 8 || c->access_key.size() > 8) {
      c->set_login(s->account_index->from_dc_nte_credentials(c->serial_number, c->access_key, s->allow_unregistered_users));
    } else {
      serial_number = stoull(c->serial_number, nullptr, 16);
      c->set_login(s->account_index->from_dc_credentials(
          serial_number, c->access_key, c->login_character_name, s->allow_unregistered_users));
    }
    if (c->log.should_log(phosg::LogLevel::L_INFO)) {
      string login_str = c->login->str();
      c->log.info_f("Login: {}", login_str);
    }

  } catch (const AccountIndex::no_username& e) {
    c->log.info_f("Login failed (no username)");
    send_message_box(c, "Incorrect serial number");
  } catch (const AccountIndex::incorrect_access_key& e) {
    c->log.info_f("Login failed (incorrect access key)");
    send_message_box(c, "Incorrect access key");
  } catch (const AccountIndex::missing_account& e) {
    c->log.info_f("Login failed (no account)");
    send_message_box(c, "Incorrect serial number");
  } catch (const AccountIndex::account_banned& e) {
    c->log.info_f("Login failed (account banned)");
    send_message_box(c, "Account is banned");
  }
  if (!c->login) {
    c->channel->disconnect();
    co_return;
  }

  if (cmd.is_extended) {
    const auto& ext_cmd = check_size_t<C_LoginExtendedV1_DC_93>(msg.data);
    if (ext_cmd.extension.lobby_refs[0].menu_id == MenuID::LOBBY) {
      c->preferred_lobby_id = ext_cmd.extension.lobby_refs[0].item_id;
    }
  }

  // The first time we receive a 93 from a DC client, we set this flag and send
  // a 92. The IS_DC_V1_PROTOTYPE flag will be removed if the client sends a 92
  // command (which it seems the prototype never does). This is why we always
  // respond with 90 01 here - that's the only case where actual DCv1 sends a
  // 92 command. The IS_DC_V1_PROTOTYPE flag will be removed if the client does
  // indeed send a 92.
  if (!c->check_flag(Client::Flag::CHECKED_FOR_DC_V1_PROTOTYPE)) {
    send_command(c, 0x90, 0x01);
    c->set_flag(Client::Flag::CHECKED_FOR_DC_V1_PROTOTYPE);
    c->channel->version = Version::DC_11_2000;
    if (specific_version_is_indeterminate(c->specific_version)) {
      c->specific_version = SPECIFIC_VERSION_DC_11_2000_PROTOTYPE;
    }
    c->log.info_f("Game version changed to DC_11_2000 (will be changed to V1 if 92 is received)");
  } else {
    co_await on_login_complete(c);
  }
}

static asio::awaitable<void> on_9A(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& cmd = check_size_t<C_Login_DC_PC_V3_9A>(msg.data);
  c->v1_serial_number = cmd.v1_serial_number.decode();
  c->v1_access_key = cmd.v1_access_key.decode();
  c->serial_number = cmd.serial_number.decode();
  c->access_key = cmd.access_key.decode();
  c->serial_number2 = cmd.serial_number2.decode();
  c->access_key2 = cmd.access_key2.decode();
  c->email_address = cmd.email_address.decode();
  set_console_client_flags(c, cmd.sub_version);

  try {
    auto s = c->require_server_state();
    switch (c->version()) {
      case Version::DC_V2: {
        uint32_t serial_number = stoul(c->serial_number, nullptr, 16);
        c->set_login(s->account_index->from_dc_credentials(serial_number, c->access_key, "", s->allow_unregistered_users));
        if (c->log.should_log(phosg::LogLevel::L_INFO)) {
          string login_str = c->login->str();
          c->log.info_f("Login: {}", login_str);
        }
        break;
      }
      case Version::PC_NTE:
      case Version::PC_V2: {
        if ((c->sub_version == 0x29) &&
            c->v1_serial_number.empty() &&
            c->v1_access_key.empty() &&
            c->serial_number.empty() &&
            c->access_key.empty() &&
            c->serial_number2.empty() &&
            c->access_key2.empty() &&
            c->email_address.empty()) {
          c->channel->version = Version::PC_NTE;
          c->log.info_f("Changed client version to PC_NTE");
          c->set_login(s->account_index->from_pc_nte_credentials(
              cmd.guild_card_number, s->allow_unregistered_users && s->allow_pc_nte));
        } else {
          uint32_t serial_number = stoul(cmd.serial_number.decode(), nullptr, 16);
          c->set_login(s->account_index->from_pc_credentials(serial_number, c->access_key, "", s->allow_unregistered_users));
        }
        break;
      }
      case Version::GC_NTE:
      case Version::GC_V3:
      case Version::GC_EP3_NTE:
      case Version::GC_EP3: {
        uint32_t serial_number = stoul(c->serial_number, nullptr, 16);
        // On V3, the client should have sent a DB command containing the
        // password already, which should have created an account if needed. So
        // if no account exists at this point, disconnect the client even if
        // unregistered users are allowed.
        c->set_login(s->account_index->from_gc_credentials(serial_number, c->access_key, nullptr, "", false));
        break;
      }
      default:
        throw runtime_error("unsupported versioned command");
    }
    send_command(c, 0x9A, 0x02);

  } catch (const AccountIndex::no_username& e) {
    c->log.info_f("Login failed (no username)");
    send_command(c, 0x9A, 0x03);
  } catch (const AccountIndex::incorrect_access_key& e) {
    c->log.info_f("Login failed (incorrect access key)");
    send_command(c, 0x9A, 0x03);
  } catch (const AccountIndex::incorrect_password& e) {
    c->log.info_f("Login failed (incorrect password)");
    send_command(c, 0x9A, 0x01);
  } catch (const AccountIndex::missing_account& e) {
    c->log.info_f("Login failed (missing account)");
    send_command(c, 0x9A, 0x03);
  } catch (const AccountIndex::account_banned& e) {
    c->log.info_f("Login failed (account banned)");
    send_command(c, 0x9A, 0x0F);
  }

  if (!c->login) {
    c->channel->disconnect();
  }
  co_return;
}

static asio::awaitable<void> on_9C(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& cmd = check_size_t<C_Register_DC_PC_V3_9C>(msg.data);
  auto s = c->require_server_state();

  c->channel->language = cmd.language;
  c->hardware_id = cmd.hardware_id;
  set_console_client_flags(c, cmd.sub_version);

  uint32_t serial_number = stoul(cmd.serial_number.decode(), nullptr, 16);
  try {
    switch (c->version()) {
      case Version::DC_V2:
        c->set_login(s->account_index->from_dc_credentials(serial_number, cmd.access_key.decode(), "", false));
        break;
      case Version::PC_V2:
        c->set_login(s->account_index->from_pc_credentials(serial_number, cmd.access_key.decode(), "", false));
        break;
      case Version::GC_NTE:
      case Version::GC_V3:
      case Version::GC_EP3_NTE:
      case Version::GC_EP3: {
        string password = cmd.password.decode();
        c->set_login(s->account_index->from_gc_credentials(serial_number, cmd.access_key.decode(), &password, "", false));
        break;
      }
      default:
        // TODO: PC_NTE can probably send 9C, but due to the way we've
        // implemented PC_NTE's login sequence, it never should send 9C.
        throw logic_error("unsupported versioned command");
    }
    send_command(c, 0x9C, 0x01);

  } catch (const AccountIndex::no_username& e) {
    c->log.info_f("Login failed (no username)");
    send_message_box(c, "Incorrect serial number");
  } catch (const AccountIndex::incorrect_password& e) {
    c->log.info_f("Login failed (incorrect password)");
    send_command(c, 0x9C, 0x00);
  } catch (const AccountIndex::missing_account& e) {
    c->log.info_f("Login failed (missing account)");
    send_command(c, 0x9C, 0x00);
  } catch (const AccountIndex::account_banned& e) {
    c->log.info_f("Login failed (account banned)");
    send_message_box(c, "Account is banned");
  }
  if (!c->login) {
    c->channel->disconnect();
  }
  co_return;
}

static asio::awaitable<void> on_9D_9E(shared_ptr<Client> c, Channel::Message& msg) {
  const C_Login_DC_PC_GC_9D* base_cmd;
  auto s = c->require_server_state();

  if (msg.command == 0x9D) {
    base_cmd = &check_size_t<C_Login_DC_PC_GC_9D>(msg.data, sizeof(C_LoginExtended_PC_9D));
    if (base_cmd->is_extended) {
      if ((c->version() == Version::PC_NTE) || (c->version() == Version::PC_V2)) {
        const auto& cmd = check_size_t<C_LoginExtended_PC_9D>(msg.data);
        if (cmd.extension.lobby_refs[0].menu_id == MenuID::LOBBY) {
          c->preferred_lobby_id = cmd.extension.lobby_refs[0].item_id;
        }
      } else {
        const auto& cmd = check_size_t<C_LoginExtended_DC_GC_9D>(msg.data);
        if (cmd.extension.lobby_refs[0].menu_id == MenuID::LOBBY) {
          c->preferred_lobby_id = cmd.extension.lobby_refs[0].item_id;
        }
      }
    }

  } else if (msg.command == 0x9E) {
    const auto& cmd = check_size_t<C_Login_GC_9E>(msg.data, sizeof(C_LoginExtended_GC_9E));
    base_cmd = &cmd;
    if (cmd.is_extended) {
      const auto& cmd = check_size_t<C_LoginExtended_GC_9E>(msg.data);
      if (cmd.extension.lobby_refs[0].menu_id == MenuID::LOBBY) {
        c->preferred_lobby_id = cmd.extension.lobby_refs[0].item_id;
      }
    }

    if (is_v3(c->version())) {
      c->set_flag(Client::Flag::AT_WELCOME_MESSAGE);
    }

  } else {
    throw logic_error("9D/9E handler called for incorrect command");
  }

  c->hardware_id = base_cmd->hardware_id;
  c->channel->language = base_cmd->language;
  c->v1_serial_number = base_cmd->v1_serial_number.decode();
  c->v1_access_key = base_cmd->v1_access_key.decode();
  c->serial_number = base_cmd->serial_number.decode();
  c->access_key = base_cmd->access_key.decode();
  c->serial_number2 = base_cmd->serial_number2.decode();
  c->access_key2 = base_cmd->access_key2.decode();
  c->login_character_name = base_cmd->login_character_name.decode(c->language());
  set_console_client_flags(c, base_cmd->sub_version);

  try {
    switch (c->version()) {
      case Version::DC_V2: {
        uint32_t serial_number = stoul(c->serial_number, nullptr, 16);
        c->set_login(s->account_index->from_dc_credentials(
            serial_number, c->access_key, c->login_character_name, s->allow_unregistered_users));
        break;
      }
      case Version::PC_NTE:
      case Version::PC_V2:
        if ((c->sub_version == 0x29) &&
            c->v1_serial_number.empty() &&
            c->v1_access_key.empty() &&
            c->serial_number.empty() &&
            c->access_key.empty() &&
            c->serial_number2.empty() &&
            c->access_key2.empty()) {
          c->channel->version = Version::PC_NTE;
          c->log.info_f("Changed client version to PC_NTE");
          c->set_login(s->account_index->from_pc_nte_credentials(
              base_cmd->guild_card_number, s->allow_unregistered_users && s->allow_pc_nte));
        } else {
          uint32_t serial_number = stoul(base_cmd->serial_number.decode(), nullptr, 16);
          c->set_login(s->account_index->from_pc_credentials(
              serial_number, c->access_key, c->login_character_name, s->allow_unregistered_users));
        }
        break;
      case Version::GC_NTE:
      case Version::GC_V3:
      case Version::GC_EP3_NTE:
      case Version::GC_EP3: {
        uint32_t serial_number = stoul(base_cmd->serial_number.decode(), nullptr, 16);
        // GC clients should have sent a DB command first which would have
        // created the account if needed
        c->set_login(s->account_index->from_gc_credentials(
            serial_number, c->access_key, nullptr, c->login_character_name, false));
        break;
      }
      default:
        throw logic_error("unsupported versioned command");
    }

  } catch (const AccountIndex::no_username& e) {
    c->log.info_f("Login failed (no username)");
    send_command(c, 0x04, 0x03);
  } catch (const AccountIndex::incorrect_access_key& e) {
    c->log.info_f("Login failed (incorrect access key)");
    send_command(c, 0x04, 0x03);
  } catch (const AccountIndex::incorrect_password& e) {
    c->log.info_f("Login failed (incorrect password)");
    send_command(c, 0x04, 0x06);
  } catch (const AccountIndex::missing_account& e) {
    c->log.info_f("Login failed (missing account)");
    send_command(c, 0x04, 0x04);
  } catch (const AccountIndex::account_banned& e) {
    c->log.info_f("Login failed (account banned)");
    send_command(c, 0x04, 0x04);
  }

  if (!c->login) {
    c->channel->disconnect();
  } else {
    // On PCv2, we send a B2 command to get the client's specific_version and
    // check whether it has patch support or not; we'll call on_login_complete
    // once we receive the B3 response
    if (c->version() == Version::PC_V2) {
      try {
        auto code = s->function_code_index->name_to_function.at("ReturnTokenX86");
        unordered_map<string, uint32_t> label_writes{{"token", c->login->account->account_id}};
        auto resp = co_await send_function_call(c, code, label_writes, nullptr, 0, 0x00400000, 0x0000E000, 0, true);

        if (!c->login) {
          throw logic_error("received PC_V2 version detect response with no login");
        }
        if (resp.return_value == c->login->account->account_id) {
          // Client already has the patch that enables patches
          c->set_flag(Client::Flag::HAS_SEND_FUNCTION_CALL);
          c->set_flag(Client::Flag::SEND_FUNCTION_CALL_ACTUALLY_RUNS_CODE);
          c->set_flag(Client::Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH);
        }
        if (resp.checksum == 0x3677024C) {
          c->specific_version = SPECIFIC_VERSION_PC_V2_DEFAULT;
          c->log.info_f("Version detected as {:08X} from PE header checksum {:08X}",
              c->specific_version, resp.checksum);
        } else {
          c->specific_version = SPECIFIC_VERSION_PC_V2_INDETERMINATE;
          c->log.info_f("Version cannot be determined from PE header checksum {:08X}", resp.checksum);
        }
      } catch (const out_of_range&) {
      }
    }
    co_await on_login_complete(c);
  }
}

static asio::awaitable<void> on_9E_XB(shared_ptr<Client> c, Channel::Message& msg) {
  auto s = c->require_server_state();

  const auto& cmd = check_size_t<C_Login_XB_9E>(msg.data, sizeof(C_LoginExtended_XB_9E));
  if (cmd.is_extended) {
    const auto& cmd = check_size_t<C_LoginExtended_XB_9E>(msg.data);
    if (cmd.extension.lobby_refs[0].menu_id == MenuID::LOBBY) {
      c->preferred_lobby_id = cmd.extension.lobby_refs[0].item_id;
    }
  }
  c->hardware_id = cmd.hardware_id;
  c->channel->language = cmd.language;
  c->v1_serial_number = cmd.v1_serial_number.decode();
  c->v1_access_key = cmd.v1_access_key.decode();
  c->serial_number = cmd.serial_number.decode();
  c->access_key = cmd.access_key.decode();
  c->serial_number2 = cmd.serial_number2.decode();
  c->access_key2 = cmd.access_key2.decode();
  c->login_character_name = cmd.login_character_name.decode(c->language());
  c->xb_netloc = cmd.xb_netloc;
  c->xb_unknown_a1a = cmd.xb_unknown_a1a;
  c->xb_user_id = (static_cast<uint64_t>(cmd.xb_user_id_high) << 32) | cmd.xb_user_id_low;
  c->xb_unknown_a1b = cmd.xb_unknown_a1b;
  set_console_client_flags(c, cmd.sub_version);

  const string& xb_gamertag = c->serial_number;
  uint64_t xb_user_id = stoull(c->access_key, nullptr, 16);
  uint64_t xb_account_id = cmd.xb_netloc.account_id;
  try {
    c->set_login(s->account_index->from_xb_credentials(xb_gamertag, xb_user_id, xb_account_id, s->allow_unregistered_users));
  } catch (const AccountIndex::no_username& e) {
    c->log.info_f("Login failed (no username)");
    send_command(c, 0x04, 0x03);
  } catch (const AccountIndex::incorrect_access_key& e) {
    c->log.info_f("Login failed (incorrect access key)");
    send_command(c, 0x04, 0x03);
  } catch (const AccountIndex::missing_account& e) {
    c->log.info_f("Login failed (missing account)");
    send_command(c, 0x04, 0x03);
  } catch (const AccountIndex::account_banned& e) {
    c->log.info_f("Login failed (account banned)");
    send_command(c, 0x04, 0x04);
  }

  if (!c->login) {
    c->channel->disconnect();
  } else {
    co_await on_login_complete(c);
  }
  co_return;
}

static asio::awaitable<void> on_93_BB(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& base_cmd = check_size_t<C_LoginBase_BB_93>(msg.data, 0xFFFF);
  c->sub_version = base_cmd.sub_version;
  // c->channel->language set after version check
  c->bb_character_index = base_cmd.character_slot;
  c->bb_connection_phase = base_cmd.connection_phase;
  c->bb_client_code = base_cmd.client_code;
  c->bb_security_token = base_cmd.security_token;
  c->username = base_cmd.username.decode();
  c->password = base_cmd.password.decode();

  if (msg.data.size() == sizeof(C_LoginWithoutHardwareInfo_BB_93)) {
    c->bb_client_config = check_size_t<C_LoginWithoutHardwareInfo_BB_93>(msg.data).client_config;
  } else {
    const auto& full_cmd = check_size_t<C_LoginWithHardwareInfo_BB_93>(msg.data);
    c->hardware_id = full_cmd.hardware_id;
    c->bb_client_config = full_cmd.client_config;
  }
  c->set_flags_for_version(c->version(), base_cmd.sub_version);

  auto s = c->require_server_state();
  try {
    c->set_login(s->account_index->from_bb_credentials(c->username, &c->password, s->allow_unregistered_users));
  } catch (const AccountIndex::no_username& e) {
    c->log.info_f("Login failed (no username)");
    send_client_init_bb(c, 0x08);
  } catch (const AccountIndex::incorrect_password& e) {
    c->log.info_f("Login failed (incorrect password)");
    send_client_init_bb(c, 0x03);
  } catch (const AccountIndex::missing_account& e) {
    c->log.info_f("Login failed (missing account)");
    send_client_init_bb(c, 0x08);
  } catch (const AccountIndex::account_banned& e) {
    c->log.info_f("Login failed (account banned)");
    send_client_init_bb(c, 0x06);
  }
  if (!c->login) {
    c->channel->disconnect();
    co_return;
  }

  string version_string = c->bb_client_config.as_string();
  phosg::strip_trailing_zeroes(version_string);
  // If the version string starts with "Ver.", assume it's Sega and apply the
  // normal version encoding logic. Otherwise, assume it's a community mod,
  // almost all of which are based on TethVer12513, so assume that version.
  if (version_string.starts_with("Ver.")) {
    // Basic algorithm: take all numeric characters from the version string
    // and ignore everything else. Treat that as a decimal integer, then
    // base36-encode it into the low 3 bytes of specific_version.
    uint64_t version = 0;
    for (char ch : version_string) {
      if (isdigit(ch)) {
        version = (version * 10) + (ch - '0');
      }
    }
    uint8_t shift = 0;
    uint32_t specific_version = 0;
    while (version) {
      if (shift > 16) {
        throw runtime_error("invalid version string");
      }
      uint8_t ch = (version % 36) + '0';
      version /= 36;
      if (ch > '9') {
        ch += 7;
      }
      specific_version |= (ch << shift);
      shift += 8;
    }
    if (!(specific_version & 0x00FF0000)) {
      specific_version |= 0x00300000;
    }
    if (!(specific_version & 0x0000FF00)) {
      specific_version |= 0x00003000;
    }
    if (!(specific_version & 0x000000FF)) {
      specific_version |= 0x00000030;
    }
    c->specific_version = 0x35000000 | specific_version;

  } else {
    c->specific_version = 0x35394E4C; // 59NL

    // Note: Tethealla PSOBB is actually Japanese PSOBB, but with most of the
    // files replaced with English text/graphics/etc. For this reason, it still
    // reports its language as Japanese, so we have to account for that
    // manually here.
    if (version_string.starts_with("TethVer")) {
      c->log.info_f("Client is TethVer subtype; forcing English language");
      c->set_flag(Client::Flag::FORCE_ENGLISH_LANGUAGE_BB);
    }
  }
  c->channel->language = c->check_flag(Client::Flag::FORCE_ENGLISH_LANGUAGE_BB) ? 1 : base_cmd.language;

  if (base_cmd.menu_id == MenuID::LOBBY) {
    c->preferred_lobby_id = base_cmd.preferred_lobby_id;
  }

  if (base_cmd.guild_card_number == 0) {
    // There is a (bug? feature?) in the BB client such that it has to receive
    // a reconnect command during the data server phase, or else it won't know
    // where to connect to during character selection. It's not clear why they
    // didn't just make it use the initial connection address by default...
    send_client_init_bb(c, 0);
    send_reconnect(c, s->connect_address_for_client(c), s->name_to_port_config.at("bb-data1")->port);
    co_return;

  } else if (s->proxy_destination_bb.has_value()) {
    // Start a proxy session immediately if there's a destination set. Two
    // things to watch out for:
    // - Ignore the persistent config if this is the first data server
    //   connection, to prevent quick reconnects from incorrectly reusing the
    //   old session's state.
    // - We don't send 00E6 (send_client_init_bb) in this case. This is because
    //   the login command is resent to the remote server, and we forward its
    //   response back to the client directly.
    const auto& [host, port] = *s->proxy_destination_bb;
    co_await start_proxy_session(c, host, port, c->bb_connection_phase != 0);
    c->proxy_session->remote_client_config_data = c->bb_client_config;
    co_return;

  } else {
    send_client_init_bb(c, 0);
  }

  if (c->bb_connection_phase >= 0x04) {
    // This means the client is done with the data server phase and is in the
    // game server phase; we should send the ship select menu or a lobby join
    // command.
    co_await on_login_complete(c);

  } else if (s->hide_download_commands) {
    // The BB data server protocol is fairly well-understood and has some large
    // commands, so we omit data logging for clients on the data server.
    c->log.info_f("Client is in the BB data server phase; disabling command data logging for the rest of this client\'s session");
    c->channel->terminal_recv_color = phosg::TerminalFormat::END;
    c->channel->terminal_send_color = phosg::TerminalFormat::END;
  }
}

static asio::awaitable<void> on_96(shared_ptr<Client> c, Channel::Message& msg) {
  check_size_t<C_CharSaveInfo_DCv2_PC_V3_BB_96>(msg.data);
  c->set_flag(Client::Flag::SHOULD_SEND_ENABLE_SAVE);
  co_return;
}

static asio::awaitable<void> on_B1(shared_ptr<Client> c, Channel::Message& msg) {
  check_size_v(msg.data.size(), 0);
  send_server_time(c);
  co_return;
}

static asio::awaitable<void> on_B7_Ep3(shared_ptr<Client> c, Channel::Message& msg) {
  check_size_v(msg.data.size(), 0);

  // If the client is not in any lobby, assume they're at the main menu and
  // send the menu song (if any).
  auto s = c->require_server_state();
  auto l = c->lobby.lock();
  if (!l && (s->ep3_menu_song >= 0)) {
    send_ep3_change_music(c->channel, s->ep3_menu_song);
  }
  co_return;
}

static asio::awaitable<void> on_BA_Ep3(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& in_cmd = check_size_t<C_MesetaTransaction_Ep3_BA>(msg.data);
  auto s = c->require_server_state();
  auto l = c->lobby.lock();
  bool is_lobby = l && !l->is_game();

  uint32_t current_meseta, total_meseta_earned;
  if (s->ep3_infinite_meseta) {
    current_meseta = 1000000;
    total_meseta_earned = 1000000;
  } else if (is_lobby && s->ep3_jukebox_is_free) {
    current_meseta = c->login->account->ep3_current_meseta;
    total_meseta_earned = c->login->account->ep3_total_meseta_earned;
  } else {
    if (c->login->account->ep3_current_meseta < in_cmd.value) {
      S_MesetaTransaction_Ep3_BA out_cmd = {
          c->login->account->ep3_current_meseta,
          c->login->account->ep3_total_meseta_earned,
          in_cmd.request_token};
      send_command(c, msg.command, 0x04, &out_cmd, sizeof(out_cmd));
      co_return;
    }
    c->login->account->ep3_current_meseta -= in_cmd.value;
    if (s->allow_saving_accounts) {
      c->login->account->save();
    }
    current_meseta = c->login->account->ep3_current_meseta;
    total_meseta_earned = c->login->account->ep3_total_meseta_earned;
  }

  S_MesetaTransaction_Ep3_BA out_cmd = {current_meseta, total_meseta_earned, in_cmd.request_token};
  send_command(c, msg.command, 0x03, &out_cmd, sizeof(out_cmd));
  co_return;
}

static bool add_next_game_client(shared_ptr<Lobby> l) {
  auto it = l->clients_to_add.begin();
  if (it == l->clients_to_add.end()) {
    return false;
  }
  size_t target_client_id = it->first;
  shared_ptr<Client> c = it->second.lock();
  l->clients_to_add.erase(it);

  auto tourn = l->tournament_match ? l->tournament_match->tournament.lock() : nullptr;

  // If the game is a tournament match and the client has disconnected before
  // they could join the match, disband the entire game
  if (!c && l->tournament_match) {
    l->log.info_f("Client in slot {} has disconnected before joining the game; disbanding it", target_client_id);
    send_command(l, 0xED, 0x00);
    return false;
  }

  if (l->clients.at(target_client_id) != nullptr) {
    throw logic_error("client id is already in use");
  }

  auto s = c->require_server_state();
  if (tourn) {
    G_SetStateFlags_Ep3_6xB4x03 state_cmd;
    state_cmd.state.turn_num = 1;
    state_cmd.state.battle_phase = Episode3::BattlePhase::INVALID_00;
    state_cmd.state.current_team_turn1 = 0xFF;
    state_cmd.state.current_team_turn2 = 0xFF;
    state_cmd.state.action_subphase = Episode3::ActionSubphase::ATTACK;
    state_cmd.state.setup_phase = Episode3::SetupPhase::REGISTRATION;
    state_cmd.state.registration_phase = Episode3::RegistrationPhase::AWAITING_NUM_PLAYERS;
    state_cmd.state.team_exp.clear(0);
    state_cmd.state.team_dice_bonus.clear(0);
    state_cmd.state.first_team_turn = 0xFF;
    state_cmd.state.tournament_flag = 0x01;
    state_cmd.state.client_sc_card_types.clear(Episode3::CardType::INVALID_FF);
    if ((c->version() != Version::GC_EP3_NTE) && !(s->ep3_behavior_flags & Episode3::BehaviorFlag::DISABLE_MASKING)) {
      uint8_t mask_key = (phosg::random_object<uint32_t>() % 0xFF) + 1;
      set_mask_for_ep3_game_command(&state_cmd, sizeof(state_cmd), mask_key);
    }
    send_command_t(c, 0xC9, 0x00, state_cmd);
  }

  s->change_client_lobby(c, l, true, target_client_id);
  c->set_flag(Client::Flag::LOADING);
  c->log.info_f("LOADING flag set");
  if (tourn) {
    c->set_flag(Client::Flag::LOADING_TOURNAMENT);
  }
  c->disconnect_hooks.emplace(ADD_NEXT_CLIENT_DISCONNECT_HOOK_NAME, [s, l]() -> void {
    add_next_game_client(l);
  });

  return true;
}

static bool start_ep3_battle_table_game_if_ready(shared_ptr<Lobby> l, int16_t table_number) {
  if (table_number < 0) {
    // Negative numbers are supposed to mean the client is not seated at a
    // table, so it's an error for this function to be called with a negative
    // table number
    throw runtime_error("negative table number");
  }

  // Figure out which clients are at this table. If any client has declined, we
  // never start a match, but we may start a match even if all clients have not
  // yet accepted (in case of a tournament match).
  Version game_version = Version::UNKNOWN;
  unordered_map<size_t, shared_ptr<Client>> table_clients;
  bool all_clients_accepted = true;
  for (const auto& c : l->clients) {
    if (!c || (c->card_battle_table_number != table_number)) {
      continue;
    }
    // Prevent match from starting unless all players are on the same version
    if (game_version == Version::UNKNOWN) {
      game_version = c->version();
    } else if (game_version != c->version()) {
      return false;
    }
    if (c->card_battle_table_seat_number >= 4) {
      throw runtime_error("invalid seat number");
    }
    // Apparently this can actually happen; just prevent them from starting a
    // battle if multiple players are in the same seat
    if (!table_clients.emplace(c->card_battle_table_seat_number, c).second) {
      return false;
    }
    if (c->card_battle_table_seat_state == 3) {
      return false;
    }
    if (c->card_battle_table_seat_state != 2) {
      all_clients_accepted = false;
    }
  }
  if (table_clients.size() > 4) {
    throw runtime_error("too many clients at battle table");
  }

  // Figure out if this is a tournament match setup
  unordered_set<shared_ptr<Episode3::Tournament::Match>> tourn_matches;
  for (const auto& it : table_clients) {
    auto team = it.second->ep3_tournament_team.lock();
    auto tourn = team ? team->tournament.lock() : nullptr;
    auto match = tourn ? tourn->next_match_for_team(team) : nullptr;
    // Note: We intentionally don't check for null here. This is to handle the
    // case where a tournament-registered player steps into a seat at a table
    // where a non-tournament-registered player is already present - we should
    // NOT start any match until the non-tournament-registered player leaves,
    // or they both accept (and we start a non-tournament match).
    tourn_matches.emplace(match);
  }

  // Get the tournament. Invariant: both tourn_match and tourn are null, or
  // neither are null.
  auto tourn_match = (tourn_matches.size() == 1) ? *tourn_matches.begin() : nullptr;
  auto tourn = tourn_match ? tourn_match->tournament.lock() : nullptr;
  if (!tourn || !tourn_match->preceding_a->winner_team || !tourn_match->preceding_b->winner_team) {
    tourn.reset();
    tourn_match.reset();
  }

  // If this is a tournament match setup, check if all required players are
  // present and rearrange their client IDs to match their team positions
  unordered_map<size_t, shared_ptr<Client>> game_clients;
  if (tourn_match) {
    unordered_map<size_t, uint32_t> required_account_ids;
    auto add_team_players = [&](shared_ptr<const Episode3::Tournament::Team> team, size_t base_index) -> void {
      size_t z = 0;
      for (const auto& player : team->players) {
        if (z >= 2) {
          throw logic_error("more than 2 players on team");
        }
        if (player.is_human()) {
          required_account_ids.emplace(base_index + z, player.account_id);
        }
        z++;
      }
    };
    add_team_players(tourn_match->preceding_a->winner_team, 0);
    add_team_players(tourn_match->preceding_b->winner_team, 2);

    for (const auto& it : required_account_ids) {
      size_t client_id = it.first;
      uint32_t account_id = it.second;
      for (const auto& it : table_clients) {
        if (it.second->login->account->account_id == account_id) {
          game_clients.emplace(client_id, it.second);
        }
      }
    }

    if (game_clients.size() != required_account_ids.size()) {
      // Not all tournament match participants are present, so we can't start
      // the tournament match. (But they can still use the battle table)
      tourn_match.reset();
      tourn.reset();
    } else {
      // If there is already a game for this match, don't allow a new one to
      // start
      auto s = l->require_server_state();
      for (auto l : s->all_lobbies()) {
        if (l->tournament_match == tourn_match) {
          tourn_match.reset();
          tourn.reset();
        }
      }
    }
  }

  // In the non-tournament case (or if the tournament case was rejected above),
  // only start the game if all players have accepted. If they have, just put
  // them in the clients map in seat order.
  if (!tourn_match) {
    if (!all_clients_accepted) {
      return false;
    }
    game_clients = std::move(table_clients);
  }

  // If there are no clients, do nothing (this happens when the last player
  // leaves a battle table without starting a game)
  if (game_clients.empty()) {
    return false;
  }

  // At this point, we've checked all the necessary conditions for a game to
  // begin, but create_game_generic can still return null if an internal
  // precondition fails (though this should never happen for Episode 3 games).

  auto c = game_clients.begin()->second;
  auto s = c->require_server_state();
  string name = tourn ? tourn->get_name() : "<BattleTable>";
  auto game = create_game_generic(s, c, name, "", Episode::EP3);
  if (!game) {
    return false;
  }
  game->tournament_match = tourn_match;
  game->ep3_ex_result_values = (tourn_match && tourn && tourn->get_final_match() == tourn_match)
      ? s->ep3_tournament_final_round_ex_values
      : s->ep3_tournament_ex_values;
  game->clients_to_add.clear();
  for (const auto& it : game_clients) {
    game->clients_to_add.emplace(it.first, it.second);
  }

  // Remove all players from the battle table (but don't tell them about this)
  for (const auto& it : game_clients) {
    auto other_c = it.second;
    other_c->card_battle_table_number = -1;
    other_c->card_battle_table_seat_number = 0;
    other_c->disconnect_hooks.erase(BATTLE_TABLE_DISCONNECT_HOOK_NAME);
  }

  // If there's only one client in the match, skip the wait phase - they'll be
  // added to the match immediately by add_next_game_client anyway
  if (game_clients.empty()) {
    throw logic_error("no clients to add to battle table match");

  } else if (game_clients.size() != 1) {
    for (const auto& it : game_clients) {
      auto other_c = it.second;
      send_self_leave_notification(other_c);
      string message;
      if (tourn) {
        message = std::format(
            "$C7Waiting to begin match in tournament\n$C6{}$C7...\n\n(Hold B+X+START to abort)",
            tourn->get_name());
      } else {
        message = "$C7Waiting to begin battle table match...\n\n(Hold B+X+START to abort)";
      }
      send_message_box(other_c, message);
    }
  }

  // Add the first client to the game (the remaining clients will be added when
  // the previous is done loading)
  add_next_game_client(game);

  return true;
}

static void on_ep3_battle_table_state_updated(shared_ptr<Lobby> l, int16_t table_number) {
  send_ep3_card_battle_table_state(l, table_number);
  start_ep3_battle_table_game_if_ready(l, table_number);
}

static asio::awaitable<void> on_E4_Ep3(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& cmd = check_size_t<C_CardBattleTableState_Ep3_E4>(msg.data);
  auto l = c->require_lobby();

  if (cmd.seat_number >= 4) {
    throw runtime_error("invalid seat number");
  }

  if (msg.flag) {
    if (l->is_game() || !l->is_ep3()) {
      throw runtime_error("battle table join command sent in non-CARD lobby");
    }
    c->card_battle_table_number = cmd.table_number;
    c->card_battle_table_seat_number = cmd.seat_number;
    c->card_battle_table_seat_state = 1;

  } else { // Leaving battle table
    c->card_battle_table_number = -1;
    c->card_battle_table_seat_number = 0;
    c->card_battle_table_seat_state = 0;
  }

  on_ep3_battle_table_state_updated(l, cmd.table_number);

  bool should_have_disconnect_hook = (c->card_battle_table_number != -1);

  if (should_have_disconnect_hook && !c->disconnect_hooks.count(BATTLE_TABLE_DISCONNECT_HOOK_NAME)) {
    c->disconnect_hooks.emplace(BATTLE_TABLE_DISCONNECT_HOOK_NAME, [l, c]() -> void {
      int16_t table_number = c->card_battle_table_number;
      c->card_battle_table_number = -1;
      c->card_battle_table_seat_number = 0;
      c->card_battle_table_seat_state = 0;
      if (table_number != -1) {
        on_ep3_battle_table_state_updated(l, table_number);
      }
    });
  } else if (!should_have_disconnect_hook) {
    c->disconnect_hooks.erase(BATTLE_TABLE_DISCONNECT_HOOK_NAME);
  }
  co_return;
}

static asio::awaitable<void> on_E5_Ep3(shared_ptr<Client> c, Channel::Message& msg) {
  check_size_t<S_CardBattleTableConfirmation_Ep3_E5>(msg.data);
  auto l = c->require_lobby();
  if (l->is_game() || !l->is_ep3()) {
    throw runtime_error("battle table command sent in non-CARD lobby");
  }

  if (c->card_battle_table_number < 0) {
    throw runtime_error("invalid table number");
  }

  if (msg.flag) {
    c->card_battle_table_seat_state = 2;
  } else {
    c->card_battle_table_seat_state = 3;
  }
  on_ep3_battle_table_state_updated(l, c->card_battle_table_number);
  co_return;
}

static asio::awaitable<void> on_DC_Ep3(shared_ptr<Client> c, Channel::Message& msg) {
  check_size_v(msg.data.size(), 0);

  auto l = c->lobby.lock();
  if (!l) {
    co_return;
  }

  if (msg.flag != 0) {
    c->clear_flag(Client::Flag::LOADING_TOURNAMENT);
    l->set_flag(Lobby::Flag::BATTLE_IN_PROGRESS);
    send_command(c, 0xDC, 0x00);
    send_ep3_start_tournament_deck_select_if_all_clients_ready(l);
  } else {
    l->clear_flag(Lobby::Flag::BATTLE_IN_PROGRESS);
  }
  co_return;
}

static void on_tournament_bracket_updated(shared_ptr<ServerState> s, shared_ptr<const Episode3::Tournament> tourn) {
  tourn->send_all_state_updates();
  if (tourn->get_state() == Episode3::Tournament::State::COMPLETE) {
    auto team = tourn->get_winner_team();
    if (!team->has_any_human_players()) {
      send_ep3_text_message_fmt(s, "$C7A CPU team won\nthe tournament\n$C6{}", tourn->get_name());
    } else {
      send_ep3_text_message_fmt(s, "$C6{}$C7\nwon the tournament\n$C6{}", team->name, tourn->get_name());
    }
    s->ep3_tournament_index->delete_tournament(tourn->get_name());
  } else {
    s->ep3_tournament_index->save();
  }
}

static asio::awaitable<void> on_CA_Ep3(shared_ptr<Client> c, Channel::Message& msg) {
  auto l = c->lobby.lock();
  if (!l) {
    // In rare cases (e.g. when two players end a tournament's match results
    // screens at exactly the same time), the client can send a server data
    // command when it's not in any lobby at all. We just ignore such commands.
    co_return;
  }
  if (!l->is_game() || !l->is_ep3()) {
    throw runtime_error("Episode 3 server data request sent outside of Episode 3 game");
  }

  if (l->battle_player) {
    co_return;
  }

  auto s = l->require_server_state();

  const auto& header = check_size_t<G_CardServerDataCommandHeader>(msg.data, 0xFFFF);
  if (header.subcommand != 0xB3) {
    throw runtime_error("unknown Episode 3 server data request");
  }

  if (!l->ep3_server || l->ep3_server->battle_finished) {
    auto s = c->require_server_state();

    if (s->ep3_behavior_flags & Episode3::BehaviorFlag::ENABLE_RECORDING) {
      l->battle_record = make_shared<Episode3::BattleRecord>(s->ep3_behavior_flags);
      for (auto existing_c : l->clients) {
        if (existing_c) {
          auto existing_p = existing_c->character();
          PlayerLobbyDataDCGC lobby_data;
          lobby_data.name.encode(existing_p->disp.name.decode(existing_c->language()), c->language());
          lobby_data.player_tag = 0x00010000;
          lobby_data.guild_card_number = existing_c->login->account->account_id;
          l->battle_record->add_player(
              lobby_data,
              existing_p->inventory,
              existing_p->disp.to_dcpcv3<false>(c->language(), c->language()),
              c->ep3_config ? (c->ep3_config->online_clv_exp / 100) : 0);
        }
      }
    }

    l->create_ep3_server();
  }

  bool battle_finished_before = l->ep3_server->battle_finished;

  try {
    l->ep3_server->on_server_data_input(c, msg.data);
  } catch (const exception& e) {
    c->log.error_f("Episode 3 engine returned an error: {}", e.what());
    if (l->battle_record) {
      string filename = std::format("system/ep3/battle-records/exc.{}.mzrd", phosg::now());
      phosg::save_file(filename, l->battle_record->serialize());
      c->log.error_f("Saved partial battle record as {}", filename);
    }
    throw;
  }

  // If the battle has finished, finalize the recording and link it to all
  // participating players and spectators
  if (!battle_finished_before && l->ep3_server->battle_finished && l->battle_record) {
    l->battle_record->set_battle_end_timestamp();
    unordered_set<shared_ptr<Lobby>> lobbies;
    lobbies.emplace(l);
    for (const auto& wl : l->watcher_lobbies) {
      lobbies.emplace(wl);
    }
    for (const auto& rl : lobbies) {
      for (const auto& rc : rl->clients) {
        if (rc) {
          rc->ep3_prev_battle_record = l->battle_record;
          if ((s->ep3_behavior_flags & Episode3::BehaviorFlag::ENABLE_STATUS_MESSAGES)) {
            send_text_message(rc, "$C7Recording complete");
          }
        }
      }
    }
    l->battle_record.reset();
  }

  if (l->tournament_match &&
      l->ep3_server->setup_phase == Episode3::SetupPhase::BATTLE_ENDED &&
      !l->ep3_server->tournament_match_result_sent) {
    int8_t winner_team_id = l->ep3_server->get_winner_team_id();
    if (winner_team_id == -1) {
      throw runtime_error("match complete, but winner team not specified");
    }

    auto tourn = l->tournament_match->tournament.lock();

    shared_ptr<Episode3::Tournament::Team> winner_team;
    shared_ptr<Episode3::Tournament::Team> loser_team;
    if (winner_team_id == 0) {
      winner_team = l->tournament_match->preceding_a->winner_team;
      loser_team = l->tournament_match->preceding_b->winner_team;
    } else if (winner_team_id == 1) {
      winner_team = l->tournament_match->preceding_b->winner_team;
      loser_team = l->tournament_match->preceding_a->winner_team;
    } else {
      throw logic_error("invalid winner team id");
    }
    l->tournament_match->set_winner_team(winner_team);

    uint32_t meseta_reward = 0;
    auto& round_rewards = loser_team->has_any_human_players()
        ? s->ep3_defeat_player_meseta_rewards
        : s->ep3_defeat_com_meseta_rewards;
    meseta_reward = (l->tournament_match->round_num - 1 < round_rewards.size())
        ? round_rewards[l->tournament_match->round_num - 1]
        : round_rewards.back();
    if (tourn && (l->tournament_match == tourn->get_final_match())) {
      meseta_reward += s->ep3_final_round_meseta_bonus;
    }
    for (const auto& player : winner_team->players) {
      if (player.is_human()) {
        auto winner_c = player.client.lock();
        if (winner_c) {
          winner_c->login->account->ep3_current_meseta += meseta_reward;
          winner_c->login->account->ep3_total_meseta_earned += meseta_reward;
          if (s->allow_saving_accounts) {
            winner_c->login->account->save();
          }
          send_ep3_rank_update(winner_c);
        }
      }
    }
    send_ep3_tournament_match_result(l, meseta_reward);

    if (tourn) {
      on_tournament_bracket_updated(s, tourn);
    }
    l->ep3_server->tournament_match_result_sent = true;
  }
}

static asio::awaitable<void> on_E2_Ep3(shared_ptr<Client> c, Channel::Message& msg) {
  switch (msg.flag) {
    case 0x00: // Request tournament list
      send_ep3_tournament_list(c, false);
      break;
    case 0x01: { // Check tournament
      auto team = c->ep3_tournament_team.lock();
      if (team) {
        auto tourn = team->tournament.lock();
        if (tourn) {
          send_ep3_tournament_entry_list(c, tourn, false);
        } else {
          send_lobby_message_box(c, "$C7The tournament\nhas concluded.");
        }
      } else {
        send_lobby_message_box(c, "$C7You are not\nregistered in a\ntournament.");
      }
      break;
    }
    case 0x02: { // Cancel tournament entry
      auto team = c->ep3_tournament_team.lock();
      if (team) {
        auto tourn = team->tournament.lock();
        if (tourn) {
          if (tourn->get_state() != Episode3::Tournament::State::COMPLETE) {
            auto s = c->require_server_state();
            team->unregister_player(c->login->account->account_id);
            on_tournament_bracket_updated(s, tourn);
          }
          c->ep3_tournament_team.reset();
        }
      }
      if (c->version() != Version::GC_EP3_NTE) {
        send_ep3_confirm_tournament_entry(c, nullptr);
      }
      break;
    }
    case 0x03: // Create tournament spectator team (get battle list)
    case 0x04: // Join tournament spectator team (get team list)
      send_lobby_message_box(c, "$C7Use View Regular\nBattle for this");
      break;
    default:
      throw runtime_error("invalid tournament operation");
  }
  co_return;
}

static asio::awaitable<void> on_D6_V3(shared_ptr<Client> c, Channel::Message& msg) {
  check_size_v(msg.data.size(), 0);
  if (c->check_flag(Client::Flag::IN_INFORMATION_MENU)) {
    auto s = c->require_server_state();
    send_menu(c, s->information_menu(c->version()));
  } else if (c->check_flag(Client::Flag::AT_WELCOME_MESSAGE)) {
    c->clear_flag(Client::Flag::AT_WELCOME_MESSAGE);
    send_main_menu(c);
  }
  co_return;
}

static asio::awaitable<void> on_09(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& cmd = check_size_t<C_MenuItemInfoRequest_09>(msg.data);
  auto s = c->require_server_state();

  switch (cmd.menu_id) {
    case MenuID::QUEST_CATEGORIES_EP1:
    case MenuID::QUEST_CATEGORIES_EP2:
      // Don't send anything here. The quest filter menu already has short
      // descriptions included with the entries, which the client shows in the
      // usual location on the screen.
      break;
    case MenuID::QUEST_EP1:
    case MenuID::QUEST_EP2: {
      bool is_download_quest = !c->lobby.lock();
      auto quest_index = s->quest_index(c->version());
      if (!quest_index) {
        send_quest_info(c, "$C7Quests are not available.", 0x00, is_download_quest);
      } else {
        auto q = quest_index->get(cmd.item_id);
        if (!q) {
          send_quest_info(c, "$C4Quest does not\nexist.", 0x00, is_download_quest);
        } else {
          auto vq = q->version(c->version(), c->language());
          if (!vq) {
            send_quest_info(c, "$C4Quest does not\nexist for this game\nversion.", 0x00, is_download_quest);
          } else {
            send_quest_info(c, vq->long_description, vq->description_flag, is_download_quest);
          }
        }
      }
      break;
    }

    case MenuID::GAME: {
      auto game = s->find_lobby(cmd.item_id);
      if (!game) {
        send_ship_info(c, "$C4Game no longer\nexists.");
        break;
      }

      if (!game->is_game()) {
        send_ship_info(c, "$C4Incorrect game ID");

      } else if (is_ep3(c->version()) && game->is_ep3()) {
        send_ep3_game_details(c, game);

      } else {
        string info;
        if (c->last_game_info_requested != game->lobby_id) {
          // Send page 1 (players)
          c->last_game_info_requested = game->lobby_id;
          for (size_t x = 0; x < game->max_clients; x++) {
            const auto& game_c = game->clients[x];
            if (game_c.get()) {
              static_assert(NUM_VERSIONS == 14, "Don\'t forget to update the game player listing version tokens");
              static const array<const char*, NUM_VERSIONS> version_tokens = {
                  " $C4P2$C7",
                  " $C4P4$C7",
                  " $C5DCN$C7",
                  " $C5DCP$C7",
                  " $C2DC1$C7",
                  " $C2DC2$C7",
                  " $C5PCN$C7",
                  " $C2PC$C7",
                  " $C5GCN$C7",
                  " $C2GC$C7",
                  " $C5Ep3N$C7",
                  " $C2Ep3$C7",
                  " $C2XB$C7",
                  " $C2BB$C7",
              };
              const char* version_token = (game_c->version() != c->version())
                  ? version_tokens.at(static_cast<size_t>(game_c->version()))
                  : "";
              auto player = game_c->character();
              string name = escape_player_name(player->disp.name.decode(game_c->language()));
              info += std::format("{}{}\n  {} Lv{} {}\n",
                  name,
                  version_token,
                  name_for_char_class(player->disp.visual.char_class),
                  player->disp.stats.level + 1,
                  char_for_language_code(game_c->language()));
            }
          }
        }

        // If page 1 is blank (there are no players) or we sent page 1 last
        // time, send page 2 (extended info)
        if (info.empty()) {
          c->last_game_info_requested = 0;
          info += std::format("Section ID: {}\n", name_for_section_id(game->effective_section_id()));
          if (game->max_level != 0xFFFFFFFF) {
            info += std::format("Req. level: {}-{}\n", game->min_level + 1, game->max_level + 1);
          } else if (game->min_level != 0) {
            info += std::format("Req. level: {}+\n", game->min_level + 1);
          }

          if (game->check_flag(Lobby::Flag::CHEATS_ENABLED)) {
            info += "$C6Cheats enabled$C7\n";
          }
          if (game->check_flag(Lobby::Flag::PERSISTENT)) {
            info += "$C6Persistence enabled$C7\n";
          }

          if (game->quest) {
            info += (game->check_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS)) ? "$C6Quest: " : "$C4Quest: ";
            info += remove_color(game->quest->name);
            info += "\n";
          } else if (game->check_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS)) {
            info += "$C6Quest in progress\n";
          } else if (game->check_flag(Lobby::Flag::QUEST_IN_PROGRESS)) {
            info += "$C4Quest in progress\n";
          } else if (game->check_flag(Lobby::Flag::QUEST_SELECTION_IN_PROGRESS)) {
            info += "$C4Selecting quest\n";
          }

          switch (game->drop_mode) {
            case Lobby::DropMode::DISABLED:
              info += "$C6Drops disabled$C7\n";
              break;
            case Lobby::DropMode::CLIENT:
              info += "$C6Client drops$C7\n";
              break;
            case Lobby::DropMode::SERVER_SHARED:
              info += "$C6Server drops$C7\n";
              break;
            case Lobby::DropMode::SERVER_PRIVATE:
              info += "$C6Private drops$C7\n";
              break;
            case Lobby::DropMode::SERVER_DUPLICATE:
              info += "$C6Duplicate drops$C7\n";
              break;
          }
        }
        phosg::strip_trailing_whitespace(info);
        send_ship_info(c, info);
      }
      break;
    }

    case MenuID::TOURNAMENTS_FOR_SPEC:
    case MenuID::TOURNAMENTS: {
      if (!is_ep3(c->version())) {
        send_ship_info(c, "Incorrect menu ID");
        break;
      }
      auto tourn = s->ep3_tournament_index->get_tournament(cmd.item_id);
      if (tourn) {
        send_ep3_tournament_details(c, tourn);
      }
      break;
    }

    case MenuID::TOURNAMENT_ENTRIES: {
      if (!is_ep3(c->version())) {
        send_ship_info(c, "Incorrect menu ID");
        break;
      }
      uint16_t tourn_num = cmd.item_id >> 16;
      uint16_t team_index = cmd.item_id & 0xFFFF;
      auto tourn = s->ep3_tournament_index->get_tournament(tourn_num);
      if (tourn) {
        auto team = tourn->get_team(team_index);
        if (team) {
          string message;
          string team_name = escape_player_name(team->name);
          if (team_name.empty()) {
            message = "(No registrant)";
          } else if (team->max_players == 1) {
            message = std::format("$C6{}$C7\n{} {} ({})\nPlayers:",
                team_name,
                team->num_rounds_cleared,
                team->num_rounds_cleared == 1 ? "win" : "wins",
                team->is_active ? "active" : "defeated");
          } else {
            message = std::format("$C6{}$C7\n{} {} ({}){}\nPlayers:",
                team_name,
                team->num_rounds_cleared,
                team->num_rounds_cleared == 1 ? "win" : "wins",
                team->is_active ? "active" : "defeated",
                team->password.empty() ? "" : "\n$C4Locked$C7");
          }
          for (const auto& player : team->players) {
            if (player.is_human()) {
              if (player.player_name.empty()) {
                message += std::format("\n  $C6{:08X}$C7", player.account_id);
              } else {
                string player_name = escape_player_name(player.player_name);
                message += std::format("\n  $C6{}$C7 ({:08X})", player_name, player.account_id);
              }
            } else {
              string player_name = escape_player_name(player.com_deck->player_name);
              string deck_name = escape_player_name(player.com_deck->deck_name);
              message += std::format("\n  $C3{} \"{}\"$C7", player_name, deck_name);
            }
          }
          send_ship_info(c, message);
        } else {
          send_ship_info(c, "$C7No such team");
        }
      } else {
        send_ship_info(c, "$C7No such tournament");
      }
      break;
    }

    default:
      if (!c->last_menu_sent || c->last_menu_sent->menu_id != cmd.menu_id) {
        send_ship_info(c, "Incorrect menu ID");
      } else {
        for (const auto& item : c->last_menu_sent->items) {
          if (item.item_id == cmd.item_id) {
            if (item.get_description != nullptr) {
              send_ship_info(c, item.get_description());
            } else {
              send_ship_info(c, item.description);
            }
            co_return;
          }
        }
        send_ship_info(c, "$C4Incorrect menu\nitem ID");
      }
      break;
  }
  co_return;
}

static void on_quest_loaded(shared_ptr<Lobby> l) {
  if (!l->quest) {
    throw logic_error("on_quest_loaded called without a quest loaded");
  }

  // Replace the free-play map with the quest's map
  l->load_maps();

  auto s = l->require_server_state();

  // Delete all floor items
  for (auto& m : l->floor_item_managers) {
    m.clear();
  }

  for (auto& lc : l->clients) {
    if (!lc) {
      continue;
    }

    if (lc->version() == Version::BB_V4) {
      send_rare_enemy_index_list(lc, l->map_state->bb_rare_enemy_indexes);
    }

    lc->delete_overlay();
    if (l->quest->battle_rules) {
      lc->use_default_bank();
      lc->create_battle_overlay(l->quest->battle_rules, s->level_table(lc->version()));
      lc->log.info_f("Created battle overlay");
    } else if (l->quest->challenge_template_index >= 0 && !is_v4(lc->version())) {
      // On BB, the client will send a sequence of DF commands that creates the
      // overlay; on non-BB, we do it at quest start time instead (hence the
      // version check above).
      lc->use_default_bank();
      lc->create_challenge_overlay(lc->version(), l->quest->challenge_template_index, s->level_table(lc->version()));
      lc->log.info_f("Created challenge overlay");
      l->assign_inventory_and_bank_item_ids(lc, true);
    }
  }
}

void set_lobby_quest(shared_ptr<Lobby> l, shared_ptr<const Quest> q, bool substitute_v3_for_ep3) {
  if (!l->is_game()) {
    throw logic_error("non-game lobby cannot accept a quest");
  }
  if (l->quest) {
    throw runtime_error("lobby already has an assigned quest");
  }

  // Only allow loading battle/challenge quests if the game mode is correct
  if ((q->challenge_template_index >= 0) != (l->mode == GameMode::CHALLENGE)) {
    throw runtime_error("incorrect game mode");
  }
  if ((q->battle_rules != nullptr) != (l->mode == GameMode::BATTLE)) {
    throw runtime_error("incorrect game mode");
  }

  auto s = l->require_server_state();

  if (q->joinable) {
    l->set_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS);
  } else {
    l->set_flag(Lobby::Flag::QUEST_IN_PROGRESS);
  }
  l->clear_flag(Lobby::Flag::QUEST_SELECTION_IN_PROGRESS);
  l->clear_flag(Lobby::Flag::PERSISTENT);

  l->quest = q;
  if (l->episode != Episode::EP3) {
    l->episode = q->episode;
  }
  l->create_item_creator();

  size_t num_clients_with_loading_flag = 0;
  for (size_t client_id = 0; client_id < l->max_clients; client_id++) {
    auto lc = l->clients[client_id];
    if (!lc) {
      continue;
    }

    Version effective_version = (substitute_v3_for_ep3 && is_ep3(lc->version())) ? Version::GC_V3 : lc->version();

    auto vq = q->version(effective_version, lc->language());
    if (!vq) {
      send_lobby_message_box(lc, "$C6Quest does not exist\nfor this game version.");
      lc->channel->disconnect();
      break;
    }
    lc->log.info_f("Sending {} version of quest \"{}\"", char_for_language_code(vq->language), vq->name);

    string bin_filename = vq->bin_filename();
    string dat_filename = vq->dat_filename();
    string xb_filename = vq->xb_filename();
    send_open_quest_file(lc, bin_filename, bin_filename, xb_filename, vq->quest_number, QuestFileType::ONLINE, vq->bin_contents);
    send_open_quest_file(lc, dat_filename, dat_filename, xb_filename, vq->quest_number, QuestFileType::ONLINE, vq->dat_contents);

    // There is no such thing as command AC (quest barrier) on PSO V1 and V2;
    // quests just start immediately when they're done downloading. (This is
    // also the case on GC Trial Edition.) There are also no chunk
    // acknowledgements (C->S 13 commands) like there are on v3 and later. So,
    // for pre-V3 clients, we can just not set the loading flag, since we
    // won't be told when to clear it later.
    // TODO: For V3 and V4 clients, the quest doesn't finish loading here, so
    // technically we should queue up commands sent by pre-V3 clients, since
    // they might start the quest before V3/V4 clients do. We can probably use
    // a method similar to game_join_command_queue.
    if (!is_v1_or_v2(lc->version())) {
      num_clients_with_loading_flag++;
      lc->set_flag(Client::Flag::LOADING_QUEST);
      lc->log.info_f("LOADING_QUEST flag set");
      lc->disconnect_hooks.emplace(QUEST_BARRIER_DISCONNECT_HOOK_NAME, [l]() -> void {
        send_quest_barrier_if_all_clients_ready(l);
      });
    } else {
      lc->log.info_f("LOADING_QUEST flag skipped");
    }
  }

  if (num_clients_with_loading_flag == 0) {
    l->log.info_f("No clients require the LOADING_QUEST flag; starting quest now");
    on_quest_loaded(l);
  }
}

static asio::awaitable<void> on_10_main_menu(shared_ptr<Client> c, uint32_t item_id) {
  auto s = c->require_server_state();

  switch (item_id) {
    case MainMenuItemID::GO_TO_LOBBY: {
      co_await send_auto_patches_if_needed(c);
      co_await enable_save_if_needed(c);
      send_lobby_list(c);
      if (is_pre_v1(c->version())) {
        co_await send_get_player_info(c);
      }
      if (!c->lobby.lock()) {
        s->add_client_to_available_lobby(c);
      }
      break;
    }

    case MainMenuItemID::INFORMATION: {
      send_menu(c, s->information_menu(c->version()));
      c->set_flag(Client::Flag::IN_INFORMATION_MENU);
      break;
    }

    case MainMenuItemID::PROXY_DESTINATIONS:
      send_proxy_destinations_menu(c);
      break;

    case MainMenuItemID::DOWNLOAD_QUESTS: {
      QuestMenuType menu_type = QuestMenuType::DOWNLOAD;
      if (is_ep3(c->version())) {
        menu_type = QuestMenuType::EP3_DOWNLOAD;
        // Episode 3 has only download quests, not online quests, so this is
        // always the download quest menu. (Episode 3 does actually have
        // online quests, but they're served via a server data request
        // instead of the file download paradigm that other versions use.)
        auto quest_index = s->quest_index(c->version());
        uint16_t version_flags = (1 << static_cast<size_t>(c->version()));
        const auto& categories = quest_index->categories(menu_type, Episode::EP3, version_flags);
        if (categories.size() == 1) {
          auto quests = quest_index->filter(Episode::EP3, version_flags, categories[0]->category_id);
          send_quest_menu(c, quests, true);
          break;
        }
      }

      send_quest_categories_menu(c, s->quest_index(c->version()), menu_type, Episode::NONE);
      break;
    }

    case MainMenuItemID::PATCH_SWITCHES: {
      if (!c->check_flag(Client::Flag::HAS_SEND_FUNCTION_CALL)) {
        throw runtime_error("client does not support send_function_call");
      }
      // We have to prepare the client for patches here, even though we
      // don't send them from this mennu, because we need to know the
      // client's specific_version before sending the menu.
      co_await prepare_client_for_patches(c);
      send_menu(c, c->require_server_state()->function_code_index->patch_switches_menu(c->specific_version, c->login->account->auto_patches_enabled));
      break;
    }

    case MainMenuItemID::PROGRAMS: {
      if (!c->check_flag(Client::Flag::HAS_SEND_FUNCTION_CALL)) {
        throw runtime_error("client does not support send_function_call");
      }
      co_await prepare_client_for_patches(c);
      send_menu(c, c->require_server_state()->dol_file_index->menu);
      break;
    }

    case MainMenuItemID::DISCONNECT:
      if (c->version() == Version::XB_V3) {
        // On XB (at least via Insignia) the server has to explicitly tell
        // the client to disconnect by sending this command.
        send_command(c, 0x05, 0x00);
      }
      c->channel->disconnect();
      break;

    case MainMenuItemID::CLEAR_LICENSE: {
      auto conf_menu = make_shared<Menu>(MenuID::CLEAR_LICENSE_CONFIRMATION, s->name);
      conf_menu->items.emplace_back(ClearLicenseConfirmationMenuItemID::CANCEL, "Go back",
          "Go back to the\nmain menu", 0);
      conf_menu->items.emplace_back(ClearLicenseConfirmationMenuItemID::CLEAR_LICENSE, "Clear license",
          "Disconnect with an\ninvalid license error\nso you can enter a\ndifferent serial\nnumber, access key,\nor password",
          MenuItem::Flag::INVISIBLE_ON_DC_PROTOS | MenuItem::Flag::INVISIBLE_ON_PC_NTE | MenuItem::Flag::INVISIBLE_ON_XB | MenuItem::Flag::INVISIBLE_ON_BB);

      send_menu(c, conf_menu);
      send_ship_info(c, "Are you sure?");
      break;
    }

    default:
      send_message_box(c, "Incorrect menu item ID.");
      break;
  }
}

static asio::awaitable<void> on_10_clear_license_confirmation(shared_ptr<Client> c, uint32_t item_id) {
  switch (item_id) {
    case ClearLicenseConfirmationMenuItemID::CANCEL:
      send_main_menu(c);
      break;
    case ClearLicenseConfirmationMenuItemID::CLEAR_LICENSE:
      send_command(c, 0x9A, 0x04);
      c->channel->disconnect();
  }
  co_return;
}

static asio::awaitable<void> on_10_information(shared_ptr<Client> c, uint32_t item_id) {
  if (item_id == InformationMenuItemID::GO_BACK) {
    c->clear_flag(Client::Flag::IN_INFORMATION_MENU);
    send_main_menu(c);
  } else {
    try {
      auto contents = c->require_server_state()->information_contents_for_client(c);
      send_message_box(c, contents->at(item_id));
    } catch (const out_of_range&) {
      send_message_box(c, "$C6No such information exists.");
    }
  }
  co_return;
}

static asio::awaitable<void> on_10_proxy_options(shared_ptr<Client> c, uint32_t item_id) {
  switch (item_id) {
    case ProxyOptionsMenuItemID::GO_BACK:
      send_proxy_destinations_menu(c);
      co_return;
    case ProxyOptionsMenuItemID::CHAT_COMMANDS:
      if (c->can_use_chat_commands()) {
        c->toggle_flag(Client::Flag::PROXY_CHAT_COMMANDS_ENABLED);
      } else {
        c->clear_flag(Client::Flag::PROXY_CHAT_COMMANDS_ENABLED);
      }
      break;
    case ProxyOptionsMenuItemID::PLAYER_NOTIFICATIONS:
      c->toggle_flag(Client::Flag::PROXY_PLAYER_NOTIFICATIONS_ENABLED);
      break;
    case ProxyOptionsMenuItemID::DROP_NOTIFICATIONS:
      switch (c->get_drop_notification_mode()) {
        case Client::ItemDropNotificationMode::NOTHING:
          c->set_drop_notification_mode(Client::ItemDropNotificationMode::RARES_ONLY);
          break;
        case Client::ItemDropNotificationMode::RARES_ONLY:
          c->set_drop_notification_mode(Client::ItemDropNotificationMode::ALL_ITEMS);
          break;
        case Client::ItemDropNotificationMode::ALL_ITEMS:
          c->set_drop_notification_mode(Client::ItemDropNotificationMode::ALL_ITEMS_INCLUDING_MESETA);
          break;
        case Client::ItemDropNotificationMode::ALL_ITEMS_INCLUDING_MESETA:
          c->set_drop_notification_mode(Client::ItemDropNotificationMode::NOTHING);
          break;
      }
      break;
    case ProxyOptionsMenuItemID::INFINITE_HP:
      c->toggle_flag(Client::Flag::INFINITE_HP_ENABLED);
      break;
    case ProxyOptionsMenuItemID::INFINITE_TP:
      c->toggle_flag(Client::Flag::INFINITE_TP_ENABLED);
      break;
    case ProxyOptionsMenuItemID::SWITCH_ASSIST:
      c->toggle_flag(Client::Flag::SWITCH_ASSIST_ENABLED);
      break;
    case ProxyOptionsMenuItemID::EP3_INFINITE_MESETA:
      c->toggle_flag(Client::Flag::PROXY_EP3_INFINITE_MESETA_ENABLED);
      break;
    case ProxyOptionsMenuItemID::EP3_INFINITE_TIME:
      c->toggle_flag(Client::Flag::PROXY_EP3_INFINITE_TIME_ENABLED);
      break;
    case ProxyOptionsMenuItemID::EP3_UNMASK_WHISPERS:
      c->toggle_flag(Client::Flag::PROXY_EP3_UNMASK_WHISPERS);
      break;
    case ProxyOptionsMenuItemID::BLOCK_EVENTS:
      c->override_lobby_event = (c->override_lobby_event == 0xFF) ? 0x00 : 0xFF;
      break;
    case ProxyOptionsMenuItemID::BLOCK_PATCHES:
      c->toggle_flag(Client::Flag::PROXY_BLOCK_FUNCTION_CALLS);
      break;
    case ProxyOptionsMenuItemID::SAVE_FILES:
      c->toggle_flag(Client::Flag::PROXY_SAVE_FILES);
      break;
    default:
      send_message_box(c, "Incorrect menu item ID.");
      co_return;
  }
  send_menu(c, proxy_options_menu_for_client(c));
}

static asio::awaitable<void> on_10_proxy_destinations(shared_ptr<Client> c, uint32_t item_id) {
  if (item_id == ProxyDestinationsMenuItemID::GO_BACK) {
    send_main_menu(c);

  } else if (item_id == ProxyDestinationsMenuItemID::OPTIONS) {
    send_menu(c, proxy_options_menu_for_client(c));

  } else {
    auto s = c->require_server_state();
    const pair<string, uint16_t>* dest = nullptr;
    try {
      dest = &s->proxy_destinations(c->version()).at(item_id);
    } catch (const out_of_range&) {
    }

    if (!dest) {
      send_message_box(c, "$C6No such destination exists.");
      c->channel->disconnect();
    } else {
      // Clear Check Tactics menu so client won't see newserv tournament
      // state while logically on another server. There is no such command
      // on Trial Edition though, so only do this on Ep3 final.
      if (c->version() == Version::GC_EP3) {
        send_ep3_confirm_tournament_entry(c, nullptr);
      }

      co_await enable_save_if_needed(c);
      co_await start_proxy_session(c, dest->first, dest->second, false);
    }
  }
}

static asio::awaitable<void> on_10_game_menu(shared_ptr<Client> c, uint32_t item_id, const std::string& password) {
  auto s = c->require_server_state();
  auto game = s->find_lobby(item_id);
  if (!game) {
    send_lobby_message_box(c, "$C7You cannot join this\ngame because it no\nlonger exists.");
    co_return;
  }
  switch (game->join_error_for_client(c, &password)) {
    case Lobby::JoinError::ALLOWED:
      if (!s->change_client_lobby(c, game)) {
        throw logic_error("client cannot join game after all preconditions satisfied");
      }
      if (game->is_game()) {
        c->set_flag(Client::Flag::LOADING);
        c->log.info_f("LOADING flag set");

        // If no one was in the game before, then there's no leader to send
        // the game state - send it to the joining player (who is now the
        // leader)
        if (game->count_clients() == 1) {
          c->set_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_ITEM_STATE);
          c->set_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_ENEMY_AND_SET_STATE);
          c->set_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_OBJECT_STATE);
          c->set_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_FLAG_STATE);
        }
      }
      break;
    case Lobby::JoinError::FULL:
      send_lobby_message_box(c, "$C7You cannot join this\ngame because it is\nfull.");
      break;
    case Lobby::JoinError::VERSION_CONFLICT:
      send_lobby_message_box(c, "$C7You cannot join this\ngame because it is\nfor a different\nversion of PSO.");
      break;
    case Lobby::JoinError::QUEST_SELECTION_IN_PROGRESS:
      send_lobby_message_box(c, "$C7You cannot join this\ngame because the\nplayers are currently\nchoosing a quest.");
      break;
    case Lobby::JoinError::QUEST_IN_PROGRESS:
      send_lobby_message_box(c, "$C7You cannot join this\ngame because a\nquest is already\nin progress.");
      break;
    case Lobby::JoinError::BATTLE_IN_PROGRESS:
      send_lobby_message_box(c, "$C7You cannot join this\ngame because a\nbattle is already\nin progress.");
      break;
    case Lobby::JoinError::LOADING:
      send_lobby_message_box(c, "$C7You cannot join this\ngame because\nanother player is\ncurrently loading.\nTry again soon.");
      break;
    case Lobby::JoinError::SOLO:
      send_lobby_message_box(c, "$C7You cannot join this\ngame because it is\na Solo Mode game.");
      break;
    case Lobby::JoinError::INCORRECT_PASSWORD:
      send_lobby_message_box(c, "$C7Incorrect password.");
      break;
    case Lobby::JoinError::LEVEL_TOO_LOW: {
      string msg = std::format("$C7You must be level\n{} or above to\njoin this game.",
          static_cast<size_t>(game->min_level + 1));
      send_lobby_message_box(c, msg);
      break;
    }
    case Lobby::JoinError::LEVEL_TOO_HIGH: {
      string msg = std::format("$C7You must be level\n{} or below to\njoin this game.",
          static_cast<size_t>(game->max_level + 1));
      send_lobby_message_box(c, msg);
      break;
    }
    case Lobby::JoinError::NO_ACCESS_TO_QUEST:
      send_lobby_message_box(c, "$C7You don't have access\nto the quest in progress\nin this game, or there\nis no space for another\nplayer in the quest.");
      break;
    default:
      send_lobby_message_box(c, "$C7You cannot join this\ngame.");
      break;
  }
}

static asio::awaitable<void> on_10_quest_categories(shared_ptr<Client> c, uint32_t item_id) {
  auto s = c->require_server_state();
  auto quest_index = s->quest_index(c->version());
  if (!quest_index) {
    send_lobby_message_box(c, "$C7Quests are not available.");
    co_return;
  }

  shared_ptr<Lobby> l = c->lobby.lock();
  Episode episode = l ? l->episode : Episode::NONE;
  uint16_t version_flags = (1 << static_cast<size_t>(c->version())) | (l ? l->quest_version_flags() : 0);
  QuestIndex::IncludeCondition include_condition = nullptr;
  if (l && !c->login->account->check_flag(Account::Flag::DISABLE_QUEST_REQUIREMENTS)) {
    include_condition = l->quest_include_condition();
  }

  const auto& quests = quest_index->filter(episode, version_flags, item_id, include_condition);
  send_quest_menu(c, quests, !l);
}

static asio::awaitable<void> on_10_quest_menu(shared_ptr<Client> c, uint32_t item_id) {
  auto s = c->require_server_state();
  auto quest_index = s->quest_index(c->version());
  if (!quest_index) {
    send_lobby_message_box(c, "$C7Quests are not\navailable.");
    co_return;
  }
  auto q = quest_index->get(item_id);
  if (!q) {
    send_lobby_message_box(c, "$C7Quest does not exist.");
    co_return;
  }

  // If the client is not in a lobby, send the quest as a download quest.
  // Otherwise, they must be in a game to load a quest.
  auto l = c->lobby.lock();
  if (l && !l->is_game()) {
    send_lobby_message_box(c, "$C7Quests cannot be\nloaded in lobbies.");
    co_return;
  }

  if (l) {
    if (q->episode == Episode::EP3) {
      send_lobby_message_box(c, "$C7Episode 3 quests\ncannot be loaded\nvia this interface.");
      co_return;
    }
    if (l->quest) {
      send_lobby_message_box(c, "$C7A quest is already\nin progress.");
      co_return;
    }
    if (l->quest_include_condition()(q) != QuestIndex::IncludeState::AVAILABLE) {
      send_lobby_message_box(c, "$C7This quest has not\nbeen unlocked for\nall players in this\ngame.");
      co_return;
    }
    set_lobby_quest(l, q);

  } else {
    auto vq = q->version(c->version(), c->language());
    if (!vq) {
      send_lobby_message_box(c, "$C7Quest does not exist\nfor this game version.");
      co_return;
    }
    // Episode 3 uses the download quest commands (A6/A7) but does not
    // expect the server to have already encrypted the quest files, unlike
    // other versions.
    // TODO: This is not true for Episode 3 Trial Edition. We also would
    // have to convert the map to a MapDefinitionTrial, though.
    if (is_ep3(vq->version)) {
      send_open_quest_file(c, q->name, vq->bin_filename(), "", vq->quest_number, QuestFileType::EPISODE_3, vq->bin_contents);
    } else {
      vq = vq->create_download_quest(c->language());
      string xb_filename = vq->xb_filename();
      QuestFileType type = vq->pvr_contents ? QuestFileType::DOWNLOAD_WITH_PVR : QuestFileType::DOWNLOAD_WITHOUT_PVR;
      send_open_quest_file(c, q->name, vq->bin_filename(), xb_filename, vq->quest_number, type, vq->bin_contents);
      send_open_quest_file(c, q->name, vq->dat_filename(), xb_filename, vq->quest_number, type, vq->dat_contents);
      if (vq->pvr_contents) {
        send_open_quest_file(c, q->name, vq->pvr_filename(), xb_filename, vq->quest_number, type, vq->pvr_contents);
      }
    }
  }
}

static asio::awaitable<void> on_10_patch_switches(shared_ptr<Client> c, uint32_t item_id) {
  if (item_id == PatchesMenuItemID::GO_BACK) {
    send_main_menu(c);

  } else {
    if (!c->check_flag(Client::Flag::HAS_SEND_FUNCTION_CALL)) {
      throw runtime_error("client does not support send_function_call");
    }

    auto s = c->require_server_state();
    uint64_t key = (static_cast<uint64_t>(item_id) << 32) | c->specific_version;
    auto fn = s->function_code_index->menu_item_id_and_specific_version_to_patch_function.at(key);
    if (!c->login->account->auto_patches_enabled.emplace(fn->short_name).second) {
      c->login->account->auto_patches_enabled.erase(fn->short_name);
    }
    c->login->account->save();
    send_menu(c, s->function_code_index->patch_switches_menu(c->specific_version, c->login->account->auto_patches_enabled));
  }
  co_return;
}

static asio::awaitable<void> on_10_programs(shared_ptr<Client> c, uint32_t item_id) {
  if (item_id == ProgramsMenuItemID::GO_BACK) {
    send_main_menu(c);

  } else {
    if (!c->check_flag(Client::Flag::HAS_SEND_FUNCTION_CALL)) {
      throw runtime_error("client does not support send_function_call");
    }

    auto s = c->require_server_state();
    auto dol = s->dol_file_index->item_id_to_file.at(item_id);
    co_await send_dol_file(c, dol); // Disconnects the client
  }
}

static asio::awaitable<void> on_10_tournaments(shared_ptr<Client> c, uint32_t menu_id, uint32_t item_id) {
  if (!is_ep3(c->version())) {
    throw runtime_error("non-Episode 3 client attempted to join tournament");
  }
  auto s = c->require_server_state();
  auto tourn = s->ep3_tournament_index->get_tournament(item_id);
  if (tourn) {
    send_ep3_tournament_entry_list(c, tourn, (menu_id == MenuID::TOURNAMENTS_FOR_SPEC));
  }
  co_return;
}

static asio::awaitable<void> on_10_tournament_entries(
    shared_ptr<Client> c, uint32_t item_id, std::string&& team_name, std::string&& password) {
  if (!is_ep3(c->version())) {
    throw runtime_error("non-Episode 3 client attempted to join tournament");
  }
  if (c->ep3_tournament_team.lock()) {
    send_lobby_message_box(c, "$C7You are registered\nin a different\ntournament already");
    co_return;
  }
  if (team_name.empty()) {
    team_name = c->character()->disp.name.decode(c->language());
    team_name += std::format("/{:X}", c->login->account->account_id);
  }
  uint16_t tourn_num = item_id >> 16;
  uint16_t team_index = item_id & 0xFFFF;
  auto s = c->require_server_state();
  auto tourn = s->ep3_tournament_index->get_tournament(tourn_num);
  if (tourn) {
    auto team = tourn->get_team(team_index);
    if (team) {
      try {
        team->register_player(c, team_name, password);
        c->ep3_tournament_team = team;
        tourn->send_all_state_updates();
        string message = std::format("$C7You are registered in $C6{}$C7.\n\
\n\
After the tournament begins, start your matches\n\
by standing at any Battle Table along with your\n\
partner (if any) and opponent(s).",
            tourn->get_name());
        send_ep3_timed_message_box(c->channel, 240, message);

        s->ep3_tournament_index->save();

      } catch (const exception& e) {
        string message = std::format("Cannot join team:\n{}", e.what());
        send_lobby_message_box(c, message);
      }
    } else {
      send_lobby_message_box(c, "Team does not exist");
    }
  } else {
    send_lobby_message_box(c, "Tournament does\nnot exist");
  }
}

static asio::awaitable<void> on_10(shared_ptr<Client> c, Channel::Message& msg) {
  bool uses_utf16 = ::uses_utf16(c->version());

  uint32_t menu_id;
  uint32_t item_id;
  string team_name;
  string password;

  if (msg.data.size() > sizeof(C_MenuSelection_10_Flag00)) {
    if (uses_utf16) {
      // TODO: We can support the Flag03 variant here, but PC/BB probably never
      // actually use it.
      const auto& cmd = check_size_t<C_MenuSelection_PC_BB_10_Flag02>(msg.data);
      password = cmd.password.decode(c->language());
      menu_id = cmd.basic_cmd.menu_id;
      item_id = cmd.basic_cmd.item_id;
    } else if (msg.data.size() > sizeof(C_MenuSelection_DC_V3_10_Flag02)) {
      const auto& cmd = check_size_t<C_MenuSelection_DC_V3_10_Flag03>(msg.data);
      team_name = cmd.name.decode(c->language());
      password = cmd.password.decode(c->language());
      menu_id = cmd.basic_cmd.menu_id;
      item_id = cmd.basic_cmd.item_id;
    } else {
      const auto& cmd = check_size_t<C_MenuSelection_DC_V3_10_Flag02>(msg.data);
      password = cmd.password.decode(c->language());
      menu_id = cmd.basic_cmd.menu_id;
      item_id = cmd.basic_cmd.item_id;
    }
  } else {
    const auto& cmd = check_size_t<C_MenuSelection_10_Flag00>(msg.data);
    menu_id = cmd.menu_id;
    item_id = cmd.item_id;
  }

  auto s = c->require_server_state();
  switch (menu_id) {
    case MenuID::MAIN:
      co_await on_10_main_menu(c, item_id);
      break;
    case MenuID::CLEAR_LICENSE_CONFIRMATION:
      co_await on_10_clear_license_confirmation(c, item_id);
      break;
    case MenuID::INFORMATION:
      co_await on_10_information(c, item_id);
      break;
    case MenuID::PROXY_OPTIONS:
      co_await on_10_proxy_options(c, item_id);
      break;
    case MenuID::PROXY_DESTINATIONS:
      co_await on_10_proxy_destinations(c, item_id);
      break;
    case MenuID::GAME:
      co_await on_10_game_menu(c, item_id, std::move(password));
      break;
    case MenuID::QUEST_CATEGORIES_EP1:
    case MenuID::QUEST_CATEGORIES_EP2:
      co_await on_10_quest_categories(c, item_id);
      break;
    case MenuID::QUEST_EP1:
    case MenuID::QUEST_EP2:
      co_await on_10_quest_menu(c, item_id);
      break;
    case MenuID::PATCH_SWITCHES:
      co_await on_10_patch_switches(c, item_id);
      break;
    case MenuID::PROGRAMS:
      co_await on_10_programs(c, item_id);
      break;
    case MenuID::TOURNAMENTS_FOR_SPEC:
    case MenuID::TOURNAMENTS:
      co_await on_10_tournaments(c, menu_id, item_id);
      break;
    case MenuID::TOURNAMENT_ENTRIES:
      co_await on_10_tournament_entries(c, item_id, std::move(team_name), std::move(password));
      break;
    default:
      send_message_box(c, "Incorrect menu ID");
      break;
  }
}

static asio::awaitable<void> on_84(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& cmd = check_size_t<C_LobbySelection_84>(msg.data);
  auto s = c->require_server_state();

  if (c->check_flag(Client::Flag::AWAITING_ENABLE_B2_QUEST)) {
    co_await start_login_server_procedure(c);
    c->clear_flag(Client::Flag::AWAITING_ENABLE_B2_QUEST);
    co_return;
  }

  if (cmd.menu_id != MenuID::LOBBY) {
    send_message_box(c, "Incorrect menu ID");
    co_return;
  }

  // If the client isn't in any lobby, then they just left a game. Add them to
  // the lobby they requested, but fall back to another lobby if it's full.
  if (!c->lobby.lock()) {
    c->preferred_lobby_id = cmd.item_id;
    s->add_client_to_available_lobby(c);

    // If the client already is in a lobby, then they're using the lobby
    // teleporter; add them to the lobby they requested or send a failure message.
  } else {
    auto new_lobby = s->find_lobby(cmd.item_id);
    if (!new_lobby) {
      send_lobby_message_box(c, "$C6Can't change lobby\n\n$C7The lobby does not\nexist.");
      co_return;
    }

    if (new_lobby->is_game()) {
      send_lobby_message_box(c, "$C6Can't change lobby\n\n$C7The specified lobby\nis a game.");
      co_return;
    }

    if (new_lobby->is_ep3() && !is_ep3(c->version())) {
      send_lobby_message_box(c, "$C6Can't change lobby\n\n$C7The lobby is for\nEpisode 3 only.");
      co_return;
    }

    if (!s->change_client_lobby(c, new_lobby)) {
      send_lobby_message_box(c, "$C6Can\'t change lobby\n\n$C7The lobby is full.");
    }
  }
}

static asio::awaitable<void> on_08_E6(shared_ptr<Client> c, Channel::Message& msg) {
  check_size_v(msg.data.size(), 0);
  send_game_menu(c, (msg.command == 0xE6), false);
  co_return;
}

static asio::awaitable<void> on_1F(shared_ptr<Client> c, Channel::Message& msg) {
  check_size_v(msg.data.size(), 0);
  auto s = c->require_server_state();
  send_menu(c, s->information_menu(c->version()), true);
  co_return;
}

static asio::awaitable<void> on_A0(shared_ptr<Client> c, Channel::Message&) {
  // The client sends data in this command, but none of it is important. We
  // intentionally don't call check_size here, but just ignore the data.

  // Delete the player from the lobby they're in (but only visible to themself).
  // This makes it safe to allow the player to choose download quests from the
  // main menu again - if we didn't do this, they could move in the lobby after
  // canceling the download quests menu, which looks really bad.
  send_self_leave_notification(c);

  // Sending a blank message box here works around the bug where the log window
  // contents appear prepended to the next large message box. But, we don't have
  // to do this if we're not going to show the welcome message or information
  // menu (that is, if the client will not send a close confirmation).
  if (!c->check_flag(Client::Flag::NO_D6)) {
    send_message_box(c, "");
  }

  return start_login_server_procedure(c);
}

static asio::awaitable<void> on_A1(shared_ptr<Client> c, Channel::Message& msg) {
  // newserv doesn't have blocks; treat block change the same as ship change
  return on_A0(c, msg);
}

static asio::awaitable<void> on_8E_DCNTE(shared_ptr<Client> c, Channel::Message& msg) {
  if (c->version() == Version::DC_NTE) {
    return on_A0(c, msg);
  } else {
    throw runtime_error("non-DCNTE client sent 8E");
  }
}

static asio::awaitable<void> on_8F_DCNTE(shared_ptr<Client> c, Channel::Message& msg) {
  if (c->version() == Version::DC_NTE) {
    return on_A1(c, msg);
  } else {
    throw runtime_error("non-DCNTE client sent 8F");
  }
}

static asio::awaitable<void> on_B3(shared_ptr<Client> c, Channel::Message& msg) {
  auto& cmd = check_size_t<C_ExecuteCodeResult_B3>(msg.data);
  if (!c->function_call_response_queue.empty()) {
    auto& prom = c->function_call_response_queue.front();
    if (prom) {
      prom->set_value(std::move(cmd));
    }
    c->function_call_response_queue.pop_front();
  } else {
    c->log.warning_f("Received function call response but response queue is empty");
  }
  co_return;
}

static asio::awaitable<void> on_A2(shared_ptr<Client> c, Channel::Message& msg) {
  check_size_v(msg.data.size(), 0);
  auto s = c->require_server_state();

  auto l = c->lobby.lock();
  if (!l || !l->is_game()) {
    send_lobby_message_box(c, "$C7Quests are not available\nin lobbies.");
    co_return;
  }

  if (is_ep3(c->version())) {
    send_lobby_message_box(c, "$C7Episode 3 does not\nprovide online quests\nvia this interface.");
    co_return;
  }

  QuestMenuType menu_type;
  if ((c->version() == Version::BB_V4) && msg.flag) {
    menu_type = QuestMenuType::GOVERNMENT;
  } else {
    switch (l->mode) {
      case GameMode::NORMAL:
        menu_type = QuestMenuType::NORMAL;
        break;
      case GameMode::BATTLE:
        menu_type = QuestMenuType::BATTLE;
        break;
      case GameMode::CHALLENGE:
        menu_type = QuestMenuType::CHALLENGE;
        break;
      case GameMode::SOLO:
        menu_type = QuestMenuType::SOLO;
        break;
      default:
        throw logic_error("invalid game mode");
    }
  }

  send_quest_categories_menu(c, s->quest_index(c->version()), menu_type, l->episode);
  l->set_flag(Lobby::Flag::QUEST_SELECTION_IN_PROGRESS);
}

static asio::awaitable<void> on_A9(shared_ptr<Client> c, Channel::Message&) {
  auto l = c->require_lobby();
  if (l->is_game() && (c->lobby_client_id == l->leader_id)) {
    l->clear_flag(Lobby::Flag::QUEST_SELECTION_IN_PROGRESS);
  }
  co_return;
}

static void on_joinable_quest_loaded(shared_ptr<Client> c) {
  auto l = c->require_lobby();
  if (!l->is_game() || !l->quest) {
    throw runtime_error("joinable quest load completed in non-game");
  }
  auto leader_c = l->clients.at(l->leader_id);
  if (!leader_c) {
    throw logic_error("lobby leader is missing");
  }

  // On BB, ask the leader to send the quest state to the joining player (and
  // we'll need to use the game join command queue to avoid any item ID races).
  // On other versions, the server will have to generate the state commands;
  // this happens when the response to the ping (1D) is received, so we don't
  // need the game join command queue in that case.
  if (leader_c->version() == Version::BB_V4) {
    send_command(leader_c, 0xDD, c->lobby_client_id);
    c->log.info_f("Creating game join command queue");
    c->game_join_command_queue = make_unique<deque<Client::JoinCommand>>();
  } else {
    c->set_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_ITEM_STATE);
    c->set_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_ENEMY_AND_SET_STATE);
    c->set_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_OBJECT_STATE);
    c->set_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_FLAG_STATE);
    c->set_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_PLAYER_STATES);
  }
  send_command(c, 0x1D, 0x00);

  if (!is_v1_or_v2(c->version())) {
    send_command(c, 0xAC, 0x00);
  }
}

static asio::awaitable<void> on_AC_V3_BB(shared_ptr<Client> c, Channel::Message& msg) {
  check_size_v(msg.data.size(), 0);

  if (c->check_flag(Client::Flag::LOADING_RUNNING_JOINABLE_QUEST)) {
    on_joinable_quest_loaded(c);

  } else if (c->check_flag(Client::Flag::LOADING_QUEST)) {
    c->clear_flag(Client::Flag::LOADING_QUEST);
    c->log.info_f("LOADING_QUEST flag cleared");
    auto l = c->require_lobby();
    if (l->quest && send_quest_barrier_if_all_clients_ready(l)) {
      on_quest_loaded(l);
    }
  }
  co_return;
}

static asio::awaitable<void> on_AA(shared_ptr<Client> c, Channel::Message& msg) {
  if (is_v1_or_v2(c->version())) {
    throw runtime_error("pre-V3 client sent update quest stats command");
  }

  const auto& cmd = check_size_t<C_SendQuestStatistic_V3_BB_AA>(msg.data);
  auto l = c->require_lobby();
  if (l->is_game() && l->quest.get()) {
    // TODO: Send the right value here. (When should we send label2?)
    send_quest_function_call(c, cmd.label1);
  }
  co_return;
}

static asio::awaitable<void> on_D7_GC(shared_ptr<Client> c, Channel::Message& msg) {
  string filename(msg.data);
  phosg::strip_trailing_zeroes(filename);
  if (filename.find('/') != string::npos) {
    send_command(c, 0xD7, 0x00);
  } else {
    try {
      auto s = c->require_server_state();
      auto f = s->gba_files_cache->get_or_load("system/gba/" + filename).file;
      send_open_quest_file(c, "", filename, "", 0, QuestFileType::GBA_DEMO, f->data);
    } catch (const out_of_range&) {
      send_command(c, 0xD7, 0x00);
    } catch (const phosg::cannot_open_file&) {
      send_command(c, 0xD7, 0x00);
    }
  }
  co_return;
}

static asio::awaitable<void> on_13_A7_V3_V4(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& cmd = check_size_t<C_WriteFileConfirmation_V3_BB_13_A7>(msg.data);
  bool is_download_quest = (msg.command == 0xA7);
  string filename = cmd.filename.decode();
  size_t chunk_to_send = msg.flag + V3_V4_QUEST_LOAD_MAX_CHUNKS_IN_FLIGHT;

  shared_ptr<const string> file_data;
  try {
    file_data = c->sending_files.at(filename);
  } catch (const out_of_range&) {
  }

  if (!file_data) {
    co_return;
  }

  size_t chunk_offset = chunk_to_send * 0x400;
  if (chunk_offset >= file_data->size()) {
    c->log.info_f("Done sending file {}", filename);
    c->sending_files.erase(filename);
  } else {
    const void* chunk_data = file_data->data() + (chunk_to_send * 0x400);
    size_t chunk_size = min<size_t>(file_data->size() - chunk_offset, 0x400);
    send_quest_file_chunk(c, filename, chunk_to_send, chunk_data, chunk_size, is_download_quest);
  }
}

static asio::awaitable<void> on_61_98(shared_ptr<Client> c, Channel::Message& msg) {
  auto s = c->require_server_state();

  // 98 should only be sent when leaving a game, and we should leave the client
  // in no lobby (they will send an 84 soon afterward to choose a lobby).
  if (msg.command == 0x98) {
    // Clear all temporary state from the game
    c->delete_overlay();
    c->telepipe_lobby_id = 0;
    s->remove_client_from_lobby(c);
    c->clear_flag(Client::Flag::LOADING);
    c->clear_flag(Client::Flag::LOADING_QUEST);
    c->clear_flag(Client::Flag::LOADING_RUNNING_JOINABLE_QUEST);
    c->clear_flag(Client::Flag::LOADING_TOURNAMENT);
    c->clear_flag(Client::Flag::AT_BANK_COUNTER);
    c->clear_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_ITEM_STATE);
    c->clear_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_ENEMY_AND_SET_STATE);
    c->clear_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_OBJECT_STATE);
    c->clear_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_FLAG_STATE);
    c->clear_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_PLAYER_STATES);
  }

  auto player = c->character();

  switch (c->version()) {
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1: {
      const auto& cmd = check_size_t<C_CharacterData_DCv1_61_98>(msg.data);
      c->v1_v2_last_reported_disp = make_unique<PlayerDispDataDCPCV3>(cmd.disp);
      player->inventory = cmd.inventory;
      player->disp = cmd.disp.to_bb(player->inventory.language, player->inventory.language);
      c->login->account->last_player_name = player->disp.name.decode(player->inventory.language);
      break;
    }
    case Version::DC_V2: {
      const auto& cmd = check_size_t<C_CharacterData_DCv2_61_98>(msg.data, 0xFFFF);
      c->v1_v2_last_reported_disp = make_unique<PlayerDispDataDCPCV3>(cmd.disp);
      player->inventory = cmd.inventory;
      player->disp = cmd.disp.to_bb(player->inventory.language, player->inventory.language);
      player->battle_records = cmd.records.battle;
      player->challenge_records = cmd.records.challenge;
      player->choice_search_config = cmd.choice_search_config;
      c->login->account->last_player_name = player->disp.name.decode(player->inventory.language);
      break;
    }
    case Version::PC_NTE:
    case Version::PC_V2: {
      const auto& cmd = check_size_t<C_CharacterData_PC_61_98>(msg.data, 0xFFFF);
      c->v1_v2_last_reported_disp = make_unique<PlayerDispDataDCPCV3>(cmd.disp);
      player->inventory = cmd.inventory;
      player->disp = cmd.disp.to_bb(player->inventory.language, player->inventory.language);
      player->battle_records = cmd.records.battle;
      player->challenge_records = cmd.records.challenge;
      player->choice_search_config = cmd.choice_search_config;
      c->import_blocked_senders(cmd.blocked_senders);
      if (cmd.auto_reply_enabled) {
        string auto_reply = msg.data.substr(sizeof(cmd));
        phosg::strip_trailing_zeroes(auto_reply);
        if (auto_reply.size() & 1) {
          auto_reply.push_back(0);
        }
        try {
          player->auto_reply.encode(tt_utf16_to_utf8(auto_reply), player->inventory.language);
        } catch (const runtime_error& e) {
          c->log.warning_f("Failed to decode auto-reply message: {}", e.what());
        }
        c->login->account->auto_reply_message = auto_reply;
      } else {
        player->auto_reply.clear();
        c->login->account->auto_reply_message.clear();
      }
      c->login->account->last_player_name = player->disp.name.decode(player->inventory.language);
      break;
    }
    case Version::GC_NTE: {
      const auto& cmd = check_size_t<C_CharacterData_GCNTE_61_98>(msg.data, 0xFFFF);
      c->v1_v2_last_reported_disp = make_unique<PlayerDispDataDCPCV3>(cmd.disp);

      auto s = c->require_server_state();
      player->inventory = cmd.inventory;
      player->disp = cmd.disp.to_bb(player->inventory.language, player->inventory.language);
      player->battle_records = cmd.records.battle;
      player->challenge_records = cmd.records.challenge;
      player->choice_search_config = cmd.choice_search_config;
      c->import_blocked_senders(cmd.blocked_senders);
      if (cmd.auto_reply_enabled) {
        string auto_reply = msg.data.substr(sizeof(cmd), 0xAC);
        phosg::strip_trailing_zeroes(auto_reply);
        try {
          string encoded = tt_decode_marked(auto_reply, player->inventory.language, false);
          player->auto_reply.encode(encoded, player->inventory.language);
        } catch (const runtime_error& e) {
          c->log.warning_f("Failed to decode auto-reply message: {}", e.what());
        }
        c->login->account->auto_reply_message = auto_reply;
      } else {
        player->auto_reply.clear();
        c->login->account->auto_reply_message.clear();
      }
      c->login->account->last_player_name = player->disp.name.decode(player->inventory.language);
      break;
    }

    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
    case Version::XB_V3: {
      const C_CharacterData_V3_61_98* cmd;
      if (msg.flag == 4) { // Episode 3
        if (!is_ep3(c->version())) {
          throw runtime_error("non-Episode 3 client sent Episode 3 player data");
        }
        const auto* cmd3 = &check_size_t<C_CharacterData_Ep3_61_98>(msg.data);
        c->ep3_config = make_shared<Episode3::PlayerConfig>(cmd3->ep3_config);
        cmd = reinterpret_cast<const C_CharacterData_V3_61_98*>(cmd3);
        if (specific_version_is_indeterminate(c->specific_version)) {
          c->specific_version = SPECIFIC_VERSION_GC_EP3_JP; // 3SJ0
        }
      } else {
        if (is_ep3(c->version())) {
          c->channel->version = Version::GC_EP3_NTE;
          c->log.info_f("Game version changed to GC_EP3_NTE");
          c->clear_flag(Client::Flag::ENCRYPTED_SEND_FUNCTION_CALL);
          if (specific_version_is_indeterminate(c->specific_version)) {
            c->specific_version = SPECIFIC_VERSION_GC_EP3_NTE;
          }
          c->convert_account_to_temporary_if_nte();
        }
        cmd = &check_size_t<C_CharacterData_V3_61_98>(msg.data, 0xFFFF);
      }

      player->inventory = cmd->inventory;
      player->disp = cmd->disp.to_bb(player->inventory.language, player->inventory.language);
      player->battle_records = cmd->records.battle;
      player->challenge_records = cmd->records.challenge;
      player->choice_search_config = cmd->choice_search_config;
      player->info_board.encode(cmd->info_board.decode(player->inventory.language), player->inventory.language);
      c->import_blocked_senders(cmd->blocked_senders);
      if (cmd->auto_reply_enabled) {
        string auto_reply = msg.data.substr(sizeof(cmd), 0xAC);
        phosg::strip_trailing_zeroes(auto_reply);
        try {
          string encoded = tt_decode_marked(auto_reply, player->inventory.language, false);
          player->auto_reply.encode(encoded, player->inventory.language);
        } catch (const runtime_error& e) {
          c->log.warning_f("Failed to decode auto-reply message: {}", e.what());
        }
        c->login->account->auto_reply_message = auto_reply;
      } else {
        player->auto_reply.clear();
        c->login->account->auto_reply_message.clear();
      }
      c->login->account->last_player_name = player->disp.name.decode(player->inventory.language);
      break;
    }
    case Version::BB_V4: {
      const auto& cmd = check_size_t<C_CharacterData_BB_61_98>(msg.data, 0xFFFF);
      // Note: we don't copy the inventory and disp here because we already have
      // them (we sent the player data to the client in the first place)
      player->battle_records = cmd.records.battle;
      player->challenge_records = cmd.records.challenge;
      player->choice_search_config = cmd.choice_search_config;
      player->info_board = cmd.info_board;
      c->import_blocked_senders(cmd.blocked_senders);
      if (cmd.auto_reply_enabled) {
        string auto_reply = msg.data.substr(sizeof(cmd), 0xAC);
        phosg::strip_trailing_zeroes(auto_reply);
        if (auto_reply.size() & 1) {
          auto_reply.push_back(0);
        }
        try {
          player->auto_reply.encode(tt_utf16_to_utf8(auto_reply), player->inventory.language);
        } catch (const runtime_error& e) {
          c->log.warning_f("Failed to decode auto-reply message: {}", e.what());
        }
        c->login->account->auto_reply_message = auto_reply;
      } else {
        player->auto_reply.clear();
        c->login->account->auto_reply_message.clear();
      }
      c->login->account->last_player_name = player->disp.name.decode(player->inventory.language);
      break;
    }
    default:
      throw logic_error("player data command not implemented for version");
  }
  player->inventory.decode_from_client(c->version());
  c->channel->language = player->inventory.language;
  c->login->account->save();

  c->update_channel_name();

  // If the player is BB and has just left a game, sync their save file to the
  // client to make sure it's up to date
  if ((c->version() == Version::BB_V4) && (msg.command == 0x98)) {
    send_complete_player_bb(c);
  }

  if (c->character_data_ready_promise) {
    c->character_data_ready_promise->set_value(GetPlayerInfoResult{
        .character = player,
        .ep3_character = nullptr,
        .is_full_info = false,
    });
    c->character_data_ready_promise.reset();
  }
  co_return;
}

static asio::awaitable<void> on_30(shared_ptr<Client> c, Channel::Message& msg) {
  if (!c->character_data_ready_promise) {
    co_return;
  }
  if (!c->channel->connected()) {
    throw runtime_error("Client has already disconnected");
  }

  shared_ptr<PSOBBCharacterFile> ch;
  shared_ptr<PSOGCEp3CharacterFile::Character> ep3_ch;
  if (is_ep3(c->version())) {
    ep3_ch = (c->version() == Version::GC_EP3_NTE)
        ? make_shared<PSOGCEp3CharacterFile::Character>(msg.check_size_t<PSOGCEp3NTECharacter>())
        : make_shared<PSOGCEp3CharacterFile::Character>(msg.check_size_t<PSOGCEp3CharacterFile::Character>());

  } else {
    switch (c->version()) {
      case Version::DC_V2:
        ch = PSOBBCharacterFile::create_from_file(msg.check_size_t<PSODCV2CharacterFile::Character>());
        break;
      case Version::GC_NTE:
        ch = PSOBBCharacterFile::create_from_file(msg.check_size_t<PSOGCNTECharacterFileCharacter>());
        break;
      case Version::GC_V3:
        ch = PSOBBCharacterFile::create_from_file(msg.check_size_t<PSOGCCharacterFile::Character>());
        break;
      case Version::XB_V3:
        ch = PSOBBCharacterFile::create_from_file(msg.check_size_t<PSOXBCharacterFileCharacter>());
        break;
      case Version::GC_EP3_NTE:
      case Version::GC_EP3:
        throw logic_error("Episode 3 case not handled correctly");
      case Version::DC_NTE:
      case Version::DC_11_2000:
      case Version::DC_V1:
      case Version::PC_NTE:
      case Version::PC_V2:
      case Version::BB_V4:
      default:
        throw logic_error("extended player data command not implemented for version");
    }
    ch->inventory.decode_from_client(c->version());
    ch->disp.visual.version = 4;
    ch->disp.visual.name_color_checksum = 0x00000000;
  }

  c->character_data_ready_promise->set_value(GetPlayerInfoResult{
      .character = std::move(ch),
      .ep3_character = std::move(ep3_ch),
      .is_full_info = true,
  });
  c->character_data_ready_promise.reset();
}

static asio::awaitable<void> on_6x_C9_CB(shared_ptr<Client> c, Channel::Message& msg) {
  check_size_v(msg.data.size(), 4, 0xFFFF);
  if ((msg.data.size() > 0x400) && (msg.command != 0x6C) && (msg.command != 0x6D)) {
    throw runtime_error("non-extended game command data size is too large");
  }
  co_await on_subcommand_multi(c, msg);
}

static asio::awaitable<void> on_06(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& cmd = check_size_t<SC_TextHeader_01_06_11_B0_EE>(msg.data, 0xFFFF);
  string text = msg.data.substr(sizeof(cmd));
  phosg::strip_trailing_zeroes(text);
  if (text.empty()) {
    co_return;
  }
  bool is_w = uses_utf16(c->version());
  if (is_w && (text.size() & 1)) {
    text.push_back(0);
  }

  auto l = c->lobby.lock();
  char private_flags = 0;
  if (is_ep3(c->version()) && l && l->is_ep3() && (text[0] != '\t')) {
    private_flags = text[0];
    text = text.substr(1);
  }

  try {
    text = tt_decode_marked(text, c->language(), is_w);
  } catch (const runtime_error& e) {
    c->log.warning_f("Failed to decode chat message: {}", e.what());
    send_text_message_fmt(c, "$C4Failed to decode\nchat message:\n{}", e.what());
    text.clear();
  }
  if (text.empty()) {
    co_return;
  }

  char command_sentinel = (c->version() == Version::DC_11_2000) ? '@' : '$';
  if ((text[0] == command_sentinel) && c->can_use_chat_commands()) {
    if (text[1] == command_sentinel) {
      text = text.substr(1);
    } else {
      co_await on_chat_command(c, text, true);
      co_return;
    }
  }

  if (!l || !c->can_chat) {
    co_return;
  }

  auto p = c->character();
  string from_name = p->disp.name.decode(c->language());
  static const string whisper_text = "(whisper)";
  for (size_t x = 0; x < l->max_clients; x++) {
    if (l->clients[x]) {
      bool should_hide_contents = (!(l->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM))) && (private_flags & (1 << x));
      const string& effective_text = should_hide_contents ? whisper_text : text;
      try {
        send_chat_message(l->clients[x], c->login->account->account_id, from_name, effective_text, private_flags);
      } catch (const runtime_error& e) {
        l->clients[x]->log.warning_f("Failed to encode chat message: {}", e.what());
      }
    }
  }
  for (const auto& watcher_l : l->watcher_lobbies) {
    for (size_t x = 0; x < watcher_l->max_clients; x++) {
      if (watcher_l->clients[x]) {
        try {
          send_chat_message(watcher_l->clients[x], c->login->account->account_id, from_name, text, private_flags);
        } catch (const runtime_error& e) {
          watcher_l->clients[x]->log.warning_f("Failed to encode chat message: {}", e.what());
        }
      }
    }
  }

  if (l->battle_record && l->battle_record->battle_in_progress()) {
    try {
      auto prepared_message = prepare_chat_data(
          c->version(),
          c->language(),
          c->lobby_client_id,
          p->disp.name.decode(c->language()),
          text,
          private_flags);
      l->battle_record->add_chat_message(c->login->account->account_id, std::move(prepared_message));
    } catch (const runtime_error& e) {
      l->log.warning_f("Failed to encode chat message for battle record: {}", e.what());
    }
  }
}

static asio::awaitable<void> on_E0_BB(shared_ptr<Client> c, Channel::Message& msg) {
  check_size_v(msg.data.size(), 0);
  send_system_file_bb(c);
  co_return;
}

static asio::awaitable<void> on_E3_BB(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& cmd = check_size_t<C_PlayerPreviewRequest_BB_E3>(msg.data);

  if (c->bb_connection_phase != 0x00) {
    c->save_and_unload_character();
    c->bb_character_index = cmd.character_index;
    send_approve_player_choice_bb(c);

  } else {
    if (!c->login) {
      c->channel->disconnect();
      co_return;
    }

    auto send_preview = [&c](size_t index) -> void {
      c->save_and_unload_character();
      c->bb_character_index = index;
      try {
        auto preview = c->character()->to_preview();
        send_player_preview_bb(c, c->bb_character_index, &preview);

      } catch (const exception& e) {
        // Player doesn't exist
        c->log.warning_f("Can\'t load character data: {}", e.what());
        send_player_preview_bb(c, c->bb_character_index, nullptr);
      }
    };

    if (msg.flag == 0) {
      if (cmd.character_index < 0 || cmd.character_index >= 0x80) {
        throw runtime_error("client requested invalid character slot");
      }
      send_preview(cmd.character_index);
    } else if (cmd.character_index == 0) {
      if (msg.flag >= 0x80) {
        throw runtime_error("client requested too many character slots");
      }
      auto s = c->require_server_state();
      for (size_t z = 0; z < msg.flag; z++) {
        send_preview(z);
      }
    }
  }
}

static asio::awaitable<void> on_E8_BB(shared_ptr<Client> c, Channel::Message& msg) {
  constexpr size_t max_count = sizeof(PSOBBGuildCardFile::entries) / sizeof(PSOBBGuildCardFile::Entry);
  constexpr size_t max_blocked = sizeof(PSOBBGuildCardFile::blocked) / sizeof(GuildCardBB);
  auto gcf = c->guild_card_file();
  bool should_save = false;
  switch (msg.command) {
    case 0x01E8: { // Check guild card file checksum
      const auto& cmd = check_size_t<C_GuildCardChecksum_01E8>(msg.data);
      uint32_t checksum = gcf->checksum();
      c->log.info_f("(Guild card file) Server checksum = {:08X}, client checksum = {:08X}",
          checksum, cmd.checksum);
      S_GuildCardChecksumResponse_BB_02E8 response = {
          (cmd.checksum != checksum), 0};
      send_command_t(c, 0x02E8, 0x00000000, response);
      break;
    }
    case 0x03E8: // Download guild card file
      check_size_v(msg.data.size(), 0);
      send_guild_card_header_bb(c);
      break;
    case 0x04E8: { // Add guild card
      auto& new_gc = check_size_t<GuildCardBB>(msg.data);
      for (size_t z = 0; z < max_count; z++) {
        if (!gcf->entries[z].data.present) {
          gcf->entries[z].data = new_gc;
          gcf->entries[z].unknown_a1.clear(0);
          c->log.info_f("Added guild card {} at position {}", new_gc.guild_card_number, z);
          should_save = true;
          break;
        }
      }
      break;
    }
    case 0x05E8: { // Delete guild card
      auto& cmd = check_size_t<C_DeleteGuildCard_BB_05E8_08E8>(msg.data);
      for (size_t z = 0; z < max_count; z++) {
        if (gcf->entries[z].data.guild_card_number == cmd.guild_card_number) {
          c->log.info_f("Deleted guild card {} at position {}", cmd.guild_card_number, z);
          for (z = 0; z < max_count - 1; z++) {
            gcf->entries[z] = gcf->entries[z + 1];
          }
          gcf->entries[max_count - 1].clear();
          should_save = true;
          break;
        }
      }
      break;
    }
    case 0x06E8: { // Update guild card
      auto& new_gc = check_size_t<GuildCardBB>(msg.data);
      for (size_t z = 0; z < max_count; z++) {
        if (gcf->entries[z].data.guild_card_number == new_gc.guild_card_number) {
          gcf->entries[z].data = new_gc;
          c->log.info_f("Updated guild card {} at position {}", new_gc.guild_card_number, z);
          should_save = true;
        }
      }
      if (c->login && new_gc.guild_card_number == c->login->account->account_id) {
        c->character(true, false)->guild_card.description = new_gc.description;
        c->log.info_f("Updated character's guild card");
      }
      break;
    }
    case 0x07E8: { // Add blocked user
      auto& new_gc = check_size_t<GuildCardBB>(msg.data);
      for (size_t z = 0; z < max_blocked; z++) {
        if (!gcf->blocked[z].present) {
          gcf->blocked[z] = new_gc;
          c->log.info_f("Added blocked guild card {} at position {}", new_gc.guild_card_number, z);
          // Note: The client also sends a C6 command, so we don't have to
          // manually sync the actual blocked senders list here
          should_save = true;
          break;
        }
      }
      break;
    }
    case 0x08E8: { // Delete blocked user
      auto& cmd = check_size_t<C_DeleteGuildCard_BB_05E8_08E8>(msg.data);
      for (size_t z = 0; z < max_blocked; z++) {
        if (gcf->blocked[z].guild_card_number == cmd.guild_card_number) {
          c->log.info_f("Deleted blocked guild card {} at position {}",
              cmd.guild_card_number, z);
          for (z = 0; z < max_blocked - 1; z++) {
            gcf->blocked[z] = gcf->blocked[z + 1];
          }
          gcf->blocked[max_blocked - 1].clear();
          // Note: The client also sends a C6 command, so we don't have to
          // manually sync the actual blocked senders list here
          should_save = true;
          break;
        }
      }
      break;
    }
    case 0x09E8: { // Write comment
      auto& cmd = check_size_t<C_WriteGuildCardComment_BB_09E8>(msg.data);
      for (size_t z = 0; z < max_count; z++) {
        if (gcf->entries[z].data.guild_card_number == cmd.guild_card_number) {
          gcf->entries[z].comment = cmd.comment;
          c->log.info_f("Updated comment on guild card {} at position {}", cmd.guild_card_number, z);
          should_save = true;
          break;
        }
      }
      break;
    }
    case 0x0AE8: { // Swap guild card positions in list
      auto& cmd = check_size_t<C_SwapGuildCardPositions_BB_0AE8>(msg.data);
      size_t index1 = max_count;
      size_t index2 = max_count;
      for (size_t z = 0; z < max_count; z++) {
        if (gcf->entries[z].data.guild_card_number == cmd.guild_card_number1) {
          if (index1 >= max_count) {
            index1 = z;
          } else {
            throw runtime_error("guild card 1 appears multiple times in file");
          }
        }
        if (gcf->entries[z].data.guild_card_number == cmd.guild_card_number2) {
          if (index2 >= max_count) {
            index2 = z;
          } else {
            throw runtime_error("guild card 2 appears multiple times in file");
          }
        }
      }
      if ((index1 >= max_count) || (index2 >= max_count)) {
        throw runtime_error("player does not have both requested guild cards");
      }

      if (index1 != index2) {
        PSOBBGuildCardFile::Entry displaced_entry = gcf->entries[index1];
        gcf->entries[index1] = gcf->entries[index2];
        gcf->entries[index2] = displaced_entry;
        c->log.info_f("Swapped positions of guild cards {} and {}",
            cmd.guild_card_number1, cmd.guild_card_number2);
        should_save = true;
      }
      break;
    }
    default:
      throw invalid_argument("invalid command");
  }
  if (should_save) {
    c->save_guild_card_file();
  }
  co_return;
}

static asio::awaitable<void> on_DC_BB(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& cmd = check_size_t<C_GuildCardDataRequest_BB_03DC>(msg.data);
  if (cmd.cont) {
    send_guild_card_chunk_bb(c, cmd.chunk_index);
  }
  co_return;
}

static asio::awaitable<void> on_EB_BB(shared_ptr<Client> c, Channel::Message& msg) {
  check_size_v(msg.data.size(), 0);

  if (msg.command == 0x04EB) {
    send_stream_file_index_bb(c);
  } else if (msg.command == 0x03EB) {
    send_stream_file_chunk_bb(c, msg.flag);
  } else {
    throw invalid_argument("unimplemented command");
  }
  co_return;
}

static asio::awaitable<void> on_EC_BB(shared_ptr<Client>, Channel::Message& msg) {
  msg.check_size_t<C_LeaveCharacterSelect_BB_00EC>();
  co_return;
}

static asio::awaitable<void> on_E5_BB(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& cmd = check_size_t<SC_PlayerPreview_CreateCharacter_BB_00E5>(msg.data);

  if (!c->login) {
    send_message_box(c, "$C6You are not logged in.");
    co_return;
  }

  if (c->character(false).get()) {
    throw runtime_error("player already exists");
  }

  c->bb_character_index = -1;
  c->system_file(); // Ensure system file is loaded
  c->bb_character_index = cmd.character_index;

  bool should_send_approve = true;
  if (c->bb_connection_phase == 0x03) { // Dressing room
    try {
      c->character()->disp.apply_dressing_room(cmd.preview);
    } catch (const exception& e) {
      send_message_box(c, std::format("$C6Character could not be modified:\n{}", e.what()));
      should_send_approve = false;
    }
  } else {
    try {
      auto s = c->require_server_state();
      c->create_character_file(c->login->account->account_id, c->language(), cmd.preview, s->level_table(c->version()));
    } catch (const exception& e) {
      send_message_box(c, std::format("$C6New character could not be created:\n{}", e.what()));
      should_send_approve = false;
    }
  }

  if (should_send_approve) {
    send_approve_player_choice_bb(c);
  }
}

static asio::awaitable<void> on_ED_BB(shared_ptr<Client> c, Channel::Message& msg) {
  auto sys = c->system_file();
  switch (msg.command) {
    case 0x01ED: {
      const auto& cmd = check_size_t<C_UpdateOptionFlags_BB_01ED>(msg.data);
      c->character(true, false)->option_flags = cmd.option_flags;
      break;
    }
    case 0x02ED: {
      const auto& cmd = check_size_t<C_UpdateSymbolChats_BB_02ED>(msg.data);
      c->character(true, false)->symbol_chats = cmd.symbol_chats;
      break;
    }
    case 0x03ED: {
      const auto& cmd = check_size_t<C_UpdateChatShortcuts_BB_03ED>(msg.data);
      c->character(true, false)->shortcuts = cmd.chat_shortcuts;
      break;
    }
    case 0x04ED: {
      const auto& cmd = check_size_t<C_UpdateKeyConfig_BB_04ED>(msg.data);
      sys->key_config = cmd.key_config;
      c->save_system_file();
      break;
    }
    case 0x05ED: {
      const auto& cmd = check_size_t<C_UpdatePadConfig_BB_05ED>(msg.data);
      sys->joystick_config = cmd.pad_config;
      c->save_system_file();
      break;
    }
    case 0x06ED: {
      const auto& cmd = check_size_t<C_UpdateTechMenu_BB_06ED>(msg.data);
      c->character(true, false)->tech_menu_shortcut_entries = cmd.tech_menu;
      break;
    }
    case 0x07ED: {
      const auto& cmd = check_size_t<C_UpdateCustomizeMenu_BB_07ED>(msg.data);
      c->character()->disp.config = cmd.customize;
      break;
    }
    case 0x08ED: {
      const auto& cmd = check_size_t<C_UpdateChallengeRecords_BB_08ED>(msg.data);
      c->character(true, false)->challenge_records = cmd.records;
      break;
    }
    default:
      throw invalid_argument("unknown account command");
  }
  co_return;
}

static asio::awaitable<void> on_E7_BB(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& cmd = check_size_t<SC_SyncSaveFiles_BB_E7>(msg.data);

  // TODO: In the future, we shouldn't need to trust any of the client's data
  // here. We should instead verify our copy of the player against what the
  // client sent, and alert on anything that's out of sync.
  auto p = c->character();
  p->challenge_records = cmd.char_file.challenge_records;
  p->battle_records = cmd.char_file.battle_records;
  p->death_count = cmd.char_file.death_count;
  *c->system_file() = cmd.system_file;
  co_return;
}

static asio::awaitable<void> on_E2_BB(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& cmd = check_size_t<S_SyncSystemFile_BB_E2>(msg.data);
  *c->system_file() = cmd.system_file;
  c->save_system_file();

  S_SystemFileCreated_00E1_BB out_cmd = {1};
  send_command_t(c, 0x00E1, 0x00000000, out_cmd);
  co_return;
}

static asio::awaitable<void> on_DF_BB(shared_ptr<Client> c, Channel::Message& msg) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();
  if (!l->is_game()) {
    throw runtime_error("challenge mode config command sent outside of game");
  }
  if (l->mode != GameMode::CHALLENGE) {
    throw runtime_error("challenge mode config command sent in non-challenge game");
  }
  auto cp = l->require_challenge_params();

  switch (msg.command) {
    case 0x01DF: {
      const auto& cmd = check_size_t<C_SetChallengeModeStageNumber_BB_01DF>(msg.data);
      cp->stage_number = cmd.stage;
      l->log.info_f("(Challenge mode) Stage number set to {:02X}", cp->stage_number);
      break;
    }

    case 0x02DF: {
      const auto& cmd = check_size_t<C_SetChallengeModeCharacterTemplate_BB_02DF>(msg.data);
      if (!l->quest) {
        throw runtime_error("challenge mode character template config command sent in non-challenge game");
      }
      auto vq = l->quest->version(Version::BB_V4, c->language());
      if (vq->challenge_template_index != static_cast<ssize_t>(cmd.template_index)) {
        throw runtime_error("challenge template index in quest metadata does not match index sent by client");
      }

      for (auto& m : l->floor_item_managers) {
        m.clear();
      }

      for (auto lc : l->clients) {
        // On non-BB, there is no DF command, and overlays are created at quest
        // start time instead, hence the version check here.
        if (lc && is_v4(lc->version())) {
          lc->use_default_bank();
          lc->create_challenge_overlay(lc->version(), l->quest->challenge_template_index, s->level_table(lc->version()));
          lc->log.info_f("Created challenge overlay");
          l->assign_inventory_and_bank_item_ids(lc, true);
        }
      }

      l->map_state->reset();
      break;
    }

    case 0x03DF: {
      const auto& cmd = check_size_t<C_SetChallengeModeDifficulty_BB_03DF>(msg.data);
      if (l->difficulty != cmd.difficulty) {
        l->difficulty = cmd.difficulty;
        l->create_item_creator();
      }
      l->log.info_f("(Challenge mode) Difficulty set to {:02X}", l->difficulty);
      break;
    }

    case 0x04DF: {
      const auto& cmd = check_size_t<C_SetChallengeModeEXPMultiplier_BB_04DF>(msg.data);
      l->challenge_exp_multiplier = cmd.exp_multiplier;
      l->log.info_f("(Challenge mode) EXP multiplier set to {:g}", l->challenge_exp_multiplier);
      break;
    }

    case 0x05DF: {
      const auto& cmd = check_size_t<C_SetChallengeRankText_BB_05DF>(msg.data);
      cp->rank_color = cmd.rank_color;
      cp->rank_text = cmd.rank_text.decode();
      l->log.info_f("(Challenge mode) Rank text set to {} (color {:08X})", cp->rank_text, cp->rank_color);
      break;
    }

    case 0x06DF: {
      const auto& cmd = check_size_t<C_SetChallengeRankThreshold_BB_06DF>(msg.data);
      auto& threshold = cp->rank_thresholds[cmd.rank];
      threshold.bitmask = cmd.rank_bitmask;
      threshold.seconds = cmd.seconds;
      string time_str = phosg::format_duration(static_cast<uint64_t>(threshold.seconds) * 1000000);
      l->log.info_f("(Challenge mode) Rank {} threshold set to {} (bitmask {:08X})",
          char_for_challenge_rank(cmd.rank), time_str, threshold.bitmask);
      break;
    }

    case 0x07DF: {
      const auto& cmd = check_size_t<C_CreateChallengeModeAwardItem_BB_07DF>(msg.data);
      auto p = c->character(true, false);
      auto& award_state = (l->episode == Episode::EP2)
          ? p->challenge_records.ep2_online_award_state
          : p->challenge_records.ep1_online_award_state;
      award_state.rank_award_flags |= cmd.rank_bitmask;
      p->add_item(cmd.item, *s->item_stack_limits(c->version()));
      l->on_item_id_generated_externally(cmd.item.id);
      string desc = s->describe_item(Version::BB_V4, cmd.item, false);
      l->log.info_f("(Challenge mode) Item awarded to player {}: {}", c->lobby_client_id, desc);
      break;
    }
  }
  co_return;
}

static asio::awaitable<void> on_89(shared_ptr<Client> c, Channel::Message& msg) {
  check_size_v(msg.data.size(), 0);

  c->lobby_arrow_color = msg.flag;
  auto l = c->lobby.lock();
  if (l) {
    send_arrow_update(l);
  }
  co_return;
}

static asio::awaitable<void> on_40(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& cmd = check_size_t<C_GuildCardSearch_40>(msg.data);
  try {
    auto s = c->require_server_state();
    auto result = s->find_client(nullptr, cmd.target_guild_card_number);
    if (!result->blocked_senders.count(c->login->account->account_id)) {
      auto result_lobby = result->lobby.lock();
      if (result_lobby) {
        send_card_search_result(c, result, result_lobby);
      }
    }
  } catch (const out_of_range&) {
  }
  co_return;
}

static asio::awaitable<void> on_C0(shared_ptr<Client> c, Channel::Message&) {
  send_choice_search_choices(c);
  co_return;
}

static asio::awaitable<void> on_C2(shared_ptr<Client> c, Channel::Message& msg) {
  c->character()->choice_search_config = check_size_t<ChoiceSearchConfig>(msg.data);
  co_return;
}

template <typename ResultT>
static void on_choice_search_t(shared_ptr<Client> c, const ChoiceSearchConfig& cmd) {
  auto s = c->require_server_state();

  vector<ResultT> results;
  for (const auto& l : s->all_lobbies()) {
    for (const auto& lc : l->clients) {
      if (!lc || !lc->login || lc->character()->choice_search_config.disabled) {
        continue;
      }

      bool is_match = true;
      for (const auto& cat : CHOICE_SEARCH_CATEGORIES) {
        int32_t setting = cmd.get_setting(cat.id);
        if (setting == -1) {
          continue;
        }
        try {
          if (!cat.client_matches(c, lc, setting)) {
            is_match = false;
            break;
          }
        } catch (const exception& e) {
          c->log.info_f("Error in Choice Search matching for category {}: {}", cat.name, e.what());
        }
      }

      if (is_match) {
        auto lp = lc->character();
        auto& result = results.emplace_back();
        result.guild_card_number = lc->login->account->account_id;
        result.name.encode(lp->disp.name.decode(lc->language()), c->language());
        string info_string = std::format("{} Lv{}\n{}\n",
            name_for_char_class(lp->disp.visual.char_class),
            static_cast<size_t>(lp->disp.stats.level + 1),
            name_for_section_id(lp->disp.visual.section_id));
        result.info_string.encode(info_string, c->language());
        string location_string;
        if (l->is_game()) {
          location_string = std::format("{},,BLOCK01,{}", l->name, s->name);
        } else if (l->is_ep3()) {
          location_string = std::format("BLOCK01-C{:02},,BLOCK01,{}", l->lobby_id - 15, s->name);
        } else {
          location_string = std::format("BLOCK01-{:02},,BLOCK01,{}", l->lobby_id, s->name);
        }
        result.location_string.encode(location_string, c->language());
        result.reconnect_command_header.command = 0x19;
        result.reconnect_command_header.flag = 0x00;
        result.reconnect_command_header.size = sizeof(result.reconnect_command) + sizeof(result.reconnect_command_header);
        result.reconnect_command.address = s->connect_address_for_client(c);
        result.reconnect_command.port = s->game_server_port_for_version(c->version());
        result.meet_user.lobby_refs[0].menu_id = MenuID::LOBBY;
        result.meet_user.lobby_refs[0].item_id = l->lobby_id;
        result.meet_user.player_name.encode(lp->disp.name.decode(lc->language()), c->language());
        // The client can only handle 32 results
        if (results.size() >= 0x20) {
          break;
        }
      }
    }
  }

  if (results.empty()) {
    // There is a client bug that causes garbage to appear in the info window
    // when the server returns no entries in this command, since the client
    // tries to display the first entry in the list even if the list contains
    // "No player". If the server sends no entries at all, the entry will
    // uninitialized memory which can cause crashes on v2, so we send a blank
    // entry to prevent this.
    auto& result = results.emplace_back();
    result.reconnect_command_header.command = 0x00;
    result.reconnect_command_header.flag = 0x00;
    result.reconnect_command_header.size = 0x0000;
    send_command_vt(c, 0xC4, 0, results);
  } else {
    send_command_vt(c, 0xC4, results.size(), results);
  }
}

static asio::awaitable<void> on_C3(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& cmd = check_size_t<ChoiceSearchConfig>(msg.data);
  switch (c->version()) {
      // DC V1 and the prototypes do not support this command
    case Version::DC_V2:
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
    case Version::XB_V3:
      on_choice_search_t<S_ChoiceSearchResultEntry_DC_V3_C4>(c, cmd);
      break;
    case Version::PC_NTE:
    case Version::PC_V2:
      on_choice_search_t<S_ChoiceSearchResultEntry_PC_C4>(c, cmd);
      break;
    case Version::BB_V4:
      on_choice_search_t<S_ChoiceSearchResultEntry_BB_C4>(c, cmd);
      break;
    default:
      throw runtime_error("unimplemented versioned command");
  }
  co_return;
}

static asio::awaitable<void> on_81(shared_ptr<Client> c, Channel::Message& msg) {
  string message;
  uint32_t to_guild_card_number;
  switch (c->version()) {
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
    case Version::XB_V3: {
      const auto& cmd = check_size_t<SC_SimpleMail_DC_V3_81>(msg.data);
      to_guild_card_number = cmd.to_guild_card_number;
      message = cmd.text.decode(c->language());
      break;
    }
    case Version::PC_NTE:
    case Version::PC_V2: {
      const auto& cmd = check_size_t<SC_SimpleMail_PC_81>(msg.data);
      to_guild_card_number = cmd.to_guild_card_number;
      message = cmd.text.decode(c->language());
      break;
    }
    case Version::BB_V4: {
      const auto& cmd = check_size_t<SC_SimpleMail_BB_81>(msg.data);
      to_guild_card_number = cmd.to_guild_card_number;
      message = cmd.text.decode(c->language());
      break;
    }
    default:
      throw logic_error("invalid game version");
  }

  auto s = c->require_server_state();
  shared_ptr<Client> target;
  try {
    target = s->find_client(nullptr, to_guild_card_number);
  } catch (const out_of_range&) {
  }

  if (!target || !target->login) {
    // TODO: We should store pending messages for accounts somewhere, and send
    // them when the player signs on again.
    if (!c->blocked_senders.count(to_guild_card_number)) {
      try {
        auto target_account = s->account_index->from_account_id(to_guild_card_number);
        if (!target_account->auto_reply_message.empty()) {
          send_simple_mail(
              c,
              target_account->account_id,
              target_account->last_player_name,
              target_account->auto_reply_message);
        }
      } catch (const AccountIndex::missing_account&) {
      }
    }
    send_text_message(c, "$C6Player is offline");

  } else {
    // If the sender is blocked, don't forward the mail
    if (target->blocked_senders.count(c->login->account->account_id)) {
      co_return;
    }

    // If the target has auto-reply enabled, send the autoreply. Note that we also
    // forward the message in this case.
    if (!c->blocked_senders.count(target->login->account->account_id)) {
      auto target_p = target->character();
      if (!target_p->auto_reply.empty()) {
        send_simple_mail(
            c,
            target->login->account->account_id,
            target_p->disp.name.decode(target_p->inventory.language),
            target_p->auto_reply.decode(target_p->inventory.language));
      }
    }

    // Forward the message
    send_simple_mail(
        target,
        c->login->account->account_id,
        c->character()->disp.name.decode(c->language()),
        message);
  }
}

static asio::awaitable<void> on_D8(shared_ptr<Client> c, Channel::Message& msg) {
  check_size_v(msg.data.size(), 0);
  send_info_board(c);
  co_return;
}

static asio::awaitable<void> on_D9(shared_ptr<Client> c, Channel::Message& msg) {
  phosg::strip_trailing_zeroes(msg.data);
  bool is_w = uses_utf16(c->version());
  if (is_w && (msg.data.size() & 1)) {
    msg.data.push_back(0);
  }
  try {
    c->character(true, false)->info_board.encode(tt_decode_marked(msg.data, c->language(), is_w), c->language());
  } catch (const runtime_error& e) {
    c->log.warning_f("Failed to decode info board message: {}", e.what());
  }
  co_return;
}

static asio::awaitable<void> on_C7(shared_ptr<Client> c, Channel::Message& msg) {
  phosg::strip_trailing_zeroes(msg.data);
  bool is_w = uses_utf16(c->version());
  if (is_w && (msg.data.size() & 1)) {
    msg.data.push_back(0);
  }

  string message = tt_decode_marked(msg.data, c->language(), is_w);
  c->character(true, false)->auto_reply.encode(message, c->language());
  c->login->account->auto_reply_message = message;
  c->login->account->save();
  co_return;
}

static asio::awaitable<void> on_C8(shared_ptr<Client> c, Channel::Message& msg) {
  check_size_v(msg.data.size(), 0);
  c->character(true, false)->auto_reply.clear();
  c->login->account->auto_reply_message.clear();
  c->login->account->save();
  co_return;
}

static asio::awaitable<void> on_C6(shared_ptr<Client> c, Channel::Message& msg) {
  c->blocked_senders.clear();
  if (c->version() == Version::BB_V4) {
    const auto& cmd = check_size_t<C_SetBlockedSenders_BB_C6>(msg.data);
    c->import_blocked_senders(cmd.blocked_senders);
  } else {
    const auto& cmd = check_size_t<C_SetBlockedSenders_V3_C6>(msg.data);
    c->import_blocked_senders(cmd.blocked_senders);
  }
  co_return;
}

static asio::awaitable<void> on_C9_XB(shared_ptr<Client> c, Channel::Message& msg) {
  check_size_v(msg.data.size(), 0);
  c->log.warning_f("Ignoring connection status change command ({:02X})", msg.flag);
  co_return;
}

shared_ptr<Lobby> create_game_generic(
    shared_ptr<ServerState> s,
    shared_ptr<Client> creator_c,
    const std::string& name,
    const std::string& password,
    Episode episode,
    GameMode mode,
    uint8_t difficulty,
    bool allow_v1,
    shared_ptr<Lobby> watched_lobby,
    shared_ptr<Episode3::BattleRecordPlayer> battle_player) {

  if ((episode != Episode::EP1) &&
      (episode != Episode::EP2) &&
      (episode != Episode::EP3) &&
      (episode != Episode::EP4)) {
    throw invalid_argument("incorrect episode number");
  }

  if (difficulty > 3) {
    throw invalid_argument("incorrect difficulty level");
  }

  auto current_lobby = creator_c->require_lobby();

  size_t min_level = s->default_min_level_for_game(creator_c->version(), episode, difficulty);

  auto p = creator_c->character();
  if (!creator_c->login->account->check_flag(Account::Flag::FREE_JOIN_GAMES) && (min_level > p->disp.stats.level)) {
    // Note: We don't throw here because this is a situation players might
    // actually encounter while playing the game normally
    string msg = std::format("You must be level {}\nor above to play\nthis difficulty.", static_cast<size_t>(min_level + 1));
    send_lobby_message_box(creator_c, msg);
    return nullptr;
  }

  shared_ptr<Lobby> game = s->create_lobby(true);
  game->name = name;
  game->episode = episode;
  game->mode = mode;
  game->difficulty = difficulty;
  game->allowed_versions = s->compatibility_groups.at(static_cast<size_t>(creator_c->version()));
  static_assert(NUM_VERSIONS == 14, "Don't forget to update the group compatibility restrictions");
  if (!allow_v1 || (difficulty > 2) || (mode == GameMode::CHALLENGE) || (mode == GameMode::SOLO)) {
    game->forbid_version(Version::DC_NTE);
    game->forbid_version(Version::DC_11_2000);
    game->forbid_version(Version::DC_V1);
  } else if (mode == GameMode::BATTLE) {
    game->forbid_version(Version::DC_NTE);
    game->forbid_version(Version::DC_11_2000);
    // v1 supports battle mode but not battle quests
  }
  switch (game->episode) {
    case Episode::NONE:
      throw logic_error("game episode not set at creation time");
    case Episode::EP1:
      for (Version v : ALL_VERSIONS) {
        if (is_ep3(v)) {
          game->forbid_version(v);
        }
      }
      break;
    case Episode::EP2:
      for (Version v : ALL_VERSIONS) {
        if (is_v1_or_v2(v) || is_ep3(v)) {
          game->forbid_version(v);
        }
      }
      break;
    case Episode::EP3:
      for (Version v : ALL_VERSIONS) {
        if (!is_ep3(v)) {
          game->forbid_version(v);
        }
      }
      break;
    case Episode::EP4:
      for (Version v : ALL_VERSIONS) {
        if (!is_v4(v)) {
          game->forbid_version(v);
        }
      }
      break;
  }

  if (creator_c->check_flag(Client::Flag::DEBUG_ENABLED)) {
    game->set_flag(Lobby::Flag::DEBUG);
  }
  if (creator_c->check_flag(Client::Flag::IS_CLIENT_CUSTOMIZATION)) {
    game->set_flag(Lobby::Flag::IS_CLIENT_CUSTOMIZATION);
  }

  while (game->floor_item_managers.size() < 0x12) {
    game->floor_item_managers.emplace_back(game->lobby_id, game->floor_item_managers.size());
  }

  if (s->behavior_enabled(s->cheat_mode_behavior)) {
    game->set_flag(Lobby::Flag::CHEATS_ENABLED);
  }
  if (!s->behavior_can_be_overridden(s->cheat_mode_behavior)) {
    game->set_flag(Lobby::Flag::CANNOT_CHANGE_CHEAT_MODE);
  }
  if (s->use_game_creator_section_id) {
    game->set_flag(Lobby::Flag::USE_CREATOR_SECTION_ID);
  }
  if (watched_lobby || battle_player) {
    game->set_flag(Lobby::Flag::IS_SPECTATOR_TEAM);
  }
  game->password = password;

  game->creator_section_id = p->disp.visual.section_id;
  game->override_section_id = creator_c->override_section_id;
  if (game->mode == GameMode::CHALLENGE) {
    game->challenge_params = make_shared<Lobby::ChallengeParameters>();
  }
  if (creator_c->override_random_seed >= 0) {
    game->random_seed = creator_c->override_random_seed;
    game->log.info_f("Using random seed {:08X} from client override", game->random_seed);
  } else {
    game->random_seed = phosg::random_object<uint32_t>();
    game->log.info_f("Using random seed {:08X} from system generator", game->random_seed);
  }
  if (s->use_psov2_rand_crypt) {
    game->rand_crypt = make_shared<PSOV2Encryption>(game->random_seed);
    game->log.info_f("Using PSOV2Encryption for item generation", game->random_seed);
  } else {
    game->rand_crypt = make_shared<MT19937Generator>(game->random_seed);
    game->log.info_f("Using MT19937Generator for item generation", game->random_seed);
  }
  if (battle_player) {
    game->battle_player = battle_player;
    battle_player->set_lobby(game);
  }
  game->base_exp_multiplier = s->bb_global_exp_multiplier;
  game->exp_share_multiplier = s->exp_share_multiplier;

  const unordered_map<uint16_t, IntegralExpression>* quest_flag_rewrites;
  switch (creator_c->version()) {
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::PC_NTE:
    case Version::PC_V2:
      quest_flag_rewrites = &s->quest_flag_rewrites_v1_v2;
      if (game->mode == GameMode::BATTLE) {
        game->drop_mode = s->default_drop_mode_v1_v2_battle;
        game->allowed_drop_modes = s->allowed_drop_modes_v1_v2_battle;
      } else if (game->mode == GameMode::CHALLENGE) {
        game->drop_mode = s->default_drop_mode_v1_v2_challenge;
        game->allowed_drop_modes = s->allowed_drop_modes_v1_v2_challenge;
      } else {
        game->drop_mode = s->default_drop_mode_v1_v2_normal;
        game->allowed_drop_modes = s->allowed_drop_modes_v1_v2_normal;
      }
      break;
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::XB_V3:
      quest_flag_rewrites = &s->quest_flag_rewrites_v3;
      if (game->mode == GameMode::BATTLE) {
        game->drop_mode = s->default_drop_mode_v3_battle;
        game->allowed_drop_modes = s->allowed_drop_modes_v3_battle;
      } else if (game->mode == GameMode::CHALLENGE) {
        game->drop_mode = s->default_drop_mode_v3_challenge;
        game->allowed_drop_modes = s->allowed_drop_modes_v3_challenge;
      } else {
        game->drop_mode = s->default_drop_mode_v3_normal;
        game->allowed_drop_modes = s->allowed_drop_modes_v3_normal;
      }
      break;
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      quest_flag_rewrites = nullptr;
      game->drop_mode = Lobby::DropMode::DISABLED;
      game->allowed_drop_modes = (1 << static_cast<size_t>(game->drop_mode));
      break;
    case Version::BB_V4:
      quest_flag_rewrites = &s->quest_flag_rewrites_v4;
      if (game->mode == GameMode::BATTLE) {
        game->drop_mode = s->default_drop_mode_v4_battle;
        game->allowed_drop_modes = s->allowed_drop_modes_v4_battle;
      } else if (game->mode == GameMode::CHALLENGE) {
        game->drop_mode = s->default_drop_mode_v4_challenge;
        game->allowed_drop_modes = s->allowed_drop_modes_v4_challenge;
      } else {
        game->drop_mode = s->default_drop_mode_v4_normal;
        game->allowed_drop_modes = s->allowed_drop_modes_v4_normal;
      }
      // Disallow CLIENT mode on BB
      if (game->drop_mode == Lobby::DropMode::CLIENT) {
        throw logic_error("CLIENT mode not allowed on BB");
      }
      if (game->allowed_drop_modes & (1 << static_cast<size_t>(Lobby::DropMode::CLIENT))) {
        throw logic_error("CLIENT mode not allowed on BB");
      }
      break;
    default:
      throw logic_error("invalid quest script version");
  }
  game->create_item_creator(creator_c->version());

  game->event = current_lobby->event;
  game->block = 0xFF;
  game->max_clients = game->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM) ? 12 : 4;
  game->min_level = min_level;
  game->max_level = 0xFFFFFFFF;
  if (watched_lobby) {
    game->watched_lobby = watched_lobby;
    watched_lobby->watcher_lobbies.emplace(game);
  }

  bool is_solo = (game->mode == GameMode::SOLO);

  if (game->mode == GameMode::CHALLENGE) {
    game->rare_enemy_rates = s->rare_enemy_rates_challenge;
  } else {
    game->rare_enemy_rates = s->rare_enemy_rates_by_difficulty.at(game->difficulty);
  }

  if (game->episode != Episode::EP3) {
    // GC NTE ignores the passed-in variations and always uses all zeroes
    if (creator_c->version() == Version::GC_NTE) {
      game->variations = Variations();
      game->log.info_f("Base version is GC_NTE; using blank variations");
    } else if (creator_c->override_variations) {
      game->variations = *creator_c->override_variations;
      creator_c->override_variations.reset();
      auto vars_str = game->variations.str();
      game->log.info_f("Using variations from client override: {}", vars_str);
    } else {
      auto sdt = s->set_data_table(creator_c->version(), game->episode, game->mode, game->difficulty);
      game->variations = sdt->generate_variations(game->episode, is_solo, game->rand_crypt);
      auto vars_str = game->variations.str();
      game->log.info_f("Using random variations: {}", vars_str);
    }
  } else {
    game->variations = Variations();
  }
  game->load_maps(); // Load free-play maps

  // The game's quest flags are inherited from the creator, if known
  if (creator_c->version() == Version::BB_V4) {
    game->quest_flag_values = make_unique<QuestFlags>(p->quest_flags);
    game->quest_flags_known = nullptr;
  } else {
    game->quest_flag_values = make_unique<QuestFlags>();
    game->quest_flags_known = make_unique<QuestFlags>();
  }

  if (quest_flag_rewrites && !quest_flag_rewrites->empty()) {
    IntegralExpression::Env env = {
        .flags = &p->quest_flags.data.at(difficulty),
        .challenge_records = &p->challenge_records,
        .team = creator_c->team(),
        .num_players = 1,
        .event = game->event,
        .v1_present = is_v1(creator_c->version()),
    };
    for (const auto& it : *quest_flag_rewrites) {
      bool should_set = it.second.evaluate(env);
      game->log.info_f("Overriding quest flag {:04X} = {}", it.first, should_set ? "true" : "false");
      if (should_set) {
        game->quest_flag_values->set(game->difficulty, it.first);
      } else {
        game->quest_flag_values->clear(game->difficulty, it.first);
      }
      if (game->quest_flags_known) {
        game->quest_flags_known->set(game->difficulty, it.first);
      }
    }
    creator_c->set_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_FLAG_STATE);
  }

  game->switch_flags = make_unique<SwitchFlags>();

  return game;
}

static asio::awaitable<void> on_C1_PC(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& cmd = check_size_t<C_CreateGame_PC_C1>(msg.data);
  auto s = c->require_server_state();

  GameMode mode = GameMode::NORMAL;
  if (cmd.battle_mode) {
    mode = GameMode::BATTLE;
  } else if (cmd.challenge_mode) {
    mode = GameMode::CHALLENGE;
  }
  auto game = create_game_generic(s, c, cmd.name.decode(c->language()), cmd.password.decode(c->language()), Episode::EP1, mode, cmd.difficulty, true);
  if (game) {
    s->change_client_lobby(c, game);
    c->set_flag(Client::Flag::LOADING);
    c->log.info_f("LOADING flag set");
  }
  co_return;
}

static asio::awaitable<void> on_0C_C1_E7_EC(shared_ptr<Client> c, Channel::Message& msg) {
  auto s = c->require_server_state();

  shared_ptr<Lobby> game;
  if (is_pre_v1(c->version())) {
    const auto& cmd = check_size_t<C_CreateGame_DCNTE>(msg.data);
    game = create_game_generic(s, c, cmd.name.decode(c->language()), cmd.password.decode(c->language()), Episode::EP1, GameMode::NORMAL, 0, true);

  } else {
    const auto& cmd = check_size_t<C_CreateGame_DC_V3_0C_C1_Ep3_EC>(msg.data);

    // Only allow E7/EC from Ep3 clients
    bool client_is_ep3 = is_ep3(c->version());
    if (((msg.command & 0xF0) == 0xE0) != client_is_ep3) {
      throw runtime_error("invalid command");
    }

    Episode episode = Episode::NONE;
    bool allow_v1 = false;
    if (is_v1_or_v2(c->version()) && (c->version() != Version::GC_NTE)) {
      allow_v1 = (cmd.episode == 0);
      episode = Episode::EP1;
    } else if (client_is_ep3) {
      episode = Episode::EP3;
    } else { // XB/GC non-Ep3
      episode = cmd.episode == 2 ? Episode::EP2 : Episode::EP1;
    }

    GameMode mode = GameMode::NORMAL;
    bool spectators_forbidden = false;
    if (cmd.battle_mode) {
      mode = GameMode::BATTLE;
    }
    if (cmd.challenge_mode) {
      if (client_is_ep3) {
        spectators_forbidden = true;
      } else {
        mode = GameMode::CHALLENGE;
      }
    }

    shared_ptr<Lobby> watched_lobby;
    if (msg.command == 0xE7) {
      if (cmd.menu_id != MenuID::GAME) {
        throw runtime_error("incorrect menu ID");
      }
      watched_lobby = s->find_lobby(cmd.item_id);
      if (!watched_lobby) {
        send_lobby_message_box(c, "$C7This game no longer\nexists");
        co_return;
      }
      if (watched_lobby->check_flag(Lobby::Flag::SPECTATORS_FORBIDDEN)) {
        send_lobby_message_box(c, "$C7This game does not\nallow spectators");
        co_return;
      }
    }

    game = create_game_generic(s, c, cmd.name.decode(c->language()), cmd.password.decode(c->language()), episode, mode, cmd.difficulty, allow_v1, watched_lobby);
    if (game && (game->episode == Episode::EP3)) {
      game->ep3_ex_result_values = s->ep3_default_ex_values;
      if (spectators_forbidden) {
        game->set_flag(Lobby::Flag::SPECTATORS_FORBIDDEN);
      }
    }
  }

  if (game) {
    s->change_client_lobby(c, game);
    c->set_flag(Client::Flag::LOADING);
    c->log.info_f("LOADING flag set");

    // There is a bug in DC NTE and 11/2000 that causes them to assign item IDs
    // twice when joining a game. If there are other players in the game, this
    // isn't an issue because the equivalent of the 6x6D command resets the next
    // item ID before the second assignment, so the item IDs stay in sync with
    // the server. If there was no one else in the game, however (as in this
    // case, when it was just created), we need to artificially change the next
    // item IDs during the client's loading procedure.
    if (is_pre_v1(c->version())) {
      c->set_flag(Client::Flag::SHOULD_SEND_ARTIFICIAL_ITEM_STATE);
    }
  }
}

static asio::awaitable<void> on_C1_BB(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& cmd = check_size_t<C_CreateGame_BB_C1>(msg.data);
  auto s = c->require_server_state();

  GameMode mode = GameMode::NORMAL;
  if (cmd.battle_mode) {
    mode = GameMode::BATTLE;
  }
  if (cmd.challenge_mode) {
    mode = GameMode::CHALLENGE;
  }
  if (cmd.solo_mode) {
    mode = GameMode::SOLO;
  }

  Episode episode;
  switch (cmd.episode) {
    case 1:
      episode = Episode::EP1;
      break;
    case 2:
      episode = Episode::EP2;
      break;
    case 3:
      episode = Episode::EP4;
      // Disallow battle/challenge in Ep4
      if (mode == GameMode::BATTLE) {
        send_lobby_message_box(c, "$C7Episode 4 does not\nsupport Battle Mode.");
        co_return;
      }
      if (mode == GameMode::CHALLENGE) {
        send_lobby_message_box(c, "$C7Episode 4 does not\nsupport Challenge Mode.");
        co_return;
      }
      break;
    default:
      throw runtime_error("invalid episode number");
  }

  auto game = create_game_generic(s, c, cmd.name.decode(c->language()), cmd.password.decode(c->language()), episode, mode, cmd.difficulty);
  if (game) {
    s->change_client_lobby(c, game);
    c->set_flag(Client::Flag::LOADING);
    c->log.info_f("LOADING flag set");
  }
}

static asio::awaitable<void> on_8A(shared_ptr<Client> c, Channel::Message& msg) {
  if (c->version() == Version::DC_NTE) {
    const auto& cmd = check_size_t<C_ConnectionInfo_DCNTE_8A>(msg.data);
    c->hardware_id = cmd.hardware_id;
    set_console_client_flags(c, cmd.sub_version);
    send_command(c, 0x8A, 0x01);

  } else {
    check_size_v(msg.data.size(), 0);
    auto l = c->lobby.lock();
    if (!l) {
      if (c->check_flag(Client::Flag::AWAITING_ENABLE_B2_QUEST)) {
        send_lobby_name(c, "");
      } else {
        throw std::runtime_error("received 8A command from client not in any lobby");
      }
    } else {
      send_lobby_name(c, l->name);
    }
  }
  co_return;
}

static asio::awaitable<void> on_6F(shared_ptr<Client> c, Channel::Message& msg) {
  check_size_v(msg.data.size(), 0);

  auto l = c->lobby.lock();
  if (!l) {
    if (!c->check_flag(Client::Flag::AWAITING_ENABLE_B2_QUEST)) {
      throw runtime_error("client is not in any lobby and is not awaiting a patch enabler quest");
    }
    auto s = c->require_server_state();
    shared_ptr<const Quest> q;
    try {
      int64_t quest_num = s->enable_send_function_call_quest_numbers.at(c->specific_version);
      q = s->default_quest_index->get(quest_num);
    } catch (const out_of_range&) {
      throw std::logic_error("cannot find patch enable quest after it was previously found during login");
    }
    auto vq = q->version(is_ep3(c->version()) ? Version::GC_V3 : c->version(), 1);
    if (!vq) {
      throw std::logic_error("cannot find patch enable quest version after it was previously found during login");
    }
    c->log.info_f("Sending {} version of quest \"{}\"", char_for_language_code(vq->language), vq->name);
    string bin_filename = vq->bin_filename();
    string dat_filename = vq->dat_filename();
    string xb_filename = vq->xb_filename();
    send_open_quest_file(c, bin_filename, bin_filename, xb_filename, vq->quest_number, QuestFileType::ONLINE, vq->bin_contents);
    send_open_quest_file(c, dat_filename, dat_filename, xb_filename, vq->quest_number, QuestFileType::ONLINE, vq->dat_contents);
    co_return;
  }
  // Now l is not null

  if (!l->is_game()) {
    throw runtime_error("client sent ready command outside of game");
  }

  // Episode 3 sends a 6F after a CAx21 (end battle) command, so we shouldn't
  // reassign the item IDs again in that case (even though item IDs really
  // don't matter for Ep3)
  if (c->check_flag(Client::Flag::LOADING)) {
    c->clear_flag(Client::Flag::LOADING);
    c->log.info_f("LOADING flag cleared");

    // The client sends 6F when it has created its TObjPlayer and assigned its
    // item IDs. For the leader, however, this happens before any inbound commands
    // are processed, so we already did it when the client was added to the lobby.
    // So, we only assign item IDs here if the client is not the leader.
    if ((msg.command == 0x006F) && (c->lobby_client_id != l->leader_id)) {
      l->assign_inventory_and_bank_item_ids(c, true);
    }
  }

  if (l->ep3_server && l->ep3_server->battle_finished) {
    auto s = l->require_server_state();
    l->log.info_f("Deleting Episode 3 server state");
    l->ep3_server.reset();
  }

  send_server_time(c);
  if (c->check_flag(Client::Flag::DEBUG_ENABLED)) {
    string variations_str;
    for (size_t z = 0; z < l->variations.entries.size(); z++) {
      const auto& e = l->variations.entries[z];
      variations_str += std::format(" {:X}{:X}", e.layout, e.entities);
    }
    char type_ch = dynamic_pointer_cast<DisabledRandomGenerator>(l->rand_crypt).get()
        ? 'X'
        : dynamic_pointer_cast<MT19937Generator>(l->rand_crypt).get()
        ? 'M'
        : dynamic_pointer_cast<PSOV2Encryption>(l->rand_crypt).get()
        ? 'L'
        : '?';
    send_text_message_fmt(c, "Rare seed: {:08X}/{:c}\nVariations:{}\n", l->random_seed, type_ch, variations_str);
  }

  bool should_resume_game = true;
  if (c->version() == Version::BB_V4) {
    send_set_exp_multiplier(l);
    send_update_team_reward_flags(c);
    send_all_nearby_team_metadatas_to_client(c, false);

    // On BB, send the joinable quest file as soon as the client is ready (6F).
    // On other versions, we send joinable quests in the 99 handler instead,
    // since we need to wait for the client's save to complete.
    // BB sends 016F when the client is done loading a quest. In that case, we
    // shouldn't send the quest to them again!
    if ((msg.command == 0x006F) && l->check_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS)) {
      if (!l->quest) {
        throw runtime_error("JOINABLE_QUEST_IN_PROGRESS is set, but lobby has no quest");
      }
      auto vq = l->quest->version(c->version(), c->language());
      if (!vq) {
        throw runtime_error("JOINABLE_QUEST_IN_PROGRESS is set, but lobby has no quest for client version");
      }
      string bin_filename = vq->bin_filename();
      string dat_filename = vq->dat_filename();

      send_open_quest_file(c, bin_filename, bin_filename, "", vq->quest_number, QuestFileType::ONLINE, vq->bin_contents);
      send_open_quest_file(c, dat_filename, dat_filename, "", vq->quest_number, QuestFileType::ONLINE, vq->dat_contents);
      c->set_flag(Client::Flag::LOADING_RUNNING_JOINABLE_QUEST);
      c->log.info_f("LOADING_RUNNING_JOINABLE_QUEST flag set");
      should_resume_game = false;

    } else if ((msg.command == 0x016F) && c->check_flag(Client::Flag::LOADING_RUNNING_JOINABLE_QUEST)) {
      c->clear_flag(Client::Flag::LOADING_RUNNING_JOINABLE_QUEST);
      c->log.info_f("LOADING_RUNNING_JOINABLE_QUEST flag cleared");
    }
    send_rare_enemy_index_list(c, l->map_state->bb_rare_enemy_indexes);
  }

  // We should resume the game if:
  // - command is 016F and a joinable quest is in progress
  // - command is 006F and a joinable quest is NOT in progress
  if (should_resume_game) {
    send_resume_game(l, c);
  }

  // Handle initial commands for spectator teams
  auto watched_lobby = l->watched_lobby.lock();
  if (l->battle_player && l->check_flag(Lobby::Flag::START_BATTLE_PLAYER_IMMEDIATELY)) {
    l->battle_player->start();
  } else if (watched_lobby && watched_lobby->ep3_server) {
    if (!watched_lobby->ep3_server->battle_finished) {
      watched_lobby->ep3_server->send_commands_for_joining_spectator(c->channel);
    }
    send_ep3_update_game_metadata(watched_lobby);
  }

  // If there are more players to bring in, try to do so
  c->disconnect_hooks.erase(ADD_NEXT_CLIENT_DISCONNECT_HOOK_NAME);
  add_next_game_client(l);
}

static asio::awaitable<void> on_99(shared_ptr<Client> c, Channel::Message& msg) {
  check_size_v(msg.data.size(), 0);

  // This is an odd place to send 6xB4x52, but there's a reason for it. If the
  // client receives 6xB4x52 while it's loading the battlefield, it won't set
  // the spectator count or top-bar text. But the client doesn't send anything
  // when it's done loading the battlefield, so we have to have some other way
  // of knowing when it's ready. We do this by sending a B1 (server time)
  // command immediately after the E8 (join spectator team) command, which
  // allows us to delay sending the 6xB4x52 until the server responds with a 99
  // command after loading is done.
  auto l = c->lobby.lock();
  if (l && l->is_game() && (l->episode == Episode::EP3) && l->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM)) {
    auto watched_l = l->watched_lobby.lock();
    if (watched_l) {
      send_ep3_update_game_metadata(watched_l);
    }
  }

  // See the comment in on_6F about why we do this here, but only for non-BB
  // versions.
  if (l && l->check_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS) && (c->version() != Version::BB_V4)) {
    if (!l->quest) {
      throw runtime_error("JOINABLE_QUEST_IN_PROGRESS is set, but lobby has no quest");
    }
    auto vq = l->quest->version(c->version(), c->language());
    if (!vq) {
      throw runtime_error("JOINABLE_QUEST_IN_PROGRESS is set, but lobby has no quest for client version");
    }
    string bin_filename = vq->bin_filename();
    string dat_filename = vq->dat_filename();

    send_open_quest_file(c, bin_filename, bin_filename, "", vq->quest_number, QuestFileType::ONLINE, vq->bin_contents);
    send_open_quest_file(c, dat_filename, dat_filename, "", vq->quest_number, QuestFileType::ONLINE, vq->dat_contents);
    c->set_flag(Client::Flag::LOADING_RUNNING_JOINABLE_QUEST);
    c->log.info_f("LOADING_RUNNING_JOINABLE_QUEST flag set");

    // On v1 and v2, there is no confirmation when the client is done
    // downloading the quest file, so just set the in-quest state immediately.
    // On v3 and later, we do this when we receive the AC command.
    // TODO: This might not work for GC NTE, since we wait for file chunk
    // confirmations (13 commands) but there is no AC command.
    if (is_v1_or_v2(c->version())) {
      on_joinable_quest_loaded(c);
    }
  }

  // If enable_save_if_needed is waiting, unblock it.
  if (c->enable_save_promise) {
    c->enable_save_promise->set_value();
    c->enable_save_promise.reset();
  }
  co_return;
}

static asio::awaitable<void> on_D0_V3_BB(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& cmd = check_size_t<SC_TradeItems_D0_D3>(msg.data);

  if (c->pending_item_trade) {
    throw runtime_error("player started a trade when one is already pending");
  }
  if (cmd.item_count > 0x20) {
    throw runtime_error("invalid item count in trade items command");
  }

  auto l = c->require_lobby();
  if (!l->is_game()) {
    throw runtime_error("trade command received in non-game lobby");
  }
  auto target_c = l->clients.at(cmd.target_client_id);
  if (!target_c) {
    throw runtime_error("trade command sent to missing player");
  }

  c->pending_item_trade = make_unique<Client::PendingItemTrade>();
  c->pending_item_trade->other_client_id = cmd.target_client_id;
  for (size_t x = 0; x < cmd.item_count; x++) {
    auto& item = c->pending_item_trade->items.emplace_back(cmd.item_datas[x]);
    item.decode_for_version(c->version());
  }

  // If the other player has a pending trade as well, assume this is the second
  // half of the trade sequence, and send a D1 to both clients (which should
  // cause them to delete the appropriate inventory items and send D2s). If the
  // other player does not have a pending trade, assume this is the first half
  // of the trade sequence, and send a D1 only to the target player (to request
  // its D0 command).
  // See the description of the D0 command in CommandFormats.hh for more
  // information on how this sequence is supposed to work.
  send_command(target_c, 0xD1, 0x00);
  if (target_c->pending_item_trade) {
    send_command(c, 0xD1, 0x00);
  }
  co_return;
}

static asio::awaitable<void> on_D2_V3_BB(shared_ptr<Client> c, Channel::Message& msg) {
  check_size_v(msg.data.size(), 0);

  if (!c->pending_item_trade) {
    throw runtime_error("player executed a trade with none pending");
  }

  auto l = c->require_lobby();
  if (!l->is_game()) {
    throw runtime_error("trade command received in non-game lobby");
  }
  auto target_c = l->clients.at(c->pending_item_trade->other_client_id);
  if (!target_c) {
    throw runtime_error("target player is missing");
  }
  if (!target_c->pending_item_trade) {
    throw runtime_error("player executed a trade with no other side pending");
  }

  auto s = c->require_server_state();
  auto complete_trade_for_side = +[](shared_ptr<Client> to_c, shared_ptr<Client> from_c) {
    auto l = to_c->require_lobby();
    auto s = to_c->require_server_state();

    if (to_c->version() == Version::BB_V4) {
      // On BB, the server is expected to generate the delete item and create
      // item commands
      auto to_p = to_c->character();
      auto from_p = from_c->character();
      for (const auto& trade_item : from_c->pending_item_trade->items) {
        size_t amount = trade_item.stack_size(*s->item_stack_limits(from_c->version()));

        auto item = from_p->remove_item(trade_item.id, amount, *s->item_stack_limits(from_c->version()));
        // This is a special case: when the trade is executed, the client
        // deletes the traded items from its own inventory automatically, so we
        // should NOT send the 6x29 to that client; we should only send it to
        // the other clients in the game.
        G_DeleteInventoryItem_6x29 cmd = {{0x29, 0x03, from_c->lobby_client_id}, item.id, amount};
        for (auto lc : l->clients) {
          if (lc && (lc != from_c)) {
            send_command_t(l, 0x60, 0x00, cmd);
          }
        }

        to_p->add_item(trade_item, *s->item_stack_limits(to_c->version()));
        send_create_inventory_item_to_lobby(to_c, to_c->lobby_client_id, item);
      }
      send_command(to_c, 0xD3, 0x00);

    } else {
      // On V3, the clients will handle it; we just send their final trade lists
      // to each other
      send_execute_item_trade(to_c, from_c->pending_item_trade->items);
    }

    send_command(to_c, 0xD4, 0x01);
  };

  c->pending_item_trade->confirmed = true;
  if (target_c->pending_item_trade->confirmed) {
    complete_trade_for_side(c, target_c);
    complete_trade_for_side(target_c, c);
    c->pending_item_trade.reset();
    target_c->pending_item_trade.reset();
  }
  co_return;
}

static asio::awaitable<void> on_D4_V3_BB(shared_ptr<Client> c, Channel::Message& msg) {
  check_size_v(msg.data.size(), 0);

  // Annoyingly, if the other client disconnects at a certain point during the
  // trade sequence, the client can get into a state where it sends this command
  // many times in a row. To deal with this, we just do nothing if the client
  // has no trade pending.
  if (!c->pending_item_trade) {
    co_return;
  }
  uint8_t other_client_id = c->pending_item_trade->other_client_id;
  c->pending_item_trade.reset();
  send_command(c, 0xD4, 0x00);

  // Cancel the other side of the trade too, if it's open
  auto l = c->require_lobby();
  if (!l->is_game()) {
    throw runtime_error("trade command received in non-game lobby");
  }
  auto target_c = l->clients.at(other_client_id);
  if (!target_c || !target_c->pending_item_trade) {
    co_return;
  }
  target_c->pending_item_trade.reset();
  send_command(target_c, 0xD4, 0x00);
}

static asio::awaitable<void> on_EE_Ep3(shared_ptr<Client> c, Channel::Message& msg) {
  if (!is_ep3(c->version())) {
    throw runtime_error("non-Ep3 client sent card trade command");
  }
  auto l = c->require_lobby();
  if (!l->is_game() || !l->is_ep3()) {
    throw runtime_error("client sent card trade command outside of Ep3 game");
  }

  if (msg.flag == 0xD0) {
    auto& cmd = check_size_t<SC_TradeCards_Ep3_EE_FlagD0_FlagD3>(msg.data);

    if (c->pending_card_trade) {
      throw runtime_error("player started a card trade when one is already pending");
    }
    if (cmd.entry_count > 4) {
      throw runtime_error("invalid entry count in card trade command");
    }

    auto target_c = l->clients.at(cmd.target_client_id);
    if (!target_c) {
      throw runtime_error("card trade command sent to missing player");
    }
    if (!is_ep3(target_c->version())) {
      throw runtime_error("card trade target is not Episode 3");
    }

    c->pending_card_trade = make_unique<Client::PendingCardTrade>();
    c->pending_card_trade->other_client_id = cmd.target_client_id;
    for (size_t x = 0; x < cmd.entry_count; x++) {
      c->pending_card_trade->card_to_count.emplace_back(
          make_pair(cmd.entries[x].card_type, cmd.entries[x].count));
    }

    // If the other player has a pending trade as well, assume this is the
    // second half of the trade sequence, and send an EE D1 to both clients. If
    // the other player does not have a pending trade, assume this is the first
    // half of the trade sequence, and send an EE D1 only to the target player
    // (to request its EE D0 command).
    // See the description of the D0 command in CommandFormats.hh for more
    // information on how this sequence is supposed to work. (The EE D0 command
    // is analogous to Episodes 1&2's D0 command.)
    S_AdvanceCardTradeState_Ep3_EE_FlagD1 resp = {0};
    send_command_t(target_c, 0xEE, 0xD1, resp);
    if (target_c->pending_card_trade) {
      send_command_t(c, 0xEE, 0xD1, resp);
    }

  } else if (msg.flag == 0xD2) {
    check_size_v(msg.data.size(), 0);

    if (!c->pending_card_trade) {
      throw runtime_error("player executed a card trade with none pending");
    }

    auto target_c = l->clients.at(c->pending_card_trade->other_client_id);
    if (!target_c) {
      throw runtime_error("card trade target player is missing");
    }
    if (!target_c->pending_card_trade) {
      throw runtime_error("player executed a card trade with no other side pending");
    }

    c->pending_card_trade->confirmed = true;
    if (target_c->pending_card_trade->confirmed) {
      send_execute_card_trade(c, target_c->pending_card_trade->card_to_count);
      send_execute_card_trade(target_c, c->pending_card_trade->card_to_count);
      S_CardTradeComplete_Ep3_EE_FlagD4 resp = {1};
      send_command_t(c, 0xEE, 0xD4, resp);
      send_command_t(target_c, 0xEE, 0xD4, resp);
      c->pending_card_trade.reset();
      target_c->pending_card_trade.reset();
    }

  } else if (msg.flag == 0xD4) {
    check_size_v(msg.data.size(), 0);

    // See the D4 handler for why this check exists (and why it doesn't throw)
    if (!c->pending_card_trade) {
      co_return;
    }
    uint8_t other_client_id = c->pending_card_trade->other_client_id;
    c->pending_card_trade.reset();
    S_CardTradeComplete_Ep3_EE_FlagD4 resp = {0};
    send_command_t(c, 0xEE, 0xD4, resp);

    // Cancel the other side of the trade too, if it's open
    auto target_c = l->clients.at(other_client_id);
    if (!target_c || !target_c->pending_card_trade) {
      co_return;
    }
    target_c->pending_card_trade.reset();
    send_command_t(target_c, 0xEE, 0xD4, resp);

  } else {
    throw runtime_error("invalid card trade operation");
  }
}

static asio::awaitable<void> on_EF_Ep3(shared_ptr<Client> c, Channel::Message& msg) {
  check_size_v(msg.data.size(), 0);

  if (!is_ep3(c->version())) {
    throw runtime_error("non-Ep3 client sent card auction request");
  }
  auto l = c->require_lobby();
  if (!l->is_game() || !l->is_ep3()) {
    throw runtime_error("client sent card auction request outside of Ep3 game");
  }

  send_ep3_card_auction(l);
  co_return;
}

static asio::awaitable<void> on_EA_BB(shared_ptr<Client> c, Channel::Message& msg) {
  auto s = c->require_server_state();

  switch (msg.command) {
    case 0x01EA: { // Create team
      const auto& cmd = check_size_t<C_CreateTeam_BB_01EA>(msg.data);
      string team_name = cmd.name.decode(c->language());
      if (s->team_index->get_by_name(team_name)) {
        send_command(c, 0x02EA, 0x00000002);
      } else if (c->login->account->bb_team_id != 0) {
        // TODO: What's the right error code to use here?
        send_command(c, 0x02EA, 0x00000001);
      } else {
        string player_name = c->character()->disp.name.decode(c->language());
        auto team = s->team_index->create(team_name, c->login->account->account_id, player_name);
        c->login->account->bb_team_id = team->team_id;
        c->login->account->save();

        send_command(c, 0x02EA, 0x00000000);
        send_team_metadata_change_notifications(s, team, c->login->account->account_id, TeamMetadataChange::TEAM_CREATED);
      }
      break;
    }
    case 0x03EA: { // Add team member
      auto team = c->team();
      if (team && team->members.at(c->login->account->account_id).privilege_level() >= 0x30) {
        const auto& cmd = check_size_t<C_AddOrRemoveTeamMember_BB_03EA_05EA>(msg.data);
        auto s = c->require_server_state();
        shared_ptr<Client> added_c;
        try {
          added_c = s->find_client(nullptr, cmd.guild_card_number);
        } catch (const out_of_range&) {
          send_command(c, 0x04EA, 0x00000006);
        }

        if (added_c && added_c->login) {
          auto added_c_team = added_c->team();
          if (added_c_team) {
            send_command(c, 0x04EA, 0x00000001);
            send_command(added_c, 0x04EA, 0x00000001);

          } else if (!team->can_add_member()) {
            // Send "team is full" error
            send_command(c, 0x04EA, 0x00000005);
            send_command(added_c, 0x04EA, 0x00000005);

          } else {
            added_c->login->account->bb_team_id = team->team_id;
            added_c->login->account->save();
            s->team_index->add_member(
                team->team_id,
                added_c->login->account->account_id,
                added_c->character()->disp.name.decode(added_c->language()));
            send_command(c, 0x04EA, 0x00000000);
            send_command(added_c, 0x04EA, 0x00000000);
            send_team_metadata_change_notifications(
                s, team, added_c->login->account->account_id, TeamMetadataChange::TEAM_MEMBER_COUNT);
          }
        }
      }
      break;
    }
    case 0x05EA: { // Remove team member
      auto team = c->team();
      if (team) {
        const auto& cmd = check_size_t<C_AddOrRemoveTeamMember_BB_03EA_05EA>(msg.data);
        bool is_removing_self = (cmd.guild_card_number == c->login->account->account_id);
        if (is_removing_self ||
            (team->members.at(c->login->account->account_id).privilege_level() >= 0x30)) {
          s->team_index->remove_member(cmd.guild_card_number);
          auto removed_account = s->account_index->from_account_id(cmd.guild_card_number);
          removed_account->bb_team_id = 0;
          removed_account->save();
          send_command(c, 0x06EA, 0x00000000);

          shared_ptr<Client> removed_c;
          if (is_removing_self) {
            removed_c = c;
          } else {
            try {
              removed_c = s->find_client(nullptr, cmd.guild_card_number);
            } catch (const out_of_range&) {
            }
          }
          uint32_t removed_account_id = (removed_c && removed_c->login) ? removed_c->login->account->account_id : 0;
          send_team_metadata_change_notifications(s, team, removed_account_id, TeamMetadataChange::TEAM_MEMBER_COUNT);
        } else {
          // TODO: Figure out the right error code to use here.
          send_command(c, 0x06EA, 0x00000001);
        }
      }
      break;
    }
    case 0x07EA: { // Team chat
      auto team = c->team();
      if (team) {
        check_size_v(msg.data.size(), sizeof(SC_TeamChat_BB_07EA) + 4, 0xFFFF);
        static const string required_end("\0\0", 2);
        if (msg.data.ends_with(required_end)) {
          for (const auto& it : team->members) {
            try {
              auto target_c = s->find_client(nullptr, it.second.account_id);
              send_command(target_c, 0x07EA, 0x00000000, msg.data);
            } catch (const out_of_range&) {
            }
          }
        }
      }
      break;
    }
    case 0x08EA:
      send_team_member_list(c);
      break;
    case 0x0DEA: {
      auto team = c->team();
      if (team) {
        S_TeamName_BB_0EEA cmd;
        cmd.team_name.encode(team->name, c->language());
        send_command_t(c, 0x0EEA, 0x00000000, cmd);
      } else {
        throw runtime_error("client is not in a team");
      }
      break;
    }
    case 0x0FEA: { // Set team flag
      auto team = c->team();
      if (team && team->members.at(c->login->account->account_id).check_flag(TeamIndex::Team::Member::Flag::IS_MASTER)) {
        const auto& cmd = check_size_t<C_SetTeamFlag_BB_0FEA>(msg.data);
        s->team_index->set_flag_data(team->team_id, cmd.flag_data);
        send_team_metadata_change_notifications(s, team, 0, TeamMetadataChange::FLAG_DATA);
      }
      break;
    }
    case 0x10EA: { // Disband team
      auto team = c->team();
      if (team && team->members.at(c->login->account->account_id).check_flag(TeamIndex::Team::Member::Flag::IS_MASTER)) {
        s->team_index->disband(team->team_id);
        send_command(c, 0x10EA, 0x00000000);
        send_team_metadata_change_notifications(s, team, 0, TeamMetadataChange::TEAM_DISBANDED);
      }
      break;
    }
    case 0x11EA: { // Change member privilege level
      auto team = c->team();
      if (team) {
        auto& cmd = check_size_t<C_ChangeTeamMemberPrivilegeLevel_BB_11EA>(msg.data);
        if (cmd.guild_card_number == c->login->account->account_id) {
          throw runtime_error("this command cannot be used to modify your own permissions");
        }

        // The client only sends this command with flag = 0x00, 0x30, or 0x40
        switch (msg.flag) {
          case 0x00: // Demote member
            if (s->team_index->demote_leader(c->login->account->account_id, cmd.guild_card_number)) {
              send_command(c, 0x11EA, 0x00000000);
              send_team_metadata_change_notifications(s, team, cmd.guild_card_number, 0);
            } else {
              send_command(c, 0x11EA, 0x00000005);
            }
            break;
          case 0x30: // Promote member
            if (s->team_index->promote_leader(c->login->account->account_id, cmd.guild_card_number)) {
              send_command(c, 0x11EA, 0x00000000);
              send_team_metadata_change_notifications(s, team, cmd.guild_card_number, 0);
            } else {
              send_command(c, 0x11EA, 0x00000005);
            }
            break;
          case 0x40: // Transfer master
            s->team_index->change_master(c->login->account->account_id, cmd.guild_card_number);
            send_command(c, 0x11EA, 0x00000000);
            send_team_metadata_change_notifications(s, team, cmd.guild_card_number, TeamMetadataChange::TEAM_MASTER);
            break;
          default:
            throw runtime_error("invalid privilege level");
        }
      }
      break;
    }
    case 0x13EA:
      send_all_nearby_team_metadatas_to_client(c, true);
      break;
    case 0x14EA:
      send_all_nearby_team_metadatas_to_client(c, false);
      break;
    case 0x18EA: // Ranking information
      send_intra_team_ranking(c);
      break;
    case 0x19EA: // List purchased team rewards
    case 0x1AEA: // List team rewards available for purchase
      send_team_reward_list(c, (msg.command == 0x19EA));
      break;
    case 0x1BEA: { // Buy team reward
      auto team = c->team();
      if (team) {
        check_size_v(msg.data.size(), 0); // No data should be sent
        const auto& reward = s->team_index->reward_definitions().at(msg.flag);

        for (const auto& key : reward.prerequisite_keys) {
          if (!team->has_reward(key)) {
            throw runtime_error("not all prerequisite rewards have been purchased");
          }
        }
        if (reward.is_unique && team->has_reward(reward.key)) {
          throw runtime_error("team reward already purchased");
        }

        s->team_index->buy_reward(team->team_id, reward.key, reward.team_points, reward.reward_flag);

        if (reward.reward_flag != TeamIndex::Team::RewardFlag::NONE) {
          send_team_metadata_change_notifications(s, team, 0, TeamMetadataChange::REWARD_FLAGS);
        }
        if (!reward.reward_item.empty()) {
          c->current_bank().add_item(reward.reward_item, *s->item_stack_limits(c->version()));
        }
      }
      break;
    }
    case 0x1CEA:
      send_cross_team_ranking(c);
      break;
    case 0x1EEA: {
      const auto& cmd = check_size_t<C_RenameTeam_BB_1EEA>(msg.data);
      auto team = c->team();
      string new_team_name = cmd.new_team_name.decode(c->language());
      if (!team) {
        // TODO: What's the right error code to use here?
        send_command(c, 0x1FEA, 0x00000001);
      } else if (s->team_index->get_by_name(new_team_name)) {
        send_command(c, 0x1FEA, 0x00000002);
      } else {
        s->team_index->rename(team->team_id, new_team_name);
        send_command(c, 0x1FEA, 0x00000000);
        send_team_metadata_change_notifications(s, team, 0, TeamMetadataChange::TEAM_NAME);
      }
      break;
    }
    default:
      throw runtime_error("invalid team command");
  }
  co_return;
}

static asio::awaitable<void> on_ignored(shared_ptr<Client>, Channel::Message&) {
  co_return;
}

typedef asio::awaitable<void> (*on_command_t)(shared_ptr<Client> c, Channel::Message& msg);

// Command handler table, indexed by command number and game version. Null
// entries in this table cause on_unimplemented_command to be called, which
// disconnects the client.
static_assert(NUM_VERSIONS == 14, "Don\'t forget to update the ReceiveCommands handler table");
static on_command_t handlers[0x100][NUM_VERSIONS] = {
    // clang-format off
//        nullptr, nullptr, DC_NTE          DC_112000       DCV1            DCV2            PC_NTE       PC           GCNTE           GC              EP3TE           EP3             XB              BB
/* 00 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 01 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 02 */ {on_02_U, on_02_U, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 03 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 04 */ {on_04_U, on_04_U, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 05 */ {nullptr, nullptr, on_ignored,     on_ignored,     on_ignored,     on_ignored,     on_ignored,  on_ignored,  on_ignored,     on_ignored,     on_ignored,     on_ignored,     on_05_XB,       on_ignored},
/* 06 */ {nullptr, nullptr, on_06,          on_06,          on_06,          on_06,          on_06,       on_06,       on_06,          on_06,          on_06,          on_06,          on_06,          on_06},
/* 07 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 08 */ {nullptr, nullptr, on_08_E6,       on_08_E6,       on_08_E6,       on_08_E6,       on_08_E6,    on_08_E6,    on_08_E6,       on_08_E6,       on_08_E6,       on_08_E6,       on_08_E6,       on_08_E6},
/* 09 */ {nullptr, nullptr, on_09,          on_09,          on_09,          on_09,          on_09,       on_09,       on_09,          on_09,          on_09,          on_09,          on_09,          on_09},
/* 0A */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 0B */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 0C */ {nullptr, nullptr, on_0C_C1_E7_EC, on_0C_C1_E7_EC, on_0C_C1_E7_EC, on_0C_C1_E7_EC, nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 0D */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 0E */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 0F */ {on_0F_U, on_0F_U, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
//        PC_PATCH BB_PATCH DC_NTE          DC_PROTO        DCV1            DCV2            PC-NTE       PC           GCNTE           GC              EP3TE           EP3             XB              BB
/* 10 */ {on_10_U, on_10_U, on_10,          on_10,          on_10,          on_10,          on_10,       on_10,       on_10,          on_10,          on_10,          on_10,          on_10,          on_10},
/* 11 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 12 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 13 */ {nullptr, nullptr, on_ignored,     on_ignored,     on_ignored,     on_ignored,     on_ignored,  on_ignored,  on_13_A7_V3_V4, on_13_A7_V3_V4, on_13_A7_V3_V4, on_13_A7_V3_V4, on_13_A7_V3_V4, on_13_A7_V3_V4},
/* 14 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 15 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 16 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 17 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 18 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 19 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 1A */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 1B */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 1C */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 1D */ {nullptr, nullptr, on_1D,          on_1D,          on_1D,          on_1D,          on_1D,       on_1D,       on_1D,          on_1D,          on_1D,          on_1D,          on_1D,          on_1D},
/* 1E */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 1F */ {nullptr, nullptr, on_1F,          on_1F,          on_1F,          on_1F,          on_1F,       on_1F,       nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
//        PC_PATCH BB_PATCH DC_NTE          DC_PROTO        DCV1            DCV2            PC-NTE       PC           GCNTE           GC              EP3TE           EP3             XB              BB
/* 20 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 21 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 22 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        on_ignored},
/* 23 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 24 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 25 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 26 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 27 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 28 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 29 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 2A */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 2B */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 2C */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 2D */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 2E */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 2F */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
//        PC_PATCH BB_PATCH DC_NTE          DC_PROTO        DCV1            DCV2            PC-NTE       PC           GCNTE           GC              EP3TE           EP3             XB              BB
/* 30 */ {nullptr, nullptr, on_30,          on_30,          on_30,          on_30,          on_30,       on_30,       on_30,          on_30,          on_30,          on_30,          on_30,          on_30},
/* 31 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 32 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 33 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 34 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 35 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 36 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 37 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 38 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 39 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 3A */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 3B */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 3C */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 3D */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 3E */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 3F */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
//        PC_PATCH BB_PATCH DC_NTE          DC_PROTO        DCV1            DCV2            PC-NTE       PC           GCNTE           GC              EP3TE           EP3             XB              BB
/* 40 */ {nullptr, nullptr, on_40,          on_40,          on_40,          on_40,          on_40,       on_40,       on_40,          on_40,          on_40,          on_40,          on_40,          on_40},
/* 41 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 42 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 43 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 44 */ {nullptr, nullptr, on_ignored,     on_ignored,     on_ignored,     on_ignored,     on_ignored,  on_ignored,  on_ignored,     on_ignored,     on_ignored,     on_ignored,     on_ignored,     on_ignored},
/* 45 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 46 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 47 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 48 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 49 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 4A */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 4B */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 4C */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 4D */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 4E */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 4F */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
//        PC_PATCH BB_PATCH DC_NTE          DC_PROTO        DCV1            DCV2            PC-NTE       PC           GCNTE           GC              EP3TE           EP3             XB              BB
/* 50 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 51 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 52 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 53 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 54 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 55 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 56 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 57 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 58 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 59 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 5A */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 5B */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 5C */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 5D */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 5E */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 5F */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
//        PC_PATCH BB_PATCH DC_NTE          DC_PROTO        DCV1            DCV2            PC-NTE       PC           GCNTE           GC              EP3TE           EP3             XB              BB
/* 60 */ {nullptr, nullptr, on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB, on_6x_C9_CB, on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB},
/* 61 */ {nullptr, nullptr, on_61_98,       on_61_98,       on_61_98,       on_61_98,       on_61_98,    on_61_98,    on_61_98,       on_61_98,       on_61_98,       on_61_98,       on_61_98,       on_61_98},
/* 62 */ {nullptr, nullptr, on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB, on_6x_C9_CB, on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB},
/* 63 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 64 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 65 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 66 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 67 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 68 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 69 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 6A */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 6B */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 6C */ {nullptr, nullptr, on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB, on_6x_C9_CB, on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB},
/* 6D */ {nullptr, nullptr, on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB, on_6x_C9_CB, on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB},
/* 6E */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 6F */ {nullptr, nullptr, on_6F,          on_6F,          on_6F,          on_6F,          on_6F,       on_6F,       on_6F,          on_6F,          on_6F,          on_6F,          on_6F,          on_6F},
//        PC_PATCH BB_PATCH DC_NTE          DC_PROTO        DCV1            DCV2            PC-NTE       PC           GCNTE           GC              EP3TE           EP3             XB              BB
/* 70 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 71 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 72 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 73 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 74 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 75 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 76 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 77 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 78 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 79 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 7A */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 7B */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 7C */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 7D */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 7E */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 7F */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
//        PC_PATCH BB_PATCH DC_NTE          DC_PROTO        DCV1            DCV2            PC-NTE       PC           GCNTE           GC              EP3TE           EP3             XB              BB
/* 80 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 81 */ {nullptr, nullptr, on_81,          on_81,          on_81,          on_81,          on_81,       on_81,       on_81,          on_81,          on_81,          on_81,          on_81,          on_81},
/* 82 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 83 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 84 */ {nullptr, nullptr, on_84,          on_84,          on_84,          on_84,          on_84,       on_84,       on_84,          on_84,          on_84,          on_84,          on_84,          on_84},
/* 85 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 86 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 87 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 88 */ {nullptr, nullptr, on_88_DCNTE,    on_88_DCNTE,    on_88_DCNTE,    on_88_DCNTE,    nullptr,     nullptr,     on_88_DCNTE,    on_88_DCNTE,    on_88_DCNTE,    on_88_DCNTE,    nullptr,        nullptr},
/* 89 */ {nullptr, nullptr, on_89,          on_89,          on_89,          on_89,          on_89,       on_89,       on_89,          on_89,          on_89,          on_89,          on_89,          on_89},
/* 8A */ {nullptr, nullptr, on_8A,          on_8A,          on_8A,          on_8A,          on_8A,       on_8A,       on_8A,          on_8A,          on_8A,          on_8A,          on_8A,          on_8A},
/* 8B */ {nullptr, nullptr, on_8B_DCNTE,    on_8B_DCNTE,    on_8B_DCNTE,    on_8B_DCNTE,    nullptr,     nullptr,     on_8B_DCNTE,    on_8B_DCNTE,    on_8B_DCNTE,    on_8B_DCNTE,    nullptr,        nullptr},
/* 8C */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 8D */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 8E */ {nullptr, nullptr, on_8E_DCNTE,    on_8E_DCNTE,    on_8E_DCNTE,    on_8E_DCNTE,    nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 8F */ {nullptr, nullptr, on_8F_DCNTE,    on_8F_DCNTE,    on_8F_DCNTE,    on_8F_DCNTE,    nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
//        PC_PATCH BB_PATCH DC_NTE          DC_PROTO        DCV1            DCV2            PC-NTE       PC           GCNTE           GC              EP3TE           EP3             XB              BB
/* 90 */ {nullptr, nullptr, on_90_DC,       on_90_DC,       on_90_DC,       on_90_DC,       nullptr,     nullptr,     on_90_DC,       on_90_DC,       on_90_DC,       on_90_DC,       nullptr,        nullptr},
/* 91 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 92 */ {nullptr, nullptr, on_92_DC,       on_92_DC,       on_92_DC,       on_92_DC,       nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 93 */ {nullptr, nullptr, on_93_DC,       on_93_DC,       on_93_DC,       on_93_DC,       nullptr,     nullptr,     on_93_DC,       on_93_DC,       on_93_DC,       on_93_DC,       nullptr,        on_93_BB},
/* 94 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 95 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 96 */ {nullptr, nullptr, on_96,          on_96,          on_96,          on_96,          on_96,       on_96,       on_96,          on_96,          on_96,          on_96,          on_96,          nullptr},
/* 97 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 98 */ {nullptr, nullptr, on_61_98,       on_61_98,       on_61_98,       on_61_98,       on_61_98,    on_61_98,    on_61_98,       on_61_98,       on_61_98,       on_61_98,       on_61_98,       on_61_98},
/* 99 */ {nullptr, nullptr, on_99,          on_99,          on_99,          on_99,          on_99,       on_99,       on_99,          on_99,          on_99,          on_99,          on_99,          on_99},
/* 9A */ {nullptr, nullptr, on_9A,          on_9A,          on_9A,          on_9A,          on_9A,       on_9A,       on_9A,          on_9A,          on_9A,          on_9A,          nullptr,        nullptr},
/* 9B */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* 9C */ {nullptr, nullptr, on_9C,          on_9C,          on_9C,          on_9C,          on_9C,       on_9C,       on_9C,          on_9C,          on_9C,          on_9C,          on_9C,          nullptr},
/* 9D */ {nullptr, nullptr, on_9D_9E,       on_9D_9E,       on_9D_9E,       on_9D_9E,       on_9D_9E,    on_9D_9E,    on_9D_9E,       on_9D_9E,       on_9D_9E,       on_9D_9E,       on_9D_9E,       nullptr},
/* 9E */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        on_9D_9E,    on_9D_9E,    on_9D_9E,       on_9D_9E,       on_9D_9E,       on_9D_9E,       on_9E_XB,       nullptr},
/* 9F */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
//        PC_PATCH BB_PATCH DC_NTE          DC_PROTO        DCV1            DCV2            PC-NTE       PC           GCNTE           GC              EP3TE           EP3             XB              BB
/* A0 */ {nullptr, nullptr, on_A0,          on_A0,          on_A0,          on_A0,          on_A0,       on_A0,       on_A0,          on_A0,          on_A0,          on_A0,          on_A0,          on_A0},
/* A1 */ {nullptr, nullptr, on_A1,          on_A1,          on_A1,          on_A1,          on_A1,       on_A1,       on_A1,          on_A1,          on_A1,          on_A1,          on_A1,          on_A1},
/* A2 */ {nullptr, nullptr, on_A2,          on_A2,          on_A2,          on_A2,          on_A2,       on_A2,       on_A2,          on_A2,          on_A2,          on_A2,          on_A2,          on_A2},
/* A3 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* A4 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* A5 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* A6 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     on_ignored,     on_ignored,     on_ignored,     on_ignored,     on_ignored,     nullptr},
/* A7 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     on_13_A7_V3_V4, on_13_A7_V3_V4, on_13_A7_V3_V4, on_13_A7_V3_V4, on_13_A7_V3_V4, on_13_A7_V3_V4},
/* A8 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* A9 */ {nullptr, nullptr, on_A9,          on_A9,          on_A9,          on_A9,          on_A9,       on_A9,       on_A9,          on_A9,          on_A9,          on_A9,          on_A9,          on_A9},
/* AA */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        on_AA,       on_AA,       on_AA,          on_AA,          on_AA,          on_AA,          on_AA,          on_AA},
/* AB */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* AC */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     on_AC_V3_BB,    on_AC_V3_BB,    on_AC_V3_BB,    on_AC_V3_BB,    on_AC_V3_BB,    on_AC_V3_BB},
/* AD */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* AE */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* AF */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
//        PC_PATCH BB_PATCH DC_NTE          DC_PROTO        DCV1            DCV2            PC-NTE       PC           GCNTE           GC              EP3TE           EP3             XB              BB
/* B0 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* B1 */ {nullptr, nullptr, on_B1,          on_B1,          on_B1,          on_B1,          on_B1,       on_B1,       on_B1,          on_B1,          on_B1,          on_B1,          on_B1,          nullptr},
/* B2 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* B3 */ {nullptr, nullptr, on_B3,          on_B3,          on_B3,          on_B3,          on_B3,       on_B3,       on_B3,          on_B3,          on_B3,          on_B3,          on_B3,          on_B3},
/* B4 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* B5 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* B6 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* B7 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        on_B7_Ep3,      on_B7_Ep3,      nullptr,        nullptr},
/* B8 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        on_ignored,     on_ignored,     nullptr,        nullptr},
/* B9 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        on_ignored,     on_ignored,     nullptr,        nullptr},
/* BA */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        on_BA_Ep3,      on_BA_Ep3,      nullptr,        nullptr},
/* BB */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* BC */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* BD */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* BE */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* BF */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
//        PC_PATCH BB_PATCH DC_NTE          DC_PROTO        DCV1            DCV2            PC-NTE       PC           GCNTE           GC              EP3TE           EP3             XB              BB
/* C0 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        on_C0,          on_C0,       on_C0,       on_C0,          on_C0,          on_C0,          on_C0,          on_C0,          on_C0},
/* C1 */ {nullptr, nullptr, on_0C_C1_E7_EC, on_0C_C1_E7_EC, on_0C_C1_E7_EC, on_0C_C1_E7_EC, on_C1_PC,    on_C1_PC,    on_0C_C1_E7_EC, on_0C_C1_E7_EC, on_0C_C1_E7_EC, on_0C_C1_E7_EC, on_0C_C1_E7_EC, on_C1_BB},
/* C2 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        on_C2,          on_C2,       on_C2,       on_C2,          on_C2,          on_C2,          on_C2,          on_C2,          on_C2},
/* C3 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        on_C3,          on_C3,       on_C3,       on_C3,          on_C3,          on_C3,          on_C3,          on_C3,          on_C3},
/* C4 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* C5 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* C6 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        on_C6,       on_C6,       on_C6,          on_C6,          on_C6,          on_C6,          on_C6,          on_C6},
/* C7 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        on_C7,       on_C7,       on_C7,          on_C7,          on_C7,          on_C7,          on_C7,          on_C7},
/* C8 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        on_C8,       on_C8,       on_C8,          on_C8,          on_C8,          on_C8,          on_C8,          on_C8},
/* C9 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        on_6x_C9_CB,    on_6x_C9_CB,    on_C9_XB,       nullptr},
/* CA */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        on_CA_Ep3,      on_CA_Ep3,      nullptr,        nullptr},
/* CB */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        on_6x_C9_CB,    on_6x_C9_CB,    nullptr,        nullptr},
/* CC */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* CD */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* CE */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* CF */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
//        PC_PATCH BB_PATCH DC_NTE          DC_PROTO        DCV1            DCV2            PC-NTE       PC           GCNTE           GC              EP3TE           EP3             XB              BB
/* D0 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     on_D0_V3_BB,    on_D0_V3_BB,    on_D0_V3_BB,    on_D0_V3_BB,    on_D0_V3_BB,    on_D0_V3_BB},
/* D1 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* D2 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     on_D2_V3_BB,    on_D2_V3_BB,    on_D2_V3_BB,    on_D2_V3_BB,    on_D2_V3_BB,    on_D2_V3_BB},
/* D3 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* D4 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     on_D4_V3_BB,    on_D4_V3_BB,    on_D4_V3_BB,    on_D4_V3_BB,    on_D4_V3_BB,    on_D4_V3_BB},
/* D5 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* D6 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     on_D6_V3,       on_D6_V3,       on_D6_V3,       on_D6_V3,       on_D6_V3,       nullptr},
/* D7 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     on_D7_GC,       on_D7_GC,       on_D7_GC,       on_D7_GC,       on_D7_GC,       nullptr},
/* D8 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        on_D8,       on_D8,       on_D8,          on_D8,          on_D8,          on_D8,          on_D8,          on_D8},
/* D9 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        on_D9,       on_D9,       on_D9,          on_D9,          on_D9,          on_D9,          on_D9,          on_D9},
/* DA */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* DB */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     on_DB_V3,       on_DB_V3,       on_DB_V3,       on_DB_V3,       on_DB_V3,       nullptr},
/* DC */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        on_DC_Ep3,      on_DC_Ep3,      nullptr,        on_DC_BB},
/* DD */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* DE */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* DF */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        on_DF_BB},
//        PC_PATCH BB_PATCH DC_NTE          DC_PROTO        DCV1            DCV2            PC-NTE       PC           GCNTE           GC              EP3TE           EP3             XB              BB
/* E0 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        on_E0_BB},
/* E1 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* E2 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        on_E2_Ep3,      on_E2_Ep3,      nullptr,        on_E2_BB},
/* E3 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        on_E3_BB},
/* E4 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        on_E4_Ep3,      on_E4_Ep3,      nullptr,        nullptr},
/* E5 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        on_E5_Ep3,      on_E5_Ep3,      nullptr,        on_E5_BB},
/* E6 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        on_08_E6,       on_08_E6,       nullptr,        nullptr},
/* E7 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        on_0C_C1_E7_EC, on_0C_C1_E7_EC, nullptr,        on_E7_BB},
/* E8 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        on_E8_BB},
/* E9 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* EA */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        on_EA_BB},
/* EB */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        on_EB_BB},
/* EC */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        on_0C_C1_E7_EC, on_0C_C1_E7_EC, nullptr,        on_EC_BB},
/* ED */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        on_ED_BB},
/* EE */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        on_EE_Ep3,      on_EE_Ep3,      nullptr,        nullptr},
/* EF */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        on_EF_Ep3,      on_EF_Ep3,      nullptr,        nullptr},
//        PC_PATCH BB_PATCH DC_NTE          DC_PROTO        DCV1            DCV2            PC-NTE       PC           GCNTE           GC              EP3TE           EP3             XB              BB
/* F0 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* F1 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* F2 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* F3 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* F4 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* F5 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* F6 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* F7 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* F8 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* F9 */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* FA */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* FB */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* FC */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* FD */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* FE */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
/* FF */ {nullptr, nullptr, nullptr,        nullptr,        nullptr,        nullptr,        nullptr,     nullptr,     nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr},
//        PC_PATCH BB_PATCH DC_NTE          DC_PROTO        DCV1            DCV2            PC-NTE       PC           GCNTE           GC              EP3TE           EP3             XB              BB
    // clang-format on
};

static void check_logged_out_command(Version version, uint8_t command) {
  switch (version) {
    case Version::PC_PATCH:
    case Version::BB_PATCH:
      break;
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
      // newserv doesn't actually know that DC clients are DC until it receives
      // an appropriate login command (93, 9A, or 9D), but those commands also
      // log the client in, so this case should never be executed.
      throw logic_error("cannot check logged-out command for DC client");
    case Version::PC_NTE:
    case Version::PC_V2:
      if (command != 0x9A && command != 0x9C && command != 0x9D) {
        throw runtime_error("only commands 9A, 9C, and 9D may be sent before login");
      }
      break;
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      // See comment in the DC case above for why DC commands are included here.
      if (command != 0x88 && // DC NTE
          command != 0x8B && // DC NTE
          command != 0x90 && // DC v1
          command != 0x93 && // DC v1
          command != 0x9A && // DC v2
          command != 0x9C && // DC v2, GC
          command != 0x9D && // DC v2, GC trial edition
          command != 0x9E && // GC non-trial
          command != 0xDB) { // GC non-trial
        throw runtime_error("only commands 88, 8B, 90, 93, 9A, 9C, 9D, 9E, and DB may be sent before login");
      }
      break;
    case Version::XB_V3:
      if (command != 0x9E && command != 0x9F) {
        throw runtime_error("only commands 9E and 9F may be sent before login");
      }
      break;
    case Version::BB_V4:
      if (command != 0x93) {
        throw runtime_error("only command 93 may be sent before login");
      }
      break;
    default:
      throw logic_error("invalid game version");
  }
}

asio::awaitable<void> on_command(shared_ptr<Client> c, unique_ptr<Channel::Message> msg) {
  c->reschedule_ping_and_timeout_timers();

  // Most of the command handlers assume the client is registered, logged in,
  // and not banned (and therefore that c->login is not null), so the client is
  // allowed to access normal functionality. This check prevents clients from
  // sneakily sending commands to access functionality without logging in.
  if (!c->login) {
    check_logged_out_command(c->version(), msg->command);
  }

  if (c->proxy_session) {
    co_await on_proxy_command(c, false, std::move(msg));
  } else {
    auto fn = handlers[msg->command & 0xFF][static_cast<size_t>(c->version())];
    if (fn) {
      co_await fn(c, *msg);
    } else {
      c->log.warning_f("Unknown command: size={:04X} command={:04X} flag={:08X}", msg->data.size(), msg->command, msg->flag);
      throw invalid_argument("unimplemented command");
    }
  }
}

asio::awaitable<void> on_command_with_header(shared_ptr<Client> c, const string& data) {
  switch (c->version()) {
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
    case Version::XB_V3: {
      auto& header = check_size_t<PSOCommandHeaderDCV3>(data, 0xFFFF);
      auto msg = make_unique<Channel::Message>(Channel::Message{
          .command = header.command, .flag = header.flag, .data = data.substr(sizeof(header))});
      co_await on_command(c, std::move(msg));
      break;
    }
    case Version::PC_NTE:
    case Version::PC_V2: {
      auto& header = check_size_t<PSOCommandHeaderPC>(data, 0xFFFF);
      auto msg = make_unique<Channel::Message>(Channel::Message{
          .command = header.command, .flag = header.flag, .data = data.substr(sizeof(header))});
      co_await on_command(c, std::move(msg));
      break;
    }
    case Version::BB_V4: {
      auto& header = check_size_t<PSOCommandHeaderBB>(data, 0xFFFF);
      auto msg = make_unique<Channel::Message>(Channel::Message{
          .command = header.command, .flag = header.flag, .data = data.substr(sizeof(header))});
      co_await on_command(c, std::move(msg));
      break;
    }
    default:
      throw logic_error("unimplemented game version in on_command_with_header");
  }
}
