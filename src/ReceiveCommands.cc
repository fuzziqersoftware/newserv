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
#include "ItemCreator.hh"
#include "Loggers.hh"
#include "PSOProtocol.hh"
#include "ProxyServer.hh"
#include "ReceiveSubcommands.hh"
#include "SendCommands.hh"
#include "StaticGameData.hh"
#include "Text.hh"

using namespace std;

const char* BATTLE_TABLE_DISCONNECT_HOOK_NAME = "battle_table_state";
const char* QUEST_BARRIER_DISCONNECT_HOOK_NAME = "quest_barrier";
const char* ADD_NEXT_CLIENT_DISCONNECT_HOOK_NAME = "add_next_game_client";

static shared_ptr<const Menu> proxy_options_menu_for_client(shared_ptr<const Client> c) {
  auto s = c->require_server_state();

  shared_ptr<Menu> ret(new Menu(MenuID::PROXY_OPTIONS, "Proxy options"));
  ret->items.emplace_back(ProxyOptionsMenuItemID::GO_BACK, "Go back", "Return to the\nProxy Server menu", 0);

  auto add_bool_option = [&](uint32_t item_id, bool is_enabled, const char* text, const char* description) -> void {
    string option = is_enabled ? "* " : "- ";
    option += text;
    ret->items.emplace_back(item_id, option, description, 0);
  };
  auto add_option = [&](uint32_t item_id, Client::Flag flag, const char* text, const char* description) -> void {
    add_bool_option(item_id, c->config.check_flag(flag), text, description);
  };

  add_option(ProxyOptionsMenuItemID::CHAT_COMMANDS, Client::Flag::PROXY_CHAT_COMMANDS_ENABLED,
      "Chat commands", "Enable chat\ncommands");
  add_option(ProxyOptionsMenuItemID::CHAT_FILTER, Client::Flag::PROXY_CHAT_FILTER_ENABLED,
      "Chat filter", "Enable escape\nsequences in\nchat messages\nand info board");
  add_option(ProxyOptionsMenuItemID::PLAYER_NOTIFICATIONS, Client::Flag::PROXY_PLAYER_NOTIFICATIONS_ENABLED,
      "Player notifs", "Show a message\nwhen other players\njoin or leave");
  add_option(ProxyOptionsMenuItemID::BLOCK_PINGS, Client::Flag::PROXY_SUPPRESS_CLIENT_PINGS,
      "Block pings", "Block ping commands\nsent by the client");
  if (s->cheat_mode_behavior != ServerState::BehaviorSwitch::OFF) {
    if (!c->config.check_flag(Client::Flag::IS_EPISODE_3)) {
      add_option(ProxyOptionsMenuItemID::INFINITE_HP, Client::Flag::INFINITE_HP_ENABLED,
          "Infinite HP", "Enable automatic HP\nrestoration when\nyou are hit by an\nenemy or trap\n\nCannot revive you\nfrom one-hit kills");
      add_option(ProxyOptionsMenuItemID::INFINITE_TP, Client::Flag::INFINITE_TP_ENABLED,
          "Infinite TP", "Enable automatic TP\nrestoration when\nyou cast any\ntechnique");
      add_option(ProxyOptionsMenuItemID::SWITCH_ASSIST, Client::Flag::SWITCH_ASSIST_ENABLED,
          "Switch assist", "Automatically try\nto unlock 2-player\ndoors when you step\non both switches\nsequentially");
    } else {
      // Note: This option's text is the maximum possible length for any menu item
      add_option(ProxyOptionsMenuItemID::EP3_INFINITE_MESETA, Client::Flag::PROXY_EP3_INFINITE_MESETA_ENABLED,
          "Infinite Meseta", "Fix Meseta value\nat 1,000,000");
      add_option(ProxyOptionsMenuItemID::EP3_INFINITE_TIME, Client::Flag::PROXY_EP3_INFINITE_TIME_ENABLED,
          "Infinite time", "Disable overall and\nper-phase time limits\nin battle");
    }
  }
  add_bool_option(ProxyOptionsMenuItemID::BLOCK_EVENTS, (c->config.override_lobby_event != 0xFF),
      "Block events", "Disable seasonal\nevents in the lobby\nand in games");
  add_option(ProxyOptionsMenuItemID::BLOCK_PATCHES, Client::Flag::PROXY_BLOCK_FUNCTION_CALLS,
      "Block patches", "Disable patches sent\nby the remote server");
  if (s->proxy_allow_save_files) {
    add_option(ProxyOptionsMenuItemID::SAVE_FILES, Client::Flag::PROXY_SAVE_FILES,
        "Save files", "Save local copies of\nfiles from the\nremote server\n(quests, etc.)");
  }
  if (s->proxy_enable_login_options) {
    add_option(ProxyOptionsMenuItemID::RED_NAME, Client::Flag::PROXY_RED_NAME_ENABLED,
        "Red name", "Set the colors\nof your name and\nChallenge Mode\nrank to red");
    add_option(ProxyOptionsMenuItemID::BLANK_NAME, Client::Flag::PROXY_BLANK_NAME_ENABLED,
        "Blank name", "Suppress your\ncharacter name\nduring login");
    add_option(ProxyOptionsMenuItemID::SUPPRESS_LOGIN, Client::Flag::PROXY_SUPPRESS_REMOTE_LOGIN,
        "Skip login", "Use an alternate\nlogin sequence");
    add_option(ProxyOptionsMenuItemID::SKIP_CARD, Client::Flag::PROXY_ZERO_REMOTE_GUILD_CARD,
        "Skip card", "Use an alternate\nvalue for your initial\nGuild Card");
  }

  return ret;
}

static void send_client_to_lobby_server(shared_ptr<Client> c) {
  auto s = c->require_server_state();
  const auto& port_name = version_to_lobby_port_name.at(static_cast<size_t>(c->version()));
  send_reconnect(c, s->connect_address_for_client(c),
      s->name_to_port_config.at(port_name)->port);
}

static void send_client_to_proxy_server(shared_ptr<Client> c) {
  auto s = c->require_server_state();

  const auto& port_name = version_to_proxy_port_name.at(static_cast<size_t>(c->version()));
  uint16_t local_port = s->name_to_port_config.at(port_name)->port;

  s->proxy_server->delete_session(c->license->serial_number);
  auto ses = s->proxy_server->create_licensed_session(c->license, local_port, c->version(), c->config);
  if (!s->proxy_allow_save_files) {
    ses->config.clear_flag(Client::Flag::PROXY_SAVE_FILES);
  }
  if (!s->proxy_enable_login_options) {
    ses->config.clear_flag(Client::Flag::PROXY_SUPPRESS_REMOTE_LOGIN);
    ses->config.clear_flag(Client::Flag::PROXY_ZERO_REMOTE_GUILD_CARD);
  }
  if (ses->config.check_flag(Client::Flag::PROXY_ZERO_REMOTE_GUILD_CARD)) {
    ses->remote_guild_card_number = 0;
  }

  send_reconnect(c, s->connect_address_for_client(c), local_port);
}

static void send_proxy_destinations_menu(shared_ptr<Client> c) {
  auto s = c->require_server_state();
  send_menu(c, s->proxy_destinations_menu_for_version(c->version()));
}

static bool send_enable_send_function_call_if_applicable(shared_ptr<Client> c) {
  auto s = c->require_server_state();
  if (function_compiler_available() && c->config.check_flag(Client::Flag::USE_OVERFLOW_FOR_SEND_FUNCTION_CALL)) {
    if (s->ep3_send_function_call_enabled) {
      send_quest_buffer_overflow(c);
    } else {
      c->config.set_flag(Client::Flag::NO_SEND_FUNCTION_CALL);
    }
    c->config.clear_flag(Client::Flag::USE_OVERFLOW_FOR_SEND_FUNCTION_CALL);
    return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////

void on_connect(std::shared_ptr<Client> c) {
  switch (c->server_behavior) {
    case ServerBehavior::PC_CONSOLE_DETECT: {
      auto s = c->require_server_state();
      uint16_t pc_port = s->name_to_port_config.at("pc-login")->port;
      uint16_t console_port = s->name_to_port_config.at("console-login")->port;
      send_pc_console_split_reconnect(c, s->connect_address_for_client(c), pc_port, console_port);
      c->should_disconnect = true;
      break;
    }

    case ServerBehavior::LOGIN_SERVER:
      send_server_init(c, SendServerInitFlag::IS_INITIAL_CONNECTION);
      break;

    case ServerBehavior::PATCH_SERVER_BB:
      c->config.set_flag(Client::Flag::IS_BB_PATCH);
      send_server_init(c, 0);
      break;

    case ServerBehavior::PATCH_SERVER_PC:
    case ServerBehavior::DATA_SERVER_BB:
    case ServerBehavior::LOBBY_SERVER:
      send_server_init(c, 0);
      break;

    default:
      c->log.error("Unimplemented behavior: %" PRId64,
          static_cast<int64_t>(c->server_behavior));
  }
}

static void send_main_menu(shared_ptr<Client> c) {
  auto s = c->require_server_state();

  shared_ptr<Menu> main_menu(new Menu(MenuID::MAIN, s->name));
  main_menu->items.emplace_back(
      MainMenuItemID::GO_TO_LOBBY, "Go to lobby",
      [s, wc = weak_ptr<Client>(c)]() -> string {
        auto c = wc.lock();
        if (!c) {
          return "";
        }

        size_t num_players = 0;
        size_t num_games = 0;
        size_t num_compatible_games = 0;
        for (const auto& it : s->id_to_lobby) {
          const auto& l = it.second;
          if (l->is_game()) {
            num_games++;
            if (l->version_is_allowed(c->quest_version()) &&
                (l->is_ep3() == c->config.check_flag(Client::Flag::IS_EPISODE_3))) {
              num_compatible_games++;
            }
          }
          for (const auto& c : l->clients) {
            if (c) {
              num_players++;
            }
          }
        }
        return string_printf(
            "$C6%zu$C7 players online\n$C6%zu$C7 games\n$C6%zu$C7 compatible games",
            num_players, num_games, num_compatible_games);
      },
      0);
  main_menu->items.emplace_back(MainMenuItemID::INFORMATION, "Information",
      "View server\ninformation", MenuItem::Flag::INVISIBLE_ON_DCNTE | MenuItem::Flag::REQUIRES_MESSAGE_BOXES);

  uint32_t proxy_destinations_menu_item_flags =
      // DCNTE doesn't support multiple ship select menus without changing
      // servers (via a 19 command) apparently :(
      MenuItem::Flag::INVISIBLE_ON_DCNTE |
      (s->proxy_destinations_dc.empty() ? MenuItem::Flag::INVISIBLE_ON_DC : 0) |
      (s->proxy_destinations_pc.empty() ? MenuItem::Flag::INVISIBLE_ON_PC : 0) |
      (s->proxy_destinations_gc.empty() ? MenuItem::Flag::INVISIBLE_ON_GC : 0) |
      (s->proxy_destinations_xb.empty() ? MenuItem::Flag::INVISIBLE_ON_XB : 0) |
      MenuItem::Flag::INVISIBLE_ON_BB;
  main_menu->items.emplace_back(MainMenuItemID::PROXY_DESTINATIONS, "Proxy server",
      "Connect to another\nserver through the\nproxy", proxy_destinations_menu_item_flags);

  main_menu->items.emplace_back(MainMenuItemID::DOWNLOAD_QUESTS, "Download quests",
      "Download quests", MenuItem::Flag::INVISIBLE_ON_DCNTE | MenuItem::Flag::INVISIBLE_ON_BB);
  if (!s->is_replay) {
    if (!s->function_code_index->patch_menu_empty(c->config.specific_version)) {
      main_menu->items.emplace_back(MainMenuItemID::PATCHES, "Patches",
          "Change game\nbehaviors", MenuItem::Flag::GC_ONLY | MenuItem::Flag::REQUIRES_SEND_FUNCTION_CALL);
    }
    if (!s->dol_file_index->empty()) {
      main_menu->items.emplace_back(MainMenuItemID::PROGRAMS, "Programs",
          "Run GameCube\nprograms", MenuItem::Flag::GC_ONLY | MenuItem::Flag::REQUIRES_SEND_FUNCTION_CALL | MenuItem::Flag::REQUIRES_SAVE_DISABLED);
    }
  }
  main_menu->items.emplace_back(MainMenuItemID::DISCONNECT, "Disconnect",
      "Disconnect", 0);
  main_menu->items.emplace_back(MainMenuItemID::CLEAR_LICENSE, "Clear license",
      "Disconnect with an\ninvalid license error\nso you can enter a\ndifferent serial\nnumber, access key,\nor password",
      MenuItem::Flag::INVISIBLE_ON_DCNTE | MenuItem::Flag::INVISIBLE_ON_BB);

  send_menu(c, main_menu);
}

void on_login_complete(shared_ptr<Client> c) {
  // On the BB data server, this function is called only on the last connection
  // (when we should send the ship select menu).
  if ((c->server_behavior == ServerBehavior::LOGIN_SERVER) ||
      (c->server_behavior == ServerBehavior::DATA_SERVER_BB)) {
    auto s = c->require_server_state();

    // On the login server, send the events/songs, ep3 updates, and the main
    // menu or welcome message
    if (c->config.check_flag(Client::Flag::IS_EPISODE_3)) {
      if (s->ep3_menu_song >= 0) {
        send_ep3_change_music(c->channel, s->ep3_menu_song);
      } else if (s->pre_lobby_event) {
        send_change_event(c, s->pre_lobby_event);
      }

      send_ep3_rank_update(c);
      send_get_player_info(c);

    } else if (s->pre_lobby_event) {
      send_change_event(c, s->pre_lobby_event);
    }

    if (s->welcome_message.empty() ||
        c->config.check_flag(Client::Flag::NO_D6) ||
        !c->config.check_flag(Client::Flag::AT_WELCOME_MESSAGE)) {
      c->config.clear_flag(Client::Flag::AT_WELCOME_MESSAGE);
      if (send_enable_send_function_call_if_applicable(c)) {
        send_update_client_config(c);
      }
      send_main_menu(c);
    } else {
      send_message_box(c, s->welcome_message.c_str());
    }

  } else if (c->server_behavior == ServerBehavior::LOBBY_SERVER) {

    if (c->version() == GameVersion::BB) {
      // This implicitly loads the client's account and player data
      send_complete_player_bb(c);
      c->game_data.should_update_play_time = true;
    }

    if (c->config.check_flag(Client::Flag::IS_EPISODE_3)) {
      send_ep3_rank_update(c);
    }

    send_lobby_list(c);
    send_get_player_info(c);
  }
}

void on_disconnect(shared_ptr<Client> c) {
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
}

////////////////////////////////////////////////////////////////////////////////

static void set_console_client_flags(shared_ptr<Client> c, uint32_t sub_version) {
  if (c->channel.crypt_in->type() == PSOEncryption::Type::V2) {
    if (sub_version <= 0x28) {
      c->channel.version = GameVersion::DC;
      c->log.info("Game version changed to DC");
    } else if (c->version() == GameVersion::GC) {
      c->config.set_flag(Client::Flag::IS_GC_TRIAL_EDITION);
      c->log.info("GC Trial Edition flag set");
    }
  }
  c->config.set_flags_for_version(c->version(), sub_version);
  if (c->config.specific_version == default_specific_version_for_version(c->version(), -1)) {
    c->config.specific_version = default_specific_version_for_version(c->version(), sub_version);
  }
}

static void on_DB_V3(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  const auto& cmd = check_size_t<C_VerifyLicense_V3_DB>(data);
  auto s = c->require_server_state();

  if (c->channel.crypt_in->type() == PSOEncryption::Type::V2) {
    throw runtime_error("GC trial edition client sent V3 verify license command");
  }
  set_console_client_flags(c, cmd.sub_version);

  uint32_t serial_number = stoul(cmd.serial_number.decode(), nullptr, 16);
  try {
    auto l = s->license_index->verify_gc(serial_number, cmd.access_key.decode(), cmd.password.decode());
    c->set_license(l);
    send_command(c, 0x9A, 0x02);

  } catch (const LicenseIndex::no_username& e) {
    send_command(c, 0x9A, 0x03);
    c->should_disconnect = true;
    return;

  } catch (const LicenseIndex::incorrect_access_key& e) {
    send_command(c, 0x9A, 0x03);
    c->should_disconnect = true;
    return;

  } catch (const LicenseIndex::incorrect_password& e) {
    send_command(c, 0x9A, 0x01);
    return;

  } catch (const LicenseIndex::missing_license& e) {
    if (!s->allow_unregistered_users) {
      send_command(c, 0x9A, 0x04);
      c->should_disconnect = true;
      return;
    } else {
      shared_ptr<License> l(new License());
      l->serial_number = serial_number;
      l->access_key = cmd.access_key.decode();
      l->gc_password = cmd.password.decode();
      s->license_index->add(l);
      if (!s->is_replay) {
        l->save();
      }
      c->set_license(l);
      string l_str = l->str();
      c->log.info("Created license %s", l_str.c_str());
      send_command(c, 0x9A, 0x02);
    }
  }
}

static void on_88_DCNTE(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  const auto& cmd = check_size_t<C_Login_DCNTE_88>(data);
  auto s = c->require_server_state();

  c->channel.version = GameVersion::DC;
  c->config.set_flag(Client::Flag::IS_DC_V1);
  c->config.set_flag(Client::Flag::IS_DC_TRIAL_EDITION);
  c->config.set_flags_for_version(c->version(), -1);

  uint32_t serial_number = stoul(cmd.serial_number.decode(), nullptr, 16);
  try {
    shared_ptr<License> l = s->license_index->verify_v1_v2(serial_number, cmd.access_key.decode());
    c->set_license(l);
    send_command(c, 0x88, 0x00);

  } catch (const LicenseIndex::no_username& e) {
    send_message_box(c, "Incorrect serial number");
    c->should_disconnect = true;
    return;

  } catch (const LicenseIndex::incorrect_access_key& e) {
    send_message_box(c, "Incorrect access key");
    c->should_disconnect = true;

  } catch (const LicenseIndex::missing_license& e) {
    if (!s->allow_unregistered_users) {
      send_message_box(c, "Incorrect serial number");
      c->should_disconnect = true;
    } else {
      shared_ptr<License> l(new License());
      l->serial_number = serial_number;
      l->access_key = cmd.access_key.decode();
      s->license_index->add(l);
      if (!s->is_replay) {
        l->save();
      }
      c->set_license(l);
      string l_str = l->str();
      c->log.info("Created license %s", l_str.c_str());
      send_command(c, 0x88, 0x00);
    }
  }
}

static void on_8B_DCNTE(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  const auto& cmd = check_size_t<C_Login_DCNTE_8B>(data, sizeof(C_LoginExtended_DCNTE_8B));
  auto s = c->require_server_state();

  c->channel.version = GameVersion::DC;
  c->channel.language = cmd.language;
  c->config.set_flag(Client::Flag::IS_DC_V1);
  c->config.set_flag(Client::Flag::IS_DC_TRIAL_EDITION);
  c->config.set_flags_for_version(c->version(), -1);

  uint32_t serial_number = stoul(cmd.serial_number.decode(), nullptr, 16);
  try {
    shared_ptr<License> l = s->license_index->verify_v1_v2(serial_number, cmd.access_key.decode());
    c->set_license(l);

  } catch (const LicenseIndex::no_username& e) {
    send_message_box(c, "Incorrect serial number");
    c->should_disconnect = true;
    return;

  } catch (const LicenseIndex::incorrect_access_key& e) {
    send_message_box(c, "Incorrect access key");
    c->should_disconnect = true;

  } catch (const LicenseIndex::missing_license& e) {
    if (!s->allow_unregistered_users) {
      send_message_box(c, "Incorrect serial number");
      c->should_disconnect = true;
    } else {
      shared_ptr<License> l(new License());
      l->serial_number = serial_number;
      l->access_key = cmd.access_key.decode();
      s->license_index->add(l);
      if (!s->is_replay) {
        l->save();
      }
      c->set_license(l);
      string l_str = l->str();
      c->log.info("Created license %s", l_str.c_str());
    }
  }

  if (cmd.is_extended) {
    const auto& ext_cmd = check_size_t<C_LoginExtended_DCNTE_8B>(data);
    if (ext_cmd.extension.lobby_refs[0].menu_id == MenuID::LOBBY) {
      c->preferred_lobby_id = ext_cmd.extension.lobby_refs[0].item_id;
    }
  }

  if (!c->should_disconnect) {
    send_update_client_config(c);
    on_login_complete(c);
  }
}

static void on_90_DC(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  const auto& cmd = check_size_t<C_LoginV1_DC_PC_V3_90>(data, 0xFFFF);
  auto s = c->require_server_state();

  c->channel.version = GameVersion::DC;
  c->config.set_flag(Client::Flag::IS_DC_V1);
  c->config.set_flags_for_version(c->version(), -1);

  uint32_t serial_number = stoul(cmd.serial_number.decode(), nullptr, 16);
  try {
    shared_ptr<License> l = s->license_index->verify_v1_v2(serial_number, cmd.access_key.decode());
    c->set_license(l);
    send_command(c, 0x90, 0x02);

  } catch (const LicenseIndex::no_username& e) {
    send_command(c, 0x90, 0x03);
    c->should_disconnect = true;
    return;

  } catch (const LicenseIndex::incorrect_access_key& e) {
    send_command(c, 0x90, 0x03);
    c->should_disconnect = true;

  } catch (const LicenseIndex::missing_license& e) {
    if (!s->allow_unregistered_users) {
      send_command(c, 0x90, 0x03);
      c->should_disconnect = true;
    } else {
      shared_ptr<License> l(new License());
      l->serial_number = serial_number;
      l->access_key = cmd.access_key.decode();
      s->license_index->add(l);
      if (!s->is_replay) {
        l->save();
      }
      c->set_license(l);
      string l_str = l->str();
      c->log.info("Created license %s", l_str.c_str());
      send_command(c, 0x90, 0x01);
    }
  }
}

static void on_92_DC(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  const auto& cmd = check_size_t<C_RegisterV1_DC_92>(data);
  c->channel.language = cmd.language;
  // It appears that in response to 90 01, the DCv1 prototype sends 93 rather
  // than 92, so we use the presence of a 92 command to determine that the
  // client is actually DCv1 and not the prototype.
  c->config.set_flag(Client::Flag::CHECKED_FOR_DC_V1_PROTOTYPE);
  c->config.clear_flag(Client::Flag::IS_DC_V1_PROTOTYPE);
  send_command(c, 0x92, 0x01);
}

static void on_93_DC(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  const auto& cmd = check_size_t<C_LoginV1_DC_93>(data, sizeof(C_LoginExtendedV1_DC_93));
  auto s = c->require_server_state();

  c->channel.language = cmd.language;
  set_console_client_flags(c, cmd.sub_version);

  uint32_t serial_number = stoul(cmd.serial_number.decode(), nullptr, 16);
  try {
    shared_ptr<License> l = s->license_index->verify_v1_v2(serial_number, cmd.access_key.decode());
    c->set_license(l);

  } catch (const LicenseIndex::no_username& e) {
    send_message_box(c, "Incorrect serial number");
    c->should_disconnect = true;
    return;

  } catch (const LicenseIndex::incorrect_access_key& e) {
    send_message_box(c, "Incorrect access key");
    c->should_disconnect = true;
    return;

  } catch (const LicenseIndex::missing_license& e) {
    if (!s->allow_unregistered_users) {
      send_message_box(c, "Incorrect serial number");
      c->should_disconnect = true;
      return;
    } else {
      shared_ptr<License> l(new License());
      l->serial_number = serial_number;
      l->access_key = cmd.access_key.decode();
      s->license_index->add(l);
      if (!s->is_replay) {
        l->save();
      }
      c->set_license(l);
      string l_str = l->str();
      c->log.info("Created license %s", l_str.c_str());
    }
  }

  if (cmd.is_extended) {
    const auto& ext_cmd = check_size_t<C_LoginExtendedV1_DC_93>(data);
    if (ext_cmd.extension.lobby_refs[0].menu_id == MenuID::LOBBY) {
      c->preferred_lobby_id = ext_cmd.extension.lobby_refs[0].item_id;
    }
  }

  send_update_client_config(c);

  // The first time we receive a 93 from a DC client, we set this flag and send
  // a 92. The IS_DC_V1_PROTOTYPE flag will be removed if the client sends a 92
  // command (which it seems the prototype never does). This is why we always
  // respond with 90 01 here - that's the only case where actual DCv1 sends a
  // 92 command. The IS_DC_V1_PROTOTYPE flag will be removed if the client does
  // indeed send a 92.
  if (!c->config.check_flag(Client::Flag::CHECKED_FOR_DC_V1_PROTOTYPE)) {
    send_command(c, 0x90, 0x01);
    c->config.set_flag(Client::Flag::CHECKED_FOR_DC_V1_PROTOTYPE);
    c->config.set_flag(Client::Flag::IS_DC_V1_PROTOTYPE);
  } else {
    on_login_complete(c);
  }
}

static void on_9A(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  const auto& cmd = check_size_t<C_Login_DC_PC_V3_9A>(data);
  auto s = c->require_server_state();

  set_console_client_flags(c, cmd.sub_version);

  uint32_t serial_number = stoul(cmd.serial_number.decode(), nullptr, 16);
  try {
    shared_ptr<License> l;
    switch (c->version()) {
      case GameVersion::DC:
      case GameVersion::PC:
        l = s->license_index->verify_v1_v2(serial_number, cmd.access_key.decode());
        break;
      case GameVersion::GC:
        l = s->license_index->verify_gc(serial_number, cmd.access_key.decode());
        break;
      case GameVersion::XB:
        throw runtime_error("xbox licenses are not implemented");
        break;
      default:
        throw logic_error("unsupported versioned command");
    }
    c->set_license(l);
    send_command(c, 0x9A, 0x02);

  } catch (const LicenseIndex::no_username& e) {
    send_command(c, 0x9A, 0x03);
    c->should_disconnect = true;
    return;

  } catch (const LicenseIndex::incorrect_access_key& e) {
    send_command(c, 0x9A, 0x03);
    c->should_disconnect = true;
    return;

  } catch (const LicenseIndex::incorrect_password& e) {
    send_command(c, 0x9A, 0x01);
    return;

  } catch (const LicenseIndex::missing_license& e) {
    // On V3, the client should have sent a different command containing the
    // password already, which should have created and added a license. So, if
    // no license exists at this point, disconnect the client even if
    // unregistered clients are allowed.
    shared_ptr<License> l;
    if ((c->version() == GameVersion::GC) || (c->version() == GameVersion::XB)) {
      send_command(c, 0x9A, 0x04);
      c->should_disconnect = true;
      return;
    } else if ((c->version() == GameVersion::DC) || (c->version() == GameVersion::PC)) {
      shared_ptr<License> l(new License());
      l->serial_number = serial_number;
      l->access_key = cmd.access_key.decode();
      s->license_index->add(l);
      if (!s->is_replay) {
        l->save();
      }
      c->set_license(l);
      string l_str = l->str();
      c->log.info("Created license %s", l_str.c_str());
      send_command(c, 0x9A, 0x02);
    } else {
      throw runtime_error("unsupported game version");
    }
  }
}

static void on_9C(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  const auto& cmd = check_size_t<C_Register_DC_PC_V3_9C>(data);
  auto s = c->require_server_state();

  c->channel.language = cmd.language;
  set_console_client_flags(c, cmd.sub_version);

  uint32_t serial_number = stoul(cmd.serial_number.decode(), nullptr, 16);
  try {
    shared_ptr<License> l;
    switch (c->version()) {
      case GameVersion::DC:
      case GameVersion::PC:
        l = s->license_index->verify_v1_v2(serial_number, cmd.access_key.decode());
        break;
      case GameVersion::GC:
        l = s->license_index->verify_gc(serial_number, cmd.access_key.decode(), cmd.password.decode());
        break;
      case GameVersion::XB:
        throw runtime_error("xbox licenses are not implemented");
        break;
      default:
        throw logic_error("unsupported versioned command");
    }
    c->set_license(l);
    send_command(c, 0x9C, 0x01);

  } catch (const LicenseIndex::no_username& e) {
    send_message_box(c, "Incorrect serial number");
    c->should_disconnect = true;
    return;

  } catch (const LicenseIndex::incorrect_password& e) {
    send_command(c, 0x9C, 0x00);
    c->should_disconnect = true;
    return;

  } catch (const LicenseIndex::missing_license& e) {
    if (!s->allow_unregistered_users) {
      send_command(c, 0x9C, 0x00);
      c->should_disconnect = true;
      return;
    } else {
      shared_ptr<License> l(new License());
      l->serial_number = serial_number;
      l->access_key = cmd.access_key.decode();
      if (c->version() == GameVersion::GC) {
        l->gc_password = cmd.password.decode();
      }
      s->license_index->add(l);
      if (!s->is_replay) {
        l->save();
      }
      c->set_license(l);
      string l_str = l->str();
      c->log.info("Created license %s", l_str.c_str());
      send_command(c, 0x9C, 0x01);
    }
  }
}

static void on_9D_9E(shared_ptr<Client> c, uint16_t command, uint32_t, string& data) {
  const C_Login_DC_PC_GC_9D* base_cmd;
  auto s = c->require_server_state();

  if (command == 0x9D) {
    base_cmd = &check_size_t<C_Login_DC_PC_GC_9D>(data, sizeof(C_LoginExtended_PC_9D));
    if (base_cmd->is_extended) {
      if (c->version() == GameVersion::PC) {
        const auto& cmd = check_size_t<C_LoginExtended_PC_9D>(data);
        if (cmd.extension.lobby_refs[0].menu_id == MenuID::LOBBY) {
          c->preferred_lobby_id = cmd.extension.lobby_refs[0].item_id;
        }
      } else {
        const auto& cmd = check_size_t<C_LoginExtended_DC_GC_9D>(data);
        if (cmd.extension.lobby_refs[0].menu_id == MenuID::LOBBY) {
          c->preferred_lobby_id = cmd.extension.lobby_refs[0].item_id;
        }
      }
    }

  } else if (command == 0x9E) {
    // GC and XB send different amounts of data in this command. This is how
    // newserv determines if a V3 client is GC or XB.
    const auto& cmd = check_size_t<C_Login_GC_9E>(data, sizeof(C_LoginExtended_XB_9E));
    switch (data.size()) {
      case sizeof(C_Login_GC_9E):
      case sizeof(C_LoginExtended_GC_9E):
        break;
      case sizeof(C_Login_XB_9E):
      case sizeof(C_LoginExtended_XB_9E):
        c->channel.version = GameVersion::XB;
        c->log.info("Game version set to XB");
        break;
      default:
        throw runtime_error("invalid size for 9E command");
    }
    base_cmd = &cmd;
    if (cmd.is_extended) {
      const auto& cmd = check_size_t<C_LoginExtended_GC_9E>(data);
      if (cmd.extension.lobby_refs[0].menu_id == MenuID::LOBBY) {
        c->preferred_lobby_id = cmd.extension.lobby_refs[0].item_id;
      }
    }

    try {
      c->config.parse_from(cmd.client_config);
    } catch (const invalid_argument&) {
      // If we can't import the config, assume that the client was not connected
      // to newserv before, so we should show the welcome message.
      c->config.set_flag(Client::Flag::AT_WELCOME_MESSAGE);
    }

  } else {
    throw logic_error("9D/9E handler called for incorrect command");
  }

  c->channel.language = base_cmd->language;
  set_console_client_flags(c, base_cmd->sub_version);

  // See system/ppc/Episode3USAQuestBufferOverflow.s for where this value gets
  // set. We use this to determine if the client has already run the code or
  // not; sending it again when the client has already run it will likely cause
  // the client to crash.
  if (base_cmd->unused1 == 0x5F5CA297) {
    c->config.clear_flag(Client::Flag::USE_OVERFLOW_FOR_SEND_FUNCTION_CALL);
    c->config.clear_flag(Client::Flag::NO_SEND_FUNCTION_CALL);
  } else if (!s->ep3_send_function_call_enabled && c->config.check_flag(Client::Flag::USE_OVERFLOW_FOR_SEND_FUNCTION_CALL)) {
    c->config.clear_flag(Client::Flag::USE_OVERFLOW_FOR_SEND_FUNCTION_CALL);
    c->config.set_flag(Client::Flag::NO_SEND_FUNCTION_CALL);
  }

  uint32_t serial_number = stoul(base_cmd->serial_number.decode(), nullptr, 16);
  try {
    shared_ptr<License> l;
    switch (c->version()) {
      case GameVersion::DC:
      case GameVersion::PC:
        l = s->license_index->verify_v1_v2(serial_number, base_cmd->access_key.decode());
        break;
      case GameVersion::GC:
        l = s->license_index->verify_gc(serial_number, base_cmd->access_key.decode());
        break;
      case GameVersion::XB:
        throw runtime_error("xbox licenses are not implemented");
        break;
      default:
        throw logic_error("unsupported versioned command");
    }
    c->set_license(l);

  } catch (const LicenseIndex::no_username& e) {
    send_command(c, 0x04, 0x03);
    c->should_disconnect = true;
    return;

  } catch (const LicenseIndex::incorrect_access_key& e) {
    send_command(c, 0x04, 0x03);
    c->should_disconnect = true;
    return;

  } catch (const LicenseIndex::incorrect_password& e) {
    send_command(c, 0x04, 0x06);
    c->should_disconnect = true;
    return;

  } catch (const LicenseIndex::missing_license& e) {
    // On V3, the client should have sent a different command containing the
    // password already, which should have created and added a license. So, if
    // no license exists at this point, disconnect the client even if
    // unregistered clients are allowed.
    shared_ptr<License> l;
    if ((c->version() == GameVersion::GC) || (c->version() == GameVersion::XB)) {
      send_command(c, 0x04, 0x04);
      c->should_disconnect = true;
      return;
    } else if ((c->version() == GameVersion::DC) || (c->version() == GameVersion::PC)) {
      shared_ptr<License> l(new License());
      l->serial_number = serial_number;
      l->access_key = base_cmd->access_key.decode();
      s->license_index->add(l);
      if (!s->is_replay) {
        l->save();
      }
      c->set_license(l);
      string l_str = l->str();
      c->log.info("Created license %s", l_str.c_str());
    } else {
      throw runtime_error("unsupported game version");
    }
  }

  send_update_client_config(c);
  on_login_complete(c);
}

static void on_93_BB(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  const auto& cmd = check_size_t<C_Login_BB_93>(data, sizeof(C_Login_BB_93) - 8, sizeof(C_Login_BB_93));
  auto s = c->require_server_state();

  bool is_old_format;
  if (data.size() == sizeof(C_Login_BB_93) - 8) {
    is_old_format = true;
  } else if (data.size() == sizeof(C_Login_BB_93)) {
    is_old_format = false;
  } else {
    throw runtime_error("invalid size for 93 command");
  }

  c->config.set_flags_for_version(c->version(), -1);
  // Note: We ignore cmd.language in this case because there are many patched
  // clients out there that use the Japanese codebase but English data files
  // (and hence are inaccurately reported as Japanese here).

  try {
    auto l = s->license_index->verify_bb(cmd.username.decode(), cmd.password.decode());
    c->set_license(l);

  } catch (const LicenseIndex::no_username& e) {
    send_message_box(c, "Username is missing");
    c->should_disconnect = true;
    return;

  } catch (const LicenseIndex::incorrect_password& e) {
    send_message_box(c, "Incorrect login password");
    c->should_disconnect = true;
    return;

  } catch (const LicenseIndex::missing_license& e) {
    if (!s->allow_unregistered_users) {
      send_message_box(c, "You are not registered on this server");
      c->should_disconnect = true;
      return;
    } else {
      shared_ptr<License> l(new License());
      l->serial_number = fnv1a32(cmd.username.decode()) & 0x7FFFFFFF;
      l->bb_username = cmd.username.decode();
      l->bb_password = cmd.password.decode();
      s->license_index->add(l);
      if (!s->is_replay) {
        l->save();
      }
      c->set_license(l);
      string l_str = l->str();
      c->log.info("Created license %s", l_str.c_str());
    }
  }

  try {
    if (is_old_format) {
      c->config.parse_from(cmd.var.old_client_config);
    } else {
      c->config.parse_from(cmd.var.new_clients.client_config);
    }
  } catch (const invalid_argument&) {
  }

  c->bb_connection_phase = cmd.connection_phase;
  c->game_data.bb_player_index = cmd.character_slot;

  if (cmd.menu_id == MenuID::LOBBY) {
    c->preferred_lobby_id = cmd.preferred_lobby_id;
  }

  send_client_init_bb(c, 0);

  if (cmd.guild_card_number == 0) {
    // On first login, send the client to the data server port
    send_reconnect(c, s->connect_address_for_client(c), s->name_to_port_config.at("bb-data1")->port);

  } else if (c->bb_connection_phase >= 0x04) {
    // This means the client is done with the data server phase and is in the
    // game server phase; we should send the ship select menu or a lobby join
    // command.
    on_login_complete(c);
  }
}

static void on_9F_V3(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  if (c->version() == GameVersion::BB) {
    const auto& cmd = check_size_t<C_ClientConfig_BB_9F>(data);
    c->config.parse_from(cmd.data);
  } else {
    const auto& cmd = check_size_t<C_ClientConfig_V3_9F>(data);
    c->config.parse_from(cmd.data);
  }
}

static void on_96(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  check_size_t<C_CharSaveInfo_DCv2_PC_V3_BB_96>(data);
  send_server_time(c);
}

static void on_B1(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  check_size_v(data.size(), 0);
  send_server_time(c);
  // The B1 command is sent in response to a 97 command, which is normally part
  // of the pre-ship-select login sequence. However, newserv delays this until
  // after the ship select menu so that loading a GameCube program doesn't cause
  // the player's items to be deleted when they next play PSO. It's also not a
  // good idea to send a 97 and 19 at the same time, because the memory card and
  // BBA are on the same EXI bus on the GameCube and this seems to cause the SYN
  // packet after a 19 to get dropped pretty often, which causes a delay in
  // joining the lobby. This is why we delay the 19 command until the client
  // responds after saving.
  if (c->should_send_to_lobby_server) {
    send_client_to_lobby_server(c);
  } else if (c->should_send_to_proxy_server) {
    send_client_to_proxy_server(c);
  }
}

static void on_BA_Ep3(shared_ptr<Client> c, uint16_t command, uint32_t, string& data) {
  const auto& in_cmd = check_size_t<C_MesetaTransaction_GC_Ep3_BA>(data);
  auto s = c->require_server_state();
  auto l = c->lobby.lock();
  bool is_lobby = l && !l->is_game();

  uint32_t current_meseta, total_meseta_earned;
  if (s->ep3_infinite_meseta) {
    current_meseta = 1000000;
    total_meseta_earned = 1000000;
  } else if (is_lobby && s->ep3_jukebox_is_free) {
    current_meseta = c->license->ep3_current_meseta;
    total_meseta_earned = c->license->ep3_total_meseta_earned;
  } else {
    if (c->license->ep3_current_meseta < in_cmd.value) {
      throw runtime_error("meseta overdraft not allowed");
    }
    c->license->ep3_current_meseta -= in_cmd.value;
    if (!s->is_replay) {
      c->license->save();
    }
    current_meseta = c->license->ep3_current_meseta;
    total_meseta_earned = c->license->ep3_total_meseta_earned;
  }

  S_MesetaTransaction_GC_Ep3_BA out_cmd = {current_meseta, total_meseta_earned, in_cmd.request_token};
  send_command(c, command, 0x03, &out_cmd, sizeof(out_cmd));
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
    l->log.info("Client in slot %zu has disconnected before joining the game; disbanding it", target_client_id);
    send_command(l, 0xED, 0x00);
    return false;
  }

  if (l->clients[target_client_id] != nullptr) {
    throw logic_error("client id is already in use");
  }

  auto s = c->require_server_state();
  if (tourn) {
    G_SetStateFlags_GC_Ep3_6xB4x03 state_cmd;
    state_cmd.state.turn_num = 1;
    state_cmd.state.battle_phase = Episode3::BattlePhase::INVALID_00;
    state_cmd.state.current_team_turn1 = 0xFF;
    state_cmd.state.current_team_turn2 = 0xFF;
    state_cmd.state.action_subphase = Episode3::ActionSubphase::ATTACK;
    state_cmd.state.setup_phase = Episode3::SetupPhase::REGISTRATION;
    state_cmd.state.registration_phase = Episode3::RegistrationPhase::AWAITING_NUM_PLAYERS;
    state_cmd.state.team_exp.clear(0);
    state_cmd.state.team_dice_boost.clear(0);
    state_cmd.state.first_team_turn = 0xFF;
    state_cmd.state.tournament_flag = 0x01;
    state_cmd.state.client_sc_card_types.clear(Episode3::CardType::INVALID_FF);
    if (!(s->ep3_behavior_flags & Episode3::BehaviorFlag::DISABLE_MASKING)) {
      uint8_t mask_key = (random_object<uint32_t>() % 0xFF) + 1;
      set_mask_for_ep3_game_command(&state_cmd, sizeof(state_cmd), mask_key);
    }
    send_command_t(c, 0xC9, 0x00, state_cmd);
  }

  s->change_client_lobby(c, l, true, target_client_id);
  c->config.set_flag(Client::Flag::LOADING);
  if (tourn) {
    c->config.set_flag(Client::Flag::LOADING_TOURNAMENT);
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
  unordered_map<size_t, shared_ptr<Client>> table_clients;
  bool all_clients_accepted = true;
  for (const auto& c : l->clients) {
    if (!c || (c->card_battle_table_number != table_number)) {
      continue;
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
    unordered_map<size_t, uint32_t> required_serial_numbers;
    auto add_team_players = [&](shared_ptr<const Episode3::Tournament::Team> team, size_t base_index) -> void {
      size_t z = 0;
      for (const auto& player : team->players) {
        if (z >= 2) {
          throw logic_error("more than 2 players on team");
        }
        if (player.is_human()) {
          required_serial_numbers.emplace(base_index + z, player.serial_number);
        }
        z++;
      }
    };
    add_team_players(tourn_match->preceding_a->winner_team, 0);
    add_team_players(tourn_match->preceding_b->winner_team, 2);

    for (const auto& it : required_serial_numbers) {
      size_t client_id = it.first;
      uint32_t serial_number = it.second;
      for (const auto& it : table_clients) {
        if (it.second->license->serial_number == serial_number) {
          game_clients.emplace(client_id, it.second);
        }
      }
    }

    if (game_clients.size() != required_serial_numbers.size()) {
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
        message = string_printf(
            "$C7Waiting to begin match in tournament\n$C6%s$C7...\n\n(Hold B+X+START to abort)",
            tourn->get_name().c_str());
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

static void on_E4_Ep3(shared_ptr<Client> c, uint16_t, uint32_t flag, string& data) {
  const auto& cmd = check_size_t<C_CardBattleTableState_GC_Ep3_E4>(data);
  auto l = c->require_lobby();

  if (cmd.seat_number >= 4) {
    throw runtime_error("invalid seat number");
  }

  if (flag) {
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
        on_ep3_battle_table_state_updated(l, c->card_battle_table_number);
      }
    });
  } else if (!should_have_disconnect_hook) {
    c->disconnect_hooks.erase(BATTLE_TABLE_DISCONNECT_HOOK_NAME);
  }
}

static void on_E5_Ep3(shared_ptr<Client> c, uint16_t, uint32_t flag, string& data) {
  check_size_t<S_CardBattleTableConfirmation_GC_Ep3_E5>(data);
  auto l = c->require_lobby();
  if (l->is_game() || !l->is_ep3()) {
    throw runtime_error("battle table command sent in non-CARD lobby");
  }

  if (c->card_battle_table_number < 0) {
    throw runtime_error("invalid table number");
  }

  if (flag) {
    c->card_battle_table_seat_state = 2;
  } else {
    c->card_battle_table_seat_state = 3;
  }
  on_ep3_battle_table_state_updated(l, c->card_battle_table_number);
}

static void on_DC_Ep3(shared_ptr<Client> c, uint16_t, uint32_t flag, string& data) {
  check_size_v(data.size(), 0);

  auto l = c->lobby.lock();
  if (!l) {
    return;
  }

  if (flag != 0) {
    c->config.clear_flag(Client::Flag::LOADING_TOURNAMENT);
    l->set_flag(Lobby::Flag::BATTLE_IN_PROGRESS);
    send_command(c, 0xDC, 0x00);
    send_ep3_start_tournament_deck_select_if_all_clients_ready(l);
  } else {
    l->clear_flag(Lobby::Flag::BATTLE_IN_PROGRESS);
  }
}

static void on_tournament_bracket_updated(
    shared_ptr<ServerState> s, shared_ptr<const Episode3::Tournament> tourn) {
  tourn->send_all_state_updates();
  if (tourn->get_state() == Episode3::Tournament::State::COMPLETE) {
    auto team = tourn->get_winner_team();
    if (!team->has_any_human_players()) {
      send_ep3_text_message_printf(s, "$C7A CPU team won\nthe tournament\n$C6%s", tourn->get_name().c_str());
    } else {
      send_ep3_text_message_printf(s, "$C6%s$C7\nwon the tournament\n$C6%s", team->name.c_str(), tourn->get_name().c_str());
    }
    s->ep3_tournament_index->delete_tournament(tourn->get_name());
  } else {
    s->ep3_tournament_index->save();
  }
}

static void on_CA_Ep3(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  auto l = c->lobby.lock();
  if (!l) {
    // In rare cases (e.g. when two players end a tournament's match results
    // screens at exactly the same time), the client can send a server data
    // command when it's not in any lobby at all. We just ignore such commands.
    return;
  }
  if (!l->is_game() || !l->is_ep3()) {
    throw runtime_error("Episode 3 server data request sent outside of Episode 3 game");
  }

  if (l->battle_player) {
    return;
  }

  auto s = l->require_server_state();

  const auto& header = check_size_t<G_CardServerDataCommandHeader>(data, 0xFFFF);
  if (header.subcommand != 0xB3) {
    throw runtime_error("unknown Episode 3 server data request");
  }

  if (!l->ep3_server || l->ep3_server->battle_finished) {
    auto s = c->require_server_state();

    l->create_ep3_server();

    if (s->ep3_behavior_flags & Episode3::BehaviorFlag::ENABLE_STATUS_MESSAGES) {
      for (size_t z = 0; z < l->max_clients; z++) {
        if (l->clients[z]) {
          send_text_message_printf(l->clients[z], "Your client ID: $C6%zu", z);
        }
      }
    }

    if (s->ep3_behavior_flags & Episode3::BehaviorFlag::ENABLE_RECORDING) {
      if (l->battle_record) {
        for (const auto& c : l->clients) {
          if (c) {
            c->ep3_prev_battle_record = l->battle_record;
          }
        }
      }
      l->battle_record.reset(new Episode3::BattleRecord(s->ep3_behavior_flags));
      for (auto existing_c : l->clients) {
        if (existing_c) {
          auto existing_p = existing_c->game_data.player();
          PlayerLobbyDataDCGC lobby_data;
          lobby_data.name.encode(existing_p->disp.name.decode(existing_c->language()), c->language());
          lobby_data.player_tag = 0x00010000;
          lobby_data.guild_card_number = existing_c->license->serial_number;
          l->battle_record->add_player(
              lobby_data,
              existing_p->inventory,
              existing_p->disp.to_dcpcv3(c->language(), c->language()),
              c->game_data.ep3_config ? (c->game_data.ep3_config->online_clv_exp / 100) : 0);
        }
      }
      if (s->ep3_behavior_flags & Episode3::BehaviorFlag::ENABLE_STATUS_MESSAGES) {
        if (c->ep3_prev_battle_record) {
          send_text_message(l, "$C6Recording complete");
        }
        send_text_message(l, "$C6Recording enabled");
      }
    }
  }
  bool battle_finished_before = l->ep3_server->battle_finished;
  l->ep3_server->on_server_data_input(c, data);
  if (!battle_finished_before && l->ep3_server->battle_finished && l->battle_record) {
    l->battle_record->set_battle_end_timestamp();
  }
  if (l->tournament_match &&
      l->ep3_server->setup_phase == Episode3::SetupPhase::BATTLE_ENDED &&
      !l->ep3_server->tournament_match_result_sent) {
    int8_t winner_team_id = l->ep3_server->get_winner_team_id();
    if (winner_team_id == -1) {
      throw runtime_error("match complete, but winner team not specified");
    }

    auto tourn = l->tournament_match->tournament.lock();
    tourn->print_bracket(stderr);

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
    if (l->tournament_match == tourn->get_final_match()) {
      meseta_reward += s->ep3_final_round_meseta_bonus;
    }
    for (const auto& player : winner_team->players) {
      if (player.is_human()) {
        auto winner_c = player.client.lock();
        if (winner_c) {
          winner_c->license->ep3_current_meseta += meseta_reward;
          winner_c->license->ep3_total_meseta_earned += meseta_reward;
          if (!s->is_replay) {
            winner_c->license->save();
          }
          send_ep3_rank_update(winner_c);
        }
      }
    }
    send_ep3_tournament_match_result(l, meseta_reward);

    on_tournament_bracket_updated(s, tourn);
    l->ep3_server->tournament_match_result_sent = true;
  }
}

static void on_E2_Ep3(shared_ptr<Client> c, uint16_t, uint32_t flag, string&) {
  switch (flag) {
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
          send_lobby_message_box(c, "$C6The tournament\nhas concluded.");
        }
      } else {
        send_lobby_message_box(c, "$C6You are not\nregistered in a\ntournament.");
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
            team->unregister_player(c->license->serial_number);
            on_tournament_bracket_updated(s, tourn);
          }
          c->ep3_tournament_team.reset();
        }
      }
      send_ep3_confirm_tournament_entry(c, nullptr);
      break;
    }
    case 0x03: // Create tournament spectator team (get battle list)
    case 0x04: // Join tournament spectator team (get team list)
      send_lobby_message_box(c, "$C6Use View Regular\nBattle for this");
      break;
    default:
      throw runtime_error("invalid tournament operation");
  }
}

static void on_D6_V3(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  check_size_v(data.size(), 0);
  if (c->config.check_flag(Client::Flag::IN_INFORMATION_MENU)) {
    auto s = c->require_server_state();
    send_menu(c, s->information_menu_for_version(c->version()));
  } else if (c->config.check_flag(Client::Flag::AT_WELCOME_MESSAGE)) {
    send_enable_send_function_call_if_applicable(c);
    c->config.clear_flag(Client::Flag::AT_WELCOME_MESSAGE);
    send_update_client_config(c);
    send_main_menu(c);
  }
}

static void on_09(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  const auto& cmd = check_size_t<C_MenuItemInfoRequest_09>(data);
  auto s = c->require_server_state();

  switch (cmd.menu_id) {
    case MenuID::QUEST_FILTER:
      // Don't send anything here. The quest filter menu already has short
      // descriptions included with the entries, which the client shows in the
      // usual location on the screen.
      break;
    case MenuID::QUEST: {
      bool is_download_quest = !c->lobby.lock();
      auto quest_index = s->quest_index_for_client(c);
      if (!quest_index) {
        send_quest_info(c, "$C6Quests are not available.", is_download_quest);
      } else {
        auto q = quest_index->get(cmd.item_id);
        if (!q) {
          send_quest_info(c, "$C4Quest does not\nexist.", is_download_quest);
        } else {
          auto vq = q->version(c->quest_version(), c->language());
          if (!vq) {
            send_quest_info(c, "$C4Quest does not\nexist for this game\nversion.", is_download_quest);
          } else {
            send_quest_info(c, vq->long_description, is_download_quest);
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

      } else if (c->config.check_flag(Client::Flag::IS_EPISODE_3) && game->is_ep3()) {
        send_ep3_game_details(c, game);

      } else {
        string info;
        for (size_t x = 0; x < game->max_clients; x++) {
          const auto& game_c = game->clients[x];
          if (game_c.get()) {
            auto player = game_c->game_data.player();
            string name = player->disp.name.decode(game_c->language());
            if (game->is_ep3()) {
              info += string_printf("%zu: $C6%s$C7 L%" PRIu32 "\n",
                  x + 1, name.c_str(), player->disp.stats.level + 1);
            } else {
              info += string_printf("%zu: $C6%s$C7 %s L%" PRIu32 "\n",
                  x + 1, name.c_str(),
                  abbreviation_for_char_class(player->disp.visual.char_class),
                  player->disp.stats.level + 1);
            }
          }
        }

        string secid_str = name_for_section_id(game->section_id);
        info += string_printf("%s %c %s %s\n",
            abbreviation_for_episode(game->episode),
            abbreviation_for_difficulty(game->difficulty),
            abbreviation_for_mode(game->mode),
            secid_str.c_str());

        bool cheats_enabled = game->check_flag(Lobby::Flag::CHEATS_ENABLED);
        bool locked = !game->password.empty();
        if (cheats_enabled && locked) {
          info += "$C4Locked$C7, $C6cheats enabled$C7\n";
        } else if (cheats_enabled) {
          info += "$C6Cheats enabled$C7\n";
        } else if (locked) {
          info += "$C4Locked$C7\n";
        }

        if (game->quest) {
          info += (game->check_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS)) ? "$C6Quest: " : "$C4Quest: ";
          info += game->quest->name;
          info += "\n";
        } else if (game->check_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS)) {
          info += "$C6Quest in progress\n";
        } else if (game->check_flag(Lobby::Flag::QUEST_IN_PROGRESS)) {
          info += "$C4Quest in progress\n";
        } else if (game->check_flag(Lobby::Flag::BATTLE_IN_PROGRESS)) {
          info += "$C4Battle in progress\n";
        }

        if (game->check_flag(Lobby::Flag::SPECTATORS_FORBIDDEN)) {
          info += "$C4View Battle forbidden\n";
        }

        strip_trailing_whitespace(info);
        send_ship_info(c, info);
      }
      break;
    }

    case MenuID::TOURNAMENTS_FOR_SPEC:
    case MenuID::TOURNAMENTS: {
      if (!c->config.check_flag(Client::Flag::IS_EPISODE_3)) {
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
      if (!c->config.check_flag(Client::Flag::IS_EPISODE_3)) {
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
          if (team->name.empty()) {
            message = "(No registrant)";
          } else if (team->max_players == 1) {
            message = string_printf("$C6%s$C7\n%zu %s (%s)\nPlayers:",
                team->name.c_str(),
                team->num_rounds_cleared,
                team->num_rounds_cleared == 1 ? "win" : "wins",
                team->is_active ? "active" : "defeated");
          } else {
            message = string_printf("$C6%s$C7\n%zu %s (%s)%s\nPlayers:",
                team->name.c_str(),
                team->num_rounds_cleared,
                team->num_rounds_cleared == 1 ? "win" : "wins",
                team->is_active ? "active" : "defeated",
                team->password.empty() ? "" : "\n$C4Locked$C7");
          }
          for (const auto& player : team->players) {
            if (player.is_human()) {
              if (player.player_name.empty()) {
                message += string_printf("\n  $C6%08" PRIX32 "$C7", player.serial_number);
              } else {
                message += string_printf("\n  $C6%s$C7 (%08" PRIX32 ")", player.player_name.c_str(), player.serial_number);
              }
            } else {
              message += string_printf("\n  $C3%s \"%s\"$C7", player.com_deck->player_name.c_str(), player.com_deck->deck_name.c_str());
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
            return;
          }
        }
        send_ship_info(c, "$C4Incorrect menu\nitem ID");
      }
      break;
  }
}

static void on_10(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  bool uses_unicode = ((c->version() == GameVersion::PC) || (c->version() == GameVersion::BB));

  uint32_t menu_id;
  uint32_t item_id;
  string team_name;
  string password;

  if (data.size() > sizeof(C_MenuSelection_10_Flag00)) {
    if (uses_unicode) {
      // TODO: We can support the Flag03 variant here, but PC/BB probably never
      // actually use it.
      const auto& cmd = check_size_t<C_MenuSelection_PC_BB_10_Flag02>(data);
      password = cmd.password.decode(c->language());
      menu_id = cmd.basic_cmd.menu_id;
      item_id = cmd.basic_cmd.item_id;
    } else if (data.size() > sizeof(C_MenuSelection_DC_V3_10_Flag02)) {
      const auto& cmd = check_size_t<C_MenuSelection_DC_V3_10_Flag03>(data);
      team_name = cmd.unknown_a1.decode(c->language());
      password = cmd.password.decode(c->language());
      menu_id = cmd.basic_cmd.menu_id;
      item_id = cmd.basic_cmd.item_id;
    } else {
      const auto& cmd = check_size_t<C_MenuSelection_DC_V3_10_Flag02>(data);
      password = cmd.password.decode(c->language());
      menu_id = cmd.basic_cmd.menu_id;
      item_id = cmd.basic_cmd.item_id;
    }
  } else {
    const auto& cmd = check_size_t<C_MenuSelection_10_Flag00>(data);
    menu_id = cmd.menu_id;
    item_id = cmd.item_id;
  }

  switch (menu_id) {
    case MenuID::MAIN: {
      switch (item_id) {
        case MainMenuItemID::GO_TO_LOBBY: {
          c->should_send_to_lobby_server = true;
          if (!c->config.check_flag(Client::Flag::SAVE_ENABLED)) {
            c->config.set_flag(Client::Flag::SAVE_ENABLED);
            // DC NTE and the v1 prototype crash if they receive a 97 command,
            // so we instead do the redirect immediately
            if ((c->version() == GameVersion::DC) &&
                (c->config.check_flag(Client::Flag::IS_DC_TRIAL_EDITION) || c->config.check_flag(Client::Flag::IS_DC_V1_PROTOTYPE))) {
              send_client_to_lobby_server(c);
            } else {
              send_command(c, 0x97, 0x01);
              send_update_client_config(c);
            }
          } else {
            send_client_to_lobby_server(c);
          }
          break;
        }

        case MainMenuItemID::INFORMATION: {
          auto s = c->require_server_state();
          send_menu(c, s->information_menu_for_version(c->version()));
          c->config.set_flag(Client::Flag::IN_INFORMATION_MENU);
          break;
        }

        case MainMenuItemID::PROXY_DESTINATIONS:
          send_proxy_destinations_menu(c);
          break;

        case MainMenuItemID::DOWNLOAD_QUESTS: {
          auto s = c->require_server_state();
          if (c->config.check_flag(Client::Flag::IS_EPISODE_3)) {
            // Episode 3 has only download quests, not online quests, so this is
            // always the download quest menu. (Episode 3 does actually have
            // online quests, but they're served via a server data request
            // instead of the file download paradigm that other versions use.)
            uint32_t ep3_category_id = 0;
            size_t num_ep3_categories = 0;
            for (const auto& category : s->quest_category_index->categories) {
              if (category.flags & QuestCategoryIndex::Category::Flag::EP3_DOWNLOAD) {
                ep3_category_id = category.category_id;
                num_ep3_categories++;
              }
            }
            if (num_ep3_categories == 1) {
              auto quest_index = s->quest_index_for_client(c);
              if (quest_index) {
                auto quests = quest_index->filter(ep3_category_id, c->quest_version(), c->language());
                send_quest_menu(c, MenuID::QUEST, quests, true);
              } else {
                send_lobby_message_box(c, "$C6Quests are not available.");
              }
              break;
            }
          }

          // Not Episode 3, or there are multiple Episode 3 download categories;
          // send the categories menu instead
          uint8_t flags = c->config.check_flag(Client::Flag::IS_EPISODE_3)
              ? QuestCategoryIndex::Category::Flag::EP3_DOWNLOAD
              : QuestCategoryIndex::Category::Flag::DOWNLOAD;
          if (c->version() == GameVersion::DC || c->version() == GameVersion::PC) {
            flags |= QuestCategoryIndex::Category::Flag::HIDE_ON_PRE_V3;
          }
          send_quest_menu(c, MenuID::QUEST_FILTER, s->quest_category_index, flags);
          break;
        }

        case MainMenuItemID::PATCHES:
          if (!function_compiler_available()) {
            throw runtime_error("function compiler not available");
          }
          if (c->config.check_flag(Client::Flag::NO_SEND_FUNCTION_CALL)) {
            throw runtime_error("client does not support send_function_call");
          }
          prepare_client_for_patches(c, [c]() -> void {
            send_menu(c, c->require_server_state()->function_code_index->patch_menu(c->config.specific_version));
          });
          break;

        case MainMenuItemID::PROGRAMS:
          if (!function_compiler_available()) {
            throw runtime_error("function compiler not available");
          }
          if (c->config.check_flag(Client::Flag::NO_SEND_FUNCTION_CALL)) {
            throw runtime_error("client does not support send_function_call");
          }
          prepare_client_for_patches(c, [c]() -> void {
            send_menu(c, c->require_server_state()->dol_file_index->menu);
          });
          break;

        case MainMenuItemID::DISCONNECT:
          c->should_disconnect = true;
          break;

        case MainMenuItemID::CLEAR_LICENSE:
          send_command(c, 0x9A, 0x04);
          c->should_disconnect = true;
          break;

        default:
          send_message_box(c, "Incorrect menu item ID.");
          break;
      }
      break;
    }

    case MenuID::INFORMATION: {
      if (item_id == InformationMenuItemID::GO_BACK) {
        c->config.clear_flag(Client::Flag::IN_INFORMATION_MENU);
        send_main_menu(c);

      } else {
        try {
          auto contents = c->require_server_state()->information_contents_for_client(c);
          send_message_box(c, contents->at(item_id).c_str());
        } catch (const out_of_range&) {
          send_message_box(c, "$C6No such information exists.");
        }
      }
      break;
    }

    case MenuID::PROXY_OPTIONS: {
      switch (item_id) {
        case ProxyOptionsMenuItemID::GO_BACK:
          send_proxy_destinations_menu(c);
          break;
        case ProxyOptionsMenuItemID::CHAT_COMMANDS:
          c->config.toggle_flag(Client::Flag::PROXY_CHAT_COMMANDS_ENABLED);
          goto resend_proxy_options_menu;
        case ProxyOptionsMenuItemID::CHAT_FILTER:
          c->config.toggle_flag(Client::Flag::PROXY_CHAT_FILTER_ENABLED);
          goto resend_proxy_options_menu;
        case ProxyOptionsMenuItemID::PLAYER_NOTIFICATIONS:
          c->config.toggle_flag(Client::Flag::PROXY_PLAYER_NOTIFICATIONS_ENABLED);
          goto resend_proxy_options_menu;
        case ProxyOptionsMenuItemID::BLOCK_PINGS:
          c->config.toggle_flag(Client::Flag::PROXY_SUPPRESS_CLIENT_PINGS);
          goto resend_proxy_options_menu;
        case ProxyOptionsMenuItemID::INFINITE_HP:
          c->config.toggle_flag(Client::Flag::INFINITE_HP_ENABLED);
          goto resend_proxy_options_menu;
        case ProxyOptionsMenuItemID::INFINITE_TP:
          c->config.toggle_flag(Client::Flag::INFINITE_TP_ENABLED);
          goto resend_proxy_options_menu;
        case ProxyOptionsMenuItemID::SWITCH_ASSIST:
          c->config.toggle_flag(Client::Flag::SWITCH_ASSIST_ENABLED);
          goto resend_proxy_options_menu;
        case ProxyOptionsMenuItemID::EP3_INFINITE_MESETA:
          c->config.toggle_flag(Client::Flag::PROXY_EP3_INFINITE_MESETA_ENABLED);
          goto resend_proxy_options_menu;
        case ProxyOptionsMenuItemID::EP3_INFINITE_TIME:
          c->config.toggle_flag(Client::Flag::PROXY_EP3_INFINITE_TIME_ENABLED);
          goto resend_proxy_options_menu;
        case ProxyOptionsMenuItemID::BLOCK_EVENTS:
          c->config.override_lobby_event = (c->config.override_lobby_event == 0xFF) ? 0x00 : 0xFF;
          goto resend_proxy_options_menu;
        case ProxyOptionsMenuItemID::BLOCK_PATCHES:
          c->config.toggle_flag(Client::Flag::PROXY_BLOCK_FUNCTION_CALLS);
          goto resend_proxy_options_menu;
        case ProxyOptionsMenuItemID::SAVE_FILES:
          c->config.toggle_flag(Client::Flag::PROXY_SAVE_FILES);
          goto resend_proxy_options_menu;
        case ProxyOptionsMenuItemID::RED_NAME:
          c->config.toggle_flag(Client::Flag::PROXY_RED_NAME_ENABLED);
          goto resend_proxy_options_menu;
        case ProxyOptionsMenuItemID::BLANK_NAME:
          c->config.toggle_flag(Client::Flag::PROXY_BLANK_NAME_ENABLED);
          goto resend_proxy_options_menu;
        case ProxyOptionsMenuItemID::SUPPRESS_LOGIN:
          c->config.toggle_flag(Client::Flag::PROXY_SUPPRESS_REMOTE_LOGIN);
          goto resend_proxy_options_menu;
        case ProxyOptionsMenuItemID::SKIP_CARD:
          c->config.toggle_flag(Client::Flag::PROXY_ZERO_REMOTE_GUILD_CARD);
        resend_proxy_options_menu:
          send_menu(c, proxy_options_menu_for_client(c));
          break;
        default:
          send_message_box(c, "Incorrect menu item ID.");
          break;
      }
      break;
    }

    case MenuID::PROXY_DESTINATIONS: {
      if (item_id == ProxyDestinationsMenuItemID::GO_BACK) {
        send_main_menu(c);

      } else if (item_id == ProxyDestinationsMenuItemID::OPTIONS) {
        send_menu(c, proxy_options_menu_for_client(c));

      } else {
        auto s = c->require_server_state();
        const pair<string, uint16_t>* dest = nullptr;
        try {
          dest = &s->proxy_destinations_for_version(c->version()).at(item_id);
        } catch (const out_of_range&) {
        }

        if (!dest) {
          send_message_box(c, "$C6No such destination exists.");
          c->should_disconnect = true;
        } else {
          // Clear Check Tactics menu so client won't see newserv tournament
          // state while logically on another server
          if (c->config.check_flag(Client::Flag::IS_EPISODE_3) && !c->config.check_flag(Client::Flag::IS_EP3_TRIAL_EDITION)) {
            send_ep3_confirm_tournament_entry(c, nullptr);
          }

          c->config.proxy_destination_address = resolve_ipv4(dest->first);
          c->config.proxy_destination_port = dest->second;
          if (!c->config.check_flag(Client::Flag::SAVE_ENABLED)) {
            c->should_send_to_proxy_server = true;
            c->config.set_flag(Client::Flag::SAVE_ENABLED);
            send_command(c, 0x97, 0x01);
            send_update_client_config(c);
          } else {
            send_update_client_config(c);
            send_client_to_proxy_server(c);
          }
        }
      }
      break;
    }

    case MenuID::GAME: {
      auto s = c->require_server_state();
      auto game = s->find_lobby(item_id);
      if (!game) {
        send_lobby_message_box(c, "$C6You cannot join this\ngame because it no\nlonger exists.");
        break;
      }
      if (!game->is_game()) {
        send_lobby_message_box(c, "$C6You cannot join this\ngame because it is\nnot a game.");
        break;
      }
      if (game->count_clients() >= game->max_clients) {
        send_lobby_message_box(c, "$C6You cannot join this\ngame because it is\nfull.");
        break;
      }
      if (!game->version_is_allowed(c->quest_version()) ||
          (game->is_ep3() != c->config.check_flag(Client::Flag::IS_EPISODE_3)) ||
          (game->check_flag(Lobby::Flag::IS_EP3_TRIAL) != c->config.check_flag(Client::Flag::IS_EP3_TRIAL_EDITION))) {
        send_lobby_message_box(c, "$C6You cannot join this\ngame because it is\nfor a different\nversion of PSO.");
        break;
      }
      if (game->check_flag(Lobby::Flag::QUEST_IN_PROGRESS)) {
        send_lobby_message_box(c, "$C6You cannot join this\ngame because a\nquest is already\nin progress.");
        break;
      }
      if (game->check_flag(Lobby::Flag::BATTLE_IN_PROGRESS)) {
        send_lobby_message_box(c, "$C6You cannot join this\ngame because a\nbattle is already\nin progress.");
        break;
      }
      if (game->any_client_loading()) {
        send_lobby_message_box(c, "$C6You cannot join this\ngame because\nanother player is\ncurrently loading.\nTry again soon.");
        break;
      }
      if (game->mode == GameMode::SOLO) {
        send_lobby_message_box(c, "$C6You cannot join this\n game because it is\na Solo Mode game.");
        break;
      }

      if (!(c->license->flags & License::Flag::FREE_JOIN_GAMES)) {
        if (!game->password.empty() && (password != game->password)) {
          send_lobby_message_box(c, "$C6Incorrect password.");
          break;
        }
        auto p = c->game_data.player();
        if (p->disp.stats.level < game->min_level) {
          send_lobby_message_box(c, "$C6Your level is too\nlow to join this\ngame.");
          break;
        }
        if (p->disp.stats.level > game->max_level) {
          send_lobby_message_box(c, "$C6Your level is too\nhigh to join this\ngame.");
          break;
        }
      }

      if (!s->change_client_lobby(c, game)) {
        throw logic_error("client cannot join game after all preconditions satisfied");
      }
      c->config.set_flag(Client::Flag::LOADING);
      break;
    }

    case MenuID::QUEST_FILTER: {
      auto s = c->require_server_state();
      auto quest_index = s->quest_index_for_client(c);
      if (!quest_index) {
        send_lobby_message_box(c, "$C6Quests are not available.");
        break;
      }
      shared_ptr<Lobby> l = c->lobby.lock();
      auto quests = quest_index->filter(item_id, c->quest_version(), c->language());

      // Hack: Assume the menu to be sent is the download quest menu if the
      // client is not in any lobby
      send_quest_menu(c, MenuID::QUEST, quests, !l);
      break;
    }

    case MenuID::QUEST: {
      auto s = c->require_server_state();
      auto quest_index = s->quest_index_for_client(c);
      if (!quest_index) {
        send_lobby_message_box(c, "$C6Quests are not\navailable.");
        break;
      }
      auto q = quest_index->get(item_id);
      if (!q) {
        send_lobby_message_box(c, "$C6Quest does not exist.");
        break;
      }

      // If the client is not in a lobby, send the quest as a download quest.
      // Otherwise, they must be in a game to load a quest.
      auto l = c->lobby.lock();
      if (l && !l->is_game()) {
        send_lobby_message_box(c, "$C6Quests cannot be\nloaded in lobbies.");
        break;
      }

      if (l) {
        if (q->episode == Episode::EP3) {
          send_lobby_message_box(c, "$C6Episode 3 quests\ncannot be loaded\nvia this interface.");
          break;
        }
        if (l->quest) {
          send_lobby_message_box(c, "$C6A quest is already\nin progress.");
          break;
        }
        if (q->joinable) {
          l->set_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS);
        } else {
          l->set_flag(Lobby::Flag::QUEST_IN_PROGRESS);
        }

        l->quest = q;
        l->episode = q->episode;
        if (l->item_creator) {
          l->item_creator->clear_destroyed_entities();
          if (q->battle_rules) {
            l->item_creator->set_restrictions(q->battle_rules);
          }
        }

        for (size_t client_id = 0; client_id < l->max_clients; client_id++) {
          auto lc = l->clients[client_id];
          if (!lc) {
            continue;
          }

          auto vq = q->version(lc->quest_version(), lc->language());
          if (!vq) {
            send_lobby_message_box(lc, "$C6Quest does not exist\nfor this game version.");
            lc->should_disconnect = true;
            break;
          }

          if (vq->battle_rules) {
            lc->game_data.create_battle_overlay(vq->battle_rules, s->level_table);
            lc->log.info("Created battle overlay");
            lc->game_data.player()->print_inventory(stderr, c->version(), s->item_name_index);
          } else if (vq->challenge_template_index >= 0) {
            lc->game_data.create_challenge_overlay(lc->version(), vq->challenge_template_index, s->level_table);
            lc->log.info("Created challenge overlay");
            lc->game_data.player()->print_inventory(stderr, c->version(), s->item_name_index);
          }

          // If an overlay was created, item IDs need to be assigned
          if (lc->game_data.has_overlay()) {
            auto overlay = lc->game_data.player();
            for (size_t z = 0; z < overlay->inventory.num_items; z++) {
              overlay->inventory.items[z].data.id = l->generate_item_id(client_id);
            }
          }

          string bin_filename = vq->bin_filename();
          string dat_filename = vq->dat_filename();
          send_open_quest_file(lc, bin_filename, bin_filename, vq->bin_contents, QuestFileType::ONLINE);
          send_open_quest_file(lc, dat_filename, dat_filename, vq->dat_contents, QuestFileType::ONLINE);

          // There is no such thing as command AC on PSO V1 and V2 - quests just
          // start immediately when they're done downloading. (This is also the
          // case on V3 Trial Edition.) There are also no chunk acknowledgements
          // (C->S 13 commands) like there are on GC. So, for pre-V3 clients, we
          // can just not set the loading flag, since we never need to
          // check/clear it later.
          if ((lc->version() != GameVersion::DC) &&
              (lc->version() != GameVersion::PC) &&
              !lc->config.check_flag(Client::Flag::IS_GC_TRIAL_EDITION)) {
            lc->config.set_flag(Client::Flag::LOADING_QUEST);
            lc->disconnect_hooks.emplace(QUEST_BARRIER_DISCONNECT_HOOK_NAME, [l]() -> void {
              send_quest_barrier_if_all_clients_ready(l);
            });
          }
        }

      } else {
        auto vq = q->version(c->quest_version(), c->language());
        if (!vq) {
          send_lobby_message_box(c, "$C6Quest does not exist\nfor this game version.");
          break;
        }
        // Episode 3 uses the download quest commands (A6/A7) but does not
        // expect the server to have already encrypted the quest files, unlike
        // other versions.
        // TODO: This is not true for Episode 3 Trial Edition. We also would
        // have to convert the map to a MapDefinitionTrial, though.
        if (vq->version == QuestScriptVersion::GC_EP3) {
          send_open_quest_file(c, q->name, vq->bin_filename(), vq->bin_contents, QuestFileType::EPISODE_3);
        } else {
          vq = vq->create_download_quest();
          send_open_quest_file(c, q->name, vq->bin_filename(), vq->bin_contents, QuestFileType::DOWNLOAD);
          send_open_quest_file(c, q->name, vq->dat_filename(), vq->dat_contents, QuestFileType::DOWNLOAD);
        }
      }
      break;
    }

    case MenuID::PATCHES:
      if (item_id == PatchesMenuItemID::GO_BACK) {
        send_main_menu(c);

      } else {
        if (c->config.check_flag(Client::Flag::NO_SEND_FUNCTION_CALL)) {
          throw runtime_error("client does not support send_function_call");
        }

        auto s = c->require_server_state();
        uint64_t key = (static_cast<uint64_t>(item_id) << 32) | c->config.specific_version;
        send_function_call(
            c, s->function_code_index->menu_item_id_and_specific_version_to_patch_function.at(key));
        c->function_call_response_queue.emplace_back(empty_function_call_response_handler);
        send_menu(c, s->function_code_index->patch_menu(c->config.specific_version));
      }
      break;

    case MenuID::PROGRAMS:
      if (item_id == ProgramsMenuItemID::GO_BACK) {
        send_main_menu(c);

      } else {
        if (c->config.check_flag(Client::Flag::NO_SEND_FUNCTION_CALL)) {
          throw runtime_error("client does not support send_function_call");
        }

        auto s = c->require_server_state();
        c->loading_dol_file = s->dol_file_index->item_id_to_file.at(item_id);

        // Send the first function call, which triggers the process of loading a
        // DOL file. The result of this function call determines the necessary
        // base address for loading the file.
        send_function_call(
            c,
            s->function_code_index->name_to_function.at("ReadMemoryWord"),
            {{"address", 0x80000034}}); // ArenaHigh from GC globals
      }
      break;

    case MenuID::TOURNAMENTS_FOR_SPEC:
    case MenuID::TOURNAMENTS: {
      if (!c->config.check_flag(Client::Flag::IS_EPISODE_3)) {
        throw runtime_error("non-Episode 3 client attempted to join tournament");
      }
      auto s = c->require_server_state();
      auto tourn = s->ep3_tournament_index->get_tournament(item_id);
      if (tourn) {
        send_ep3_tournament_entry_list(c, tourn, (menu_id == MenuID::TOURNAMENTS_FOR_SPEC));
      }
      break;
    }
    case MenuID::TOURNAMENT_ENTRIES: {
      if (!c->config.check_flag(Client::Flag::IS_EPISODE_3)) {
        throw runtime_error("non-Episode 3 client attempted to join tournament");
      }
      if (c->ep3_tournament_team.lock()) {
        send_lobby_message_box(c, "$C6You are registered\nin a different\ntournament already");
        break;
      }
      if (team_name.empty()) {
        team_name = c->game_data.player()->disp.name.decode(c->language());
        team_name += string_printf("/%" PRIX32, c->license->serial_number);
      }
      auto s = c->require_server_state();
      uint16_t tourn_num = item_id >> 16;
      uint16_t team_index = item_id & 0xFFFF;
      auto tourn = s->ep3_tournament_index->get_tournament(tourn_num);
      if (tourn) {
        auto team = tourn->get_team(team_index);
        if (team) {
          try {
            team->register_player(c, team_name, password);
            c->ep3_tournament_team = team;
            tourn->send_all_state_updates();
            string message = string_printf("$C7You are registered in $C6%s$C7.\n\
\n\
After the tournament begins, start your matches\n\
by standing at any Battle Table along with your\n\
partner (if any) and opponent(s).",
                tourn->get_name().c_str());
            send_ep3_timed_message_box(c->channel, 240, message.c_str());

            s->ep3_tournament_index->save();

          } catch (const exception& e) {
            string message = string_printf("Cannot join team:\n%s", e.what());
            send_lobby_message_box(c, message);
          }
        } else {
          send_lobby_message_box(c, "Team does not exist");
        }
      } else {
        send_lobby_message_box(c, "Tournament does\nnot exist");
      }
      break;
    }

    default:
      send_message_box(c, "Incorrect menu ID");
      break;
  }
}

static void on_84(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  const auto& cmd = check_size_t<C_LobbySelection_84>(data);
  auto s = c->require_server_state();

  if (cmd.menu_id != MenuID::LOBBY) {
    send_message_box(c, "Incorrect menu ID");
    return;
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
      return;
    }

    if (new_lobby->is_game()) {
      send_lobby_message_box(c, "$C6Can't change lobby\n\n$C7The specified lobby\nis a game.");
      return;
    }

    if (new_lobby->is_ep3() && !c->config.check_flag(Client::Flag::IS_EPISODE_3)) {
      send_lobby_message_box(c, "$C6Can't change lobby\n\n$C7The lobby is for\nEpisode 3 only.");
      return;
    }

    if (!s->change_client_lobby(c, new_lobby)) {
      send_lobby_message_box(c, "$C6Can\'t change lobby\n\n$C7The lobby is full.");
    }
  }
}

static void on_08_E6(shared_ptr<Client> c, uint16_t command, uint32_t, string& data) {
  check_size_v(data.size(), 0);
  send_game_menu(c, (command == 0xE6), false);
}

static void on_1F(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  check_size_v(data.size(), 0);
  auto s = c->require_server_state();
  send_menu(c, s->information_menu_for_version(c->version()), true);
}

static void on_A0(shared_ptr<Client> c, uint16_t, uint32_t, string&) {
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
  if (!c->config.check_flag(Client::Flag::NO_D6)) {
    send_message_box(c, "");
  }

  const auto& port_name = version_to_login_port_name.at(static_cast<size_t>(c->version()));

  auto s = c->require_server_state();
  send_reconnect(c, s->connect_address_for_client(c),
      s->name_to_port_config.at(port_name)->port);
}

static void on_A1(shared_ptr<Client> c, uint16_t command, uint32_t flag, string& data) {
  // newserv doesn't have blocks; treat block change the same as ship change
  on_A0(c, command, flag, data);
}

static void on_8E_DCNTE(shared_ptr<Client> c, uint16_t command, uint32_t flag, string& data) {
  if (c->config.check_flag(Client::Flag::IS_DC_TRIAL_EDITION)) {
    on_A0(c, command, flag, data);
  } else {
    throw runtime_error("non-DCNTE client sent 8E");
  }
}

static void on_8F_DCNTE(shared_ptr<Client> c, uint16_t command, uint32_t flag, string& data) {
  if (c->config.check_flag(Client::Flag::IS_DC_TRIAL_EDITION)) {
    on_A1(c, command, flag, data);
  } else {
    throw runtime_error("non-DCNTE client sent 8F");
  }
}

static void send_dol_file_chunk(shared_ptr<Client> c, uint32_t start_addr) {
  size_t offset = start_addr - c->dol_base_addr;
  if (offset >= c->loading_dol_file->data.size()) {
    throw logic_error("DOL file offset beyond end of data");
  }
  // Note: The protocol allows commands to be up to 0x7C00 bytes in size, but
  // sending large B2 commands can cause the client to crash or softlock. To
  // avoid this, we limit the payload to 4KB, which results in a B2 command
  // 0x10D0 bytes in size.
  size_t bytes_to_send = min<size_t>(0x1000, c->loading_dol_file->data.size() - offset);
  string data_to_send = c->loading_dol_file->data.substr(offset, bytes_to_send);

  auto s = c->require_server_state();
  auto fn = s->function_code_index->name_to_function.at("WriteMemory");
  unordered_map<string, uint32_t> label_writes(
      {{"dest_addr", start_addr}, {"size", bytes_to_send}});
  send_function_call(c, fn, label_writes, data_to_send);

  size_t progress_percent = ((offset + bytes_to_send) * 100) / c->loading_dol_file->data.size();
  send_ship_info(c, string_printf("%zu%%%%", progress_percent));
}

static void on_B3(shared_ptr<Client> c, uint16_t, uint32_t flag, string& data) {
  const auto& cmd = check_size_t<C_ExecuteCodeResult_B3>(data);
  auto s = c->require_server_state();

  if (!c->function_call_response_queue.empty()) {
    auto& handler = c->function_call_response_queue.front();
    handler(cmd.return_value, cmd.checksum);
    c->function_call_response_queue.pop_front();
  } else if (c->loading_dol_file.get()) {
    auto called_fn = s->function_code_index->index_to_function.at(flag);
    if (called_fn->name == "ReadMemoryWord") {
      c->dol_base_addr = (cmd.return_value - c->loading_dol_file->data.size()) & (~3);
      send_dol_file_chunk(c, c->dol_base_addr);
    } else if (called_fn->name == "WriteMemory") {
      if (cmd.return_value >= c->dol_base_addr + c->loading_dol_file->data.size()) {
        auto fn = s->function_code_index->name_to_function.at("RunDOL");
        unordered_map<string, uint32_t> label_writes(
            {{"dol_base_ptr", c->dol_base_addr}});
        send_function_call(c, fn, label_writes);
        // The client will stop running PSO after this, so disconnect them
        c->should_disconnect = true;

      } else {
        send_dol_file_chunk(c, cmd.return_value);
      }
    } else {
      throw logic_error("unknown function called during DOL loading");
    }
  } else {
    throw runtime_error("function call response queue is empty, and no program is being sent");
  }
}

static void on_A2(shared_ptr<Client> c, uint16_t, uint32_t flag, string& data) {
  check_size_v(data.size(), 0);
  auto s = c->require_server_state();

  auto l = c->lobby.lock();
  if (!l || !l->is_game()) {
    send_lobby_message_box(c, "$C6Quests are not available\nin lobbies.");
    return;
  }

  // In Episode 3, there are no quest categories, so skip directly to the quest
  // filter menu.
  if (c->config.check_flag(Client::Flag::IS_EPISODE_3)) {
    send_lobby_message_box(c, "$C6Episode 3 does not\nprovide online quests\nvia this interface.");

  } else {

    uint8_t flags = (c->version() == GameVersion::DC || c->version() == GameVersion::PC)
        ? QuestCategoryIndex::Category::Flag::HIDE_ON_PRE_V3
        : 0;

    if ((c->version() == GameVersion::BB) && flag) {
      flags |= QuestCategoryIndex::Category::Flag::GOVERNMENT;
    } else {
      switch (l->mode) {
        case GameMode::NORMAL:
          flags |= QuestCategoryIndex::Category::Flag::NORMAL;
          break;
        case GameMode::BATTLE:
          flags |= QuestCategoryIndex::Category::Flag::BATTLE;
          break;
        case GameMode::CHALLENGE:
          flags |= QuestCategoryIndex::Category::Flag::CHALLENGE;
          break;
        case GameMode::SOLO:
          flags |= QuestCategoryIndex::Category::Flag::SOLO;
          break;
        default:
          throw logic_error("invalid game mode");
      }
    }

    send_quest_menu(c, MenuID::QUEST_FILTER, s->quest_category_index, flags);
  }
}

static void on_AC_V3_BB(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  check_size_v(data.size(), 0);

  auto l = c->require_lobby();

  if (c->config.check_flag(Client::Flag::LOADING_RUNNING_JOINABLE_QUEST)) {
    c->config.clear_flag(Client::Flag::LOADING_RUNNING_JOINABLE_QUEST);
    if (l->base_version != GameVersion::BB) {
      throw logic_error("joinable quest started on non-BB version");
    }

    auto leader_c = l->clients.at(l->leader_id);
    if (!leader_c) {
      throw logic_error("lobby leader is missing");
    }

    send_command(c, 0xAC, 0x00);
    send_command(leader_c, 0xDD, c->lobby_client_id);

  } else if (c->config.check_flag(Client::Flag::LOADING_QUEST)) {
    c->config.clear_flag(Client::Flag::LOADING_QUEST);

    if (!l->quest.get()) {
      return;
    }

    if (send_quest_barrier_if_all_clients_ready(l) &&
        (l->base_version == GameVersion::BB) &&
        l->map &&
        l->quest) {
      auto vq = l->quest->version(QuestScriptVersion::BB_V4, c->language());
      auto dat_contents = prs_decompress(*vq->dat_contents);
      l->map->clear();
      l->map->add_enemies_from_quest_data(l->episode, l->difficulty, l->event, dat_contents.data(), dat_contents.size());
      c->log.info("Replaced enemies list with quest layout (%zu entries)",
          l->map->enemies.size());
      for (size_t z = 0; z < l->map->enemies.size(); z++) {
        string e_str = l->map->enemies[z].str();
        l->log.info("(Entry %zX) %s", z, e_str.c_str());
      }
    }
  }
}

static void on_AA(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  const auto& cmd = check_size_t<C_SendQuestStatistic_V3_BB_AA>(data);

  if (c->version() == GameVersion::DC || c->version() == GameVersion::PC) {
    throw runtime_error("pre-V3 client sent update quest stats command");
  }
  if (c->config.check_flag(Client::Flag::IS_GC_TRIAL_EDITION)) {
    throw runtime_error("trial edition client sent update quest stats command");
  }

  auto l = c->require_lobby();
  if (!l->is_game() || !l->quest.get()) {
    return;
  }

  // TODO: Send the right value here. (When should we send function_id2?)
  send_quest_function_call(c, cmd.function_id1);
}

static void on_D7_GC(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  string filename(data);
  strip_trailing_zeroes(filename);
  if (filename.find('/') != string::npos) {
    send_command(c, 0xD7, 0x00);
  } else {
    try {
      static FileContentsCache gba_file_cache(300 * 1000 * 1000);
      auto f = gba_file_cache.get_or_load("system/gba/" + filename).file;
      send_open_quest_file(c, "", filename, f->data, QuestFileType::GBA_DEMO);
    } catch (const out_of_range&) {
      send_command(c, 0xD7, 0x00);
    } catch (const cannot_open_file&) {
      send_command(c, 0xD7, 0x00);
    }
  }
}

static void send_file_chunk(
    shared_ptr<Client> c,
    const string& filename,
    size_t chunk_index,
    bool is_download_quest) {
  shared_ptr<const string> data;
  try {
    data = c->sending_files.at(filename);
  } catch (const out_of_range&) {
    return;
  }

  size_t chunk_offset = chunk_index * 0x400;
  if (chunk_offset >= data->size()) {
    c->log.info("Done sending file %s", filename.c_str());
    c->sending_files.erase(filename);
  } else {
    const void* chunk_data = data->data() + (chunk_index * 0x400);
    size_t chunk_size = min<size_t>(data->size() - chunk_offset, 0x400);
    send_quest_file_chunk(c, filename, chunk_index, chunk_data, chunk_size, is_download_quest);
  }
}

static void on_44_A6_V3_BB(shared_ptr<Client> c, uint16_t command, uint32_t, string& data) {
  const auto& cmd = check_size_t<C_OpenFileConfirmation_44_A6>(data);
  send_file_chunk(c, cmd.filename.decode(), 0, (command == 0xA6));
}

static void on_13_A7_V3_BB(shared_ptr<Client> c, uint16_t command, uint32_t flag, string& data) {
  const auto& cmd = check_size_t<C_WriteFileConfirmation_V3_BB_13_A7>(data);
  send_file_chunk(c, cmd.filename.decode(), flag + 1, (command == 0xA7));
}

static void on_61_98(shared_ptr<Client> c, uint16_t command, uint32_t flag, string& data) {
  auto s = c->require_server_state();
  auto player = c->game_data.player();
  auto account = c->game_data.account();

  switch (c->version()) {
    case GameVersion::DC: {
      if (c->config.check_flag(Client::Flag::IS_DC_V1)) {
        const auto& pd = check_size_t<C_CharacterData_DCv1_61_98>(data);
        player->inventory = pd.inventory;
        player->disp = pd.disp.to_bb(player->inventory.language, player->inventory.language);
      } else {
        const auto& pd = check_size_t<C_CharacterData_DCv2_61_98>(data, 0xFFFF);
        player->inventory = pd.inventory;
        player->disp = pd.disp.to_bb(player->inventory.language, player->inventory.language);
        player->battle_records = pd.records.battle;
        player->challenge_records = pd.records.challenge;
        // TODO: Parse choice search config
      }
      break;
    }
    case GameVersion::PC: {
      const auto& pd = check_size_t<C_CharacterData_PC_61_98>(data, 0xFFFF);
      player->inventory = pd.inventory;
      player->disp = pd.disp.to_bb(player->inventory.language, player->inventory.language);
      player->battle_records = pd.records.battle;
      player->challenge_records = pd.records.challenge;
      // TODO: Parse choice search config
      account->blocked_senders = pd.blocked_senders;
      if (pd.auto_reply_enabled) {
        string auto_reply = data.substr(sizeof(pd));
        strip_trailing_zeroes(auto_reply);
        if (auto_reply.size() & 1) {
          auto_reply.push_back(0);
        }
        player->auto_reply.encode(tt_utf16_to_utf8(auto_reply), player->inventory.language);
      } else {
        player->auto_reply.clear();
      }
      break;
    }
    case GameVersion::GC:
    case GameVersion::XB: {
      const C_CharacterData_V3_61_98* cmd;
      if (flag == 4) { // Episode 3
        if (!c->config.check_flag(Client::Flag::IS_EPISODE_3)) {
          throw runtime_error("non-Episode 3 client sent Episode 3 player data");
        }
        const auto* pd3 = &check_size_t<C_CharacterData_GC_Ep3_61_98>(data);
        c->game_data.ep3_config.reset(new Episode3::PlayerConfig(pd3->ep3_config));
        cmd = reinterpret_cast<const C_CharacterData_V3_61_98*>(pd3);
      } else {
        if (c->config.check_flag(Client::Flag::IS_EPISODE_3)) {
          c->config.set_flag(Client::Flag::IS_EP3_TRIAL_EDITION);
          c->config.clear_flag(Client::Flag::ENCRYPTED_SEND_FUNCTION_CALL);
        }
        cmd = &check_size_t<C_CharacterData_V3_61_98>(data, 0xFFFF);
      }

      auto s = c->require_server_state();

      // We use the flag field in this command to differentiate between Ep3
      // Trial Edition and the final version: Trial Edition sends flag=3, and
      // the final version sends flag=4. Because the contents of the card list
      // update and the current tournament entry depend on this flag, we have
      // to delay sending them until now, instead of sending them during the
      // login sequence.
      if (c->config.check_flag(Client::Flag::IS_EPISODE_3)) {
        bool flags_changed = false;
        if (!c->config.check_flag(Client::Flag::HAS_EP3_CARD_DEFS)) {
          send_ep3_card_list_update(c);
          c->config.set_flag(Client::Flag::HAS_EP3_CARD_DEFS);
          flags_changed = true;
        }
        if (!c->config.check_flag(Client::Flag::HAS_EP3_MEDIA_UPDATES)) {
          for (const auto& banner : s->ep3_lobby_banners) {
            send_ep3_media_update(c, banner.type, banner.which, banner.data);
            c->config.set_flag(Client::Flag::HAS_EP3_MEDIA_UPDATES);
            flags_changed = true;
          }
        }
        s->ep3_tournament_index->link_client(c);
        if (flags_changed) {
          send_update_client_config(c);
        }
      }

      player->inventory = cmd->inventory;
      player->disp = cmd->disp.to_bb(player->inventory.language, player->inventory.language);
      player->battle_records = cmd->records.battle;
      player->challenge_records = cmd->records.challenge;
      // TODO: Parse choice search config
      player->info_board.encode(cmd->info_board.decode(player->inventory.language), player->inventory.language);
      account->blocked_senders = cmd->blocked_senders;
      if (cmd->auto_reply_enabled) {
        string auto_reply = data.substr(sizeof(cmd), 0xAC);
        strip_trailing_zeroes(auto_reply);
        string encoded = tt_decode_marked(auto_reply, player->inventory.language, false);
        player->auto_reply.encode(encoded, player->inventory.language);
      } else {
        player->auto_reply.clear();
      }
      break;
    }
    case GameVersion::BB: {
      const auto& cmd = check_size_t<C_CharacterData_BB_61_98>(data, 0xFFFF);
      // Note: we don't copy the inventory and disp here because we already have
      // them (we sent the player data to the client in the first place)
      player->battle_records = cmd.records.battle;
      player->challenge_records = cmd.records.challenge;
      // TODO: Parse choice search config
      player->info_board = cmd.info_board;
      account->blocked_senders = cmd.blocked_senders;
      if (cmd.auto_reply_enabled) {
        string auto_reply = data.substr(sizeof(cmd), 0xAC);
        strip_trailing_zeroes(auto_reply);
        if (auto_reply.size() & 1) {
          auto_reply.push_back(0);
        }
        player->auto_reply.encode(tt_utf16_to_utf8(auto_reply), player->inventory.language);
      } else {
        player->auto_reply.clear();
      }
      break;
    }
    default:
      throw logic_error("player data command not implemented for version");
  }
  player->inventory.decode_mags(c->version());
  c->channel.language = player->inventory.language;

  string name_str = player->disp.name.decode(c->language());
  c->channel.name = string_printf("C-%" PRIX64 " (%s)", c->id, name_str.c_str());

  // 98 should only be sent when leaving a game, and we should leave the client
  // in no lobby (they will send an 84 soon afterward to choose a lobby).
  if (command == 0x98) {
    // If the client had an overlay (for battle/challenge modes), delete it
    c->game_data.delete_overlay();

    s->remove_client_from_lobby(c);

  } else if (command == 0x61) {
    if (!c->pending_bb_save_username.empty()) {
      string prev_bb_username = c->game_data.bb_username;
      size_t prev_bb_player_index = c->game_data.bb_player_index;

      c->game_data.bb_username = c->pending_bb_save_username;
      c->game_data.bb_player_index = c->pending_bb_save_player_index;

      bool failure = false;
      try {
        c->game_data.save_player_data();
      } catch (const exception& e) {
        send_text_message_printf(c, "$C6PSOBB player data could\nnot be saved:\n%s", e.what());
        failure = true;
      }

      if (!failure) {
        send_text_message_printf(c,
            "$C6BB player data saved\nas player %hhu for user\n%s",
            static_cast<uint8_t>(c->pending_bb_save_player_index + 1),
            c->pending_bb_save_username.c_str());
      }

      c->game_data.bb_username = prev_bb_username;
      c->game_data.bb_player_index = prev_bb_player_index;

      c->pending_bb_save_username.clear();
    }

    // We use 61 during the lobby server init sequence to trigger joining an
    // available lobby
    if (!c->lobby.lock() && (c->server_behavior == ServerBehavior::LOBBY_SERVER)) {
      s->add_client_to_available_lobby(c);
    }
  }
}

static void on_6x_C9_CB(shared_ptr<Client> c, uint16_t command, uint32_t flag, string& data) {
  check_size_v(data.size(), 4, 0xFFFF);
  if ((data.size() > 0x400) && (command != 0x6C) && (command != 0x6D)) {
    throw runtime_error("non-extended game command data size is too large");
  }
  on_subcommand_multi(c, command, flag, data);
}

static void on_06(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  const auto& cmd = check_size_t<SC_TextHeader_01_06_11_B0_EE>(data, 0xFFFF);
  string text = data.substr(sizeof(cmd));
  strip_trailing_zeroes(text);
  if (text.empty()) {
    return;
  }
  bool is_w = (c->version() == GameVersion::PC || c->version() == GameVersion::BB);
  if (is_w && (text.size() & 1)) {
    text.push_back(0);
  }

  auto l = c->lobby.lock();
  char private_flags = 0;
  if ((c->version() == GameVersion::GC) && c->config.check_flag(Client::Flag::IS_EPISODE_3) && l && l->is_ep3() && (text[0] != '\t')) {
    private_flags = text[0];
    text = text.substr(1);
  }

  text = tt_decode_marked(text, c->language(), is_w);
  if (text.empty()) {
    return;
  }

  if (text[0] == '$') {
    if (text[1] == '$') {
      text = text.substr(1);
    } else {
      on_chat_command(c, text);
      return;
    }
  }

  if (!l || !c->can_chat) {
    return;
  }

  auto p = c->game_data.player();
  string from_name = p->disp.name.decode(c->language());
  if (from_name.size() >= 2 && from_name[0] == '\t' && (from_name[1] == 'E' || from_name[1] == 'J')) {
    from_name = from_name.substr(2);
  }
  for (size_t x = 0; x < l->max_clients; x++) {
    if (l->clients[x]) {
      send_chat_message(l->clients[x], c->license->serial_number, from_name, text, private_flags);
    }
  }
  for (const auto& watcher_l : l->watcher_lobbies) {
    for (size_t x = 0; x < watcher_l->max_clients; x++) {
      if (watcher_l->clients[x]) {
        send_chat_message(watcher_l->clients[x], c->license->serial_number, from_name, text, private_flags);
      }
    }
  }

  if (l->battle_record && l->battle_record->battle_in_progress()) {
    auto prepared_message = prepare_chat_data(
        c->version(),
        c->config.check_flag(Client::Flag::IS_DC_TRIAL_EDITION),
        c->language(),
        c->lobby_client_id,
        p->disp.name.decode(c->language()),
        text,
        private_flags);
    l->battle_record->add_chat_message(c->license->serial_number, std::move(prepared_message));
  }
}

static void on_00E0_BB(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  check_size_v(data.size(), 0);
  send_system_file_bb(c);
}

static void on_00E3_BB(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  const auto& cmd = check_size_t<C_PlayerPreviewRequest_BB_E3>(data);

  if (c->bb_connection_phase != 0x00) {
    send_approve_player_choice_bb(c);

  } else {
    if (!c->license) {
      c->should_disconnect = true;
      return;
    }

    ClientGameData temp_gd;
    temp_gd.guild_card_number = c->license->serial_number;
    temp_gd.bb_username = c->license->bb_username;
    temp_gd.bb_player_index = cmd.player_index;

    try {
      auto preview = temp_gd.player()->disp.to_preview();
      send_player_preview_bb(c, cmd.player_index, &preview);

    } catch (const exception& e) {
      // Player doesn't exist
      send_player_preview_bb(c, cmd.player_index, nullptr);
    }
  }
}

static void on_00E8_BB(shared_ptr<Client> c, uint16_t command, uint32_t, string& data) {
  constexpr size_t max_count = sizeof(PSOBBGuildCardFile::entries) / sizeof(PSOBBGuildCardFile::Entry);
  constexpr size_t max_blocked = sizeof(PSOBBGuildCardFile::blocked) / sizeof(GuildCardBB);
  switch (command) {
    case 0x01E8: { // Check guild card file checksum
      const auto& cmd = check_size_t<C_GuildCardChecksum_01E8>(data);
      uint32_t checksum = c->game_data.account()->guild_card_file.checksum();
      c->log.info("(Guild card file) Server checksum = %08" PRIX32 ", client checksum = %08" PRIX32,
          checksum, cmd.checksum.load());
      S_GuildCardChecksumResponse_BB_02E8 response = {
          (cmd.checksum != checksum), 0};
      send_command_t(c, 0x02E8, 0x00000000, response);
      break;
    }
    case 0x03E8: // Download guild card file
      check_size_v(data.size(), 0);
      send_guild_card_header_bb(c);
      break;
    case 0x04E8: { // Add guild card
      auto& new_gc = check_size_t<GuildCardBB>(data);
      auto& gcf = c->game_data.account()->guild_card_file;
      for (size_t z = 0; z < max_count; z++) {
        if (!gcf.entries[z].data.present) {
          gcf.entries[z].data = new_gc;
          gcf.entries[z].unknown_a1.clear(0);
          c->log.info("Added guild card %" PRIu32 " at position %zu",
              new_gc.guild_card_number.load(), z);
          break;
        }
      }
      break;
    }
    case 0x05E8: { // Delete guild card
      auto& cmd = check_size_t<C_DeleteGuildCard_BB_05E8_08E8>(data);
      auto& gcf = c->game_data.account()->guild_card_file;
      for (size_t z = 0; z < max_count; z++) {
        if (gcf.entries[z].data.guild_card_number == cmd.guild_card_number) {
          c->log.info("Deleted guild card %" PRIu32 " at position %zu",
              cmd.guild_card_number.load(), z);
          for (z = 0; z < max_count - 1; z++) {
            gcf.entries[z] = gcf.entries[z + 1];
          }
          gcf.entries[max_count - 1].clear();
          break;
        }
      }
      break;
    }
    case 0x06E8: { // Update guild card
      auto& new_gc = check_size_t<GuildCardBB>(data);
      auto& gcf = c->game_data.account()->guild_card_file;
      for (size_t z = 0; z < max_count; z++) {
        if (gcf.entries[z].data.guild_card_number == new_gc.guild_card_number) {
          gcf.entries[z].data = new_gc;
          c->log.info("Updated guild card %" PRIu32 " at position %zu",
              new_gc.guild_card_number.load(), z);
        }
      }
      break;
    }
    case 0x07E8: { // Add blocked user
      auto& new_gc = check_size_t<GuildCardBB>(data);
      auto& gcf = c->game_data.account()->guild_card_file;
      for (size_t z = 0; z < max_blocked; z++) {
        if (!gcf.blocked[z].present) {
          gcf.blocked[z] = new_gc;
          c->log.info("Added blocked guild card %" PRIu32 " at position %zu",
              new_gc.guild_card_number.load(), z);
          // Note: The client also sends a C6 command, so we don't have to
          // manually sync the actual blocked senders list here
          break;
        }
      }
      break;
    }
    case 0x08E8: { // Delete blocked user
      auto& cmd = check_size_t<C_DeleteGuildCard_BB_05E8_08E8>(data);
      auto& gcf = c->game_data.account()->guild_card_file;
      for (size_t z = 0; z < max_blocked; z++) {
        if (gcf.blocked[z].guild_card_number == cmd.guild_card_number) {
          c->log.info("Deleted blocked guild card %" PRIu32 " at position %zu",
              cmd.guild_card_number.load(), z);
          for (z = 0; z < max_blocked - 1; z++) {
            gcf.blocked[z] = gcf.blocked[z + 1];
          }
          gcf.blocked[max_blocked - 1].clear();
          // Note: The client also sends a C6 command, so we don't have to
          // manually sync the actual blocked senders list here
          break;
        }
      }
      break;
    }
    case 0x09E8: { // Write comment
      auto& cmd = check_size_t<C_WriteGuildCardComment_BB_09E8>(data);
      auto& gcf = c->game_data.account()->guild_card_file;
      for (size_t z = 0; z < max_count; z++) {
        if (gcf.entries[z].data.guild_card_number == cmd.guild_card_number) {
          gcf.entries[z].comment = cmd.comment;
          c->log.info("Updated comment on guild card %" PRIu32 " at position %zu",
              cmd.guild_card_number.load(), z);
          break;
        }
      }
      break;
    }
    case 0x0AE8: { // Move guild card in list
      auto& cmd = check_size_t<C_MoveGuildCard_BB_0AE8>(data);
      auto& gcf = c->game_data.account()->guild_card_file;
      if (cmd.position >= max_count) {
        throw invalid_argument("invalid new position");
      }
      size_t index;
      for (index = 0; index < max_count; index++) {
        if (gcf.entries[index].data.guild_card_number == cmd.guild_card_number) {
          break;
        }
      }
      if (index >= max_count) {
        throw invalid_argument("player does not have requested guild card");
      }
      auto moved_gc = gcf.entries[index];
      for (; index < cmd.position; index++) {
        gcf.entries[index] = gcf.entries[index + 1];
      }
      for (; index > cmd.position; index--) {
        gcf.entries[index] = gcf.entries[index - 1];
      }
      gcf.entries[index] = moved_gc;
      c->log.info("Moved guild card %" PRIu32 " to position %zu",
          cmd.guild_card_number.load(), index);
      break;
    }
    default:
      throw invalid_argument("invalid command");
  }
}

static void on_DC_BB(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  const auto& cmd = check_size_t<C_GuildCardDataRequest_BB_03DC>(data);
  if (cmd.cont) {
    send_guild_card_chunk_bb(c, cmd.chunk_index);
  }
}

static void on_xxEB_BB(shared_ptr<Client> c, uint16_t command, uint32_t flag, string& data) {
  check_size_v(data.size(), 0);

  if (command == 0x04EB) {
    send_stream_file_index_bb(c);
  } else if (command == 0x03EB) {
    send_stream_file_chunk_bb(c, flag);
  } else {
    throw invalid_argument("unimplemented command");
  }
}

static void on_00EC_BB(shared_ptr<Client>, uint16_t, uint32_t, string& data) {
  check_size_t<C_LeaveCharacterSelect_BB_00EC>(data);
}

static void on_00E5_BB(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  const auto& cmd = check_size_t<SC_PlayerPreview_CreateCharacter_BB_00E5>(data);

  if (!c->license) {
    send_message_box(c, "$C6You are not logged in.");
    return;
  }

  if (c->game_data.player(false).get()) {
    throw runtime_error("player already exists");
  }

  c->game_data.bb_player_index = cmd.player_index;

  if (c->bb_connection_phase == 0x03) { // Dressing room
    try {
      c->game_data.player()->disp.apply_dressing_room(cmd.preview);
    } catch (const exception& e) {
      send_message_box(c, string_printf("$C6Character could not be modified:\n%s", e.what()));
      return;
    }
  } else {
    try {
      auto s = c->require_server_state();
      c->game_data.create_player(cmd.preview, s->level_table);
    } catch (const exception& e) {
      send_message_box(c, string_printf("$C6New character could not be created:\n%s", e.what()));
      return;
    }
  }
  c->log.info("Cleared dressing room flag for account");

  send_approve_player_choice_bb(c);
}

static void on_xxED_BB(shared_ptr<Client> c, uint16_t command, uint32_t, string& data) {
  switch (command) {
    case 0x01ED: {
      const auto& cmd = check_size_t<C_UpdateAccountOptionFlags_BB_01ED>(data);
      c->game_data.account()->option_flags = cmd.option_flags;
      break;
    }
    case 0x02ED: {
      const auto& cmd = check_size_t<C_UpdateAccountSymbolChats_BB_02ED>(data);
      c->game_data.account()->symbol_chats = cmd.symbol_chats;
      break;
    }
    case 0x03ED: {
      const auto& cmd = check_size_t<C_UpdateAccountChatShortcuts_BB_03ED>(data);
      c->game_data.account()->shortcuts = cmd.chat_shortcuts;
      break;
    }
    case 0x04ED: {
      const auto& cmd = check_size_t<C_UpdateAccountKeyConfig_BB_04ED>(data);
      c->game_data.account()->system_file.key_config = cmd.key_config;
      break;
    }
    case 0x05ED: {
      const auto& cmd = check_size_t<C_UpdateAccountPadConfig_BB_05ED>(data);
      c->game_data.account()->system_file.joystick_config = cmd.pad_config;
      break;
    }
    case 0x06ED: {
      const auto& cmd = check_size_t<C_UpdateAccountTechMenu_BB_06ED>(data);
      c->game_data.player()->tech_menu_config = cmd.tech_menu;
      break;
    }
    case 0x07ED: {
      const auto& cmd = check_size_t<C_UpdateAccountCustomizeMenu_BB_07ED>(data);
      c->game_data.player()->disp.config = cmd.customize;
      break;
    }
    case 0x08ED: {
      check_size_t<C_UpdateAccountChallengeAndBattleConfig_BB_08ED>(data);
      // TODO: Save this data appropriately
      break;
    }
    default:
      throw invalid_argument("unknown account command");
  }
}

static void on_00E7_BB(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  const auto& cmd = check_size_t<SC_SyncCharacterSaveFile_BB_00E7>(data);

  // We only trust the player's quest data and challenge data.
  // TODO: In the future, we shouldn't even need to trust these fields. We
  // should instead verify our copy of the player against what the client sent,
  // and alert on anything that's out of sync.
  // TODO: In the future, we should save battle records here too.
  auto p = c->game_data.player();
  p->challenge_records = cmd.challenge_records;
  p->quest_data1 = cmd.quest_data1;
  p->quest_data2 = cmd.quest_data2;
}

static void on_00E2_BB(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  auto& cmd = check_size_t<PSOBBSystemFile>(data);
  c->game_data.account()->system_file = cmd;
}

static void on_89(shared_ptr<Client> c, uint16_t, uint32_t flag, string& data) {
  check_size_v(data.size(), 0);

  c->lobby_arrow_color = flag;
  auto l = c->lobby.lock();
  if (l) {
    send_arrow_update(l);
  }
}

static void on_40(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  const auto& cmd = check_size_t<C_GuildCardSearch_40>(data);
  try {
    auto s = c->require_server_state();
    auto result = s->find_client(nullptr, cmd.target_guild_card_number);
    auto result_lobby = result->lobby.lock();
    if (result_lobby) {
      send_card_search_result(c, result, result_lobby);
    }
  } catch (const out_of_range&) {
  }
}

static void on_C0(shared_ptr<Client> c, uint16_t, uint32_t, string&) {
  // TODO: Implement choice search.
  send_text_message(c, "$C6Choice Search is\nnot supported");
}

static void on_81(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  string message;
  uint32_t to_guild_card_number;
  switch (c->version()) {
    case GameVersion::DC:
    case GameVersion::GC:
    case GameVersion::XB: {
      const auto& cmd = check_size_t<SC_SimpleMail_DC_V3_81>(data);
      to_guild_card_number = cmd.to_guild_card_number;
      message = cmd.text.decode(c->language());
      break;
    }
    case GameVersion::PC: {
      const auto& cmd = check_size_t<SC_SimpleMail_PC_81>(data);
      to_guild_card_number = cmd.to_guild_card_number;
      message = cmd.text.decode(c->language());
      break;
    }
    case GameVersion::BB: {
      const auto& cmd = check_size_t<SC_SimpleMail_BB_81>(data);
      to_guild_card_number = cmd.to_guild_card_number;
      message = cmd.text.decode(c->language());
      break;
    }
    default:
      throw logic_error("invalid game version");
  }

  shared_ptr<Client> target;
  try {
    target = c->require_server_state()->find_client(nullptr, to_guild_card_number);
  } catch (const out_of_range&) {
  }

  if (!target) {
    // TODO: We should store pending messages for accounts somewhere, and send
    // them when the player signs on again. We should also persist the player's
    // autoreply setting when they're offline and use it if appropriate here.
    send_text_message(c, "$C6Player is offline");

  } else {
    // If the sender is blocked, don't forward the mail
    for (size_t y = 0; y < 30; y++) {
      if (target->game_data.account()->blocked_senders.data()[y] == c->license->serial_number) {
        return;
      }
    }

    // If the target has auto-reply enabled, send the autoreply. Note that we also
    // forward the message in this case.
    auto target_p = target->game_data.player();
    if (!target_p->auto_reply.empty()) {
      send_simple_mail(
          c,
          target->license->serial_number,
          target_p->disp.name.decode(target_p->inventory.language),
          target_p->auto_reply.decode(target_p->inventory.language));
    }

    // Forward the message
    send_simple_mail(
        target,
        c->license->serial_number,
        c->game_data.player()->disp.name.decode(c->language()),
        message);
  }
}

static void on_D8(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  check_size_v(data.size(), 0);
  send_info_board(c);
}

void on_D9(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  strip_trailing_zeroes(data);
  bool is_w = (c->version() == GameVersion::PC || c->version() == GameVersion::BB);
  if (is_w && (data.size() & 1)) {
    data.push_back(0);
  }
  c->game_data.player(true, false)->info_board.encode(tt_decode_marked(data, c->language(), is_w), c->language());
}

void on_C7(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  strip_trailing_zeroes(data);
  bool is_w = (c->version() == GameVersion::PC || c->version() == GameVersion::BB);
  if (is_w && (data.size() & 1)) {
    data.push_back(0);
  }
  c->game_data.player(true, false)->auto_reply.encode(tt_decode_marked(data, c->language(), is_w), c->language());
}

static void on_C8(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  check_size_v(data.size(), 0);
  c->game_data.player(true, false)->auto_reply.clear();
}

static void on_C6(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  if (c->version() == GameVersion::BB) {
    const auto& cmd = check_size_t<C_SetBlockedSenders_BB_C6>(data);
    c->game_data.account()->blocked_senders = cmd.blocked_senders;
  } else {
    const auto& cmd = check_size_t<C_SetBlockedSenders_V3_C6>(data);
    c->game_data.account()->blocked_senders = cmd.blocked_senders;
  }
}

shared_ptr<Lobby> create_game_generic(
    shared_ptr<ServerState> s,
    shared_ptr<Client> c,
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

  auto current_lobby = c->require_lobby();

  uint8_t min_level;
  // A player's actual level is their displayed level - 1, so the minimums for
  // Episode 1 (for example) are actually 1, 20, 40, 80.
  switch (episode) {
    case Episode::EP1: {
      static const uint32_t min_levels[4] = {0, 19, 39, 79};
      min_level = min_levels[difficulty];
      break;
    }
    case Episode::EP2: {
      static const uint32_t min_levels[4] = {0, 29, 49, 89};
      min_level = min_levels[difficulty];
      break;
    }
    case Episode::EP3:
      min_level = 0;
      break;
    case Episode::EP4: {
      static const uint32_t min_levels[4] = {0, 39, 79, 109};
      min_level = min_levels[difficulty];
      break;
    }
    default:
      throw runtime_error("invalid episode");
  }

  auto p = c->game_data.player();
  if (!(c->license->flags & License::Flag::FREE_JOIN_GAMES) &&
      (min_level > p->disp.stats.level)) {
    // Note: We don't throw here because this is a situation players might
    // actually encounter while playing the game normally
    send_lobby_message_box(c, "Your level is too\nlow for this\ndifficulty");
    return nullptr;
  }

  shared_ptr<Lobby> game = s->create_lobby();
  game->name = name;
  game->set_flag(Lobby::Flag::GAME);
  if ((c->version() == GameVersion::GC) &&
      c->config.check_flag(Client::Flag::IS_EPISODE_3) &&
      c->config.check_flag(Client::Flag::IS_EP3_TRIAL_EDITION)) {
    game->set_flag(Lobby::Flag::IS_EP3_TRIAL);
  }
  if ((c->version() == GameVersion::BB) || s->item_tracking_enabled) {
    game->set_flag(Lobby::Flag::ITEM_TRACKING_ENABLED);
  }
  // Only disable drops if the config flag is set and for regular multi-mode;
  // drops are still enabled for battle and challenge modes
  if (s->behavior_enabled(s->enable_drops_behavior) || (mode != GameMode::NORMAL)) {
    game->set_flag(Lobby::Flag::DROPS_ENABLED);
  }
  if (s->behavior_enabled(s->cheat_mode_behavior)) {
    game->set_flag(Lobby::Flag::CHEATS_ENABLED);
  }
  if (!s->behavior_can_be_overridden(s->enable_drops_behavior)) {
    game->set_flag(Lobby::Flag::CANNOT_CHANGE_DROPS_ENABLED);
  }
  if (!game->check_flag(Lobby::Flag::ITEM_TRACKING_ENABLED) || !s->behavior_can_be_overridden(s->use_server_item_tables_behavior)) {
    game->set_flag(Lobby::Flag::CANNOT_CHANGE_ITEM_TABLE);
  }
  if (!s->behavior_can_be_overridden(s->cheat_mode_behavior)) {
    game->set_flag(Lobby::Flag::CANNOT_CHANGE_CHEAT_MODE);
  }
  if (watched_lobby || battle_player) {
    game->set_flag(Lobby::Flag::IS_SPECTATOR_TEAM);
  }
  game->password = password;

  game->base_version = c->version();
  game->allowed_versions = 0;
  switch (c->quest_version()) {
    case QuestScriptVersion::DC_NTE:
      game->allow_version(QuestScriptVersion::DC_NTE);
      break;
    case QuestScriptVersion::DC_V1:
      game->allow_version(QuestScriptVersion::DC_V1);
      game->allow_version(QuestScriptVersion::DC_V2);
      if (s->allow_dc_pc_games) {
        game->allow_version(QuestScriptVersion::PC_V2);
      }
      break;
    case QuestScriptVersion::DC_V2:
    case QuestScriptVersion::PC_V2:
      if (allow_v1 && (difficulty <= 2)) {
        game->allow_version(QuestScriptVersion::DC_V1);
      }
      game->allow_version(QuestScriptVersion::DC_V2);
      if (s->allow_dc_pc_games) {
        game->allow_version(QuestScriptVersion::PC_V2);
      }
      break;
    case QuestScriptVersion::GC_NTE:
      game->allow_version(QuestScriptVersion::GC_NTE);
      break;
    case QuestScriptVersion::GC_V3:
      game->allow_version(QuestScriptVersion::GC_V3);
      if (s->allow_gc_xb_games) {
        game->allow_version(QuestScriptVersion::XB_V3);
      }
      break;
    case QuestScriptVersion::XB_V3:
      game->allow_version(QuestScriptVersion::XB_V3);
      if (s->allow_gc_xb_games) {
        game->allow_version(QuestScriptVersion::GC_V3);
      }
      break;
    case QuestScriptVersion::GC_EP3:
      game->allow_version(QuestScriptVersion::GC_EP3);
      break;
    case QuestScriptVersion::BB_V4:
      game->allow_version(QuestScriptVersion::BB_V4);
      break;
    default:
      throw logic_error("invalid quest script version");
  }

  game->section_id = (c->config.override_section_id != 0xFF)
      ? c->config.override_section_id
      : p->disp.visual.section_id;
  game->episode = episode;
  game->mode = mode;
  game->difficulty = difficulty;
  if (c->config.check_flag(Client::Flag::USE_OVERRIDE_RANDOM_SEED)) {
    game->random_seed = c->config.override_random_seed;
  }
  game->random_crypt.reset(new PSOV2Encryption(game->random_seed));
  if (battle_player) {
    game->battle_player = battle_player;
    battle_player->set_lobby(game);
  }

  for (size_t x = 0; x < 4; x++) {
    game->next_item_id[x] = (0x00200000 * x) + 0x00010000;
  }
  game->next_game_item_id = 0x00810000;
  if ((game->base_version == GameVersion::BB) ||
      (game->check_flag(Lobby::Flag::ITEM_TRACKING_ENABLED) && s->behavior_enabled(s->use_server_item_tables_behavior))) {
    game->create_item_creator();
  }

  game->event = Lobby::game_event_for_lobby_event(current_lobby->event);
  game->block = 0xFF;
  game->max_clients = game->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM) ? 12 : 4;
  game->min_level = min_level;
  game->max_level = 0xFFFFFFFF;
  if (watched_lobby) {
    game->watched_lobby = watched_lobby;
    watched_lobby->watcher_lobbies.emplace(game);
  }

  bool is_solo = (game->mode == GameMode::SOLO);

  // Generate the map variations
  if (game->is_ep3() ||
      (c->version() == GameVersion::DC &&
          (c->config.check_flag(Client::Flag::IS_DC_TRIAL_EDITION) || c->config.check_flag(Client::Flag::IS_DC_V1_PROTOTYPE)))) {
    game->variations.clear(0);
  } else {
    generate_variations(game->variations, game->random_crypt, game->episode, is_solo);
  }

  if (game->base_version == GameVersion::BB) {
    game->map.reset(new Map());
    for (size_t area = 0; area < 0x10; area++) {
      c->log.info("[Map/%zu] Using variations %" PRIX32 ", %" PRIX32,
          area, game->variations[area * 2].load(), game->variations[area * 2 + 1].load());
      auto filenames = map_filenames_for_variation(
          game->episode,
          is_solo,
          area,
          game->variations[area * 2],
          game->variations[area * 2 + 1]);

      if (filenames.empty()) {
        c->log.info("[Map/%zu] No file to load", area);
        continue;
      }
      bool any_map_loaded = false;
      for (const string& filename : filenames) {
        try {
          auto map_data = s->load_bb_file(filename, "", "map/" + filename);
          size_t start_offset = game->map->enemies.size();
          game->map->add_enemies_from_map_data(
              game->episode, game->difficulty, game->event, map_data->data(), map_data->size());
          size_t entries_loaded = game->map->enemies.size() - start_offset;
          c->log.info("[Map/%zu] Loaded %s (%zu entries)",
              area, filename.c_str(), entries_loaded);
          for (size_t z = start_offset; z < game->map->enemies.size(); z++) {
            string e_str = game->map->enemies[z].str();
            static_game_data_log.info("(Entry %zX) %s", z, e_str.c_str());
          }
          any_map_loaded = true;
          break;
        } catch (const exception& e) {
          c->log.info("[Map/%zu] Failed to load %s: %s", area, filename.c_str(), e.what());
        }
      }
      if (!any_map_loaded) {
        throw runtime_error(string_printf("no maps loaded for area %zu", area));
      }
    }

    c->log.info("Loaded maps contain %zu entries overall (%zu as rares)",
        game->map->enemies.size(), game->map->rare_enemy_indexes.size());
  }
  return game;
}

static void on_C1_PC(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  const auto& cmd = check_size_t<C_CreateGame_PC_C1>(data);
  auto s = c->require_server_state();

  GameMode mode = GameMode::NORMAL;
  if (cmd.battle_mode) {
    mode = GameMode::BATTLE;
  } else if (cmd.challenge_mode) {
    mode = GameMode::CHALLENGE;
  }
  auto game = create_game_generic(s, c, cmd.name.decode(c->language()), cmd.password.decode(c->language()), Episode::EP1, mode, cmd.difficulty);
  if (game) {
    s->change_client_lobby(c, game);
    c->config.set_flag(Client::Flag::LOADING);
  }
}

static void on_0C_C1_E7_EC(shared_ptr<Client> c, uint16_t command, uint32_t, string& data) {
  auto s = c->require_server_state();

  shared_ptr<Lobby> game;
  if ((c->version() == GameVersion::DC) &&
      (c->config.check_flag(Client::Flag::IS_DC_TRIAL_EDITION) || c->config.check_flag(Client::Flag::IS_DC_V1_PROTOTYPE))) {
    const auto& cmd = check_size_t<C_CreateGame_DCNTE<TextEncoding::SJIS>>(data);
    game = create_game_generic(s, c, cmd.name.decode(c->language()), cmd.password.decode(c->language()), Episode::EP1, GameMode::NORMAL, 0, true);

  } else {
    const auto& cmd = check_size_t<C_CreateGame_DC_V3_0C_C1_Ep3_EC>(data);

    // Only allow E7/EC from Ep3 clients
    bool client_is_ep3 = c->config.check_flag(Client::Flag::IS_EPISODE_3);
    if (((command & 0xF0) == 0xE0) != client_is_ep3) {
      throw runtime_error("invalid command");
    }

    Episode episode = Episode::NONE;
    bool allow_v1 = false;
    if (c->version() == GameVersion::DC) {
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
    if (command == 0xE7) {
      if (cmd.menu_id != MenuID::GAME) {
        throw runtime_error("incorrect menu ID");
      }
      watched_lobby = s->find_lobby(cmd.item_id);
      if (!watched_lobby) {
        send_lobby_message_box(c, "$C6This game no longer\nexists");
        return;
      }
      if (watched_lobby->check_flag(Lobby::Flag::SPECTATORS_FORBIDDEN)) {
        send_lobby_message_box(c, "$C6This game does not\nallow spectators");
        return;
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
    c->config.set_flag(Client::Flag::LOADING);
  }
}

static void on_C1_BB(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  const auto& cmd = check_size_t<C_CreateGame_BB_C1>(data);
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
      break;
    default:
      throw runtime_error("invalid episode number");
  }

  auto game = create_game_generic(s, c, cmd.name.decode(c->language()), cmd.password.decode(c->language()), episode, mode, cmd.difficulty);
  if (game) {
    s->change_client_lobby(c, game);
    c->config.set_flag(Client::Flag::LOADING);
  }
}

static void on_8A(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  if ((c->version() == GameVersion::DC) && c->config.check_flag(Client::Flag::IS_DC_TRIAL_EDITION)) {
    const auto& cmd = check_size_t<C_ConnectionInfo_DCNTE_8A>(data);
    set_console_client_flags(c, cmd.sub_version);
    send_command(c, 0x8A, 0x01);

  } else {
    check_size_v(data.size(), 0);
    auto l = c->require_lobby();
    send_lobby_name(c, l->name.c_str());
  }
}

static void on_6F(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  check_size_v(data.size(), 0);

  auto l = c->require_lobby();
  if (!l->is_game()) {
    throw runtime_error("client sent ready command outside of game");
  }
  c->config.clear_flag(Client::Flag::LOADING);

  send_resume_game(l, c);
  if (l->base_version == GameVersion::BB) {
    send_set_exp_multiplier(l);
  }
  send_server_time(c);
  // Only get player info again on BB, since on other versions the returned info
  // only includes items that would be saved if the client disconnects
  // unexpectedly (that is, only equipped items are included).
  if (c->version() == GameVersion::BB) {
    send_get_player_info(c);
    if (l->check_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS)) {
      if (!l->quest) {
        throw runtime_error("JOINABLE_QUEST_IN_PROGRESS is set, but lobby has no quest");
      }
      auto vq = l->quest->version(c->quest_version(), c->language());
      if (!vq) {
        throw runtime_error("JOINABLE_QUEST_IN_PROGRESS is set, but lobby has no quest for client version");
      }
      string bin_basename = vq->bin_filename();
      string dat_basename = vq->dat_filename();

      send_open_quest_file(c, bin_basename + ".bin", bin_basename, vq->bin_contents, QuestFileType::ONLINE);
      send_open_quest_file(c, dat_basename + ".dat", dat_basename, vq->dat_contents, QuestFileType::ONLINE);
      c->config.set_flag(Client::Flag::LOADING_RUNNING_JOINABLE_QUEST);
    } else if (l->map) {
      send_rare_enemy_index_list(c, l->map->rare_enemy_indexes);
    }
  }

  // Handle initial commands for spectator teams
  auto watched_lobby = l->watched_lobby.lock();
  if (l->battle_player && l->check_flag(Lobby::Flag::START_BATTLE_PLAYER_IMMEDIATELY)) {
    l->battle_player->start();
  } else if (watched_lobby && watched_lobby->ep3_server) {
    if (!watched_lobby->ep3_server->battle_finished) {
      watched_lobby->ep3_server->send_commands_for_joining_spectator(
          c->channel, c->config.check_flag(Client::Flag::IS_EP3_TRIAL_EDITION));
    }
    send_ep3_update_game_metadata(watched_lobby);
  }

  // If there are more players to bring in, try to do so
  c->disconnect_hooks.erase(ADD_NEXT_CLIENT_DISCONNECT_HOOK_NAME);
  add_next_game_client(l);
}

static void on_99_GC(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  check_size_v(data.size(), 0);

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
}

static void on_D0_V3_BB(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  const auto& cmd = check_size_t<SC_TradeItems_D0_D3>(data);

  if (c->game_data.pending_item_trade) {
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

  c->game_data.pending_item_trade.reset(new PendingItemTrade());
  c->game_data.pending_item_trade->other_client_id = cmd.target_client_id;
  for (size_t x = 0; x < cmd.item_count; x++) {
    auto& item = c->game_data.pending_item_trade->items.emplace_back(cmd.item_datas[x]);
    item.decode_if_mag(c->version());
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
  if (target_c->game_data.pending_item_trade) {
    send_command(c, 0xD1, 0x00);
  }
}

static void on_D2_V3_BB(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  check_size_v(data.size(), 0);

  if (!c->game_data.pending_item_trade) {
    throw runtime_error("player executed a trade with none pending");
  }

  auto l = c->require_lobby();
  if (!l->is_game()) {
    throw runtime_error("trade command received in non-game lobby");
  }
  auto target_c = l->clients.at(c->game_data.pending_item_trade->other_client_id);
  if (!target_c) {
    throw runtime_error("target player is missing");
  }
  if (!target_c->game_data.pending_item_trade) {
    throw runtime_error("player executed a trade with no other side pending");
  }

  c->game_data.pending_item_trade->confirmed = true;
  if (target_c->game_data.pending_item_trade->confirmed) {
    send_execute_item_trade(c, target_c->game_data.pending_item_trade->items);
    send_execute_item_trade(target_c, c->game_data.pending_item_trade->items);
    send_command(c, 0xD4, 0x01);
    send_command(target_c, 0xD4, 0x01);
    c->game_data.pending_item_trade.reset();
    target_c->game_data.pending_item_trade.reset();
  }
}

static void on_D4_V3_BB(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  check_size_v(data.size(), 0);

  // Annoyingly, if the other client disconnects at a certain point during the
  // trade sequence, the client can get into a state where it sends this command
  // many times in a row. To deal with this, we just do nothing if the client
  // has no trade pending.
  if (!c->game_data.pending_item_trade) {
    return;
  }
  uint8_t other_client_id = c->game_data.pending_item_trade->other_client_id;
  c->game_data.pending_item_trade.reset();
  send_command(c, 0xD4, 0x00);

  // Cancel the other side of the trade too, if it's open
  auto l = c->require_lobby();
  if (!l->is_game()) {
    throw runtime_error("trade command received in non-game lobby");
  }
  auto target_c = l->clients.at(other_client_id);
  if (!target_c) {
    return;
  }
  if (!target_c->game_data.pending_item_trade) {
    return;
  }
  target_c->game_data.pending_item_trade.reset();
  send_command(target_c, 0xD4, 0x00);
}

static void on_EE_Ep3(shared_ptr<Client> c, uint16_t, uint32_t flag, string& data) {
  if (!c->config.check_flag(Client::Flag::IS_EPISODE_3)) {
    throw runtime_error("non-Ep3 client sent card trade command");
  }
  auto l = c->require_lobby();
  if (!l->is_game() || !l->is_ep3()) {
    throw runtime_error("client sent card trade command outside of Ep3 game");
  }

  if (flag == 0xD0) {
    auto& cmd = check_size_t<SC_TradeCards_GC_Ep3_EE_FlagD0_FlagD3>(data);

    if (c->game_data.pending_card_trade) {
      throw runtime_error("player started a card trade when one is already pending");
    }
    if (cmd.entry_count > 4) {
      throw runtime_error("invalid entry count in card trade command");
    }

    auto target_c = l->clients.at(cmd.target_client_id);
    if (!target_c) {
      throw runtime_error("card trade command sent to missing player");
    }
    if (!target_c->config.check_flag(Client::Flag::IS_EPISODE_3)) {
      throw runtime_error("card trade target is not Episode 3");
    }

    c->game_data.pending_card_trade.reset(new PendingCardTrade());
    c->game_data.pending_card_trade->other_client_id = cmd.target_client_id;
    for (size_t x = 0; x < cmd.entry_count; x++) {
      c->game_data.pending_card_trade->card_to_count.emplace_back(
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
    S_AdvanceCardTradeState_GC_Ep3_EE_FlagD1 resp = {0};
    send_command_t(target_c, 0xEE, 0xD1, resp);
    if (target_c->game_data.pending_card_trade) {
      send_command_t(c, 0xEE, 0xD1, resp);
    }

  } else if (flag == 0xD2) {
    check_size_v(data.size(), 0);

    if (!c->game_data.pending_card_trade) {
      throw runtime_error("player executed a card trade with none pending");
    }

    auto target_c = l->clients.at(c->game_data.pending_card_trade->other_client_id);
    if (!target_c) {
      throw runtime_error("card trade target player is missing");
    }
    if (!target_c->game_data.pending_card_trade) {
      throw runtime_error("player executed a card trade with no other side pending");
    }

    c->game_data.pending_card_trade->confirmed = true;
    if (target_c->game_data.pending_card_trade->confirmed) {
      send_execute_card_trade(c, target_c->game_data.pending_card_trade->card_to_count);
      send_execute_card_trade(target_c, c->game_data.pending_card_trade->card_to_count);
      S_CardTradeComplete_GC_Ep3_EE_FlagD4 resp = {1};
      send_command_t(c, 0xEE, 0xD4, resp);
      send_command_t(target_c, 0xEE, 0xD4, resp);
      c->game_data.pending_card_trade.reset();
      target_c->game_data.pending_card_trade.reset();
    }

  } else if (flag == 0xD4) {
    check_size_v(data.size(), 0);

    // See the D4 handler for why this check exists (and why it doesn't throw)
    if (!c->game_data.pending_card_trade) {
      return;
    }
    uint8_t other_client_id = c->game_data.pending_card_trade->other_client_id;
    c->game_data.pending_card_trade.reset();
    S_CardTradeComplete_GC_Ep3_EE_FlagD4 resp = {0};
    send_command_t(c, 0xEE, 0xD4, resp);

    // Cancel the other side of the trade too, if it's open
    auto target_c = l->clients.at(other_client_id);
    if (!target_c) {
      return;
    }
    if (!target_c->game_data.pending_card_trade) {
      return;
    }
    target_c->game_data.pending_card_trade.reset();
    send_command_t(target_c, 0xEE, 0xD4, resp);

  } else {
    throw runtime_error("invalid card trade operation");
  }
}

static void on_EF_Ep3(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  check_size_v(data.size(), 0);

  if (!c->config.check_flag(Client::Flag::IS_EPISODE_3)) {
    throw runtime_error("non-Ep3 client sent card auction request");
  }
  auto l = c->require_lobby();
  if (!l->is_game() || !l->is_ep3()) {
    throw runtime_error("client sent card auction request outside of Ep3 game");
  }

  send_ep3_card_auction(l);
}

static void on_xxEA_BB(shared_ptr<Client> c, uint16_t command, uint32_t, string&) {
  // TODO: Implement teams. This command has a very large number of subcommands
  // (up to 20EA!).
  if (command == 0x01EA) {
    send_lobby_message_box(c, "$C6Teams are not supported.");
  } else if (command == 0x14EA) {
    // Do nothing (for now)
  } else {
    throw invalid_argument("unimplemented team command");
  }
}

static void on_02_P(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  check_size_v(data.size(), 0);
  send_command(c, 0x04, 0x00); // This requests the user's login information
}

static void change_to_directory_patch(
    shared_ptr<Client> c,
    vector<string>& client_path_directories,
    const vector<string>& file_path_directories) {
  // First, exit all leaf directories that don't match the desired path
  while (!client_path_directories.empty() &&
      ((client_path_directories.size() > file_path_directories.size()) ||
          (client_path_directories.back() != file_path_directories[client_path_directories.size() - 1]))) {
    send_command(c, 0x0A, 0x00);
    client_path_directories.pop_back();
  }

  // At this point, client_path_directories should be a prefix of
  // file_path_directories (or should match exactly)
  if (client_path_directories.size() > file_path_directories.size()) {
    throw logic_error("did not exit all necessary directories");
  }
  for (size_t x = 0; x < client_path_directories.size(); x++) {
    if (client_path_directories[x] != file_path_directories[x]) {
      throw logic_error("intermediate path is not a prefix of final path");
    }
  }

  // Second, enter all necessary leaf directories
  while (client_path_directories.size() < file_path_directories.size()) {
    const string& dir = file_path_directories[client_path_directories.size()];
    send_enter_directory_patch(c, dir);
    client_path_directories.emplace_back(dir);
  }
}

static void on_04_P(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  const auto& cmd = check_size_t<C_Login_Patch_04>(data);
  auto s = c->require_server_state();

  try {
    auto l = s->license_index->verify_bb(cmd.username.decode(), cmd.password.decode());
    c->set_license(l);

  } catch (const LicenseIndex::incorrect_password& e) {
    send_message_box(c, string_printf("Login failed: %s", e.what()));
    c->should_disconnect = true;
    return;

  } catch (const LicenseIndex::missing_license& e) {
    if (!s->allow_unregistered_users) {
      send_message_box(c, string_printf("Login failed: %s", e.what()));
      c->should_disconnect = true;
      return;
    } else {

      shared_ptr<License> l(new License());
      l->serial_number = fnv1a32(cmd.username.decode()) & 0x7FFFFFFF;
      l->bb_username = cmd.username.decode();
      l->bb_password = cmd.password.decode();
      s->license_index->add(l);
      if (!s->is_replay) {
        l->save();
      }
      c->set_license(l);
      string l_str = l->str();
      c->log.info("Created license %s", l_str.c_str());
    }
  }

  // On BB we can use colors and newlines should be \n; on PC we can't use
  // colors, the text is auto-word-wrapped, and newlines should be \r\n.
  const string& message = c->config.check_flag(Client::Flag::IS_BB_PATCH)
      ? s->bb_patch_server_message
      : s->pc_patch_server_message;
  if (!message.empty()) {
    send_message_box(c, message.c_str());
  }

  auto index = c->config.check_flag(Client::Flag::IS_BB_PATCH) ? s->bb_patch_file_index : s->pc_patch_file_index;
  if (index.get()) {
    send_command(c, 0x0B, 0x00); // Start patch session; go to root directory

    vector<string> path_directories;
    for (const auto& file : index->all_files()) {
      change_to_directory_patch(c, path_directories, file->path_directories);

      S_FileChecksumRequest_Patch_0C req = {c->patch_file_checksum_requests.size(), {file->name, 1}};
      send_command_t(c, 0x0C, 0x00, req);
      c->patch_file_checksum_requests.emplace_back(file);
    }
    change_to_directory_patch(c, path_directories, {});

    send_command(c, 0x0D, 0x00); // End of checksum requests

  } else {
    // No patch index present: just do something that will satisfy the client
    // without actually checking or downloading any files
    send_enter_directory_patch(c, ".");
    send_enter_directory_patch(c, "data");
    send_enter_directory_patch(c, "scene");
    send_command(c, 0x0A, 0x00);
    send_command(c, 0x0A, 0x00);
    send_command(c, 0x0A, 0x00);
    send_command(c, 0x12, 0x00);
  }
}

static void on_0F_P(shared_ptr<Client> c, uint16_t, uint32_t, string& data) {
  auto& cmd = check_size_t<C_FileInformation_Patch_0F>(data);
  auto& req = c->patch_file_checksum_requests.at(cmd.request_id);
  req.crc32 = cmd.checksum;
  req.size = cmd.size;
  req.response_received = true;
}

static void on_10_P(shared_ptr<Client> c, uint16_t, uint32_t, string&) {

  S_StartFileDownloads_Patch_11 start_cmd = {0, 0};
  for (const auto& req : c->patch_file_checksum_requests) {
    if (!req.response_received) {
      throw runtime_error("client did not respond to checksum request");
    }
    if (req.needs_update()) {
      c->log.info("File %s needs update (CRC: %08" PRIX32 "/%08" PRIX32 ", size: %" PRIu32 "/%" PRIu32 ")",
          req.file->name.c_str(), req.file->crc32, req.crc32, req.file->size, req.size);
      start_cmd.total_bytes += req.file->size;
      start_cmd.num_files++;
    } else {
      c->log.info("File %s is up to date", req.file->name.c_str());
    }
  }

  if (start_cmd.num_files) {
    send_command_t(c, 0x11, 0x00, start_cmd);
    vector<string> path_directories;
    for (const auto& req : c->patch_file_checksum_requests) {
      if (req.needs_update()) {
        change_to_directory_patch(c, path_directories, req.file->path_directories);
        send_patch_file(c, req.file);
      }
    }
    change_to_directory_patch(c, path_directories, {});
  }

  send_command(c, 0x12, 0x00);
}

static void on_ignored(shared_ptr<Client>, uint16_t, uint32_t, string&) {}

static void on_unimplemented_command(
    shared_ptr<Client> c, uint16_t command, uint32_t flag, string& data) {
  c->log.warning("Unknown command: size=%04zX command=%04hX flag=%08" PRIX32,
      data.size(), command, flag);
  throw invalid_argument("unimplemented command");
}

typedef void (*on_command_t)(shared_ptr<Client> c, uint16_t command, uint32_t flag, string& data);

// Command handler table, indexed by command number and game version. Null
// entries in this table cause on_unimplemented_command to be called, which
// disconnects the client.
static on_command_t handlers[0x100][6] = {
    // clang-format off
  //        PATCH    DC              PC           GC              XB              BB
  /* 00 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 01 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 02 */ {on_02_P, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 03 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 04 */ {on_04_P, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 05 */ {nullptr, on_ignored,     on_ignored,  on_ignored,     on_ignored,     on_ignored},
  /* 06 */ {nullptr, on_06,          on_06,       on_06,          on_06,          on_06},
  /* 07 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 08 */ {nullptr, on_08_E6,       on_08_E6,    on_08_E6,       on_08_E6,       on_08_E6},
  /* 09 */ {nullptr, on_09,          on_09,       on_09,          on_09,          on_09},
  /* 0A */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 0B */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 0C */ {nullptr, on_0C_C1_E7_EC, nullptr,     nullptr,        nullptr,        nullptr},
  /* 0D */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 0E */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 0F */ {on_0F_P, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 10 */ {on_10_P, on_10,          on_10,       on_10,          on_10,          on_10},
  /* 11 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 12 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 13 */ {nullptr, on_ignored,     on_ignored,  on_13_A7_V3_BB, on_13_A7_V3_BB, on_13_A7_V3_BB},
  /* 14 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 15 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 16 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 17 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 18 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 19 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 1A */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 1B */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 1C */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 1D */ {nullptr, on_ignored,     on_ignored,  on_ignored,     on_ignored,     on_ignored},
  /* 1E */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 1F */ {nullptr, on_1F,          on_1F,       nullptr,        nullptr,        nullptr},
  //        PATCH    DC              PC           GC              XB              BB
  /* 20 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 21 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 22 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        on_ignored},
  /* 23 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 24 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 25 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 26 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 27 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 28 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 29 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 2A */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 2B */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 2C */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 2D */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 2E */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 2F */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 30 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 31 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 32 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 33 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 34 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 35 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 36 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 37 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 38 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 39 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 3A */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 3B */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 3C */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 3D */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 3E */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 3F */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  //        PATCH    DC              PC           GC              XB              BB
  /* 40 */ {nullptr, on_40,          on_40,       on_40,          on_40,          on_40},
  /* 41 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 42 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 43 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 44 */ {nullptr, on_ignored,     on_ignored,  on_44_A6_V3_BB, on_44_A6_V3_BB, on_44_A6_V3_BB},
  /* 45 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 46 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 47 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 48 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 49 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 4A */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 4B */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 4C */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 4D */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 4E */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 4F */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 50 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 51 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 52 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 53 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 54 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 55 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 56 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 57 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 58 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 59 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 5A */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 5B */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 5C */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 5D */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 5E */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 5F */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  //        PATCH    DC              PC           GC              XB              BB
  /* 60 */ {nullptr, on_6x_C9_CB,    on_6x_C9_CB, on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB},
  /* 61 */ {nullptr, on_61_98,       on_61_98,    on_61_98,       on_61_98,       on_61_98},
  /* 62 */ {nullptr, on_6x_C9_CB,    on_6x_C9_CB, on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB},
  /* 63 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 64 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 65 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 66 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 67 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 68 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 69 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 6A */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 6B */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 6C */ {nullptr, on_6x_C9_CB,    on_6x_C9_CB, on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB},
  /* 6D */ {nullptr, on_6x_C9_CB,    on_6x_C9_CB, on_6x_C9_CB,    on_6x_C9_CB,    on_6x_C9_CB},
  /* 6E */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 6F */ {nullptr, on_6F,          on_6F,       on_6F,          on_6F,          on_6F},
  /* 70 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 71 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 72 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 73 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 74 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 75 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 76 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 77 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 78 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 79 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 7A */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 7B */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 7C */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 7D */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 7E */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 7F */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  //        PATCH    DC              PC           GC              XB              BB
  /* 80 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 81 */ {nullptr, on_81,          on_81,       on_81,          on_81,          on_81},
  /* 82 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 83 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 84 */ {nullptr, on_84,          on_84,       on_84,          on_84,          on_84},
  /* 85 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 86 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 87 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 88 */ {nullptr, on_88_DCNTE,    nullptr,     on_88_DCNTE,    nullptr,        nullptr},
  /* 89 */ {nullptr, on_89,          on_89,       on_89,          on_89,          on_89},
  /* 8A */ {nullptr, on_8A,          on_8A,       on_8A,          on_8A,          on_8A},
  /* 8B */ {nullptr, on_8B_DCNTE,    nullptr,     on_8B_DCNTE,    nullptr,        nullptr},
  /* 8C */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 8D */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 8E */ {nullptr, on_8E_DCNTE,    nullptr,     nullptr,        nullptr,        nullptr},
  /* 8F */ {nullptr, on_8F_DCNTE,    nullptr,     nullptr,        nullptr,        nullptr},
  /* 90 */ {nullptr, on_90_DC,       nullptr,     on_90_DC,       nullptr,        nullptr},
  /* 91 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 92 */ {nullptr, on_92_DC,       nullptr,     nullptr,        nullptr,        nullptr},
  /* 93 */ {nullptr, on_93_DC,       nullptr,     on_93_DC,       nullptr,        on_93_BB},
  /* 94 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 95 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 96 */ {nullptr, on_96,          on_96,       on_96,          on_96,          nullptr},
  /* 97 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 98 */ {nullptr, on_61_98,       on_61_98,    on_61_98,       on_61_98,       on_61_98},
  /* 99 */ {nullptr, on_ignored,     on_ignored,  on_99_GC,       on_ignored,     on_ignored},
  /* 9A */ {nullptr, on_9A,          on_9A,       on_9A,          nullptr,        nullptr},
  /* 9B */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* 9C */ {nullptr, on_9C,          on_9C,       on_9C,          on_9C,          nullptr},
  /* 9D */ {nullptr, on_9D_9E,       on_9D_9E,    on_9D_9E,       on_9D_9E,       nullptr},
  /* 9E */ {nullptr, nullptr,        on_9D_9E,    on_9D_9E,       on_9D_9E,       nullptr},
  /* 9F */ {nullptr, nullptr,        nullptr,     on_9F_V3,       on_9F_V3,       nullptr},
  //        PATCH    DC              PC           GC              XB              BB
  /* A0 */ {nullptr, on_A0,          on_A0,       on_A0,          on_A0,          on_A0},
  /* A1 */ {nullptr, on_A1,          on_A1,       on_A1,          on_A1,          on_A1},
  /* A2 */ {nullptr, on_A2,          on_A2,       on_A2,          on_A2,          on_A2},
  /* A3 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* A4 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* A5 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* A6 */ {nullptr, nullptr,        nullptr,     on_44_A6_V3_BB, on_44_A6_V3_BB, nullptr},
  /* A7 */ {nullptr, nullptr,        nullptr,     on_13_A7_V3_BB, on_13_A7_V3_BB, nullptr},
  /* A8 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* A9 */ {nullptr, on_ignored,     on_ignored,  on_ignored,     on_ignored,     on_ignored},
  /* AA */ {nullptr, nullptr,        on_AA,       on_AA,          on_AA,          on_AA},
  /* AB */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* AC */ {nullptr, nullptr,        nullptr,     on_AC_V3_BB,    on_AC_V3_BB,    on_AC_V3_BB},
  /* AD */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* AE */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* AF */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* B0 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* B1 */ {nullptr, on_B1,          on_B1,       on_B1,          on_B1,          nullptr},
  /* B2 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* B3 */ {nullptr, on_B3,          on_B3,       on_B3,          on_B3,          on_B3},
  /* B4 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* B5 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* B6 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* B7 */ {nullptr, nullptr,        nullptr,     on_ignored,     nullptr,        nullptr},
  /* B8 */ {nullptr, nullptr,        nullptr,     on_ignored,     nullptr,        nullptr},
  /* B9 */ {nullptr, nullptr,        nullptr,     on_ignored,     nullptr,        nullptr},
  /* BA */ {nullptr, nullptr,        nullptr,     on_BA_Ep3,      nullptr,        nullptr},
  /* BB */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* BC */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* BD */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* BE */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* BF */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  //        PATCH    DC              PC           GC              XB              BB
  /* C0 */ {nullptr, on_C0,          on_C0,       on_C0,          on_C0,          nullptr},
  /* C1 */ {nullptr, on_0C_C1_E7_EC, on_C1_PC,    on_0C_C1_E7_EC, on_0C_C1_E7_EC, on_C1_BB},
  /* C2 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* C3 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* C4 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* C5 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* C6 */ {nullptr, nullptr,        on_C6,       on_C6,          on_C6,          on_C6},
  /* C7 */ {nullptr, nullptr,        on_C7,       on_C7,          on_C7,          on_C7},
  /* C8 */ {nullptr, nullptr,        on_C8,       on_C8,          on_C8,          on_C8},
  /* C9 */ {nullptr, nullptr,        nullptr,     on_6x_C9_CB,    nullptr,        nullptr},
  /* CA */ {nullptr, nullptr,        nullptr,     on_CA_Ep3,      nullptr,        nullptr},
  /* CB */ {nullptr, nullptr,        nullptr,     on_6x_C9_CB,    nullptr,        nullptr},
  /* CC */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* CD */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* CE */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* CF */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* D0 */ {nullptr, nullptr,        nullptr,     on_D0_V3_BB,    on_D0_V3_BB,    on_D0_V3_BB},
  /* D1 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* D2 */ {nullptr, nullptr,        nullptr,     on_D2_V3_BB,    on_D2_V3_BB,    on_D2_V3_BB},
  /* D3 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* D4 */ {nullptr, nullptr,        nullptr,     on_D4_V3_BB,    on_D4_V3_BB,    on_D4_V3_BB},
  /* D5 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* D6 */ {nullptr, nullptr,        nullptr,     on_D6_V3,       on_D6_V3,       nullptr},
  /* D7 */ {nullptr, nullptr,        nullptr,     on_D7_GC,       on_D7_GC,       nullptr},
  /* D8 */ {nullptr, nullptr,        on_D8,       on_D8,          on_D8,          on_D8},
  /* D9 */ {nullptr, nullptr,        on_D9,       on_D9,          on_D9,          on_D9},
  /* DA */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* DB */ {nullptr, nullptr,        nullptr,     on_DB_V3,       on_DB_V3,       nullptr},
  /* DC */ {nullptr, nullptr,        nullptr,     on_DC_Ep3,      nullptr,        on_DC_BB},
  /* DD */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* DE */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* DF */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  //        PATCH    DC              PC           GC              XB              BB
  /* E0 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        on_00E0_BB},
  /* E1 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* E2 */ {nullptr, nullptr,        nullptr,     on_E2_Ep3,      nullptr,        on_00E2_BB},
  /* E3 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        on_00E3_BB},
  /* E4 */ {nullptr, nullptr,        nullptr,     on_E4_Ep3,      nullptr,        nullptr},
  /* E5 */ {nullptr, nullptr,        nullptr,     on_E5_Ep3,      nullptr,        on_00E5_BB},
  /* E6 */ {nullptr, nullptr,        nullptr,     on_08_E6,       nullptr,        nullptr},
  /* E7 */ {nullptr, nullptr,        nullptr,     on_0C_C1_E7_EC, nullptr,        on_00E7_BB},
  /* E8 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        on_00E8_BB},
  /* E9 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* EA */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        on_xxEA_BB},
  /* EB */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        on_xxEB_BB},
  /* EC */ {nullptr, nullptr,        nullptr,     on_0C_C1_E7_EC, nullptr,        on_00EC_BB},
  /* ED */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        on_xxED_BB},
  /* EE */ {nullptr, nullptr,        nullptr,     on_EE_Ep3,      nullptr,        nullptr},
  /* EF */ {nullptr, nullptr,        nullptr,     on_EF_Ep3,      nullptr,        nullptr},
  /* F0 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* F1 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* F2 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* F3 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* F4 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* F5 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* F6 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* F7 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* F8 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* F9 */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* FA */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* FB */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* FC */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* FD */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* FE */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  /* FF */ {nullptr, nullptr,        nullptr,     nullptr,        nullptr,        nullptr},
  //        PATCH    DC              PC           GC              XB              BB
    // clang-format on
};

static void check_unlicensed_command(GameVersion version, uint8_t command) {
  switch (version) {
    case GameVersion::DC:
      // newserv doesn't actually know that DC clients are DC until it receives
      // an appropriate login command (93, 9A, or 9D), but those commands also
      // log the client in, so this case should never be executed.
      throw logic_error("cannot check unlicensed command for DC client");
    case GameVersion::PC:
      if (command != 0x9A && command != 0x9C && command != 0x9D) {
        throw runtime_error("only commands 9A, 9C, and 9D may be sent before login");
      }
      break;
    case GameVersion::GC:
    case GameVersion::XB:
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
    case GameVersion::BB:
      if (command != 0x93) {
        throw runtime_error("only command 93 may be sent before login");
      }
      break;
    case GameVersion::PATCH:
      if (command != 0x02 && command != 0x04) {
        throw runtime_error("only commands 02 and 04 may be sent before login");
      }
      break;
    default:
      throw logic_error("invalid game version");
  }
}

void on_command(
    shared_ptr<Client> c,
    uint16_t command,
    uint32_t flag,
    string& data) {
  c->reschedule_ping_and_timeout_events();

  // Most of the command handlers assume the client is registered, logged in,
  // and not banned (and therefore that c->license is not null), so the client
  // is allowed to access normal functionality. This check prevents clients from
  // sneakily sending commands to access functionality without logging in.
  if (!c->license.get()) {
    check_unlicensed_command(c->version(), command);
  }

  auto fn = handlers[command & 0xFF][static_cast<size_t>(c->version())];
  if (fn) {
    fn(c, command, flag, data);
  } else {
    on_unimplemented_command(c, command, flag, data);
  }
}

void on_command_with_header(shared_ptr<Client> c, const string& data) {
  switch (c->version()) {
    case GameVersion::DC:
    case GameVersion::GC:
    case GameVersion::XB: {
      auto& header = check_size_t<PSOCommandHeaderDCV3>(data, 0xFFFF);
      string sub_data = data.substr(sizeof(header));
      on_command(c, header.command, header.flag, sub_data);
      break;
    }
    case GameVersion::PC:
    case GameVersion::PATCH: {
      auto& header = check_size_t<PSOCommandHeaderPC>(data, 0xFFFF);
      string sub_data = data.substr(sizeof(header));
      on_command(c, header.command, header.flag, sub_data);
      break;
    }
    case GameVersion::BB: {
      auto& header = check_size_t<PSOCommandHeaderBB>(data, 0xFFFF);
      string sub_data = data.substr(sizeof(header));
      on_command(c, header.command, header.flag, sub_data);
      break;
    }
    default:
      throw logic_error("unimplemented game version in on_command_with_header");
  }
}
