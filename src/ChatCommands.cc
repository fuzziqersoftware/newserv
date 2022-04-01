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

const vector<string> section_id_to_name({
  "Viridia", "Greennill", "Skyly", "Bluefull", "Purplenum", "Pinkal", "Redria",
  "Oran", "Yellowboze", "Whitill"});

const unordered_map<string, uint8_t> name_to_section_id({
  {"viridia", 0},
  {"greennill", 1},
  {"skyly", 2},
  {"bluefull", 3},
  {"purplenum", 4},
  {"pinkal", 5},
  {"redria", 6},
  {"oran", 7},
  {"yellowboze", 8},
  {"whitill", 9}});

const vector<string> lobby_event_to_name({
  "none", "xmas", "none", "val", "easter", "hallo", "sonic", "newyear",
  "summer", "white", "wedding", "fall", "s-spring", "s-summer", "spring"});

const unordered_map<string, uint8_t> name_to_lobby_event({
  {"none",     0},
  {"xmas",     1},
  {"val",      3},
  {"easter",   4},
  {"hallo",    5},
  {"sonic",    6},
  {"newyear",  7},
  {"summer",   8},
  {"white",    9},
  {"wedding",  10},
  {"fall",     11},
  {"s-spring", 12},
  {"s-summer", 13},
  {"spring",   14},
});

const unordered_map<uint8_t, string> lobby_type_to_name({
  {0x00, "normal"},
  {0x0F, "inormal"},
  {0x10, "ipc"},
  {0x11, "iball"},
  {0x67, "cave2u"},
  {0xD4, "cave1"},
  {0xE9, "planet"},
  {0xEA, "clouds"},
  {0xED, "cave"},
  {0xEE, "jungle"},
  {0xEF, "forest2-2"},
  {0xF0, "forest2-1"},
  {0xF1, "windpower"},
  {0xF2, "overview"},
  {0xF3, "seaside"},
  {0xF4, "some?"},
  {0xF5, "dmorgue"},
  {0xF6, "caelum"},
  {0xF8, "digital"},
  {0xF9, "boss1"},
  {0xFA, "boss2"},
  {0xFB, "boss3"},
  {0xFC, "dragon"},
  {0xFD, "derolle"},
  {0xFE, "volopt"},
  {0xFF, "darkfalz"},
});

const unordered_map<string, uint8_t> name_to_lobby_type({
  {"normal",    0x00},
  {"inormal",   0x0F},
  {"ipc",       0x10},
  {"iball",     0x11},
  {"cave1",     0xD4},
  {"cave2u",    0x67},
  {"dragon",    0xFC},
  {"derolle",   0xFD},
  {"volopt",    0xFE},
  {"darkfalz",  0xFF},
  {"planet",    0xE9},
  {"clouds",    0xEA},
  {"cave",      0xED},
  {"jungle",    0xEE},
  {"forest2-2", 0xEF},
  {"forest2-1", 0xF0},
  {"windpower", 0xF1},
  {"overview",  0xF2},
  {"seaside",   0xF3},
  {"some?",     0xF4},
  {"dmorgue",   0xF5},
  {"caelum",    0xF6},
  {"digital",   0xF8},
  {"boss1",     0xF9},
  {"boss2",     0xFA},
  {"boss3",     0xFB},
  {"knight",    0xFC},
  {"sky",       0xFE},
  {"morgue",    0xFF},
});

const vector<string> tech_id_to_name({
  "foie", "gifoie", "rafoie",
  "barta", "gibarta", "rabarta",
  "zonde", "gizonde", "razonde",
  "grants", "deband", "jellen", "zalure", "shifta",
  "ryuker", "resta", "anti", "reverser", "megid"});

const unordered_map<string, uint8_t> name_to_tech_id({
  {"foie",     0},
  {"gifoie",   1},
  {"rafoie",   2},
  {"barta",    3},
  {"gibarta",  4},
  {"rabarta",  5},
  {"zonde",    6},
  {"gizonde",  7},
  {"razonde",  8},
  {"grants",   9},
  {"deband",   10},
  {"jellen",   11},
  {"zalure",   12},
  {"shifta",   13},
  {"ryuker",   14},
  {"resta",    15},
  {"anti",     16},
  {"reverser", 17},
  {"megid",    18},
});

const vector<string> npc_id_to_name({
  "ninja", "rico", "sonic", "knuckles", "tails", "flowen", "elly"});

const unordered_map<string, uint8_t> name_to_npc_id({
  {"ninja", 0},
  {"rico", 1},
  {"sonic", 2},
  {"knuckles", 3},
  {"tails", 4},
  {"flowen", 5},
  {"elly", 6}});

const string& name_for_section_id(uint8_t section_id) {
  if (section_id < section_id_to_name.size()) {
    return section_id_to_name[section_id];
  } else {
    static const string ret = "<Unknown section id>";
    return ret;
  }
}

u16string u16name_for_section_id(uint8_t section_id) {
  return decode_sjis(name_for_section_id(section_id));
}

uint8_t section_id_for_name(const string& name) {
  try {
    return name_to_section_id.at(name);
  } catch (const out_of_range&) { }
  try {
    uint64_t x = stoul(name);
    if (x < section_id_to_name.size()) {
      return x;
    }
  } catch (const invalid_argument&) {
  } catch (const out_of_range&) { }
  return 0xFF;
}

uint8_t section_id_for_name(const u16string& name) {
  return section_id_for_name(encode_sjis(name));
}

const string& name_for_event(uint8_t event) {
  if (event < lobby_event_to_name.size()) {
    return lobby_event_to_name[event];
  } else {
    static const string ret = "<Unknown lobby event>";
    return ret;
  }
}

u16string u16name_for_event(uint8_t event) {
  return decode_sjis(name_for_event(event));
}

uint8_t event_for_name(const string& name) {
  try {
    return name_to_lobby_event.at(name);
  } catch (const out_of_range&) { }
  try {
    uint64_t x = stoul(name);
    if (x < lobby_event_to_name.size()) {
      return x;
    }
  } catch (const invalid_argument&) {
  } catch (const out_of_range&) { }
  return 0xFF;
}

uint8_t event_for_name(const u16string& name) {
  return event_for_name(encode_sjis(name));
}

const string& name_for_lobby_type(uint8_t type) {
  try {
    return lobby_type_to_name.at(type);
  } catch (const out_of_range&) {
    static const string ret = "<Unknown lobby type>";
    return ret;
  }
}

u16string u16name_for_lobby_type(uint8_t type) {
  return decode_sjis(name_for_lobby_type(type));
}

uint8_t lobby_type_for_name(const string& name) {
  try {
    return name_to_lobby_type.at(name);
  } catch (const out_of_range&) { }
  try {
    uint64_t x = stoul(name);
    if (x < lobby_type_to_name.size()) {
      return x;
    }
  } catch (const invalid_argument&) {
  } catch (const out_of_range&) { }
  return 0x80;
}

uint8_t lobby_type_for_name(const u16string& name) {
  return lobby_type_for_name(encode_sjis(name));
}

const string& name_for_technique(uint8_t tech) {
  try {
    return tech_id_to_name.at(tech);
  } catch (const out_of_range&) {
    static const string ret = "<Unknown technique>";
    return ret;
  }
}

u16string u16name_for_technique(uint8_t tech) {
  return decode_sjis(name_for_technique(tech));
}

uint8_t technique_for_name(const string& name) {
  try {
    return name_to_tech_id.at(name);
  } catch (const out_of_range&) { }
  try {
    uint64_t x = stoul(name);
    if (x < tech_id_to_name.size()) {
      return x;
    }
  } catch (const invalid_argument&) {
  } catch (const out_of_range&) { }
  return 0xFF;
}

uint8_t technique_for_name(const u16string& name) {
  return technique_for_name(encode_sjis(name));
}

const string& name_for_npc(uint8_t npc) {
  try {
    return npc_id_to_name.at(npc);
  } catch (const out_of_range&) {
    static const string ret = "<Unknown NPC>";
    return ret;
  }
}

u16string u16name_for_npc(uint8_t npc) {
  return decode_sjis(name_for_npc(npc));
}

uint8_t npc_for_name(const string& name) {
  try {
    return name_to_npc_id.at(name);
  } catch (const out_of_range&) { }
  try {
    uint64_t x = stoul(name);
    if (x < npc_id_to_name.size()) {
      return x;
    }
  } catch (const invalid_argument&) {
  } catch (const out_of_range&) { }
  return 0xFF;
}

uint8_t npc_for_name(const u16string& name) {
  return npc_for_name(encode_sjis(name));
}



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

static void check_is_ep3(shared_ptr<Client> c, bool is_ep3) {
  if (!!(c->flags & ClientFlag::EPISODE_3_GAMES) != is_ep3) {
    throw precondition_failed(is_ep3 ?
        u"$C6This command can only\nbe used in Episode 3." :
        u"$C6This command cannot\nbe used in Episode 3.");
  }
}

static void check_cheats_enabled(shared_ptr<Lobby> l) {
  if (!(l->flags & LobbyFlag::CHEATS_ENABLED)) {
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

static void command_lobby_info(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t*) {
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

    send_text_message_printf(c,
        "$C6Game ID: %08X\n%s\nSection ID: %s\nCheat mode: %s",
        l->lobby_id, level_string.c_str(),
        name_for_section_id(l->section_id).c_str(),
        (l->flags & LobbyFlag::CHEATS_ENABLED) ? "on" : "off");

  } else {
    size_t num_clients = l->count_clients();
    size_t max_clients = l->max_clients;
    send_text_message_printf(c, "$C6Lobby ID: %08X\nPlayers: %zu/%zu",
        l->lobby_id, num_clients, max_clients);
  }
}

static void command_ax(shared_ptr<ServerState>, shared_ptr<Lobby>,
    shared_ptr<Client> c, const char16_t* args) {
  check_privileges(c, Privilege::ANNOUNCE);
  string message = encode_sjis(args);
  log(INFO, "[Client message from %010u] %s\n", c->license->serial_number, message.c_str());
}

static void command_announce(shared_ptr<ServerState> s, shared_ptr<Lobby>,
    shared_ptr<Client> c, const char16_t* args) {
  check_privileges(c, Privilege::ANNOUNCE);
  send_text_message(s, args);
}

static void command_arrow(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  // no preconditions
  c->lobby_arrow_color = stoull(encode_sjis(args), nullptr, 0);
  if (!l->is_game()) {
    send_arrow_update(l);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Lobby commands

static void command_cheat(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t*) {
  check_is_game(l, true);
  check_is_leader(l, c);

  l->flags ^= LobbyFlag::CHEATS_ENABLED;
  send_text_message_printf(l, "Cheat mode %s",
      (l->flags & LobbyFlag::CHEATS_ENABLED) ? "enabled" : "disabled");

  // if cheat mode was disabled, turn off all the cheat features that were on
  if (!(l->flags & LobbyFlag::CHEATS_ENABLED)) {
    for (size_t x = 0; x < l->max_clients; x++) {
      auto c = l->clients[x];
      if (!c) {
        continue;
      }
      c->infinite_hp = false;
      c->infinite_tp = false;
      c->switch_assist = false;
    }
    memset(&l->next_drop_item, 0, sizeof(l->next_drop_item));
  }
}

static void command_lobby_event(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  check_is_game(l, false);
  check_privileges(c, Privilege::CHANGE_EVENT);

  uint8_t new_event = event_for_name(args);
  if (new_event == 0xFF) {
    send_text_message(c, u"$C6No such lobby event.");
    return;
  }

  l->event = new_event;
  send_change_event(l, l->event);
}

static void command_lobby_event_all(shared_ptr<ServerState> s, shared_ptr<Lobby>,
    shared_ptr<Client> c, const char16_t* args) {
  check_privileges(c, Privilege::CHANGE_EVENT);

  uint8_t new_event = event_for_name(args);
  if (new_event == 0xFF) {
    send_text_message(c, u"$C6No such lobby event.");
    return;
  }

  for (auto l : s->all_lobbies()) {
    if (l->is_game() || !(l->flags & LobbyFlag::DEFAULT)) {
      continue;
    }

    l->event = new_event;
    send_change_event(l, l->event);
  }
}

static void command_lobby_type(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  check_is_game(l, false);
  check_privileges(c, Privilege::CHANGE_EVENT);

  uint8_t new_type = lobby_type_for_name(args);
  if (new_type == 0x80) {
    send_text_message(c, u"$C6No such lobby type.");
    return;
  }

  l->type = new_type;
  if (l->type < ((l->flags & LobbyFlag::EPISODE_3) ? 20 : 15)) {
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

static void command_secid(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  check_is_game(l, false);

  if (!args[0]) {
    c->override_section_id = -1;
    send_text_message(l, u"$C6Override section ID\nremoved");
  } else {
    c->override_section_id = section_id_for_name(args);
    send_text_message(l, u"$C6Override section ID\nset");
  }
}

static void command_password(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  check_is_game(l, true);
  check_is_leader(l, c);

  if (!args[0]) {
    l->password[0] = 0;
    send_text_message(l, u"$C6Game unlocked");

  } else {
    l->password = args;
    auto encoded = encode_sjis(l->password);
    send_text_message_printf(l, "$C6Game password:\n%s",
        encoded.c_str());
  }
}

static void command_min_level(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  check_is_game(l, true);
  check_is_leader(l, c);

  u16string buffer;
  l->min_level = stoull(encode_sjis(args)) - 1;
  send_text_message_printf(l, "$C6Minimum level set to %" PRIu32,
      l->min_level + 1);
}

static void command_max_level(shared_ptr<ServerState>, shared_ptr<Lobby> l,
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
  vector<string> tokens = split(encoded_args, ' ');

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
    uint32_t new_color;
    sscanf(tokens[1].c_str(), "%8X", &new_color);
    c->player.disp.name_color = new_color;
  } else if (tokens[0] == "secid") {
    uint8_t secid = section_id_for_name(decode_sjis(tokens[1]));
    if (secid == 0xFF) {
      send_text_message(c, u"$C6No such section ID.");
      return;
    } else {
      c->player.disp.section_id = secid;
    }
  } else if (tokens[0] == "name") {
    c->player.disp.name = add_language_marker(tokens[1], 'J');
  } else if (tokens[0] == "npc") {
    if (tokens[1] == "none") {
      c->player.disp.extra_model = 0;
      c->player.disp.v2_flags &= 0xFD;
    } else {
      uint8_t npc = npc_for_name(decode_sjis(tokens[1]));
      if (npc == 0xFF) {
        send_text_message(c, u"$C6No such NPC.");
        return;
      }
      c->player.disp.extra_model = npc;
      c->player.disp.v2_flags |= 0x02;
    }
  } else if ((tokens[0] == "tech") && (tokens.size() > 2)) {
    uint8_t level = stoul(tokens[2]) - 1;
    if (tokens[1] == "all") {
      for (size_t x = 0; x < 0x14; x++) {
        c->player.disp.technique_levels.data()[x] = level;
      }
    } else {
      uint8_t tech_id = technique_for_name(decode_sjis(tokens[1]));
      if (tech_id == 0xFF) {
        send_text_message(c, u"$C6No such technique.");
        return;
      }
      c->player.disp.technique_levels.data()[tech_id] = level;
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

static void command_change_bank(shared_ptr<ServerState>, shared_ptr<Lobby>,
    shared_ptr<Client> c, const char16_t*) {
  check_version(c, GameVersion::BB);

  // TODO: implement this
  // TODO: make sure the bank name is filesystem-safe
}

static void command_convert_char_to_bb(shared_ptr<ServerState> s,
    shared_ptr<Lobby> l, shared_ptr<Client> c, const char16_t* args) {
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
  check_privileges(c, Privilege::SILENCE_USER);

  auto target = s->find_client(args);
  if (!target->license) {
    // this should be impossible, but I'll bet it's not actually
    send_text_message(c, u"$C6Client not logged in");
    return;
  }

  if (target->license->privileges & Privilege::MODERATOR) {
    send_text_message(c, u"$C6You do not have\nsufficient privileges.");
    return;
  }

  target->can_chat = !target->can_chat;
  string target_name_sjis = encode_sjis(target->player.disp.name);
  send_text_message_printf(l, "$C6%s %ssilenced", target_name_sjis.c_str(),
      target->can_chat ? "un" : "");
}

static void command_kick(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  check_privileges(c, Privilege::KICK_USER);

  auto target = s->find_client(args);
  if (!target->license) {
    // this should be impossible, but I'll bet it's not actually
    send_text_message(c, u"$C6Client not logged in");
    return;
  }

  if (target->license->privileges & Privilege::MODERATOR) {
    send_text_message(c, u"$C6You do not have\nsufficient privileges.");
    return;
  }

  send_message_box(target, u"$C6You were kicked off by a moderator.");
  target->should_disconnect = true;
  string target_name_sjis = encode_sjis(target->player.disp.name);
  send_text_message_printf(l, "$C6%s kicked off", target_name_sjis.c_str());
}

static void command_ban(shared_ptr<ServerState> s, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  check_privileges(c, Privilege::BAN_USER);

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

  if (target->license->privileges & Privilege::BAN_USER) {
    send_text_message(c, u"$C6You do not have\nsufficient privileges.");
    return;
  }

  uint64_t usecs = stoull(encode_sjis(args), nullptr, 0) * 1000000;

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

static void command_warp(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t* args) {
  check_is_game(l, true);
  check_cheats_enabled(l);

  uint32_t area = stoul(encode_sjis(args), nullptr, 0);
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

static void command_song(shared_ptr<ServerState>, shared_ptr<Lobby>,
    shared_ptr<Client> c, const char16_t* args) {
  check_is_ep3(c, true);

  uint32_t song = stoul(encode_sjis(args), nullptr, 0);
  send_ep3_change_music(c, song);
}

static void command_infinite_hp(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t*) {
  check_is_game(l, true);
  check_cheats_enabled(l);

  c->infinite_hp = !c->infinite_hp;
  send_text_message_printf(c, "$C6Infinite HP %s", c->infinite_hp ? "enabled" : "disabled");
}

static void command_infinite_tp(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t*) {
  check_is_game(l, true);
  check_cheats_enabled(l);

  c->infinite_tp = !c->infinite_tp;
  send_text_message_printf(c, "$C6Infinite TP %s", c->infinite_tp ? "enabled" : "disabled");
}

static void command_switch_assist(shared_ptr<ServerState>, shared_ptr<Lobby> l,
    shared_ptr<Client> c, const char16_t*) {
  check_is_game(l, true);
  check_cheats_enabled(l);

  c->switch_assist = !c->switch_assist;
  send_text_message_printf(c, "$C6Switch assist %s", c->switch_assist ? "enabled" : "disabled");
}

static void command_item(shared_ptr<ServerState>, shared_ptr<Lobby> l,
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
    // TODO: implement command_help and actually use the usage strings here
    {u"allevent"  , {command_lobby_event_all   , u"Usage:\nallevent <name/ID>"}},
    {u"ann"       , {command_announce          , u"Usage:\nann <message>"}},
    {u"arrow"     , {command_arrow             , u"Usage:\narrow <color>"}},
    {u"ax"        , {command_ax                , u"Usage:\nax <message>"}},
    {u"ban"       , {command_ban               , u"Usage:\nban <name-or-number>"}},
    {u"bbchar"    , {command_convert_char_to_bb, u"Usage:\nbbchar <user> <pass> <1-4>"}},
    {u"changebank", {command_change_bank       , u"Usage:\nchangebank <bank name>"}},
    {u"cheat"     , {command_cheat             , u"Usage:\ncheat"}},
    {u"edit"      , {command_edit              , u"Usage:\nedit <stat> <value>"}},
    {u"event"     , {command_lobby_event       , u"Usage:\nevent <name>"}},
    {u"infhp"     , {command_infinite_hp       , u"Usage:\ninfhp"}},
    {u"inftp"     , {command_infinite_tp       , u"Usage:\ninftp"}},
    {u"item"      , {command_item              , u"Usage:\nitem <item-code>"}},
    {u"kick"      , {command_kick              , u"Usage:\nkick <name-or-number>"}},
    {u"li"        , {command_lobby_info        , u"Usage:\nli"}},
    {u"maxlevel"  , {command_max_level         , u"Usage:\nmax_level <level>"}},
    {u"minlevel"  , {command_min_level         , u"Usage:\nmin_level <level>"}},
    {u"password"  , {command_password          , u"Usage:\nlock [password]\nomit password to\nunlock game"}},
    {u"secid"     , {command_secid             , u"Usage:\nsecid [section ID]\nomit section ID to\nrevert to normal"}},
    {u"silence"   , {command_silence           , u"Usage:\nsilence <name-or-number>"}},
    {u"song"      , {command_song              , u"Usage:\nsong <song-number>"}},
    {u"swa"       , {command_switch_assist     , u"Usage:\nswa"}},
    {u"type"      , {command_lobby_type        , u"Usage:\ntype <name>"}},
    {u"warp"      , {command_warp              , u"Usage:\nwarp <area-number>"}},
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

  const ChatCommandDefinition* def = nullptr;
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
