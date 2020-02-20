#include "ChatCommands.hh"

#include <string.h>

#include <vector>
#include <string>
#include <unordered_map>
#include <phosg/Strings.hh>
#include <phosg/Time.hh>

#include "Server.hh"
#include "Lobby.hh"
#include "Client.hh"
#include "SendCommands.hh"
#include "Text.hh"

using namespace std;


////////////////////////////////////////////////////////////////////////////////

vector<string> section_id_to_name({
  "Viridia", "Greennill", "Skyly", "Bluefull", "Purplenum", "Pinkal", "Redria",
  "Oran", "Yellowboze", "Whitill"});

unordered_map<u16string, uint8_t> name_to_section_id({
  {u"viridia", 0},
  {u"greennill", 1},
  {u"skyly", 2},
  {u"bluefull", 3},
  {u"purplenum", 4},
  {u"pinkal", 5},
  {u"redria", 6},
  {u"oran", 7},
  {u"yellowboze", 8},
  {u"whitill", 9}});

vector<u16string> lobby_event_to_name({
  u"none", u"xmas", u"none", u"val", u"easter", u"hallo", u"sonic",
  u"newyear", u"summer", u"white", u"wedding", u"fall", u"s-spring",
  u"s-summer", u"spring"});

unordered_map<u16string, uint8_t> name_to_lobby_event({
  {u"none",     0},
  {u"xmas",     1},
  {u"val",      3},
  {u"easter",   4},
  {u"hallo",    5},
  {u"sonic",    6},
  {u"newyear",  7},
  {u"summer",   8},
  {u"white",    9},
  {u"wedding",  10},
  {u"fall",     11},
  {u"s-spring", 12},
  {u"s-summer", 13},
  {u"spring",   14},
});

unordered_map<uint8_t, u16string> lobby_type_to_name({
  {0x00, u"normal"},
  {0x0F, u"inormal"},
  {0x10, u"ipc"},
  {0x11, u"iball"},
  {0x67, u"cave2u"},
  {0xD4, u"cave1"},
  {0xE9, u"planet"},
  {0xEA, u"clouds"},
  {0xED, u"cave"},
  {0xEE, u"jungle"},
  {0xEF, u"forest2-2"},
  {0xF0, u"forest2-1"},
  {0xF1, u"windpower"},
  {0xF2, u"overview"},
  {0xF3, u"seaside"},
  {0xF4, u"some?"},
  {0xF5, u"dmorgue"},
  {0xF6, u"caelum"},
  {0xF8, u"digital"},
  {0xF9, u"boss1"},
  {0xFA, u"boss2"},
  {0xFB, u"boss3"},
  {0xFC, u"dragon"},
  {0xFD, u"derolle"},
  {0xFE, u"volopt"},
  {0xFF, u"darkfalz"},
});

unordered_map<u16string, uint8_t> name_to_lobby_type({
  {u"normal",    0x00},
  {u"inormal",   0x0F},
  {u"ipc",       0x10},
  {u"iball",     0x11},
  {u"cave1",     0xD4},
  {u"cave2u",    0x67},
  {u"dragon",    0xFC},
  {u"derolle",   0xFD},
  {u"volopt",    0xFE},
  {u"darkfalz",  0xFF},
  {u"planet",    0xE9},
  {u"clouds",    0xEA},
  {u"cave",      0xED},
  {u"jungle",    0xEE},
  {u"forest2-2", 0xEF},
  {u"forest2-1", 0xF0},
  {u"windpower", 0xF1},
  {u"overview",  0xF2},
  {u"seaside",   0xF3},
  {u"some?",     0xF4},
  {u"dmorgue",   0xF5},
  {u"caelum",    0xF6},
  {u"digital",   0xF8},
  {u"boss1",     0xF9},
  {u"boss2",     0xFA},
  {u"boss3",     0xFB},
  {u"knight",    0xFC},
  {u"sky",       0xFE},
  {u"morgue",    0xFF},
});

vector<u16string> tech_id_to_name({
  u"foie", u"gifoie", u"rafoie",
  u"barta", u"gibarta", u"rabarta",
  u"zonde", u"gizonde", u"razonde",
  u"grants", u"deband", u"jellen", u"zalure", u"shifta",
  u"ryuker", u"resta", u"anti", u"reverser", u"megid"});

unordered_map<u16string, uint8_t> name_to_tech_id({
  {u"foie",     0},
  {u"gifoie",   1},
  {u"rafoie",   2},
  {u"barta",    3},
  {u"gibarta",  4},
  {u"rabarta",  5},
  {u"zonde",    6},
  {u"gizonde",  7},
  {u"razonde",  8},
  {u"grants",   9},
  {u"deband",   10},
  {u"jellen",   11},
  {u"zalure",   12},
  {u"shifta",   13},
  {u"ryuker",   14},
  {u"resta",    15},
  {u"anti",     16},
  {u"reverser", 17},
  {u"megid",    18},
});

vector<u16string> npc_id_to_name({
  u"ninja", u"rico", u"sonic", u"knuckles", u"tails", u"flowen", u"elly"});

unordered_map<u16string, uint8_t> name_to_npc_id({
  {u"ninja", 0},
  {u"rico", 1},
  {u"sonic", 2},
  {u"knuckles", 3},
  {u"tails", 4},
  {u"flowen", 5},
  {u"elly", 6}});



////////////////////////////////////////////////////////////////////////////////
// Checks



class precondition_failed {
public:
  precondition_failed(const char16_t* user_msg) : user_msg(user_msg) { }
  ~precondition_failed() = default;

  const char16_t* what() const {
    return this->user_msg;
  }

private:
  const char16_t* user_msg;
};

static void check_privileges(shared_ptr<Client> c, uint64_t mask) {
  if (!c->license) {
    throw precondition_failed(u"$C6You are not\nlogged in.");
  }
  if ((c->license->privileges & mask) != mask) {
    throw precondition_failed(u"$C6You do not have\npermission to\nrun this command.");
  }
}

static void check_version(shared_ptr<Client> c, GameVersion version) {
  if (c->version != version) {
    throw precondition_failed(u"$C6This command cannot\nbe used for your\nversion of PSO.");
  }
}

static void check_not_version(shared_ptr<Client> c, GameVersion version) {
  if (c->version == version) {
    throw precondition_failed(u"$C6This command cannot\nbe used for your\nversion of PSO.");
  }
}

static void check_is_game(shared_ptr<Lobby> l, bool is_game) {
  if (l->is_game() != is_game) {
    throw precondition_failed(is_game ?
        u"$C6This command cannot\nbe used in lobbies." :
        u"$C6This command cannot\nbe used in games.");
  }
}

static void check_cheats_enabled(shared_ptr<Lobby> l) {
  if (!(l->flags & LobbyFlag::CheatsEnabled)) {
    throw precondition_failed(u"$C6This command can\nonly be used in\ncheat mode.");
  }
}

static void check_is_leader(shared_ptr<Lobby> l, shared_ptr<Client> c) {
  if (l->leader_id != c->lobby_client_id) {
    throw precondition_failed(u"$C6This command can\nonly be used by\nthe game leader.");
  }
}



////////////////////////////////////////////////////////////////////////////////
// Message commands

static void command_lobby_info(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  // no preconditions - everyone can use this command

  if (!l) {
    send_text_message(c, u"$C6No lobby information");

  } else if (l->is_game()) {
    string level_string;
    if (l->max_level == 0xFFFFFFFF) {
      level_string = string_printf("Levels: %d+", l->min_level + 1);
    } else {
      level_string = string_printf("Levels: %d-%d", l->min_level + 1, l->max_level + 1);
    }

    send_text_message_printf(c, "$C6Game ID: %08X\n%s\nSection ID: %s\nCheat mode: %s",
        l->lobby_id, level_string.c_str(),
        section_id_to_name.at(l->section_id).c_str(),
        (l->flags & LobbyFlag::CheatsEnabled) ? "on" : "off");

  } else {
    size_t num_clients = l->count_clients();
    size_t max_clients = l->max_clients;
    send_text_message_printf(c, "$C6Lobby ID: %08X\nPlayers: %zu/%zu",
        l->lobby_id, num_clients, max_clients);
  }
}

static void command_ax(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  check_privileges(c, Privilege::Announce);
  log(INFO, "[$ax from %010u] %S\n", c->license->serial_number, args);
}

static void command_announce(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  check_privileges(c, Privilege::Announce);
  send_text_message(s, args);
}

static void command_arrow(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  // no preconditions

  c->lobby_arrow_color = stoull(encode_sjis(args), NULL, 0);
  if (!l->is_game()) {
    send_arrow_update(l);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Lobby commands

static void command_cheat(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  check_is_game(l, true);
  check_is_leader(l, c);

  l->flags ^= LobbyFlag::CheatsEnabled;
  send_text_message_printf(l, "Cheat mode %s",
      (l->flags & LobbyFlag::CheatsEnabled) ? "enabled" : "disabled");

  // if cheat mode was disabled, turn off all the cheat features that were on
  if (!(l->flags & LobbyFlag::CheatsEnabled)) {
    for (size_t x = 0; x < l->max_clients; x++) {
      auto c = l->clients[x];
      if (!c) {
        continue;
      }
      c->infinite_hp = false;
      c->infinite_tp = false;
    }
    memset(&l->next_drop_item, 0, sizeof(l->next_drop_item));
  }
}

static void command_lobby_event(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  check_is_game(l, false);
  check_privileges(c, Privilege::ChangeEvent);

  uint8_t new_event;
  try {
    new_event = name_to_lobby_event.at(args);
  } catch (const out_of_range& e) {
    send_text_message(c, u"$C6No such lobby event.");
    return;
  }

  l->event = new_event;
  send_command(l, 0xDA, l->event, NULL, 0);
}

static void command_lobby_event_all(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  check_privileges(c, Privilege::ChangeEvent);

  uint8_t new_event;
  try {
    new_event = name_to_lobby_event.at(args);
  } catch (const out_of_range& e) {
    send_text_message(c, u"$C6No such lobby event.");
    return;
  }

  for (auto l : s->all_lobbies()) {
    if (l->is_game() || !(l->flags & LobbyFlag::Default)) {
      continue;
    }

    l->event = new_event;
    send_command(l, 0xDA, new_event, NULL, 0);
  }
}

static void command_lobby_type(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  check_is_game(l, false);
  check_privileges(c, Privilege::ChangeEvent);

  uint8_t new_type;
  try {
    new_type = name_to_lobby_type.at(args);
  } catch (const out_of_range& e) {
    send_text_message(c, u"$C6No such lobby type.");
    return;
  }

  l->type = new_type;
  if (l->type < ((l->flags & LobbyFlag::Episode3) ? 20 : 15)) {
    l->type = l->block - 1;
  }

  for (size_t x = 0; x < l->max_clients; x++) {
    if (l->clients[x]) {
      send_join_lobby(l->clients[x], l);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Game commands

static void command_password(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  check_is_game(l, true);
  check_is_leader(l, c);

  if (!args[0]) {
    l->password[0] = 0;
    send_text_message(l, u"$C6Game unlocked");

  } else {
    char16cpy(l->password, args, 0x10);
    auto encoded = encode_sjis(l->password);
    send_text_message_printf(l, "$C6Game password:\n%s",
        encoded.c_str());
  }
}

static void command_min_level(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  check_is_game(l, true);
  check_is_leader(l, c);

  u16string buffer;
  l->min_level = stoull(encode_sjis(args)) - 1;
  send_text_message_printf(l, "$C6Minimum level set to %" PRIu32,
      l->min_level + 1);
}

static void command_max_level(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  check_is_game(l, true);
  check_is_leader(l, c);

  l->max_level = stoull(encode_sjis(args)) - 1;
  if (l->max_level >= 200) {
    l->max_level = 0xFFFFFFFF;
  }

  if (l->max_level == 0xFFFFFFFF) {
    send_text_message(l, u"$C6Maximum level set to unlimited");
  } else {
    send_text_message_printf(l, "$C6Maximum level set to %" PRIu32, l->max_level + 1);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Character commands

static void command_edit(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  check_is_game(l, false);
  check_version(c, GameVersion::BB);

  string encoded_args = encode_sjis(args);
  vector<string> tokens = split(encoded_args, L' ');

  if (tokens.size() < 3) {
    send_text_message(c, u"$C6Not enough arguments");
    return;
  }

  if (tokens[0] == "atp") {
    c->player.disp.stats.atp = stoul(tokens[1]);
  } else if (tokens[0] == "mst") {
    c->player.disp.stats.mst = stoul(tokens[1]);
  } else if (tokens[0] == "evp") {
    c->player.disp.stats.evp = stoul(tokens[1]);
  } else if (tokens[0] == "hp") {
    c->player.disp.stats.hp = stoul(tokens[1]);
  } else if (tokens[0] == "dfp") {
    c->player.disp.stats.dfp = stoul(tokens[1]);
  } else if (tokens[0] == "ata") {
    c->player.disp.stats.ata = stoul(tokens[1]);
  } else if (tokens[0] == "lck") {
    c->player.disp.stats.lck = stoul(tokens[1]);
  } else if (tokens[0] == "meseta") {
    c->player.disp.meseta = stoul(tokens[1]);
  } else if (tokens[0] == "exp") {
    c->player.disp.experience = stoul(tokens[1]);
  } else if (tokens[0] == "level") {
    c->player.disp.level = stoul(tokens[1]) - 1;
  } else if (tokens[0] == "namecolor") {
    sscanf(tokens[1].c_str(), "%8X", &c->player.disp.name_color);
  } else if (tokens[0] == "secid") {
    try {
      c->player.disp.section_id = name_to_section_id.at(decode_sjis(tokens[1]));
    } catch (const out_of_range&) {
      send_text_message(c, u"$C6No such section ID.");
      return;
    }
  } else if (tokens[0] == "name") {
    decode_sjis(c->player.disp.name, tokens[1].c_str(), 0x10);
    add_language_marker_inplace(c->player.disp.name, u'J', 0x10);
  } else if (tokens[0] == "npc") {
    if (tokens[1] == "none") {
      c->player.disp.extra_model = 0;
      c->player.disp.v2_flags &= 0xFD;
    } else {
      try {
        c->player.disp.extra_model = name_to_npc_id.at(decode_sjis(tokens[1]));
      } catch (const out_of_range&) {
        send_text_message(c, u"$C6No such NPC.");
        return;
      }
      c->player.disp.v2_flags |= 0x02;
    }
  } else if ((tokens[0] == "tech") && (tokens.size() > 2)) {
    uint8_t level = stoul(tokens[2]) - 1;
    if (tokens[1] == "all") {
      for (size_t x = 0; x < 0x14; x++) {
        c->player.disp.technique_levels[x] = level;
      }
    } else {
      try {
        uint8_t tech_id = name_to_tech_id.at(decode_sjis(tokens[1]));
        c->player.disp.technique_levels[tech_id] = level;
      } catch (const out_of_range&) {
        send_text_message(c, u"$C6No such technique.");
        return;
      }
    }
  } else {
    send_text_message(c, u"$C6Unknown field.");
    return;
  }

  // reload the client in the lobby/game
  send_player_leave_notification(l, c->lobby_client_id);
  send_complete_player_bb(c);
  s->send_lobby_join_notifications(l, c);
}

static void command_change_bank(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  check_version(c, GameVersion::BB);

  // TODO: implement this
  // TODO: make sure the bank name is filesystem-safe
}

static void command_convert_char_to_bb(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  check_is_game(l, false);
  check_not_version(c, GameVersion::BB);

  vector<string> tokens = split(encode_sjis(args), L' ');
  if (tokens.size() != 3) {
    send_text_message(c, u"$C6Incorrect argument count");
    return;
  }

  // username/password are tokens[0] and [1]
  c->pending_bb_save_player_index = stoul(tokens[2]) - 1;
  if (c->pending_bb_save_player_index > 3) {
    send_text_message(c, u"$C6Player index must be 1-4");
    return;
  }

  try {
    s->license_manager->verify_bb(tokens[0].c_str(), tokens[1].c_str());
  } catch (const exception& e) {
    send_text_message_printf(c, "$C6Login failed: %s", e.what());
    return;
  }

  c->pending_bb_save_username = tokens[0];

  // request the player data. the client will respond with a 61, and the handler
  // for that command will execute the conversion
  send_command(c, 0x95, 0x00);
}

////////////////////////////////////////////////////////////////////////////////
// Administration commands

static void command_silence(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  check_privileges(c, Privilege::SilenceUser);

  auto target = s->find_client(args);
  if (!target->license) {
    // this should be impossible, but I'll bet it's not actually
    send_text_message(c, u"$C6Client not logged in");
    return;
  }

  if (target->license->privileges & Privilege::Moderator) {
    send_text_message(c, u"$C6You do not have\nsufficient privileges.");
    return;
  }

  target->can_chat = !target->can_chat;
  send_text_message_printf(l, "$C6%s %ssilenced", target->player.disp.name,
      target->can_chat ? "un" : "");
}

static void command_kick(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  check_privileges(c, Privilege::KickUser);

  auto target = s->find_client(args);
  if (!target->license) {
    // this should be impossible, but I'll bet it's not actually
    send_text_message(c, u"$C6Client not logged in");
    return;
  }

  if (target->license->privileges & Privilege::Moderator) {
    send_text_message(c, u"$C6You do not have\nsufficient privileges.");
    return;
  }

  send_message_box(target, u"$C6You were kicked off by a moderator.");
  target->should_disconnect = true;
  send_text_message_printf(l, "$C6%s kicked off", target->player.disp.name);
}

static void command_ban(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  check_privileges(c, Privilege::BanUser);

  u16string args_str(args);
  size_t space_pos = args_str.find(L' ');
  if (space_pos == string::npos) {
    send_text_message(c, u"$C6Incorrect argument count");
    return;
  }

  auto target = s->find_client(args_str.data() + space_pos + 1);
  if (!target->license) {
    // this should be impossible, but I'll bet it's not actually
    send_text_message(c, u"$C6Client not logged in");
    return;
  }

  if (target->license->privileges & Privilege::BanUser) {
    send_text_message(c, u"$C6You do not have\nsufficient privileges.");
    return;
  }

  uint64_t usecs = stoull(encode_sjis(args), NULL, 0) * 1000000;

  size_t unit_offset = 0;
  for (; isdigit(args[unit_offset]); unit_offset++);
  if (args[unit_offset] == 'm') {
    usecs *= 60;
  } else if (args[unit_offset] == 'h') {
    usecs *= 60 * 60;
  } else if (args[unit_offset] == 'd') {
    usecs *= 60 * 60 * 24;
  } else if (args[unit_offset] == 'w') {
    usecs *= 60 * 60 * 24 * 7;
  } else if (args[unit_offset] == 'M') {
    usecs *= 60 * 60 * 24 * 30;
  } else if (args[unit_offset] == 'y') {
    usecs *= 60 * 60 * 24 * 365;
  }

  // TODO: put the length of time in this message. or don't; presumably the
  // person deserved it
  s->license_manager->ban_until(target->license->serial_number, now() + usecs);
  send_message_box(target, u"$C6You were banned by a moderator.");
  target->should_disconnect = true;
  auto encoded_name = encode_sjis(target->player.disp.name);
  send_text_message_printf(l, "$C6%s banned", encoded_name.c_str());
}

////////////////////////////////////////////////////////////////////////////////
// Cheat commands

static void command_warp(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  check_is_game(l, true);
  check_cheats_enabled(l);

  uint32_t area = stoul(encode_sjis(args), NULL, 0);
  if (!l->episode || (l->episode > 3)) {
    return;
  }
  if (c->area == area) {
    return;
  }

  if ((l->episode == 1) && (area > 17)) {
    send_text_message(c, u"$C6Area numbers must be\n17 or less.");
    return;
  }
  if ((l->episode == 2) && (area > 17)) {
    send_text_message(c, u"$C6Area numbers must be\n17 or less.");
    return;
  }
  if ((l->episode == 3) && (area > 10)) {
    send_text_message(c, u"$C6Area numbers must be\n10 or less.");
    return;
  }

  send_warp(c, area);
}

static void command_infinite_hp(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  check_is_game(l, true);
  check_cheats_enabled(l);

  c->infinite_hp = !c->infinite_hp;
  send_text_message_printf(c, "$C6Infinite HP %s", c->infinite_hp ? "enabled" : "disabled");
}

static void command_infinite_tp(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  check_is_game(l, true);
  check_cheats_enabled(l);

  c->infinite_tp = !c->infinite_tp;
  send_text_message_printf(c, "$C6Infinite TP %s", c->infinite_tp ? "enabled" : "disabled");
}

static void command_item(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  check_is_game(l, true);
  check_cheats_enabled(l);

  string data = parse_data_string(encode_sjis(args));
  if (data.size() < 2) {
    send_text_message(c, u"$C6Item codes must be\n2 bytes or more.");
    return;
  }
  if (data.size() > 16) {
    send_text_message(c, u"$C6Item codes must be\n16 bytes or fewer.");
    return;
  }

  ItemData item_data;
  memset(&item_data, 0, sizeof(item_data));
  if (data.size() < 12) {
    memcpy(&l->next_drop_item.data.item_data1, data.data(), data.size());
  } else {
    memcpy(&l->next_drop_item.data.item_data1, data.data(), 12);
    memcpy(&l->next_drop_item.data.item_data2, data.data() + 12, 12 - data.size());
  }

  send_text_message(c, u"$C6Next drop chosen.");
}



////////////////////////////////////////////////////////////////////////////////

typedef void (*handler_t)(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args);
struct ChatCommandDefinition {
  handler_t handler;
  const char16_t* usage;
};

static const unordered_map<u16string, ChatCommandDefinition> chat_commands({
    {u"allevent"  , {command_lobby_event_all   , u"usage:\nallevent <name/ID>"}},
    {u"ann"       , {command_announce          , u"usage:\nann <message>"}},
    {u"arrow"     , {command_arrow             , u"usage:\narrow <color>"}},
    {u"ax"        , {command_ax                , u"usage:\nax <message>"}},
    {u"ban"       , {command_ban               , u"usage:\nban <name-or-number>"}},
    {u"bbchar"    , {command_convert_char_to_bb, u"usage:\nbbchar <user> <pass> <1-4>"}},
    {u"changebank", {command_change_bank       , u"usage:\nchangebank <bank name>"}},
    {u"cheat"     , {command_cheat             , u"usage:\nduh"}},
    {u"edit"      , {command_edit              , u"usage:\nedit <stat> <value>"}},
    {u"event"     , {command_lobby_event       , u"usage:\nevent <name>"}},
    {u"infhp"     , {command_infinite_hp       , u"usage:\nduh"}},
    {u"inftp"     , {command_infinite_tp       , u"usage:\nduh"}},
    {u"item"      , {command_item              , u"usage:\nitem <item-code>"}},
    {u"kick"      , {command_kick              , u"usage:\nkick <name-or-number>"}},
    {u"li"        , {command_lobby_info        , u"usage:\nli"}},
    {u"password"  , {command_password          , u"usage:\nlock [password]\nomit password to\nunlock game"}},
    {u"maxlevel"  , {command_max_level         , u"usage:\nmax_level <level>"}},
    {u"minlevel"  , {command_min_level         , u"usage:\nmin_level <level>"}},
    {u"silence"   , {command_silence           , u"usage:\nsilence <name-or-number>"}},
    {u"type"      , {command_lobby_type        , u"usage:\ntype <name>"}},
    {u"warp"      , {command_warp              , u"usage:\nwarp <area-number>"}},
});

// this function is called every time any player sends a chat beginning with a dollar sign.
// It is this function's responsibility to see if the chat is a command, and to
// execute the command and block the chat if it is.
void process_chat_command(std::shared_ptr<ServerState> s, std::shared_ptr<Lobby> l,
    std::shared_ptr<Client> c, const char16_t* text) {

  // remove the chat command marker
  if (text[0] == u'$') {
    text++;
  }

  u16string command_name;
  u16string text_str(text);
  size_t space_pos = text_str.find(u' ');
  if (space_pos != string::npos) {
    command_name = text_str.substr(0, space_pos);
    text_str = text_str.substr(space_pos + 1);
  } else {
    command_name = text_str;
    text_str.clear();
  }

  const ChatCommandDefinition* def = NULL;
  try {
    def = &chat_commands.at(command_name);
  } catch (const out_of_range&) {
    send_text_message(c, u"$C6Unknown command.");
    return;
  }

  try {
    def->handler(s, l, c, text_str.c_str());
  } catch (const precondition_failed& e) {
    send_text_message(c, e.what());
  } catch (const exception& e) {
    send_text_message_printf(c, "$C6Failed:\n%s", e.what());
  }
}
