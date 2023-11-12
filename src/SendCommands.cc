#include "SendCommands.hh"

#include <event2/buffer.h>
#include <inttypes.h>
#include <string.h>

#include <memory>
#include <phosg/Encoding.hh>
#include <phosg/Hash.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>
#include <phosg/Time.hh>

#include "CommandFormats.hh"
#include "Compression.hh"
#include "FileContentsCache.hh"
#include "PSOProtocol.hh"
#include "StaticGameData.hh"
#include "Text.hh"

using namespace std;

extern const char* QUEST_BARRIER_DISCONNECT_HOOK_NAME;

const unordered_set<uint32_t> v2_crypt_initial_client_commands({
    0x00260088, // (17) DCNTE license check
    0x00B0008B, // (02) DCNTE login
    0x00B0018B, // (02) DCNTE login (UDP off)
    0x0114008B, // (02) DCNTE extended login
    0x0114018B, // (02) DCNTE extended login (UDP off)
    0x00260090, // (17) DCv1 prototype and JP license check
    0x00280090, // (17) DCv1 license check
    0x00B00093, // (02) DCv1 login
    0x00B00193, // (02) DCv1 login (UDP off)
    0x01140093, // (02) DCv1 extended login
    0x01140193, // (02) DCv1 extended login (UDP off)
    0x00E0009A, // (17) DCv2/GCNTE license check
    0x00CC009D, // (02) DCv2/GCNTE login
    0x00CC019D, // (02) DCv2/GCNTE login (UDP off)
    0x0130009D, // (02) DCv2/GCNTE extended login
    0x0130019D, // (02) DCv2/GCNTE extended login (UDP off)
    // Note: PSO PC initial commands are not listed here because we don't use a
    // detector encryption for PSO PC (instead, we use the split reconnect command
    // to send PC to a different port).
});
const unordered_set<uint32_t> v3_crypt_initial_client_commands({
    0x00E000DB, // (17) GC/XB license check
    0x00EC009E, // (02) GC login
    0x00EC019E, // (02) GC login (UDP off)
    0x0150009E, // (02) GC extended login
    0x0150019E, // (02) GC extended login (UDP off)
    0x0130009E, // (02) XB login
    0x0130019E, // (02) XB login (UDP off)
    0x0194009E, // (02) XB extended login
    0x0194019E, // (02) XB extended login (UDP off)
});

const unordered_set<string> bb_crypt_initial_client_commands({
    string("\xB4\x00\x93\x00\x00\x00\x00\x00", 8),
    string("\xAC\x00\x93\x00\x00\x00\x00\x00", 8),
    string("\xDC\x00\xDB\x00\x00\x00\x00\x00", 8),
});

void send_command(std::shared_ptr<Client> c, uint16_t command,
    uint32_t flag, const std::vector<std::pair<const void*, size_t>>& blocks) {
  c->channel.send(command, flag, blocks);
}

void send_command(shared_ptr<Client> c, uint16_t command, uint32_t flag,
    const void* data, size_t size) {
  c->channel.send(command, flag, data, size);
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

void send_command_if_not_loading(shared_ptr<Lobby> l,
    uint16_t command, uint32_t flag, const void* data, size_t size) {
  for (auto& client : l->clients) {
    if (!client || client->config.check_flag(Client::Flag::LOADING)) {
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

template <typename HeaderT>
void send_command_with_header_t(Channel& ch, const void* data, size_t size) {
  const HeaderT* header = reinterpret_cast<const HeaderT*>(data);
  ch.send(header->command, header->flag, header + 1, size - sizeof(HeaderT));
}

void send_command_with_header(Channel& ch, const void* data, size_t size) {
  switch (ch.version) {
    case GameVersion::DC:
    case GameVersion::GC:
    case GameVersion::XB:
      send_command_with_header_t<PSOCommandHeaderDCV3>(ch, data, size);
      break;
    case GameVersion::PC:
    case GameVersion::PATCH:
      send_command_with_header_t<PSOCommandHeaderPC>(ch, data, size);
      break;
    case GameVersion::BB:
      send_command_with_header_t<PSOCommandHeaderBB>(ch, data, size);
      break;
    default:
      throw logic_error("unimplemented game version in send_command_with_header");
  }
}

static const char* anti_copyright = "This server is in no way affiliated, sponsored, or supported by SEGA Enterprises or SONICTEAM. The preceding message exists only to remain compatible with programs that expect it.";
static const char* dc_port_map_copyright = "DreamCast Port Map. Copyright SEGA Enterprises. 1999";
static const char* dc_lobby_server_copyright = "DreamCast Lobby Server. Copyright SEGA Enterprises. 1999";
static const char* bb_game_server_copyright = "Phantasy Star Online Blue Burst Game Server. Copyright 1999-2004 SONICTEAM.";
static const char* bb_pm_server_copyright = "PSO NEW PM Server. Copyright 1999-2002 SONICTEAM.";
static const char* patch_server_copyright = "Patch Server. Copyright SonicTeam, LTD. 2001";

S_ServerInitWithAfterMessage_DC_PC_V3_02_17_91_9B<0xB4>
prepare_server_init_contents_console(
    uint32_t server_key, uint32_t client_key, uint8_t flags) {
  bool initial_connection = (flags & SendServerInitFlag::IS_INITIAL_CONNECTION);
  S_ServerInitWithAfterMessage_DC_PC_V3_02_17_91_9B<0xB4> cmd;
  cmd.basic_cmd.copyright.encode(initial_connection ? dc_port_map_copyright : dc_lobby_server_copyright);
  cmd.basic_cmd.server_key = server_key;
  cmd.basic_cmd.client_key = client_key;
  cmd.after_message.encode(anti_copyright);
  return cmd;
}

void send_server_init_dc_pc_v3(shared_ptr<Client> c, uint8_t flags) {
  bool initial_connection = (flags & SendServerInitFlag::IS_INITIAL_CONNECTION);
  uint8_t command = initial_connection ? 0x17 : 0x02;
  uint32_t server_key = random_object<uint32_t>();
  uint32_t client_key = random_object<uint32_t>();

  auto cmd = prepare_server_init_contents_console(
      server_key, client_key, initial_connection);
  send_command_t(c, command, 0x00, cmd);

  switch (c->version()) {
    case GameVersion::PC:
      c->channel.crypt_in.reset(new PSOV2Encryption(client_key));
      c->channel.crypt_out.reset(new PSOV2Encryption(server_key));
      break;
    case GameVersion::DC:
    case GameVersion::GC: {
      shared_ptr<PSOV2OrV3DetectorEncryption> det_crypt(new PSOV2OrV3DetectorEncryption(
          client_key, v2_crypt_initial_client_commands, v3_crypt_initial_client_commands));
      c->channel.crypt_in = det_crypt;
      c->channel.crypt_out.reset(new PSOV2OrV3ImitatorEncryption(server_key, det_crypt));
      break;
    }
    case GameVersion::XB:
      c->channel.crypt_in.reset(new PSOV3Encryption(client_key));
      c->channel.crypt_out.reset(new PSOV3Encryption(server_key));
      break;
    default:
      throw invalid_argument("incorrect client version");
  }
}

S_ServerInitWithAfterMessage_BB_03_9B<0xB4>
prepare_server_init_contents_bb(
    const parray<uint8_t, 0x30>& server_key,
    const parray<uint8_t, 0x30>& client_key,
    uint8_t flags) {
  bool use_secondary_message = (flags & SendServerInitFlag::USE_SECONDARY_MESSAGE);
  S_ServerInitWithAfterMessage_BB_03_9B<0xB4> cmd;
  cmd.basic_cmd.copyright.encode(use_secondary_message ? bb_pm_server_copyright : bb_game_server_copyright);
  cmd.basic_cmd.server_key = server_key;
  cmd.basic_cmd.client_key = client_key;
  cmd.after_message.encode(anti_copyright);
  return cmd;
}

void send_server_init_bb(shared_ptr<Client> c, uint8_t flags) {
  bool use_secondary_message = (flags & SendServerInitFlag::USE_SECONDARY_MESSAGE);
  parray<uint8_t, 0x30> server_key;
  parray<uint8_t, 0x30> client_key;
  random_data(server_key.data(), server_key.bytes());
  random_data(client_key.data(), client_key.bytes());
  auto cmd = prepare_server_init_contents_bb(server_key, client_key, flags);
  send_command_t(c, use_secondary_message ? 0x9B : 0x03, 0x00, cmd);

  static const string primary_expected_first_data("\xB4\x00\x93\x00\x00\x00\x00\x00", 8);
  static const string secondary_expected_first_data("\xDC\x00\xDB\x00\x00\x00\x00\x00", 8);
  shared_ptr<PSOBBMultiKeyDetectorEncryption> detector_crypt(new PSOBBMultiKeyDetectorEncryption(
      c->require_server_state()->bb_private_keys,
      bb_crypt_initial_client_commands,
      cmd.basic_cmd.client_key.data(),
      sizeof(cmd.basic_cmd.client_key)));
  c->channel.crypt_in = detector_crypt;
  c->channel.crypt_out.reset(new PSOBBMultiKeyImitatorEncryption(
      detector_crypt, cmd.basic_cmd.server_key.data(),
      sizeof(cmd.basic_cmd.server_key), true));
}

void send_server_init_patch(shared_ptr<Client> c) {
  uint32_t server_key = random_object<uint32_t>();
  uint32_t client_key = random_object<uint32_t>();

  S_ServerInit_Patch_02 cmd;
  cmd.copyright.encode(patch_server_copyright);
  cmd.server_key = server_key;
  cmd.client_key = client_key;
  send_command_t(c, 0x02, 0x00, cmd);

  c->channel.crypt_out.reset(new PSOV2Encryption(server_key));
  c->channel.crypt_in.reset(new PSOV2Encryption(client_key));
}

void send_server_init(shared_ptr<Client> c, uint8_t flags) {
  switch (c->version()) {
    case GameVersion::DC:
    case GameVersion::PC:
    case GameVersion::GC:
    case GameVersion::XB:
      send_server_init_dc_pc_v3(c, flags);
      break;
    case GameVersion::PATCH:
      send_server_init_patch(c);
      break;
    case GameVersion::BB:
      send_server_init_bb(c, flags);
      break;
    default:
      throw logic_error("unimplemented versioned command");
  }
}

void send_update_client_config(shared_ptr<Client> c) {
  switch (c->version()) {
    case GameVersion::DC:
    case GameVersion::PC: {
      if (!c->config.check_flag(Client::Flag::HAS_GUILD_CARD_NUMBER)) {
        c->config.set_flag(Client::Flag::HAS_GUILD_CARD_NUMBER);
        S_UpdateClientConfig_DC_PC_04 cmd;
        cmd.player_tag = 0x00010000;
        cmd.guild_card_number = c->license->serial_number;
        send_command_t(c, 0x04, 0x00, cmd);
      }
      break;
    }
    case GameVersion::GC:
    case GameVersion::XB: {
      c->config.set_flag(Client::Flag::HAS_GUILD_CARD_NUMBER);
      S_UpdateClientConfig_V3_04 cmd;
      cmd.player_tag = 0x00010000;
      cmd.guild_card_number = c->license->serial_number;
      c->config.serialize_into(cmd.client_config);
      send_command_t(c, 0x04, 0x00, cmd);
      break;
    }
    default:
      throw logic_error("send_update_client_config called on incorrect game version");
  }
}

void send_quest_buffer_overflow(shared_ptr<Client> c) {
  // PSO Episode 3 USA doesn't natively support the B2 command, but we can add
  // it back to the game with some tricky commands. For details on how this
  // works, see system/ppc/Episode3USAQuestBufferOverflow.s.
  auto fn = c->require_server_state()->function_code_index->name_to_function.at("Episode3USAQuestBufferOverflow");
  if (fn->code.size() > 0x400) {
    throw runtime_error("Episode 3 buffer overflow code must be a single segment");
  }

  S_OpenFile_PC_GC_44_A6 open_cmd;
  open_cmd.name.encode("PSO/BufferOverflow");
  open_cmd.type = 3;
  open_cmd.file_size = 0x18;
  open_cmd.filename.encode("m999999p_e.bin");
  send_command_t(c, 0xA6, 0x00, open_cmd);

  S_WriteFile_13_A7 write_cmd;
  write_cmd.filename.encode("m999999p_e.bin");
  memcpy(write_cmd.data.data(), fn->code.data(), fn->code.size());
  if (fn->code.size() < 0x400) {
    memset(&write_cmd.data[fn->code.size()], 0, 0x400 - fn->code.size());
  }
  write_cmd.data_size = fn->code.size();
  send_command_t(c, 0xA7, 0x00, write_cmd);
}

void empty_function_call_response_handler(uint32_t, uint32_t) {}

void prepare_client_for_patches(shared_ptr<Client> c, std::function<void()> on_complete) {
  auto s = c->require_server_state();

  auto send_version_detect = [s, wc = weak_ptr<Client>(c), on_complete]() -> void {
    auto c = wc.lock();
    if (!c) {
      return;
    }
    if (c->version() == GameVersion::GC &&
        c->config.specific_version == default_specific_version_for_version(GameVersion::GC, -1)) {
      send_function_call(c, s->function_code_index->name_to_function.at("VersionDetect"));
      c->function_call_response_queue.emplace_back([wc = weak_ptr<Client>(c), on_complete](uint32_t specific_version, uint32_t) -> void {
        auto c = wc.lock();
        if (!c) {
          return;
        }
        c->config.specific_version = specific_version;
        c->log.info("Version detected as %08" PRIX32, c->config.specific_version);
        on_complete();
      });
    } else {
      on_complete();
    }
  };

  if (!c->config.check_flag(Client::Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH)) {
    send_function_call(c, s->function_code_index->name_to_function.at("CacheClearFix-Phase1"), {}, "", 0x80000000, 8, 0x7F2734EC);
    c->function_call_response_queue.emplace_back([s, wc = weak_ptr<Client>(c), send_version_detect](uint32_t, uint32_t header_checksum) -> void {
      auto c = wc.lock();
      if (!c) {
        return;
      }
      try {
        c->config.specific_version = specific_version_for_gc_header_checksum(header_checksum);
        c->log.info("Version detected as %08" PRIX32 " from header checksum %08" PRIX32, c->config.specific_version, header_checksum);
      } catch (const out_of_range&) {
        c->log.info("Could not detect specific version from header checksum %08" PRIX32, header_checksum);
      }
      send_function_call(c, s->function_code_index->name_to_function.at("CacheClearFix-Phase2"));
      c->function_call_response_queue.emplace_back([s, wc = weak_ptr<Client>(c), send_version_detect](uint32_t, uint32_t) -> void {
        auto c = wc.lock();
        if (!c) {
          return;
        }
        c->log.info("Client cache behavior patched");
        c->config.set_flag(Client::Flag::SEND_FUNCTION_CALL_NO_CACHE_PATCH);
        send_update_client_config(c);
        send_version_detect();
      });
    });
  } else {
    send_version_detect();
  }
}

void send_function_call(
    shared_ptr<Client> c,
    shared_ptr<CompiledFunctionCode> code,
    const unordered_map<string, uint32_t>& label_writes,
    const string& suffix,
    uint32_t checksum_addr,
    uint32_t checksum_size,
    uint32_t override_relocations_offset) {
  return send_function_call(
      c->channel,
      c->config,
      code,
      label_writes,
      suffix,
      checksum_addr,
      checksum_size,
      override_relocations_offset);
}

void send_function_call(
    Channel& ch,
    const Client::Config& client_config,
    shared_ptr<CompiledFunctionCode> code,
    const unordered_map<string, uint32_t>& label_writes,
    const string& suffix,
    uint32_t checksum_addr,
    uint32_t checksum_size,
    uint32_t override_relocations_offset) {
  if (client_config.check_flag(Client::Flag::NO_SEND_FUNCTION_CALL)) {
    throw logic_error("client does not support function calls");
  }
  if (code.get() && client_config.check_flag(Client::Flag::SEND_FUNCTION_CALL_CHECKSUM_ONLY)) {
    throw logic_error("client only supports checksums in send_function_call");
  }

  string data;
  uint32_t index = 0;
  if (code.get()) {
    data = code->generate_client_command(label_writes, suffix, override_relocations_offset);
    index = code->index;

    if (client_config.check_flag(Client::Flag::ENCRYPTED_SEND_FUNCTION_CALL)) {
      uint32_t key = random_object<uint32_t>();

      // This format was probably never used on any little-endian system, but we
      // implement the way it would probably work there if it was used.
      StringWriter w;
      if (code->is_big_endian()) {
        w.put_u32b(data.size());
        w.put_u32b(key);
      } else {
        w.put_u32l(data.size());
        w.put_u32l(key);
      }

      data = prs_compress(data);

      // Round size up to a multiple of 4 for encryption
      data.resize((data.size() + 3) & ~3);
      PSOV2Encryption crypt(key);
      if (code->is_big_endian()) {
        crypt.encrypt_big_endian(data.data(), data.size());
      } else {
        crypt.encrypt(data.data(), data.size());
      }

      w.write(data);
      data = std::move(w.str());
    }
  }

  S_ExecuteCode_B2 header = {data.size(), checksum_addr, checksum_size};

  StringWriter w;
  w.put(header);
  w.write(data);

  ch.send(0xB2, index, w.str());
}

void send_reconnect(shared_ptr<Client> c, uint32_t address, uint16_t port) {
  S_Reconnect_19 cmd = {{address, port, 0}};
  send_command_t(c, (c->version() == GameVersion::PATCH) ? 0x14 : 0x19, 0x00, cmd);
}

void send_pc_console_split_reconnect(shared_ptr<Client> c, uint32_t address,
    uint16_t pc_port, uint16_t console_port) {
  S_ReconnectSplit_19 cmd;
  cmd.pc_address = address;
  cmd.pc_port = pc_port;
  cmd.gc_command = 0x19;
  cmd.gc_flag = 0x00;
  cmd.gc_size = 0x97;
  cmd.gc_address = address;
  cmd.gc_port = console_port;
  send_command_t(c, 0x19, 0x00, cmd);
}

void send_client_init_bb(shared_ptr<Client> c, uint32_t error_code) {
  S_ClientInit_BB_00E6 cmd;
  cmd.error_code = error_code;
  cmd.player_tag = 0x00010000;
  cmd.guild_card_number = c->license->serial_number;
  cmd.team_id = static_cast<uint32_t>(random_object<uint32_t>());
  c->config.serialize_into(cmd.client_config);
  cmd.can_create_team = 1;
  cmd.episode_4_unlocked = 1;
  send_command_t(c, 0x00E6, 0x00000000, cmd);
}

void send_system_file_bb(shared_ptr<Client> c) {
  send_command_t(c, 0x00E2, 0x00000000, c->game_data.account()->system_file);
}

void send_player_preview_bb(shared_ptr<Client> c, uint8_t player_index,
    const PlayerDispDataBBPreview* preview) {

  if (!preview) {
    // no player exists
    S_PlayerPreview_NoPlayer_BB_00E4 cmd = {player_index, 0x00000002};
    send_command_t(c, 0x00E4, 0x00000000, cmd);

  } else {
    SC_PlayerPreview_CreateCharacter_BB_00E5 cmd = {player_index, *preview};
    send_command_t(c, 0x00E5, 0x00000000, cmd);
  }
}

void send_guild_card_header_bb(shared_ptr<Client> c) {
  uint32_t checksum = c->game_data.account()->guild_card_file.checksum();
  S_GuildCardHeader_BB_01DC cmd = {1, sizeof(PSOBBGuildCardFile), checksum};
  send_command_t(c, 0x01DC, 0x00000000, cmd);
}

void send_guild_card_chunk_bb(shared_ptr<Client> c, size_t chunk_index) {
  size_t chunk_offset = chunk_index * 0x6800;
  if (chunk_offset >= sizeof(PSOBBGuildCardFile)) {
    throw logic_error("attempted to send chunk beyond end of guild card file");
  }

  S_GuildCardFileChunk_02DC cmd;

  size_t data_size = min<size_t>(sizeof(PSOBBGuildCardFile) - chunk_offset, sizeof(cmd.data));
  cmd.unknown = 0;
  cmd.chunk_index = chunk_index;
  cmd.data.assign_range(
      reinterpret_cast<const uint8_t*>(&c->game_data.account()->guild_card_file) + chunk_offset,
      data_size, 0);

  send_command(c, 0x02DC, 0x00000000, &cmd, sizeof(cmd) - sizeof(cmd.data) + data_size);
}

static const vector<string> stream_file_entries = {
    "ItemMagEdit.prs",
    "ItemPMT.prs",
    "BattleParamEntry.dat",
    "BattleParamEntry_on.dat",
    "BattleParamEntry_lab.dat",
    "BattleParamEntry_lab_on.dat",
    "BattleParamEntry_ep4.dat",
    "BattleParamEntry_ep4_on.dat",
    "PlyLevelTbl.prs",
};
static FileContentsCache bb_stream_files_cache(3600000000ULL);

void send_stream_file_index_bb(shared_ptr<Client> c) {

  struct S_StreamFileIndexEntry_BB_01EB {
    le_uint32_t size;
    le_uint32_t checksum; // crc32 of file data
    le_uint32_t offset; // offset in stream (== sum of all previous files' sizes)
    pstring<TextEncoding::ASCII, 0x40> filename;
  };

  vector<S_StreamFileIndexEntry_BB_01EB> entries;
  size_t offset = 0;
  for (const string& filename : stream_file_entries) {
    string key = "system/blueburst/" + filename;
    auto cache_res = bb_stream_files_cache.get_or_load(key);
    auto& e = entries.emplace_back();
    e.size = cache_res.file->data->size();
    // Computing the checksum can be slow, so we cache it along with the file
    // data. If the cache result was just populated, then it may be different,
    // so we always recompute the checksum in that case.
    if (cache_res.generate_called) {
      e.checksum = crc32(cache_res.file->data->data(), e.size);
      bb_stream_files_cache.replace_obj<uint32_t>(key + ".crc32", e.checksum);
    } else {
      auto compute_checksum = [&](const string&) -> uint32_t {
        return crc32(cache_res.file->data->data(), e.size);
      };
      e.checksum = bb_stream_files_cache.get_obj<uint32_t>(key + ".crc32", compute_checksum).obj;
    }
    e.offset = offset;
    e.filename.encode(filename);
    offset += e.size;
  }
  send_command_vt(c, 0x01EB, entries.size(), entries);
}

void send_stream_file_chunk_bb(shared_ptr<Client> c, uint32_t chunk_index) {
  auto cache_result = bb_stream_files_cache.get(
      "<BB stream file>", +[](const string&) -> string {
        size_t bytes = 0;
        for (const auto& name : stream_file_entries) {
          bytes += bb_stream_files_cache.get_or_load("system/blueburst/" + name).file->data->size();
        }

        string ret;
        ret.reserve(bytes);
        for (const auto& name : stream_file_entries) {
          ret += *bb_stream_files_cache.get_or_load("system/blueburst/" + name).file->data;
        }
        return ret;
      });
  const auto& contents = cache_result.file->data;

  S_StreamFileChunk_BB_02EB chunk_cmd;
  chunk_cmd.chunk_index = chunk_index;
  size_t offset = sizeof(chunk_cmd.data) * chunk_index;
  if (offset > contents->size()) {
    throw runtime_error("client requested chunk beyond end of stream file");
  }
  size_t bytes = min<size_t>(contents->size() - offset, sizeof(chunk_cmd.data));
  chunk_cmd.data.assign_range(reinterpret_cast<const uint8_t*>(contents->data() + offset), bytes, 0);

  size_t cmd_size = offsetof(S_StreamFileChunk_BB_02EB, data) + bytes;
  cmd_size = (cmd_size + 3) & ~3;
  send_command(c, 0x02EB, 0x00000000, &chunk_cmd, cmd_size);
}

void send_approve_player_choice_bb(shared_ptr<Client> c) {
  S_ApprovePlayerChoice_BB_00E4 cmd = {c->game_data.bb_player_index, 1};
  send_command_t(c, 0x00E4, 0x00000000, cmd);
}

void send_complete_player_bb(shared_ptr<Client> c) {
  auto account = c->game_data.account();
  auto player = c->game_data.player(true, false);
  if (c->config.check_flag(Client::Flag::FORCE_ENGLISH_LANGUAGE_BB)) {
    player->inventory.language = 1;
  }

  SC_SyncCharacterSaveFile_BB_00E7 cmd;
  cmd.inventory = player->inventory;
  cmd.disp = player->disp;
  cmd.disp.visual.compute_name_color_checksum();
  cmd.disp.play_time = 0;
  cmd.unknown_a1 = 0;
  cmd.creation_timestamp = 0;
  cmd.signature = 0xA205B064;
  cmd.play_time_seconds = player->disp.play_time;
  cmd.option_flags = account->option_flags;
  cmd.quest_data1 = player->quest_data1;
  cmd.bank = player->bank;
  cmd.guild_card.guild_card_number = c->game_data.guild_card_number;
  cmd.guild_card.name = player->disp.name;
  cmd.guild_card.team_name = account->team_name;
  cmd.guild_card.description = player->guild_card_description;
  cmd.guild_card.present = 1;
  cmd.guild_card.language = cmd.inventory.language;
  cmd.guild_card.section_id = player->disp.visual.section_id;
  cmd.guild_card.char_class = player->disp.visual.char_class;
  cmd.unknown_a3 = 0;
  cmd.symbol_chats = account->symbol_chats;
  cmd.shortcuts = account->shortcuts;
  cmd.auto_reply = player->auto_reply;
  cmd.info_board = player->info_board;
  cmd.battle_records = player->battle_records;
  cmd.unknown_a4.clear(0);
  cmd.challenge_records = player->challenge_records;
  cmd.tech_menu_config = player->tech_menu_config;
  cmd.unknown_a6.clear(0);
  cmd.quest_data2 = player->quest_data2;
  cmd.system_file = account->system_file;

  send_command_t(c, 0x00E7, 0x00000000, cmd);
}

////////////////////////////////////////////////////////////////////////////////
// patch functions

void send_enter_directory_patch(shared_ptr<Client> c, const string& dir) {
  S_EnterDirectory_Patch_09 cmd = {{dir, 1}};
  send_command_t(c, 0x09, 0x00, cmd);
}

void send_patch_file(shared_ptr<Client> c, shared_ptr<PatchFileIndex::File> f) {
  S_OpenFile_Patch_06 open_cmd = {0, f->size, {f->name, 1}};
  send_command_t(c, 0x06, 0x00, open_cmd);

  for (size_t x = 0; x < f->chunk_crcs.size(); x++) {
    auto data = f->load_data();
    size_t chunk_size = min<uint32_t>(f->size - (x * 0x4000), 0x4000);

    vector<pair<const void*, size_t>> blocks;
    S_WriteFileHeader_Patch_07 cmd_header = {x, f->chunk_crcs[x], chunk_size};
    blocks.emplace_back(&cmd_header, sizeof(cmd_header));
    blocks.emplace_back(data->data() + (x * 0x4000), chunk_size);
    send_command(c, 0x07, 0x00, blocks);
  }

  S_CloseCurrentFile_Patch_08 close_cmd = {0};
  send_command_t(c, 0x08, 0x00, close_cmd);
}

////////////////////////////////////////////////////////////////////////////////
// message functions

enum class ColorMode {
  NONE,
  ADD,
  STRIP,
};

static void send_text(
    Channel& ch,
    StringWriter& w,
    uint16_t command,
    const string& text,
    ColorMode color_mode) {
  bool is_w = (ch.version == GameVersion::PC || ch.version == GameVersion::BB || ch.version == GameVersion::PATCH);

  switch (color_mode) {
    case ColorMode::NONE:
      w.write(tt_encode_marked_optional(text, ch.language, is_w));
      break;
    case ColorMode::ADD:
      w.write(tt_encode_marked_optional(add_color(text), ch.language, is_w));
      break;
    case ColorMode::STRIP:
      w.write(tt_encode_marked_optional(strip_color(text), ch.language, is_w));
      break;
  }

  if (is_w) {
    w.put_u16(0);
  } else {
    w.put_u8(0);
  }

  while (w.str().size() & 3) {
    w.put_u8(0);
  }
  ch.send(command, 0x00, w.str());
}

static void send_text(Channel& ch, uint16_t command, const string& text, ColorMode color_mode) {
  StringWriter w;
  send_text(ch, w, command, text, color_mode);
}

static void send_header_text(Channel& ch, uint16_t command, uint32_t guild_card_number, const string& text, ColorMode color_mode) {
  StringWriter w;
  w.put(SC_TextHeader_01_06_11_B0_EE({0, guild_card_number}));
  send_text(ch, w, command, text, color_mode);
}

void send_message_box(shared_ptr<Client> c, const string& text) {
  uint16_t command;
  switch (c->version()) {
    case GameVersion::PATCH:
      command = 0x13;
      break;
    case GameVersion::DC:
    case GameVersion::PC:
      command = 0x1A;
      break;
    case GameVersion::GC:
    case GameVersion::XB:
    case GameVersion::BB:
      command = 0xD5;
      break;
    default:
      throw logic_error("invalid game version");
  }
  send_text(c->channel, command, text, c->config.check_flag(Client::Flag::IS_DC_TRIAL_EDITION) ? ColorMode::STRIP : ColorMode::ADD);
}

void send_ep3_timed_message_box(Channel& ch, uint32_t frames, const string& message) {
  string encoded = tt_encode_marked(add_color(message), ch.language, false);
  StringWriter w;
  w.put<S_TimedMessageBoxHeader_GC_Ep3_EA>({frames});
  w.write(encoded);
  w.put_u8(0);
  while (w.size() & 3) {
    w.put_u8(0);
  }
  ch.send(0xEA, 0x00, w.str());
}

void send_lobby_name(shared_ptr<Client> c, const string& text) {
  send_text(c->channel, 0x8A, text, ColorMode::NONE);
}

void send_quest_info(shared_ptr<Client> c, const string& text, bool is_download_quest) {
  send_text(
      c->channel, is_download_quest ? 0xA5 : 0xA3, text,
      c->config.check_flag(Client::Flag::IS_DC_TRIAL_EDITION) ? ColorMode::STRIP : ColorMode::ADD);
}

void send_lobby_message_box(shared_ptr<Client> c, const string& text, bool left_side_on_bb) {
  uint16_t command = (left_side_on_bb && (c->version() == GameVersion::BB)) ? 0x0101 : 0x0001;
  send_header_text(c->channel, command, 0, text, c->config.check_flag(Client::Flag::IS_DC_TRIAL_EDITION) ? ColorMode::STRIP : ColorMode::ADD);
}

void send_ship_info(shared_ptr<Client> c, const string& text) {
  send_header_text(c->channel, 0x11, 0, text, c->config.check_flag(Client::Flag::IS_DC_TRIAL_EDITION) ? ColorMode::STRIP : ColorMode::ADD);
}

void send_ship_info(Channel& ch, const string& text) {
  send_header_text(ch, 0x11, 0, text, ColorMode::ADD);
}

void send_text_message(Channel& ch, const string& text) {
  send_header_text(ch, 0xB0, 0, text, ColorMode::ADD);
}

void send_text_message(shared_ptr<Client> c, const string& text) {
  if (!c->config.check_flag(Client::Flag::IS_DC_TRIAL_EDITION)) {
    send_header_text(c->channel, 0xB0, 0, text, ColorMode::ADD);
  }
}

void send_text_message(shared_ptr<Lobby> l, const string& text) {
  for (size_t x = 0; x < l->max_clients; x++) {
    if (l->clients[x]) {
      send_text_message(l->clients[x], text);
    }
  }
}

void send_text_message(shared_ptr<ServerState> s, const string& text) {
  // TODO: We should have a collection of all clients (even those not in any
  // lobby) and use that instead here
  for (auto& l : s->all_lobbies()) {
    send_text_message(l, text);
  }
}

__attribute__((format(printf, 2, 3))) void send_ep3_text_message_printf(shared_ptr<ServerState> s, const char* format, ...) {
  va_list va;
  va_start(va, format);
  string buf = string_vprintf(format, va);
  va_end(va);
  for (auto& it : s->id_to_lobby) {
    for (auto& c : it.second->clients) {
      if (c && c->config.check_flag(Client::Flag::IS_EPISODE_3)) {
        send_text_message(c, buf);
      }
    }
  }
}

string prepare_chat_data(
    GameVersion version,
    bool is_nte,
    uint8_t language,
    uint8_t from_client_id,
    const string& from_name,
    const string& text,
    char private_flags) {
  string data;

  if (version == GameVersion::BB) {
    data.append("\tJ");
  }
  data.append(from_name);
  if (is_nte) {
    data.append(string_printf(">%X", from_client_id));
  } else {
    data.append(1, '\t');
  }
  if (private_flags) {
    data.append(1, static_cast<uint16_t>(private_flags));
  }

  if ((version == GameVersion::BB) || (version == GameVersion::PC)) {
    data.append(language ? "\tE" : "\tJ");
    data.append(text);
    return tt_utf8_to_utf16(data);
  } else if (is_nte) {
    data.append(tt_utf8_to_sjis(text));
    return data;
  } else {
    data.append(tt_encode_marked(text, language, false));
    return data;
  }
}

void send_chat_message_from_client(Channel& ch, const string& text, char private_flags) {
  if (private_flags != 0) {
    if (ch.version != GameVersion::GC) {
      throw runtime_error("nonzero private_flags in non-GC chat message");
    }
    string effective_text;
    effective_text.push_back(private_flags);
    effective_text += text;
    send_header_text(ch, 0x06, 0, effective_text, ColorMode::NONE);
  } else {
    send_header_text(ch, 0x06, 0, text, ColorMode::NONE);
  }
}

void send_prepared_chat_message(shared_ptr<Client> c, uint32_t from_guild_card_number, const string& prepared_data) {
  StringWriter w;
  w.put(SC_TextHeader_01_06_11_B0_EE{0, from_guild_card_number});
  w.write(prepared_data);
  w.put_u8(0);
  if ((c->version() == GameVersion::BB) || (c->version() == GameVersion::PC)) {
    w.put_u8(0);
  }
  while (w.size() & 3) {
    w.put_u8(0);
  }
  send_command(c, 0x06, 0x00, w.str());
}

void send_prepared_chat_message(shared_ptr<Lobby> l, uint32_t from_guild_card_number, const string& prepared_data) {
  for (auto c : l->clients) {
    if (c) {
      send_prepared_chat_message(c, from_guild_card_number, prepared_data);
    }
  }
}

void send_chat_message(shared_ptr<Client> c, uint32_t from_guild_card_number, const string& from_name, const string& text, char private_flags) {
  string prepared_data = prepare_chat_data(
      c->version(),
      c->config.check_flag(Client::Flag::IS_DC_TRIAL_EDITION),
      c->language(),
      c->lobby_client_id,
      from_name,
      text,
      private_flags);
  send_prepared_chat_message(c, from_guild_card_number, prepared_data);
}

template <typename CmdT>
void send_simple_mail_t(shared_ptr<Client> c, uint32_t from_guild_card_number, const string& from_name, const string& text) {
  CmdT cmd;
  cmd.player_tag = 0x00010000;
  cmd.from_guild_card_number = from_guild_card_number;
  cmd.from_name.encode(from_name, c->language());
  cmd.to_guild_card_number = c->license->serial_number;
  cmd.text.encode(text, c->language());
  send_command_t(c, 0x81, 0x00, cmd);
}

void send_simple_mail_bb(shared_ptr<Client> c, uint32_t from_guild_card_number, const string& from_name, const string& text) {
  SC_SimpleMail_BB_81 cmd;
  cmd.player_tag = 0x00010000;
  cmd.from_guild_card_number = from_guild_card_number;
  cmd.from_name.encode(from_name, c->language());
  cmd.to_guild_card_number = c->license->serial_number;
  cmd.received_date.encode(format_time(now()), c->language());
  cmd.text.encode(text, c->language());
  send_command_t(c, 0x81, 0x00, cmd);
}

void send_simple_mail(shared_ptr<Client> c, uint32_t from_guild_card_number, const string& from_name, const string& text) {
  switch (c->version()) {
    case GameVersion::DC:
    case GameVersion::GC:
    case GameVersion::XB:
      send_simple_mail_t<SC_SimpleMail_DC_V3_81>(c, from_guild_card_number, from_name, text);
      break;
    case GameVersion::PC:
      send_simple_mail_t<SC_SimpleMail_PC_81>(c, from_guild_card_number, from_name, text);
      break;
    case GameVersion::BB:
      send_simple_mail_bb(c, from_guild_card_number, from_name, text);
      break;
    default:
      throw logic_error("unimplemented versioned command");
  }
}

////////////////////////////////////////////////////////////////////////////////
// info board

template <TextEncoding NameEncoding, TextEncoding MessageEncoding>
void send_info_board_t(shared_ptr<Client> c) {
  vector<S_InfoBoardEntry_D8<NameEncoding, MessageEncoding>> entries;
  auto l = c->require_lobby();
  for (const auto& other_c : l->clients) {
    if (!other_c.get()) {
      continue;
    }
    auto other_p = other_c->game_data.player(true, false);
    auto& e = entries.emplace_back();
    e.name.encode(other_p->disp.name.decode(other_p->inventory.language), c->language());
    e.message.encode(add_color(other_p->info_board.decode(other_p->inventory.language)), c->language());
  }
  send_command_vt(c, 0xD8, entries.size(), entries);
}

void send_info_board(shared_ptr<Client> c) {
  if (c->version() == GameVersion::PC ||
      c->version() == GameVersion::PATCH ||
      c->version() == GameVersion::BB) {
    send_info_board_t<TextEncoding::UTF16, TextEncoding::UTF16>(c);
  } else if (c->language()) {
    send_info_board_t<TextEncoding::ASCII, TextEncoding::ISO8859>(c);
  } else {
    send_info_board_t<TextEncoding::ASCII, TextEncoding::SJIS>(c);
  }
}

template <typename CommandHeaderT, TextEncoding Encoding>
void send_card_search_result_t(
    shared_ptr<Client> c,
    shared_ptr<Client> result,
    shared_ptr<Lobby> result_lobby) {
  auto s = c->require_server_state();
  const auto& port_name = version_to_lobby_port_name.at(static_cast<size_t>(c->version()));

  S_GuildCardSearchResult<CommandHeaderT, Encoding> cmd;
  cmd.player_tag = 0x00010000;
  cmd.searcher_guild_card_number = c->license->serial_number;
  cmd.result_guild_card_number = result->license->serial_number;
  cmd.reconnect_command_header.size = sizeof(cmd.reconnect_command_header) + sizeof(cmd.reconnect_command);
  cmd.reconnect_command_header.command = 0x19;
  cmd.reconnect_command_header.flag = 0x00;
  cmd.reconnect_command.address = s->connect_address_for_client(c);
  cmd.reconnect_command.port = s->name_to_port_config.at(port_name)->port;
  cmd.reconnect_command.unused = 0;

  string location_string;
  if (result_lobby->is_game()) {
    location_string = string_printf("%s,BLOCK01,%s", result_lobby->name.c_str(), s->name.c_str());
  } else if (result_lobby->is_ep3()) {
    location_string = string_printf("BLOCK01-C%02" PRIu32 ",BLOCK01,%s", result_lobby->lobby_id - 15, s->name.c_str());
  } else {
    location_string = string_printf("BLOCK01-%02" PRIu32 ",BLOCK01,%s", result_lobby->lobby_id, s->name.c_str());
  }
  cmd.location_string.encode(location_string, c->language());
  cmd.extension.lobby_refs[0].menu_id = MenuID::LOBBY;
  cmd.extension.lobby_refs[0].item_id = result_lobby->lobby_id;
  auto rp = result->game_data.player(true, false);
  cmd.extension.player_name.encode(rp->disp.name.decode(rp->inventory.language), c->language());

  send_command_t(c, 0x41, 0x00, cmd);
}

void send_card_search_result(
    shared_ptr<Client> c,
    shared_ptr<Client> result,
    shared_ptr<Lobby> result_lobby) {
  if ((c->version() == GameVersion::DC) ||
      (c->version() == GameVersion::GC) ||
      (c->version() == GameVersion::XB)) {
    send_card_search_result_t<PSOCommandHeaderDCV3, TextEncoding::SJIS>(c, result, result_lobby);
  } else if (c->version() == GameVersion::PC) {
    send_card_search_result_t<PSOCommandHeaderPC, TextEncoding::UTF16>(c, result, result_lobby);
  } else if (c->version() == GameVersion::BB) {
    send_card_search_result_t<PSOCommandHeaderBB, TextEncoding::UTF16>(c, result, result_lobby);
  } else {
    throw logic_error("unimplemented versioned command");
  }
}

template <typename CmdT>
void send_guild_card_dc_pc_gc_t(
    Channel& ch,
    uint32_t guild_card_number,
    const string& name,
    const string& description,
    uint8_t language,
    uint8_t section_id,
    uint8_t char_class) {
  CmdT cmd;
  cmd.header.subcommand = 0x06;
  cmd.header.size = sizeof(CmdT) / 4;
  cmd.header.unused = 0x0000;
  cmd.guild_card.player_tag = 0x00010000;
  cmd.guild_card.guild_card_number = guild_card_number;
  cmd.guild_card.name.encode(name, ch.language);
  cmd.guild_card.description.encode(description, ch.language);
  cmd.guild_card.present = 1;
  cmd.guild_card.language = language;
  cmd.guild_card.section_id = section_id;
  cmd.guild_card.char_class = char_class;
  ch.send(0x60, 0x00, &cmd, sizeof(cmd));
}

void send_guild_card_xb(
    Channel& ch,
    uint32_t guild_card_number,
    uint64_t xb_user_id,
    const string& name,
    const string& description,
    uint8_t language,
    uint8_t section_id,
    uint8_t char_class) {
  G_SendGuildCard_XB_6x06 cmd;
  cmd.header.subcommand = 0x06;
  cmd.header.size = sizeof(G_SendGuildCard_XB_6x06) / 4;
  cmd.header.unused = 0x0000;
  cmd.guild_card.player_tag = 0x00010000;
  cmd.guild_card.guild_card_number = guild_card_number;
  cmd.guild_card.xb_user_id_high = (xb_user_id >> 32) & 0xFFFFFFFF;
  cmd.guild_card.xb_user_id_low = xb_user_id & 0xFFFFFFFF;
  cmd.guild_card.name.encode(name, ch.language);
  cmd.guild_card.description.encode(description, ch.language);
  cmd.guild_card.present = 1;
  cmd.guild_card.language = language;
  cmd.guild_card.section_id = section_id;
  cmd.guild_card.char_class = char_class;
  ch.send(0x60, 0x00, &cmd, sizeof(cmd));
}

static void send_guild_card_bb(
    Channel& ch,
    uint32_t guild_card_number,
    const string& name,
    const string& team_name,
    const string& description,
    uint8_t language,
    uint8_t section_id,
    uint8_t char_class) {
  G_SendGuildCard_BB_6x06 cmd;
  cmd.header.subcommand = 0x06;
  cmd.header.size = sizeof(cmd) / 4;
  cmd.header.unused = 0x0000;
  cmd.guild_card.guild_card_number = guild_card_number;
  cmd.guild_card.name.encode(name, ch.language);
  cmd.guild_card.team_name.encode(team_name, ch.language);
  cmd.guild_card.description.encode(description, ch.language);
  cmd.guild_card.present = 1;
  cmd.guild_card.language = language;
  cmd.guild_card.section_id = section_id;
  cmd.guild_card.char_class = char_class;
  ch.send(0x60, 0x00, &cmd, sizeof(cmd));
}

void send_guild_card(
    Channel& ch,
    uint32_t guild_card_number,
    uint64_t xb_user_id,
    const string& name,
    const string& team_name,
    const string& description,
    uint8_t language,
    uint8_t section_id,
    uint8_t char_class) {
  switch (ch.version) {
    case GameVersion::DC:
      send_guild_card_dc_pc_gc_t<G_SendGuildCard_DC_6x06>(
          ch, guild_card_number, name, description, language, section_id, char_class);
      break;
    case GameVersion::PC:
      send_guild_card_dc_pc_gc_t<G_SendGuildCard_PC_6x06>(
          ch, guild_card_number, name, description, language, section_id, char_class);
      break;
    case GameVersion::GC:
      send_guild_card_dc_pc_gc_t<G_SendGuildCard_GC_6x06>(
          ch, guild_card_number, name, description, language, section_id, char_class);
      break;
    case GameVersion::XB:
      send_guild_card_xb(
          ch, guild_card_number, xb_user_id, name, description, language, section_id, char_class);
      break;
    case GameVersion::BB:
      send_guild_card_bb(ch, guild_card_number, name, team_name, description, language, section_id, char_class);
      break;
    default:
      throw logic_error("unimplemented versioned command");
  }
}

void send_guild_card(shared_ptr<Client> c, shared_ptr<Client> source) {
  if (!source->license) {
    throw runtime_error("source player does not have a license");
  }

  auto source_p = source->game_data.player(true, false);
  uint32_t guild_card_number = source->license->serial_number;
  uint64_t xb_user_id = source->license->xb_user_id
      ? source->license->xb_user_id
      : (0xAE00000000000000 | guild_card_number);
  uint8_t language = source_p->inventory.language;
  string name = source_p->disp.name.decode(language);
  string description = source_p->guild_card_description.decode(language);
  uint8_t section_id = source_p->disp.visual.section_id;
  uint8_t char_class = source_p->disp.visual.char_class;

  send_guild_card(c->channel, guild_card_number, xb_user_id, name, "", description, language, section_id, char_class);
}

////////////////////////////////////////////////////////////////////////////////
// menus

template <typename EntryT>
void send_menu_t(shared_ptr<Client> c, shared_ptr<const Menu> menu, bool is_info_menu) {
  vector<EntryT> entries;
  {
    auto& e = entries.emplace_back();
    e.menu_id = menu->menu_id;
    e.item_id = 0xFFFFFFFF;
    e.flags = 0x0004;
    e.text.encode(menu->name, c->language());
  }

  for (const auto& item : menu->items) {
    bool is_visible = true;
    switch (c->version()) {
      case GameVersion::DC:
        is_visible &= !(item.flags & MenuItem::Flag::INVISIBLE_ON_DC);
        if (c->config.check_flag(Client::Flag::IS_DC_TRIAL_EDITION)) {
          is_visible &= !(item.flags & MenuItem::Flag::INVISIBLE_ON_DCNTE);
        }
        break;
      case GameVersion::PC:
        is_visible &= !(item.flags & MenuItem::Flag::INVISIBLE_ON_PC);
        break;
      case GameVersion::GC:
        is_visible &= !(item.flags & MenuItem::Flag::INVISIBLE_ON_GC);
        if (c->config.check_flag(Client::Flag::IS_GC_TRIAL_EDITION)) {
          is_visible &= !(item.flags & MenuItem::Flag::INVISIBLE_ON_GC_TRIAL_EDITION);
        }
        break;
      case GameVersion::XB:
        is_visible &= !(item.flags & MenuItem::Flag::INVISIBLE_ON_XB);
        break;
      case GameVersion::BB:
        is_visible &= !(item.flags & MenuItem::Flag::INVISIBLE_ON_BB);
        break;
      default:
        throw runtime_error("menus not supported for this game version");
    }
    if (item.flags & MenuItem::Flag::REQUIRES_MESSAGE_BOXES) {
      is_visible &= !c->config.check_flag(Client::Flag::NO_D6);
    }
    if (item.flags & MenuItem::Flag::REQUIRES_SEND_FUNCTION_CALL) {
      is_visible &= !c->config.check_flag(Client::Flag::NO_SEND_FUNCTION_CALL);
    }
    if (item.flags & MenuItem::Flag::REQUIRES_SAVE_DISABLED) {
      is_visible &= !c->config.check_flag(Client::Flag::SAVE_ENABLED);
    }
    if (item.flags & MenuItem::Flag::INVISIBLE_IN_INFO_MENU) {
      is_visible &= !is_info_menu;
    }

    if (is_visible) {
      auto& e = entries.emplace_back();
      e.menu_id = menu->menu_id;
      e.item_id = item.item_id;
      e.flags = (c->version() == GameVersion::BB) ? 0x0004 : 0x0F04;
      e.text.encode(item.name, c->language());
    }
  }

  send_command_vt(c, is_info_menu ? 0x1F : 0x07, entries.size() - 1, entries);
  c->last_menu_sent = menu;
}

void send_menu(shared_ptr<Client> c, shared_ptr<const Menu> menu, bool is_info_menu) {
  if (c->version() == GameVersion::PC ||
      c->version() == GameVersion::PATCH ||
      c->version() == GameVersion::BB) {
    send_menu_t<S_MenuEntry_PC_BB_07_1F>(c, menu, is_info_menu);
  } else {
    send_menu_t<S_MenuEntry_DC_V3_07_1F>(c, menu, is_info_menu);
  }
}

template <TextEncoding Encoding>
void send_game_menu_t(
    shared_ptr<Client> c,
    bool is_spectator_team_list,
    bool show_tournaments_only) {
  auto s = c->require_server_state();

  vector<S_GameMenuEntry<Encoding>> entries;
  {
    auto& e = entries.emplace_back();
    e.menu_id = MenuID::GAME;
    e.game_id = 0x00000000;
    e.difficulty_tag = 0x00;
    e.num_players = 0x00;
    e.name.encode(s->name, c->language());
    e.episode = 0x00;
    e.flags = 0x04;
  }

  for (shared_ptr<Lobby> l : s->all_lobbies()) {
    if (!l->is_game()) {
      continue;
    }
    if (!l->version_is_allowed(c->quest_version())) {
      continue;
    }
    if (l->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM) != is_spectator_team_list) {
      continue;
    }
    if (show_tournaments_only && !l->tournament_match) {
      continue;
    }

    uint8_t episode_num;
    switch (l->episode) {
      case Episode::EP1:
        episode_num = 1;
        break;
      case Episode::EP2:
        episode_num = 2;
        break;
      case Episode::EP3:
        episode_num = 0;
        break;
      case Episode::EP4:
        episode_num = 3;
        break;
      default:
        throw runtime_error("lobby has incorrect episode number");
    }

    auto& e = entries.emplace_back();
    e.menu_id = MenuID::GAME;
    e.game_id = l->lobby_id;
    e.difficulty_tag = (l->is_ep3() ? 0x0A : (l->difficulty + 0x22));
    e.num_players = l->count_clients();
    if (c->version() == GameVersion::DC) {
      e.episode = l->version_is_allowed(QuestScriptVersion::DC_V1) ? 1 : 0;
    } else {
      e.episode = ((c->version() == GameVersion::BB) ? (l->max_clients << 4) : 0) | episode_num;
    }
    if (l->is_ep3()) {
      e.flags = (l->password.empty() ? 0 : 2) | (l->check_flag(Lobby::Flag::BATTLE_IN_PROGRESS) ? 4 : 0);
    } else {
      e.flags = ((episode_num << 6) | (l->password.empty() ? 0 : 2));
      switch (l->mode) {
        case GameMode::NORMAL:
          break;
        case GameMode::BATTLE:
          e.flags |= 0x10;
          break;
        case GameMode::CHALLENGE:
          e.flags |= 0x20;
          break;
        case GameMode::SOLO:
          e.flags |= 0x34;
          break;
        default:
          throw logic_error("invalid game mode");
      }
    }
    e.name.encode(l->name, c->language());
  }

  send_command_vt(c, is_spectator_team_list ? 0xE6 : 0x08, entries.size() - 1, entries);
}

void send_game_menu(
    shared_ptr<Client> c,
    bool is_spectator_team_list,
    bool show_tournaments_only) {
  if ((c->version() == GameVersion::DC) ||
      (c->version() == GameVersion::GC) ||
      (c->version() == GameVersion::XB)) {
    send_game_menu_t<TextEncoding::SJIS>(c, is_spectator_team_list, show_tournaments_only);
  } else {
    send_game_menu_t<TextEncoding::UTF16>(c, is_spectator_team_list, show_tournaments_only);
  }
}

template <typename EntryT>
void send_quest_menu_t(
    shared_ptr<Client> c,
    uint32_t menu_id,
    const vector<shared_ptr<const Quest>>& quests,
    bool is_download_menu) {
  auto v = c->quest_version();
  vector<EntryT> entries;
  for (const auto& quest : quests) {
    auto vq = quest->version(v, c->language());
    if (!vq) {
      continue;
    }

    auto& e = entries.emplace_back();
    e.menu_id = menu_id;
    e.item_id = quest->quest_number;
    e.name.encode(vq->name, c->language());
    e.short_description.encode(add_color(vq->short_description), c->language());
  }
  send_command_vt(c, is_download_menu ? 0xA4 : 0xA2, entries.size(), entries);
}

template <typename EntryT>
void send_quest_menu_t(
    shared_ptr<Client> c,
    uint32_t menu_id,
    shared_ptr<const QuestCategoryIndex> category_index,
    uint8_t flags) {
  bool is_download_menu = flags & (QuestCategoryIndex::Category::Flag::DOWNLOAD | QuestCategoryIndex::Category::Flag::EP3_DOWNLOAD);
  vector<EntryT> entries;
  for (const auto& category : category_index->categories) {
    if (!category.matches_flags(flags)) {
      continue;
    }
    auto& e = entries.emplace_back();
    e.menu_id = menu_id;
    e.item_id = category.category_id;
    e.name.encode(category.name, c->language());
    e.short_description.encode(add_color(category.description), c->language());
  }
  send_command_vt(c, is_download_menu ? 0xA4 : 0xA2, entries.size(), entries);
}

void send_quest_menu(shared_ptr<Client> c, uint32_t menu_id,
    const vector<shared_ptr<const Quest>>& quests, bool is_download_menu) {
  switch (c->version()) {
    case GameVersion::PC:
      send_quest_menu_t<S_QuestMenuEntry_PC_A2_A4>(c, menu_id, quests, is_download_menu);
      break;
    case GameVersion::DC:
    case GameVersion::GC:
      send_quest_menu_t<S_QuestMenuEntry_DC_GC_A2_A4>(c, menu_id, quests, is_download_menu);
      break;
    case GameVersion::XB:
      send_quest_menu_t<S_QuestMenuEntry_XB_A2_A4>(c, menu_id, quests, is_download_menu);
      break;
    case GameVersion::BB:
      send_quest_menu_t<S_QuestMenuEntry_BB_A2_A4>(c, menu_id, quests, is_download_menu);
      break;
    default:
      throw logic_error("unimplemented versioned command");
  }
}

void send_quest_menu(shared_ptr<Client> c, uint32_t menu_id,
    shared_ptr<const QuestCategoryIndex> category_index, uint8_t flags) {
  switch (c->version()) {
    case GameVersion::PC:
      send_quest_menu_t<S_QuestMenuEntry_PC_A2_A4>(c, menu_id, category_index, flags);
      break;
    case GameVersion::DC:
    case GameVersion::GC:
      send_quest_menu_t<S_QuestMenuEntry_DC_GC_A2_A4>(c, menu_id, category_index, flags);
      break;
    case GameVersion::XB:
      send_quest_menu_t<S_QuestMenuEntry_XB_A2_A4>(c, menu_id, category_index, flags);
      break;
    case GameVersion::BB:
      send_quest_menu_t<S_QuestMenuEntry_BB_A2_A4>(c, menu_id, category_index, flags);
      break;
    default:
      throw logic_error("unimplemented versioned command");
  }
}

void send_lobby_list(shared_ptr<Client> c) {
  // This command appears to be deprecated, as PSO expects it to be exactly how
  // this server sends it, and does not react if it's different, except by
  // changing the lobby IDs.

  auto s = c->require_server_state();
  vector<S_LobbyListEntry_83> entries;
  for (shared_ptr<Lobby> l : s->all_lobbies()) {
    if (!l->check_flag(Lobby::Flag::DEFAULT)) {
      continue;
    }
    if (l->check_flag(Lobby::Flag::V2_AND_LATER) && c->config.check_flag(Client::Flag::IS_DC_V1)) {
      continue;
    }
    if (l->is_ep3() && !c->config.check_flag(Client::Flag::IS_EPISODE_3)) {
      continue;
    }
    auto& e = entries.emplace_back();
    e.menu_id = MenuID::LOBBY;
    e.item_id = l->lobby_id;
    e.unused = 0;
  }

  send_command_vt(c, 0x83, entries.size(), entries);
}

////////////////////////////////////////////////////////////////////////////////
// lobby joining

template <typename EntryT>
void send_player_records_t(shared_ptr<Client> c, shared_ptr<Lobby> l, shared_ptr<Client> joining_client) {
  vector<EntryT> entries;
  auto add_client = [&](shared_ptr<Client> lc) -> void {
    auto lp = lc->game_data.player(true, false);
    auto& e = entries.emplace_back();
    e.client_id = lc->lobby_client_id;
    e.challenge = lp->challenge_records;
    e.battle = lp->battle_records;
  };

  if (joining_client) {
    add_client(joining_client);
  } else {
    entries.reserve(12);
    for (auto lc : l->clients) {
      if (lc) {
        add_client(lc);
      }
    }
  }
  send_command_vt(c->channel, 0xC5, entries.size(), entries);
}

static void send_join_spectator_team(shared_ptr<Client> c, shared_ptr<Lobby> l) {
  if (!c->config.check_flag(Client::Flag::IS_EPISODE_3)) {
    throw runtime_error("lobby is not Episode 3");
  }
  if (!l->is_ep3()) {
    throw runtime_error("lobby is not Episode 3");
  }
  if (!l->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM)) {
    throw runtime_error("lobby is not a spectator team");
  }

  auto s = c->require_server_state();

  S_JoinSpectatorTeam_GC_Ep3_E8 cmd;

  cmd.variations.clear(0);
  cmd.client_id = c->lobby_client_id;
  cmd.event = l->event;
  cmd.section_id = l->section_id;
  cmd.rare_seed = l->random_seed;
  cmd.episode = 0xFF;

  uint8_t player_count = 0;
  auto watched_lobby = l->watched_lobby.lock();
  if (watched_lobby) {
    // Live spectating
    cmd.leader_id = watched_lobby->leader_id;
    for (size_t z = 0; z < 4; z++) {
      auto& wc = watched_lobby->clients[z];
      if (!wc) {
        continue;
      }
      auto wc_p = wc->game_data.player();
      auto& p = cmd.players[z];
      p.lobby_data.player_tag = 0x00010000;
      p.lobby_data.guild_card_number = wc->license->serial_number;
      p.lobby_data.client_id = wc->lobby_client_id;
      p.lobby_data.name.encode(wc_p->disp.name.decode(wc_p->inventory.language), c->language());
      p.inventory = wc_p->inventory;
      p.inventory.encode_for_version(c->version(), s->item_parameter_table_for_version(c->version()));
      p.disp = wc_p->disp.to_dcpcv3(c->language(), p.inventory.language);
      p.disp.enforce_lobby_join_limits(c->version());

      auto& e = cmd.entries[z];
      e.player_tag = 0x00010000;
      e.guild_card_number = wc->license->serial_number;
      e.name.encode(wc_p->disp.name.decode(wc_p->inventory.language), c->language());
      e.present = 1;
      e.level = wc->game_data.ep3_config
          ? (wc->game_data.ep3_config->online_clv_exp / 100)
          : wc_p->disp.stats.level.load();
      e.name_color = wc_p->disp.visual.name_color;

      player_count++;
    }

  } else if (l->battle_player) {
    // Battle record replay
    const auto* ev = l->battle_player->get_record()->get_first_event();
    if (!ev) {
      throw runtime_error("battle record contains no events");
    }
    if (ev->type != Episode3::BattleRecord::Event::Type::SET_INITIAL_PLAYERS) {
      throw runtime_error("battle record does not begin with set players event");
    }
    if (ev->players.empty()) {
      throw runtime_error("battle record contains no players");
    }
    cmd.leader_id = ev->players[0].lobby_data.client_id;
    for (const auto& entry : ev->players) {
      uint8_t client_id = entry.lobby_data.client_id;
      if (client_id >= 4) {
        throw runtime_error("invalid client id in battle record");
      }
      auto& p = cmd.players[client_id];
      p.lobby_data = entry.lobby_data;
      p.inventory = entry.inventory;
      p.inventory.encode_for_version(c->version(), s->item_parameter_table_for_version(c->version()));
      p.disp = entry.disp;
      p.disp.enforce_lobby_join_limits(c->version());

      auto& e = cmd.entries[client_id];
      e.player_tag = 0x00010000;
      e.guild_card_number = entry.lobby_data.guild_card_number;
      e.name = entry.disp.visual.name;
      e.present = 1;
      e.level = entry.level.load();
      e.name_color = entry.disp.visual.name_color;

      player_count++;
    }

  } else {
    throw runtime_error("neither a watched lobby nor a battle player are present");
  }

  for (size_t z = 4; z < 12; z++) {
    if (l->clients[z]) {
      auto other_c = l->clients[z];
      auto other_p = other_c->game_data.player();
      auto& cmd_p = cmd.spectator_players[z - 4];
      auto& cmd_e = cmd.entries[z];
      cmd_p.lobby_data.player_tag = 0x00010000;
      cmd_p.lobby_data.guild_card_number = other_c->license->serial_number;
      cmd_p.lobby_data.client_id = other_c->lobby_client_id;
      cmd_p.lobby_data.name.encode(other_p->disp.name.decode(other_p->inventory.language), c->language());
      cmd_p.inventory = other_p->inventory;
      cmd_p.disp = other_p->disp.to_dcpcv3(c->language(), cmd_p.inventory.language);
      cmd_p.disp.enforce_lobby_join_limits(c->version());

      cmd_e.player_tag = 0x00010000;
      cmd_e.guild_card_number = other_c->license->serial_number;
      cmd_e.name = cmd_p.lobby_data.name;
      cmd_e.present = 1;
      cmd_e.level = other_c->game_data.ep3_config
          ? (other_c->game_data.ep3_config->online_clv_exp / 100)
          : other_p->disp.stats.level.load();
      cmd_e.name_color = other_p->disp.visual.name_color;

      player_count++;
    }
  }
  cmd.spectator_team_name.encode(l->name, c->language());

  send_command_t(c, 0xE8, player_count, cmd);
}

void send_join_game(shared_ptr<Client> c, shared_ptr<Lobby> l) {
  if (l->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM)) {
    send_join_spectator_team(c, l);
    return;
  }

  auto populate_lobby_data = [&](auto& cmd) -> size_t {
    size_t player_count = 0;
    for (size_t x = 0; x < 4; x++) {
      auto lc = l->clients[x];
      if (lc) {
        cmd.lobby_data[x].player_tag = 0x00010000;
        cmd.lobby_data[x].guild_card_number = lc->license->serial_number;
        cmd.lobby_data[x].client_id = lc->lobby_client_id;
        cmd.lobby_data[x].name.encode(lc->game_data.player()->disp.name.decode(lc->language()), c->language());
        player_count++;
      } else {
        cmd.lobby_data[x].clear();
      }
    }
    return player_count;
  };
  auto populate_base_cmd = [&](auto& cmd) -> size_t {
    cmd.variations = l->variations;
    cmd.client_id = c->lobby_client_id;
    cmd.leader_id = l->leader_id;
    cmd.disable_udp = 0x01; // Unused on PC/XB/BB
    cmd.difficulty = l->difficulty;
    cmd.battle_mode = (l->mode == GameMode::BATTLE) ? 1 : 0;
    cmd.event = l->event;
    cmd.section_id = l->section_id;
    cmd.challenge_mode = (l->mode == GameMode::CHALLENGE) ? 1 : 0;
    cmd.rare_seed = l->random_seed;
    return populate_lobby_data(cmd);
  };
  auto populate_v3_cmd = [&](auto& cmd) -> size_t {
    switch (l->episode) {
      case Episode::EP1:
        cmd.episode = 1;
        break;
      case Episode::EP2:
        cmd.episode = 2;
        break;
      case Episode::EP3:
        cmd.episode = 0xFF;
        break;
      case Episode::EP4:
        cmd.episode = 3;
        break;
      default:
        throw logic_error("invalid episode number in game");
    }
    return populate_base_cmd(cmd);
  };

  switch (c->version()) {
    case GameVersion::DC: {
      if (c->config.check_flag(Client::Flag::IS_DC_TRIAL_EDITION)) {
        S_JoinGame_DCNTE_64 cmd;
        cmd.client_id = c->lobby_client_id;
        cmd.leader_id = l->leader_id;
        cmd.disable_udp = 0x01;
        cmd.variations = l->variations;
        size_t player_count = populate_lobby_data(cmd);
        send_command_t(c, 0x64, player_count, cmd);
      } else {
        S_JoinGame_DC_64 cmd;
        size_t player_count = populate_base_cmd(cmd);
        send_command_t(c, 0x64, player_count, cmd);
      }
      break;
    }
    case GameVersion::PC: {
      S_JoinGame_PC_64 cmd;
      size_t player_count = populate_base_cmd(cmd);
      send_command_t(c, 0x64, player_count, cmd);
      break;
    }
    case GameVersion::GC: {
      if (c->config.check_flag(Client::Flag::IS_EPISODE_3)) {
        S_JoinGame_GC_Ep3_64 cmd;
        size_t player_count = populate_v3_cmd(cmd);
        auto s = c->require_server_state();
        for (size_t x = 0; x < 4; x++) {
          if (l->clients[x]) {
            auto other_p = l->clients[x]->game_data.player();
            cmd.players_ep3[x].inventory = other_p->inventory;
            cmd.players_ep3[x].inventory.encode_for_version(c->version(), s->item_parameter_table_for_version(c->version()));
            cmd.players_ep3[x].disp = convert_player_disp_data<PlayerDispDataDCPCV3>(other_p->disp, c->language(), other_p->inventory.language);
            cmd.players_ep3[x].disp.enforce_lobby_join_limits(c->version());
          }
        }
        send_command_t(c, 0x64, player_count, cmd);
      } else {
        S_JoinGame_GC_64 cmd;
        size_t player_count = populate_v3_cmd(cmd);
        send_command_t(c, 0x64, player_count, cmd);
      }
      break;
    }
    case GameVersion::XB: {
      S_JoinGame_XB_64 cmd;
      size_t player_count = populate_v3_cmd(cmd);
      for (size_t x = 0; x < 4; x++) {
        auto lc = l->clients[x];
        if (lc) {
          if (lc->xb_netloc) {
            cmd.lobby_data[x].netloc = *lc->xb_netloc;
          } else {
            cmd.lobby_data[x].netloc.account_id = 0xAE00000000000000 | lc->license->serial_number;
          }
        }
      }
      send_command_t(c, 0x64, player_count, cmd);
      break;
    }
    case GameVersion::BB: {
      S_JoinGame_BB_64 cmd;
      size_t player_count = populate_v3_cmd(cmd);
      cmd.unused1 = 0;
      cmd.solo_mode = (l->mode == GameMode::SOLO) ? 1 : 0;
      cmd.unused2 = 0;
      send_command_t(c, 0x64, player_count, cmd);
      break;
    }
    case GameVersion::PATCH:
      throw logic_error("patch server clients cannot join games");
    default:
      throw logic_error("invalid game version");
  }
}

template <typename LobbyDataT, typename DispDataT, typename RecordsT, bool UseLanguageMarkerInName>
void send_join_lobby_t(shared_ptr<Client> c, shared_ptr<Lobby> l, shared_ptr<Client> joining_client = nullptr) {
  auto s = c->require_server_state();

  uint8_t command;
  if (l->is_game()) {
    if (joining_client) {
      command = l->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM) ? 0xEB : 0x65;
    } else {
      throw logic_error("send_join_lobby_t should not be used for primary game join command");
    }
  } else {
    command = joining_client ? 0x68 : 0x67;
  }

  if ((c->version() != GameVersion::DC) || !c->config.check_flag(Client::Flag::IS_DC_V1)) {
    send_player_records_t<RecordsT>(c, l, joining_client);
  }

  uint8_t lobby_type;
  if (c->config.override_lobby_number != 0x80) {
    lobby_type = c->config.override_lobby_number;
  } else if (l->check_flag(Lobby::Flag::IS_OVERFLOW)) {
    lobby_type = c->config.check_flag(Client::Flag::IS_EPISODE_3) ? 15 : 0;
  } else {
    lobby_type = l->block - 1;
  }

  // Allow non-canonical lobby types on GC. They may work on other versions too,
  // but I haven't verified which values don't crash on each version.
  if (c->version() == GameVersion::GC) {
    if (c->config.check_flag(Client::Flag::IS_EPISODE_3)) {
      if ((lobby_type > 0x14) && (lobby_type < 0xE9)) {
        lobby_type = l->block - 1;
      }
    } else {
      if ((lobby_type > 0x11) && (lobby_type != 0x67) && (lobby_type != 0xD4) && (lobby_type < 0xFC)) {
        lobby_type = l->block - 1;
      }
    }
  } else {
    if (lobby_type > 0x0E) {
      lobby_type = l->block - 1;
    }
  }

  S_JoinLobby<LobbyFlags, LobbyDataT, DispDataT> cmd;
  cmd.lobby_flags.client_id = c->lobby_client_id;
  cmd.lobby_flags.leader_id = l->leader_id;
  cmd.lobby_flags.disable_udp = 0x01;
  cmd.lobby_flags.lobby_number = lobby_type;
  cmd.lobby_flags.block_number = l->block;
  cmd.lobby_flags.unknown_a1 = 0;
  cmd.lobby_flags.event = l->event;
  cmd.lobby_flags.unknown_a2 = 0;
  cmd.lobby_flags.unused = 0;

  vector<shared_ptr<Client>> lobby_clients;
  if (joining_client) {
    lobby_clients.emplace_back(joining_client);
  } else {
    for (auto lc : l->clients) {
      if (lc) {
        lobby_clients.emplace_back(lc);
      }
    }
  }

  size_t used_entries = 0;
  for (const auto& lc : lobby_clients) {
    auto lp = lc->game_data.player();
    auto& e = cmd.entries[used_entries++];
    e.lobby_data.player_tag = 0x00010000;
    e.lobby_data.guild_card_number = lc->license->serial_number;
    e.lobby_data.client_id = lc->lobby_client_id;
    if (UseLanguageMarkerInName) {
      e.lobby_data.name.encode("\tJ" + lp->disp.name.decode(lp->inventory.language), c->language());
    } else {
      e.lobby_data.name.encode(lp->disp.name.decode(lp->inventory.language), c->language());
    }
    e.inventory = lp->inventory;
    e.inventory.encode_for_version(c->version(), s->item_parameter_table_for_version(c->version()));
    e.disp = convert_player_disp_data<DispDataT>(lp->disp, c->language(), lp->inventory.language);
    e.disp.enforce_lobby_join_limits(c->version());
  }

  send_command(c, command, used_entries, &cmd, cmd.size(used_entries));
}

void send_join_lobby_xb(shared_ptr<Client> c, shared_ptr<Lobby> l, shared_ptr<Client> joining_client = nullptr) {
  auto s = c->require_server_state();

  uint8_t command;
  if (l->is_game()) {
    if (joining_client) {
      command = 0x65;
    } else {
      throw logic_error("send_join_lobby_xb should not be used for primary game join command");
    }
  } else {
    command = joining_client ? 0x68 : 0x67;
  }

  send_player_records_t<PlayerRecordsEntry_V3>(c, l, joining_client);

  uint8_t lobby_type;
  if (c->config.override_lobby_number != 0x80) {
    lobby_type = c->config.override_lobby_number;
  } else if (l->check_flag(Lobby::Flag::IS_OVERFLOW)) {
    lobby_type = c->config.check_flag(Client::Flag::IS_EPISODE_3) ? 15 : 0;
  } else {
    lobby_type = l->block - 1;
  }

  if ((lobby_type > 0x11) && (lobby_type != 0x67) && (lobby_type != 0xD4) && (lobby_type < 0xFC)) {
    lobby_type = l->block - 1;
  }

  S_JoinLobby_XB_65_67_68 cmd;
  cmd.lobby_flags.client_id = c->lobby_client_id;
  cmd.lobby_flags.leader_id = l->leader_id;
  cmd.lobby_flags.disable_udp = 0x01;
  cmd.lobby_flags.lobby_number = lobby_type;
  cmd.lobby_flags.block_number = l->block;
  cmd.lobby_flags.unknown_a1 = 0;
  cmd.lobby_flags.event = l->event;
  cmd.lobby_flags.unknown_a2 = 0;
  cmd.lobby_flags.unused = 0;

  vector<shared_ptr<Client>> lobby_clients;
  if (joining_client) {
    lobby_clients.emplace_back(joining_client);
  } else {
    for (auto lc : l->clients) {
      if (lc) {
        lobby_clients.emplace_back(lc);
      }
    }
  }

  size_t used_entries = 0;
  for (const auto& lc : lobby_clients) {
    auto lp = lc->game_data.player();
    auto& e = cmd.entries[used_entries++];
    e.lobby_data.player_tag = 0x00010000;
    e.lobby_data.guild_card_number = lc->license->serial_number;
    if (lc->xb_netloc) {
      e.lobby_data.netloc = *lc->xb_netloc;
    } else {
      e.lobby_data.netloc.account_id = 0xAE00000000000000 | lc->license->serial_number;
    }
    e.lobby_data.client_id = lc->lobby_client_id;
    e.lobby_data.name.encode(lp->disp.name.decode(lp->inventory.language), c->language());
    e.inventory = lp->inventory;
    e.inventory.encode_for_version(c->version(), s->item_parameter_table_for_version(c->version()));
    e.disp = convert_player_disp_data<PlayerDispDataDCPCV3>(lp->disp, c->language(), lp->inventory.language);
    e.disp.enforce_lobby_join_limits(c->version());
  }

  send_command(c, command, used_entries, &cmd, cmd.size(used_entries));
}

void send_join_lobby_dc_nte(shared_ptr<Client> c, shared_ptr<Lobby> l,
    shared_ptr<Client> joining_client = nullptr) {
  uint8_t command;
  if (l->is_game()) {
    if (joining_client) {
      command = 0x65;
    } else {
      throw logic_error("send_join_lobby_dc_nte should not be used for primary game join command");
    }
  } else {
    command = joining_client ? 0x68 : 0x67;
  }

  S_JoinLobby_DCNTE_65_67_68 cmd;
  cmd.lobby_flags.client_id = c->lobby_client_id;
  cmd.lobby_flags.leader_id = l->leader_id;
  cmd.lobby_flags.disable_udp = 0x01;

  vector<shared_ptr<Client>> lobby_clients;
  if (joining_client) {
    lobby_clients.emplace_back(joining_client);
  } else {
    for (auto lc : l->clients) {
      if (lc) {
        lobby_clients.emplace_back(lc);
      }
    }
  }

  auto s = c->require_server_state();

  size_t used_entries = 0;
  for (const auto& lc : lobby_clients) {
    auto lp = lc->game_data.player();
    auto& e = cmd.entries[used_entries++];
    e.lobby_data.player_tag = 0x00010000;
    e.lobby_data.guild_card_number = lc->license->serial_number;
    e.lobby_data.client_id = lc->lobby_client_id;
    e.lobby_data.name.encode(lp->disp.name.decode(lp->inventory.language), c->language());
    e.inventory = lp->inventory;
    e.inventory.encode_for_version(c->version(), s->item_parameter_table_for_version(c->version()));
    e.disp = convert_player_disp_data<PlayerDispDataDCPCV3>(lp->disp, c->language(), lp->inventory.language);
    e.disp.enforce_lobby_join_limits(c->version());
  }

  send_command(c, command, used_entries, &cmd, cmd.size(used_entries));
}

void send_join_lobby(shared_ptr<Client> c, shared_ptr<Lobby> l) {
  if (l->is_game()) {
    send_join_game(c, l);
  } else {
    switch (c->version()) {
      case GameVersion::DC:
        if (c->config.check_flag(Client::Flag::IS_DC_TRIAL_EDITION) || c->config.check_flag(Client::Flag::IS_DC_V1_PROTOTYPE)) {
          send_join_lobby_dc_nte(c, l);
        } else {
          send_join_lobby_t<PlayerLobbyDataDCGC, PlayerDispDataDCPCV3, PlayerRecordsEntry_DC, false>(c, l);
        }
        break;
      case GameVersion::PC:
        send_join_lobby_t<PlayerLobbyDataPC, PlayerDispDataDCPCV3, PlayerRecordsEntry_PC, false>(c, l);
        break;
      case GameVersion::GC:
        send_join_lobby_t<PlayerLobbyDataDCGC, PlayerDispDataDCPCV3, PlayerRecordsEntry_V3, false>(c, l);
        break;
      case GameVersion::XB:
        send_join_lobby_xb(c, l);
        break;
      case GameVersion::BB:
        send_join_lobby_t<PlayerLobbyDataBB, PlayerDispDataBB, PlayerRecordsEntry_BB, true>(c, l);
        break;
      default:
        throw logic_error("unimplemented versioned command");
    }
  }

  // If the client will stop sending message box close confirmations after
  // joining any lobby, set the appropriate flag and update the client config
  if (c->config.check_flag(Client::Flag::NO_D6_AFTER_LOBBY) && !c->config.check_flag(Client::Flag::NO_D6)) {
    c->config.set_flag(Client::Flag::NO_D6);
    send_update_client_config(c);
  }
}

void send_player_join_notification(shared_ptr<Client> c,
    shared_ptr<Lobby> l, shared_ptr<Client> joining_client) {
  switch (c->version()) {
    case GameVersion::DC:
      if (c->config.check_flag(Client::Flag::IS_DC_TRIAL_EDITION) || c->config.check_flag(Client::Flag::IS_DC_V1_PROTOTYPE)) {
        send_join_lobby_dc_nte(c, l, joining_client);
      } else {
        send_join_lobby_t<PlayerLobbyDataDCGC, PlayerDispDataDCPCV3, PlayerRecordsEntry_DC, false>(c, l, joining_client);
      }
      break;
    case GameVersion::PC:
      send_join_lobby_t<PlayerLobbyDataPC, PlayerDispDataDCPCV3, PlayerRecordsEntry_PC, false>(c, l, joining_client);
      break;
    case GameVersion::GC:
      send_join_lobby_t<PlayerLobbyDataDCGC, PlayerDispDataDCPCV3, PlayerRecordsEntry_V3, false>(c, l, joining_client);
      break;
    case GameVersion::XB:
      send_join_lobby_xb(c, l, joining_client);
      break;
    case GameVersion::BB:
      send_join_lobby_t<PlayerLobbyDataBB, PlayerDispDataBB, PlayerRecordsEntry_BB, true>(c, l, joining_client);
      break;
    default:
      throw logic_error("unimplemented versioned command");
  }
}

void send_player_leave_notification(shared_ptr<Lobby> l, uint8_t leaving_client_id) {
  S_LeaveLobby_66_69_Ep3_E9 cmd = {leaving_client_id, l->leader_id, 1, 0};
  uint8_t cmd_num;
  if (l->is_game()) {
    cmd_num = l->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM) ? 0xE9 : 0x66;
  } else {
    cmd_num = 0x69;
  }
  send_command_t(l, cmd_num, leaving_client_id, cmd);
  for (const auto& watcher_l : l->watcher_lobbies) {
    send_command_t(watcher_l, cmd_num, leaving_client_id, cmd);
  }
}

void send_self_leave_notification(shared_ptr<Client> c) {
  S_LeaveLobby_66_69_Ep3_E9 cmd = {c->lobby_client_id, 0, 1, 0};
  send_command_t(c, 0x69, c->lobby_client_id, cmd);
}

void send_get_player_info(shared_ptr<Client> c) {
  if ((c->version() == GameVersion::DC) &&
      c->config.check_flag(Client::Flag::IS_DC_TRIAL_EDITION)) {
    send_command(c, 0x8D, 0x00);
  } else {
    send_command(c, 0x95, 0x00);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Trade window

void send_execute_item_trade(shared_ptr<Client> c, const vector<ItemData>& items) {
  auto s = c->require_server_state();

  SC_TradeItems_D0_D3 cmd;
  if (items.size() > cmd.item_datas.size()) {
    throw logic_error("too many items in execute trade command");
  }
  cmd.target_client_id = c->lobby_client_id;
  cmd.item_count = items.size();
  for (size_t x = 0; x < items.size(); x++) {
    cmd.item_datas[x] = items[x];
    cmd.item_datas[x].encode_for_version(c->version(), s->item_parameter_table_for_version(c->version()));
  }
  send_command_t(c, 0xD3, 0x00, cmd);
}

void send_execute_card_trade(shared_ptr<Client> c,
    const vector<pair<uint32_t, uint32_t>>& card_to_count) {
  if (!c->config.check_flag(Client::Flag::IS_EPISODE_3)) {
    throw logic_error("cannot send trade cards command to non-Ep3 client");
  }

  SC_TradeCards_GC_Ep3_EE_FlagD0_FlagD3 cmd;
  constexpr size_t max_entries = sizeof(cmd.entries) / sizeof(cmd.entries[0]);
  if (card_to_count.size() > max_entries) {
    throw logic_error("too many items in execute card trade command");
  }

  cmd.target_client_id = c->lobby_client_id;
  cmd.entry_count = card_to_count.size();
  size_t x;
  for (x = 0; x < card_to_count.size(); x++) {
    cmd.entries[x].card_type = card_to_count[x].first;
    cmd.entries[x].count = card_to_count[x].second;
  }
  for (; x < max_entries; x++) {
    cmd.entries[x].card_type = 0;
    cmd.entries[x].count = 0;
  }
  send_command_t(c, 0xEE, 0xD3, cmd);
}

////////////////////////////////////////////////////////////////////////////////
// arrows

void send_arrow_update(shared_ptr<Lobby> l) {
  vector<S_ArrowUpdateEntry_88> entries;

  for (size_t x = 0; x < l->max_clients; x++) {
    if (!l->clients[x]) {
      continue;
    }
    auto& e = entries.emplace_back();
    e.player_tag = 0x00010000;
    e.guild_card_number = l->clients[x]->license->serial_number;
    e.arrow_color = l->clients[x]->lobby_arrow_color;
  }

  for (size_t x = 0; x < l->max_clients; x++) {
    if (!l->clients[x] || l->clients[x]->config.check_flag(Client::Flag::IS_DC_V1)) {
      continue;
    }
    send_command_vt(l->clients[x], 0x88, entries.size(), entries);
  }
}

// tells the player that the joining player is done joining, and the game can resume
void send_resume_game(shared_ptr<Lobby> l, shared_ptr<Client> ready_client) {
  static const be_uint32_t data = 0x72010000;
  send_command_excluding_client(l, ready_client, 0x60, 0x00, &data, sizeof(be_uint32_t));
}

////////////////////////////////////////////////////////////////////////////////
// Game/cheat commands

static vector<G_UpdatePlayerStat_6x9A> generate_stats_change_subcommands(
    uint16_t client_id, PlayerStatsChange stat, uint32_t amount) {
  if (amount > (0x7BF8 * 0xFF) / sizeof(G_UpdatePlayerStat_6x9A)) {
    throw runtime_error("stats change command is too large");
  }

  uint8_t stat_ch = static_cast<uint8_t>(stat);
  vector<G_UpdatePlayerStat_6x9A> subs;
  while (amount > 0) {
    uint8_t sub_amount = min<size_t>(amount, 0xFF);
    subs.emplace_back(G_UpdatePlayerStat_6x9A{{0x9A, 0x02, client_id}, 0, stat_ch, sub_amount});
    amount -= sub_amount;
  }
  return subs;
}

void send_player_stats_change(shared_ptr<Client> c, PlayerStatsChange stat, uint32_t amount) {
  auto l = c->require_lobby();
  auto subs = generate_stats_change_subcommands(c->lobby_client_id, stat, amount);
  send_command_vt(l, (subs.size() > 0x400 / sizeof(G_UpdatePlayerStat_6x9A)) ? 0x6C : 0x60, 0x00, subs);
}

void send_player_stats_change(Channel& ch, uint16_t client_id, PlayerStatsChange stat, uint32_t amount) {
  auto subs = generate_stats_change_subcommands(client_id, stat, amount);
  send_command_vt(ch, (subs.size() > 0x400 / sizeof(G_UpdatePlayerStat_6x9A)) ? 0x6C : 0x60, 0x00, subs);
}

void send_warp(Channel& ch, uint8_t client_id, uint32_t area, bool is_private) {
  G_InterLevelWarp_6x94 cmd = {{0x94, 0x02, 0}, area, {}};
  ch.send(is_private ? 0x62 : 0x60, client_id, &cmd, sizeof(cmd));
}

void send_warp(shared_ptr<Client> c, uint32_t area, bool is_private) {
  send_warp(c->channel, c->lobby_client_id, area, is_private);
  c->area = area;
}

void send_warp(shared_ptr<Lobby> l, uint32_t area, bool is_private) {
  for (const auto& c : l->clients) {
    if (c) {
      send_warp(c, area, is_private);
    }
  }
}

void send_ep3_change_music(Channel& ch, uint32_t song) {
  G_ChangeLobbyMusic_GC_Ep3_6xBF cmd = {{0xBF, 0x02, 0}, song};
  ch.send(0x60, 0x00, cmd);
}

void send_set_player_visibility(shared_ptr<Lobby> l, shared_ptr<Client> c,
    bool visible) {
  uint8_t subcmd = visible ? 0x23 : 0x22;
  uint16_t client_id = c->lobby_client_id;
  G_SetPlayerVisibility_6x22_6x23 cmd = {{subcmd, 0x01, client_id}};
  send_command_t(l, 0x60, 0x00, cmd);
}

////////////////////////////////////////////////////////////////////////////////
// BB game commands

void send_drop_item(shared_ptr<ServerState> s, Channel& ch, const ItemData& item,
    bool from_enemy, uint8_t area, float x, float z, uint16_t entity_id) {
  G_DropItem_PC_V3_BB_6x5F cmd = {
      {{0x5F, 0x0B, 0x0000}, {area, from_enemy, entity_id, x, z, 0, 0, item}}, 0};
  cmd.item.item.encode_for_version(ch.version, s->item_parameter_table_for_version(ch.version));
  ch.send(0x60, 0x00, &cmd, sizeof(cmd));
}

void send_drop_item(shared_ptr<Lobby> l, const ItemData& item,
    bool from_enemy, uint8_t area, float x, float z, uint16_t entity_id) {
  auto s = l->require_server_state();
  for (auto& c : l->clients) {
    if (!c) {
      continue;
    }
    send_drop_item(s, c->channel, item, from_enemy, area, x, z, entity_id);
  }
}

void send_drop_stacked_item(shared_ptr<ServerState> s, Channel& ch, const ItemData& item, uint8_t area, float x, float z) {
  G_DropStackedItem_PC_V3_BB_6x5D cmd = {{{0x5D, 0x0A, 0x0000}, area, 0, x, z, item}, 0};
  cmd.item_data.encode_for_version(ch.version, s->item_parameter_table_for_version(ch.version));
  ch.send(0x60, 0x00, &cmd, sizeof(cmd));
}

void send_drop_stacked_item(shared_ptr<Lobby> l, const ItemData& item, uint8_t area, float x, float z) {
  auto s = l->require_server_state();
  for (auto& c : l->clients) {
    if (!c) {
      continue;
    }
    send_drop_stacked_item(s, c->channel, item, area, x, z);
  }
}

void send_pick_up_item(shared_ptr<Client> c, uint32_t item_id, uint8_t area) {
  auto l = c->require_lobby();
  uint16_t client_id = c->lobby_client_id;
  G_PickUpItem_6x59 cmd = {{0x59, 0x03, client_id}, client_id, area, item_id};
  send_command_t(l, 0x60, 0x00, cmd);
}

void send_create_inventory_item(shared_ptr<Client> c, const ItemData& item) {
  auto l = c->require_lobby();
  if (c->version() != GameVersion::BB) {
    throw logic_error("6xBE can only be sent to BB clients");
  }
  uint16_t client_id = c->lobby_client_id;
  G_CreateInventoryItem_BB_6xBE cmd = {{0xBE, 0x07, client_id}, item, 0};
  send_command_t(l, 0x60, 0x00, cmd);
}

void send_destroy_item(shared_ptr<Client> c, uint32_t item_id, uint32_t amount) {
  auto l = c->require_lobby();
  uint16_t client_id = c->lobby_client_id;
  G_DeleteInventoryItem_6x29 cmd = {{0x29, 0x03, client_id}, item_id, amount};
  send_command_t(l, 0x60, 0x00, cmd);
}

void send_item_identify_result(shared_ptr<Client> c) {
  auto l = c->require_lobby();
  if (c->version() != GameVersion::BB) {
    throw logic_error("cannot send item identify result to non-BB client");
  }
  G_IdentifyResult_BB_6xB9 res;
  res.header.subcommand = 0xB9;
  res.header.size = sizeof(res) / 4;
  res.header.client_id = c->lobby_client_id;
  res.item_data = c->game_data.identify_result;
  send_command_t(l, 0x60, 0x00, res);
}

void send_bank(shared_ptr<Client> c) {
  if (c->version() != GameVersion::BB) {
    throw logic_error("6xBC can only be sent to BB clients");
  }

  auto p = c->game_data.player();
  const auto* items_it = p->bank.items.data();
  vector<PlayerBankItem> items(items_it, items_it + p->bank.num_items);

  G_BankContentsHeader_BB_6xBC cmd = {
      {{0xBC, 0, 0}, sizeof(G_BankContentsHeader_BB_6xBC) + items.size() * sizeof(PlayerBankItem)},
      random_object<uint32_t>(),
      p->bank.num_items,
      p->bank.meseta};

  send_command_t_vt(c, 0x6C, 0x00, cmd, items);
}

void send_shop(shared_ptr<Client> c, uint8_t shop_type) {
  if (c->version() != GameVersion::BB) {
    throw logic_error("6xB6 can only be sent to BB clients");
  }

  const auto& contents = c->game_data.shop_contents.at(shop_type);

  G_ShopContents_BB_6xB6 cmd = {
      {0xB6, static_cast<uint8_t>(2 + (sizeof(ItemData) >> 2) * contents.size()), 0x0000},
      shop_type,
      static_cast<uint8_t>(contents.size()),
      0,
      {},
  };
  for (size_t x = 0; x < contents.size(); x++) {
    cmd.item_datas[x] = contents[x];
  }

  send_command(c, 0x60, 0x00, &cmd, sizeof(cmd) - sizeof(cmd.item_datas[0]) * (20 - contents.size()));
}

void send_level_up(shared_ptr<Client> c) {
  auto l = c->require_lobby();
  auto p = c->game_data.player();
  CharacterStats stats = p->disp.stats.char_stats;

  const ItemData* mag = nullptr;
  try {
    mag = &p->inventory.items[p->inventory.find_equipped_mag()].data;
  } catch (const out_of_range&) {
  }

  G_LevelUp_6x30 cmd = {
      {0x30, sizeof(G_LevelUp_6x30) / 4, c->lobby_client_id},
      stats.atp + (mag ? (mag->data1w[3] / 50) : 0),
      stats.mst + (mag ? (mag->data1w[5] / 50) : 0),
      stats.evp,
      stats.hp,
      stats.dfp + (mag ? (mag->data1w[2] / 100) : 0),
      stats.ata + (mag ? (mag->data1w[4] / 20) : 0),
      p->disp.stats.level.load(),
      0};
  send_command_t(l, 0x60, 0x00, cmd);
}

void send_give_experience(shared_ptr<Client> c, uint32_t amount) {
  auto l = c->require_lobby();
  if (c->version() != GameVersion::BB) {
    throw logic_error("6xBF can only be sent to BB clients");
  }
  uint16_t client_id = c->lobby_client_id;
  G_GiveExperience_BB_6xBF cmd = {
      {0xBF, sizeof(G_GiveExperience_BB_6xBF) / 4, client_id}, amount};
  send_command_t(l, 0x60, 0x00, cmd);
}

void send_set_exp_multiplier(std::shared_ptr<Lobby> l) {
  if (l->base_version != GameVersion::BB) {
    throw logic_error("6xDD can only be sent to BB clients");
  }
  if (!l->is_game()) {
    throw logic_error("6xDD can only be sent in games (not in lobbies)");
  }
  if (floorf(l->exp_multiplier) != l->exp_multiplier) {
    throw runtime_error("EXP multiplier is not an integer");
  }
  G_SetEXPMultiplier_BB_6xDD cmd = {{0xDD, sizeof(G_SetEXPMultiplier_BB_6xDD) / 4, static_cast<uint16_t>(l->exp_multiplier)}};
  send_command_t(l, 0x60, 0x00, cmd);
}

void send_rare_enemy_index_list(shared_ptr<Client> c, const vector<size_t>& indexes) {
  S_RareMonsterList_BB_DE cmd;
  if (indexes.size() > cmd.enemy_indexes.size()) {
    throw runtime_error("too many rare enemies");
  }
  for (size_t z = 0; z < indexes.size(); z++) {
    cmd.enemy_indexes[z] = indexes[z];
  }
  cmd.enemy_indexes.clear_after(indexes.size(), 0xFFFF);
  send_command_t(c, 0xDE, 0x00, cmd);
}

void send_quest_function_call(Channel& ch, uint16_t function_id) {
  S_CallQuestFunction_V3_BB_AB cmd;
  cmd.function_id = function_id;
  ch.send(0xAB, 0x00, &cmd, sizeof(cmd));
}

void send_quest_function_call(shared_ptr<Client> c, uint16_t function_id) {
  send_quest_function_call(c->channel, function_id);
}

////////////////////////////////////////////////////////////////////////////////
// ep3 only commands

void send_ep3_card_list_update(shared_ptr<Client> c) {
  if (!c->config.check_flag(Client::Flag::HAS_EP3_CARD_DEFS)) {
    auto s = c->require_server_state();
    const auto& data = c->config.check_flag(Client::Flag::IS_EP3_TRIAL_EDITION)
        ? s->ep3_card_index_trial->get_compressed_definitions()
        : s->ep3_card_index->get_compressed_definitions();

    StringWriter w;
    w.put_u32l(data.size());
    w.write(data);

    send_command(c, 0xB8, 0x00, w.str());
  }
}

void send_ep3_media_update(
    shared_ptr<Client> c,
    uint32_t type,
    uint32_t which,
    const string& compressed_data) {
  StringWriter w;
  w.put<S_UpdateMediaHeader_GC_Ep3_B9>({type, which, compressed_data.size(), 0});
  w.write(compressed_data);
  while (w.size() & 3) {
    w.put_u8(0);
  }
  send_command(c, 0xB9, 0x00, w.str());
}

void send_ep3_rank_update(shared_ptr<Client> c) {
  auto s = c->require_server_state();
  uint32_t current_meseta = s->ep3_infinite_meseta ? 1000000 : c->license->ep3_current_meseta;
  uint32_t total_meseta_earned = s->ep3_infinite_meseta ? 1000000 : c->license->ep3_total_meseta_earned;
  S_RankUpdate_GC_Ep3_B7 cmd = {0, {}, current_meseta, total_meseta_earned, 0xFFFFFFFF};
  send_command_t(c, 0xB7, 0x00, cmd);
}

void send_ep3_card_battle_table_state(shared_ptr<Lobby> l, uint16_t table_number) {
  S_CardBattleTableState_GC_Ep3_E4 cmd;
  for (size_t z = 0; z < 4; z++) {
    cmd.entries[z].state = 0;
    cmd.entries[z].unknown_a1 = 0;
    cmd.entries[z].guild_card_number = 0;
  }

  set<shared_ptr<Client>> clients;
  for (const auto& c : l->clients) {
    if (!c) {
      continue;
    }
    if (c->card_battle_table_number == table_number) {
      if (c->card_battle_table_seat_number > 3) {
        throw runtime_error("invalid battle table seat number");
      }
      auto& e = cmd.entries[c->card_battle_table_seat_number];
      if (e.state == 0) {
        e.state = c->card_battle_table_seat_state;
        e.guild_card_number = c->license->serial_number;
        clients.emplace(c);
      }
    }
  }

  for (const auto& c : clients) {
    send_command_t(c, 0xE4, table_number, cmd);
  }
}

void send_ep3_set_context_token(shared_ptr<Client> c, uint32_t context_token) {
  G_SetContextToken_GC_Ep3_6xB4x1F cmd;
  cmd.context_token = context_token;
  send_command_t(c, 0xC9, 0x00, cmd);
}

void send_ep3_confirm_tournament_entry(
    shared_ptr<Client> c,
    shared_ptr<const Episode3::Tournament> tourn) {
  if (c->config.check_flag(Client::Flag::IS_EP3_TRIAL_EDITION)) {
    throw runtime_error("cannot send tournament entry command to Episode 3 Trial Edition client");
  }

  S_ConfirmTournamentEntry_GC_Ep3_CC cmd;
  if (tourn) {
    auto s = c->require_server_state();
    cmd.tournament_name.encode(tourn->get_name(), c->language());
    cmd.server_name.encode(s->name, c->language());
    // TODO: Fill this in appropriately when we support scheduled start times
    cmd.start_time.encode("Unknown", c->language());
    auto& teams = tourn->all_teams();
    cmd.num_teams = min<size_t>(teams.size(), 0x20);
    cmd.players_per_team = (tourn->get_flags() & Episode3::Tournament::Flag::IS_2V2) ? 2 : 1;
    for (size_t z = 0; z < min<size_t>(teams.size(), 0x20); z++) {
      cmd.team_entries[z].win_count = teams[z]->num_rounds_cleared;
      cmd.team_entries[z].is_active = teams[z]->is_active;
      cmd.team_entries[z].name.encode(teams[z]->name, c->language());
    }
  }
  send_command_t(c, 0xCC, tourn ? 0x01 : 0x00, cmd);
}

void send_ep3_tournament_list(
    shared_ptr<Client> c,
    bool is_for_spectator_team_create) {
  auto s = c->require_server_state();

  S_TournamentList_GC_Ep3_E0 cmd;
  size_t z = 0;
  for (const auto& it : s->ep3_tournament_index->all_tournaments()) {
    const auto& tourn = it.second;
    if (z >= 0x20) {
      throw logic_error("more than 32 tournaments exist");
    }
    auto& entry = cmd.entries[z];
    entry.menu_id = is_for_spectator_team_create
        ? MenuID::TOURNAMENTS_FOR_SPEC
        : MenuID::TOURNAMENTS;
    entry.item_id = tourn->get_menu_item_id();
    // TODO: What does it mean for a tournament to be locked? Should we support
    // that?
    // TODO: Write appropriate round text (1st, 2nd, 3rd) here. This is
    // nontrivial because unlike Sega's implementation, newserv does not require
    // a round to completely finish before starting matches in the next round,
    // as long as the winners of the preceding matches have been determined.
    entry.state =
        (tourn->get_state() == Episode3::Tournament::State::REGISTRATION)
        ? 0x00
        : 0x05;
    // TODO: Fill in cmd.start_time here when we implement scheduled starts.
    entry.name.encode(tourn->get_name(), c->language());
    const auto& teams = tourn->all_teams();
    for (auto team : teams) {
      if (!team->name.empty()) {
        entry.num_teams++;
      }
    }
    entry.max_teams = teams.size();
    entry.unknown_a3 = 0xFFFF;
    entry.unknown_a4 = 0xFFFF;
    z++;
  }
  send_command_t(c, 0xE0, z, cmd);
}

void send_ep3_tournament_entry_list(
    shared_ptr<Client> c,
    shared_ptr<const Episode3::Tournament> tourn,
    bool is_for_spectator_team_create) {
  S_TournamentEntryList_GC_Ep3_E2 cmd;
  cmd.players_per_team = (tourn->get_flags() & Episode3::Tournament::Flag::IS_2V2) ? 2 : 1;
  size_t z = 0;
  for (const auto& team : tourn->all_teams()) {
    if (z >= 0x20) {
      throw logic_error("more than 32 teams in tournament");
    }
    auto& entry = cmd.entries[z];
    entry.menu_id = MenuID::TOURNAMENT_ENTRIES;
    entry.item_id = (tourn->get_menu_item_id() << 16) | z;
    entry.unknown_a2 = team->num_rounds_cleared;
    entry.locked = team->password.empty() ? 0 : 1;
    if (tourn->get_state() != Episode3::Tournament::State::REGISTRATION) {
      entry.state = 2;
    } else if (team->name.empty()) {
      entry.state = 0;
    } else if (team->players.size() < team->max_players) {
      entry.state = 1;
    } else {
      entry.state = 2;
    }
    entry.name.encode(team->name, c->language());
    z++;
  }
  send_command_t(c, is_for_spectator_team_create ? 0xE7 : 0xE2, z, cmd);
}

void send_ep3_tournament_details(
    shared_ptr<Client> c,
    shared_ptr<const Episode3::Tournament> tourn) {
  S_TournamentGameDetails_GC_Ep3_E3 cmd;
  auto vm = tourn->get_map()->version(c->language());
  cmd.name.encode(tourn->get_name(), c->language());
  cmd.map_name.encode(vm->map->name.decode(vm->language), c->language());
  cmd.rules = tourn->get_rules();
  const auto& teams = tourn->all_teams();
  for (size_t z = 0; z < min<size_t>(teams.size(), 0x20); z++) {
    cmd.bracket_entries[z].win_count = teams[z]->num_rounds_cleared;
    cmd.bracket_entries[z].is_active = teams[z]->is_active ? 1 : 0;
    cmd.bracket_entries[z].team_name.encode(teams[z]->name, c->language());
  }
  cmd.num_bracket_entries = teams.size();
  cmd.players_per_team = (tourn->get_flags() & Episode3::Tournament::Flag::IS_2V2) ? 2 : 1;
  send_command_t(c, 0xE3, 0x02, cmd);
}

string ep3_description_for_client(shared_ptr<Client> c) {
  if (!c->config.check_flag(Client::Flag::IS_EPISODE_3)) {
    throw runtime_error("client is not Episode 3");
  }
  auto player = c->game_data.player();
  return string_printf(
      "%s CLv%" PRIu32 " %c",
      name_for_char_class(player->disp.visual.char_class),
      player->disp.stats.level + 1,
      char_for_language_code(player->inventory.language));
}

void send_ep3_game_details(shared_ptr<Client> c, shared_ptr<Lobby> l) {

  shared_ptr<Lobby> primary_lobby;
  if (l->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM)) {
    primary_lobby = l->watched_lobby.lock();
  } else {
    primary_lobby = l;
  }

  auto tourn_match = primary_lobby ? primary_lobby->tournament_match : nullptr;
  auto tourn = tourn_match ? tourn_match->tournament.lock() : nullptr;

  if (tourn) {
    S_TournamentGameDetails_GC_Ep3_E3 cmd;
    cmd.name.encode(l->name, c->language());

    auto vm = tourn->get_map()->version(c->language());
    cmd.map_name.encode(vm->map->name.decode(vm->language), c->language());
    cmd.rules = tourn->get_rules();

    const auto& teams = tourn->all_teams();
    for (size_t z = 0; z < min<size_t>(teams.size(), 0x20); z++) {
      auto& entry = cmd.bracket_entries[z];
      entry.win_count = teams[z]->num_rounds_cleared;
      entry.is_active = teams[z]->is_active ? 1 : 0;
      entry.team_name.encode(teams[z]->name, c->language());
    }
    cmd.num_bracket_entries = teams.size();
    cmd.players_per_team = (tourn->get_flags() & Episode3::Tournament::Flag::IS_2V2) ? 2 : 1;

    if (primary_lobby) {
      auto serial_number_to_client = primary_lobby->clients_by_serial_number();
      auto describe_team = [&](S_TournamentGameDetails_GC_Ep3_E3::TeamEntry& team_entry, shared_ptr<const Episode3::Tournament::Team> team) -> void {
        team_entry.team_name.encode(team->name, c->language());
        for (size_t z = 0; z < team->players.size(); z++) {
          auto& entry = team_entry.players[z];
          const auto& player = team->players[z];
          if (player.is_human()) {
            try {
              auto other_c = serial_number_to_client.at(player.serial_number);
              entry.name.encode(other_c->game_data.player()->disp.name.decode(other_c->language()), c->language());
              entry.description.encode(ep3_description_for_client(other_c), c->language());
            } catch (const out_of_range&) {
              entry.name.encode(player.player_name, c->language());
              entry.description.encode("(Not connected)", c->language());
            }
          } else {
            entry.name.encode(player.com_deck->player_name, c->language());
            entry.description.encode("Deck: " + player.com_deck->deck_name, c->language());
          }
        }
      };
      describe_team(cmd.team_entries[0], tourn_match->preceding_a->winner_team);
      describe_team(cmd.team_entries[1], tourn_match->preceding_b->winner_team);
    }

    uint8_t flag;
    if (l != primary_lobby) {
      for (auto spec_c : l->clients) {
        if (spec_c) {
          auto& entry = cmd.spectator_entries[cmd.num_spectators++];
          entry.name.encode(spec_c->game_data.player()->disp.name.decode(spec_c->language()), c->language());
          entry.description.encode(ep3_description_for_client(spec_c), c->language());
        }
      }
      flag = 0x05;
    } else {
      flag = 0x03;
    }
    send_command_t(c, 0xE3, flag, cmd);

  } else {
    S_GameInformation_GC_Ep3_E1 cmd;
    cmd.game_name.encode(l->name, c->language());
    if (primary_lobby) {
      size_t num_players = 0;
      for (const auto& opp_c : primary_lobby->clients) {
        if (opp_c) {
          cmd.player_entries[num_players].name.encode(opp_c->game_data.player()->disp.name.decode(opp_c->language()), c->language());
          cmd.player_entries[num_players].description.encode(ep3_description_for_client(opp_c), c->language());
          num_players++;
        }
      }
    }

    uint8_t flag;
    if (l != primary_lobby) {
      size_t num_spectators = 0;
      for (auto spec_c : l->clients) {
        if (spec_c) {
          auto& entry = cmd.spectator_entries[num_spectators++];
          entry.name.encode(spec_c->game_data.player()->disp.name.decode(spec_c->language()), c->language());
          entry.description.encode(ep3_description_for_client(spec_c), c->language());
        }
      }

      // There is a client bug that causes the spectators list to always be
      // empty when sent with E1, because there's no way for E1 to set the
      // spectator count in the info window object. To account for this, we send
      // a mostly-blank E3 to set the spectator count, followed by an E1 with
      // the correct data.
      S_TournamentGameDetails_GC_Ep3_E3 cmd_E3;
      cmd_E3.num_spectators = num_spectators;
      send_command_t(c, 0xE3, 0x04, cmd_E3);

      flag = 0x04;

    } else if (primary_lobby &&
        primary_lobby->ep3_server &&
        primary_lobby->ep3_server->get_setup_phase() != Episode3::SetupPhase::REGISTRATION) {
      cmd.rules = primary_lobby->ep3_server->map_and_rules->rules;
      flag = 0x01;

    } else {
      flag = 0x00;
    }

    send_command_t(c, 0xE1, flag, cmd);
  }
}

void send_ep3_set_tournament_player_decks(shared_ptr<Client> c) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();

  auto& match = l->tournament_match;
  auto tourn = match->tournament.lock();
  if (!tourn) {
    throw runtime_error("tournament is deleted");
  }

  G_SetTournamentPlayerDecks_GC_Ep3_6xB4x3D cmd;
  cmd.rules = tourn->get_rules();
  cmd.map_number = tourn->get_map()->map_number;
  cmd.player_slot = 0xFF;

  for (size_t z = 0; z < 4; z++) {
    auto& entry = cmd.entries[z];
    entry.player_name.clear();
    entry.deck_name.clear();
    entry.unknown_a1.clear(0);
    entry.card_ids.clear(0);
    entry.client_id = z;
  }

  auto add_entries_for_team = [&](shared_ptr<const Episode3::Tournament::Team> team, size_t base_index) -> void {
    for (size_t z = 0; z < team->players.size(); z++) {
      auto& entry = cmd.entries[base_index + z];
      const auto& player = team->players[z];
      if (player.is_human()) {
        entry.type = 1; // Human
        entry.player_name.encode(player.player_name, c->language());
        if (player.serial_number == c->license->serial_number) {
          cmd.player_slot = base_index + z;
        }
      } else {
        entry.type = 2; // COM
        entry.player_name.encode(player.com_deck->player_name, c->language());
        entry.deck_name.encode(player.com_deck->deck_name, c->language());
        entry.card_ids = player.com_deck->card_ids;
      }
      entry.unknown_a2 = 6;
    }
  };
  add_entries_for_team(match->preceding_a->winner_team, 0);
  add_entries_for_team(match->preceding_b->winner_team, 2);

  if (!(s->ep3_behavior_flags & Episode3::BehaviorFlag::DISABLE_MASKING)) {
    uint8_t mask_key = (random_object<uint32_t>() % 0xFF) + 1;
    set_mask_for_ep3_game_command(&cmd, sizeof(cmd), mask_key);
  }

  send_command_t(c, 0xC9, 0x00, cmd);

  // TODO: Handle disconnection during the match (the other team should win)
}

void send_ep3_tournament_match_result(shared_ptr<Lobby> l, uint32_t meseta_reward) {
  auto s = l->require_server_state();
  auto& match = l->tournament_match;
  auto tourn = match->tournament.lock();
  if (!tourn) {
    return;
  }

  if ((match->winner_team != match->preceding_a->winner_team) &&
      (match->winner_team != match->preceding_b->winner_team)) {
    throw logic_error("cannot send tournament result without valid winner team");
  }

  auto serial_number_to_client = l->clients_by_serial_number();

  for (const auto& lc : l->clients) {
    if (!lc) {
      continue;
    }
    auto write_player_names = [&](G_TournamentMatchResult_GC_Ep3_6xB4x51::NamesEntry& entry, shared_ptr<const Episode3::Tournament::Team> team) -> void {
      for (size_t z = 0; z < team->players.size(); z++) {
        const auto& player = team->players[z];
        if (player.is_human()) {
          try {
            auto pc = serial_number_to_client.at(player.serial_number);
            entry.player_names[z].encode(pc->game_data.player()->disp.name.decode(pc->language()), lc->language());
          } catch (const out_of_range&) {
            entry.player_names[z].encode(player.player_name, lc->language());
          }
        } else {
          entry.player_names[z].encode(player.com_deck->player_name, lc->language());
        }
      }
    };

    G_TournamentMatchResult_GC_Ep3_6xB4x51 cmd;
    cmd.match_description.encode((match == tourn->get_final_match())
            ? string_printf("(%s) Final match", tourn->get_name().c_str())
            : string_printf("(%s) Round %zu", tourn->get_name().c_str(), match->round_num),
        lc->language());
    cmd.names_entries[0].team_name.encode(match->preceding_a->winner_team->name, lc->language());
    write_player_names(cmd.names_entries[0], match->preceding_a->winner_team);
    cmd.names_entries[1].team_name.encode(match->preceding_b->winner_team->name, lc->language());
    write_player_names(cmd.names_entries[1], match->preceding_b->winner_team);
    // The value 6 here causes the client to show the "Congratulations" text
    // instead of "On to the next round"
    cmd.round_num = (match == tourn->get_final_match()) ? 6 : match->round_num;
    cmd.num_players_per_team = match->preceding_a->winner_team->max_players;
    cmd.winner_team_id = (match->preceding_b->winner_team == match->winner_team);
    cmd.meseta_amount = meseta_reward;
    cmd.meseta_reward_text.encode("You got %s meseta!", 1);
    if (!(s->ep3_behavior_flags & Episode3::BehaviorFlag::DISABLE_MASKING)) {
      uint8_t mask_key = (random_object<uint32_t>() % 0xFF) + 1;
      set_mask_for_ep3_game_command(&cmd, sizeof(cmd), mask_key);
    }
    send_command_t(lc, 0xC9, 0x00, cmd);
  }

  if (s->ep3_behavior_flags & Episode3::BehaviorFlag::ENABLE_STATUS_MESSAGES) {
    send_text_message_printf(l, "$C5TOURN/%" PRIX32 "/%zu WIN %c",
        tourn->get_menu_item_id(), match->round_num,
        match->winner_team == match->preceding_a->winner_team ? 'A' : 'B');
  }
}

void send_ep3_update_game_metadata(shared_ptr<Lobby> l) {
  size_t total_spectators = 0;
  for (auto watcher_l : l->watcher_lobbies) {
    for (auto c : watcher_l->clients) {
      total_spectators += (c.get() != nullptr);
    }
  }

  auto s = l->require_server_state();

  {
    G_SetGameMetadata_GC_Ep3_6xB4x52 cmd;
    cmd.total_spectators = total_spectators;
    if (!(s->ep3_behavior_flags & Episode3::BehaviorFlag::DISABLE_MASKING)) {
      uint8_t mask_key = (random_object<uint32_t>() % 0xFF) + 1;
      set_mask_for_ep3_game_command(&cmd, sizeof(cmd), mask_key);
    }
    // Note: We can't use send_command_t(l, ...) here because that would send
    // the same command to l and to all watcher lobbies. The commands should
    // have different values depending on who's in each watcher lobby, so we
    // have to manually send to each client here.
    for (auto c : l->clients) {
      if (c) {
        send_command_t(c, 0xC9, 0x00, cmd);
      }
    }
  }
  if (!l->watcher_lobbies.empty()) {
    string text;
    auto tourn = l->tournament_match ? l->tournament_match->tournament.lock() : 0;
    if (l->tournament_match && tourn) {
      if (tourn->get_final_match() == l->tournament_match) {
        text = string_printf("Viewing final match of tournament %s", tourn->get_name().c_str());
      } else {
        text = string_printf(
            "Viewing match in round %zu of tournament %s",
            l->tournament_match->round_num, tourn->get_name().c_str());
      }
    } else {
      text = "Viewing battle in game " + l->name;
    }
    add_color_inplace(text);
    for (auto watcher_l : l->watcher_lobbies) {
      G_SetGameMetadata_GC_Ep3_6xB4x52 cmd;
      cmd.local_spectators = 0;
      for (auto c : watcher_l->clients) {
        cmd.local_spectators += (c.get() != nullptr);
      }
      cmd.total_spectators = total_spectators;
      cmd.text_size = text.size();
      cmd.text.encode(text, 1);
      if (!(s->ep3_behavior_flags & Episode3::BehaviorFlag::DISABLE_MASKING)) {
        uint8_t mask_key = (random_object<uint32_t>() % 0xFF) + 1;
        set_mask_for_ep3_game_command(&cmd, sizeof(cmd), mask_key);
      }
      send_command_t(watcher_l, 0xC9, 0x00, cmd);
    }
  }
}

void set_mask_for_ep3_game_command(void* vdata, size_t size, uint8_t mask_key) {
  if (size < 8) {
    throw logic_error("Episode 3 game command is too short for masking");
  }

  auto* header = reinterpret_cast<G_CardBattleCommandHeader*>(vdata);
  size_t command_bytes = header->size * 4;
  if (command_bytes != size) {
    throw runtime_error("command size field does not match actual size");
  }

  // Don't waste time if the existing mask_key is the same as the requested one
  if (header->mask_key == mask_key) {
    return;
  }

  // If header->mask_key isn't zero when we get here, then the command is
  // already masked with a different mask_key, so unmask it first
  if ((mask_key != 0) && (header->mask_key != 0)) {
    set_mask_for_ep3_game_command(vdata, size, 0);
  }

  // Now, exactly one of header->mask_key and mask_key should be nonzero, and we
  // are either directly masking or unmasking the command. Since this operation
  // is symmetric, we don't need to split it into two cases.
  if ((header->mask_key == 0) == (mask_key == 0)) {
    throw logic_error("only one of header->mask_key and mask_key may be nonzero");
  }

  uint8_t* data = reinterpret_cast<uint8_t*>(vdata);
  uint8_t k = (header->mask_key ^ mask_key) + 0x80;
  for (size_t z = 8; z < command_bytes; z++) {
    k = (k * 7) + 3;
    data[z] ^= k;
  }
  header->mask_key = mask_key;
}

void send_quest_file_chunk(
    shared_ptr<Client> c,
    const string& filename,
    size_t chunk_index,
    const void* data,
    size_t size,
    bool is_download_quest) {
  if (size > 0x400) {
    throw logic_error("quest file chunks must be 1KB or smaller");
  }

  S_WriteFile_13_A7 cmd;
  cmd.filename.encode(filename);
  memcpy(cmd.data.data(), data, size);
  if (size < 0x400) {
    memset(&cmd.data[size], 0, 0x400 - size);
  }
  cmd.data_size = size;

  send_command_t(c, is_download_quest ? 0xA7 : 0x13, chunk_index, cmd);
}

template <typename CommandT>
void send_open_quest_file_t(
    shared_ptr<Client> c,
    const string& quest_name,
    const string& filename,
    const string&,
    uint32_t file_size,
    uint32_t,
    QuestFileType type) {
  CommandT cmd;
  uint8_t command_num;
  switch (type) {
    case QuestFileType::ONLINE:
      command_num = 0x44;
      cmd.name.encode("PSO/" + quest_name);
      cmd.type = 0;
      break;
    case QuestFileType::GBA_DEMO:
      command_num = 0xA6;
      cmd.name.encode("GBA Demo");
      cmd.type = 2;
      break;
    case QuestFileType::DOWNLOAD:
      command_num = 0xA6;
      cmd.name.encode("PSO/" + quest_name);
      cmd.type = 0;
      break;
    case QuestFileType::EPISODE_3:
      command_num = 0xA6;
      cmd.name.encode("PSO/" + quest_name);
      cmd.type = 3;
      break;
    default:
      throw logic_error("invalid quest file type");
  }
  cmd.file_size = file_size;
  cmd.filename.encode(filename);
  send_command_t(c, command_num, 0x00, cmd);
}

template <>
void send_open_quest_file_t<S_OpenFile_XB_44_A6>(
    shared_ptr<Client> c,
    const string& quest_name,
    const string& filename,
    const string& xb_filename,
    uint32_t file_size,
    uint32_t quest_number,
    QuestFileType type) {
  S_OpenFile_XB_44_A6 cmd;
  cmd.name.encode("PSO/" + quest_name);
  cmd.type = 0;
  cmd.file_size = file_size;
  cmd.filename.encode(filename);
  cmd.xb_filename.encode(xb_filename);
  cmd.content_meta = 0x30000000 | quest_number;
  send_command_t(c, (type == QuestFileType::ONLINE) ? 0x44 : 0xA6, 0x00, cmd);
}

void send_open_quest_file(
    shared_ptr<Client> c,
    const string& quest_name,
    const string& filename,
    const string& xb_filename,
    uint32_t quest_number,
    QuestFileType type,
    shared_ptr<const string> contents) {

  switch (c->version()) {
    case GameVersion::DC:
      send_open_quest_file_t<S_OpenFile_DC_44_A6>(c, quest_name, filename, xb_filename, contents->size(), quest_number, type);
      break;
    case GameVersion::PC:
    case GameVersion::GC:
      send_open_quest_file_t<S_OpenFile_PC_GC_44_A6>(c, quest_name, filename, xb_filename, contents->size(), quest_number, type);
      break;
    case GameVersion::XB:
      send_open_quest_file_t<S_OpenFile_XB_44_A6>(c, quest_name, filename, xb_filename, contents->size(), quest_number, type);
      break;
    case GameVersion::BB:
      send_open_quest_file_t<S_OpenFile_BB_44_A6>(c, quest_name, filename, xb_filename, contents->size(), quest_number, type);
      break;
    default:
      throw logic_error("cannot send quest files to this version of client");
  }

  // For GC/XB/BB, we wait for acknowledgement commands before sending each
  // chunk. For DC/PC, we send the entire quest all at once.
  if ((c->version() == GameVersion::DC) || (c->version() == GameVersion::PC)) {
    for (size_t offset = 0; offset < contents->size(); offset += 0x400) {
      size_t chunk_bytes = contents->size() - offset;
      if (chunk_bytes > 0x400) {
        chunk_bytes = 0x400;
      }
      send_quest_file_chunk(c, filename.c_str(), offset / 0x400,
          contents->data() + offset, chunk_bytes, (type != QuestFileType::ONLINE));
    }
  } else {
    c->sending_files.emplace(filename, contents);
    c->log.info("Opened file %s", filename.c_str());
  }
}

bool send_quest_barrier_if_all_clients_ready(shared_ptr<Lobby> l) {
  if (!l || !l->is_game()) {
    return false;
  }

  // Check if any client is still loading
  size_t x;
  for (x = 0; x < l->max_clients; x++) {
    if (!l->clients[x]) {
      continue;
    }
    if (l->clients[x]->config.check_flag(Client::Flag::LOADING_QUEST)) {
      break;
    }
  }

  // If they're all done, start the quest
  if (x != l->max_clients) {
    return false;
  }

  send_command(l, 0xAC, 0x00);
  for (x = 0; x < l->max_clients; x++) {
    if (l->clients[x]) {
      l->clients[x]->disconnect_hooks.erase(QUEST_BARRIER_DISCONNECT_HOOK_NAME);
    }
  }
  return true;
}

bool send_ep3_start_tournament_deck_select_if_all_clients_ready(shared_ptr<Lobby> l) {
  if (!l || !l->is_game() || (l->episode != Episode::EP3) || !l->tournament_match) {
    return false;
  }
  auto tourn = l->tournament_match->tournament.lock();
  if (!tourn) {
    return false;
  }

  // Check if any client is still loading
  size_t x;
  for (x = 0; x < l->max_clients; x++) {
    if (!l->clients[x]) {
      continue;
    }
    if (l->clients[x]->config.check_flag(Client::Flag::LOADING_TOURNAMENT)) {
      break;
    }
  }

  // If they're all done, start deck selection
  if (x == l->max_clients) {
    if (!l->ep3_server) {
      l->create_ep3_server();
    }
    l->ep3_server->send_6xB6x41_to_all_clients();
    for (auto c : l->clients) {
      if (c) {
        send_ep3_set_tournament_player_decks(c);
      }
    }
    return true;
  } else {
    return false;
  }
}

void send_ep3_card_auction(shared_ptr<Lobby> l) {
  auto s = l->require_server_state();
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
  for (const auto& e : s->ep3_card_auction_pool) {
    distribution_size += e.probability;
  }

  auto card_index = l->check_flag(Lobby::Flag::IS_EP3_TRIAL)
      ? s->ep3_card_index_trial
      : s->ep3_card_index;

  S_StartCardAuction_GC_Ep3_EF cmd;
  cmd.points_available = s->ep3_card_auction_points;
  for (size_t z = 0; z < num_cards; z++) {
    uint64_t v = random_object<uint64_t>() % distribution_size;
    for (const auto& e : s->ep3_card_auction_pool) {
      if (v >= e.probability) {
        v -= e.probability;
      } else {
        cmd.entries[z].card_id = e.card_id;
        cmd.entries[z].min_price = e.min_price;
        break;
      }
    }
  }
  send_command_t(l, 0xEF, num_cards, cmd);
}

void send_ep3_disband_watcher_lobbies(shared_ptr<Lobby> primary_l) {
  for (auto watcher_l : primary_l->watcher_lobbies) {
    if (!watcher_l->is_ep3()) {
      throw logic_error("spectator team is not an Episode 3 lobby");
    }
    primary_l->log.info("Disbanding watcher lobby %" PRIX32, watcher_l->lobby_id);
    send_command(watcher_l, 0xED, 0x00);
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
  // This command isn't supported on versions before V3, nor on Trial Edition.
  if ((c->version() == GameVersion::DC) ||
      (c->version() == GameVersion::PC) ||
      c->config.check_flag(Client::Flag::IS_GC_TRIAL_EDITION)) {
    return;
  }
  send_command(c, 0xDA, new_event);
}

void send_change_event(shared_ptr<Lobby> l, uint8_t new_event) {
  for (auto& c : l->clients) {
    if (!c) {
      continue;
    }
    send_change_event(c, new_event);
  }
}

void send_change_event(shared_ptr<ServerState> s, uint8_t new_event) {
  // TODO: Create a collection of all clients on the server (including those not
  // in lobbies) and use that here instead
  for (auto& l : s->all_lobbies()) {
    send_change_event(l, new_event);
  }
}
