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

#include "Loggers.hh"
#include "ChatCommands.hh"
#include "FileContentsCache.hh"
#include "ProxyServer.hh"
#include "PSOProtocol.hh"
#include "ReceiveSubcommands.hh"
#include "SendCommands.hh"
#include "StaticGameData.hh"
#include "Text.hh"

using namespace std;



vector<MenuItem> quest_categories_menu({
  MenuItem(static_cast<uint32_t>(QuestCategory::RETRIEVAL), u"Retrieval", u"$E$C6Quests that involve\nretrieving an object", 0),
  MenuItem(static_cast<uint32_t>(QuestCategory::EXTERMINATION), u"Extermination", u"$E$C6Quests that involve\ndestroying all\nmonsters", 0),
  MenuItem(static_cast<uint32_t>(QuestCategory::EVENT), u"Events", u"$E$C6Quests that are part\nof an event", 0),
  MenuItem(static_cast<uint32_t>(QuestCategory::SHOP), u"Shops", u"$E$C6Quests that contain\nshops", 0),
  MenuItem(static_cast<uint32_t>(QuestCategory::VR), u"Virtual Reality", u"$E$C6Quests that are\ndone in a simulator", MenuItem::Flag::INVISIBLE_ON_DC | MenuItem::Flag::INVISIBLE_ON_PC),
  MenuItem(static_cast<uint32_t>(QuestCategory::TOWER), u"Control Tower", u"$E$C6Quests that take\nplace at the Control\nTower", MenuItem::Flag::INVISIBLE_ON_DC | MenuItem::Flag::INVISIBLE_ON_PC),
});

vector<MenuItem> quest_battle_menu({
  MenuItem(static_cast<uint32_t>(QuestCategory::BATTLE), u"Battle", u"$E$C6Battle mode rule\nsets", 0),
});

vector<MenuItem> quest_challenge_menu({
  MenuItem(static_cast<uint32_t>(QuestCategory::CHALLENGE), u"Challenge", u"$E$C6Challenge mode\nquests", 0),
});

vector<MenuItem> quest_solo_menu({
  MenuItem(static_cast<uint32_t>(QuestCategory::SOLO), u"Solo Quests", u"$E$C6Quests that require\na single player", 0),
});

vector<MenuItem> quest_government_menu({
  MenuItem(static_cast<uint32_t>(QuestCategory::GOVERNMENT_EPISODE_1), u"Hero in Red",u"$E$CG-Red Ring Rico-\n$C6Quests that follow\nthe Episode 1\nstoryline", 0),
  MenuItem(static_cast<uint32_t>(QuestCategory::GOVERNMENT_EPISODE_2), u"The Military's Hero",u"$E$CG-Heathcliff Flowen-\n$C6Quests that follow\nthe Episode 2\nstoryline", 0),
  MenuItem(static_cast<uint32_t>(QuestCategory::GOVERNMENT_EPISODE_4), u"The Meteor Impact Incident", u"$E$C6Quests that follow\nthe Episode 4\nstoryline", 0),
});

vector<MenuItem> quest_download_menu({
  MenuItem(static_cast<uint32_t>(QuestCategory::RETRIEVAL), u"Retrieval", u"$E$C6Quests that involve\nretrieving an object", 0),
  MenuItem(static_cast<uint32_t>(QuestCategory::EXTERMINATION), u"Extermination", u"$E$C6Quests that involve\ndestroying all\nmonsters", 0),
  MenuItem(static_cast<uint32_t>(QuestCategory::EVENT), u"Events", u"$E$C6Quests that are part\nof an event", 0),
  MenuItem(static_cast<uint32_t>(QuestCategory::SHOP), u"Shops", u"$E$C6Quests that contain\nshops", 0),
  MenuItem(static_cast<uint32_t>(QuestCategory::VR), u"Virtual Reality", u"$E$C6Quests that are\ndone in a simulator", MenuItem::Flag::INVISIBLE_ON_DC | MenuItem::Flag::INVISIBLE_ON_PC),
  MenuItem(static_cast<uint32_t>(QuestCategory::TOWER), u"Control Tower", u"$E$C6Quests that take\nplace at the Control\nTower", MenuItem::Flag::INVISIBLE_ON_DC | MenuItem::Flag::INVISIBLE_ON_PC),
  MenuItem(static_cast<uint32_t>(QuestCategory::DOWNLOAD), u"Download", u"$E$C6Quests to download\nto your Memory Card", 0),
});

static const unordered_map<uint32_t, const char16_t*> proxy_options_menu_descriptions({
  {ProxyOptionsMenuItemID::GO_BACK, u"Return to the\nproxy menu"},
  {ProxyOptionsMenuItemID::INFINITE_HP, u"If enabled, the proxy\nwill restore your HP\nwhen you are hit by\nan enemy or trap,\nbut cannot revive\nyou from one-hit\nkills"},
  {ProxyOptionsMenuItemID::INFINITE_TP, u"If enabled, the proxy\nwill restore your TP\nwhen you cast any\ntechnique"},
  {ProxyOptionsMenuItemID::SWITCH_ASSIST, u"If enabled, the proxy\nwill attempt to\nunlock 2-player\ndoors when you step\non both switches\nsequentially"},
  {ProxyOptionsMenuItemID::BLOCK_EVENTS, u"If enabled, seasonal\nevents in the lobby\nand in games are\ndisabled."},
  {ProxyOptionsMenuItemID::BLOCK_PATCHES, u"If enabled, patches\nsent by the remote\nserver are blocked."},
  {ProxyOptionsMenuItemID::SAVE_FILES, u"If enabled, the proxy\nwill save local\ncopies of files from\nthe remote server\n(quests, etc.)"},
  {ProxyOptionsMenuItemID::SUPPRESS_LOGIN, u"If enabled, the proxy\nwill use an alternate\nlogin sequence"},
  {ProxyOptionsMenuItemID::SKIP_CARD, u"If enabled, the proxy\nwill use an alternate\nvalue for your initial\nGuild Card"},
});

static vector<MenuItem> proxy_options_menu_for_client(
    shared_ptr<const Client> c) {
  vector<MenuItem> ret;
  // Note: The descriptions are instead in the map above, because this menu is
  // dynamically created every time it's sent to the client. This is just one
  // way in which the menu abstraction is currently insufficient (there is a
  // TODO about this in README.md).
  ret.emplace_back(ProxyOptionsMenuItemID::GO_BACK, u"Go back", u"", 0);
  if (!(c->flags & Client::Flag::IS_EPISODE_3)) {
    ret.emplace_back(ProxyOptionsMenuItemID::INFINITE_HP,
        c->infinite_hp ? u"Infinite HP ON" : u"Infinite HP OFF", u"", 0);
    ret.emplace_back(ProxyOptionsMenuItemID::INFINITE_TP,
        c->infinite_tp ? u"Infinite TP ON" : u"Infinite TP OFF", u"", 0);
    ret.emplace_back(ProxyOptionsMenuItemID::SWITCH_ASSIST,
        c->switch_assist ? u"Switch assist ON" : u"Switch assist OFF", u"", 0);
  }
  ret.emplace_back(ProxyOptionsMenuItemID::BLOCK_EVENTS,
      c->proxy_block_events ? u"Block events ON" : u"Block events OFF", u"", 0);
  ret.emplace_back(ProxyOptionsMenuItemID::BLOCK_PATCHES,
      c->proxy_block_function_calls ? u"Block patches ON" : u"Block patches OFF", u"", 0);
  ret.emplace_back(ProxyOptionsMenuItemID::SAVE_FILES,
      c->proxy_save_files ? u"Save files ON" : u"Save files OFF", u"", 0);
  ret.emplace_back(ProxyOptionsMenuItemID::SUPPRESS_LOGIN,
      c->proxy_suppress_remote_login ? u"Skip login ON" : u"Skip login OFF", u"", 0);
  ret.emplace_back(ProxyOptionsMenuItemID::SKIP_CARD,
      c->proxy_zero_remote_guild_card ? u"Skip card ON" : u"Skip card OFF", u"", 0);
  return ret;
}



static void send_client_to_lobby_server(shared_ptr<ServerState> s, shared_ptr<Client> c) {
  static const vector<string> version_to_port_name({
      "bb-lobby", "console-lobby", "pc-lobby", "console-lobby", "console-lobby", "bb-lobby"});
  const auto& port_name = version_to_port_name.at(static_cast<size_t>(c->version()));
  send_reconnect(c, s->connect_address_for_client(c),
      s->name_to_port_config.at(port_name)->port);
}

static void send_client_to_proxy_server(shared_ptr<ServerState> s, shared_ptr<Client> c) {
  static const vector<string> version_to_port_name({
      "", "dc-proxy", "pc-proxy", "gc-proxy", "xb-proxy", "bb-proxy"});
  const auto& port_name = version_to_port_name.at(static_cast<size_t>(c->version()));
  uint16_t local_port = s->name_to_port_config.at(port_name)->port;

  s->proxy_server->delete_session(c->license->serial_number);
  auto session = s->proxy_server->create_licensed_session(
      c->license, local_port, c->version(), c->export_config_bb());
  session->infinite_hp = c->infinite_hp;
  session->infinite_tp = c->infinite_tp;
  session->switch_assist = c->switch_assist;
  session->save_files = c->proxy_save_files;
  session->suppress_remote_login = c->proxy_suppress_remote_login;
  if (c->proxy_block_events) {
    session->override_lobby_event = 0;
  }
  if (c->proxy_block_function_calls) {
    session->function_call_return_value = 0xFFFFFFFF;
  }
  if (c->proxy_zero_remote_guild_card) {
    session->remote_guild_card_number = 0;
  } else {
    try {
      string key = string_printf("proxy_remote_guild_card_number:%" PRIX32, c->license->serial_number);
      const auto& entry = client_options_cache.get_or_throw(key);
      session->remote_guild_card_number = stoul(entry->data, nullptr, 10);
    } catch (const out_of_range&) { }
  }

  send_reconnect(c, s->connect_address_for_client(c), local_port);
}

static void send_proxy_destinations_menu(shared_ptr<ServerState> s, shared_ptr<Client> c) {
  send_menu(c, u"Proxy server", MenuID::PROXY_DESTINATIONS,
      s->proxy_destinations_menu_for_version(c->version()));
  try {
    string key = string_printf("proxy_remote_guild_card_number:%" PRIX32, c->license->serial_number);
    const auto& entry = client_options_cache.get_or_throw(key);
    uint32_t proxy_remote_guild_card_number = stoul(entry->data, nullptr, 10);
    string info_str = string_printf("Your remote Guild\nCard number is\noverridden as\n$C6%" PRIu32, proxy_remote_guild_card_number);
    send_ship_info(c, decode_sjis(info_str));
  } catch (const out_of_range&) { }
}

static bool send_enable_send_function_call_if_applicable(
    shared_ptr<ServerState> s, shared_ptr<Client> c) {
  if (c->flags & Client::Flag::USE_OVERFLOW_FOR_SEND_FUNCTION_CALL) {
    if (s->episode_3_send_function_call_enabled) {
      send_quest_buffer_overflow(s, c);
    } else {
      c->flags |= Client::Flag::NO_SEND_FUNCTION_CALL;
    }
    c->flags &= ~Client::Flag::USE_OVERFLOW_FOR_SEND_FUNCTION_CALL;
    return true;
  }
  return false;
}



////////////////////////////////////////////////////////////////////////////////

void on_connect(std::shared_ptr<ServerState> s, std::shared_ptr<Client> c) {
  switch (c->server_behavior) {
    case ServerBehavior::PC_CONSOLE_DETECT: {
      uint16_t pc_port = s->name_to_port_config.at("pc-login")->port;
      uint16_t console_port = s->name_to_port_config.at("console-login")->port;
      send_pc_console_split_reconnect(c, s->connect_address_for_client(c), pc_port, console_port);
      c->should_disconnect = true;
      break;
    }

    case ServerBehavior::LOGIN_SERVER:
      send_server_init(s, c, SendServerInitFlag::IS_INITIAL_CONNECTION);
      break;

    case ServerBehavior::PATCH_SERVER_BB:
      c->flags |= Client::Flag::IS_BB_PATCH;
      send_server_init(s, c, 0);
      break;

    case ServerBehavior::PATCH_SERVER_PC:
    case ServerBehavior::DATA_SERVER_BB:
    case ServerBehavior::LOBBY_SERVER:
      send_server_init(s, c, 0);
      break;

    default:
      c->log.error("Unimplemented behavior: %" PRId64,
          static_cast<int64_t>(c->server_behavior));
  }
}

void on_login_complete(shared_ptr<ServerState> s, shared_ptr<Client> c) {
  // On the BB data server, this function is called only on the last connection
  // (when we should send the ship select menu).
  if ((c->server_behavior == ServerBehavior::LOGIN_SERVER) ||
      (c->server_behavior == ServerBehavior::DATA_SERVER_BB)) {
    // On the login server, send the events/songs, ep3 updates, and the main
    // menu or welcome message
    if (c->flags & Client::Flag::IS_EPISODE_3) {
      if (s->ep3_menu_song >= 0) {
        send_ep3_change_music(c, s->ep3_menu_song);
      } else if (s->pre_lobby_event) {
        send_change_event(c, s->pre_lobby_event);
      }
      send_ep3_card_list_update(s, c);
      send_ep3_rank_update(c);
    } else if (s->pre_lobby_event) {
      send_change_event(c, s->pre_lobby_event);
    }

    if (s->welcome_message.empty() ||
        (c->flags & Client::Flag::NO_D6) ||
        !(c->flags & Client::Flag::AT_WELCOME_MESSAGE)) {
      c->flags &= ~Client::Flag::AT_WELCOME_MESSAGE;
      if (send_enable_send_function_call_if_applicable(s, c)) {
        send_update_client_config(c);
      }
      send_menu(c, s->name.c_str(), MenuID::MAIN, s->main_menu);
    } else {
      send_message_box(c, s->welcome_message.c_str());
    }

  } else if (c->server_behavior == ServerBehavior::LOBBY_SERVER) {

    if (c->version() == GameVersion::BB) {
      // This implicitly loads the client's account and player data
      send_complete_player_bb(c);
      c->game_data.should_update_play_time = true;
    }

    send_lobby_list(c, s);
    send_get_player_info(c);
  }
}



void on_disconnect(shared_ptr<ServerState> s, shared_ptr<Client> c) {
  // If the client was in a lobby, remove them and notify the other clients
  if (c->lobby_id) {
    s->remove_client_from_lobby(c);
  }

  // TODO: Track play time somewhere for BB players
  // c->game_data.player()->disp.play_time += ((now() - c->play_time_begin) / 1000000);

  // Note: The client's GameData destructor should save their player data
  // shortly after this point
}



////////////////////////////////////////////////////////////////////////////////

static void set_console_client_flags(
    shared_ptr<Client> c, uint32_t sub_version) {
  if (c->channel.crypt_in->type() == PSOEncryption::Type::V2) {
    if (sub_version < 0x28) {
      c->channel.version = GameVersion::DC;
      c->log.info("Game version changed to DC");
    } else if (c->version() == GameVersion::GC) {
      c->flags |= Client::Flag::IS_TRIAL_EDITION;
      c->log.info("Trial edition flag set");
    }
  }
  c->flags |= flags_for_version(c->version(), sub_version);
}

static void on_verify_license_v3(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // DB
  const auto& cmd = check_size_t<C_VerifyLicense_V3_DB>(data);

  if (c->channel.crypt_in->type() == PSOEncryption::Type::V2) {
    throw runtime_error("GC trial edition client sent V3 verify license command");
  }
  set_console_client_flags(c, cmd.sub_version);

  uint32_t serial_number = stoul(cmd.serial_number, nullptr, 16);
  try {
    auto l = s->license_manager->verify_gc(serial_number, cmd.access_key,
        cmd.password);
    c->set_license(l);
    send_command(c, 0x9A, 0x02);

  } catch (const incorrect_access_key& e) {
    send_command(c, 0x9A, 0x03);
    c->should_disconnect = true;
    return;

  } catch (const incorrect_password& e) {
    send_command(c, 0x9A, 0x07);
    c->should_disconnect = true;
    return;

  } catch (const missing_license& e) {
    if (!s->allow_unregistered_users) {
      send_command(c, 0x9A, 0x04);
      c->should_disconnect = true;
      return;
    } else {
      auto l = LicenseManager::create_license_gc(serial_number, cmd.access_key,
          cmd.password, true);
      s->license_manager->add(l);
      c->set_license(l);
      send_command(c, 0x9A, 0x02);
    }
  }
}

static void on_login_8_dcnte(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 88
  const auto& cmd = check_size_t<C_Login_DCNTE_88>(data);
  c->channel.version = GameVersion::DC;
  c->flags |= flags_for_version(c->version(), -1);
  c->flags |= Client::Flag::IS_DC_V1 | Client::Flag::IS_TRIAL_EDITION;

  uint32_t serial_number = stoul(cmd.serial_number, nullptr, 16);
  try {
    shared_ptr<const License> l = s->license_manager->verify_pc(
        serial_number, cmd.access_key);
    c->set_license(l);
    send_command(c, 0x88, 0x00);

  } catch (const incorrect_access_key& e) {
    send_message_box(c, u"Incorrect access key");
    c->should_disconnect = true;

  } catch (const missing_license& e) {
    if (!s->allow_unregistered_users) {
      send_message_box(c, u"Incorrect serial number");
      c->should_disconnect = true;
    } else {
      auto l = LicenseManager::create_license_pc(
          serial_number, cmd.access_key, true);
      s->license_manager->add(l);
      c->set_license(l);
      send_command(c, 0x88, 0x00);
    }
  }
}

static void on_login_b_dcnte(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 8B
  const auto& cmd = check_size_t<C_Login_DCNTE_8B>(data,
      sizeof(C_Login_DCNTE_8B), sizeof(C_LoginExtended_DCNTE_8B));
  c->channel.version = GameVersion::DC;
  c->flags |= flags_for_version(c->version(), -1);
  c->flags |= Client::Flag::IS_DC_V1 | Client::Flag::IS_TRIAL_EDITION;

  uint32_t serial_number = stoul(cmd.serial_number, nullptr, 16);
  try {
    shared_ptr<const License> l = s->license_manager->verify_pc(
        serial_number, cmd.access_key);
    c->set_license(l);
    // send_command(c, 0x8B, 0x01);

  } catch (const incorrect_access_key& e) {
    send_message_box(c, u"Incorrect access key");
    c->should_disconnect = true;

  } catch (const missing_license& e) {
    if (!s->allow_unregistered_users) {
      send_message_box(c, u"Incorrect serial number");
      c->should_disconnect = true;
    } else {
      auto l = LicenseManager::create_license_pc(
          serial_number, cmd.access_key, true);
      s->license_manager->add(l);
      c->set_license(l);
      // send_command(c, 0x8B, 0x01);
    }
  }

  if (cmd.is_extended) {
    const auto& ext_cmd = check_size_t<C_LoginExtended_DCNTE_8B>(data);
    if (ext_cmd.extension.menu_id == MenuID::LOBBY) {
      c->preferred_lobby_id = ext_cmd.extension.lobby_id;
    }
  }

  if (!c->should_disconnect) {
    send_update_client_config(c);
    on_login_complete(s, c);
  }
}

static void on_login_0_dc_pc_v3(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 90
  const auto& cmd = check_size_t<C_LoginV1_DC_PC_V3_90>(data);
  c->channel.version = GameVersion::DC;
  c->flags |= flags_for_version(c->version(), -1);
  c->flags |= Client::Flag::IS_DC_V1;

  uint32_t serial_number = stoul(cmd.serial_number, nullptr, 16);
  try {
    shared_ptr<const License> l = s->license_manager->verify_pc(
        serial_number, cmd.access_key);
    c->set_license(l);
    send_command(c, 0x90, 0x02);

  } catch (const incorrect_access_key& e) {
    send_command(c, 0x90, 0x03);
    c->should_disconnect = true;

  } catch (const missing_license& e) {
    if (!s->allow_unregistered_users) {
      send_command(c, 0x90, 0x03);
      c->should_disconnect = true;
    } else {
      auto l = LicenseManager::create_license_pc(
          serial_number, cmd.access_key, true);
      s->license_manager->add(l);
      c->set_license(l);
      send_command(c, 0x90, 0x01);
    }
  }
}

static void on_login_2_dc(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 92
  check_size_t<C_RegisterV1_DC_92>(data);
  send_command(c, 0x92, 0x01);
}

static void on_login_3_dc_pc_v3(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 93
  const auto& cmd = check_size_t<C_LoginV1_DC_93>(data,
      sizeof(C_LoginV1_DC_93), sizeof(C_LoginExtendedV1_DC_93));
  set_console_client_flags(c, cmd.sub_version);

  uint32_t serial_number = stoul(cmd.serial_number, nullptr, 16);
  try {
    shared_ptr<const License> l = s->license_manager->verify_pc(
        serial_number, cmd.access_key);
    c->set_license(l);

  } catch (const incorrect_access_key& e) {
    send_message_box(c, u"Incorrect access key");
    c->should_disconnect = true;
    return;

  } catch (const missing_license& e) {
    if (!s->allow_unregistered_users) {
      send_message_box(c, u"Incorrect serial number");
      c->should_disconnect = true;
      return;
    } else {
      auto l = LicenseManager::create_license_pc(
          serial_number, cmd.access_key, true);
      s->license_manager->add(l);
      c->set_license(l);
    }
  }

  if (cmd.is_extended) {
    const auto& ext_cmd = check_size_t<C_LoginExtendedV1_DC_93>(data);
    if (ext_cmd.extension.menu_id == MenuID::LOBBY) {
      c->preferred_lobby_id = ext_cmd.extension.lobby_id;
    }
  }

  send_update_client_config(c);

  on_login_complete(s, c);
}

static void on_login_a_dc_pc_v3(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 9A
  const auto& cmd = check_size_t<C_Login_DC_PC_V3_9A>(data);
  set_console_client_flags(c, cmd.sub_version);

  uint32_t serial_number = stoul(cmd.serial_number, nullptr, 16);
  try {
    shared_ptr<const License> l;
    switch (c->version()) {
      case GameVersion::DC:
      case GameVersion::PC:
        l = s->license_manager->verify_pc(serial_number, cmd.access_key);
        break;
      case GameVersion::GC:
        l = s->license_manager->verify_gc(serial_number, cmd.access_key);
        break;
      case GameVersion::XB:
        throw runtime_error("xbox licenses are not implemented");
        break;
      default:
        throw logic_error("unsupported versioned command");
    }
    c->set_license(l);
    send_command(c, 0x9A, 0x02);

  } catch (const incorrect_access_key& e) {
    send_command(c, 0x9A, 0x03);
    c->should_disconnect = true;
    return;

  } catch (const incorrect_password& e) {
    send_command(c, 0x9A, 0x07);
    c->should_disconnect = true;
    return;

  } catch (const missing_license& e) {
    // On V3, the client should have sent a different command containing the
    // password already, which should have created and added a temporary
    // license. So, if no license exists at this point, disconnect the client
    // even if unregistered clients are allowed.
    shared_ptr<License> l;
    if ((c->version() == GameVersion::GC) || (c->version() == GameVersion::XB)) {
      send_command(c, 0x9A, 0x04);
      c->should_disconnect = true;
      return;
    } else if ((c->version() == GameVersion::DC) || (c->version() == GameVersion::PC)) {
      l = LicenseManager::create_license_pc(serial_number, cmd.access_key, true);
      s->license_manager->add(l);
      c->set_license(l);
      send_command(c, 0x9A, 0x02);
    } else {
      throw runtime_error("unsupported game version");
    }
  }
}

static void on_login_c_dc_pc_v3(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 9C
  const auto& cmd = check_size_t<C_Register_DC_PC_V3_9C>(data);

  c->flags |= flags_for_version(c->version(), cmd.sub_version);

  uint32_t serial_number = stoul(cmd.serial_number, nullptr, 16);
  try {
    shared_ptr<const License> l;
    switch (c->version()) {
      case GameVersion::DC:
      case GameVersion::PC:
        l = s->license_manager->verify_pc(serial_number, cmd.access_key);
        break;
      case GameVersion::GC:
        l = s->license_manager->verify_gc(serial_number, cmd.access_key,
            cmd.password);
        break;
      case GameVersion::XB:
        throw runtime_error("xbox licenses are not implemented");
        break;
      default:
        throw logic_error("unsupported versioned command");
    }
    c->set_license(l);
    send_command(c, 0x9C, 0x01);

  } catch (const incorrect_password& e) {
    send_command(c, 0x9C, 0x00);
    c->should_disconnect = true;
    return;

  } catch (const missing_license& e) {
    if (!s->allow_unregistered_users) {
      send_command(c, 0x9C, 0x00);
      c->should_disconnect = true;
      return;
    } else {
      shared_ptr<License> l;
      switch (c->version()) {
        case GameVersion::DC:
        case GameVersion::PC:
          l = LicenseManager::create_license_pc(serial_number, cmd.access_key,
              true);
          break;
        case GameVersion::GC:
          l = LicenseManager::create_license_gc(serial_number, cmd.access_key,
              cmd.password, true);
          break;
        case GameVersion::XB:
          throw runtime_error("xbox licenses are not implemented");
          break;
        default:
          throw logic_error("unsupported versioned command");
      }
      s->license_manager->add(l);
      c->set_license(l);
      send_command(c, 0x9C, 0x01);
    }
  }
}

static void on_login_d_e_dc_pc_v3(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t command, uint32_t, const string& data) { // 9D 9E
  const C_Login_DC_PC_GC_9D* base_cmd;
  if (command == 0x9D) {
    base_cmd = &check_size_t<C_Login_DC_PC_GC_9D>(data,
        sizeof(C_Login_DC_PC_GC_9D), sizeof(C_LoginExtended_PC_9D));
    if (base_cmd->is_extended) {
      if (c->version() == GameVersion::PC) {
        const auto& cmd = check_size_t<C_LoginExtended_PC_9D>(data);
        if (cmd.extension.menu_id == MenuID::LOBBY) {
          c->preferred_lobby_id = cmd.extension.lobby_id;
        }
      } else {
        const auto& cmd = check_size_t<C_LoginExtended_DC_GC_9D>(data);
        if (cmd.extension.menu_id == MenuID::LOBBY) {
          c->preferred_lobby_id = cmd.extension.lobby_id;
        }
      }
    }

  } else if (command == 0x9E) {
    // GC and XB send different amounts of data in this command. This is how
    // newserv determines if a V3 client is GC or XB.
    const auto& cmd = check_size_t<C_Login_GC_9E>(data,
        sizeof(C_Login_GC_9E), sizeof(C_LoginExtended_XB_9E));
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
      if (cmd.extension.menu_id == MenuID::LOBBY) {
        c->preferred_lobby_id = cmd.extension.lobby_id;
      }
    }

    try {
      c->import_config(cmd.client_config.cfg);
    } catch (const invalid_argument&) {
      // If we can't import the config, assume that the client was not connected
      // to newserv before, so we should show the welcome message.
      c->flags |= Client::Flag::AT_WELCOME_MESSAGE;
      c->bb_game_state = ClientStateBB::INITIAL_LOGIN;
      c->game_data.bb_player_index = 0;
    }

  } else {
    throw logic_error("9D/9E handler called for incorrect command");
  }

  set_console_client_flags(c, base_cmd->sub_version);
  // See system/ppc/Episode3USAQuestBufferOverflow.s for where this value gets
  // set. We use this to determine if the client has already run the code or
  // not; sending it again when the client has already run it will likely cause
  // the client to crash.
  if (base_cmd->unused1 == 0x5F5CA297) {
    c->flags &= ~(Client::Flag::USE_OVERFLOW_FOR_SEND_FUNCTION_CALL | Client::Flag::NO_SEND_FUNCTION_CALL);
  } else if (!s->episode_3_send_function_call_enabled &&
             (c->flags & Client::Flag::USE_OVERFLOW_FOR_SEND_FUNCTION_CALL)) {
    c->flags &= ~Client::Flag::USE_OVERFLOW_FOR_SEND_FUNCTION_CALL;
    c->flags |= Client::Flag::NO_SEND_FUNCTION_CALL;
  }

  uint32_t serial_number = stoul(base_cmd->serial_number, nullptr, 16);
  try {
    shared_ptr<const License> l;
    switch (c->version()) {
      case GameVersion::DC:
      case GameVersion::PC:
        l = s->license_manager->verify_pc(serial_number, base_cmd->access_key);
        break;
      case GameVersion::GC:
        l = s->license_manager->verify_gc(serial_number, base_cmd->access_key);
        break;
      case GameVersion::XB:
        throw runtime_error("xbox licenses are not implemented");
        break;
      default:
        throw logic_error("unsupported versioned command");
    }
    c->set_license(l);

  } catch (const incorrect_access_key& e) {
    send_command(c, 0x04, 0x03);
    c->should_disconnect = true;
    return;

  } catch (const incorrect_password& e) {
    send_command(c, 0x04, 0x06);
    c->should_disconnect = true;
    return;

  } catch (const missing_license& e) {
    // On V3, the client should have sent a different command containing the
    // password already, which should have created and added a temporary
    // license. So, if no license exists at this point, disconnect the client
    // even if unregistered clients are allowed.
    shared_ptr<License> l;
    if ((c->version() == GameVersion::GC) || (c->version() == GameVersion::XB)) {
      send_command(c, 0x04, 0x04);
      c->should_disconnect = true;
      return;
    } else if ((c->version() == GameVersion::DC) || (c->version() == GameVersion::PC)) {
      l = LicenseManager::create_license_pc(serial_number, base_cmd->access_key, true);
      s->license_manager->add(l);
      c->set_license(l);
    } else {
      throw runtime_error("unsupported game version");
    }
  }

  send_update_client_config(c);
  on_login_complete(s, c);
}

static void on_login_bb(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 93
  const auto& cmd = check_size_t<C_Login_BB_93>(data,
      sizeof(C_Login_BB_93) - 8, sizeof(C_Login_BB_93));

  bool is_old_format;
  if (data.size() == sizeof(C_Login_BB_93) - 8) {
    is_old_format = true;
  } else if (data.size() == sizeof(C_Login_BB_93)) {
    is_old_format = false;
  } else {
    throw runtime_error("invalid size for 93 command");
  }

  c->flags |= flags_for_version(c->version(), -1);

  try {
    auto l = s->license_manager->verify_bb(cmd.username, cmd.password);
    c->set_license(l);

  } catch (const incorrect_password& e) {
    u16string message = u"Login failed: " + decode_sjis(e.what());
    send_message_box(c, message.c_str());
    c->should_disconnect = true;
    return;

  } catch (const missing_license& e) {
    if (!s->allow_unregistered_users) {
      u16string message = u"Login failed: " + decode_sjis(e.what());
      send_message_box(c, message.c_str());
      c->should_disconnect = true;
      return;
    } else {
      shared_ptr<License> l = LicenseManager::create_license_bb(
          fnv1a32(cmd.username) & 0x7FFFFFFF, cmd.username, cmd.password, true);
      s->license_manager->add(l);
      c->set_license(l);
    }
  }

  try {
    if (is_old_format) {
      c->import_config(cmd.var.old_clients_cfg.cfg);
    } else {
      c->import_config(cmd.var.new_clients.cfg.cfg);
    }
    if (c->bb_game_state < ClientStateBB::IN_GAME) {
      c->bb_game_state++;
    }
  } catch (const invalid_argument&) {
    c->bb_game_state = ClientStateBB::INITIAL_LOGIN;
    c->game_data.bb_player_index = 0;
  }

  if (cmd.menu_id == MenuID::LOBBY) {
    c->preferred_lobby_id = cmd.preferred_lobby_id;
  }

  send_client_init_bb(c, 0);

  switch (c->bb_game_state) {
    case ClientStateBB::INITIAL_LOGIN:
      // On first login, send the client to the data server port
      send_reconnect(c, s->connect_address_for_client(c),
          s->name_to_port_config.at("bb-data1")->port);
      break;

    case ClientStateBB::DOWNLOAD_DATA:
    case ClientStateBB::CHOOSE_PLAYER:
    case ClientStateBB::SAVE_PLAYER:
      // Just wait in these cases; the client will request something from us and
      // the command handlers will take care of it
      break;

    case ClientStateBB::SHIP_SELECT:
      on_login_complete(s, c);
      break;

    case ClientStateBB::IN_GAME:
      break;

    default:
      throw runtime_error("invalid bb game state");
  }
}

static void on_return_client_config(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 9F
  if (c->version() == GameVersion::BB) {
    const auto& cfg = check_size_t<ClientConfigBB>(data);
    c->import_config(cfg);
  } else {
    const auto& cfg = check_size_t<ClientConfig>(data);
    c->import_config(cfg);
  }
}

static void on_client_checksum(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 96
  check_size_t<C_CharSaveInfo_V3_BB_96>(data);
  send_server_time(c);
}

static void on_server_time_request(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // B1
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
    send_client_to_lobby_server(s, c);
  } else if (c->should_send_to_proxy_server) {
    send_client_to_proxy_server(s, c);
  }
}



static void on_ep3_meseta_transaction(shared_ptr<ServerState>,
    shared_ptr<Client> c, uint16_t command, uint32_t, const string& data) {
  const auto& in_cmd = check_size_t<C_Meseta_GC_Ep3_BA>(data);

  S_Meseta_GC_Ep3_BA out_cmd = {1000000, 1000000, in_cmd.request_token};
  send_command(c, command, 0x03, &out_cmd, sizeof(out_cmd));
}

static void on_ep3_battle_table_state(shared_ptr<ServerState> s,
    shared_ptr<Client> c, uint16_t, uint32_t flag, const string& data) { // E4
  const auto& cmd = check_size_t<C_CardBattleTableState_GC_Ep3_E4>(data);
  auto l = s->find_lobby(c->lobby_id);
  if (l->is_game() || !(l->flags & Lobby::Flag::EPISODE_3_ONLY)) {
    throw runtime_error("battle table command sent in non-CARD lobby");
  }

  if (flag) {
    c->card_battle_table_number = cmd.table_number;
    c->card_battle_table_seat_number = cmd.seat_number;
  } else {
    c->card_battle_table_number = -1;
    c->card_battle_table_seat_number = 0;
  }
  send_ep3_card_battle_table_state(l, c->card_battle_table_number);
  // TODO: If a client disconnects while at the battle table, we need to send
  // a table update to all the other clients at the table (if any). Use a
  // disconnect hook for this.
}

static void on_ep3_battle_table_confirm(shared_ptr<ServerState> s,
    shared_ptr<Client> c, uint16_t, uint32_t flag, const string& data) { // E4
  check_size_t<S_CardBattleTableConfirmation_GC_Ep3_E5>(data);
  auto l = s->find_lobby(c->lobby_id);
  if (l->is_game() || !(l->flags & Lobby::Flag::EPISODE_3_ONLY)) {
    throw runtime_error("battle table command sent in non-CARD lobby");
  }

  if (flag) {
    // TODO
    send_lobby_message_box(c, u"CARD battles are\nnot yet supported.");
  }
}

static void on_ep3_counter_state(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t flag, const string& data) { // DC
  check_size_v(data.size(), 0);
  auto l = s->find_lobby(c->lobby_id);
  if (flag != 0) {
    send_command(c, 0xDC, 0x00);
    l->flags |= Lobby::Flag::BATTLE_IN_PROGRESS;
  } else {
    l->flags &= ~Lobby::Flag::BATTLE_IN_PROGRESS;
  }
}

static void on_ep3_server_data_request(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // CA
  auto l = s->find_lobby(c->lobby_id);
  if (!l || !(l->flags & Lobby::Flag::EPISODE_3_ONLY) || !l->is_game()) {
    throw runtime_error("Episode 3 server data request sent in lobby or non-Episode 3 game");
  }

  const auto& header = check_size_t<G_CardBattleCommandHeader>(
      data, sizeof(G_CardBattleCommandHeader), 0xFFFF);

  if (header.subcommand != 0xB3) {
    throw runtime_error("unknown Episode 3 server data request");
  }

  if (!l->ep3_server_base || l->ep3_server_base->server->battle_finished) {
    if (!l->ep3_server_base) {
      l->log.info("Creating Episode 3 server state");
    } else {
      l->log.info("Recreating Episode 3 server state");
    }
    l->ep3_server_base = make_shared<Episode3::ServerBase>(
        l, s->ep3_data_index, l->random_seed);
    l->ep3_server_base->init();

    if (s->ep3_behavior_flags & Episode3::BehaviorFlag::ENABLE_STATUS_MESSAGES) {
      for (size_t z = 0; z < l->max_clients; z++) {
        if (l->clients[z]) {
          send_text_message_printf(l->clients[z], "Your client ID: $C6%zu", z);
        }
      }
      send_text_message_printf(l, "State seed: $C6%08" PRIX32, l->random_seed);
    }

    if (s->ep3_behavior_flags & Episode3::BehaviorFlag::ENABLE_RECORDING) {
      if (l->battle_record) {
        l->prev_battle_record = l->battle_record;
        l->prev_battle_record->set_battle_end_timestamp();
      }
      l->battle_record.reset(new Episode3::BattleRecord(s->ep3_behavior_flags));
      for (auto existing_c : l->clients) {
        if (existing_c) {
          PlayerLobbyDataDCGC lobby_data;
          lobby_data.name = encode_sjis(existing_c->game_data.player()->disp.name);
          lobby_data.player_tag = 0x00010000;
          lobby_data.guild_card = existing_c->license->serial_number;
          l->battle_record->add_player(lobby_data,
              existing_c->game_data.player()->inventory,
              existing_c->game_data.player()->disp.to_dcpcv3());
        }
      }
      if (l->prev_battle_record) {
        send_text_message(l, u"$C6Recording complete");
      }
      send_text_message(l, u"$C6Recording enabled");
    }
  }
  l->ep3_server_base->server->on_server_data_input(data);
}

static void on_ep3_tournament_control(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string&) { // E2
  // The client will set their interaction mode expecting a menu to be sent, but
  // since we don't implement tournaments, they will get stuck here unless we
  // send something. An 01 (lobby message box) seems to work fine.
  send_lobby_message_box(c, u"$C6Tournaments are\nnot supported.");
}



static void on_message_box_closed(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // D6
  check_size_v(data.size(), 0);
  if (c->flags & Client::Flag::IN_INFORMATION_MENU) {
    send_menu(c, u"Information", MenuID::INFORMATION,
        *s->information_menu_for_version(c->version()));
  } else if (c->flags & Client::Flag::AT_WELCOME_MESSAGE) {
    send_enable_send_function_call_if_applicable(s, c);
    c->flags &= ~Client::Flag::AT_WELCOME_MESSAGE;
    send_update_client_config(c);
    send_menu(c, s->name.c_str(), MenuID::MAIN, s->main_menu);
  }
}

static void on_menu_item_info_request(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 09
  const auto& cmd = check_size_t<C_MenuItemInfoRequest_09>(data);

  switch (cmd.menu_id) {
    case MenuID::MAIN:
      for (const auto& item : s->main_menu) {
        if (item.item_id == cmd.item_id) {
          send_ship_info(c, item.description);
        }
      }
      break;

    case MenuID::INFORMATION:
      if (cmd.item_id == InformationMenuItemID::GO_BACK) {
        send_ship_info(c, u"Return to the\nmain menu.");
      } else {
        try {
          // We use item_id + 1 here because "go back" is the first item
          send_ship_info(c, s->information_menu_for_version(c->version())->at(cmd.item_id + 1).description.c_str());
        } catch (const out_of_range&) {
          send_ship_info(c, u"$C4Missing information\nmenu item");
        }
      }
      break;

    case MenuID::PROXY_DESTINATIONS:
      if (cmd.item_id == ProxyDestinationsMenuItemID::GO_BACK) {
        send_ship_info(c, u"Return to the\nmain menu");
      } else if (cmd.item_id == ProxyDestinationsMenuItemID::OPTIONS) {
        send_ship_info(c, u"Set proxy session\noptions");
      } else {
        try {
          const auto& menu = s->proxy_destinations_menu_for_version(c->version());
          // We use item_id + 2 here because "go back" and "options" are the
          // first items
          send_ship_info(c, menu.at(cmd.item_id + 2).description.c_str());
        } catch (const out_of_range&) {
          send_ship_info(c, u"$C4Missing proxy\ndestination");
        }
      }
      break;

    case MenuID::PROXY_OPTIONS:
      try {
        const auto* description = proxy_options_menu_descriptions.at(cmd.item_id);
        send_ship_info(c, description);
      } catch (const out_of_range&) {
        send_ship_info(c, u"$C4Missing proxy\noption");
      }
      break;

    case MenuID::QUEST_FILTER:
      // Don't send anything here. The quest filter menu already has short
      // descriptions included with the entries, which the client shows in the
      // usual location on the screen.
      break;

    case MenuID::QUEST: {
      if (!s->quest_index) {
        send_quest_info(c, u"$C6Quests are not available.", !c->lobby_id);
        break;
      }
      auto q = s->quest_index->get(c->version(), cmd.item_id);
      if (!q) {
        send_quest_info(c, u"$C4Quest does not\nexist.", !c->lobby_id);
        break;
      }
      send_quest_info(c, q->long_description.c_str(), !c->lobby_id);
      break;
    }

    case MenuID::GAME: {
      shared_ptr<Lobby> game;
      try {
        game = s->find_lobby(cmd.item_id);
      } catch (const out_of_range& e) {
        send_ship_info(c, u"$C4Game no longer\nexists.");
        break;
      }

      if (!game->is_game()) {
        send_ship_info(c, u"$C4Incorrect game ID");

      } else {
        string info;
        for (size_t x = 0; x < game->max_clients; x++) {
          const auto& game_c = game->clients[x];
          if (game_c.get()) {
            auto player = game_c->game_data.player();
            auto name = encode_sjis(player->disp.name);
            if (game->flags & Lobby::Flag::EPISODE_3_ONLY) {
              info += string_printf("%zu: $C6%s$C7 L%" PRIu32 "\n",
                  x + 1, name.c_str(), player->disp.level + 1);
            } else {
              info += string_printf("%zu: $C6%s$C7 %s L%" PRIu32 "\n",
                  x + 1, name.c_str(),
                  abbreviation_for_char_class(player->disp.char_class),
                  player->disp.level + 1);
            }
          }
        }

        int episode = game->episode;
        if (episode == 3) {
          episode = 4;
        } else if (episode == 0xFF) {
          episode = 3;
        }
        string secid_str = name_for_section_id(game->section_id);
        const char* mode_abbrev = "Nml";
        if (game->flags & Lobby::Flag::BATTLE_MODE) {
          mode_abbrev = "Btl";
        } else if (game->flags & Lobby::Flag::CHALLENGE_MODE) {
          mode_abbrev = "Chl";
        } else if (game->flags & Lobby::Flag::SOLO_MODE) {
          mode_abbrev = "Solo";
        }
        info += string_printf("Ep%d %c %s %s\n",
            episode,
            abbreviation_for_difficulty(game->difficulty),
            mode_abbrev,
            secid_str.c_str());

        bool cheats_enabled = game->flags & Lobby::Flag::CHEATS_ENABLED;
        bool locked = !game->password.empty();
        if (cheats_enabled && locked) {
          info += "$C4Locked$C7, $C6cheats enabled$C7\n";
        } else if (cheats_enabled) {
          info += "$C6Cheats enabled$C7\n";
        } else if (locked) {
          info += "$C4Locked$C7\n";
        }

        if (game->loading_quest) {
          if (game->flags & Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS) {
            info += "$C6Quest: " + encode_sjis(game->loading_quest->name);
          } else {
            info += "$C4Quest: " + encode_sjis(game->loading_quest->name);
          }
        } else if (game->flags & Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS) {
          info += "$C6Quest in progress";
        } else if (game->flags & Lobby::Flag::QUEST_IN_PROGRESS) {
          info += "$C4Quest in progress";
        } else if (game->flags & Lobby::Flag::BATTLE_IN_PROGRESS) {
          info += "$C4Battle in progress";
        }

        if (game->flags & Lobby::Flag::SPECTATORS_FORBIDDEN) {
          info += "$C4View Battle forbidden";
        }

        send_ship_info(c, decode_sjis(info));
      }
      break;
    }

    case MenuID::PATCHES:
      // TODO: Find a way to provide descriptions for patches.
      break;

    case MenuID::PROGRAMS: {
      if (cmd.item_id == ProgramsMenuItemID::GO_BACK) {
        send_ship_info(c, u"Return to the\nmain menu.");
      } else {
        try {
          auto dol = s->dol_file_index->item_id_to_file.at(cmd.item_id);
          string size_str = format_size(dol->data.size());
          string info = string_printf("$C6%s$C7\n%s", dol->name.c_str(), size_str.c_str());
          send_ship_info(c, decode_sjis(info));
        } catch (const out_of_range&) {
          send_ship_info(c, u"Incorrect program ID.");
        }
      }
      break;
    }

    default:
      send_ship_info(c, u"Incorrect menu ID.");
      break;
  }
}

static void on_menu_selection(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 10
  bool uses_unicode = ((c->version() == GameVersion::PC) || (c->version() == GameVersion::BB));

  uint32_t menu_id;
  uint32_t item_id;
  u16string password;

  if (data.size() > sizeof(C_MenuSelection_10_Flag00)) {
    if (uses_unicode) {
      const auto& cmd = check_size_t<C_MenuSelection_PC_BB_10_Flag02>(data);
      password = cmd.password;
      menu_id = cmd.basic_cmd.menu_id;
      item_id = cmd.basic_cmd.item_id;
    } else {
      const auto& cmd = check_size_t<C_MenuSelection_DC_V3_10_Flag02>(data);
      password = decode_sjis(cmd.password);
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
          if (!(c->flags & Client::Flag::SAVE_ENABLED)) {
            c->flags |= Client::Flag::SAVE_ENABLED;
            // DC NTE crashes if it receives a 97 command, so we instead do the
            // redirect immediately
            if ((c->version() == GameVersion::DC) && (c->flags & Client::Flag::IS_TRIAL_EDITION)) {
              send_client_to_lobby_server(s, c);
            } else {
              send_command(c, 0x97, 0x01);
              send_update_client_config(c);
            }
          } else {
            send_client_to_lobby_server(s, c);
          }
          break;
        }

        case MainMenuItemID::INFORMATION:
          send_menu(c, u"Information", MenuID::INFORMATION,
              *s->information_menu_for_version(c->version()));
          c->flags |= Client::Flag::IN_INFORMATION_MENU;
          break;

        case MainMenuItemID::PROXY_DESTINATIONS:
          send_proxy_destinations_menu(s, c);
          break;

        case MainMenuItemID::DOWNLOAD_QUESTS:
          if (c->flags & Client::Flag::IS_EPISODE_3) {
            shared_ptr<Lobby> l = c->lobby_id ? s->find_lobby(c->lobby_id) : nullptr;
            auto quests = s->quest_index->filter(
                c->version(), c->flags & Client::Flag::IS_DC_V1, QuestCategory::EPISODE_3);
            if (quests.empty()) {
              send_lobby_message_box(c, u"$C6There are no quests\navailable.");
            } else {
              // Episode 3 has only download quests, not online quests, so this
              // is always the download quest menu. (Episode 3 does actually
              // have online quests, but they're served via a server data
              // request instead of the file download paradigm that all other
              // versions use.)
              send_quest_menu(c, MenuID::QUEST, quests, true);
            }
          } else {
            send_quest_menu(c, MenuID::QUEST_FILTER, quest_download_menu, true);
          }
          break;

        case MainMenuItemID::PATCHES:
          send_menu(c, u"Patches", MenuID::PATCHES, s->function_code_index->patch_menu());
          break;

        case MainMenuItemID::PROGRAMS:
          send_menu(c, u"Programs", MenuID::PROGRAMS, s->dol_file_index->menu());
          break;

        case MainMenuItemID::DISCONNECT:
          c->should_disconnect = true;
          break;

        case MainMenuItemID::CLEAR_LICENSE:
          send_command(c, 0x9A, 0x04);
          c->should_disconnect = true;
          break;

        default:
          send_message_box(c, u"Incorrect menu item ID.");
          break;
      }
      break;
    }

    case MenuID::INFORMATION: {
      if (item_id == InformationMenuItemID::GO_BACK) {
        c->flags &= ~Client::Flag::IN_INFORMATION_MENU;
        send_menu(c, s->name.c_str(), MenuID::MAIN, s->main_menu);

      } else {
        try {
          send_message_box(c, s->information_contents->at(item_id).c_str());
        } catch (const out_of_range&) {
          send_message_box(c, u"$C6No such information exists.");
        }
      }
      break;
    }

    case MenuID::PROXY_OPTIONS: {
      switch (item_id) {
        case ProxyOptionsMenuItemID::GO_BACK:
          send_proxy_destinations_menu(s, c);
          break;
        case ProxyOptionsMenuItemID::INFINITE_HP:
          c->infinite_hp = !c->infinite_hp;
          goto resend_proxy_options_menu;
        case ProxyOptionsMenuItemID::INFINITE_TP:
          c->infinite_tp = !c->infinite_tp;
          goto resend_proxy_options_menu;
        case ProxyOptionsMenuItemID::SWITCH_ASSIST:
          c->switch_assist = !c->switch_assist;
          goto resend_proxy_options_menu;
        case ProxyOptionsMenuItemID::BLOCK_EVENTS:
          c->proxy_block_events = !c->proxy_block_events;
          goto resend_proxy_options_menu;
        case ProxyOptionsMenuItemID::BLOCK_PATCHES:
          c->proxy_block_function_calls = !c->proxy_block_function_calls;
          goto resend_proxy_options_menu;
        case ProxyOptionsMenuItemID::SAVE_FILES:
          c->proxy_save_files = !c->proxy_save_files;
          goto resend_proxy_options_menu;
        case ProxyOptionsMenuItemID::SUPPRESS_LOGIN:
          c->proxy_suppress_remote_login = !c->proxy_suppress_remote_login;
          goto resend_proxy_options_menu;
        case ProxyOptionsMenuItemID::SKIP_CARD:
          c->proxy_zero_remote_guild_card = !c->proxy_zero_remote_guild_card;
        resend_proxy_options_menu:
          send_menu(c, s->name.c_str(), MenuID::PROXY_OPTIONS,
              proxy_options_menu_for_client(c));
          break;
        default:
          send_message_box(c, u"Incorrect menu item ID.");
          break;
      }
      break;
    }

    case MenuID::PROXY_DESTINATIONS: {
      if (item_id == ProxyDestinationsMenuItemID::GO_BACK) {
        send_menu(c, s->name.c_str(), MenuID::MAIN, s->main_menu);

      } else if (item_id == ProxyDestinationsMenuItemID::OPTIONS) {
        send_menu(c, s->name.c_str(), MenuID::PROXY_OPTIONS,
            proxy_options_menu_for_client(c));

      } else {
        const pair<string, uint16_t>* dest = nullptr;
        try {
          dest = &s->proxy_destinations_for_version(c->version()).at(item_id);
        } catch (const out_of_range&) { }

        if (!dest) {
          send_message_box(c, u"$C6No such destination exists.");
          c->should_disconnect = true;
        } else {
          c->proxy_destination_address = resolve_ipv4(dest->first);
          c->proxy_destination_port = dest->second;
          if (!(c->flags & Client::Flag::SAVE_ENABLED)) {
            c->should_send_to_proxy_server = true;
            c->flags |= Client::Flag::SAVE_ENABLED;
            send_command(c, 0x97, 0x01);
            send_update_client_config(c);
          } else {
            send_update_client_config(c);
            send_client_to_proxy_server(s, c);
          }
        }
      }
      break;
    }

    case MenuID::GAME: {
      auto game = s->find_lobby(item_id);
      if (!game) {
        send_lobby_message_box(c, u"$C6You cannot join this\ngame because it no\nlonger exists.");
        break;
      }
      if (!game->is_game()) {
        send_lobby_message_box(c, u"$C6You cannot join this\ngame because it is\nnot a game.");
        break;
      }
      if (game->count_clients() >= game->max_clients) {
        send_lobby_message_box(c, u"$C6You cannot join this\ngame because it is\nfull.");
        break;
      }
      if ((game->version != c->version()) ||
          (!(game->flags & Lobby::Flag::EPISODE_3_ONLY) != !(c->flags & Client::Flag::IS_EPISODE_3)) ||
          ((game->flags & Lobby::Flag::NON_V1_ONLY) && (c->flags & Client::Flag::IS_DC_V1))) {
        send_lobby_message_box(c, u"$C6You cannot join this\ngame because it is\nfor a different\nversion of PSO.");
        break;
      }
      if (game->flags & Lobby::Flag::QUEST_IN_PROGRESS) {
        send_lobby_message_box(c, u"$C6You cannot join this\ngame because a\nquest is already\nin progress.");
        break;
      }
      if (game->flags & Lobby::Flag::BATTLE_IN_PROGRESS) {
        send_lobby_message_box(c, u"$C6You cannot join this\ngame because a\nbattle is already\nin progress.");
        break;
      }
      if (game->any_client_loading()) {
        send_lobby_message_box(c, u"$C6You cannot join this\ngame because\nanother player is\ncurrently loading.\nTry again soon.");
        break;
      }
      if (game->flags & Lobby::Flag::SOLO_MODE) {
        send_lobby_message_box(c, u"$C6You cannot join this\n game because it is\na Solo Mode game.");
        break;
      }

      if (!(c->license->privileges & Privilege::FREE_JOIN_GAMES)) {
        if (!game->password.empty() && (password != game->password)) {
          send_message_box(c, u"$C6Incorrect password.");
          break;
        }
        if (c->game_data.player()->disp.level < game->min_level) {
          send_message_box(c, u"$C6Your level is too\nlow to join this\ngame.");
          break;
        }
        if (c->game_data.player()->disp.level > game->max_level) {
          send_message_box(c, u"$C6Your level is too\nhigh to join this\ngame.");
          break;
        }
      }

      if (!s->change_client_lobby(c, game)) {
        throw logic_error("client cannot join game after all preconditions satisfied");
      }
      c->flags |= Client::Flag::LOADING;
      break;
    }

    case MenuID::QUEST_FILTER: {
      if (!s->quest_index) {
        send_lobby_message_box(c, u"$C6Quests are not available.");
        break;
      }
      shared_ptr<Lobby> l = c->lobby_id ? s->find_lobby(c->lobby_id) : nullptr;
      auto quests = s->quest_index->filter(c->version(),
          c->flags & Client::Flag::IS_DC_V1,
          static_cast<QuestCategory>(item_id & 0xFF));
      if (quests.empty()) {
        send_lobby_message_box(c, u"$C6There are no quests\navailable in that\ncategory.");
        break;
      }

      // Hack: Assume the menu to be sent is the download quest menu if the
      // client is not in any lobby
      send_quest_menu(c, MenuID::QUEST, quests, !c->lobby_id);
      break;
    }

    case MenuID::QUEST: {
      if (!s->quest_index) {
        send_lobby_message_box(c, u"$C6Quests are not available.");
        break;
      }
      auto q = s->quest_index->get(c->version(), item_id);
      if (!q) {
        send_lobby_message_box(c, u"$C6Quest does not exist.");
        break;
      }

      // If the client is not in a lobby, send the quest as a download quest.
      // Otherwise, they must be in a game to load a quest.
      shared_ptr<Lobby> l;
      if (c->lobby_id) {
        l = s->find_lobby(c->lobby_id);
        if (!l->is_game()) {
          send_lobby_message_box(c, u"$C6Quests cannot be\nloaded in lobbies.");
          break;
        }
      }

      bool is_ep3 = (q->episode == 0xFF);
      string bin_basename = q->bin_filename();
      shared_ptr<const string> bin_contents = q->bin_contents();
      string dat_basename;
      shared_ptr<const string> dat_contents;
      if (!is_ep3) {
        dat_basename = q->dat_filename();
        dat_contents = q->dat_contents();
      }

      if (l) {
        if (is_ep3 || !dat_contents) {
          throw runtime_error("episode 3 quests cannot be loaded during games");
        }
        if (q->joinable) {
          l->flags |= Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS;
        } else {
          l->flags |= Lobby::Flag::QUEST_IN_PROGRESS;
        }
        l->loading_quest = q;
        for (size_t x = 0; x < l->max_clients; x++) {
          if (!l->clients[x]) {
            continue;
          }

          // TODO: It looks like blasting all the chunks to the client at once
          // can cause GC clients to crash in rare cases. Find a way to slow
          // this down (perhaps by only sending each new chunk when they
          // acknowledge the previous chunk with a 13 command).
          send_quest_file(l->clients[x], bin_basename + ".bin", bin_basename,
              *bin_contents, QuestFileType::ONLINE);
          send_quest_file(l->clients[x], dat_basename + ".dat", dat_basename,
              *dat_contents, QuestFileType::ONLINE);

          // There is no such thing as command AC on PSO V2 - quests just start
          // immediately when they're done downloading. (This is also the case
          // on V3 Trial Edition.) There are also no chunk acknowledgements
          // (C->S 13 commands) like there are on GC. So, for PC/Trial clients,
          // we can just not set the loading flag, since we never need to
          // check/clear it later.
          if ((l->clients[x]->version() != GameVersion::DC) &&
              (l->clients[x]->version() != GameVersion::PC) &&
              !(l->clients[x]->flags & Client::Flag::IS_TRIAL_EDITION)) {
            l->clients[x]->flags |= Client::Flag::LOADING_QUEST;
          }
        }

      } else {
        string quest_name = encode_sjis(q->name);
        // Episode 3 uses the download quest commands (A6/A7) but does not
        // expect the server to have already encrypted the quest files, unlike
        // other versions.
        if (!is_ep3) {
          q = q->create_download_quest();
        }
        send_quest_file(c, quest_name, bin_basename, *q->bin_contents(),
            is_ep3 ? QuestFileType::EPISODE_3 : QuestFileType::DOWNLOAD);
        if (dat_contents) {
          send_quest_file(c, quest_name, dat_basename, *q->dat_contents(),
              is_ep3 ? QuestFileType::EPISODE_3 : QuestFileType::DOWNLOAD);
        }
      }
      break;
    }

    case MenuID::PATCHES:
      if (item_id == PatchesMenuItemID::GO_BACK) {
        send_menu(c, s->name.c_str(), MenuID::MAIN, s->main_menu);

      } else {
        if (c->flags & Client::Flag::NO_SEND_FUNCTION_CALL) {
          throw runtime_error("client does not support send_function_call");
        }

        send_function_call(
            c, s->function_code_index->menu_item_id_to_patch_function.at(item_id));
        send_menu(c, u"Patches", MenuID::PATCHES, s->function_code_index->patch_menu());
      }
      break;

    case MenuID::PROGRAMS:
      if (item_id == ProgramsMenuItemID::GO_BACK) {
        send_menu(c, s->name.c_str(), MenuID::MAIN, s->main_menu);

      } else {
        if (c->flags & Client::Flag::NO_SEND_FUNCTION_CALL) {
          throw runtime_error("client does not support send_function_call");
        }

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

    default:
      send_message_box(c, u"Incorrect menu ID.");
      break;
  }
}

static void on_change_lobby(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 84
  const auto& cmd = check_size_t<C_LobbySelection_84>(data);

  if (cmd.menu_id != MenuID::LOBBY) {
    send_message_box(c, u"Incorrect menu ID.");
    return;
  }

  // If the client isn't in any lobby, then they just left a game. Add them to
  // the lobby they requested, but fall back to another lobby if it's full.
  if (c->lobby_id == 0) {
    c->preferred_lobby_id = cmd.item_id;
    s->add_client_to_available_lobby(c);

  // If the client already is in a lobby, then they're using the lobby
  // teleporter; add them to the lobby they requested or send a failure message.
  } else {
    shared_ptr<Lobby> new_lobby;
    try {
      new_lobby = s->find_lobby(cmd.item_id);
    } catch (const out_of_range&) {
      send_lobby_message_box(c, u"$C6Can't change lobby\n\n$C7The lobby does not\nexist.");
      return;
    }

    if ((new_lobby->flags & Lobby::Flag::EPISODE_3_ONLY) && !(c->flags & Client::Flag::IS_EPISODE_3)) {
      send_lobby_message_box(c, u"$C6Can't change lobby\n\n$C7The lobby is for\nEpisode 3 only.");
      return;
    }

    if (!s->change_client_lobby(c, new_lobby)) {
      send_lobby_message_box(c, u"$C6Can\'t change lobby\n\n$C7The lobby is full.");
    }
  }
}

static void on_game_list_request(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 08
  check_size_v(data.size(), 0);
  send_game_menu(c, s);
}

static void on_info_menu_request_dc_pc(shared_ptr<ServerState> s,
    shared_ptr<Client> c, uint16_t, uint32_t, const string& data) { // 1F
  check_size_v(data.size(), 0);
  send_menu(c, u"Information", MenuID::INFORMATION,
      *s->information_menu_for_version(c->version()), true);
}

static void on_change_ship(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string&) { // A0
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
  if (!(c->flags & Client::Flag::NO_D6)) {
    send_message_box(c, u"");
  }

  static const vector<string> version_to_port_name({
      "bb-patch", "console-login", "pc-login", "console-login", "console-login", "bb-init"});
  const auto& port_name = version_to_port_name.at(static_cast<size_t>(c->version()));

  send_reconnect(c, s->connect_address_for_client(c),
      s->name_to_port_config.at(port_name)->port);
}

static void on_change_block(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t command, uint32_t flag, const string& data) { // A1
  // newserv doesn't have blocks; treat block change the same as ship change
  on_change_ship(s, c, command, flag, data);
}



static void send_dol_file_chunk(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint32_t start_addr) {
  size_t offset = start_addr - c->dol_base_addr;
  if (offset >= c->loading_dol_file->data.size()) {
    throw logic_error("DOL file offset beyond end of data");
  }
  size_t bytes_to_send = min<size_t>(0x7800, c->loading_dol_file->data.size() - offset);
  string data_to_send = c->loading_dol_file->data.substr(offset, bytes_to_send);

  auto fn = s->function_code_index->name_to_function.at("WriteMemory");
  unordered_map<string, uint32_t> label_writes(
      {{"dest_addr", start_addr}, {"size", bytes_to_send}});
  send_function_call(c, fn, label_writes, data_to_send);

  size_t progress_percent = ((offset + bytes_to_send) * 100) / c->loading_dol_file->data.size();
  string info = string_printf("Loading $C6%s$C7\n%zu%%%% complete",
      c->loading_dol_file->name.c_str(), progress_percent);
  send_ship_info(c, decode_sjis(info));
}

static void on_function_call_result(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t flag, const string& data) { // B3
  const auto& cmd = check_size_t<C_ExecuteCodeResult_B3>(data);
  if (flag == 0) {
    return;
  }

  auto called_fn = s->function_code_index->index_to_function.at(flag);
  if (c->loading_dol_file.get()) {
    if (called_fn->name == "ReadMemoryWord") {
      c->dol_base_addr = (cmd.return_value - c->loading_dol_file->data.size()) & (~3);
      send_dol_file_chunk(s, c, c->dol_base_addr);
    } else if (called_fn->name == "WriteMemory") {
      if (cmd.return_value >= c->dol_base_addr + c->loading_dol_file->data.size()) {
        auto fn = s->function_code_index->name_to_function.at("RunDOL");
        unordered_map<string, uint32_t> label_writes(
            {{"dol_base_ptr", c->dol_base_addr}});
        send_function_call(c, fn, label_writes);
        // The client will stop running PSO after this, so disconnect them
        c->should_disconnect = true;

      } else {
        send_dol_file_chunk(s, c, cmd.return_value);
      }
    }
  }
}



static void on_quest_list_request(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t flag, const string& data) { // A2
  check_size_v(data.size(), 0);

  if (!s->quest_index) {
    send_lobby_message_box(c, u"$C6Quests are not available.");
    return;
  }

  auto l = s->find_lobby(c->lobby_id);
  if (!l || !l->is_game()) {
    send_lobby_message_box(c, u"$C6Quests are not available\nin lobbies.");
    return;
  }

  // In Episode 3, there are no quest categories, so skip directly to the quest
  // filter menu.
  if (c->flags & Client::Flag::IS_EPISODE_3) {
    send_lobby_message_box(c, u"$C6Episode 3 does not\nprovide online quests\nvia this interface.");

  } else {
    vector<MenuItem>* menu = nullptr;
    if ((c->version() == GameVersion::BB) && flag) {
      menu = &quest_government_menu;
    } else {
      if (l->flags & Lobby::Flag::BATTLE_MODE) {
        menu = &quest_battle_menu;
      } else if (l->flags & Lobby::Flag::CHALLENGE_MODE) {
        menu = &quest_challenge_menu;
      } else if (l->flags & Lobby::Flag::SOLO_MODE) {
        menu = &quest_solo_menu;
      } else {
        menu = &quest_categories_menu;
      }
    }

    send_quest_menu(c, MenuID::QUEST_FILTER, *menu, false);
  }
}

static void on_quest_barrier(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // AC
  check_size_v(data.size(), 0);

  auto l = s->find_lobby(c->lobby_id);
  if (!l || !l->is_game()) {
    return;
  }

  // If this client is NOT loading, they should not send an AC. Sending an AC to
  // a client that isn't waiting to start a quest will crash the client, so we
  // have to be careful not to do so.
  if (!(c->flags & Client::Flag::LOADING_QUEST)) {
    return;
  }
  c->flags &= ~Client::Flag::LOADING_QUEST;

  // Check if any client is still loading
  // TODO: We need to handle clients disconnecting while loading. Probably
  // on_client_disconnect needs to check for this case...
  size_t x;
  for (x = 0; x < l->max_clients; x++) {
    if (!l->clients[x]) {
      continue;
    }
    if (l->clients[x]->flags & Client::Flag::LOADING_QUEST) {
      break;
    }
  }

  // If they're all done, start the quest
  if (x == l->max_clients) {
    send_command(l, 0xAC, 0x00);
  }
}

static void on_update_quest_statistics(shared_ptr<ServerState> s,
    shared_ptr<Client> c, uint16_t, uint32_t, const string& data) { // AA
  const auto& cmd = check_size_t<C_UpdateQuestStatistics_V3_BB_AA>(data);

  if (c->flags & Client::Flag::IS_TRIAL_EDITION) {
    throw runtime_error("trial edition client sent update quest stats command");
  }

  auto l = s->find_lobby(c->lobby_id);
  if (!l || !l->is_game() || !l->loading_quest.get() ||
      (l->loading_quest->internal_id != cmd.quest_internal_id)) {
    return;
  }

  S_ConfirmUpdateQuestStatistics_V3_BB_AB response;
  response.unknown_a1 = 0x0000;
  response.unknown_a2 = 0x0000;
  response.request_token = cmd.request_token;
  response.unknown_a3 = 0xBFFF;
  send_command_t(c, 0xAB, 0x00, response);
}

static void on_gba_file_request(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // D7
  string filename(data);
  strip_trailing_zeroes(filename);

  static FileContentsCache gba_file_cache(300 * 1000 * 1000);
  auto f = gba_file_cache.get_or_load("system/gba/" + filename).file;

  send_quest_file(c, "", filename, f->data, QuestFileType::GBA_DEMO);
}



static void on_player_data(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t command, uint32_t flag, const string& data) { // 61 98

  switch (c->version()) {
    case GameVersion::DC:
    case GameVersion::PC: {
      const auto& pd = check_size_t<PSOPlayerDataDCPC>(data,
          sizeof(PSOPlayerDataDCPC), 0xFFFF);
      c->game_data.import_player(pd);
      break;
    }
    case GameVersion::GC:
    case GameVersion::XB: {
      const PSOPlayerDataV3* pd;
      if (flag == 4) { // Episode 3
        if (!(c->flags & Client::Flag::IS_EPISODE_3)) {
          throw runtime_error("non-Episode 3 client sent Episode 3 player data");
        }
        const auto* pd3 = &check_size_t<PSOPlayerDataGCEp3>(data);
        c->game_data.ep3_config.reset(new Episode3::PlayerConfig(pd3->ep3_config));
        pd = reinterpret_cast<const PSOPlayerDataV3*>(pd3);
      } else {
        pd = &check_size_t<PSOPlayerDataV3>(data, sizeof(PSOPlayerDataV3),
            sizeof(PSOPlayerDataV3) + c->game_data.player()->auto_reply.bytes());
      }
      c->game_data.import_player(*pd);
      break;
    }
    case GameVersion::BB: {
      const auto& pd = check_size_t<PSOPlayerDataBB>(data, sizeof(PSOPlayerDataBB),
          sizeof(PSOPlayerDataBB) + c->game_data.player()->auto_reply.bytes());
      c->game_data.import_player(pd);
      break;
    }
    default:
      throw logic_error("player data command not implemented for version");
  }

  auto player = c->game_data.player(false);
  if (player) {
    string name_str = remove_language_marker(encode_sjis(player->disp.name));
    c->channel.name = string_printf("C-%" PRIX64 " (%s)",
        c->id, name_str.c_str());
  }

  // 98 should only be sent when leaving a game, and we should leave the client
  // in no lobby (they will send an 84 soon afterward to choose a lobby).
  if (command == 0x98) {
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
        u16string buffer = u"$C6PSOBB player data could\nnot be saved:\n" + decode_sjis(e.what());
        send_text_message(c, buffer.c_str());
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
    if (!c->lobby_id && (c->server_behavior == ServerBehavior::LOBBY_SERVER)) {
      s->add_client_to_available_lobby(c);
    }
  }
}



static void on_game_command(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t command, uint32_t flag, const string& data) { // 60 62 6C 6D C9 CB (C9 CB are ep3 only)
  check_size_v(data.size(), 4, 0xFFFF);

  auto l = s->find_lobby(c->lobby_id);
  if (!l) {
    return;
  }

  on_subcommand(s, l, c, command, flag, data);
}



static void on_chat_generic(shared_ptr<ServerState> s, shared_ptr<Client> c,
    const u16string& text) { // 06

  if (text.empty()) {
    return;
  }

  auto l = s->find_lobby(c->lobby_id);
  if (!l) {
    return;
  }

  char private_flags = 0;
  u16string processed_text;
  if ((text[0] != '\t') && (l->flags & Lobby::Flag::EPISODE_3_ONLY)) {
    private_flags = text[0];
    processed_text = remove_language_marker(text.substr(1));
  } else {
    processed_text = remove_language_marker(text);
  }
  if (processed_text.empty()) {
    return;
  }

  if (processed_text[0] == L'$') {
    if (processed_text[1] == L'$') {
      processed_text = processed_text.substr(1);
    } else {
      on_chat_command(s, l, c, processed_text);
      return;
    }
  }

  if (!c->can_chat) {
    return;
  }

  for (size_t x = 0; x < l->max_clients; x++) {
    if (!l->clients[x]) {
      continue;
    }
    send_chat_message(l->clients[x], c->license->serial_number,
        c->game_data.player()->disp.name.data(), processed_text.c_str(),
        private_flags);
  }

  if (l->battle_record && l->battle_record->battle_in_progress()) {
    auto prepared_message = prepare_chat_message(
        c->version(), c->game_data.player()->disp.name.data(),
        processed_text.c_str(), private_flags);
    string prepared_message_sjis = encode_sjis(prepared_message);
    l->battle_record->add_chat_message(c->license->serial_number, move(prepared_message_sjis));
  }
}

static void on_chat_pc_bb(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 06
  const auto& cmd = check_size_t<C_Chat_06>(data, sizeof(C_Chat_06), 0xFFFF);
  u16string text(cmd.text.pcbb, (data.size() - sizeof(C_Chat_06)) / sizeof(char16_t));
  strip_trailing_zeroes(text);
  on_chat_generic(s, c, text);
}

static void on_chat_dc_v3(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) {
  const auto& cmd = check_size_t<C_Chat_06>(data, sizeof(C_Chat_06), 0xFFFF);
  u16string decoded_s = decode_sjis(cmd.text.dcv3, data.size() - sizeof(C_Chat_06));
  on_chat_generic(s, c, decoded_s);
}



static void on_key_config_request_bb(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) {
  check_size_v(data.size(), 0);
  send_team_and_key_config_bb(c);
  c->game_data.account()->newserv_flags &= ~AccountFlag::IN_DRESSING_ROOM;
  c->log.info("Cleared dressing room flag for account");
}

static void on_player_preview_request_bb(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) {
  const auto& cmd = check_size_t<C_PlayerPreviewRequest_BB_E3>(data);

  if (c->bb_game_state == ClientStateBB::CHOOSE_PLAYER) {
    c->game_data.bb_player_index = cmd.player_index;
    c->bb_game_state++;
    send_client_init_bb(c, 0);
    send_approve_player_choice_bb(c);
  } else {
    if (!c->license) {
      c->should_disconnect = true;
      return;
    }

    ClientGameData temp_gd;
    temp_gd.guild_card_number = c->license->serial_number;
    temp_gd.bb_username = c->license->username;
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

static void on_client_checksum_bb(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t command, uint32_t, const string& data) {
  constexpr size_t max_count = sizeof(GuildCardFileBB::entries) / sizeof(GuildCardEntryBB);
  constexpr size_t max_blocked = sizeof(GuildCardFileBB::blocked) / sizeof(GuildCardBB);
  switch (command) {
    case 0x01E8: { // Check guild card file checksum
      const auto& cmd = check_size_t<C_GuildCardChecksum_01E8>(data);
      uint32_t checksum = c->game_data.account()->guild_cards.checksum();
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
      auto& gcf = c->game_data.account()->guild_cards;
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
      auto& gcf = c->game_data.account()->guild_cards;
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
      auto& gcf = c->game_data.account()->guild_cards;
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
      auto& gcf = c->game_data.account()->guild_cards;
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
      auto& gcf = c->game_data.account()->guild_cards;
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
      auto& gcf = c->game_data.account()->guild_cards;
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
      auto& gcf = c->game_data.account()->guild_cards;
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

static void on_guild_card_data_request_bb(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) {
  const auto& cmd = check_size_t<C_GuildCardDataRequest_BB_03DC>(data);
  if (cmd.cont) {
    send_guild_card_chunk_bb(c, cmd.chunk_index);
  }
}

static void on_stream_file_request_bb(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t command, uint32_t flag, const string& data) {
  check_size_v(data.size(), 0);

  if (command == 0x04EB) {
    send_stream_file_index_bb(c);
  } else if (command == 0x03EB) {
    send_stream_file_chunk_bb(c, flag);
  } else {
    throw invalid_argument("unimplemented command");
  }
}

static void on_leave_char_select_bb(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // EC
  const auto& cmd = check_size_t<C_LeaveCharacterSelect_BB_00EC>(data);
  if (cmd.reason == 2) {
    c->game_data.account()->newserv_flags |= AccountFlag::IN_DRESSING_ROOM;
    c->log.info("Set dressing room flag for account");
  } else {
    c->game_data.account()->newserv_flags &= ~AccountFlag::IN_DRESSING_ROOM;
    c->log.info("Cleared dressing room flag for account");
  }
}

static void on_create_character_bb(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) {
  const auto& cmd = check_size_t<SC_PlayerPreview_CreateCharacter_BB_00E5>(data);

  if (!c->license) {
    send_message_box(c, u"$C6You are not logged in.");
    return;
  }

  // Hack: We use the security data to indicate to the server which phase the
  // client is in (download data, character select, lobby, etc.). This presents
  // a problem: the client expects to get an E4 (approve player choice) command
  // immediately after the E5 (create character) command, but the client also
  // will disconnect immediately after it receives that command. If we send an
  // E6 before the E5 to update the security data (setting the correct next
  // state), then the client sends ANOTHER E5, but this time it's blank! So, to
  // be able to both create characters correctly and set security data
  // correctly, we need to process only the first E5, and ignore the second. We
  // do this by only creating a player if the current connection has no loaded
  // player data.
  if (c->game_data.player(false).get()) {
    return;
  }

  c->game_data.bb_player_index = cmd.player_index;

  if (c->game_data.account()->newserv_flags & AccountFlag::IN_DRESSING_ROOM) {
    try {
      c->game_data.player()->disp.apply_dressing_room(cmd.preview);
    } catch (const exception& e) {
      string message = string_printf("$C6Character could not be modified:\n%s", e.what());
      send_message_box(c, decode_sjis(message));
      return;
    }
  } else {
    try {
      c->game_data.create_player(cmd.preview, s->level_table);
    } catch (const exception& e) {
      string message = string_printf("$C6New character could not be created:\n%s", e.what());
      send_message_box(c, decode_sjis(message));
      return;
    }
  }
  c->game_data.account()->newserv_flags &= ~AccountFlag::IN_DRESSING_ROOM;
  c->log.info("Cleared dressing room flag for account");

  send_client_init_bb(c, 0);
  send_approve_player_choice_bb(c);
}

static void on_change_account_data_bb(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t command, uint32_t, const string& data) {
  const auto* cmd = reinterpret_cast<const C_UpdateAccountData_BB_ED*>(data.data());

  switch (command) {
    case 0x01ED:
      check_size_v(data.size(), sizeof(cmd->option));
      c->game_data.account()->option_flags = cmd->option;
      break;
    case 0x02ED:
      check_size_v(data.size(), sizeof(cmd->symbol_chats));
      c->game_data.account()->symbol_chats = cmd->symbol_chats;
      break;
    case 0x03ED:
      check_size_v(data.size(), sizeof(cmd->chat_shortcuts));
      c->game_data.account()->shortcuts = cmd->chat_shortcuts;
      break;
    case 0x04ED:
      check_size_v(data.size(), sizeof(cmd->key_config));
      c->game_data.account()->key_config.key_config = cmd->key_config;
      break;
    case 0x05ED:
      check_size_v(data.size(), sizeof(cmd->pad_config));
      c->game_data.account()->key_config.joystick_config = cmd->pad_config;
      break;
    case 0x06ED:
      check_size_v(data.size(), sizeof(cmd->tech_menu));
      c->game_data.player()->tech_menu_config = cmd->tech_menu;
      break;
    case 0x07ED:
      check_size_v(data.size(), sizeof(cmd->customize));
      c->game_data.player()->disp.config = cmd->customize;
      break;
    default:
      throw invalid_argument("unknown account command");
  }
}

static void on_return_player_data_bb(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 00E7
  const auto& cmd = check_size_t<PlayerBB>(data);

  // We only trust the player's quest data and challenge data.
  c->game_data.player()->challenge_data = cmd.challenge_data;
  c->game_data.player()->quest_data1 = cmd.quest_data1;
  c->game_data.player()->quest_data2 = cmd.quest_data2;
}

static void on_update_key_config_bb(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) {
  // Some clients have only a uint32_t at the end for team rewards
  auto& cmd = check_size_t<KeyAndTeamConfigBB>(data,
      sizeof(KeyAndTeamConfigBB) - 4, sizeof(KeyAndTeamConfigBB));
  c->game_data.account()->key_config = cmd;
  // TODO: We should probably send a response here, but I don't know which one!
}



static void on_change_arrow_color(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t flag, const string& data) { // 89
  check_size_v(data.size(), 0);

  c->lobby_arrow_color = flag;
  auto l = s->find_lobby(c->lobby_id);
  if (l) {
    send_arrow_update(l);
  }
}

static void on_card_search(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 40
  const auto& cmd = check_size_t<C_GuildCardSearch_40>(data);
  try {
    auto result = s->find_client(nullptr, cmd.target_guild_card_number);
    auto result_lobby = s->find_lobby(result->lobby_id);
    send_card_search_result(s, c, result, result_lobby);
  } catch (const out_of_range&) { }
}

static void on_choice_search(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string&) { // C0
  // TODO: Implement choice search.
  send_text_message(c, u"$C6Choice Search is\nnot supported");
}

static void on_simple_mail(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 81
  u16string message;
  uint32_t to_guild_card_number;
  switch (c->version()) {
    case GameVersion::DC:
    case GameVersion::GC:
    case GameVersion::XB: {
      const auto& cmd = check_size_t<SC_SimpleMail_DC_V3_81>(data);
      to_guild_card_number = cmd.to_guild_card_number;
      message = decode_sjis(cmd.text);
      break;
    }
    case GameVersion::PC: {
      const auto& cmd = check_size_t<SC_SimpleMail_PC_81>(data);
      to_guild_card_number = cmd.to_guild_card_number;
      message = cmd.text;
      break;
    }
    case GameVersion::BB: {
      const auto& cmd = check_size_t<SC_SimpleMail_BB_81>(data);
      to_guild_card_number = cmd.to_guild_card_number;
      message = cmd.text;
      break;
    }
    default:
      throw logic_error("invalid game version");
  }

  shared_ptr<Client> target;
  try {
    target = s->find_client(nullptr, to_guild_card_number);
  } catch (const out_of_range&) { }

  if (!target) {
    // TODO: We should store pending messages for accounts somewhere, and send
    // them when the player signs on again. We should also persist the player's
    // autoreply setting when they're offline and use it if appropriate here.
    send_text_message(c, u"$C6Player is offline");

  } else {
    // If the sender is blocked, don't forward the mail
    for (size_t y = 0; y < 30; y++) {
      if (target->game_data.account()->blocked_senders.data()[y] == c->license->serial_number) {
        return;
      }
    }

    // If the target has auto-reply enabled, send the autoreply. Note that we also
    // forward the message in this case.
    if (!target->game_data.player()->auto_reply.empty()) {
      send_simple_mail(c, target->license->serial_number,
          target->game_data.player()->disp.name,
          target->game_data.player()->auto_reply);
    }

    // Forward the message
    send_simple_mail(
        target,
        c->license->serial_number,
        c->game_data.player()->disp.name,
        message);
  }
}



static void on_info_board_request(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // D8
  check_size_v(data.size(), 0);
  send_info_board(c, s->find_lobby(c->lobby_id));
}

template <typename CharT>
void on_write_info_board_t(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // D9
  check_size_v(data.size(), 0, c->game_data.player()->info_board.size() * sizeof(CharT));
  c->game_data.player()->info_board.assign(
      reinterpret_cast<const CharT*>(data.data()),
      data.size() / sizeof(CharT));
}

template <typename CharT>
void on_set_auto_reply_t(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // C7
  check_size_v(data.size(), 0, c->game_data.player()->auto_reply.size() * sizeof(CharT));
  c->game_data.player()->auto_reply.assign(
      reinterpret_cast<const CharT*>(data.data()),
      data.size() / sizeof(CharT));
}

static void on_disable_auto_reply(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // C8
  check_size_v(data.size(), 0);
  c->game_data.player()->auto_reply.clear(0);
}

static void on_set_blocked_senders_list(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // C6
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
    const std::u16string& name,
    const std::u16string& password,
    uint8_t episode,
    uint8_t difficulty,
    uint32_t flags,
    shared_ptr<Episode3::BattleRecordPlayer> battle_player) {

  // A player's actual level is their displayed level - 1, so the minimums for
  // Episode 1 (for example) are actually 1, 20, 40, 80.
  static const uint32_t default_minimum_levels[3][4] = {
      {0, 19, 39, 79}, // Episode 1
      {0, 29, 49, 89}, // Episode 2
      {0, 39, 79, 109}}; // Episode 4

  bool is_ep3 = (flags & Lobby::Flag::EPISODE_3_ONLY);
  if (episode == 0) {
    episode = 0xFF;
  }
  if (((episode != 0xFF) && (episode > 3)) || (episode == 0)) {
    throw invalid_argument("incorrect episode number");
  }

  if (difficulty > 3) {
    throw invalid_argument("incorrect difficulty level");
  }

  auto current_lobby = s->find_lobby(c->lobby_id);
  if (!current_lobby) {
    throw invalid_argument("cannot make a game from outside any lobby");
  }

  uint8_t min_level = ((episode == 0xFF) ? 0 : default_minimum_levels[episode - 1][difficulty]);
  if (!(c->license->privileges & Privilege::FREE_JOIN_GAMES) &&
      (min_level > c->game_data.player()->disp.level)) {
    throw invalid_argument("level too low for difficulty");
  }

  bool item_tracking_enabled = (c->version() == GameVersion::BB) | s->item_tracking_enabled;

  shared_ptr<Lobby> game = s->create_lobby();
  game->name = name;
  game->flags = flags |
      Lobby::Flag::GAME |
      (is_ep3 ? Lobby::Flag::EPISODE_3_ONLY : 0) |
      (item_tracking_enabled ? Lobby::Flag::ITEM_TRACKING_ENABLED : 0);
  game->password = password;
  game->version = c->version();
  game->section_id = c->override_section_id >= 0
      ? c->override_section_id : c->game_data.player()->disp.section_id;
  game->episode = episode;
  game->difficulty = difficulty;
  if (c->override_random_seed >= 0) {
    game->random_seed = c->override_random_seed;
    game->random->seed(game->random_seed);
  }
  if (battle_player) {
    game->battle_player = battle_player;
    battle_player->set_lobby(game);
  }
  game->common_item_creator.reset(new CommonItemCreator(
      s->common_item_data, game->random));
  game->event = Lobby::game_event_for_lobby_event(current_lobby->event);
  game->block = 0xFF;
  game->max_clients = (game->flags & Lobby::Flag::IS_SPECTATOR_TEAM) ? 12 : 4;
  game->min_level = min_level;
  game->max_level = 0xFFFFFFFF;

  bool is_solo = (game->flags & Lobby::Flag::SOLO_MODE);

  // Generate the map variations
  if (is_ep3) {
    game->variations.clear(0);
  } else {
    generate_variations(game->variations, game->random, game->episode, is_solo);
  }

  if (game->version == GameVersion::BB) {
    for (size_t x = 0; x < 4; x++) {
      game->next_item_id[x] = (0x00200000 * x) + 0x00010000;
    }
    game->next_game_item_id = 0x00810000;

    for (size_t area = 0; area < 0x10; area++) {
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
          std::vector<PSOEnemy> area_enemies = parse_map(
              s->battle_params,
              is_solo,
              game->episode,
              game->difficulty,
              map_data,
              false);
          game->enemies.insert(
              game->enemies.end(),
              area_enemies.begin(),
              area_enemies.end());
          c->log.info("[Map/%zu] Loaded %s (%zu entries)",
              area, filename.c_str(), area_enemies.size());
          for (size_t z = 0; z < area_enemies.size(); z++) {
            string e_str = area_enemies[z].str();
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

    c->log.info("Loaded maps contain %zu entries overall", game->enemies.size());
  }

  s->change_client_lobby(c, game);
  c->flags |= Client::Flag::LOADING;

  return game;
}

static void on_create_game_pc(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // C1
  const auto& cmd = check_size_t<C_CreateGame_PC_C1>(data);

  uint32_t flags = Lobby::Flag::NON_V1_ONLY;
  if (cmd.battle_mode) {
    flags |= Lobby::Flag::BATTLE_MODE;
  }
  if (cmd.challenge_mode) {
    flags |= Lobby::Flag::CHALLENGE_MODE;
  }
  create_game_generic(s, c, cmd.name, cmd.password, 1, cmd.difficulty, flags);
}

static void on_create_game_dc_v3(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t command, uint32_t, const string& data) { // 0C C1 EC (EC Ep3 only)
  const auto& cmd = check_size_t<C_CreateGame_DC_V3_0C_C1_Ep3_EC>(data);

  // Only allow EC from Ep3 clients
  bool client_is_ep3 = !!(c->flags & Client::Flag::IS_EPISODE_3);
  if ((command == 0xEC) != client_is_ep3) {
    return;
  }

  uint8_t episode = cmd.episode;
  uint32_t flags = 0;
  if (c->version() == GameVersion::DC) {
    if (episode) {
      flags |= Lobby::Flag::NON_V1_ONLY;
    }
    episode = 1;
  } else if (client_is_ep3) {
    flags |= (Lobby::Flag::NON_V1_ONLY | Lobby::Flag::EPISODE_3_ONLY);
    episode = 0xFF;
  } else { // XB/GC non-Ep3
    flags |= Lobby::Flag::NON_V1_ONLY;
  }

  u16string name = decode_sjis(cmd.name);
  u16string password = decode_sjis(cmd.password);

  if (cmd.battle_mode) {
    flags |= Lobby::Flag::BATTLE_MODE;
  }
  if (cmd.challenge_mode) {
    if (client_is_ep3) {
      flags |= Lobby::Flag::SPECTATORS_FORBIDDEN;
    } else {
      flags |= Lobby::Flag::CHALLENGE_MODE;
    }
  }
  create_game_generic(
      s, c, name.c_str(), password.c_str(), episode, cmd.difficulty, flags);
}

static void on_create_game_bb(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // C1
  const auto& cmd = check_size_t<C_CreateGame_BB_C1>(data);

  uint32_t flags = Lobby::Flag::NON_V1_ONLY;
  if (cmd.battle_mode) {
    flags |= Lobby::Flag::BATTLE_MODE;
  }
  if (cmd.challenge_mode) {
    flags |= Lobby::Flag::CHALLENGE_MODE;
  }
  if (cmd.solo_mode) {
    flags |= Lobby::Flag::SOLO_MODE;
  }
  create_game_generic(
      s, c, cmd.name, cmd.password, cmd.episode, cmd.difficulty, flags);
}

static void on_lobby_name_request(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 8A
  if ((c->version() == GameVersion::DC) && (c->flags & Client::Flag::IS_TRIAL_EDITION)) {
    const auto& cmd = check_size_t<C_ConnectionInfo_DCNTE_8A>(data);
    set_console_client_flags(c, cmd.sub_version);
    send_command(c, 0x8A, 0x01);

  } else {
    check_size_v(data.size(), 0);
    auto l = s->find_lobby(c->lobby_id);
    if (!l) {
      throw invalid_argument("client not in any lobby");
    }
    send_lobby_name(c, l->name.c_str());
  }
}

static void on_client_ready(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 6F
  check_size_v(data.size(), 0);

  auto l = s->find_lobby(c->lobby_id);
  if (!l || !l->is_game()) {
    throw runtime_error("client sent ready command ontside of game");
  }
  c->flags &= (~Client::Flag::LOADING);

  send_resume_game(l, c);
  send_server_time(c);
  // Only get player info again on BB, since on other versions the returned info
  // only includes items that would be saved if the client disconnects
  // unexpectedly (that is, only equipped items are included).
  if (c->version() == GameVersion::BB) {
    send_get_player_info(c);
  }

  if (l->battle_player && (l->flags & Lobby::Flag::START_BATTLE_PLAYER_IMMEDIATELY)) {
    l->battle_player->start();
  }
}



static void on_trade_start(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // D0
  auto& cmd = check_size_t<SC_TradeItems_D0_D3>(data);

  if (c->game_data.pending_item_trade) {
    throw runtime_error("player started a trade when one is already pending");
  }
  if (cmd.item_count > 0x20) {
    throw runtime_error("invalid item count in trade items command");
  }

  auto l = s->find_lobby(c->lobby_id);
  if (!l || !l->is_game()) {
    throw runtime_error("trade command received in non-game lobby");
  }
  auto target_c = l->clients.at(cmd.target_client_id);
  if (!target_c) {
    throw runtime_error("trade command sent to missing player");
  }

  c->game_data.pending_item_trade.reset(new PendingItemTrade());
  c->game_data.pending_item_trade->other_client_id = cmd.target_client_id;
  for (size_t x = 0; x < cmd.item_count; x++) {
    c->game_data.pending_item_trade->items.emplace_back(cmd.items[x]);
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

static void on_trade_execute(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // D2
  check_size_v(data.size(), 0);

  if (!c->game_data.pending_item_trade) {
    throw runtime_error("player executed a trade with none pending");
  }

  auto l = s->find_lobby(c->lobby_id);
  if (!l || !l->is_game()) {
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

static void on_trade_error(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // D4
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
  auto l = s->find_lobby(c->lobby_id);
  if (!l || !l->is_game()) {
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



static void on_card_trade(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t flag, const string& data) { // EE
  if (!(c->flags & Client::Flag::IS_EPISODE_3)) {
    throw runtime_error("non-Ep3 client sent card trade command");
  }
  auto l = s->find_lobby(c->lobby_id);
  if (!(l->flags & Lobby::Flag::EPISODE_3_ONLY)) {
    throw runtime_error("client sent card trade command outside of Ep3 lobby");
  }
  if (!l->is_game()) {
    throw runtime_error("client sent card trade command in non-game lobby");
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
    if (!(target_c->flags & Client::Flag::IS_EPISODE_3)) {
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

static void on_card_auction_join(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // EF
  check_size_v(data.size(), 0);

  if (!(c->flags & Client::Flag::IS_EPISODE_3)) {
    throw runtime_error("non-Ep3 client sent card auction join command");
  }
  auto l = s->find_lobby(c->lobby_id);
  if (!(l->flags & Lobby::Flag::EPISODE_3_ONLY)) {
    throw runtime_error("client sent card auction join command outside of Ep3 lobby");
  }
  if (!l->is_game()) {
    throw runtime_error("client sent card auction join command in non-game lobby");
  }

  if (c->flags & Client::Flag::AWAITING_CARD_AUCTION) {
    return;
  }
  c->flags |= Client::Flag::AWAITING_CARD_AUCTION;

  // Check if any client is still loading
  // TODO: We need to handle clients disconnecting during this procedure.
  // Probably on_client_disconnect needs to check for this case...
  size_t x;
  for (x = 0; x < l->max_clients; x++) {
    if (!l->clients[x]) {
      continue;
    }
    if (!(l->clients[x]->flags & Client::Flag::AWAITING_CARD_AUCTION)) {
      break;
    }
  }
  if (x != l->max_clients) {
    return;
  }

  for (x = 0; x < l->max_clients; x++) {
    if (l->clients[x]) {
      l->clients[x]->flags &= ~Client::Flag::AWAITING_CARD_AUCTION;
    }
  }

  if ((s->ep3_card_auction_points == 0) ||
      (s->ep3_card_auction_min_size == 0) ||
      (s->ep3_card_auction_max_size == 0)) {
    throw runtime_error("card auctions are not configured on this server");
  }

  uint16_t num_cards;
  if (s->ep3_card_auction_min_size == s->ep3_card_auction_max_size) {
    num_cards = s->ep3_card_auction_min_size;
  } else {
    num_cards = s->ep3_card_auction_min_size +
        (random_object<uint16_t>() % (s->ep3_card_auction_max_size - s->ep3_card_auction_min_size + 1));
  }
  num_cards = min<uint16_t>(num_cards, 0x14);

  uint64_t distribution_size = 0;
  for (const auto& it : s->ep3_card_auction_pool) {
    distribution_size += it.second.first;
  }

  S_StartCardAuction_GC_Ep3_EF cmd;
  cmd.points_available = s->ep3_card_auction_points;
  for (size_t z = 0; z < num_cards; z++) {
    uint64_t v = random_object<uint64_t>() % distribution_size;
    for (const auto& it : s->ep3_card_auction_pool) {
      if (v >= it.second.first) {
        v -= it.second.first;
      } else {
        cmd.entries[z].card_id = s->ep3_data_index->definition_for_card_name(it.first)->def.card_id.load();
        cmd.entries[z].min_price = it.second.second;
        break;
      }
    }
  }
  send_command_t(l, 0xEF, num_cards, cmd);
}



static void on_team_command_bb(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t command, uint32_t, const string&) { // EA

  // TODO: Implement teams. This command has a very large number of subcommands
  // (up to 20EA!).
  if (command == 0x01EA) {
    send_lobby_message_box(c, u"$C6Teams are not supported.");
  } else if (command == 0x14EA) {
    // Do nothing (for now)
  } else {
    throw invalid_argument("unimplemented team command");
  }
}



static void on_connected_patch(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) {
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

static void on_login_patch(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) {
  const auto& cmd = check_size_t<C_Login_Patch_04>(data);

  try {
    auto l = s->license_manager->verify_bb(cmd.username, cmd.password);
    c->set_license(l);

  } catch (const incorrect_password& e) {
    u16string message = u"Login failed: " + decode_sjis(e.what());
    send_message_box(c, message.c_str());
    c->should_disconnect = true;
    return;

  } catch (const missing_license& e) {
    if (!s->allow_unregistered_users) {
      u16string message = u"Login failed: " + decode_sjis(e.what());
      send_message_box(c, message.c_str());
      c->should_disconnect = true;
      return;
    } else {
      shared_ptr<License> l = LicenseManager::create_license_bb(
          fnv1a32(cmd.username) & 0x7FFFFFFF, cmd.username, cmd.password, true);
      s->license_manager->add(l);
      c->set_license(l);
    }
  }

  // On BB we can use colors and newlines should be \n; on PC we can't use
  // colors, the text is auto-word-wrapped, and newlines should be \r\n.
  const u16string& message = (c->flags & Client::Flag::IS_BB_PATCH)
      ? s->bb_patch_server_message : s->pc_patch_server_message;
  if (!message.empty()) {
    send_message_box(c, message.c_str());
  }

  auto index = (c->flags & Client::Flag::IS_BB_PATCH) ?
      s->bb_patch_file_index : s->pc_patch_file_index;
  if (index.get()) {
    send_command(c, 0x0B, 0x00); // Start patch session; go to root directory

    vector<string> path_directories;
    for (const auto& file : index->all_files()) {
      change_to_directory_patch(c, path_directories, file->path_directories);

      S_FileChecksumRequest_Patch_0C req = {
          c->patch_file_checksum_requests.size(), file->name};
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

static void on_checksum_patch(shared_ptr<ServerState>,
    shared_ptr<Client> c, uint16_t, uint32_t, const string& data) { // 0F
  auto& cmd = check_size_t<C_FileInformation_Patch_0F>(data);
  auto& req = c->patch_file_checksum_requests.at(cmd.request_id);
  req.crc32 = cmd.checksum;
  req.size = cmd.size;
  req.response_received = true;
}

static void on_checksums_done_patch(shared_ptr<ServerState>,
    shared_ptr<Client> c, uint16_t, uint32_t, const string&) { // 10

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



static void on_ignored_command(shared_ptr<ServerState>, shared_ptr<Client>,
    uint16_t, uint32_t, const string&) { }

static void on_unimplemented_command(shared_ptr<ServerState>,
    shared_ptr<Client> c, uint16_t command, uint32_t flag, const string& data) {
  c->log.warning("Unknown command: size=%04zX command=%04hX flag=%08" PRIX32,
      data.size(), command, flag);
  throw invalid_argument("unimplemented command");
}



typedef void (*on_command_t)(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t command, uint32_t flag, const string& data);

// Command handler table, indexed by command number and game version. Null
// entries in this table cause on_unimplemented_command to be called, which
// disconnects the client.
static on_command_t handlers[0x100][6] = {
  //        PATCH                    DC                          PC                               GC                           XB                           BB
  /* 00 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 00 */
  /* 01 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 01 */
  /* 02 */ {on_connected_patch,      nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 02 */
  /* 03 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 03 */
  /* 04 */ {on_login_patch,          nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 04 */
  /* 05 */ {nullptr,                 on_ignored_command,         on_ignored_command,              on_ignored_command,          on_ignored_command,          on_ignored_command,             }, /* 05 */
  /* 06 */ {nullptr,                 on_chat_dc_v3,              on_chat_pc_bb,                   on_chat_dc_v3,               on_chat_dc_v3,               on_chat_pc_bb,                  }, /* 06 */
  /* 07 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 07 */
  /* 08 */ {nullptr,                 on_game_list_request,       on_game_list_request,            on_game_list_request,        on_game_list_request,        on_game_list_request,           }, /* 08 */
  /* 09 */ {nullptr,                 on_menu_item_info_request,  on_menu_item_info_request,       on_menu_item_info_request,   on_menu_item_info_request,   on_menu_item_info_request,      }, /* 09 */
  /* 0A */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 0A */
  /* 0B */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 0B */
  /* 0C */ {nullptr,                 on_create_game_dc_v3,       nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 0C */
  /* 0D */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 0D */
  /* 0E */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 0E */
  /* 0F */ {on_checksum_patch,       nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 0F */
  /* 10 */ {on_checksums_done_patch, on_menu_selection,          on_menu_selection,               on_menu_selection,           on_menu_selection,           on_menu_selection,              }, /* 10 */
  /* 11 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 11 */
  /* 12 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 12 */
  /* 13 */ {nullptr,                 on_ignored_command,         on_ignored_command,              on_ignored_command,          on_ignored_command,          on_ignored_command,             }, /* 13 */
  /* 14 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 14 */
  /* 15 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 15 */
  /* 16 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 16 */
  /* 17 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 17 */
  /* 18 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 18 */
  /* 19 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 19 */
  /* 1A */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 1A */
  /* 1B */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 1B */
  /* 1C */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 1C */
  /* 1D */ {nullptr,                 on_ignored_command,         on_ignored_command,              on_ignored_command,          on_ignored_command,          on_ignored_command,             }, /* 1D */
  /* 1E */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 1E */
  /* 1F */ {nullptr,                 on_info_menu_request_dc_pc, on_info_menu_request_dc_pc,      nullptr,                     nullptr,                     nullptr,                        }, /* 1F */
  //        PATCH                    DC                          PC                               GC                           XB                           BB
  /* 20 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 20 */
  /* 21 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 21 */
  /* 22 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     on_ignored_command,             }, /* 22 */
  /* 23 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 23 */
  /* 24 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 24 */
  /* 25 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 25 */
  /* 26 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 26 */
  /* 27 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 27 */
  /* 28 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 28 */
  /* 29 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 29 */
  /* 2A */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 2A */
  /* 2B */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 2B */
  /* 2C */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 2C */
  /* 2D */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 2D */
  /* 2E */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 2E */
  /* 2F */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 2F */
  /* 30 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 30 */
  /* 31 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 31 */
  /* 32 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 32 */
  /* 33 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 33 */
  /* 34 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 34 */
  /* 35 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 35 */
  /* 36 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 36 */
  /* 37 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 37 */
  /* 38 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 38 */
  /* 39 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 39 */
  /* 3A */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 3A */
  /* 3B */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 3B */
  /* 3C */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 3C */
  /* 3D */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 3D */
  /* 3E */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 3E */
  /* 3F */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 3F */
  //        PATCH                    DC                          PC                               GC                           XB                           BB
  /* 40 */ {nullptr,                 on_card_search,             on_card_search,                  on_card_search,              on_card_search,              on_card_search,                 }, /* 40 */
  /* 41 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 41 */
  /* 42 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 42 */
  /* 43 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 43 */
  /* 44 */ {nullptr,                 on_ignored_command,         on_ignored_command,              on_ignored_command,          on_ignored_command,          on_ignored_command,             }, /* 44 */
  /* 45 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 45 */
  /* 46 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 46 */
  /* 47 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 47 */
  /* 48 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 48 */
  /* 49 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 49 */
  /* 4A */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 4A */
  /* 4B */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 4B */
  /* 4C */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 4C */
  /* 4D */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 4D */
  /* 4E */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 4E */
  /* 4F */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 4F */
  /* 50 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 50 */
  /* 51 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 51 */
  /* 52 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 52 */
  /* 53 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 53 */
  /* 54 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 54 */
  /* 55 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 55 */
  /* 56 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 56 */
  /* 57 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 57 */
  /* 58 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 58 */
  /* 59 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 59 */
  /* 5A */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 5A */
  /* 5B */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 5B */
  /* 5C */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 5C */
  /* 5D */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 5D */
  /* 5E */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 5E */
  /* 5F */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 5F */
  //        PATCH                    DC                          PC                               GC                           XB                           BB
  /* 60 */ {nullptr,                 on_game_command,            on_game_command,                 on_game_command,             on_game_command,             on_game_command,                }, /* 60 */
  /* 61 */ {nullptr,                 on_player_data,             on_player_data,                  on_player_data,              on_player_data,              on_player_data,                 }, /* 61 */
  /* 62 */ {nullptr,                 on_game_command,            on_game_command,                 on_game_command,             on_game_command,             on_game_command,                }, /* 62 */
  /* 63 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 63 */
  /* 64 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 64 */
  /* 65 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 65 */
  /* 66 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 66 */
  /* 67 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 67 */
  /* 68 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 68 */
  /* 69 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 69 */
  /* 6A */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 6A */
  /* 6B */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 6B */
  /* 6C */ {nullptr,                 on_game_command,            on_game_command,                 on_game_command,             on_game_command,             on_game_command,                }, /* 6C */
  /* 6D */ {nullptr,                 on_game_command,            on_game_command,                 on_game_command,             on_game_command,             on_game_command,                }, /* 6D */
  /* 6E */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 6E */
  /* 6F */ {nullptr,                 on_client_ready,            on_client_ready,                 on_client_ready,             on_client_ready,             on_client_ready,                }, /* 6F */
  /* 70 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 70 */
  /* 71 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 71 */
  /* 72 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 72 */
  /* 73 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 73 */
  /* 74 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 74 */
  /* 75 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 75 */
  /* 76 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 76 */
  /* 77 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 77 */
  /* 78 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 78 */
  /* 79 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 79 */
  /* 7A */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 7A */
  /* 7B */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 7B */
  /* 7C */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 7C */
  /* 7D */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 7D */
  /* 7E */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 7E */
  /* 7F */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 7F */
  //        PATCH                    DC                          PC                               GC                           XB                           BB
  /* 80 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 80 */
  /* 81 */ {nullptr,                 on_simple_mail,             on_simple_mail,                  on_simple_mail,              on_simple_mail,              on_simple_mail,                 }, /* 81 */
  /* 82 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 82 */
  /* 83 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 83 */
  /* 84 */ {nullptr,                 on_change_lobby,            on_change_lobby,                 on_change_lobby,             on_change_lobby,             on_change_lobby,                }, /* 84 */
  /* 85 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 85 */
  /* 86 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 86 */
  /* 87 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 87 */
  /* 88 */ {nullptr,                 on_login_8_dcnte,           nullptr,                         on_login_8_dcnte,            nullptr,                     nullptr,                        }, /* 88 */
  /* 89 */ {nullptr,                 on_change_arrow_color,      on_change_arrow_color,           on_change_arrow_color,       on_change_arrow_color,       on_change_arrow_color,          }, /* 89 */
  /* 8A */ {nullptr,                 on_lobby_name_request,      on_lobby_name_request,           on_lobby_name_request,       on_lobby_name_request,       on_lobby_name_request,          }, /* 8A */
  /* 8B */ {nullptr,                 on_login_b_dcnte,           nullptr,                         on_login_b_dcnte,            nullptr,                     nullptr,                        }, /* 8B */
  /* 8C */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 8C */
  /* 8D */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 8D */
  /* 8E */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 8E */
  /* 8F */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 8F */
  /* 90 */ {nullptr,                 on_login_0_dc_pc_v3,        nullptr,                         on_login_0_dc_pc_v3,         nullptr,                     nullptr,                        }, /* 90 */
  /* 91 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 91 */
  /* 92 */ {nullptr,                 on_login_2_dc,              nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 92 */
  /* 93 */ {nullptr,                 on_login_3_dc_pc_v3,        nullptr,                         on_login_3_dc_pc_v3,         nullptr,                     on_login_bb,                    }, /* 93 */
  /* 94 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 94 */
  /* 95 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 95 */
  /* 96 */ {nullptr,                 on_client_checksum,         on_client_checksum,              on_client_checksum,          on_client_checksum,          nullptr,                        }, /* 96 */
  /* 97 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 97 */
  /* 98 */ {nullptr,                 on_player_data,             on_player_data,                  on_player_data,              on_player_data,              on_player_data,                 }, /* 98 */
  /* 99 */ {nullptr,                 on_ignored_command,         on_ignored_command,              on_ignored_command,          on_ignored_command,          on_ignored_command,             }, /* 99 */
  /* 9A */ {nullptr,                 on_login_a_dc_pc_v3,        on_login_a_dc_pc_v3,             on_login_a_dc_pc_v3,         nullptr,                     nullptr,                        }, /* 9A */
  /* 9B */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* 9B */
  /* 9C */ {nullptr,                 on_login_c_dc_pc_v3,        on_login_c_dc_pc_v3,             on_login_c_dc_pc_v3,         on_login_c_dc_pc_v3,         nullptr,                        }, /* 9C */
  /* 9D */ {nullptr,                 on_login_d_e_dc_pc_v3,      on_login_d_e_dc_pc_v3,           on_login_d_e_dc_pc_v3,       on_login_d_e_dc_pc_v3,       nullptr,                        }, /* 9D */
  /* 9E */ {nullptr,                 nullptr,                    on_login_d_e_dc_pc_v3,           on_login_d_e_dc_pc_v3,       on_login_d_e_dc_pc_v3,       nullptr,                        }, /* 9E */
  /* 9F */ {nullptr,                 nullptr,                    nullptr,                         on_return_client_config,     on_return_client_config,     nullptr,                        }, /* 9F */
  //        PATCH                    DC                          PC                               GC                           XB                           BB
  /* A0 */ {nullptr,                 on_change_ship,             on_change_ship,                  on_change_ship,              on_change_ship,              on_change_ship,                 }, /* A0 */
  /* A1 */ {nullptr,                 on_change_block,            on_change_block,                 on_change_block,             on_change_block,             on_change_block,                }, /* A1 */
  /* A2 */ {nullptr,                 on_quest_list_request,      on_quest_list_request,           on_quest_list_request,       on_quest_list_request,       on_quest_list_request,          }, /* A2 */
  /* A3 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* A3 */
  /* A4 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* A4 */
  /* A5 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* A5 */
  /* A6 */ {nullptr,                 nullptr,                    nullptr,                         on_ignored_command,          on_ignored_command,          nullptr,                        }, /* A6 */
  /* A7 */ {nullptr,                 nullptr,                    nullptr,                         on_ignored_command,          on_ignored_command,          nullptr,                        }, /* A7 */
  /* A8 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* A8 */
  /* A9 */ {nullptr,                 on_ignored_command,         on_ignored_command,              on_ignored_command,          on_ignored_command,          on_ignored_command,             }, /* A9 */
  /* AA */ {nullptr,                 nullptr,                    on_update_quest_statistics,      on_update_quest_statistics,  on_update_quest_statistics,  on_update_quest_statistics,     }, /* AA */
  /* AB */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* AB */
  /* AC */ {nullptr,                 nullptr,                    nullptr,                         on_quest_barrier,            on_quest_barrier,            on_quest_barrier,               }, /* AC */
  /* AD */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* AD */
  /* AE */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* AE */
  /* AF */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* AF */
  /* B0 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* B0 */
  /* B1 */ {nullptr,                 on_server_time_request,     on_server_time_request,          on_server_time_request,      on_server_time_request,      nullptr,                        }, /* B1 */
  /* B2 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* B2 */
  /* B3 */ {nullptr,                 on_function_call_result,    on_function_call_result,         on_function_call_result,     on_function_call_result,     on_function_call_result,        }, /* B3 */
  /* B4 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* B4 */
  /* B5 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* B5 */
  /* B6 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* B6 */
  /* B7 */ {nullptr,                 nullptr,                    nullptr,                         on_ignored_command,          nullptr,                     nullptr,                        }, /* B7 */
  /* B8 */ {nullptr,                 nullptr,                    nullptr,                         on_ignored_command,          nullptr,                     nullptr,                        }, /* B8 */
  /* B9 */ {nullptr,                 nullptr,                    nullptr,                         on_ignored_command,          nullptr,                     nullptr,                        }, /* B9 */
  /* BA */ {nullptr,                 nullptr,                    nullptr,                         on_ep3_meseta_transaction,   nullptr,                     nullptr,                        }, /* BA */
  /* BB */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* BB */
  /* BC */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* BC */
  /* BD */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* BD */
  /* BE */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* BE */
  /* BF */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* BF */
  //        PATCH                    DC                          PC                               GC                           XB                           BB
  /* C0 */ {nullptr,                 on_choice_search,           nullptr,                         on_choice_search,            on_choice_search,            nullptr,                        }, /* C0 */
  /* C1 */ {nullptr,                 on_create_game_dc_v3,       on_create_game_pc,               on_create_game_dc_v3,        on_create_game_dc_v3,        on_create_game_bb,              }, /* C1 */
  /* C2 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* C2 */
  /* C3 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* C3 */
  /* C4 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* C4 */
  /* C5 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* C5 */
  /* C6 */ {nullptr,                 nullptr,                    on_set_blocked_senders_list,     on_set_blocked_senders_list, on_set_blocked_senders_list, on_set_blocked_senders_list,    }, /* C6 */
  /* C7 */ {nullptr,                 nullptr,                    on_set_auto_reply_t<char16_t>,   on_set_auto_reply_t<char>,   on_set_auto_reply_t<char>,   on_set_auto_reply_t<char16_t>,  }, /* C7 */
  /* C8 */ {nullptr,                 nullptr,                    on_disable_auto_reply,           on_disable_auto_reply,       on_disable_auto_reply,       on_disable_auto_reply,          }, /* C8 */
  /* C9 */ {nullptr,                 nullptr,                    nullptr,                         on_game_command,             nullptr,                     nullptr,                        }, /* C9 */
  /* CA */ {nullptr,                 nullptr,                    nullptr,                         on_ep3_server_data_request,  nullptr,                     nullptr,                        }, /* CA */
  /* CB */ {nullptr,                 nullptr,                    nullptr,                         on_game_command,             nullptr,                     nullptr,                        }, /* CB */
  /* CC */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* CC */
  /* CD */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* CD */
  /* CE */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* CE */
  /* CF */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* CF */
  /* D0 */ {nullptr,                 nullptr,                    nullptr,                         on_trade_start,              on_trade_start,              on_trade_start,                 }, /* D0 */
  /* D1 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* D1 */
  /* D2 */ {nullptr,                 nullptr,                    nullptr,                         on_trade_execute,            on_trade_execute,            on_trade_execute,               }, /* D2 */
  /* D3 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* D3 */
  /* D4 */ {nullptr,                 nullptr,                    nullptr,                         on_trade_error,              on_trade_error,              on_trade_error,                 }, /* D4 */
  /* D5 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* D5 */
  /* D6 */ {nullptr,                 nullptr,                    nullptr,                         on_message_box_closed,       on_message_box_closed,       nullptr,                        }, /* D6 */
  /* D7 */ {nullptr,                 nullptr,                    nullptr,                         on_gba_file_request,         on_gba_file_request,         nullptr,                        }, /* D7 */
  /* D8 */ {nullptr,                 nullptr,                    on_info_board_request,           on_info_board_request,       on_info_board_request,       on_info_board_request,          }, /* D8 */
  /* D9 */ {nullptr,                 nullptr,                    on_write_info_board_t<char16_t>, on_write_info_board_t<char>, on_write_info_board_t<char>, on_write_info_board_t<char16_t>,}, /* D9 */
  /* DA */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* DA */
  /* DB */ {nullptr,                 nullptr,                    nullptr,                         on_verify_license_v3,        on_verify_license_v3,        nullptr,                        }, /* DB */
  /* DC */ {nullptr,                 nullptr,                    nullptr,                         on_ep3_counter_state,        nullptr,                     on_guild_card_data_request_bb,  }, /* DC */
  /* DD */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* DD */
  /* DE */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* DE */
  /* DF */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* DF */
  //        PATCH                    DC                          PC                               GC                           XB                           BB
  /* E0 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     on_key_config_request_bb,       }, /* E0 */
  /* E1 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* E1 */
  /* E2 */ {nullptr,                 nullptr,                    nullptr,                         on_ep3_tournament_control,   nullptr,                     on_update_key_config_bb,        }, /* E2 */
  /* E3 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     on_player_preview_request_bb,   }, /* E3 */
  /* E4 */ {nullptr,                 nullptr,                    nullptr,                         on_ep3_battle_table_state,   nullptr,                     nullptr,                        }, /* E4 */
  /* E5 */ {nullptr,                 nullptr,                    nullptr,                         on_ep3_battle_table_confirm, nullptr,                     on_create_character_bb,         }, /* E5 */
  /* E6 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* E6 */
  /* E7 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     on_return_player_data_bb,       }, /* E7 */
  /* E8 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     on_client_checksum_bb,          }, /* E8 */
  /* E9 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* E9 */
  /* EA */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     on_team_command_bb,             }, /* EA */
  /* EB */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     on_stream_file_request_bb,      }, /* EB */
  /* EC */ {nullptr,                 nullptr,                    nullptr,                         on_create_game_dc_v3,        nullptr,                     on_leave_char_select_bb,        }, /* EC */
  /* ED */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     on_change_account_data_bb,      }, /* ED */
  /* EE */ {nullptr,                 nullptr,                    nullptr,                         on_card_trade,               nullptr,                     nullptr,                        }, /* EE */
  /* EF */ {nullptr,                 nullptr,                    nullptr,                         on_card_auction_join,        nullptr,                     nullptr,                        }, /* EF */
  /* F0 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* F0 */
  /* F1 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* F1 */
  /* F2 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* F2 */
  /* F3 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* F3 */
  /* F4 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* F4 */
  /* F5 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* F5 */
  /* F6 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* F6 */
  /* F7 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* F7 */
  /* F8 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* F8 */
  /* F9 */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* F9 */
  /* FA */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* FA */
  /* FB */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* FB */
  /* FC */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* FC */
  /* FD */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* FD */
  /* FE */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* FE */
  /* FF */ {nullptr,                 nullptr,                    nullptr,                         nullptr,                     nullptr,                     nullptr,                        }, /* FF */
  //        PATCH                    DC                          PC                               GC                           XB                           BB
};

static void check_unlicensed_command(GameVersion version, uint8_t command) {
  switch (version) {
    case GameVersion::DC:
      // newserv doesn't actually know that DC clients are DC until it receives
      // an appropriate login command (93, 9A, or 9D), but those commands also
      // log the client in, so this case should never be executed.
      throw logic_error("cannot check unlicensed command for DC client");
    case GameVersion::PC:
      if (command != 0x9A && command != 0x9D) {
        throw runtime_error("only commands 9A and 9D may be sent before login");
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
          command != 0x9D && // DC v2, GC trial edition
          command != 0x9E && // GC non-trial
          command != 0xDB) { // GC non-trial
        throw runtime_error("only commands 88, 8B, 90, 93, 9A, 9D, 9E, and DB may be sent before login");
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

void on_command(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t command, uint32_t flag, const string& data) {
  string encoded_name;
  auto player = c->game_data.player(false);
  if (player) {
    encoded_name = remove_language_marker(encode_sjis(player->disp.name));
  }

  // Most of the command handlers assume the client is registered, logged in,
  // and not banned (and therefore that c->license is not null), so the client
  // is allowed to access normal functionality. This check prevents clients from
  // sneakily sending commands to access functionality without logging in.
  if (!c->license.get()) {
    check_unlicensed_command(c->version(), command);
  }

  auto fn = handlers[command & 0xFF][static_cast<size_t>(c->version())];
  if (fn) {
    fn(s, c, command, flag, data);
  } else {
    on_unimplemented_command(s, c, command, flag, data);
  }
}
