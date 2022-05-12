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



extern FileContentsCache file_cache;



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

    case ServerBehavior::DATA_SERVER_BB:
    case ServerBehavior::PATCH_SERVER:
    case ServerBehavior::LOBBY_SERVER:
      send_server_init(s, c, false);
      break;

    default:
      log(ERROR, "Unimplemented behavior: %" PRId64,
          static_cast<int64_t>(c->server_behavior));
  }
}

void process_login_complete(shared_ptr<ServerState> s, shared_ptr<Client> c) {
  // On the BB data server, this function is called only on the last connection
  // (when we should send ths ship select menu).
  if ((c->server_behavior == ServerBehavior::LOGIN_SERVER) ||
      (c->server_behavior == ServerBehavior::DATA_SERVER_BB)) {
    // On the login server, send the ep3 updates and the main menu or welcome
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

    if (c->version == GameVersion::BB) {
      // This implicitly loads the client's account and player data
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

  // TODO: Make a timer event for each connected player that saves their data
  // periodically, not only when they disconnect
  // TODO: Track play time somewhere for BB players
  // c->game_data.player()->disp.play_time += ((now() - c->play_time_begin) / 1000000);

  // Note: The client's GameData destructor should save their player data
  // shortly after this point
}



////////////////////////////////////////////////////////////////////////////////

void process_verify_license_gc(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // DB
  const auto& cmd = check_size_t<C_VerifyLicense_GC_DB>(data);

  uint32_t serial_number = stoul(cmd.serial_number, nullptr, 16);
  try {
    auto l = s->license_manager->verify_gc(serial_number, cmd.access_key,
        cmd.password);
    c->set_license(l);
  } catch (const exception& e) {
    if (!s->allow_unregistered_users) {
      u16string message = u"Login failed: " + decode_sjis(e.what());
      send_message_box(c, message.c_str());
      c->should_disconnect = true;
      return;
    } else {
      auto l = LicenseManager::create_license_gc(serial_number, cmd.access_key,
          cmd.password, true);
      s->license_manager->add(l);
      c->set_license(l);
    }
  }

  c->flags |= flags_for_version(c->version, cmd.sub_version);
  send_command(c, 0x9A, 0x02);
}

void process_login_a_dc_pc_gc(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 9A
  const auto& cmd = check_size_t<C_Login_DC_PC_GC_9A>(data);

  c->flags |= flags_for_version(c->version, cmd.sub_version);

  uint32_t serial_number = stoul(cmd.serial_number, nullptr, 16);
  try {
    shared_ptr<const License> l;
    if (c->version == GameVersion::GC) {
      l = s->license_manager->verify_gc(serial_number, cmd.access_key);
    } else {
      l = s->license_manager->verify_pc(serial_number, cmd.access_key);
    }
    c->set_license(l);

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

  uint32_t serial_number = stoul(cmd.serial_number, nullptr, 16);
  try {
    shared_ptr<const License> l;
    if (c->version == GameVersion::GC) {
      l = s->license_manager->verify_gc(serial_number, cmd.access_key,
          cmd.password);
    } else {
      l = s->license_manager->verify_pc(serial_number, cmd.access_key);
    }
    c->set_license(l);

  } catch (const exception& e) {
    if (!s->allow_unregistered_users) {
      u16string message = u"Login failed: " + decode_sjis(e.what());
      send_message_box(c, message.c_str());
      c->should_disconnect = true;
      return;
    } else {
      shared_ptr<License> l;
      if (c->version == GameVersion::GC) {
        l = LicenseManager::create_license_gc(serial_number, cmd.access_key,
            cmd.password, true);
      } else {
        l = LicenseManager::create_license_pc(serial_number, cmd.access_key,
            true);
      }
      s->license_manager->add(l);
      c->set_license(l);
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
      c->bb_game_state = ClientStateBB::INITIAL_LOGIN;
      c->game_data.bb_player_index = 0;
    }

  } else {
    throw logic_error("9D/9E handler called for incorrect command");
  }

  c->flags |= flags_for_version(c->version, base_cmd->sub_version);

  uint32_t serial_number = stoul(base_cmd->serial_number, nullptr, 16);
  try {
    shared_ptr<const License> l;
    if (c->version == GameVersion::GC) {
      l = s->license_manager->verify_gc(serial_number,
          base_cmd->access_key);
    } else {
      l = s->license_manager->verify_pc(serial_number,
          base_cmd->access_key);
    }
    c->set_license(l);

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

  c->flags |= flags_for_version(c->version, 0);

  try {
    auto l = s->license_manager->verify_bb(cmd.username, cmd.password);
    c->set_license(l);
  } catch (const exception& e) {
    u16string message = u"Login failed: " + decode_sjis(e.what());
    send_message_box(c, message.c_str());
    c->should_disconnect = true;
    return;
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
      process_login_complete(s, c);
      break;

    case ClientStateBB::IN_GAME:
      break;

    default:
      throw runtime_error("invalid bb game state");
  }
}

void process_client_checksum(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 96
  check_size_t<C_ClientChecksum_GC_96>(data);
  send_command(c, 0x97, 0x01);
}

void process_server_time_request(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // B1
  check_size_v(data.size(), 0);
  send_server_time(c);
}



////////////////////////////////////////////////////////////////////////////////
// Ep3 commands. Note that these commands are not at all functional. The command
// handlers that partially worked were lost in a dead hard drive, unfortunately.

void process_ep3_jukebox(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t command, uint32_t, const string& data) {
  const auto& in_cmd = check_size_t<C_Meseta_GC_Ep3_BA>(data);

  S_Meseta_GC_Ep3_BA out_cmd = {1000000, 0x80E8, in_cmd.request_token};

  auto l = s->find_lobby(c->lobby_id);
  if (!l || !(l->flags & Lobby::Flag::EPISODE_3_ONLY)) {
    return;
  }

  send_command(c, command, 0x03, &out_cmd, sizeof(out_cmd));
}

void process_ep3_menu_challenge(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t flag, const string& data) { // DC
  check_size_v(data.size(), 0);
  if (flag != 0) {
    send_command(c, 0xDC, 0x00);
  }
}

void process_ep3_server_data_request(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // CA
  check_size_v(data.size(), 8, 0xFFFF);
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
  check_size_v(data.size(), 0);
  if (c->flags & Client::Flag::IN_INFORMATION_MENU) {
    send_menu(c, u"Information", INFORMATION_MENU_ID,
        *s->information_menu_for_version(c->version), false);
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
          send_ship_info(c, s->information_menu_for_version(c->version)->at(cmd.item_id + 1).description.c_str());
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
              *s->information_menu_for_version(c->version), true);
          c->flags |= Client::Flag::IN_INFORMATION_MENU;
          break;

        case MAIN_MENU_PROXY_DESTINATIONS:
          send_menu(c, u"Proxy server", PROXY_DESTINATIONS_MENU_ID,
              s->proxy_destinations_menu_for_version(c->version), false);
          break;

        case MAIN_MENU_DOWNLOAD_QUESTS:
          if (c->flags & Client::Flag::EPISODE_3) {
            shared_ptr<Lobby> l = c->lobby_id ? s->find_lobby(c->lobby_id) : nullptr;
            auto quests = s->quest_index->filter(
                c->version, false, QuestCategory::EPISODE_3);
            if (quests.empty()) {
              send_lobby_message_box(c, u"$C6There are no quests\navailable.");
            } else {
              // Episode 3 has only download quests, not online quests, so this
              // is always the download quest menu. (Episode 3 does actually
              // have online quests, but they don't use the file download
              // paradigm that all other versions use.)
              send_quest_menu(c, QUEST_MENU_ID, quests, true);
            }
          } else {
            send_quest_menu(c, QUEST_FILTER_MENU_ID, quest_download_menu, true);
          }
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
              c->license, local_port, c->version, c->export_config_bb());

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
        if (c->game_data.player()->disp.level < game->min_level) {
          send_message_box(c, u"$C6Your level is too\nlow to join this\ngame.");
          break;
        }
        if (c->game_data.player()->disp.level > game->max_level) {
          send_message_box(c, u"$C6Your level is too\nhigh to join this\ngame.");
          break;
        }
      }

      s->change_client_lobby(c, game);
      c->flags |= Client::Flag::LOADING;
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
          static_cast<QuestCategory>(cmd.item_id & 0xFF));
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

          // TODO: It looks like blasting all the chunks to the client at once can
          // cause GC clients to crash in rare cases. Find a way to slow this down
          // (perhaps by only sending each new chunk when they acknowledge the
          // previous chunk with a 44 [first chunk] or 13 [later chunks] command).
          send_quest_file(l->clients[x], bin_basename + ".bin", bin_basename,
              *bin_contents, QuestFileType::ONLINE);
          send_quest_file(l->clients[x], dat_basename + ".dat", dat_basename,
              *dat_contents, QuestFileType::ONLINE);

          // There is no such thing as command AC on PSO PC - quests just start
          // immediately when they're done downloading. There are also no chunk
          // acknowledgements (C->S 13 commands) like there are on GC. So, for
          // PC clients, we can just not set the loading flag, since we never
          // need to check/clear it later.
          if (l->clients[x]->version != GameVersion::PC) {
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
  check_size_v(data.size(), 0);
  send_game_menu(c, s);
}

void process_information_menu_request_pc(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 1F
  check_size_v(data.size(), 0);
  send_menu(c, u"Information", INFORMATION_MENU_ID,
      *s->information_menu_for_version(c->version), true);
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
  if (c->flags & Client::Flag::EPISODE_3) {
    send_lobby_message_box(c, u"$C6Episode 3 does not\nprovide online quests\nvia this interface.");

  } else {
    vector<MenuItem>* menu = nullptr;
    if ((c->version == GameVersion::BB) && flag) {
      menu = &quest_government_menu;
    } else {
      if (l->mode == 0) {
        menu = &quest_categories_menu;
      } else if (l->mode == 1) {
        menu = &quest_battle_menu;
      } else if (l->mode == 2) {
        menu = &quest_challenge_menu;
      } else if (l->mode == 3) {
        menu = &quest_solo_menu;
      } else {
        throw logic_error("no quest menu available for mode");
      }
    }

    send_quest_menu(c, QUEST_FILTER_MENU_ID, *menu, false);
  }
}

void process_quest_ready(shared_ptr<ServerState> s, shared_ptr<Client> c,
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

  // check if any client is still loading
  // TODO: we need to handle clients disconnecting while loading. probably
  // process_client_disconnect needs to check for this case or something
  size_t x;
  for (x = 0; x < l->max_clients; x++) {
    if (!l->clients[x]) {
      continue;
    }
    if (l->clients[x]->flags & Client::Flag::LOADING_QUEST) {
      break;
    }
  }

  // if they're all done, start the quest
  if (x == l->max_clients) {
    send_command(l, 0xAC, 0x00);
  }
}

void process_update_quest_statistics(shared_ptr<ServerState> s,
    shared_ptr<Client> c, uint16_t, uint32_t, const string& data) { // AA
  const auto& cmd = check_size_t<C_UpdateQuestStatistics_AA>(data);

  auto l = s->find_lobby(c->lobby_id);
  if (!l || !l->is_game() || !l->loading_quest.get() ||
      (l->loading_quest->internal_id != cmd.quest_internal_id)) {
    return;
  }

  S_ConfirmUpdateQuestStatistics_AB response;
  response.unknown_a1 = 0;
  response.request_token = cmd.request_token;
  response.unknown_a2 = 0xBFFF;
  send_command_t(c, 0xAB, 0x00, response);
}

void process_gba_file_request(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // D7
  string filename(data);
  strip_trailing_zeroes(filename);
  auto contents = file_cache.get("system/gba/" + filename);

  send_quest_file(c, "", filename, *contents, QuestFileType::GBA_DEMO);
}



////////////////////////////////////////////////////////////////////////////////
// player data commands

void process_player_data(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t command, uint32_t flag, const string& data) { // 61 98

  // Note: we add extra buffer on the end when checking sizes because the
  // autoreply text is a variable length
  switch (c->version) {
    case GameVersion::PC: {
      const auto& disp = check_size_t<PSOPlayerDataPC>(data,
          sizeof(PSOPlayerDataPC), 0xFFFF);
      c->game_data.import_player(disp);
      break;
    }
    case GameVersion::GC: {
      const PSOPlayerDataGC* disp;
      if (flag == 4) { // Episode 3
        disp = &check_size_t<PSOPlayerDataGC>(data,
            sizeof(PSOPlayerDataGC) + 0x23FC, sizeof(PSOPlayerDataGC) + 0x23FC);
        // TODO: import Episode 3 data somewhere
      } else {
        disp = &check_size_t<PSOPlayerDataGC>(data, sizeof(PSOPlayerDataGC),
            sizeof(PSOPlayerDataGC) + c->game_data.player()->auto_reply.bytes());
      }
      c->game_data.import_player(*disp);
      break;
    }
    case GameVersion::BB: {
      const auto& disp = check_size_t<PSOPlayerDataBB>(data, sizeof(PSOPlayerDataBB),
          sizeof(PSOPlayerDataBB) + c->game_data.player()->auto_reply.bytes());
      c->game_data.import_player(disp);
      break;
    }
    default:
      throw logic_error("player data command not implemented for version");
  }

  if (command == 0x61 && !c->pending_bb_save_username.empty()) {
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

  // if the client isn't in a lobby, add them to an available lobby
  if (!c->lobby_id && (c->server_behavior == ServerBehavior::LOBBY_SERVER)) {
    s->add_client_to_available_lobby(c);
  }
}

////////////////////////////////////////////////////////////////////////////////
// subcommands

void process_game_command(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t command, uint32_t flag, const string& data) { // 60 62 6C 6D C9 CB (C9 CB are ep3 only)
  check_size_v(data.size(), 4, 0xFFFF);

  auto l = s->find_lobby(c->lobby_id);
  if (!l) {
    return;
  }

  process_subcommand(s, l, c, command, flag, data);
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
      process_chat_command(s, l, c, processed_text);
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
          c->game_data.player()->disp.name.data(), processed_text.c_str());
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
  check_size_v(data.size(), 0);
  send_team_and_key_config_bb(c);
}

void process_player_preview_request_bb(shared_ptr<ServerState>, shared_ptr<Client> c,
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
    temp_gd.serial_number = c->license->serial_number;
    temp_gd.bb_username = c->license->username;
    temp_gd.bb_player_index = cmd.player_index;

    try {
      auto preview = temp_gd.player()->disp.to_preview();
      send_player_preview_bb(c, cmd.player_index, &preview);

    } catch (const exception& e) {
      // Player doesn't exist
      log(INFO, "[BB debug] No player in slot: %s", e.what());
      send_player_preview_bb(c, cmd.player_index, nullptr);
    }
  }
}

void process_client_checksum_bb(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t command, uint32_t, const string& data) {
  if (command == 0x01E8) {
    check_size_v(data.size(), 8);
    send_accept_client_checksum_bb(c);
  } else if (command == 0x03E8) {
    check_size_v(data.size(), 0);
    send_guild_card_header_bb(c);
  } else {
    throw invalid_argument("unimplemented command");
  }
}

void process_guild_card_data_request_bb(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) {
  const auto& cmd = check_size_t<C_GuildCardDataRequest_BB_03DC>(data);
  if (cmd.cont) {
    send_guild_card_chunk_bb(c, cmd.chunk_index);
  }
}

void process_stream_file_request_bb(shared_ptr<ServerState>, shared_ptr<Client> c,
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

void process_create_character_bb(shared_ptr<ServerState> s, shared_ptr<Client> c,
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

  try {
    c->game_data.create_player(cmd.preview, s->level_table);
  } catch (const exception& e) {
    string message = string_printf("$C6New character could not be created:\n%s", e.what());
    send_message_box(c, decode_sjis(message));
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

void process_return_player_data_bb(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 00E7
  const auto& cmd = check_size_t<PlayerBB>(data);

  // We only trust the player's quest data and challenge data.
  c->game_data.player()->challenge_data = cmd.challenge_data;
  c->game_data.player()->quest_data1 = cmd.quest_data1;
  c->game_data.player()->quest_data2 = cmd.quest_data2;
}

void process_update_key_config_bb(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) {
  // Some clients have only a uint32_t at the end for team rewards
  auto& cmd = check_size_t<KeyAndTeamConfigBB>(data,
      sizeof(KeyAndTeamConfigBB) - 4, sizeof(KeyAndTeamConfigBB));
  c->game_data.account()->key_config = cmd;
  // TODO: We should send a response here, but I don't know which one!
}

////////////////////////////////////////////////////////////////////////////////
// Lobby commands

void process_change_arrow_color(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t flag, const string& data) { // 89
  check_size_v(data.size(), 0);

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
    if (target->game_data.account()->blocked_senders.data()[y] == c->license->serial_number) {
      return;
    }
  }

  // If the target has auto-reply enabled, send the autoreply
  if (!target->game_data.player()->auto_reply.empty()) {
    send_simple_mail(c, target->license->serial_number,
        target->game_data.player()->disp.name,
        target->game_data.player()->auto_reply);
  }

  // Forward the message
  u16string msg = decode_sjis(cmd.text);
  send_simple_mail(target, c->license->serial_number, c->game_data.player()->disp.name, msg);
}



////////////////////////////////////////////////////////////////////////////////
// Info board commands

void process_info_board_request(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // D8
  check_size_v(data.size(), 0);
  send_info_board(c, s->find_lobby(c->lobby_id));
}

template <typename CharT>
void process_write_info_board_t(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // D9
  check_size_v(data.size(), 0, c->game_data.player()->info_board.size() * sizeof(CharT));
  c->game_data.player()->info_board.assign(
      reinterpret_cast<const CharT*>(data.data()),
      data.size() / sizeof(CharT));
}

template <typename CharT>
void process_set_auto_reply_t(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // C7
  check_size_v(data.size(), 0, c->game_data.player()->auto_reply.size() * sizeof(CharT));
  c->game_data.player()->auto_reply.assign(
      reinterpret_cast<const CharT*>(data.data()),
      data.size() / sizeof(CharT));
}

void process_disable_auto_reply(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // C8
  check_size_v(data.size(), 0);
  c->game_data.player()->auto_reply.clear();
}

void process_set_blocked_senders_list(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // C6
  const auto& cmd = check_size_t<C_SetBlockedSenders_C6>(data);
  c->game_data.account()->blocked_senders = cmd.blocked_senders;
}



////////////////////////////////////////////////////////////////////////////////
// Game commands

shared_ptr<Lobby> create_game_generic(shared_ptr<ServerState> s,
    shared_ptr<Client> c, const std::u16string& name,
    const std::u16string& password, uint8_t episode, uint8_t difficulty,
    uint8_t battle, uint8_t challenge, uint8_t solo) {

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

  // A player's actual level is their displayed level - 1, so the minimums for
  // Episode 1 (for example) are actually 1, 20, 40, 80.
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
  if (!(c->license->privileges & Privilege::FREE_JOIN_GAMES) &&
      (min_level > c->game_data.player()->disp.level)) {
    throw invalid_argument("level too low for difficulty");
  }

  shared_ptr<Lobby> game(new Lobby());
  game->name = name;
  game->password = password;
  game->version = c->version;
  game->section_id = c->override_section_id >= 0
      ? c->override_section_id : c->game_data.player()->disp.section_id;
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

    const char* type_chars = (game->mode == 3) ? "sm" : "m";
    if (episode > 0 && episode < 4) {
      variation_maxes = (game->mode == 3) ? variation_maxes_solo[episode - 1] : variation_maxes_online[episode - 1];
    }

    for (size_t x = 0; x < 0x10; x++) {
      for (const char* type = type_chars; *type; type++) {
        auto filename = string_printf(
            "system/blueburst/map/%c%hhX%zX%" PRIX32 "%" PRIX32 ".dat",
            *type, game->episode, x,
            game->variations.data()[x * 2].load(),
            game->variations.data()[(x * 2) + 1].load());
        try {
          game->enemies = load_map(filename.c_str(), game->episode,
              game->difficulty, bp_subtable, false);
          log(INFO, "Loaded map %s", filename.c_str());
          break;
        } catch (const exception& e) {
          log(WARNING, "Failed to load map %s: %s", filename.c_str(), e.what());
        }
      }
    }

    if (game->enemies.empty()) {
      throw runtime_error("failed to load any map data");
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

  auto game = create_game_generic(s, c, cmd.name, cmd.password, 1,
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

  auto game = create_game_generic(s, c, cmd.name, cmd.password, cmd.episode,
      cmd.difficulty, cmd.battle_mode, cmd.challenge_mode, cmd.solo_mode);

  s->add_lobby(game);
  s->change_client_lobby(c, game);
  c->flags |= Client::Flag::LOADING;
}

void process_lobby_name_request(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 8A
  check_size_v(data.size(), 0);
  auto l = s->find_lobby(c->lobby_id);
  if (!l) {
    throw invalid_argument("client not in any lobby");
  }
  send_lobby_name(c, l->name.c_str());
}

void process_client_ready(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) { // 6F
  check_size_v(data.size(), 0);

  auto l = s->find_lobby(c->lobby_id);
  if (!l || !l->is_game()) {
    // go home client; you're drunk
    throw invalid_argument("ready command cannot be sent outside game");
  }
  c->flags &= (~Client::Flag::LOADING);

  send_resume_game(l, c);
  send_server_time(c);
  // Only get player info again on BB, since on other versions the returned info
  // only includes items that would be saved if the client disconnects
  // unexpectedly (that is, only equipped items are included).
  if (c->version == GameVersion::BB) {
    send_get_player_info(c);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Team commands

void process_team_command_bb(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t command, uint32_t, const string&) { // EA

  if (command == 0x01EA) {
    send_lobby_message_box(c, u"$C6Teams are not supported.");
  } else if (command == 0x14EA) {
    // Do nothing (for now)
  } else {
    throw invalid_argument("unimplemented team command");
  }
}

////////////////////////////////////////////////////////////////////////////////
// Patch server commands

void process_encryption_ok_patch(shared_ptr<ServerState>, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) {
  check_size_v(data.size(), 0);
  send_command(c, 0x04, 0x00); // This requests the user's login information
}

void process_login_patch(shared_ptr<ServerState> s, shared_ptr<Client> c,
    uint16_t, uint32_t, const string& data) {
  const auto& cmd = check_size_t<C_Login_Patch_04>(data);

  if (cmd.email.len() == 0) {
    c->flags |= Client::Flag::BB_PATCH;
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
    shared_ptr<const License> l;
    if (c->flags & Client::Flag::BB_PATCH) {
      l = s->license_manager->verify_bb(cmd.username, cmd.password);
    } else {
      l = s->license_manager->verify_pc(
          stoul(cmd.username, nullptr, 16), cmd.password);
    }
    c->set_license(l);
    message += u"OK";
  } catch (const exception& e) {
    message += u"failed: ";
    message += decode_sjis(e.what());
  }

  send_message_box(c, message.c_str());
  send_enter_directory_patch(c, ".");
  send_enter_directory_patch(c, "data");
  send_enter_directory_patch(c, "scene");
  send_command(c, 0x0A, 0x00);
  send_command(c, 0x0A, 0x00);
  send_command(c, 0x0A, 0x00);

  // This command terminates the patch connection successfully. PSOBB complains
  // if we don't check the above directories before sending this though
  send_command(c, 0x12, 0x00);
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
  nullptr, process_ignored_command, process_update_quest_statistics, nullptr,
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
  nullptr, process_ignored_command, nullptr, process_information_menu_request_pc,

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
  nullptr, process_ignored_command, process_update_quest_statistics, nullptr,
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
  nullptr, process_ignored_command, process_update_quest_statistics, nullptr,
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
  nullptr, process_ignored_command, process_update_quest_statistics, nullptr,
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
  process_key_config_request_bb, nullptr, process_update_key_config_bb, process_player_preview_request_bb,
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
  string encoded_name;
  auto player = c->game_data.player(false);
  if (player) {
    encoded_name = remove_language_marker(encode_sjis(player->disp.name));
  }
  print_received_command(command, flag, data.data(), data.size(), c->version,
      encoded_name.c_str());

  auto fn = handlers[static_cast<size_t>(c->version)][command & 0xFF];
  if (fn) {
    fn(s, c, command, flag, data);
  } else {
    process_unimplemented_command(s, c, command, flag, data);
  }
}
