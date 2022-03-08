#include "SendCommands.hh"

#include <inttypes.h>
#include <string.h>

#include <memory>
#include <phosg/Encoding.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>
#include <phosg/Time.hh>

#include "PSOProtocol.hh"
#include "FileContentsCache.hh"
#include "Text.hh"

using namespace std;



#pragma pack(push)
#pragma pack(1)

static FileContentsCache file_cache;



void send_command(shared_ptr<Client> c, uint16_t command, uint32_t flag,
    const void* data, size_t size) {
  string send_data;

  switch (c->version) {
    case GameVersion::GC:
    case GameVersion::DC: {
      PSOCommandHeaderDCGC header;
      header.command = command;
      header.flag = flag;
      header.size = sizeof(header) + size;
      send_data.append(reinterpret_cast<const char*>(&header), sizeof(header));
      if (size) {
        send_data.append(reinterpret_cast<const char*>(data), size);
        send_data.resize((send_data.size() + 3) & ~3);
      }
      break;
    }

    case GameVersion::PC:
    case GameVersion::Patch: {
      PSOCommandHeaderPC header;
      header.size = sizeof(header) + size;
      header.command = command;
      header.flag = flag;
      send_data.append(reinterpret_cast<const char*>(&header), sizeof(header));
      if (size) {
        send_data.append(reinterpret_cast<const char*>(data), size);
        send_data.resize((send_data.size() + 3) & ~3);
      }
      break;
    }

    case GameVersion::BB: {
      PSOCommandHeaderBB header;
      header.size = sizeof(header) + size;
      header.command = command;
      header.flag = flag;
      send_data.append(reinterpret_cast<const char*>(&header), sizeof(header));
      if (size) {
        send_data.append(reinterpret_cast<const char*>(data), size);
        send_data.resize((send_data.size() + 7) & ~7);
      }
      break;
    }

    default:
      throw logic_error("unimplemented game version in send_command");
  }

  string name_token;
  if (c->player.disp.name[0]) {
    name_token = " to " + remove_language_marker(encode_sjis(c->player.disp.name));
  }
  log(INFO, "Sending%s version=%d size=%04zX command=%04hX flag=%08X",
      name_token.c_str(), static_cast<int>(c->version), size, command, flag);
  print_data(stderr, send_data.data(), send_data.size());

  c->send(move(send_data));
}

void send_command_excluding_client(shared_ptr<Lobby> l, shared_ptr<Client> c,
    uint16_t command, uint32_t flag, const void* data, size_t size) {
  for (auto& client : l->clients) {
    if (!client || (client == c)) {
      continue;
    }
    send_command(client, command, flag, data, size);
  }
}

void send_command(shared_ptr<Lobby> l, uint16_t command, uint32_t flag,
    const void* data, size_t size) {
  send_command_excluding_client(l, nullptr, command, flag, data, size);
}

void send_command(shared_ptr<ServerState> s, uint16_t command, uint32_t flag,
    const void* data, size_t size) {
  for (auto& l : s->all_lobbies()) {
    send_command(l, command, flag, data, size);
  }
}



// specific command sending functions follow. in general, they're written in
// such a way that you don't need to think about anything, even the client's
// version, before calling them. for this reason, some of them are quite
// complex. many are split into several functions, one for each version of PSO,
// named with suffixes _GC, _BB, and the like. in these cases, the function
// without the suffix simply calls the appropriate function for the client's
// version. thus, if you change something in one of the version-specific
// functions, you may have to change it in all of them.

////////////////////////////////////////////////////////////////////////////////
// CommandServerInit: this function sends the command that initializes encryption

// strings needed for various functions
static const char* anti_copyright = "This server is in no way affiliated, sponsored, or supported by SEGA Enterprises or SONICTEAM. The preceding message exists only in order to remain compatible with programs that expect it.";
static const char* dc_port_map_copyright = "DreamCast Port Map. Copyright SEGA Enterprises. 1999";
static const char* dc_lobby_server_copyright = "DreamCast Lobby Server. Copyright SEGA Enterprises. 1999";
static const char* bb_game_server_copyright = "Phantasy Star Online Blue Burst Game Server. Copyright 1999-2004 SONICTEAM.";
static const char* patch_server_copyright = "Patch Server. Copyright SonicTeam, LTD. 2001";

static void send_server_init_dc_pc_gc(shared_ptr<Client> c, const char* copyright_text,
    uint8_t command) {
  struct {
    char copyright[0x40];
    uint32_t server_key;
    uint32_t client_key;
    char after_message[200];
  } cmd;

  uint32_t server_key = random_object<uint32_t>();
  uint32_t client_key = random_object<uint32_t>();

  memset(&cmd, 0, sizeof(cmd));
  strcpy(cmd.copyright, copyright_text);
  cmd.server_key = server_key;
  cmd.client_key = client_key;
  strcpy(cmd.after_message, anti_copyright);
  send_command(c, command, 0x00, cmd);

  switch (c->version) {
    case GameVersion::DC:
    case GameVersion::PC:
      c->crypt_out.reset(new PSOPCEncryption(server_key));
      c->crypt_in.reset(new PSOPCEncryption(client_key));
      break;
    case GameVersion::GC:
      c->crypt_out.reset(new PSOGCEncryption(server_key));
      c->crypt_in.reset(new PSOGCEncryption(client_key));
      break;
    default:
      throw invalid_argument("incorrect client version");
  }
}

static void send_server_init_pc(shared_ptr<Client> c, bool initial_connection) {
  send_server_init_dc_pc_gc(c,
      initial_connection ? dc_port_map_copyright : dc_lobby_server_copyright,
      0x17);
}

static void send_server_init_gc(shared_ptr<Client> c, bool initial_connection) {
  send_server_init_dc_pc_gc(c,
      initial_connection ? dc_port_map_copyright : dc_lobby_server_copyright,
      initial_connection ? 0x17 : 0x02);
}

static void send_server_init_bb(shared_ptr<ServerState> s, shared_ptr<Client> c,
    bool) {
  struct {
    char copyright[0x60];
    uint8_t server_key[0x30];
    uint8_t client_key[0x30];
    char after_message[200];
  } cmd;

  memset(&cmd, 0, sizeof(cmd));
  strcpy(cmd.copyright, bb_game_server_copyright);
  random_data(cmd.server_key, 0x30);
  random_data(cmd.client_key, 0x30);
  strcpy(cmd.after_message, anti_copyright);
  send_command(c, 0x03, 0x00, cmd);

  c->crypt_out.reset(new PSOBBEncryption(s->default_key_file, cmd.server_key,
      sizeof(cmd.server_key)));
  c->crypt_in.reset(new PSOBBEncryption(s->default_key_file, cmd.client_key,
      sizeof(cmd.client_key)));
}

static void send_server_init_patch(shared_ptr<Client> c, bool) {
  struct {
    char copyright[0x40];
    uint32_t server_key;
    uint32_t client_key;
    // BB rejects the command if it's not exactly this size, so we can't add the
    // anti-copyright message... lawyers plz be kind kthx
  } cmd;

  uint32_t server_key = random_object<uint32_t>();
  uint32_t client_key = random_object<uint32_t>();

  memset(&cmd, 0, sizeof(cmd));
  strcpy(cmd.copyright, patch_server_copyright);
  cmd.server_key = server_key;
  cmd.client_key = client_key;
  send_command(c, 0x02, 0x00, cmd);

  c->crypt_out.reset(new PSOPCEncryption(server_key));
  c->crypt_in.reset(new PSOPCEncryption(client_key));
}

void send_server_init(shared_ptr<ServerState> s, shared_ptr<Client> c,
    bool initial_connection) {
  if (c->version == GameVersion::PC) {
    send_server_init_pc(c, initial_connection);
  } else if (c->version == GameVersion::Patch) {
    send_server_init_patch(c, initial_connection);
  } else if (c->version == GameVersion::GC) {
    send_server_init_gc(c, initial_connection);
  } else if (c->version == GameVersion::BB) {
    send_server_init_bb(s, c, initial_connection);
  } else {
    throw logic_error("unimplemented versioned command");
  }
}

// for non-BB clients, updates the client's guild card and security data
void send_update_client_config(shared_ptr<Client> c) {
  struct {
    uint32_t player_tag;
    uint32_t serial_number;
    ClientConfig config;
  } cmd = {
    0x00010000,
    c->license->serial_number,
    c->export_config(),
  };
  send_command(c, 0x04, 0x00, cmd);
}



void send_reconnect(shared_ptr<Client> c, uint32_t address, uint16_t port) {
  struct {
    // The address is big-endian, for some reason. Probably it was defined as a
    // uint8_t[4] in the original PSO source rather than a uint32_t
    be_uint32_t address;
    uint16_t port;
    uint16_t unused;
  } cmd = {address, port, 0};
  send_command(c, 0x19, 0x00, cmd);
}

// sends the command (first used by Schthack) that separates PC and GC users
// that connect on the same port
void send_pc_gc_split_reconnect(shared_ptr<Client> c, uint32_t address,
    uint16_t pc_port, uint16_t gc_port) {
  struct {
    be_uint32_t pc_address;
    uint16_t pc_port;
    uint8_t unused1[0x0F];
    uint8_t gc_command;
    uint8_t gc_flag;
    uint16_t gc_size;
    be_uint32_t gc_address;
    uint16_t gc_port;
    uint8_t unused2[0xB0 - 0x23];
  } cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.pc_address = address;
  cmd.pc_port = pc_port;
  cmd.gc_command = 0x19;
  cmd.gc_size = 0x97;
  cmd.gc_address = address;
  cmd.gc_port = gc_port;
  send_command(c, 0x19, 0x00, cmd);
}



// sends the command that signals an error or updates the client's guild card
// number and security data
void send_client_init_bb(shared_ptr<Client> c, uint32_t error) {
  struct {
    uint32_t error; // see below
    uint32_t player_tag;
    uint32_t serial_number;
    uint32_t team_id; // just randomize it; teams aren't supported
    ClientConfig cfg;
    uint32_t caps; // should be 0x00000102
  } cmd = {
    error,
    0x00010000,
    c->license->serial_number,
    static_cast<uint32_t>(random_object<uint32_t>()),
    c->export_config(),
    0x00000102,
  };
  send_command(c, 0x00E6, 0x00000000, cmd);
}

void send_team_and_key_config_bb(shared_ptr<Client> c) {
  send_command(c, 0x00E2, 0x00000000, c->player.key_config);
}

// sends a player preview. these are used by the caracter select and character
// creation mechanism
void send_player_preview_bb(shared_ptr<Client> c, uint8_t player_index,
    const PlayerDispDataBBPreview* preview) {

  if (!preview) {
    // no player exists
    struct {
      uint32_t player_index;
      uint32_t error;
    } cmd = {player_index, 0x00000002};
    send_command(c, 0x00E4, 0x00000000, cmd);

  } else {
    struct {
      uint32_t player_index;
      PlayerDispDataBBPreview preview;
    } cmd = {player_index, *preview};
    send_command(c, 0x00E3, 0x00000000, cmd);
  }
}

// sent in response to the client's 01E8 command
void send_accept_client_checksum_bb(shared_ptr<Client> c) {
  struct {
    uint32_t verify;
    uint32_t unused;
  } cmd = {1, 0};
  send_command(c, 0x02E8, 0x00000000, cmd);
}

// sends the "I'm about to send your guild card file" command
void send_guild_card_header_bb(shared_ptr<Client> c) {
  uint32_t checksum = compute_guild_card_checksum(&c->player.guild_cards,
      sizeof(GuildCardFileBB));
  struct {
    uint32_t unknown; // should be 1
    uint32_t filesize; // 0x0000490
    uint32_t checksum;
  } cmd = {1, 0x490, checksum};
  send_command(c, 0x01DC, 0x00000000, cmd);
}

// sends a chunk of guild card data
void send_guild_card_chunk_bb(shared_ptr<Client> c, size_t chunk_index) {
  size_t chunk_offset = chunk_index * 0x6800;
  if (chunk_offset >= sizeof(GuildCardFileBB)) {
    throw logic_error("attempted to send chunk beyond end of guild card file");
  }
  size_t data_size = sizeof(GuildCardFileBB) - chunk_offset;
  if (data_size > 0x6800) {
    data_size = 0x6800;
  }

  string contents(8, '\0');
  *reinterpret_cast<uint32_t*>(contents.data()) = 0;
  *reinterpret_cast<uint32_t*>(contents.data() + 4) = chunk_index;
  contents.append(reinterpret_cast<char*>(&c->player.guild_cards + chunk_offset),
      data_size);

  send_command(c, 0x02DC, 0x00000000, contents);
}

// sends the game data (battleparamentry files, etc.)
void send_stream_file_bb(shared_ptr<Client> c) {

  struct StreamFileEntry {
    uint32_t size;
    uint32_t checksum;
    uint32_t offset;
    char filename[0x40];
  };

  auto index_data = file_cache.get("system/blueburst/streamfile.ind");
  if (index_data->size() % sizeof(StreamFileEntry)) {
    throw invalid_argument("stream file index not a multiple of entry size");
  }

  size_t entry_count = index_data->size() / sizeof(StreamFileEntry);
  send_command(c, 0x01EB, entry_count, index_data);

  auto* entries = reinterpret_cast<const StreamFileEntry*>(index_data->data());

  struct {
    uint32_t chunk_index;
    uint8_t data[0x6800];
  } chunk_cmd;
  chunk_cmd.chunk_index = 0;

  uint32_t buffer_offset = 0;
  for (size_t x = 0; x < entry_count; x++) {
    auto filename = string_printf("system/blueburst/%s", entries[x].filename);
    auto file_data = file_cache.get(filename);

    size_t file_data_remaining = file_data->size();
    if (file_data_remaining != entries[x].size) {
      throw invalid_argument(filename + " does not match size in stream file index");
    }
    while (file_data_remaining) {
      size_t read_size = 0x6800 - buffer_offset;
      if (read_size > file_data_remaining) {
        read_size = file_data_remaining;
      }
      memcpy(&chunk_cmd.data[buffer_offset],
          file_data->data() + file_data->size() - file_data_remaining, read_size);
      buffer_offset += read_size;
      file_data_remaining -= read_size;

      if (buffer_offset == 0x6800) {
        // note: the client sends 0x03EB in response to these, but we'll just
        // ignore them because we don't need any of the contents
        send_command(c, 0x02EB, 0x00000000, chunk_cmd);
        buffer_offset = 0;
        chunk_cmd.chunk_index++;
      }
    }

    if (buffer_offset > 0) {
      send_command(c, 0x02EB, 0x00000000, &chunk_cmd, (buffer_offset + 15) & ~3);
    }
  }
}

// accepts the player's choice at char select
void send_approve_player_choice_bb(shared_ptr<Client> c) {
  struct {
    uint32_t player_index;
    uint32_t unused;
  } cmd = {c->bb_player_index, 1};
  send_command(c, 0x00E4, 0x00000000, cmd);
}

// sends player data to the client (usually sent right before entering lobby)
void send_complete_player_bb(shared_ptr<Client> c) {
  send_command(c, 0x00E7, 0x00000000, c->player.export_bb_player_data());
}



////////////////////////////////////////////////////////////////////////////////
// patch functions

void send_check_directory_patch(shared_ptr<Client> c, const char* dir) {
  char data[0x40];
  memset(data, 0, 0x40);
  strcpy(data, dir);
  send_command(c, 0x09, 0x00, data, 0x40);
}



////////////////////////////////////////////////////////////////////////////////
// message functions

struct LargeMessageOptionalHeader {
  uint32_t unused;
  uint32_t serial_number;
};

static void send_large_message_pc_patch_bb(shared_ptr<Client> c, uint8_t command,
    const char16_t* text, uint32_t from_serial_number, bool include_header) {
  u16string data;
  if (include_header) {
    data.resize(sizeof(LargeMessageOptionalHeader) / sizeof(char16_t));
    *reinterpret_cast<LargeMessageOptionalHeader*>(data.data()) =
        {0, from_serial_number};
  }
  data += text;
  add_color_inplace(data, (include_header ? sizeof(LargeMessageOptionalHeader) : 0));
  data.resize((data.size() + 4) & ~3);
  send_command(c, command, 0x00, data);
}

static void send_large_message_dc_gc(shared_ptr<Client> c, uint8_t command,
    const char16_t* text, uint32_t from_serial_number, bool include_header) {
  string data;
  if (include_header) {
    data.resize(sizeof(LargeMessageOptionalHeader) / sizeof(char));
    *reinterpret_cast<LargeMessageOptionalHeader*>(data.data()) =
        {0, from_serial_number};
  }
  data += encode_sjis(text);
  add_color_inplace(data, (include_header ? sizeof(LargeMessageOptionalHeader) : 0));
  data.resize((data.size() + 4) & ~3);
  send_command(c, command, 0x00, data);
}

static void send_large_message(shared_ptr<Client> c, uint8_t command,
    const char16_t* text, uint32_t from_serial_number, bool include_header) {
  if (c->version == GameVersion::PC || c->version == GameVersion::Patch ||
      c->version == GameVersion::BB) {
    send_large_message_pc_patch_bb(c, command, text, from_serial_number, include_header);
  } else {
    send_large_message_dc_gc(c, command, text, from_serial_number, include_header);
  }
}

void send_message_box(shared_ptr<Client> c, const char16_t* text) {
  return send_large_message(c, (c->version == GameVersion::Patch) ? 0x13 : 0x1A,
      text, 0, false);
}

void send_lobby_name(shared_ptr<Client> c, const char16_t* text) {
  return send_large_message(c, 0x8A, text, 0, false);
}

void send_quest_info(shared_ptr<Client> c, const char16_t* text) {
  return send_large_message(c, 0xA3, text, 0, false);
}

void send_lobby_message_box(shared_ptr<Client> c, const char16_t* text) {
  return send_large_message(c, 0x01, text, 0, true);
}

void send_ship_info(shared_ptr<Client> c, const char16_t* text) {
  return send_large_message(c, 0x11, text, 0, true);
}

void send_text_message(shared_ptr<Client> c, const char16_t* text) {
  return send_large_message(c, 0xB0, text, 0, true);
}

void send_text_message(shared_ptr<Lobby> l, const char16_t* text) {
  for (size_t x = 0; x < l->max_clients; x++) {
    if (l->clients[x]) {
      send_text_message(l->clients[x], text);
    }
  }
}

void send_text_message(shared_ptr<ServerState> s, const char16_t* text) {
  for (auto& l : s->all_lobbies()) {
    send_text_message(l, text);
  }
}

void send_chat_message(shared_ptr<Client> c, uint32_t from_serial_number,
    const char16_t* from_name, const char16_t* text) {
  u16string data;
  if (c->version == GameVersion::BB) {
    data.append(u"\x09J");
  }
  data.append(remove_language_marker(from_name));
  data.append(u"\x09\x09J");
  data.append(text);
  send_large_message(c, 0x06, data.c_str(), from_serial_number, true);
}

void send_simple_mail_gc(std::shared_ptr<Client> c, uint32_t from_serial_number,
    const char16_t* from_name, const char16_t* text) {
  struct {
    uint32_t player_tag;
    uint32_t from_serial_number;
    char from_name[0x10];
    uint32_t to_serial_number;
    char text[0x200];
  } cmd;

  cmd.player_tag = 0x00010000;
  cmd.from_serial_number = from_serial_number;
  encode_sjis(cmd.from_name, from_name, sizeof(cmd.from_name) / sizeof(cmd.from_name[0]));
  cmd.to_serial_number = c->license->serial_number;
  encode_sjis(cmd.text, text, sizeof(cmd.text) / sizeof(cmd.text[0]));

  send_command(c, 0x81, 0x00, cmd);
}

void send_simple_mail(std::shared_ptr<Client> c, uint32_t from_serial_number,
    const char16_t* from_name, const char16_t* text) {
  if (c->version == GameVersion::GC) {
    send_simple_mail_gc(c, from_serial_number, from_name, text);
  } else {
    throw logic_error("unimplemented versioned command");
  }
}



////////////////////////////////////////////////////////////////////////////////
// info board

static void send_info_board_pc_bb(shared_ptr<Client> c, shared_ptr<Lobby> l) {
  struct Entry {
    char16_t name[0x10];
    char16_t message[0xAC];
  };
  vector<Entry> entries;

  for (const auto& c : l->clients) {
    if (!c.get()) {
      continue;
    }

    entries.emplace_back();
    auto& e = entries.back();
    memset(&e, 0, sizeof(Entry));
    char16cpy(e.name, c->player.disp.name, 0x10);
    char16cpy(e.message, c->player.info_board, 0xAC);
    add_color_inplace(e.message);
  }

  send_command(c, 0xD8, 0x00, entries);
}

static void send_info_board_dc_gc(shared_ptr<Client> c, shared_ptr<Lobby> l) {
  struct Entry {
    char name[0x10];
    char message[0xAC];
  };
  vector<Entry> entries;

  for (const auto& c : l->clients) {
    if (!c.get()) {
      continue;
    }

    entries.emplace_back();
    auto& e = entries.back();
    memset(&e, 0, sizeof(Entry));
    encode_sjis(e.name, c->player.disp.name, 0x10);
    encode_sjis(e.message, c->player.info_board, 0xAC);
    add_color_inplace(e.message);
  }

  send_command(c, 0xD8, 0x00, entries);
}

void send_info_board(shared_ptr<Client> c, shared_ptr<Lobby> l) {
  if (c->version == GameVersion::PC || c->version == GameVersion::Patch ||
      c->version == GameVersion::BB) {
    send_info_board_pc_bb(c, l);
  } else {
    send_info_board_dc_gc(c, l);
  }
}



////////////////////////////////////////////////////////////////////////////////
// CommandCardSearchResult: sends a guild card search result to a player.

static void send_card_search_result_dc_pc_gc(shared_ptr<ServerState> s,
    shared_ptr<Client> c, shared_ptr<Client> result,
    shared_ptr<Lobby> result_lobby) {
  struct {
    uint32_t player_tag;
    uint32_t searcher_serial_number;
    uint32_t result_serial_number;
    struct {
      union {
        struct {
          uint8_t dcgc_command;
          uint8_t dcgc_flag;
          uint16_t dcgc_size;
        };
        struct {
          uint16_t pc_size;
          uint8_t pc_command;
          uint8_t pc_flag;
        };
      };
      uint32_t address;
      uint16_t port;
      uint16_t unused;
    } destination_command;
    char location_string[0x44];
    uint32_t menu_id;
    uint32_t lobby_id;
    char unused[0x3C];
    char16_t name[0x20];
  } cmd;

  memset(&cmd, 0, sizeof(cmd));
  cmd.player_tag = 0x00010000;
  cmd.searcher_serial_number = c->license->serial_number;
  cmd.result_serial_number = result->license->serial_number;
  if (c->version == GameVersion::PC) {
    cmd.destination_command.pc_size = 0x000C;
    cmd.destination_command.pc_command = 0x19;
    cmd.destination_command.pc_flag = 0x00;
  } else {
    cmd.destination_command.dcgc_command = 0x19;
    cmd.destination_command.dcgc_flag = 0x00;
    cmd.destination_command.dcgc_size = 0x000C;
  }
  // TODO: make this actually make sense... currently we just take the sockname
  // for the target client
  const sockaddr_in* local_addr = reinterpret_cast<const sockaddr_in*>(&result->local_addr);
  cmd.destination_command.address = local_addr->sin_addr.s_addr;
  cmd.destination_command.port = ntohs(local_addr->sin_port);
  cmd.destination_command.unused = 0;

  auto encoded_server_name = encode_sjis(s->name);
  if (result_lobby->is_game()) {
    string encoded_lobby_name = encode_sjis(result_lobby->name);
    snprintf(cmd.location_string, sizeof(cmd.location_string),
        "%s, Block 00, ,%s", encoded_lobby_name.c_str(), encoded_server_name.c_str());
  } else {
    snprintf(cmd.location_string, sizeof(cmd.location_string), "Block 00, ,%s",
        encoded_server_name.c_str());
  }
  cmd.menu_id = LOBBY_MENU_ID;
  cmd.lobby_id = result->lobby_id;
  memset(cmd.unused, 0, sizeof(cmd.unused));
  char16cpy(cmd.name, result->player.disp.name, 0x20);

  send_command(c, 0x40, 0x00, cmd);
}

static void send_card_search_result_bb(shared_ptr<ServerState> s,
    shared_ptr<Client> c, shared_ptr<Client> result,
    shared_ptr<Lobby> result_lobby) {
  // this is identical to the dc/pc/gc function above, except the reconnect
  // command format is different. why did you do this, sega? are you so lazy
  // that really the best thing you could do is call handle_command() on a
  // substring of another command? lrn2code plz
  struct {
    uint32_t player_tag;
    uint32_t searcher_serial_number;
    uint32_t result_serial_number;
    struct {
      uint16_t size;
      uint16_t command;
      uint32_t flag;
      uint32_t address;
      uint16_t port;
      uint16_t unused;
    } destination_command;
    char location_string[0x44];
    uint32_t menu_id;
    uint32_t lobby_id;
    char unused[0x3C];
    char16_t name[0x20];
  } cmd;

  memset(&cmd, 0, sizeof(cmd));
  cmd.player_tag = 0x00010000;
  cmd.searcher_serial_number = c->license->serial_number;
  cmd.result_serial_number = result->license->serial_number;
  cmd.destination_command.size = 0x0010;
  cmd.destination_command.command = 0x19;
  cmd.destination_command.flag = 0x00000000;
  const sockaddr_in* local_addr = reinterpret_cast<const sockaddr_in*>(&result->local_addr);
  cmd.destination_command.address = local_addr->sin_addr.s_addr;
  cmd.destination_command.port = ntohs(local_addr->sin_port);
  cmd.destination_command.unused = 0;

  auto encoded_server_name = encode_sjis(s->name);
  if (result_lobby->is_game()) {
    string encoded_lobby_name = encode_sjis(result_lobby->name);
    snprintf(cmd.location_string, sizeof(cmd.location_string),
        "%s, Block 00, ,%s", encoded_lobby_name.c_str(), encoded_server_name.c_str());
  } else {
    snprintf(cmd.location_string, sizeof(cmd.location_string), "Block 00, ,%s",
        encoded_server_name.c_str());
  }
  cmd.menu_id = LOBBY_MENU_ID;
  cmd.lobby_id = result->lobby_id;
  memset(cmd.unused, 0, sizeof(cmd.unused));
  char16cpy(cmd.name, result->player.disp.name, 0x20);

  send_command(c, 0x40, 0x00, cmd);
}

void send_card_search_result(shared_ptr<ServerState> s, shared_ptr<Client> c,
    shared_ptr<Client> result, shared_ptr<Lobby> result_lobby) {
  if (c->version == GameVersion::BB) {
    send_card_search_result_bb(s, c, result, result_lobby);
  } else {
    send_card_search_result_dc_pc_gc(s, c, result, result_lobby);
  }
}

////////////////////////////////////////////////////////////////////////////////
// CommandSendGuildCard: generates a guild card for the source player and sends it to the destination player

static void send_guild_card_gc(shared_ptr<Client> c, shared_ptr<Client> source) {
  struct {
    uint8_t subcommand;
    uint8_t subsize;
    uint16_t unused;
    uint32_t player_tag;
    uint32_t serial_number;
    char name[0x18];
    char desc[0x6C];
    uint8_t reserved1;
    uint8_t reserved2;
    uint8_t section_id;
    uint8_t char_class;
  } cmd;

  cmd.subcommand = 0x06;
  cmd.subsize = 0x25;
  cmd.unused = 0x0000;
  cmd.player_tag = 0x00010000;
  cmd.reserved1 = 1;
  cmd.reserved2 = 1;

  cmd.serial_number = source->license->serial_number;
  encode_sjis(cmd.name, source->player.disp.name, 0x18);
  remove_language_marker_inplace(cmd.name);
  encode_sjis(cmd.desc, source->player.guild_card_desc, 0x6C);
  cmd.section_id = source->player.disp.section_id;
  cmd.char_class = source->player.disp.char_class;

  send_command(c, 0x62, c->lobby_client_id, cmd);
}

static void send_guild_card_bb(shared_ptr<Client> c, shared_ptr<Client> source) {
  struct {
    uint8_t subcommand;
    uint8_t subsize;
    uint16_t unused;
    uint32_t serial_number;
    char16_t name[0x18];
    char16_t team_name[0x10];
    char16_t desc[0x58];
    uint8_t reserved1;
    uint8_t reserved2;
    uint8_t section_id;
    uint8_t char_class;
  } cmd;

  cmd.subcommand = 0x06;
  cmd.subsize = 0x43;
  cmd.unused = 0x0000;
  cmd.reserved1 = 1;
  cmd.reserved2 = 1;

  cmd.serial_number = source->license->serial_number;
  char16cpy(cmd.name, source->player.disp.name, 0x18);
  remove_language_marker_inplace(cmd.name);
  char16cpy(cmd.team_name, source->player.team_name, 0x10);
  remove_language_marker_inplace(cmd.team_name);
  char16cpy(cmd.desc, source->player.guild_card_desc, 0x58);
  cmd.section_id = source->player.disp.section_id;
  cmd.char_class = source->player.disp.char_class;

  send_command(c, 0x62, c->lobby_client_id, cmd);
}

void send_guild_card(shared_ptr<Client> c, shared_ptr<Client> source) {
  if (c->version == GameVersion::GC) {
    send_guild_card_gc(c, source);
  } else if (c->version == GameVersion::BB) {
    send_guild_card_bb(c, source);
  } else {
    throw logic_error("unimplemented versioned command");
  }
}



////////////////////////////////////////////////////////////////////////////////
// menus

static void send_menu_pc_bb(shared_ptr<Client> c, const char16_t* menu_name,
    uint32_t menu_id, const vector<MenuItem>& items, bool is_info_menu) {
  struct Entry {
    uint32_t menu_id;
    uint32_t item_id;
    uint16_t flags; // should be 0x0F04
    char16_t text[17];
  };

  vector<Entry> entries;
  entries.emplace_back();
  {
    auto& entry = entries.back();
    entry.menu_id = menu_id;
    entry.item_id = 0xFFFFFFFF;
    entry.flags = 0x0004;
    char16cpy(entry.text, menu_name, 17);
  }

  for (const auto& item : items) {
    if ((c->version == GameVersion::BB) && (item.flags & MenuItemFlag::InvisibleOnBB)) {
      continue;
    }
    if ((c->version == GameVersion::PC) && (item.flags & MenuItemFlag::InvisibleOnPC)) {
      continue;
    }
    if ((item.flags & MenuItemFlag::RequiresMessageBoxes) &&
        (c->flags & ClientFlag::NoMessageBoxCloseConfirmation)) {
      continue;
    }

    entries.emplace_back();
    auto& entry = entries.back();
    entry.menu_id = menu_id;
    entry.item_id = item.item_id;
    entry.flags = (c->version == GameVersion::BB) ? 0x0004 : 0x0F04;
    char16cpy(entry.text, item.name.c_str(), 17);
  }

  send_command(c, is_info_menu ? 0x1F : 0x07, entries.size() - 1, entries);
}

static void send_menu_dc_gc(shared_ptr<Client> c, const char16_t* menu_name,
    uint32_t menu_id, const vector<MenuItem>& items, bool is_info_menu) {
  struct Entry {
    uint32_t menu_id;
    uint32_t item_id;
    uint16_t flags; // should be 0x0F04
    char text[18];
  };

  vector<Entry> entries;
  entries.emplace_back();
  {
    auto& entry = entries.back();
    entry.menu_id = menu_id;
    entry.item_id = 0xFFFFFFFF;
    entry.flags = 0x0004;
    encode_sjis(entry.text, menu_name, 18);
  }

  for (const auto& item : items) {
    if ((c->version == GameVersion::DC) && (item.flags & MenuItemFlag::InvisibleOnDC)) {
      continue;
    }
    if ((c->version == GameVersion::GC) && (item.flags & MenuItemFlag::InvisibleOnGC)) {
      continue;
    }
    if ((c->flags & ClientFlag::Episode3Games) && (item.flags & MenuItemFlag::InvisibleOnGCEpisode3)) {
      continue;
    }
    if ((item.flags & MenuItemFlag::RequiresMessageBoxes) &&
        (c->flags & ClientFlag::NoMessageBoxCloseConfirmation)) {
      continue;
    }

    entries.emplace_back();
    auto& entry = entries.back();
    entry.menu_id = menu_id;
    entry.item_id = item.item_id;
    entry.flags = 0x0F04;
    encode_sjis(entry.text, item.name.c_str(), 18);
  }

  send_command(c, is_info_menu ? 0x1F : 0x07, entries.size() - 1, entries);
}

void send_menu(shared_ptr<Client> c, const char16_t* menu_name,
    uint32_t menu_id, const vector<MenuItem>& items, bool is_info_menu) {
  if (c->version == GameVersion::PC || c->version == GameVersion::Patch ||
      c->version == GameVersion::BB) {
    send_menu_pc_bb(c, menu_name, menu_id, items, is_info_menu);
  } else {
    send_menu_dc_gc(c, menu_name, menu_id, items, is_info_menu);
  }
}

////////////////////////////////////////////////////////////////////////////////
// CommandGameSelect: presents the player with a Game Select menu. returns the selection in the same way as CommandShipSelect.

static void send_game_menu_pc(shared_ptr<Client> c, shared_ptr<ServerState> s) {
  struct Entry {
    uint32_t menu_id;
    uint32_t game_id;
    uint8_t difficulty_tag; // (s->teams[x]->episode == 0xFF ? 0x0A : s->teams[x]->difficulty + 0x22);
    uint8_t num_players;
    char16_t name[0x10];
    uint8_t episode;
    uint8_t flags;
  };

  vector<Entry> entries;
  {
    entries.emplace_back();
    auto& e = entries.back();
    e.menu_id = GAME_MENU_ID;
    e.game_id = 0;
    char16cpy(e.name, s->name.c_str(), 0x10);
  }
  for (shared_ptr<Lobby> l : s->all_lobbies()) {
    if (!l->is_game()) {
      continue;
    }
    if (l->version != c->version) {
      continue;
    }

    entries.emplace_back();
    auto& e = entries.back();
    e.menu_id = GAME_MENU_ID;

    e.game_id = l->lobby_id;
    e.difficulty_tag = l->difficulty + 0x22;
    e.num_players = l->count_clients();
    e.episode = 0;
    e.flags = (l->mode << 4) | (l->password[0] ? 2 : 0);
    char16cpy(e.name, l->name, 0x10);
  }

  send_command(c, 0x08, entries.size() - 1, entries);
}

static void send_game_menu_gc(shared_ptr<Client> c, shared_ptr<ServerState> s) {
  struct Entry {
    uint32_t menu_id;
    uint32_t game_id;
    uint8_t difficulty_tag; // (s->teams[x]->episode == 0xFF ? 0x0A : s->teams[x]->difficulty + 0x22);
    uint8_t num_players;
    char name[0x10];
    uint8_t episode;
    uint8_t flags;
  };

  vector<Entry> entries;
  {
    entries.emplace_back();
    auto& e = entries.back();
    e.menu_id = GAME_MENU_ID;
    e.game_id = 0;
    encode_sjis(e.name, s->name.c_str(), 0x10);
    e.flags = 0x0004;
  }
  for (shared_ptr<Lobby> l : s->all_lobbies()) {
    if (!l->is_game()) {
      continue;
    }
    if (l->version != c->version) {
      continue;
    }

    entries.emplace_back();
    auto& e = entries.back();
    e.menu_id = GAME_MENU_ID;

    e.game_id = l->lobby_id;
    e.difficulty_tag = ((l->flags & LobbyFlag::Episode3) ? 0x0A : (l->difficulty + 0x22));
    e.num_players = l->count_clients();
    e.episode = 0;
    if (l->flags & LobbyFlag::Episode3) {
      e.flags = (l->password[0] ? 2 : 0);
    } else {
      e.flags = ((l->episode << 6) | (l->mode << 4) | (l->password[0] ? 2 : 0));
    }
    encode_sjis(e.name, l->name, 0x10);
  }

  send_command(c, 0x08, entries.size() - 1, entries);
}

static void send_game_menu_bb(shared_ptr<Client> c, shared_ptr<ServerState> s) {
  struct Entry {
    uint32_t menu_id;
    uint32_t game_id;
    uint8_t difficulty_tag; // (s->teams[x]->episode == 0xFF ? 0x0A : s->teams[x]->difficulty + 0x22);
    uint8_t num_players;
    char16_t name[0x10];
    uint8_t episode;
    uint8_t flags;
  };

  vector<Entry> entries;
  {
    entries.emplace_back();
    auto& e = entries.back();
    e.menu_id = GAME_MENU_ID;
    e.game_id = 0;
    e.flags = 0x0004;
    char16cpy(e.name, s->name.c_str(), 0x10);
  }
  for (shared_ptr<Lobby> l : s->all_lobbies()) {
    if (!l->is_game()) {
      continue;
    }
    if (l->version != c->version) {
      continue;
    }

    entries.emplace_back();
    auto& e = entries.back();
    e.menu_id = GAME_MENU_ID;

    e.game_id = l->lobby_id;
    e.difficulty_tag = l->difficulty + 0x22;
    e.num_players = l->count_clients();
    e.episode = (l->max_clients << 4) | l->episode;
    e.flags = ((l->mode % 3) << 4) | (l->password[0] ? 2 : 0) | ((l->mode == 3) ? 4 : 0);
    char16cpy(e.name, l->name, 0x10);
  }

  send_command(c, 0x08, entries.size() - 1, entries);
}

void send_game_menu(shared_ptr<Client> c, shared_ptr<ServerState> s) {
  if (c->version == GameVersion::PC) {
    send_game_menu_pc(c, s);
  } else if (c->version == GameVersion::GC) {
    send_game_menu_gc(c, s);
  } else if (c->version == GameVersion::BB) {
    send_game_menu_bb(c, s);
  } else {
    throw logic_error("unimplemented versioned command");
  }
}



////////////////////////////////////////////////////////////////////////////////
// CommandQuestSelect: presents the user with a quest select menu based on a quest list.

static void send_quest_menu_pc(shared_ptr<Client> c, uint32_t menu_id,
    const vector<shared_ptr<const Quest>>& quests, bool is_download_menu) {
  struct Entry {
    uint32_t menu_id;
    uint32_t quest_id;
    char16_t name[0x20];
    char16_t short_desc[0x70];
  };

  vector<Entry> entries;
  for (const auto& quest : quests) {
    entries.emplace_back();
    auto& e = entries.back();
    e.menu_id = menu_id;
    e.quest_id = quest->quest_id;
    char16cpy(e.name, quest->name.c_str(), 0x20);
    char16cpy(e.short_desc, quest->short_description.c_str(), 0x70);
    add_color_inplace(e.short_desc);
  }

  send_command(c, is_download_menu ? 0xA4 : 0xA2, entries.size(), entries);
}

static void send_quest_menu_pc(std::shared_ptr<Client> c, uint32_t menu_id,
    const std::vector<MenuItem>& items, bool is_download_menu) {
  struct Entry {
    uint32_t menu_id;
    uint32_t item_id;
    char16_t name[0x20];
    char16_t short_desc[0x70];
  };

  vector<Entry> entries;
  for (const auto& item : items) {
    entries.emplace_back();
    auto& e = entries.back();
    e.menu_id = menu_id;
    e.item_id = item.item_id;
    char16cpy(e.name, item.name.c_str(), 0x20);
    char16cpy(e.short_desc, item.description.c_str(), 0x70);
    add_color_inplace(e.short_desc);
  }

  send_command(c, is_download_menu ? 0xA4 : 0xA2, entries.size(), entries);
}

static void send_quest_menu_gc(shared_ptr<Client> c, uint32_t menu_id,
    const vector<shared_ptr<const Quest>>& quests, bool is_download_menu) {
  struct Entry {
    uint32_t menu_id;
    uint32_t quest_id;
    char name[0x20];
    char short_desc[0x70];
  };

  vector<Entry> entries;
  for (const auto& quest : quests) {
    entries.emplace_back();
    auto& e = entries.back();
    e.menu_id = menu_id;
    e.quest_id = quest->quest_id;
    encode_sjis(e.name, quest->name.c_str(), 0x20);
    encode_sjis(e.short_desc, quest->short_description.c_str(), 0x70);
    add_color_inplace(e.short_desc);
  }

  send_command(c, is_download_menu ? 0xA4 : 0xA2, entries.size(), entries);
}

static void send_quest_menu_gc(shared_ptr<Client> c, uint32_t menu_id,
    const std::vector<MenuItem>& items, bool is_download_menu) {
  struct Entry {
    uint32_t menu_id;
    uint32_t item_id;
    char name[0x20];
    char short_desc[0x70];
  };

  vector<Entry> entries;
  for (const auto& item : items) {
    entries.emplace_back();
    auto& e = entries.back();
    e.menu_id = menu_id;
    e.item_id = item.item_id;
    encode_sjis(e.name, item.name.c_str(), 0x20);
    encode_sjis(e.short_desc, item.description.c_str(), 0x70);
    add_color_inplace(e.short_desc);
  }

  send_command(c, is_download_menu ? 0xA4 : 0xA2, entries.size(), entries);
}

static void send_quest_menu_bb(shared_ptr<Client> c, uint32_t menu_id,
    const vector<shared_ptr<const Quest>>& quests, bool is_download_menu) {
  // yet again sega does something inexplicable: the description is 10 chars
  // longer than on pc, necessitating a separate function here for BB
  struct Entry {
    uint32_t menu_id;
    uint32_t quest_id;
    char16_t name[0x20];
    char16_t short_desc[0x7A];
  };

  vector<Entry> entries;
  for (const auto& quest : quests) {
    entries.emplace_back();
    auto& e = entries.back();
    e.menu_id = menu_id;
    e.quest_id = quest->quest_id;
    char16cpy(e.name, quest->name.c_str(), 0x20);
    char16cpy(e.short_desc, quest->short_description.c_str(), 0x7A);
    add_color_inplace(e.short_desc);
  }

  send_command(c, is_download_menu ? 0xA4 : 0xA2, entries.size(), entries);
}

static void send_quest_menu_bb(shared_ptr<Client> c, uint32_t menu_id,
    const std::vector<MenuItem>& items, bool is_download_menu) {
  // yet again sega does something inexplicable: the description is 10 chars
  // longer than on pc, necessitating a separate function here for BB
  struct Entry {
    uint32_t menu_id;
    uint32_t item_id;
    char16_t name[0x20];
    char16_t short_desc[0x7A];
  };

  vector<Entry> entries;
  for (const auto& item : items) {
    entries.emplace_back();
    auto& e = entries.back();
    e.menu_id = menu_id;
    e.item_id = item.item_id;
    char16cpy(e.name, item.name.c_str(), 0x20);
    char16cpy(e.short_desc, item.description.c_str(), 0x7A);
    add_color_inplace(e.short_desc);
  }

  send_command(c, is_download_menu ? 0xA4 : 0xA2, entries.size(), entries);
}

void send_quest_menu(shared_ptr<Client> c, uint32_t menu_id,
    const vector<shared_ptr<const Quest>>& quests, bool is_download_menu) {
  if (c->version == GameVersion::PC) {
    send_quest_menu_pc(c, menu_id, quests, is_download_menu);
  } else if (c->version == GameVersion::GC) {
    send_quest_menu_gc(c, menu_id, quests, is_download_menu);
  } else if (c->version == GameVersion::BB) {
    send_quest_menu_bb(c, menu_id, quests, is_download_menu);
  } else {
    throw logic_error("unimplemented versioned command");
  }
}

void send_quest_menu(shared_ptr<Client> c, uint32_t menu_id,
    const std::vector<MenuItem>& items, bool is_download_menu) {
  if (c->version == GameVersion::PC) {
    send_quest_menu_pc(c, menu_id, items, is_download_menu);
  } else if (c->version == GameVersion::GC) {
    send_quest_menu_gc(c, menu_id, items, is_download_menu);
  } else if (c->version == GameVersion::BB) {
    send_quest_menu_bb(c, menu_id, items, is_download_menu);
  } else {
    throw logic_error("unimplemented versioned command");
  }
}

void send_lobby_list(shared_ptr<Client> c, shared_ptr<ServerState> s) {
  // This command appears to be deprecated, as PSO expects it to be exactly how
  // this server sends it, and does not react if it's different, except by
  // changing the lobby IDs.

  struct Entry {
    uint32_t menu_id;
    uint32_t item_id;
    uint32_t unused; // should be 0x00000000
  };

  vector<Entry> entries;
  for (shared_ptr<Lobby> l : s->all_lobbies()) {
    if (!(l->flags & LobbyFlag::Default)) {
      continue;
    }
    if ((l->flags & LobbyFlag::Episode3) && !(c->flags & ClientFlag::Episode3Games)) {
      continue;
    }

    entries.emplace_back();
    auto& e = entries.back();
    e.menu_id = LOBBY_MENU_ID;
    e.item_id = l->lobby_id;
    e.unused = 0;
  }

  send_command(c, 0x83, entries.size(), entries);
}



////////////////////////////////////////////////////////////////////////////////
// lobby joining

static void send_join_game_pc(shared_ptr<Client> c, shared_ptr<Lobby> l) {
  struct {
    uint32_t variations[0x20];
    PlayerLobbyDataPC lobby_data[4];
    uint8_t client_id;
    uint8_t leader_id;
    uint8_t unused;
    uint8_t difficulty;
    uint8_t battle_mode;
    uint8_t event;
    uint8_t section_id;
    uint8_t challenge_mode;
    uint32_t rare_seed;
    uint8_t episode;
    uint8_t unused2;
    uint8_t solo_mode;
    uint8_t unused3;
  } cmd;

  size_t player_count = 0;
  memcpy(cmd.variations, l->variations, sizeof(cmd.variations));
  for (size_t x = 0; x < 4; x++) {
    if (!l->clients[x]) {
      memset(&cmd.lobby_data[x], 0, sizeof(PlayerLobbyDataPC));
    } else {
      cmd.lobby_data[x].player_tag = 0x00010000;
      cmd.lobby_data[x].guild_card = l->clients[x]->license->serial_number;
      cmd.lobby_data[x].ip_address = 0x00000000;
      cmd.lobby_data[x].client_id = c->lobby_client_id;
      char16cpy(cmd.lobby_data[x].name, l->clients[x]->player.disp.name, 0x10);
      player_count++;
    }
  }

  cmd.client_id = c->lobby_client_id;
  cmd.leader_id = l->leader_id;
  cmd.unused = 0x00;
  cmd.difficulty = l->difficulty;
  cmd.battle_mode = (l->mode == 1) ? 1 : 0;
  cmd.event = l->event;
  cmd.section_id = l->section_id;
  cmd.challenge_mode = (l->mode == 2) ? 1 : 0;
  cmd.rare_seed = l->rare_seed;
  cmd.episode = 0x00;
  cmd.unused2 = 0x01;
  cmd.solo_mode = 0x00;
  cmd.unused3 = 0x00;

  send_command(c, 0x64, player_count, cmd);
}

static void send_join_game_gc(shared_ptr<Client> c, shared_ptr<Lobby> l) {
  struct {
    uint32_t variations[0x20];
    PlayerLobbyDataGC lobby_data[4];
    uint8_t client_id;
    uint8_t leader_id;
    uint8_t disable_udp; // guess; putting 0 here causes no movement messages to be sent
    uint8_t difficulty;
    uint8_t battle_mode;
    uint8_t event;
    uint8_t section_id;
    uint8_t challenge_mode;
    uint32_t rare_seed;
    uint32_t episode; // for PSOPC, this must be 0x00000100
    struct {
      PlayerInventory inventory;
      PlayerDispDataPCGC disp;
    } player[4]; // only used on ep3
  } cmd;

  size_t player_count = 0;
  memcpy(cmd.variations, l->variations, sizeof(cmd.variations));
  for (size_t x = 0; x < 4; x++) {
    if (!l->clients[x]) {
      memset(&cmd.lobby_data[x], 0, sizeof(PlayerLobbyDataGC));
    } else {
      cmd.lobby_data[x].player_tag = 0x00010000;
      cmd.lobby_data[x].guild_card = l->clients[x]->license->serial_number;
      cmd.lobby_data[x].ip_address = 0x00000000;
      cmd.lobby_data[x].client_id = c->lobby_client_id;
      encode_sjis(cmd.lobby_data[x].name, l->clients[x]->player.disp.name, 0x10);
      if (l->flags & LobbyFlag::Episode3) {
        cmd.player[x].inventory = l->clients[x]->player.inventory;
        cmd.player[x].disp = l->clients[x]->player.disp.to_pcgc();
      }
      player_count++;
    }
  }

  cmd.client_id = c->lobby_client_id;
  cmd.leader_id = l->leader_id;
  cmd.disable_udp = 0x01;
  cmd.difficulty = l->difficulty;
  cmd.battle_mode = (l->mode == 1) ? 1 : 0;
  cmd.event = l->event;
  cmd.section_id = l->section_id;
  cmd.challenge_mode = (l->mode == 2) ? 1 : 0;
  cmd.rare_seed = l->rare_seed;
  cmd.episode = l->episode;

  // player is only sent in ep3 games
  size_t data_size = (l->flags & LobbyFlag::Episode3) ? 0x1184 : 0x0110;
  send_command(c, 0x64, player_count, &cmd, data_size);
}

static void send_join_game_bb(shared_ptr<Client> c, shared_ptr<Lobby> l) {
  struct {
    uint32_t variations[0x20];
    PlayerLobbyDataBB lobby_data[4];
    uint8_t client_id;
    uint8_t leader_id;
    uint8_t unused;
    uint8_t difficulty;
    uint8_t battle_mode;
    uint8_t event;
    uint8_t section_id;
    uint8_t challenge_mode;
    uint32_t rare_seed;
    uint8_t episode;
    uint8_t unused2;
    uint8_t solo_mode;
    uint8_t unused3;
  } cmd;

  size_t player_count = 0;
  memcpy(cmd.variations, l->variations, sizeof(cmd.variations));
  for (size_t x = 0; x < 4; x++) {
    memset(&cmd.lobby_data[x], 0, sizeof(PlayerLobbyDataBB));
    if (l->clients[x]) {
      cmd.lobby_data[x].player_tag = 0x00010000;
      cmd.lobby_data[x].guild_card = l->clients[x]->license->serial_number;
      cmd.lobby_data[x].client_id = c->lobby_client_id;
      char16cpy(cmd.lobby_data[x].name, l->clients[x]->player.disp.name, 0x10);
      player_count++;
    }
  }

  cmd.client_id = c->lobby_client_id;
  cmd.leader_id = l->leader_id;
  cmd.unused = 0x00;
  cmd.difficulty = l->difficulty;
  cmd.battle_mode = (l->mode == 1) ? 1 : 0;
  cmd.event = l->event;
  cmd.section_id = l->section_id;
  cmd.challenge_mode = (l->mode == 2) ? 1 : 0;
  cmd.rare_seed = l->rare_seed;
  cmd.episode = 0x00;
  cmd.unused2 = 0x01;
  cmd.solo_mode = 0x00;
  cmd.unused3 = 0x00;

  send_command(c, 0x64, player_count, cmd);
}

static void send_join_lobby_pc(shared_ptr<Client> c, shared_ptr<Lobby> l) {
  uint8_t lobby_type = (l->type > 14) ? (l->block - 1) : l->type;
  struct {
    uint8_t client_id;
    uint8_t leader_id;
    uint8_t disable_udp;
    uint8_t lobby_number;
    uint16_t block_number;
    uint16_t event;
    uint32_t unused;
  } cmd = {
    c->lobby_client_id,
    l->leader_id,
    0x01,
    lobby_type,
    l->block,
    l->event,
    0x00000000,
  };

  struct Entry {
    PlayerLobbyDataPC lobby_data;
    PlayerLobbyJoinDataPCGC data;
  };
  vector<Entry> entries;

  for (size_t x = 0; x < l->max_clients; x++) {
    if (!l->clients[x]) {
      continue;
    }

    entries.emplace_back();
    auto& e = entries.back();

    e.lobby_data.player_tag = 0x00010000;
    e.lobby_data.guild_card = l->clients[x]->license->serial_number;
    e.lobby_data.ip_address = 0x00000000;
    e.lobby_data.client_id = l->clients[x]->lobby_client_id;
    char16cpy(e.lobby_data.name, l->clients[x]->player.disp.name, 0x10);
    e.data = l->clients[x]->player.export_lobby_data_pc();
  }

  send_command(c, 0x67, entries.size(), cmd, entries);
}

static void send_join_lobby_gc(shared_ptr<Client> c, shared_ptr<Lobby> l) {
  uint8_t lobby_type = l->type;
  if (c->flags & ClientFlag::Episode3Games) {
    if ((l->type > 0x14) && (l->type < 0xE9)) {
      lobby_type = l->block - 1;
    }
  } else {
    if ((l->type > 0x11) && (l->type != 0x67) && (l->type != 0xD4) && (l->type < 0xFC)) {
      lobby_type = l->block - 1;
    }
  }

  struct {
    uint8_t client_id;
    uint8_t leader_id;
    uint8_t disable_udp;
    uint8_t lobby_number;
    uint16_t block_number;
    uint16_t event;
    uint32_t unused;
  } cmd = {
    c->lobby_client_id,
    l->leader_id,
    0x01,
    lobby_type,
    l->block,
    l->event,
    0x00000000,
  };

  struct Entry {
    PlayerLobbyDataGC lobby_data;
    PlayerLobbyJoinDataPCGC data;
  };
  vector<Entry> entries;

  for (size_t x = 0; x < l->max_clients; x++) {
    if (!l->clients[x]) {
      continue;
    }

    entries.emplace_back();
    auto& e = entries.back();

    e.lobby_data.player_tag = 0x00010000;
    e.lobby_data.guild_card = l->clients[x]->license->serial_number;
    e.lobby_data.ip_address = 0x00000000;
    e.lobby_data.client_id = l->clients[x]->lobby_client_id;
    encode_sjis(e.lobby_data.name, l->clients[x]->player.disp.name, 0x10);
    e.data = l->clients[x]->player.export_lobby_data_gc();
  }

  send_command(c, 0x67, entries.size(), cmd, entries);
}

static void send_join_lobby_bb(shared_ptr<Client> c, shared_ptr<Lobby> l) {
  uint8_t lobby_type = (l->type > 14) ? (l->block - 1) : l->type;
  struct {
    uint8_t client_id;
    uint8_t leader_id;
    uint8_t disable_udp;
    uint8_t lobby_number;
    uint16_t block_number;
    uint16_t event;
    uint32_t unused;
  } cmd = {
    c->lobby_client_id,
    l->leader_id,
    0x01,
    lobby_type,
    l->block,
    l->event,
    0x00000000,
  };

  struct Entry {
    PlayerLobbyDataBB lobby_data;
    PlayerLobbyJoinDataBB data;
  };
  vector<Entry> entries;

  for (size_t x = 0; x < l->max_clients; x++) {
    if (!l->clients[x]) {
      continue;
    }

    entries.emplace_back();
    auto& e = entries.back();
    memset(&e.lobby_data, 0, sizeof(e.lobby_data));

    e.lobby_data.player_tag = 0x00010000;
    e.lobby_data.guild_card = l->clients[x]->license->serial_number;
    e.lobby_data.client_id = l->clients[x]->lobby_client_id;
    char16cpy(e.lobby_data.name, l->clients[x]->player.disp.name, 0x10);
    e.data = l->clients[x]->player.export_lobby_data_bb();
  }

  send_command(c, 0x67, entries.size(), cmd, entries);
}

void send_join_lobby(shared_ptr<Client> c, shared_ptr<Lobby> l) {
  if (l->is_game()) {
    if (c->version == GameVersion::PC) {
      send_join_game_pc(c, l);
    } else if (c->version == GameVersion::GC) {
      send_join_game_gc(c, l);
    } else if (c->version == GameVersion::BB) {
      send_join_game_bb(c, l);
    } else {
      throw logic_error("unimplemented versioned command");
    }

  } else {
    if (c->version == GameVersion::PC) {
      send_join_lobby_pc(c, l);
    } else if (c->version == GameVersion::GC) {
      send_join_lobby_gc(c, l);
    } else if (c->version == GameVersion::BB) {
      send_join_lobby_bb(c, l);
    } else {
      throw logic_error("unimplemented versioned command");
    }
  }

  // If the client will stop sending message box close confirmations after
  // joining any lobby, set the appropriate flag and update the client config
  if ((c->flags & (ClientFlag::NoMessageBoxCloseConfirmationAfterLobbyJoin | ClientFlag::NoMessageBoxCloseConfirmation))
      == ClientFlag::NoMessageBoxCloseConfirmationAfterLobbyJoin) {
    c->flags |= ClientFlag::NoMessageBoxCloseConfirmation;
    send_update_client_config(c);
  }
}



////////////////////////////////////////////////////////////////////////////////
// CommandLobbyAddPlayer: notifies all players in a lobby that a new player is joining.
// this command, unlike the previous, is virtually the same between games and lobbies.

static void send_player_join_notification_pc(shared_ptr<Client> c,
    shared_ptr<Lobby> l, shared_ptr<Client> joining_client) {
  uint8_t lobby_type = (l->type > 14) ? (l->block - 1) : l->type;
  struct {
    uint8_t client_id;
    uint8_t leader_id;
    uint8_t disable_udp;
    uint8_t lobby_number;
    uint16_t block_number;
    uint16_t event;
    uint32_t unused;
    PlayerLobbyDataPC lobby_data;
    PlayerLobbyJoinDataPCGC data;
  } cmd = {
    0xFF,
    l->leader_id,
    0x01,
    lobby_type,
    l->block,
    l->event,
    0x00000000,
    {0x00000100, joining_client->license->serial_number, 0x00000000, joining_client->lobby_client_id, {0}},
    joining_client->player.export_lobby_data_pc(),
  };
  char16cpy(cmd.lobby_data.name, joining_client->player.disp.name, 0x10);

  send_command(c, l->is_game() ? 0x65 : 0x68, 0x01, cmd);
}

static void send_player_join_notification_gc(shared_ptr<Client> c,
    shared_ptr<Lobby> l, shared_ptr<Client> joining_client) {
  uint8_t lobby_type = (l->type > 14) ? (l->block - 1) : l->type;
  struct {
    uint8_t client_id;
    uint8_t leader_id;
    uint8_t disable_udp;
    uint8_t lobby_number;
    uint16_t block_number;
    uint16_t event;
    uint32_t unused;
    PlayerLobbyDataGC lobby_data;
    PlayerLobbyJoinDataPCGC data;
  } cmd = {
    0xFF,
    l->leader_id,
    0x01,
    lobby_type,
    l->block,
    l->event,
    0x00000000,
    {0x00000100, joining_client->license->serial_number, 0x00000000, joining_client->lobby_client_id, {0}},
    joining_client->player.export_lobby_data_gc(),
  };
  encode_sjis(cmd.lobby_data.name, joining_client->player.disp.name, 0x10);

  send_command(c, l->is_game() ? 0x65 : 0x68, 0x01, cmd);
}

static void send_player_join_notification_bb(shared_ptr<Client> c,
    shared_ptr<Lobby> l, shared_ptr<Client> joining_client) {
  uint8_t lobby_type = (l->type > 14) ? (l->block - 1) : l->type;
  struct {
    uint8_t client_id;
    uint8_t leader_id;
    uint8_t disable_udp;
    uint8_t lobby_number;
    uint16_t block_number;
    uint16_t event;
    uint32_t unused;
    PlayerLobbyDataBB lobby_data;
    PlayerLobbyJoinDataBB data;
  } cmd = {
    0xFF,
    l->leader_id,
    0x01,
    lobby_type,
    l->block,
    l->event,
    0x00000000,
    {},
    joining_client->player.export_lobby_data_bb(),
  };
  memset(&cmd.lobby_data, 0, sizeof(cmd.lobby_data));
  cmd.lobby_data.player_tag = 0x00010000;
  cmd.lobby_data.guild_card = joining_client->license->serial_number;
  cmd.lobby_data.client_id = joining_client->lobby_client_id;
  char16cpy(cmd.lobby_data.name, joining_client->player.disp.name, 0x10);

  send_command(c, l->is_game() ? 0x65 : 0x68, 0x01, cmd);
}

void send_player_join_notification(shared_ptr<Client> c, shared_ptr<Lobby> l,
    shared_ptr<Client> joining_client) {
  if (c->version == GameVersion::PC) {
    send_player_join_notification_pc(c, l, joining_client);
  } else if (c->version == GameVersion::GC) {
    send_player_join_notification_gc(c, l, joining_client);
  } else if (c->version == GameVersion::BB) {
    send_player_join_notification_bb(c, l, joining_client);
  } else {
    throw logic_error("unimplemented versioned command");
  }
}

void send_player_leave_notification(shared_ptr<Lobby> l, uint8_t leaving_client_id) {
  struct {
    uint8_t client_id;
    uint8_t leader_id;
    uint16_t unused;
  } cmd = {leaving_client_id, l->leader_id, 0};
  send_command(l, l->is_game() ? 0x66 : 0x69, leaving_client_id, cmd);
}

void send_get_player_info(shared_ptr<Client> c) {
  send_command(c, 0x95);
}



////////////////////////////////////////////////////////////////////////////////
// arrows

void send_arrow_update(shared_ptr<Lobby> l) {
  struct Entry {
    uint32_t player_tag;
    uint32_t serial_number;
    uint32_t arrow_color;
  };
  vector<Entry> entries;

  for (size_t x = 0; x < l->max_clients; x++) {
    if (!l->clients[x]) {
      continue;
    }

    entries.emplace_back();
    auto& e = entries.back();
    e.player_tag = 0x00010000;
    e.serial_number = l->clients[x]->license->serial_number;
    e.arrow_color = l->clients[x]->lobby_arrow_color;
  }

  send_command(l, 0x88, entries.size(), entries);
}

// tells the player that the joining player is done joining, and the game can resume
void send_resume_game(shared_ptr<Lobby> l, shared_ptr<Client> ready_client) {
  uint32_t data = 0x081C0372;
  send_command_excluding_client(l, ready_client, 0x60, 0x00, &data, 4);
}



////////////////////////////////////////////////////////////////////////////////
// Game/cheat commands

// sends an HP/TP/Meseta modifying command (see flag definitions in command-functions.h)
void send_player_stats_change(shared_ptr<Lobby> l, shared_ptr<Client> c,
    PlayerStatsChange stat, uint32_t amount) {

  if (amount > 2550) {
    throw invalid_argument("amount cannot be larger than 2550");
  }

  vector<PSOSubcommand> subs;
  while (amount > 0) {
    {
      subs.emplace_back();
      auto& sub = subs.back();
      sub.byte[0] = 0x9A;
      sub.byte[1] = 0x02;
      sub.byte[2] = c->lobby_client_id;
      sub.byte[3] = 0x00;
    }
    {
      subs.emplace_back();
      auto& sub = subs.back();
      sub.byte[0] = 0x00;
      sub.byte[1] = 0x00;
      sub.byte[2] = stat;
      sub.byte[3] = (amount > 0xFF) ? 0xFF : amount;
      amount -= sub.byte[3];
    }
  }

  send_command(l, 0x60, 0x00, subs);
}

void send_warp(shared_ptr<Client> c, uint32_t area) {
  PSOSubcommand cmds[2];
  cmds[0].byte[0] = 0x94;
  cmds[0].byte[1] = 0x02;
  cmds[0].byte[2] = c->lobby_client_id;
  cmds[0].byte[3] = 0x00;
  cmds[1].dword = area;
  send_command(c, 0x62, c->lobby_client_id, cmds, 8);
}

void send_ep3_change_music(shared_ptr<Client> c, uint32_t song) {
  PSOSubcommand cmds[2];
  cmds[0].byte[0] = 0xBF;
  cmds[0].byte[1] = 0x02;
  cmds[0].byte[2] = c->lobby_client_id;
  cmds[0].byte[3] = 0x00;
  cmds[1].dword = song;
  send_command(c, 0x60, 0x00, cmds, 8);
}

void send_set_player_visibility(shared_ptr<Lobby> l, shared_ptr<Client> c,
    bool visible) {
  PSOSubcommand cmd;
  cmd.byte[0] = visible ? 0x23 : 0x22;
  cmd.byte[1] = 0x01;
  cmd.byte[2] = c->lobby_client_id;
  cmd.byte[3] = 0x00;
  send_command(l, 0x60, 0x00, &cmd, 4);
}

void send_revive_player(shared_ptr<Lobby> l, shared_ptr<Client> c) {
  PSOSubcommand cmd;
  cmd.byte[0] = 0x31;
  cmd.byte[1] = 0x01;
  cmd.byte[2] = c->lobby_client_id;
  cmd.byte[3] = 0x00;
  send_command(l, 0x60, 0x00, &cmd, 4);
}



////////////////////////////////////////////////////////////////////////////////
// BB game commands

// notifies other players of a dropped item from a box or enemy
void send_drop_item(shared_ptr<Lobby> l, const ItemData& item,
    bool from_enemy, uint8_t area, float x, float y, uint16_t request_id) {
  struct {
    uint8_t subcommand;
    uint8_t subsize;
    uint16_t unused;
    uint8_t area;
    uint8_t dude;
    uint16_t request_id;
    float x;
    float y;
    uint32_t unused2;
    ItemData data;
  } cmd = {0x5F, 0x0A, 0x0000, area, from_enemy, request_id, x, y, 0, item};
  send_command(l, 0x60, 0x00, cmd);
}

// notifies other players that a stack was split and part of it dropped (a new item was created)
void send_drop_stacked_item(shared_ptr<Lobby> l, const ItemData& item,
    uint8_t area, float x, float y) {
  struct {
    uint8_t subcommand;
    uint8_t subsize;
    uint16_t unused;
    uint16_t area;
    uint16_t unused2;
    float x;
    float y;
    uint32_t unused3;
    ItemData data;
  } cmd = {0x5D, 0x09, 0x0000, area, 0, x, y, 0, item};
  send_command(l, 0x60, 0x00, cmd);
}

// notifies other players that an item was picked up
void send_pick_up_item(shared_ptr<Lobby> l, shared_ptr<Client> c,
    uint32_t item_id, uint8_t area) {
  struct {
    uint8_t subcommand;
    uint8_t subsize;
    uint16_t client_id;
    uint16_t client_id2;
    uint16_t area;
    uint32_t item_id;
  } cmd = {0x59, 0x03, c->lobby_client_id, c->lobby_client_id, area, item_id};
  send_command(l, 0x60, 0x00, cmd);
}

// creates an item in a player's inventory (used for withdrawing items from the bank)
void send_create_inventory_item(shared_ptr<Lobby> l, shared_ptr<Client> c,
    const ItemData& item) {
  struct {
    uint8_t subcommand;
    uint8_t subsize;
    uint16_t client_id;
    ItemData item;
    uint32_t unused;
  } cmd = {0xBE, 0x07, c->lobby_client_id, item, 0};
  send_command(l, 0x60, 0x00, cmd);
}

// destroys an item
void send_destroy_item(shared_ptr<Lobby> l, shared_ptr<Client> c,
    uint32_t item_id, uint32_t amount) {
  struct {
    uint8_t subcommand;
    uint8_t subsize;
    uint16_t client_id;
    uint32_t item_id;
    uint32_t amount;
  } cmd = {0x29, 0x03, c->lobby_client_id, item_id, amount};
  send_command(l, 0x60, 0x00, cmd);
}

// sends the player his/her bank data
void send_bank(shared_ptr<Client> c) {
  vector<PlayerBankItem> items(c->player.bank.items,
      &c->player.bank.items[c->player.bank.num_items]);

  uint32_t checksum = random_object<uint32_t>();
  struct {
    uint8_t subcommand;
    uint8_t unused1;
    uint16_t unused2;
    uint32_t size; // same as size in header (computed later)
    uint32_t checksum; // can be random; client won't notice
    uint32_t numItems;
    uint32_t meseta;
  } cmd = {0xBC, 0, 0, 0, checksum, c->player.bank.num_items, c->player.bank.meseta};

  size_t size = 8 + sizeof(cmd) + items.size() * sizeof(PlayerBankItem);
  cmd.size = size;

  send_command(c, 0x6C, 0x00, cmd, items);
}

// sends the player a shop's contents
void send_shop(shared_ptr<Client> c, uint8_t shop_type) {
  struct {
    uint8_t subcommand; // B6
    uint8_t size; // 2C regardless of the number of items??
    uint16_t params; // 037F
    uint8_t shop_type;
    uint8_t num_items;
    uint16_t unused;
    ItemData entries[20];
  } cmd = {
    0xB6,
    0x2C,
    0x037F,
    shop_type,
    static_cast<uint8_t>(c->player.current_shop_contents.size()),
    0,
    {},
  };

  size_t count = c->player.current_shop_contents.size();
  if (count > sizeof(cmd.entries) / sizeof(cmd.entries[0])) {
    throw logic_error("too many items in shop");
  }

  for (size_t x = 0; x < count; x++) {
    cmd.entries[x] = c->player.current_shop_contents[x];
  }

  send_command(c, 0x6C, 0x00, &cmd, sizeof(cmd) - sizeof(cmd.entries[0]) * (20 - count));
}

// notifies players about a level up
void send_level_up(shared_ptr<Lobby> l, shared_ptr<Client> c) {
  PlayerStats stats = c->player.disp.stats;

  for (size_t x = 0; x < c->player.inventory.num_items; x++) {
    if ((c->player.inventory.items[x].equip_flags & 0x08) &&
        (c->player.inventory.items[x].data.item_data1[0] == 0x02)) {
      stats.dfp += (c->player.inventory.items[x].data.item_data1w[2] / 100);
      stats.atp += (c->player.inventory.items[x].data.item_data1w[3] / 50);
      stats.ata += (c->player.inventory.items[x].data.item_data1w[4] / 200);
      stats.mst += (c->player.inventory.items[x].data.item_data1w[5] / 50);
    }
  }

  PSOSubcommand sub[5];
  sub[0].byte[0] = 0x30;
  sub[0].byte[1] = 0x05;
  sub[0].word[1] = c->lobby_client_id;
  sub[1].word[0] = stats.atp;
  sub[1].word[1] = stats.mst;
  sub[2].word[0] = stats.evp;
  sub[2].word[1] = stats.hp;
  sub[3].word[0] = stats.dfp;
  sub[3].word[1] = stats.ata;
  sub[4].dword = c->player.disp.level;
  send_command(l, 0x60, 0x00, sub, 0x14);
}

// gives a player EXP
void send_give_experience(shared_ptr<Lobby> l, shared_ptr<Client> c,
    uint32_t amount) {
  PSOSubcommand sub[2];
  sub[0].word[0] = 0x02BF;
  sub[0].word[1] = c->lobby_client_id;
  sub[1].dword = amount;
  send_command(l, 0x60, 0x00, sub, 8);
}



////////////////////////////////////////////////////////////////////////////////
// ep3 only commands

// sends the (PRS-compressed) card list to the client
void send_ep3_card_list_update(shared_ptr<Client> c) {
  auto file_data = file_cache.get("system/ep3/cardupdate.mnr");

  string data("\0\0\0\0", 4);
  *reinterpret_cast<uint32_t*>(data.data()) = file_data->size();
  data += *file_data;
  data.resize((data.size() + 3) & ~3);

  send_command(c, 0xB8, 0x00, data);
}

// sends the client a generic rank
void send_ep3_rank_update(shared_ptr<Client> c) {
  struct {
    uint32_t rank;
    char rankText[0x0C];
    uint32_t meseta;
    uint32_t max_meseta;
    uint32_t jukebox_songs_unlocked;
  } cmd = {0, "\0\0\0\0\0\0\0\0\0\0\0", 0x00FFFFFF, 0x00FFFFFF, 0xFFFFFFFF};
  send_command(c, 0xB7, 0x00, cmd);
}

// sends the map list (used for battle setup) to all players in a game
void send_ep3_map_list(shared_ptr<Lobby> l) {
  auto file_data = file_cache.get("system/ep3/maplist.mnr");

  string data(16, '\0');
  PSOSubcommand* subs = reinterpret_cast<PSOSubcommand*>(data.data());
  subs[0].dword = 0x000000B6;
  subs[1].dword = (23 + file_data->size()) & 0xFFFFFFFC;
  subs[2].dword = 0x00000040;
  subs[3].dword = file_data->size();
  data += *file_data;

  send_command(l, 0x6C, 0x00, data);
}

// sends the map data for the chosen map to all players in the game
void send_ep3_map_data(shared_ptr<Lobby> l, uint32_t map_id) {
  string filename = string_printf("system/ep3/map%08" PRIX32 ".mnm", map_id);
  auto file_data = file_cache.get(filename);

  string data(12, '\0');
  PSOSubcommand* subs = reinterpret_cast<PSOSubcommand*>(data.data());
  subs[0].dword = 0x000000B6;
  subs[1].dword = (19 + file_data->size()) & 0xFFFFFFFC;
  subs[2].dword = 0x00000041;
  data += *file_data;

  send_command(l, 0x6C, 0x00, data);
}



////////////////////////////////////////////////////////////////////////////////
// CommandLoadQuestFile: sends a quest file to the client.
// the _OpenFile functions send the begin command (44/A6), and the _SendChunk functions send a chunk of data (13/A7).

static void send_quest_open_file_pc_gc(shared_ptr<Client> c,
    const string& filename, uint32_t file_size, bool is_download_quest,
    bool is_ep3_quest) {
  struct {
    char name[0x20];
    uint16_t unused;
    uint16_t flags;
    char filename[0x10];
    uint32_t file_size;
  } cmd;
  memset(&cmd, 0, sizeof(cmd));
  strncpy(cmd.name, filename.c_str(), 0x1F);
  cmd.flags = 2 + is_ep3_quest;
  strncpy(cmd.filename, filename.c_str(), 0x0F);
  cmd.file_size = file_size;
  send_command(c, is_download_quest ? 0xA6 : 0x44, 0x00, cmd);
}

static void send_quest_open_file_bb(shared_ptr<Client> c,
    const string& filename, uint32_t file_size, bool is_download_quest,
    bool is_ep3_quest) {
  struct {
    uint8_t unused[0x22];
    uint16_t flags;
    char filename[0x10];
    uint32_t file_size;
    char name[0x18];
  } cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.flags = 2 + is_ep3_quest;
  strncpy(cmd.filename, filename.c_str(), 0x0F);
  cmd.file_size = file_size;
  send_command(c, is_download_quest ? 0xA6 : 0x44, 0x00, cmd);
}

static void send_quest_file_chunk(shared_ptr<Client> c, const char* filename,
    size_t chunk_index, const void* data, size_t size, bool is_download_quest) {
  if (size > 0x400) {
    throw invalid_argument("quest file chunks must be 1KB or smaller");
  }

  struct {
    char filename[0x10];
    uint8_t data[0x400];
    uint32_t data_size;
  } cmd;
  memset(cmd.filename, 0, 0x10);
  strncpy(cmd.filename, filename, 0x0F);
  memcpy(cmd.data, data, size);
  if (size < 0x400) {
    memset(&cmd.data[size], 0, 0x400 - size);
  }
  cmd.data_size = size;

  send_command(c, is_download_quest ? 0xA7 : 0x13, chunk_index, cmd);
}

void send_quest_file(shared_ptr<Client> c, const string& basename,
    const string& contents, bool is_download_quest, bool is_ep3_quest) {

  if (c->version == GameVersion::PC || c->version == GameVersion::GC) {
    send_quest_open_file_pc_gc(c, basename, contents.size(), is_download_quest,
        is_ep3_quest);
  } else if (c->version == GameVersion::BB) {
    send_quest_open_file_bb(c, basename, contents.size(), is_download_quest,
        is_ep3_quest);
  } else {
    throw invalid_argument("cannot send quest files to this version of client");
  }

  for (size_t offset = 0; offset < contents.size(); offset += 0x400) {
    size_t chunk_bytes = contents.size() - offset;
    if (chunk_bytes > 0x400) {
      chunk_bytes = 0x400;
    }
    send_quest_file_chunk(c, basename.c_str(), offset / 0x400,
        contents.data() + offset, chunk_bytes, is_download_quest);
  }
}

void send_server_time(shared_ptr<Client> c) {
  uint64_t t = now();

  time_t t_secs = t / 1000000;
  struct tm t_parsed;
  gmtime_r(&t_secs, &t_parsed);

  string time_str(128, 0);
  size_t len = strftime(time_str.data(), time_str.size(),
      "%Y:%m:%d: %H:%M:%S.000", &t_parsed);
  if (len == 0) {
    throw runtime_error("format_time buffer too short");
  }
  time_str.resize(len);

  send_command(c, 0xB1, 0x00, time_str);
}

void send_change_event(shared_ptr<Client> c, uint8_t new_event) {
  send_command(c, 0xDA, new_event);
}

void send_change_event(shared_ptr<Lobby> l, uint8_t new_event) {
  send_command(l, 0xDA, new_event);
}

void send_change_event(shared_ptr<ServerState> s, uint8_t new_event) {
  send_command(s, 0xDA, new_event);
}

#pragma pack(pop)
