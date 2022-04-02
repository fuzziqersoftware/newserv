#include "ReceiveCommands.hh"

#include <inttypes.h>
#include <string.h>

#include <memory>
#include <phosg/Filesystem.hh>
#include <phosg/Network.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>
#include <phosg/Time.hh>

#include "PSOProtocol.hh"
#include "FileContentsCache.hh"
#include "Text.hh"
#include "SendCommands.hh"
#include "ReceiveSubcommands.hh"
#include "ChatCommands.hh"
#include "ProxyServer.hh"

using namespace std;

#define CONFIG_MAGIC 0x48615467



template <typename T>
const T& check_size_t(
    const string& data,
    size_t min_size = sizeof(T),
    size_t max_size = sizeof(T)) {
  if (data.size() < min_size) {
    throw runtime_error(string_printf(
        "command too small (expected at least 0x%zX bytes, received 0x%zX bytes)",
        min_size, data.size()));
  }
  if (data.size() > max_size) {
    throw runtime_error(string_printf(
        "command too large (expected at most 0x%zX bytes, received 0x%zX bytes)",
        max_size, data.size()));
  }
  return *reinterpret_cast<const T*>(data.data());
}



enum ClientStateBB {
  // initial connection. server will redirect client to another port.
  INITIAL_LOGIN = 0x00,
  // second connection. server will send client game data and account data.
  DOWNLOAD_DATA = 0x01,
  // third connection. choose character menu
  CHOOSE_PLAYER = 0x02,
  // fourth connection, used for saving characters only. if you do not create a
  // character, server sets this state in order to skip it.
  SAVE_PLAYER   = 0x03,
  // last connection. redirects client to login server.
  SHIP_SELECT   = 0x04,
};



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



////////////////////////////////////////////////////////////////////////////////

void process_connect(std::shared_ptr<ServerState> s, std::shared_ptr<Client> c) {
  switch (c->server_behavior) {
    case ServerBehavior::SPLIT_RECONNECT: {
      uint16_t pc_port = s->name_to_port_config.at("pc-login")->port;
      uint16_t gc_port = s->name_to_port_config.at("gc-jp10")->port;
      send_pc_gc_split_reconnect(c, s->connect_address_for_client(c), pc_port, gc_port);
      c->should_disconnect = true;
      break;
    }

    case ServerBehavior::LOGIN_SERVER:
      send_server_init(s, c, true);
      if (s->pre_lobby_event) {
        send_change_event(c, s->pre_lobby_event);
      }
      break;

    case ServerBehavior::LOBBY_SERVER:
    case ServerBehavior::DATA_SERVER_BB:
    case ServerBehavior::PATCH_SERVER:
      send_server_init(s, c, false);
      break;

    default:
      log(ERROR, "Unimplemented behavior: %" PRId64,
          static_cast<int64_t>(c->server_behavior));
  }
}

void process_login_complete(shared_ptr<ServerState> s, shared_ptr<Client> c) {
  if (c->server_behavior == ServerBehavior::LOGIN_SERVER) {
    // on the login server, send the ep3 updates and the main menu or welcome
    // message
    if (c->flags & Client::Flag::EPISODE_3) {
      send_ep3_card_list_update(c);
      send_ep3_rank_update(c);
    }

    if (s->welcome_message.empty() ||
        (c->flags & Client::Flag::NO_MESSAGE_BOX_CLOSE_CONFIRMATION) ||
        !(c->flags & Client::Flag::AT_WELCOME_MESSAGE)) {
      c->flags &= ~Client::Flag::AT_WELCOME_MESSAGE;
      send_menu(c, s->name.c_str(), MAIN_MENU_ID, s->main_menu, false);
    } else {
      send_message_box(c, s->welcome_message.c_str());
    }

  } else if (c->server_behavior == ServerBehavior::LOBBY_SERVER) {

    // if the client is BB, load thair player and account data
    if (c->version == GameVersion::BB) {
      string account_filename = filename_for_account_bb(c->license->username);
      try {
        c->player.load_account_data(account_filename);
      } catch (const exception& e) {
        c->player.load_account_data("system/blueburst/default.nsa");
      }

      c->player.bank_name = string_printf("player%d", c->bb_player_index + 1);

      string player_filename = filename_for_player_bb(c->license->username,
          c->bb_player_index);
      try {
        c->player.load_player_data(player_filename);
      } catch (const exception&) {
        send_message_box(c, u"$C6Your player data cannot be found.");
        c->should_disconnect = true;
        return;
      }

      string bank_filename = filename_for_bank_bb(c->license->username,
          c->player.bank_name);
      try {
        c->player.bank.load(bank_filename);
      } catch (const exception&) {
        send_message_box(c, u"$C6Your bank data cannot be found.");
        c->should_disconnect = true;
        return;
      }

      send_complete_player_bb(c);
    }

    send_lobby_list(c, s);
    send_get_player_info(c);
  }
}



void process_disconnect(shared_ptr<ServerState> s, shared_ptr<Client> c) {
  // if the client was in a lobby, remove them and notify the other clients
  if (c->lobby_id) {
    s->remove_client_from_lobby(c);
  }

  if (c->version == GameVersion::BB) {
    // TODO: Make a timer event for each connected player that saves their data
    // periodically, not only when they disconnect
    // TODO: Track play time somewhere
    // c->player.disp.play_time += ((now() - c->play_time_begin) / 1000000);
    string account_filename = filename_for_account_bb(c->license->username);
    string player_filename = filename_for_player_bb(c->license->username,
        c->bb_player_index);
    string bank_filename = filename_for_bank_bb(c->license->username,
        c->player.bank_name);
    c->player.save_account_data(account_filename);
    c->player.save_player_data(player_filename);
    c->player.bank.save(bank_filename);
  }
}



////////////////////////////////////////////////////////////////////////////////

void process_verify_license_gc(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // DB
  const auto& cmd = check_size_t<C_VerifyLicense_GC_DB>(data);

  uint32_t serial_number = strtoul(cmd.serial_number.c_str(), nullptr, 16);
  try {
    c->license = s->license_manager->verify_gc(serial_number,
        cmd.access_key.c_str(), cmd.password.c_str());
  } catch (const exception& e) {
    if (!s->allow_unregistered_users) {
      u16string message = u"Login failed: " + decode_sjis(e.what());
      send_message_box(c, message.c_str());
      c->should_disconnect = true;
      return;
    } else {
      auto l = LicenseManager::create_license_gc(serial_number,
          cmd.access_key.c_str(), cmd.password.c_str(), true);
      s->license_manager->add(l);
      c->license = l;
    }
  }

  c->flags |= flags_for_version(c->version, cmd.sub_version);
  send_command(c, 0x9A, 0x02);
}

void process_login_a_dc_pc_gc(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 9A
  const auto& cmd = check_size_t<C_Login_DC_PC_GC_9A>(data);

  c->flags |= flags_for_version(c->version, cmd.sub_version);

  uint32_t serial_number = strtoul(cmd.serial_number.c_str(), nullptr, 16);
  try {
    if (c->version == GameVersion::GC) {
      c->license = s->license_manager->verify_gc(serial_number,
          cmd.access_key.c_str(), nullptr);
    } else {
      c->license = s->license_manager->verify_pc(serial_number,
          cmd.access_key.c_str(), nullptr);
    }
  } catch (const exception& e) {
    // On GC, the client should have sent a different command containing the
    // password already, which should have created and added a temporary
    // license. So, if no license exists at this point, disconnect the client
    // even if unregistered clients are allowed.
    u16string message = u"Login failed: " + decode_sjis(e.what());
    send_message_box(c, message.c_str());
    c->should_disconnect = true;
    return;
  }

  send_command(c, 0x9C, 0x01);
}

void process_login_c_dc_pc_gc(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 9C
  const auto& cmd = check_size_t<C_Register_DC_PC_GC_9C>(data);

  c->flags |= flags_for_version(c->version, cmd.sub_version);

  uint32_t serial_number = strtoul(cmd.serial_number.c_str(), nullptr, 16);
  try {
    if (c->version == GameVersion::GC) {
      c->license = s->license_manager->verify_gc(serial_number,
          cmd.access_key.c_str(), cmd.password.c_str());
    } else {
      c->license = s->license_manager->verify_pc(serial_number,
          cmd.access_key.c_str(), cmd.password.c_str());
    }
  } catch (const exception& e) {
    if (!s->allow_unregistered_users) {
      u16string message = u"Login failed: " + decode_sjis(e.what());
      send_message_box(c, message.c_str());
      c->should_disconnect = true;
      return;
    } else {
      shared_ptr<License> l;
      if (c->version == GameVersion::GC) {
        l = LicenseManager::create_license_gc(serial_number,
            cmd.access_key.c_str(), cmd.password.c_str(), true);
      } else {
        l = LicenseManager::create_license_pc(serial_number,
            cmd.access_key.c_str(), cmd.password.c_str(), true);
      }
      s->license_manager->add(l);
      c->license = l;
    }
  }

  send_command(c, 0x9C, 0x01);
}

void process_login_d_e_pc_gc(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t command, uint32_t, const string& data) { // 9D 9E

  // The client sends extra unused data the first time it sends these commands,
  // hence the odd check_size calls here

  const C_Login_PC_9D* base_cmd;
  if (command == 0x9D) {
    base_cmd = &check_size_t<C_Login_PC_9D>(data,
        sizeof(C_Login_PC_9D), sizeof(C_Login_PC_9D) + 0x84);

  } else if (command == 0x9E) {
    const auto& cmd = check_size_t<C_Login_GC_9E>(data,
        sizeof(C_Login_GC_9E), sizeof(C_Login_GC_9E) + 0x64);
    base_cmd = &cmd;

    try {
      c->import_config(cmd.client_config.cfg);
    } catch (const invalid_argument&) {
      // If we can't import the config, assume that the client was not connected
      // to newserv before, so we should show the welcome message.
      c->flags |= Client::Flag::AT_WELCOME_MESSAGE;
      c->bb_game_state = 0;
      c->bb_player_index = 0;
    }

  } else {
    throw logic_error("9D/9E handler called for incorrect command");
  }

  c->flags |= flags_for_version(c->version, base_cmd->sub_version);

  uint32_t serial_number = strtoul(base_cmd->serial_number.c_str(), nullptr, 16);
  try {
    if (c->version == GameVersion::GC) {
      c->license = s->license_manager->verify_gc(serial_number,
          base_cmd->access_key.c_str(), nullptr);
    } else {
      c->license = s->license_manager->verify_pc(serial_number,
          base_cmd->access_key.c_str(), nullptr);
    }
  } catch (const exception& e) {
    // See comment in 9A handler about why we do this even if unregistered users
    // are allowed on the server
    u16string message = u"Login failed: " + decode_sjis(e.what());
    send_message_box(c, message.c_str());
    c->should_disconnect = true;
    return;
  }

  if ((c->flags & Client::Flag::EPISODE_3) && (s->ep3_menu_song >= 0)) {
    send_ep3_change_music(c, s->ep3_menu_song);
  }

  send_update_client_config(c);

  process_login_complete(s, c);
}

void process_login_bb(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 93
  const auto& cmd = check_size_t<C_Login_BB_93>(data);

  c->flags |= flags_for_version(c->version, 0);

  try {
    c->license = s->license_manager->verify_bb(
        cmd.username.c_str(), cmd.password.c_str());
  } catch (const exception& e) {
    u16string message = u"Login failed: " + decode_sjis(e.what());
    send_message_box(c, message.c_str());
    c->should_disconnect = true;
    return;
  }

  try {
    c->import_config(cmd.client_config.cfg);
    c->bb_game_state++;
  } catch (const invalid_argument&) {
    c->bb_game_state = 0;
    c->bb_player_index = 0;
  }

  send_client_init_bb(c, 0);

  switch (c->bb_game_state) {
    case ClientStateBB::INITIAL_LOGIN:
      // first login? send them to the other port
      send_reconnect(c, s->connect_address_for_client(c),
          s->name_to_port_config.at("bb-data1")->port);
      break;

    case ClientStateBB::DOWNLOAD_DATA: {
      // download data? send them their account data and player previews
      string account_filename = filename_for_account_bb(c->license->username);
      try {
        c->player.load_account_data(account_filename);
      } catch (const exception& e) {
        c->player.load_account_data("system/blueburst/default.nsa");
      }
      break;
    }

    case ClientStateBB::CHOOSE_PLAYER:
    case ClientStateBB::SAVE_PLAYER:
      // just wait; the command handlers will handle it
      break;

    case ClientStateBB::SHIP_SELECT:
      // this happens on the login server and later
      process_login_complete(s, c);
      break;

    default:
      send_reconnect(c, s->connect_address_for_client(c),
          s->name_to_port_config.at("bb-login")->port);
  }
}

void process_client_checksum(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 96
  check_size_t<C_ClientChecksum_GC_96>(data);
  send_command(c, 0x97, 0x01);
}

void process_server_time_request(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // B1
  check_size(data.size(), 0);
  send_server_time(c);
}



////////////////////////////////////////////////////////////////////////////////
// Ep3 commands. Note that these commands are not at all functional. The command
// handlers that partially worked were lost in a dead hard drive, unfortunately.

void process_ep3_jukebox(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t command, uint32_t, const string& data) {
  const auto& in_cmd = check_size_t<C_Meseta_GC_Ep3_BA>(data);

  S_Meseta_GC_Ep3_BA out_cmd = {1000000, 0x80E8, in_cmd.unknown_token};

  auto l = s->find_lobby(c->lobby_id);
  if (!l || !(l->flags & Lobby::Flag::EPISODE_3_ONLY)) {
    return;
  }

  send_command(c, command, 0x03, &out_cmd, sizeof(out_cmd));
}

void process_ep3_menu_challenge(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t flag, const string& data) { // DC
  check_size(data.size(), 0);
  if (flag != 0) {
    send_command(c, 0xDC);
  }
}

void process_ep3_server_data_request(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // CA
  check_size(data.size(), 8, 0xFFFF);
  const PSOSubcommand* cmds = reinterpret_cast<const PSOSubcommand*>(data.data());

  auto l = s->find_lobby(c->lobby_id);
  if (!l || !(l->flags & Lobby::Flag::EPISODE_3_ONLY) || !l->is_game()) {
    c->should_disconnect = true;
    return;
  }

  if (cmds[0].byte[0] != 0xB3) {
    c->should_disconnect = true;
    return;
  }

  switch (cmds[1].byte[0]) {
    // phase 1: map select
    case 0x40:
      send_ep3_map_list(l);
      break;
    case 0x41:
      send_ep3_map_data(l, cmds[4].dword);
      break;
    /*// phase 2: deck/name entry
    case 0x13:
      ti = FindTeam(s, c->teamID);
      memcpy(&ti->ep3game, ((DWORD)c->bufferin + 0x14), 0x2AC);
      CommandEp3InitChangeState(s, c, 1);
      break;
    case 0x1B:
      ti = FindTeam(s, c->teamID);
      memcpy(&ti->ep3names[*(BYTE*)((DWORD)c->bufferin + 0x24)], ((DWORD)c->bufferin + 0x14), 0x14); // NOTICE: may be 0x26 instead of 0x24
      CommandEp3InitSendNames(s, c);
      break;
    case 0x14:
      ti = FindTeam(s, c->teamID);
      memcpy(&ti->ep3decks[*(BYTE*)((DWORD)c->bufferin + 0x14)], ((DWORD)c->bufferin + 0x18), 0x58); // NOTICE: may be 0x16 instead of 0x14
      Ep3FillHand(&ti->ep3game, &ti->ep3decks[*(BYTE*)((DWORD)c->bufferin + 0x14)], &ti->ep3pcs[*(BYTE*)((DWORD)c->bufferin + 0x14)]);
      //Ep3RollDice(&ti->ep3game, &ti->ep3pcs[*(BYTE*)((DWORD)c->bufferin + 0x14)]);
      CommandEp3InitSendDecks(s, c);
      CommandEp3InitSendMapLayout(s, c);
      for (x = 0, param = 0; x < 4; x++) if ((ti->ep3decks[x].clientID != 0xFFFFFFFF) && (ti->ep3names[x].clientID != 0xFF)) param++;
      if (param >= ti->ep3game.numPlayers) CommandEp3InitChangeState(s, c, 3);
      break;
    // phase 3: hands & game states
    case 0x1D:
      ti = FindTeam(s, c->teamID);
      Ep3ReprocessMap(&ti->ep3game);
      CommandEp3SendMapData(s, c, ti->ep3game.mapID);
      for (y = 0, x = 0; x < 4; x++)
      {
          if ((ti->ep3decks[x].clientID == 0xFFFFFFFF) || (ti->ep3names[x].clientID == 0xFF)) continue;
          Ep3EquipCard(&ti->ep3game, &ti->ep3decks[x], &ti->ep3pcs[x], 0); // equip SC card
          CommandEp3InitHandUpdate(s, c, x);
          CommandEp3InitStatUpdate(s, c, x);
          y++;
      }
      CommandEp3Init_B4_06(s, c, (y == 4) ? true : false);
      CommandEp3InitSendMapLayout(s, c);
      for (x = 0; x < 4; x++)
      {
          if ((ti->ep3decks[x].clientID == 0xFFFFFFFF) || (ti->ep3names[x].clientID == 0xFF)) continue;
          CommandEp3Init_B4_4E(s, c, x);
          CommandEp3Init_B4_4C(s, c, x);
          CommandEp3Init_B4_4D(s, c, x);
          CommandEp3Init_B4_4F(s, c, x);
      }
      CommandEp3InitSendDecks(s, c);
      CommandEp3InitSendMapLayout(s, c);
      for (x = 0; x < 4; x++)
      {
          if ((ti->ep3decks[x].clientID == 0xFFFFFFFF) || (ti->ep3names[x].clientID == 0xFF)) continue;
          CommandEp3InitHandUpdate(s, c, x);
      }
      CommandEp3InitSendNames(s, c);
      CommandEp3InitChangeState(s, c, 4);
      CommandEp3Init_B4_50(s, c);
      CommandEp3InitSendMapLayout(s, c);
      CommandEp3Init_B4_39(s, c); // MISSING: 60 00 AC 00 B4 2A 00 00 39 56 00 08 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
      CommandEp3InitBegin(s, c);
      break; */
    default:
      log(WARNING, "Unknown Episode III server data request: %02X", cmds[1].byte[0]);
  }
}

void process_ep3_tournament_control(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string&) { // E2
  // The client will get stuck here unless we send something. An 01 (lobby
  // message box) seems to get them unstuck.
  send_lobby_message_box(c, u"$C6Tournaments are\nnot supported.");

  // In case we ever implement this (doubtful), the flag values are:
  // 00 - list tournaments
  // 01 - check tournament entry status
  // 02 - cancel tournament entry
  // 03 - create tournament spectator team (presumably get battle list, like get team list)
  // 04 - join tournament spectator team (presumably also get battle list)
}



////////////////////////////////////////////////////////////////////////////////
// menu commands

void process_message_box_closed(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // D6
  check_size(data.size(), 0);
  if (c->flags & Client::Flag::IN_INFORMATION_MENU) {
    send_menu(c, u"Information", INFORMATION_MENU_ID, *s->information_menu, false);
  } else if (c->flags & Client::Flag::AT_WELCOME_MESSAGE) {
    send_menu(c, s->name.c_str(), MAIN_MENU_ID, s->main_menu, false);
    c->flags &= ~Client::Flag::AT_WELCOME_MESSAGE;
    send_update_client_config(c);
  }
}

void process_menu_item_info_request(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 09
  const auto& cmd = check_size_t<C_MenuItemInfoRequest_09>(data);

  switch (cmd.menu_id) {
    case MAIN_MENU_ID:
      switch (cmd.item_id) {
        case MAIN_MENU_GO_TO_LOBBY:
          send_ship_info(c, u"Go to the lobby.");
          break;
        case MAIN_MENU_INFORMATION:
          send_ship_info(c, u"View server\ninformation.");
          break;
        case MAIN_MENU_PROXY_DESTINATIONS:
          send_ship_info(c, u"Connect to another\nserver.");
          break;
        case MAIN_MENU_DOWNLOAD_QUESTS:
          send_ship_info(c, u"Download a quest.");
          break;
        case MAIN_MENU_DISCONNECT:
          send_ship_info(c, u"End your session.");
          break;
        default:
          send_ship_info(c, u"Incorrect menu item ID.");
          break;
      }
      break;

    case INFORMATION_MENU_ID:
      if (cmd.item_id == INFORMATION_MENU_GO_BACK) {
        send_ship_info(c, u"Return to the\nmain menu.");
      } else {
        try {
          // we use item_id + 1 here because "go back" is the first item
          send_ship_info(c, s->information_menu->at(cmd.item_id + 1).description.c_str());
        } catch (const out_of_range&) {
          send_ship_info(c, u"$C6No such information exists.");
        }
      }
      break;

    case PROXY_DESTINATIONS_MENU_ID:
      if (cmd.item_id == PROXY_DESTINATIONS_MENU_GO_BACK) {
        send_ship_info(c, u"Return to the\nmain menu.");
      } else {
        try {
          const auto& menu = s->proxy_destinations_menu_for_version(c->version);
          // we use item_id + 1 here because "go back" is the first item
          send_ship_info(c, menu.at(cmd.item_id + 1).description.c_str());
        } catch (const out_of_range&) {
          send_ship_info(c, u"$C6No such information exists.");
        }
      }
      break;

    case QUEST_MENU_ID: {
      if (!s->quest_index) {
        send_quest_info(c, u"$C6Quests are not available.");
        break;
      }
      auto q = s->quest_index->get(c->version, cmd.item_id);
      if (!q) {
        send_quest_info(c, u"$C6Quest does not exist.");
        break;
      }
      send_quest_info(c, q->long_description.c_str());
      break;
    }

    default:
      send_ship_info(c, u"Incorrect menu ID.");
      break;
  }
}

void process_menu_selection(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 10
  bool uses_unicode = ((c->version == GameVersion::PC) || (c->version == GameVersion::BB));

  const auto& cmd = check_size_t<C_MenuSelection>(data,
      sizeof(C_MenuSelection), sizeof(C_MenuSelection) + 0x10 * (1 + uses_unicode));

  switch (cmd.menu_id) {
    case MAIN_MENU_ID: {
      switch (cmd.item_id) {
        case MAIN_MENU_GO_TO_LOBBY: {
          static const vector<string> version_to_port_name({
              "dc-lobby", "pc-lobby", "bb-lobby", "gc-lobby", "bb-lobby"});
          const auto& port_name = version_to_port_name.at(static_cast<size_t>(c->version));

          send_reconnect(c, s->connect_address_for_client(c),
              s->name_to_port_config.at(port_name)->port);
          break;
        }

        case MAIN_MENU_INFORMATION:
          send_menu(c, u"Information", INFORMATION_MENU_ID,
              *s->information_menu, false);
          c->flags |= Client::Flag::IN_INFORMATION_MENU;
          break;

        case MAIN_MENU_PROXY_DESTINATIONS:
          send_menu(c, u"Proxy server", PROXY_DESTINATIONS_MENU_ID,
              s->proxy_destinations_menu_for_version(c->version), false);
          break;

        case MAIN_MENU_DOWNLOAD_QUESTS:
          send_quest_menu(c, QUEST_FILTER_MENU_ID, quest_download_menu, true);
          break;

        case MAIN_MENU_DISCONNECT:
          c->should_disconnect = true;
          break;

        default:
          send_message_box(c, u"Incorrect menu item ID.");
          break;
      }
      break;
    }

    case INFORMATION_MENU_ID: {
      if (cmd.item_id == INFORMATION_MENU_GO_BACK) {
        c->flags &= ~Client::Flag::IN_INFORMATION_MENU;
        send_menu(c, s->name.c_str(), MAIN_MENU_ID, s->main_menu, false);

      } else {
        try {
          send_message_box(c, s->information_contents->at(cmd.item_id).c_str());
        } catch (const out_of_range&) {
          send_message_box(c, u"$C6No such information exists.");
        }
      }
      break;
    }

    case PROXY_DESTINATIONS_MENU_ID: {
      if (cmd.item_id == PROXY_DESTINATIONS_MENU_GO_BACK) {
        send_menu(c, s->name.c_str(), MAIN_MENU_ID, s->main_menu, false);

      } else {
        const pair<string, uint16_t>* dest = nullptr;
        try {
          dest = &s->proxy_destinations_for_version(c->version).at(cmd.item_id);
        } catch (const out_of_range&) { }

        if (!dest) {
          send_message_box(c, u"$C6No such destination exists.");
          c->should_disconnect = true;
        } else {
          // TODO: We can probably avoid using client config and reconnecting the
          // client here; it's likely we could build a way to just directly link
          // the client to the proxy server instead (would have to provide
          // license/char name/etc. for remote auth)

          static const vector<string> version_to_port_name({
              "dc-proxy", "pc-proxy", "", "gc-proxy", "bb-proxy"});
          const auto& port_name = version_to_port_name.at(static_cast<size_t>(c->version));
          uint16_t local_port = s->name_to_port_config.at(port_name)->port;

          c->proxy_destination_address = resolve_ipv4(dest->first);
          c->proxy_destination_port = dest->second;
          send_update_client_config(c);

          s->proxy_server->delete_session(c->license->serial_number);
          s->proxy_server->create_licensed_session(
              c->license, local_port, c->version, c->export_config());

          send_reconnect(c, s->connect_address_for_client(c), local_port);
        }
      }
      break;
    }

    case GAME_MENU_ID: {
      auto game = s->find_lobby(cmd.item_id);
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
      if ((game->version != c->version) ||
          (!(game->flags & Lobby::Flag::EPISODE_3_ONLY) != !(c->flags & Client::Flag::EPISODE_3))) {
        send_lobby_message_box(c, u"$C6You cannot join this\ngame because it is\nfor a different\nversion of PSO.");
        break;
      }
      if (game->flags & Lobby::Flag::QUEST_IN_PROGRESS) {
        send_lobby_message_box(c, u"$C6You cannot join this\ngame because a\nquest is already\nin progress.");
        break;
      }
      if (game->any_client_loading()) {
        send_lobby_message_box(c, u"$C6You cannot join this\ngame because\nanother player is\ncurrently loading.\nTry again soon.");
        break;
      }
      if (game->mode == 3) {
        send_lobby_message_box(c, u"$C6You cannot join this\n game because it is\na Solo Mode game.");
        break;
      }

      if (!(c->license->privileges & Privilege::FREE_JOIN_GAMES)) {
        ptext<char16_t, 0x10> password;
        if (data.size() > sizeof(C_MenuSelection)) {
          if (uses_unicode) {
            size_t max_chars = (data.size() - sizeof(C_MenuSelection)) / sizeof(char16_t);
            password.assign(cmd.password.pcbb, max_chars);
          } else {
            size_t max_chars = (data.size() - sizeof(C_MenuSelection)) / sizeof(char);
            password = decode_sjis(cmd.password.dcgc, max_chars);
          }
        }

        if (!game->password.empty() && (password != game->password)) {
          send_message_box(c, u"$C6Incorrect password.");
          break;
        }
        if (c->player.disp.level < game->min_level) {
          send_message_box(c, u"$C6Your level is too\nlow to join this\ngame.");
          break;
        }
        if (c->player.disp.level > game->max_level) {
          send_message_box(c, u"$C6Your level is too\nhigh to join this\ngame.");
          break;
        }
      }

      s->change_client_lobby(c, game);
      c->flags |= Client::Flag::LOADING;
      if (c->version == GameVersion::BB) {
        game->assign_item_ids_for_player(c->lobby_client_id, c->player.inventory);
      }
      break;
    }

    case QUEST_FILTER_MENU_ID: {
      if (!s->quest_index) {
        send_lobby_message_box(c, u"$C6Quests are not available.");
        break;
      }
      shared_ptr<Lobby> l = c->lobby_id ? s->find_lobby(c->lobby_id) : nullptr;
      auto quests = s->quest_index->filter(c->version,
          c->flags & Client::Flag::DCV1,
          static_cast<QuestCategory>(cmd.item_id & 0xFF),
          l.get() ? (l->episode - 1) : -1);
      if (quests.empty()) {
        send_lobby_message_box(c, u"$C6There are no quests\navailable in that\ncategory.");
        break;
      }

      // Hack: assume the menu to be sent is the download quest menu if the
      // client is not in any lobby
      send_quest_menu(c, QUEST_MENU_ID, quests, !c->lobby_id);
      break;
    }

    case QUEST_MENU_ID: {
      if (!s->quest_index) {
        send_lobby_message_box(c, u"$C6Quests are not available.");
        break;
      }
      auto q = s->quest_index->get(c->version, cmd.item_id);
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
          send_lobby_message_box(c, u"$C6Quests cannot be loaded\nin lobbies.");
          break;
        }
      }

      auto bin_basename = q->bin_filename();
      auto dat_basename = q->dat_filename();
      auto bin_contents = q->bin_contents();
      auto dat_contents = q->dat_contents();

      if (l) {
        if (q->joinable) {
          l->flags |= Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS;
        } else {
          l->flags |= Lobby::Flag::QUEST_IN_PROGRESS;
        }
        l->loading_quest_id = q->quest_id;
        for (size_t x = 0; x < l->max_clients; x++) {
          if (!l->clients[x]) {
            continue;
          }

          // TODO: It looks like blasting all the chunks to the client at once can
          // cause GC clients to crash in rare cases. Find a way to slow this down
          // (perhaps by only sending each new chunk when they acknowledge the
          // previous chunk with a 44 [first chunk] or 13 [later chunks] command).
          send_quest_file(l->clients[x], bin_basename, *bin_contents, false, false);
          send_quest_file(l->clients[x], dat_basename, *dat_contents, false, false);

          l->clients[x]->flags |= Client::Flag::LOADING;
        }

      } else {
        // TODO: cache dlq somewhere maybe
        auto dlq = q->create_download_quest();
        send_quest_file(c, bin_basename, *bin_contents, true, false);
        send_quest_file(c, dat_basename, *dat_contents, true, false);
      }
      break;
    }

    case LOBBY_MENU_ID:
      // TODO;
      break;

    default:
      send_message_box(c, u"Incorrect menu ID.");
      break;
  }
}

void process_change_lobby(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 84
  const auto& cmd = check_size_t<C_LobbySelection_84>(data);

  shared_ptr<Lobby> new_lobby;
  try {
    new_lobby = s->find_lobby(cmd.item_id);
  } catch (const out_of_range&) {
    send_lobby_message_box(c, u"$C6Can't change lobby\n\n$C7The lobby does not\nexist.");
    return;
  }

  if ((new_lobby->flags & Lobby::Flag::EPISODE_3_ONLY) && !(c->flags & Client::Flag::EPISODE_3)) {
    send_lobby_message_box(c, u"$C6Can't change lobby\n\n$C7The lobby is for\nEpisode 3 only.");
    return;
  }

  s->change_client_lobby(c, new_lobby);
}

void process_game_list_request(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 08
  check_size(data.size(), 0);
  send_game_menu(c, s);
}

void process_change_ship(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string&) { // A0
  // The client actually sends data in this command... looks like nothing
  // important (player_tag and guild_card_number are the only discernable
  // things, which we already know). We intentionally don't call check_size
  // here, but instead just ignore the data.

  send_message_box(c, u""); // we do this to avoid the "log window in message box" bug

  static const vector<string> version_to_port_name({
      "dc-login", "pc-login", "bb-patch", "gc-us3", "bb-login"});
  const auto& port_name = version_to_port_name.at(static_cast<size_t>(c->version));

  send_reconnect(c, s->connect_address_for_client(c),
      s->name_to_port_config.at(port_name)->port);
}

void process_change_block(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t command, uint32_t flag, const string& data) { // A1
  // newserv doesn't have blocks; treat block change as ship change
  process_change_ship(s, c, command, flag, data);
}

////////////////////////////////////////////////////////////////////////////////
// Quest commands

void process_quest_list_request(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t flag, const string& data) { // A2
  check_size(data.size(), 0);

  if (!s->quest_index) {
    send_lobby_message_box(c, u"$C6Quests are not available.");
    return;
  }

  auto l = s->find_lobby(c->lobby_id);
  if (!l || !l->is_game()) {
    send_lobby_message_box(c, u"$C6Quests are not available\nin lobbies.");
    return;
  }

  vector<MenuItem>* menu = nullptr;
  if ((c->version == GameVersion::BB) && flag) {
    menu = &quest_government_menu;
  } else {
    if (l->mode == 0) {
      menu = &quest_categories_menu;
    } else if (l->mode == 1) {
      menu = &quest_battle_menu;
    } else if (l->mode == 1) {
      menu = &quest_challenge_menu;
    } else if (l->mode == 1) {
      menu = &quest_solo_menu;
    } else {
      throw logic_error("no quest menu available for mode");
    }
  }

  send_quest_menu(c, QUEST_FILTER_MENU_ID, *menu, false);
}

void process_quest_ready(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // AC
  check_size(data.size(), 0);

  auto l = s->find_lobby(c->lobby_id);
  if (!l || !l->is_game()) {
    return;
  }

  c->flags &= ~Client::Flag::LOADING;

  // check if any client is still loading
  // TODO: we need to handle clients disconnecting while loading. probably
  // process_client_disconnect needs to check for this case or something
  size_t x;
  for (x = 0; x < l->max_clients; x++) {
    if (!l->clients[x]) {
      continue;
    }
    if (l->clients[x]->flags & Client::Flag::LOADING) {
      break;
    }
  }

  // if they're all done, start the quest
  if (x == l->max_clients) {
    send_command(l, 0xAC);
  }
}

void process_gba_file_request(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // D7
  static FileContentsCache file_cache;

  string filename(data);
  strip_trailing_zeroes(filename);
  auto contents = file_cache.get(filename);

  send_quest_file(c, filename, *contents, false, false);
}



////////////////////////////////////////////////////////////////////////////////
// player data commands

void process_player_data(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t command, uint32_t flag, const string& data) { // 61 98

  // Note: we add extra buffer on the end when checking sizes because the
  // autoreply text is a variable length
  switch (c->version) {
    case GameVersion::PC:
      check_size(data.size(), sizeof(PSOPlayerDataPC),
          sizeof(PSOPlayerDataPC) + sizeof(char16_t) * c->player.auto_reply.size());
      c->player.import(*reinterpret_cast<const PSOPlayerDataPC*>(data.data()));
      break;
    case GameVersion::GC:
      if (flag == 4) { // Episode 3
        check_size(data.size(), sizeof(PSOPlayerDataGC) + 0x23FC);
        // TODO: import Episode 3 data somewhere
      } else {
        check_size(data.size(), sizeof(PSOPlayerDataGC),
            sizeof(PSOPlayerDataGC) + sizeof(char) * c->player.auto_reply.size());
      }
      c->player.import(*reinterpret_cast<const PSOPlayerDataGC*>(data.data()));
      break;
    case GameVersion::BB:
      check_size(data.size(), sizeof(PSOPlayerDataBB),
          sizeof(PSOPlayerDataBB) + sizeof(char16_t) * c->player.auto_reply.size());
      c->player.import(*reinterpret_cast<const PSOPlayerDataBB*>(data.data()));
      break;
    default:
      throw logic_error("player data command not implemented for version");
  }

  if (command == 0x61 && !c->pending_bb_save_username.empty()) {
    bool failure = false;
    try {
      string filename = filename_for_player_bb(c->pending_bb_save_username,
          c->pending_bb_save_player_index + 1);
      c->player.save_player_data(filename);
    } catch (const exception& e) {
      u16string buffer = u"$C6PSOBB player data could\nnot be saved:\n" + decode_sjis(e.what());
      send_text_message(c, buffer.c_str());
      failure = true;
    }

    try {
      string filename = string_printf("system/players/player_%s_player%d.nsb",
          c->pending_bb_save_username.c_str(), c->pending_bb_save_player_index + 1);
      c->player.bank.save(filename);
    } catch (const exception& e) {
      u16string buffer = u"$C6PSOBB bank data could\nnot be saved:\n" + decode_sjis(e.what());
      send_text_message(c, buffer.c_str());
      failure = true;
    }

    if (!failure) {
      send_text_message_printf(c,
          "$C6PSOBB player data saved\nas player %hhu for user\n%s",
          static_cast<uint8_t>(c->pending_bb_save_player_index + 1),
          c->pending_bb_save_username.c_str());
    }

    c->pending_bb_save_username.clear();
  }

  // if the client isn't in a lobby, add them to an available lobby
  if (!c->lobby_id && (c->server_behavior == ServerBehavior::LOBBY_SERVER)) {
    s->add_client_to_available_lobby(c);
  }
}

////////////////////////////////////////////////////////////////////////////////
// subcommands

void process_game_command(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t command, uint32_t flag, const string& data) { // 60 62 6C 6D C9 CB (C9 CB are ep3 only)
  check_size(data.size(), 4, 0xFFFF);
  const PSOSubcommand* sub = reinterpret_cast<const PSOSubcommand*>(data.data());

  auto l = s->find_lobby(c->lobby_id);
  if (!l) {
    return;
  }

  process_subcommand(s, l, c, command, flag, sub,
      data.size() / sizeof(PSOSubcommand));
}

////////////////////////////////////////////////////////////////////////////////
// chat commands

void process_chat_generic(shared_ptr<ServerState> s, shared_ptr<Client> c,
    const u16string& text) { // 06

  if (!c->can_chat) {
    return;
  }

  u16string processed_text = remove_language_marker(text);
  if (processed_text.empty()) {
    return;
  }

  if (processed_text[0] == L'$') {
    auto l = s->find_lobby(c->lobby_id);
    if (l) {
      process_chat_command(s, l, c, &processed_text[1]);
    }
  } else {
    if (!c->can_chat) {
      return;
    }

    auto l = s->find_lobby(c->lobby_id);
    if (!l) {
      return;
    }

    for (size_t x = 0; x < l->max_clients; x++) {
      if (!l->clients[x]) {
        continue;
      }
      send_chat_message(l->clients[x], c->license->serial_number,
          c->player.disp.name.data(), processed_text.c_str());
    }
  }
}

void process_chat_pc_bb(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 06
  const auto& cmd = check_size_t<C_Chat_06>(data, sizeof(C_Chat_06), 0xFFFF);
  u16string text(cmd.text.pcbb, (data.size() - sizeof(C_Chat_06)) / sizeof(char16_t));
  strip_trailing_zeroes(text);
  process_chat_generic(s, c, text);
}

void process_chat_dc_gc(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) {
  const auto& cmd = check_size_t<C_Chat_06>(data, sizeof(C_Chat_06), 0xFFFF);
  u16string decoded_s = decode_sjis(cmd.text.dcgc, data.size() - sizeof(C_Chat_06));
  process_chat_generic(s, c, decoded_s);
}

////////////////////////////////////////////////////////////////////////////////
// BB commands

void process_key_config_request_bb(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) {
  check_size(data.size(), 0);
  send_team_and_key_config_bb(c);
}

void process_player_preview_request_bb(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) {
  const auto& cmd = check_size_t<C_PlayerPreviewRequest_BB_E3>(data);

  if (c->bb_game_state == ClientStateBB::CHOOSE_PLAYER) {
    c->bb_player_index = cmd.player_index;
    c->bb_game_state++;
    send_client_init_bb(c, 0);
    send_approve_player_choice_bb(c);
  } else {
    if (!c->license) {
      c->should_disconnect = true;
      return;
    }
    string filename = filename_for_player_bb(c->license->username,
        c->bb_player_index);

    try {
      // generate a preview
      Player p;
      p.load_player_data(filename);
      auto preview = p.disp.to_preview();
      send_player_preview_bb(c, cmd.player_index, &preview);

    } catch (const exception&) {
      // player doesn't exist
      send_player_preview_bb(c, cmd.player_index, nullptr);
    }
  }
}

void process_client_checksum_bb(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t command, uint32_t, const string& data) {
  check_size(data.size(), 0);

  if (command == 0x01E8) {
    send_accept_client_checksum_bb(c);
  } else if (command == 0x03E8) {
    send_guild_card_header_bb(c);
  } else {
    throw invalid_argument("unimplemented command");
  }
}

void process_guild_card_data_request_bb(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) {
  const auto& cmd = check_size_t<C_GuildCardDataRequest_BB_DC>(data);
  if (cmd.cont) {
    send_guild_card_chunk_bb(c, cmd.chunk_index);
  }
}

void process_stream_file_request_bb(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t command, uint32_t, const string& data) {
  check_size(data.size(), 0);

  if (command == 0x04EB) {
    send_stream_file_bb(c);
  } else {
    throw invalid_argument("unimplemented command");
  }
}

void process_create_character_bb(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) {
  const auto& cmd = check_size_t<C_CreateCharacter_BB_E5>(data);

  if (!c->license) {
    send_message_box(c, u"$C6You are not logged in.");
    return;
  }
  if (!c->player.disp.name.empty()) {
    send_message_box(c, u"$C6You have already loaded a character.");
    return;
  }

  c->bb_player_index = cmd.player_index;
  c->player.bank_name = string_printf("player%" PRIu32, cmd.player_index + 1);
  string player_filename = filename_for_player_bb(c->license->username, cmd.player_index);
  string bank_filename = filename_for_bank_bb(c->license->username, c->player.bank_name);
  string template_filename = filename_for_class_template_bb(cmd.preview.char_class);

  Player p;
  try {
    p.load_player_data(template_filename);
  } catch (const exception& e) {
    send_message_box(c, u"$C6New character could not be created.\n\nA server file is missing.");
    return;
  }

  try {
    p.disp.apply_preview(cmd.preview);
    c->player.disp.stats = s->level_table->base_stats_for_class(c->player.disp.char_class);
  } catch (const exception& e) {
    send_message_box(c, u"$C6New character could not be created.\n\nTemplate application failed.");
    return;
  }

  try {
    p.save_player_data(player_filename);
  } catch (const exception& e) {
    send_message_box(c, u"$C6New character could not be created.\n\nThe disk is full or write-protected.");
    return;
  }

  try {
    p.bank.save(bank_filename);
  } catch (const exception& e) {
    unlink(player_filename);
    send_message_box(c, u"$C6New bank could not be created.\n\nThe disk is full or write-protected.");
    return;
  }

  send_client_init_bb(c, 0);
  send_approve_player_choice_bb(c);
}

void process_change_account_data_bb(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t command, uint32_t, const string& data) {
  const auto* cmd = reinterpret_cast<const C_UpdateAccountData_BB_ED*>(data.data());

  switch (command) {
    case 0x01ED:
      check_size(data.size(), sizeof(cmd->option));
      c->player.option_flags = cmd->option;
      break;
    case 0x02ED:
      check_size(data.size(), sizeof(cmd->symbol_chats));
      c->player.symbol_chats = cmd->symbol_chats;
      break;
    case 0x03ED:
      check_size(data.size(), sizeof(cmd->chat_shortcuts));
      c->player.shortcuts = cmd->chat_shortcuts;
      break;
    case 0x04ED:
      check_size(data.size(), sizeof(cmd->key_config));
      c->player.key_config.key_config = cmd->key_config;
      break;
    case 0x05ED:
      check_size(data.size(), sizeof(cmd->pad_config));
      c->player.key_config.joystick_config = cmd->pad_config;
      break;
    case 0x06ED:
      check_size(data.size(), sizeof(cmd->tech_menu));
      c->player.tech_menu_config = cmd->tech_menu;
      break;
    case 0x07ED:
      check_size(data.size(), sizeof(cmd->customize));
      c->player.disp.config = cmd->customize;
      break;
    default:
      throw invalid_argument("unknown account command");
  }
}

void process_return_player_data_bb(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) {
  const auto& cmd = check_size_t<PlayerBB>(data);

  // We only trust the player's quest data and challenge data.
  c->player.challenge_data = cmd.challenge_data;
  c->player.quest_data1 = cmd.quest_data1;
  c->player.quest_data2 = cmd.quest_data2;
}

////////////////////////////////////////////////////////////////////////////////
// Lobby commands

void process_change_arrow_color(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t flag, const string& data) { // 89
  check_size(data.size(), 0);

  c->lobby_arrow_color = flag;
  auto l = s->find_lobby(c->lobby_id);
  if (l) {
    send_arrow_update(l);
  }
}

void process_card_search(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 40
  const auto& cmd = check_size_t<C_GuildCardSearch_40>(data);
  try {
    auto result = s->find_client(nullptr, cmd.target_guild_card_number);
    auto result_lobby = s->find_lobby(result->lobby_id);
    send_card_search_result(s, c, result, result_lobby);
  } catch (const out_of_range&) { }
}

void process_choice_search(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string&) { // C0
  // TODO: Implement choice search.
  send_text_message(c, u"$C6Choice Search is\nnot supported");
}

void process_simple_mail(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 81
  if (c->version != GameVersion::GC) {
    // TODO: implement this for DC, PC, BB
    send_text_message(c, u"$C6Simple Mail is not\nsupported yet on\nthis platform.");
    return;
  }

  const auto& cmd = check_size_t<SC_SimpleMail_GC_81>(data);

  auto target = s->find_client(nullptr, cmd.to_guild_card_number);

  // If the sender is blocked, don't forward the mail
  for (size_t y = 0; y < 30; y++) {
    if (target->player.blocked_senders.data()[y] == c->license->serial_number) {
      return;
    }
  }

  // If the target has auto-reply enabled, send the autoreply
  if (!target->player.auto_reply.empty()) {
    send_simple_mail(c, target->license->serial_number,
        target->player.disp.name.c_str(), target->player.auto_reply.c_str());
  }

  // Forward the message
  u16string u16message = decode_sjis(cmd.text);
  send_simple_mail(target, c->license->serial_number,
      c->player.disp.name.c_str(), u16message.c_str());
}



////////////////////////////////////////////////////////////////////////////////
// Info board commands

void process_info_board_request(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // D8
  check_size(data.size(), 0);
  send_info_board(c, s->find_lobby(c->lobby_id));
}

template <typename CharT>
void process_write_info_board_t(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // D9
  check_size(data.size(), 0, c->player.info_board.size() * sizeof(CharT));
  c->player.info_board.assign(
      reinterpret_cast<const CharT*>(data.data()),
      data.size() / sizeof(CharT));
}

template <typename CharT>
void process_set_auto_reply_t(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // C7
  check_size(data.size(), 0, c->player.auto_reply.size() * sizeof(CharT));
  c->player.auto_reply.assign(
      reinterpret_cast<const CharT*>(data.data()),
      data.size() / sizeof(CharT));
}

void process_disable_auto_reply(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // C8
  check_size(data.size(), 0);
  c->player.auto_reply.clear();
}

void process_set_blocked_senders_list(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // C6
  const auto& cmd = check_size_t<C_SetBlockedSenders_C6>(data);
  c->player.blocked_senders = cmd.blocked_senders;
}



////////////////////////////////////////////////////////////////////////////////
// Game commands

shared_ptr<Lobby> create_game_generic(shared_ptr<ServerState> s,
    shared_ptr<Client> c, const char16_t* name, const char16_t* password,
    uint8_t episode, uint8_t difficulty, uint8_t battle, uint8_t challenge,
    uint8_t solo) {

  static const uint32_t variation_maxes_online[3][0x20] = {
      {1, 1, 1, 5, 1, 5, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2,
       3, 2, 3, 2, 3, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
      {1, 1, 2, 1, 2, 1, 2, 1, 2, 1, 1, 3, 1, 3, 1, 3,
       2, 2, 1, 3, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1},
      {1, 1, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 3, 1, 1, 3,
       3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}};

  static const uint32_t variation_maxes_solo[3][0x20] = {
      {1, 1, 1, 3, 1, 3, 3, 1, 3, 1, 3, 1, 3, 2, 3, 2,
       3, 2, 3, 2, 3, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
      {1, 1, 2, 1, 2, 1, 2, 1, 2, 1, 1, 3, 1, 3, 1, 3,
       2, 2, 1, 3, 2, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1},
      {1, 1, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 3, 1, 1, 3,
       3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}};

  static const uint32_t default_minimum_levels[3][4] = {
      {0, 19, 39, 79}, // episode 1
      {0, 29, 49, 89}, // episode 2
      {0, 39, 79, 109}}; // episode 4

  if (episode == 0) {
    episode = 0xFF;
  }
  if (((episode != 0xFF) && (episode > 3)) || (episode == 0)) {
    throw invalid_argument("incorrect episode number");
  }
  bool is_ep3 = (episode == 0xFF);

  if (difficulty > 3) {
    throw invalid_argument("incorrect difficulty level");
  }

  auto current_lobby = s->find_lobby(c->lobby_id);
  if (!current_lobby) {
    throw invalid_argument("cannot make a game from outside any lobby");
  }

  uint8_t min_level = ((episode == 0xFF) ? 0 : default_minimum_levels[episode - 1][difficulty]);
  if (min_level > c->player.disp.level) {
    throw invalid_argument("level too low for difficulty");
  }

  shared_ptr<Lobby> game(new Lobby());
  game->name = name;
  game->password = password;
  game->version = c->version;
  game->section_id = c->override_section_id >= 0
      ? c->override_section_id : c->player.disp.section_id;
  game->episode = episode;
  game->difficulty = difficulty;
  if (battle) {
    game->mode = 1;
  }
  if (challenge) {
    game->mode = 2;
  }
  if (solo) {
    game->mode = 3;
  }
  game->event = Lobby::game_event_for_lobby_event(current_lobby->event);
  game->block = 0xFF;
  game->max_clients = 4;
  game->flags = (is_ep3 ? Lobby::Flag::EPISODE_3_ONLY : 0) | Lobby::Flag::GAME;
  game->min_level = min_level;
  game->max_level = 0xFFFFFFFF;

  const uint32_t* variation_maxes = nullptr;
  if (game->version == GameVersion::BB) {
    // TODO: cache these somewhere so we don't read the file every time, lolz
    game->rare_item_set.reset(new RareItemSet("system/blueburst/ItemRT.rel",
        game->episode - 1, game->difficulty, game->section_id));

    for (size_t x = 0; x < 4; x++) {
      game->next_item_id[x] = (0x00200000 * x) + 0x00010000;
    }
    game->next_game_item_id = 0x00810000;
    game->enemies.resize(0x0B50);

    const auto* bp_subtable = s->battle_params->get_subtable(game->mode == 3,
        game->episode - 1, game->difficulty);

    if (game->mode == 3) {
      if (episode > 0 && episode < 4) {
        variation_maxes = variation_maxes_solo[episode - 1];
      }
      for (size_t x = 0; x < 0x10; x++) {
        for (const char* type_char = "sm"; *type_char; type_char++) {
          try {
            auto filename = string_printf(
                "system/blueburst/map/%c%hhu%zu%" PRIu32 "%" PRIu32 ".dat",
                *type_char, game->episode, x,
                game->variations.data()[x * 2].load(),
                game->variations.data()[(x * 2) + 1].load());
            game->enemies = load_map(filename.c_str(), game->episode,
                game->difficulty, bp_subtable, false);
            break;
          } catch (const exception& e) { }
        }
        if (game->enemies.empty()) {
          throw runtime_error("failed to load any map data");
        }
      }
    } else {
      if (episode > 0 && episode < 4) {
        variation_maxes = variation_maxes_online[episode - 1];
      }
      for (size_t x = 0; x < 0x10; x++) {
        auto filename = string_printf(
            "system/blueburst/map/m%hhu%zu%" PRIu32 "%" PRIu32 ".dat",
            game->episode, x,
            game->variations.data()[x * 2].load(),
            game->variations.data()[(x * 2) + 1].load());
        game->enemies = load_map(filename.c_str(), game->episode,
            game->difficulty, bp_subtable, false);
      }
    }

  } else {
    // In non-BB games, just set the variations (we don't track items/enemies/
    // etc.)
    if (episode > 0 && episode < 4 && !is_ep3) {
      variation_maxes = variation_maxes_online[episode - 1];
    }
  }

  if (variation_maxes) {
    for (size_t x = 0; x < 0x20; x++) {
      game->variations.data()[x] = random_int(0, variation_maxes[x] - 1);
    }
  } else {
    for (size_t x = 0; x < 0x20; x++) {
      game->variations.data()[x] = 0;
    }
  }

  return game;
}

void process_create_game_pc(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // C1
  const auto& cmd = check_size_t<C_CreateGame_PC_C1>(data);

  auto game = create_game_generic(s, c, cmd.name.c_str(), cmd.password.c_str(), 1,
      cmd.difficulty, cmd.battle_mode, cmd.challenge_mode, 0);

  s->add_lobby(game);
  s->change_client_lobby(c, game);
  c->flags |= Client::Flag::LOADING;
}

void process_create_game_dc_gc(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t command, uint32_t, const string& data) { // C1 EC (EC Ep3 only)
  const auto& cmd = check_size_t<C_CreateGame_DC_GC_C1_EC>(data);

  // only allow EC from Ep3 clients
  bool client_is_ep3 = c->flags & Client::Flag::EPISODE_3;
  if ((command == 0xEC) && !client_is_ep3) {
    return;
  }

  uint8_t episode = cmd.episode;
  if (c->version == GameVersion::DC) {
    episode = 1;
  }
  if (client_is_ep3) {
    episode = 0xFF;
  }

  u16string name = decode_sjis(cmd.name);
  u16string password = decode_sjis(cmd.password);

  auto game = create_game_generic(s, c, name.c_str(), password.c_str(),
      episode, cmd.difficulty, cmd.battle_mode, cmd.challenge_mode, 0);

  s->add_lobby(game);
  s->change_client_lobby(c, game);
  c->flags |= Client::Flag::LOADING;
}

void process_create_game_bb(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // C1
  const auto& cmd = check_size_t<C_CreateGame_BB_C1>(data);

  auto game = create_game_generic(s, c, cmd.name.c_str(), cmd.password.c_str(),
      cmd.episode, cmd.difficulty, cmd.battle_mode, cmd.challenge_mode,
      cmd.solo_mode);

  s->add_lobby(game);
  s->change_client_lobby(c, game);
  c->flags |= Client::Flag::LOADING;

  game->assign_item_ids_for_player(c->lobby_client_id, c->player.inventory);
}

void process_lobby_name_request(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 8A
  check_size(data.size(), 0);
  auto l = s->find_lobby(c->lobby_id);
  if (!l) {
    throw invalid_argument("client not in any lobby");
  }
  send_lobby_name(c, l->name.c_str());
}

void process_client_ready(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 6F
  check_size(data.size(), 0);

  auto l = s->find_lobby(c->lobby_id);
  if (!l || !l->is_game()) {
    // go home client; you're drunk
    throw invalid_argument("ready command cannot be sent outside game");
  }
  c->flags &= (~Client::Flag::LOADING);

  send_resume_game(l, c);
  send_server_time(c);
  send_get_player_info(c);
}

////////////////////////////////////////////////////////////////////////////////
// Team commands

void process_team_command_bb(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t command, uint32_t, const string&) { // EA

  if (command == 0x01EA) {
    send_lobby_message_box(c, u"$C6Teams are not supported.");
  } else {
    throw invalid_argument("unimplemented team command");
  }
}

////////////////////////////////////////////////////////////////////////////////
// Patch server commands

void process_encryption_ok_patch(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) {
  check_size(data.size(), 0);
  send_command(c, 0x04); // this requests the user's login information
}

void process_login_patch(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) {
  const auto& cmd = check_size_t<C_Login_Patch_04>(data,
      offsetof(C_Login_Patch_04, email), sizeof(C_Login_Patch_04));

  if (data.size() == offsetof(C_Login_Patch_04, email)) {
    c->flags |= Client::Flag::BB_PATCH;
  } else if (data.size() != sizeof(C_Login_Patch_04)) {
    throw runtime_error("unknown patch server login format");
  }

  // On BB we can use colors and newlines should be \n; on PC we can't use
  // colors, the text is auto-word-wrapped, and newlines should be \r\n.
  u16string message;
  if (c->flags & Client::Flag::BB_PATCH) {
    message = u"\
$C7newserv patch server\n\
\n\
This server is not affiliated with, sponsored by, or in any\n\
other way connected to SEGA or Sonic Team, and is owned\n\
and operated completely independently.\n\
\n";
  } else {
    message = u"\
newserv patch server\r\n\
\r\n\
This server is not affiliated with, sponsored by, or in any other way \
connected to SEGA or Sonic Team, and is owned and operated completely \
independently.\r\n\
\r\n";
  }
  message += u"License check ";
  try {
    if (c->flags & Client::Flag::BB_PATCH) {
      c->license = s->license_manager->verify_bb(
          cmd.username.c_str(), cmd.password.c_str());
    } else {
      uint32_t serial_number = strtoul(cmd.username.c_str(), nullptr, 16);
      c->license = s->license_manager->verify_pc(
          serial_number, cmd.password.c_str(), nullptr);
    }
    message += u"OK";
  } catch (const exception& e) {
    message += u"failed: ";
    message += decode_sjis(e.what());
  }

  send_message_box(c, message.c_str());
  send_check_directory_patch(c, ".");
  send_check_directory_patch(c, "data");
  send_check_directory_patch(c, "scene");
  send_command(c, 0x0A);
  send_command(c, 0x0A);
  send_command(c, 0x0A);

  // This command terminates the patch connection successfully. PSOBB complains
  // if we don't check the above directories before sending this though
  send_command(c, 0x0012, 0x00000000);
}

////////////////////////////////////////////////////////////////////////////////
// Command pointer arrays

void process_ignored_command(shared_ptr<ServerState>, shared_ptr<Client>,
    uint16_t, uint32_t, const string&) { }

void process_unimplemented_command(shared_ptr<ServerState>, shared_ptr<Client>,
    uint16_t command, uint32_t flag, const string& data) {
  log(WARNING, "Unknown command: size=%04zX command=%04hX flag=%08" PRIX32 "\n",
      data.size(), command, flag);
  throw invalid_argument("unimplemented command");
}



typedef void (*process_command_t)(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t command, uint32_t flag, const string& data);

// The entries in these arrays correspond to the ID of the command received. For
// instance, if a command 6C is received, the function at position 0x6C in the
// array corresponding to the client's version is called.
static process_command_t dc_handlers[0x100] = {
  // 00
  nullptr, nullptr, nullptr, nullptr,
  nullptr, process_ignored_command, process_chat_dc_gc, nullptr,
  process_game_list_request, process_menu_item_info_request, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr,

  // 10
  process_menu_selection, nullptr, nullptr, process_ignored_command,
  nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr,
  nullptr, process_ignored_command, nullptr, nullptr,

  // 20
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

  // 30
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

  // 40
  process_card_search, nullptr, nullptr, nullptr,
  process_ignored_command, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

  // 50
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

  // 60
  process_game_command, nullptr, process_game_command, nullptr,
  nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr,
  process_game_command, process_game_command, nullptr, process_client_ready,

  // 70
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

  // 80
  nullptr, process_simple_mail, nullptr, nullptr,
  process_change_lobby, nullptr, nullptr, nullptr,
  nullptr, process_change_arrow_color, process_lobby_name_request, nullptr,
  nullptr, nullptr, nullptr, nullptr,

  // 90
  nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, process_client_checksum, nullptr,
  process_player_data, process_ignored_command, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr,

  // A0
  process_change_ship, process_change_block, process_quest_list_request, nullptr,
  nullptr, nullptr, nullptr, nullptr,
  nullptr, process_ignored_command, nullptr, nullptr,
  process_quest_ready, nullptr, nullptr, nullptr,

  // B0
  nullptr, process_server_time_request, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

  // C0
  nullptr, process_create_game_dc_gc, nullptr, nullptr,
  nullptr, nullptr, process_set_blocked_senders_list, process_set_auto_reply_t<char>,
  process_disable_auto_reply, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr,

  // D0
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  process_info_board_request, process_write_info_board_t<char>, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr,

  // E0
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

  // F0
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
};

static process_command_t pc_handlers[0x100] = {
  // 00
  nullptr, nullptr, nullptr, nullptr,
  nullptr, process_ignored_command, process_chat_pc_bb, nullptr,
  process_game_list_request, process_menu_item_info_request, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr,

  // 10
  process_menu_selection, nullptr, nullptr, process_ignored_command,
  nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr,
  nullptr, process_ignored_command, nullptr, nullptr,

  // 20
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

  // 30
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

  // 40
  process_card_search, nullptr, nullptr, nullptr,
  process_ignored_command, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

  // 50
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

  // 60
  process_game_command, process_player_data, process_game_command, nullptr,
  nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr,
  process_game_command, process_game_command, nullptr, process_client_ready,

  // 70
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

  // 80
  nullptr, process_simple_mail, nullptr, nullptr,
  process_change_lobby, nullptr, nullptr, nullptr,
  nullptr, process_change_arrow_color, process_lobby_name_request, nullptr,
  nullptr, nullptr, nullptr, nullptr,

  // 90
  nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, process_client_checksum, nullptr,
  process_player_data, process_ignored_command, process_login_a_dc_pc_gc, nullptr,
  process_login_c_dc_pc_gc, process_login_d_e_pc_gc, process_login_d_e_pc_gc, nullptr,

  // A0
  process_change_ship, process_change_block, process_quest_list_request, nullptr,
  nullptr, nullptr, nullptr, nullptr,
  nullptr, process_ignored_command, nullptr, nullptr,
  process_quest_ready, nullptr, nullptr, nullptr,

  // B0
  nullptr, process_server_time_request, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

  // C0
  nullptr, process_create_game_pc, nullptr, nullptr,
  nullptr, nullptr, process_set_blocked_senders_list, process_set_auto_reply_t<char16_t>,
  process_disable_auto_reply, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr,

  // D0
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  process_info_board_request, process_write_info_board_t<char16_t>, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr,

  // E0
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

  // F0
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
};

static process_command_t gc_handlers[0x100] = {
  // 00
  nullptr, nullptr, nullptr, nullptr,
  nullptr, process_ignored_command, process_chat_dc_gc, nullptr,
  process_game_list_request, process_menu_item_info_request, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr,

  // 10
  process_menu_selection, nullptr, nullptr, process_ignored_command,
  nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr,
  nullptr, process_ignored_command, nullptr, nullptr,

  // 20
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

  // 30
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

  // 40
  process_card_search, nullptr, nullptr, nullptr,
  process_ignored_command, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr,

  // 50
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

  // 60
  process_game_command, process_player_data, process_game_command, nullptr,
  nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr,
  process_game_command, process_game_command, nullptr, process_client_ready,

  // 70
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

  // 80
  nullptr, process_simple_mail, nullptr, nullptr,
  process_change_lobby, nullptr, nullptr, nullptr,
  nullptr, process_change_arrow_color, process_lobby_name_request, nullptr,
  nullptr, nullptr, nullptr, nullptr,

  // 90
  nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, process_client_checksum, nullptr,
  process_player_data, process_ignored_command, nullptr, nullptr,
  process_login_c_dc_pc_gc, process_login_d_e_pc_gc, process_login_d_e_pc_gc, nullptr,

  // A0
  process_change_ship, process_change_block, process_quest_list_request, nullptr,
  nullptr, nullptr, process_ignored_command, process_ignored_command,
  nullptr, process_ignored_command, nullptr, nullptr,
  process_quest_ready, nullptr, nullptr, nullptr,

  // B0
  nullptr, process_server_time_request, nullptr, nullptr,
  nullptr, nullptr, nullptr, process_ignored_command,
  process_ignored_command, nullptr, process_ep3_jukebox, nullptr,
  nullptr, nullptr, nullptr, nullptr,

  // C0
  process_choice_search, process_create_game_dc_gc, nullptr, nullptr,
  nullptr, nullptr, process_set_blocked_senders_list, process_set_auto_reply_t<char>,
  process_disable_auto_reply, process_game_command, process_ep3_server_data_request, process_game_command,
  nullptr, nullptr, nullptr, nullptr,

  // D0
  nullptr, nullptr, nullptr, nullptr, // D0 is process trade
  nullptr, nullptr, process_message_box_closed, process_gba_file_request,
  process_info_board_request, process_write_info_board_t<char>, nullptr, process_verify_license_gc,
  process_ep3_menu_challenge, nullptr, nullptr, nullptr,

  // E0
  nullptr, nullptr, process_ep3_tournament_control, nullptr,
  process_ignored_command, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr,
  process_create_game_dc_gc, nullptr, nullptr, nullptr,

  // F0
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
};

static process_command_t bb_handlers[0x100] = {
  // 00
  nullptr, nullptr, nullptr, nullptr,
  nullptr, process_ignored_command, process_chat_pc_bb, nullptr,
  process_game_list_request, process_menu_item_info_request, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr,

  // 10
  process_menu_selection, nullptr, nullptr, process_ignored_command,
  nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr,
  nullptr, process_ignored_command, nullptr, nullptr,

  // 20
  nullptr, nullptr, process_ignored_command, nullptr,
  nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

  // 30
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

  // 40
  process_card_search, nullptr, nullptr, nullptr,
  process_ignored_command, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr,

  // 50
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

  // 60
  process_game_command, process_player_data, process_game_command, nullptr,
  nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr,
  process_game_command, process_game_command, nullptr, process_client_ready,

  // 70
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

  // 80
  nullptr, process_simple_mail, nullptr, nullptr,
  process_change_lobby, nullptr, nullptr, nullptr,
  nullptr, process_change_arrow_color, process_lobby_name_request, nullptr,
  nullptr, nullptr, nullptr, nullptr,

  // 90
  nullptr, nullptr, nullptr, process_login_bb,
  nullptr, nullptr, nullptr, nullptr,
  process_player_data, process_ignored_command, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr,

  // A0
  process_change_ship, process_change_block, process_quest_list_request, nullptr,
  nullptr, nullptr, nullptr, nullptr,
  nullptr, process_ignored_command, nullptr, nullptr,
  process_quest_ready, nullptr, nullptr, nullptr,

  // B0
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

  // C0
  nullptr, process_create_game_bb, nullptr, nullptr,
  nullptr, nullptr, process_set_blocked_senders_list, process_set_auto_reply_t<char16_t>,
  process_disable_auto_reply, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr,

  // D0
  nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr,
  process_info_board_request, process_write_info_board_t<char16_t>, nullptr, nullptr,
  process_guild_card_data_request_bb, nullptr, nullptr, nullptr,

  // E0
  process_key_config_request_bb, nullptr, nullptr, process_player_preview_request_bb,
  nullptr, process_create_character_bb, nullptr, process_return_player_data_bb,
  process_client_checksum_bb, nullptr, process_team_command_bb, process_stream_file_request_bb,
  process_ignored_command, process_change_account_data_bb, nullptr, nullptr,

  // F0
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
};

static process_command_t patch_handlers[0x100] = {
  // 00
  nullptr, nullptr, process_encryption_ok_patch, nullptr,
  process_login_patch, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

  // 10
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  // 20
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  // 30
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  // 40
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  // 50
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  // 60
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  // 70
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  // 80
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  // 90
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  // A0
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  // B0
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  // C0
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  // D0
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  // E0
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  // F0
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
};

static process_command_t* handlers[6] = {
    dc_handlers, pc_handlers, patch_handlers, gc_handlers, bb_handlers};

void process_command(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t command, uint32_t flag, const string& data) {
  string encoded_name = remove_language_marker(encode_sjis(c->player.disp.name));
  print_received_command(command, flag, data.data(), data.size(), c->version,
      encoded_name.c_str());

  auto fn = handlers[static_cast<size_t>(c->version)][command & 0xFF];
  if (fn) {
    fn(s, c, command, flag, data);
  } else {
    process_unimplemented_command(s, c, command, flag, data);
  }
}
