#include "SendCommands.hh"

#include <event2/buffer.h>
#include <inttypes.h>
#include <stdarg.h>
#include <string.h>

#include <functional>
#include <memory>
#include <phosg/Encoding.hh>
#include <phosg/Hash.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>
#include <phosg/Time.hh>
#include <set>

#include "CommandFormats.hh"
#include "Compression.hh"
#include "FileContentsCache.hh"
#include "PSOProtocol.hh"
#include "ProxyServer.hh"
#include "ReceiveSubcommands.hh"
#include "StaticGameData.hh"
#include "Text.hh"

using namespace std;

extern const char* QUEST_BARRIER_DISCONNECT_HOOK_NAME;

inline uint8_t get_pre_v1_subcommand(Version v, uint8_t nte_subcommand, uint8_t proto_subcommand, uint8_t final_subcommand) {
  if (v == Version::DC_NTE) {
    return nte_subcommand;
  } else if (v == Version::DC_11_2000) {
    return proto_subcommand;
  } else {
    return final_subcommand;
  }
}

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
});

const unordered_set<string> bb_crypt_initial_client_commands({
    string("\xB4\x00\x93\x00\x00\x00\x00\x00", 8),
    string("\xAC\x00\x93\x00\x00\x00\x00\x00", 8),
    string("\xDC\x00\xDB\x00\x00\x00\x00\x00", 8),
});

void send_command(shared_ptr<Client> c, uint16_t command, uint32_t flag, const vector<pair<const void*, size_t>>& blocks) {
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
    case Version::PC_PATCH:
    case Version::BB_PATCH:
    case Version::PC_NTE:
    case Version::PC_V2:
      send_command_with_header_t<PSOCommandHeaderPC>(ch, data, size);
      break;
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
    case Version::XB_V3:
      send_command_with_header_t<PSOCommandHeaderDCV3>(ch, data, size);
      break;
    case Version::BB_V4:
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

S_ServerInitWithAfterMessageT_DC_PC_V3_02_17_91_9B<0xB4>
prepare_server_init_contents_console(
    uint32_t server_key, uint32_t client_key, uint8_t flags) {
  bool initial_connection = (flags & SendServerInitFlag::IS_INITIAL_CONNECTION);
  S_ServerInitWithAfterMessageT_DC_PC_V3_02_17_91_9B<0xB4> cmd;
  cmd.basic_cmd.copyright.encode(initial_connection ? dc_port_map_copyright : dc_lobby_server_copyright);
  cmd.basic_cmd.server_key = server_key;
  cmd.basic_cmd.client_key = client_key;
  cmd.after_message.encode(anti_copyright);
  return cmd;
}

void send_server_init_dc_pc_v3(shared_ptr<Client> c, uint8_t flags) {
  bool initial_connection = (flags & SendServerInitFlag::IS_INITIAL_CONNECTION);
  uint8_t command = initial_connection ? 0x17 : 0x02;
  uint32_t server_key = phosg::random_object<uint32_t>();
  uint32_t client_key = phosg::random_object<uint32_t>();

  auto cmd = prepare_server_init_contents_console(server_key, client_key, initial_connection);
  send_command_t(c, command, 0x00, cmd);

  switch (c->version()) {
    case Version::PC_NTE:
    case Version::PC_V2:
      c->channel.crypt_in = make_shared<PSOV2Encryption>(client_key);
      c->channel.crypt_out = make_shared<PSOV2Encryption>(server_key);
      break;
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3: {
      auto det_crypt = make_shared<PSOV2OrV3DetectorEncryption>(
          client_key, v2_crypt_initial_client_commands, v3_crypt_initial_client_commands);
      c->channel.crypt_in = det_crypt;
      c->channel.crypt_out = make_shared<PSOV2OrV3ImitatorEncryption>(server_key, det_crypt);
      break;
    }
    case Version::XB_V3:
      c->channel.crypt_in = make_shared<PSOV3Encryption>(client_key);
      c->channel.crypt_out = make_shared<PSOV3Encryption>(server_key);
      break;
    default:
      throw invalid_argument("incorrect client version");
  }
}

S_ServerInitWithAfterMessageT_BB_03_9B<0xB4>
prepare_server_init_contents_bb(
    const parray<uint8_t, 0x30>& server_key,
    const parray<uint8_t, 0x30>& client_key,
    uint8_t flags) {
  bool use_secondary_message = (flags & SendServerInitFlag::USE_SECONDARY_MESSAGE);
  S_ServerInitWithAfterMessageT_BB_03_9B<0xB4> cmd;
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
  phosg::random_data(server_key.data(), server_key.bytes());
  phosg::random_data(client_key.data(), client_key.bytes());
  auto cmd = prepare_server_init_contents_bb(server_key, client_key, flags);
  send_command_t(c, use_secondary_message ? 0x9B : 0x03, 0x00, cmd);

  static const string primary_expected_first_data("\xB4\x00\x93\x00\x00\x00\x00\x00", 8);
  static const string secondary_expected_first_data("\xDC\x00\xDB\x00\x00\x00\x00\x00", 8);
  auto detector_crypt = make_shared<PSOBBMultiKeyDetectorEncryption>(
      c->require_server_state()->bb_private_keys,
      bb_crypt_initial_client_commands,
      cmd.basic_cmd.client_key.data(),
      sizeof(cmd.basic_cmd.client_key));
  c->channel.crypt_in = detector_crypt;
  c->channel.crypt_out = make_shared<PSOBBMultiKeyImitatorEncryption>(
      detector_crypt, cmd.basic_cmd.server_key.data(),
      sizeof(cmd.basic_cmd.server_key), true);
}

void send_server_init(shared_ptr<Client> c, uint8_t flags) {
  switch (c->version()) {
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::PC_NTE:
    case Version::PC_V2:
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
    case Version::XB_V3:
      send_server_init_dc_pc_v3(c, flags);
      break;
    case Version::BB_V4:
      send_server_init_bb(c, flags);
      break;
    default:
      throw logic_error("unimplemented versioned command");
  }
}

void send_update_client_config(shared_ptr<Client> c, bool always_send) {
  if (always_send || (is_v3(c->version()) && (c->config.should_update_vs(c->synced_config)))) {
    switch (c->version()) {
      case Version::DC_NTE:
      case Version::DC_11_2000:
      case Version::DC_V1:
      case Version::DC_V2:
      case Version::PC_NTE:
      case Version::PC_V2: {
        if (!c->config.check_flag(Client::Flag::HAS_GUILD_CARD_NUMBER)) {
          c->config.set_flag(Client::Flag::HAS_GUILD_CARD_NUMBER);
          S_UpdateClientConfig_DC_PC_04 cmd;
          cmd.player_tag = 0x00010000;
          cmd.guild_card_number = c->login->account->account_id;
          send_command_t(c, 0x04, 0x00, cmd);
        }
        break;
      }
      case Version::GC_NTE:
      case Version::GC_V3:
      case Version::GC_EP3_NTE:
      case Version::GC_EP3:
      case Version::XB_V3: {
        c->config.set_flag(Client::Flag::HAS_GUILD_CARD_NUMBER);
        S_UpdateClientConfig_V3_04 cmd;
        cmd.player_tag = 0x00010000;
        cmd.guild_card_number = c->login->account->account_id;
        c->config.serialize_into(cmd.client_config);
        send_command_t(c, 0x04, 0x00, cmd);
        break;
      }
      default:
        throw logic_error("send_update_client_config called on incorrect game version");
    }
    c->synced_config = c->config;
  }
}

void send_quest_buffer_overflow(shared_ptr<Client> c) {
  // PSO Episode 3 USA doesn't natively support the B2 command, but we can add
  // it back to the game with some tricky commands. For details on how this
  // works, see system/client-functions/Episode3USAQuestBufferOverflow.ppc.s.
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

void prepare_client_for_patches(shared_ptr<Client> c, function<void()> on_complete) {
  auto s = c->require_server_state();

  auto send_version_detect = [s, wc = weak_ptr<Client>(c), on_complete]() -> void {
    auto c = wc.lock();
    if (!c) {
      return;
    }
    const char* version_detect_name = nullptr;
    if (c->version() == Version::DC_V2) {
      version_detect_name = "VersionDetectDC";
    } else if (is_gc(c->version())) {
      version_detect_name = "VersionDetectGC";
    } else if (c->version() == Version::XB_V3) {
      version_detect_name = "VersionDetectXB";
    }
    if (version_detect_name && specific_version_is_indeterminate(c->config.specific_version)) {
      send_function_call(c, s->function_code_index->name_to_function.at(version_detect_name));
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
    auto fn = s->function_code_index->name_to_function.at("CacheClearFix-Phase1");
    send_function_call(c, fn, {}, nullptr, 0, 0x80000000, 8, 0x7F2734EC);
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
        send_update_client_config(c, false);
        send_version_detect();
      });
    });
  } else {
    send_version_detect();
  }
}

string prepare_send_function_call_data(
    shared_ptr<const CompiledFunctionCode> code,
    const unordered_map<string, uint32_t>& label_writes,
    const void* suffix_data,
    size_t suffix_size,
    uint32_t checksum_addr,
    uint32_t checksum_size,
    uint32_t override_relocations_offset,
    bool use_encrypted_format) {
  string data;
  if (code.get()) {
    data = code->generate_client_command(label_writes, suffix_data, suffix_size, override_relocations_offset);

    if (use_encrypted_format) {
      uint32_t key = phosg::random_object<uint32_t>();

      // This format was probably never used on any little-endian system, but we
      // implement the way it would probably work there if it was used.
      phosg::StringWriter w;
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

  phosg::StringWriter w;
  w.put(S_ExecuteCode_B2{data.size(), checksum_addr, checksum_size});
  w.write(data);
  return std::move(w.str());
}

void send_function_call(
    shared_ptr<Client> c,
    shared_ptr<const CompiledFunctionCode> code,
    const unordered_map<string, uint32_t>& label_writes,
    const void* suffix_data,
    size_t suffix_size,
    uint32_t checksum_addr,
    uint32_t checksum_size,
    uint32_t override_relocations_offset) {
  return send_function_call(
      c->channel,
      c->config,
      code,
      label_writes,
      suffix_data,
      suffix_size,
      checksum_addr,
      checksum_size,
      override_relocations_offset);
}

void send_function_call(
    Channel& ch,
    const Client::Config& client_config,
    shared_ptr<const CompiledFunctionCode> code,
    const unordered_map<string, uint32_t>& label_writes,
    const void* suffix_data,
    size_t suffix_size,
    uint32_t checksum_addr,
    uint32_t checksum_size,
    uint32_t override_relocations_offset) {
  if (!client_config.check_flag(Client::Flag::HAS_SEND_FUNCTION_CALL)) {
    throw logic_error("client does not support function calls");
  }
  if (code.get() && client_config.check_flag(Client::Flag::SEND_FUNCTION_CALL_CHECKSUM_ONLY)) {
    throw logic_error("client only supports checksums in send_function_call");
  }

  string data = prepare_send_function_call_data(
      code, label_writes, suffix_data, suffix_size, checksum_addr, checksum_size, override_relocations_offset,
      client_config.check_flag(Client::Flag::ENCRYPTED_SEND_FUNCTION_CALL));

  ch.send(0xB2, code ? code->index : 0x00, data);
}

bool send_protected_command(std::shared_ptr<Client> c, const void* data, size_t size, bool echo_to_lobby) {
  switch (c->version()) {
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::PC_NTE:
    case Version::PC_V2:
    case Version::GC_NTE:
      if (echo_to_lobby) {
        send_command(c->require_lobby(), 0x60, 0x00, data, size);
      } else {
        send_command(c, 0x60, 0x00, data, size);
      }
      return true;

    case Version::GC_V3:
    case Version::XB_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
    case Version::BB_V4: {
      auto s = c->require_server_state();
      if (!s->enable_v3_v4_protected_subcommands ||
          !c->config.check_flag(Client::Flag::HAS_SEND_FUNCTION_CALL) ||
          c->config.check_flag(Client::Flag::SEND_FUNCTION_CALL_CHECKSUM_ONLY)) {
        return false;
      }

      prepare_client_for_patches(c, [wc = weak_ptr<Client>(c), data = string(reinterpret_cast<const char*>(data), size), echo_to_lobby]() {
        auto c = wc.lock();
        if (!c) {
          return;
        }
        try {
          auto s = c->require_server_state();
          auto fn = s->function_code_index->get_patch("CallProtectedHandler", c->config.specific_version);
          send_function_call(c, fn, {{"size", data.size()}}, data.data(), data.size());
          c->function_call_response_queue.emplace_back(empty_function_call_response_handler);
          if (echo_to_lobby) {
            auto l = c->lobby.lock();
            if (l) {
              send_command_excluding_client(l, c, 0x60, 0x00, data.data(), data.size());
            }
          }
        } catch (const exception& e) {
          c->log.warning("Failed to send protected command: %s", e.what());
        }
      });
      return true;
    }

    default:
      return false;
  }
}

void send_reconnect(shared_ptr<Client> c, uint32_t address, uint16_t port) {
  S_Reconnect_19 cmd = {address, port, 0};
  send_command_t(c, is_patch(c->version()) ? 0x14 : 0x19, 0x00, cmd);
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

static void scramble_bb_security_data(parray<uint8_t, 0x28>& data, uint8_t which, bool reverse) {
  static const uint8_t forward_orders[8][5] = {
      {2, 0, 1, 4, 3},
      {3, 4, 0, 1, 2},
      {2, 3, 4, 0, 1},
      {2, 3, 0, 1, 4},
      {0, 2, 3, 4, 1},
      {1, 4, 2, 3, 0},
      {2, 0, 1, 4, 3},
      {1, 0, 3, 4, 2},
  };
  static const uint8_t reverse_orders[8][5] = {
      {1, 2, 0, 4, 3},
      {2, 3, 4, 0, 1},
      {3, 4, 0, 1, 2},
      {2, 3, 0, 1, 4},
      {0, 4, 1, 2, 3},
      {4, 0, 2, 3, 1},
      {1, 2, 0, 4, 3},
      {1, 0, 4, 2, 3},
  };
  const auto& order = reverse ? reverse_orders[which & 7] : forward_orders[which & 7];
  parray<uint8_t, 0x28> scrambled_data;
  for (size_t z = 0; z < 5; z++) {
    for (size_t x = 0; x < 8; x++) {
      scrambled_data[(z * 8) + x] = data[(order[z] * 8) + x];
    }
  }
  data = scrambled_data;
}

void send_client_init_bb(shared_ptr<Client> c, uint32_t error_code) {
  auto team = c->team();
  S_ClientInit_BB_00E6 cmd;
  cmd.error_code = error_code;
  cmd.player_tag = 0x00010000;
  cmd.guild_card_number = c->login->account->account_id;
  cmd.security_token = team ? team->team_id : 0;
  c->config.serialize_into(cmd.client_config);
  cmd.can_create_team = 1;
  cmd.episode_4_unlocked = 1;

  // If security_token is zero, the game scrambles the client config data based
  // on the first character in the username. We undo the scramble here, so when
  // the client scrambles the data upon receipt, it will be correct when it next
  // is sent back to the server.
  if (cmd.security_token == 0 && c->login && c->login->bb_license) {
    scramble_bb_security_data(cmd.client_config, c->login->bb_license->username.at(0), true);
  }

  send_command_t(c, 0x00E6, 0x00000000, cmd);
}

void send_system_file_bb(shared_ptr<Client> c) {
  auto team = c->team();

  S_SyncSystemFile_BB_E2 cmd;
  cmd.system_file = *c->system_file();
  if (team) {
    cmd.team_membership = team->membership_for_member(c->login->account->account_id);
  }
  send_command_t(c, 0x00E2, 0x00000000, cmd);
}

void send_player_preview_bb(shared_ptr<Client> c, int8_t character_index, const PlayerDispDataBBPreview* preview) {
  if (!preview) {
    // no player exists
    S_PlayerPreview_NoPlayer_BB_00E4 cmd = {character_index, 0x00000002};
    send_command_t(c, 0x00E4, 0x00000000, cmd);

  } else {
    SC_PlayerPreview_CreateCharacter_BB_00E5 cmd = {character_index, *preview};
    send_command_t(c, 0x00E5, 0x00000000, cmd);
  }
}

void send_guild_card_header_bb(shared_ptr<Client> c) {
  uint32_t checksum = c->guild_card_file()->checksum();
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
      reinterpret_cast<const uint8_t*>(c->guild_card_file().get()) + chunk_offset,
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

void send_stream_file_index_bb(shared_ptr<Client> c) {
  auto s = c->require_server_state();

  vector<S_StreamFileIndexEntry_BB_01EB> entries;
  size_t offset = 0;
  for (const string& filename : stream_file_entries) {
    string key = "system/blueburst/" + filename;
    auto cache_res = s->bb_stream_files_cache->get_or_load(key);
    auto& e = entries.emplace_back();
    e.size = cache_res.file->data->size();
    // Computing the checksum can be slow, so we cache it along with the file
    // data. If the cache result was just populated, then it may be different,
    // so we always recompute the checksum in that case.
    if (cache_res.generate_called) {
      e.checksum = crc32(cache_res.file->data->data(), e.size);
      s->bb_stream_files_cache->replace_obj<uint32_t>(key + ".crc32", e.checksum);
    } else {
      auto compute_checksum = [&](const string&) -> uint32_t {
        return crc32(cache_res.file->data->data(), e.size);
      };
      e.checksum = s->bb_stream_files_cache->get_obj<uint32_t>(key + ".crc32", compute_checksum).obj;
    }
    e.offset = offset;
    e.filename.encode(filename);
    offset += e.size;
  }
  send_command_vt(c, 0x01EB, entries.size(), entries);
}

void send_stream_file_chunk_bb(shared_ptr<Client> c, uint32_t chunk_index) {
  auto s = c->require_server_state();

  auto cache_result = s->bb_stream_files_cache->get(
      "<BB stream file>", [&](const string&) -> string {
        size_t bytes = 0;
        for (const auto& name : stream_file_entries) {
          bytes += s->bb_stream_files_cache->get_or_load("system/blueburst/" + name).file->data->size();
        }

        string ret;
        ret.reserve(bytes);
        for (const auto& name : stream_file_entries) {
          ret += *s->bb_stream_files_cache->get_or_load("system/blueburst/" + name).file->data;
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
  S_ApprovePlayerChoice_BB_00E4 cmd = {c->bb_character_index, 1};
  send_command_t(c, 0x00E4, 0x00000000, cmd);
}

void send_complete_player_bb(shared_ptr<Client> c) {
  auto p = c->character(true, false);
  auto sys = c->system_file(true);
  auto team = c->team();
  if (c->config.check_flag(Client::Flag::FORCE_ENGLISH_LANGUAGE_BB)) {
    p->inventory.language = 1;
    p->guild_card.language = 1;
    sys->language = 1;
  }

  SC_SyncSaveFiles_BB_E7 cmd;
  cmd.char_file = *p;
  cmd.system_file = *sys;
  if (team) {
    cmd.team_membership = team->membership_for_member(c->login->account->account_id);
  }
  send_command_t(c, 0x00E7, 0x00000000, cmd);
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
    phosg::StringWriter& w,
    uint16_t command,
    uint32_t flag,
    const string& text,
    ColorMode color_mode) {
  bool is_w = uses_utf16(ch.version);
  if (ch.version == Version::DC_NTE) {
    color_mode = ColorMode::STRIP;
  }

  try {
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
  } catch (const runtime_error& e) {
    phosg::log_warning("Failed to encode message for %02hX command: %s", command, e.what());
    return;
  }

  if (is_w) {
    w.put_u16(0);
  } else {
    w.put_u8(0);
  }

  while (w.str().size() & 3) {
    w.put_u8(0);
  }
  ch.send(command, flag, w.str());
}

static void send_text(Channel& ch, uint16_t command, uint32_t flag, const string& text, ColorMode color_mode) {
  phosg::StringWriter w;
  send_text(ch, w, command, flag, text, color_mode);
}

static void send_header_text(Channel& ch, uint16_t command, uint32_t guild_card_number, const string& text, ColorMode color_mode) {
  phosg::StringWriter w;
  w.put(SC_TextHeader_01_06_11_B0_EE({0, guild_card_number}));
  send_text(ch, w, command, 0x00, text, color_mode);
}

void send_message_box(shared_ptr<Client> c, const string& text) {
  uint16_t command;
  switch (c->version()) {
    case Version::PC_PATCH:
    case Version::BB_PATCH:
      command = 0x13;
      break;
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::PC_NTE:
    case Version::PC_V2:
      command = 0x1A;
      break;
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
    case Version::XB_V3:
    case Version::BB_V4:
      command = 0xD5;
      break;
    default:
      throw logic_error("invalid game version");
  }
  send_text(c->channel, command, 0x00, text, ColorMode::ADD);
}

void send_ep3_timed_message_box(Channel& ch, uint32_t frames, const string& message) {
  string encoded;
  try {
    encoded = tt_encode_marked(add_color(message), ch.language, false);
  } catch (const runtime_error& e) {
    phosg::log_warning("Failed to encode message for EA command: %s", e.what());
    return;
  }
  phosg::StringWriter w;
  w.put<S_TimedMessageBoxHeader_Ep3_EA>({frames});
  w.write(encoded);
  w.put_u8(0);
  while (w.size() & 3) {
    w.put_u8(0);
  }
  ch.send(0xEA, 0x00, w.str());
}

void send_lobby_name(shared_ptr<Client> c, const string& text) {
  send_text(c->channel, 0x8A, 0x00, text, ColorMode::NONE);
}

void send_quest_info(shared_ptr<Client> c, const string& text, uint8_t description_flag, bool is_download_quest) {
  send_text(c->channel, is_download_quest ? 0xA5 : 0xA3, description_flag, text, ColorMode::ADD);
}

void send_lobby_message_box(shared_ptr<Client> c, const string& text, bool left_side_on_bb) {
  uint16_t command = (left_side_on_bb && (c->version() == Version::BB_V4)) ? 0x0101 : 0x0001;
  send_header_text(c->channel, command, 0, text, ColorMode::ADD);
}

void send_ship_info(shared_ptr<Client> c, const string& text) {
  send_header_text(c->channel, 0x11, 0, text, ColorMode::ADD);
}

void send_ship_info(Channel& ch, const string& text) {
  send_header_text(ch, 0x11, 0, text, ColorMode::ADD);
}

void send_text_message(Channel& ch, const string& text) {
  if ((ch.version != Version::DC_NTE) && (ch.version != Version::DC_11_2000)) {
    send_header_text(ch, 0xB0, 0, text, ColorMode::ADD);
  }
}

void send_text_message(shared_ptr<Client> c, const string& text) {
  if ((c->version() != Version::DC_NTE) && (c->version() != Version::DC_11_2000)) {
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
  for (auto& it : s->channel_to_client) {
    if (it.second->login && !is_patch(it.second->version())) {
      send_text_message(it.second, text);
    }
  }
}

__attribute__((format(printf, 2, 3))) void send_ep3_text_message_printf(shared_ptr<ServerState> s, const char* format, ...) {
  va_list va;
  va_start(va, format);
  string buf = phosg::string_vprintf(format, va);
  va_end(va);
  for (auto& it : s->id_to_lobby) {
    for (auto& c : it.second->clients) {
      if (c && is_ep3(c->version())) {
        send_text_message(c, buf);
      }
    }
  }
}

void send_scrolling_message_bb(shared_ptr<Client> c, const string& text) {
  if (c->version() != Version::BB_V4) {
    throw logic_error("cannot send scrolling message to non-BB player");
  }
  send_header_text(c->channel, 0x00EE, 0, text, ColorMode::ADD);
}

void send_text_or_scrolling_message(shared_ptr<Client> c, const string& text, const string& scrolling) {
  if (is_v4(c->version())) {
    send_scrolling_message_bb(c, scrolling);
  } else {
    send_text_message(c, text);
  }
}

void send_text_or_scrolling_message(
    std::shared_ptr<Lobby> l, std::shared_ptr<Client> exclude_c, const std::string& text, const std::string& scrolling) {
  for (const auto& lc : l->clients) {
    if (!lc || (lc == exclude_c)) {
      continue;
    }
    send_text_or_scrolling_message(lc, text, scrolling);
  }
}

void send_text_or_scrolling_message(shared_ptr<ServerState> s, const std::string& text, const std::string& scrolling) {
  for (const auto& it : s->channel_to_client) {
    if (it.second->login && !is_patch(it.second->version())) {
      send_text_or_scrolling_message(it.second, text, scrolling);
    }
  }
}

string prepare_chat_data(
    Version version,
    uint8_t language,
    uint8_t from_client_id,
    const string& from_name,
    const string& text,
    char private_flags) {
  string data;

  if (version == Version::BB_V4) {
    data.append(language ? "\tE" : "\tJ");
  }
  data.append(from_name);
  if (version == Version::DC_NTE) {
    data.append(phosg::string_printf(">%X", from_client_id));
  } else {
    data.append(1, '\t');
  }
  if (private_flags) {
    data.append(1, static_cast<uint16_t>(private_flags));
  }

  if (uses_utf16(version)) {
    data.append(language ? "\tE" : "\tJ");
    data.append(text);
    return tt_utf8_to_utf16(data);
  } else if (version == Version::DC_NTE) {
    data.append(tt_utf8_to_sega_sjis(text));
    return data;
  } else {
    data.append(tt_encode_marked(text, language, false));
    return data;
  }
}

void send_chat_message_from_client(Channel& ch, const string& text, char private_flags) {
  if (private_flags != 0) {
    if (!is_ep3(ch.version)) {
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
  phosg::StringWriter w;
  w.put(SC_TextHeader_01_06_11_B0_EE{0, from_guild_card_number});
  w.write(prepared_data);
  w.put_u8(0);
  if (uses_utf16(c->version())) {
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

void send_chat_message(
    shared_ptr<Client> c,
    uint32_t from_guild_card_number,
    const string& from_name,
    const string& text,
    char private_flags) {
  string prepared_data = prepare_chat_data(
      c->version(),
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
  cmd.player_tag = from_guild_card_number ? 0x00010000 : 0;
  cmd.from_guild_card_number = from_guild_card_number;
  cmd.from_name.encode(from_name, c->language());
  cmd.to_guild_card_number = c->login->account->account_id;
  cmd.text.encode(text, c->language());
  send_command_t(c, 0x81, 0x00, cmd);
}

void send_simple_mail_bb(shared_ptr<Client> c, uint32_t from_guild_card_number, const string& from_name, const string& text) {
  SC_SimpleMail_BB_81 cmd;
  cmd.player_tag = from_guild_card_number ? 0x00010000 : 0;
  cmd.from_guild_card_number = from_guild_card_number;
  cmd.from_name.encode(from_name, c->language());
  cmd.to_guild_card_number = c->login->account->account_id;
  cmd.received_date.encode(phosg::format_time(phosg::now()), c->language());
  cmd.text.encode(text, c->language());
  send_command_t(c, 0x81, 0x00, cmd);
}

void send_simple_mail(shared_ptr<Client> c, uint32_t from_guild_card_number, const string& from_name, const string& text) {
  switch (c->version()) {
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
    case Version::XB_V3:
      send_simple_mail_t<SC_SimpleMail_DC_V3_81>(c, from_guild_card_number, from_name, text);
      break;
    case Version::PC_NTE:
    case Version::PC_V2:
      send_simple_mail_t<SC_SimpleMail_PC_81>(c, from_guild_card_number, from_name, text);
      break;
    case Version::BB_V4:
      send_simple_mail_bb(c, from_guild_card_number, from_name, text);
      break;
    default:
      throw logic_error("unimplemented versioned command");
  }
}

void send_simple_mail(shared_ptr<ServerState> s, uint32_t from_guild_card_number, const string& from_name, const string& text) {
  for (const auto& it : s->channel_to_client) {
    if (it.second->login && !is_patch(it.second->version())) {
      send_simple_mail(it.second, from_guild_card_number, from_name, text);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// info board

template <TextEncoding NameEncoding, TextEncoding MessageEncoding>
void send_info_board_t(shared_ptr<Client> c) {
  vector<S_InfoBoardEntryT_D8<NameEncoding, MessageEncoding>> entries;
  auto l = c->require_lobby();
  for (const auto& other_c : l->clients) {
    if (!other_c.get()) {
      continue;
    }
    auto other_p = other_c->character(true, false);
    auto& e = entries.emplace_back();
    e.name.encode(other_p->disp.name.decode(other_p->inventory.language), c->language());
    e.message.encode(add_color(other_p->info_board.decode(other_p->inventory.language)), c->language());
  }
  send_command_vt(c, 0xD8, entries.size(), entries);
}

void send_info_board(shared_ptr<Client> c) {
  if (uses_utf16(c->version())) {
    send_info_board_t<TextEncoding::UTF16, TextEncoding::UTF16>(c);
  } else {
    send_info_board_t<TextEncoding::ASCII, TextEncoding::MARKED>(c);
  }
}

template <typename CmdT>
void send_choice_search_choices_t(shared_ptr<Client> c) {
  vector<CmdT> entries;
  for (const auto& cat : CHOICE_SEARCH_CATEGORIES) {
    auto& cat_e = entries.emplace_back();
    cat_e.parent_choice_id = 0;
    cat_e.choice_id = cat.id;
    cat_e.text.encode(cat.name, c->language());
    for (const auto& choice : cat.choices) {
      auto& e = entries.emplace_back();
      e.parent_choice_id = cat.id;
      e.choice_id = choice.id;
      e.text.encode(choice.name, c->language());
    }
  }
  send_command_vt(c, 0xC0, entries.size(), entries);
}

void send_choice_search_choices(shared_ptr<Client> c) {
  switch (c->version()) {
      // DC V1 and the prototypes do not support this command
    case Version::DC_V2:
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
    case Version::XB_V3:
      send_choice_search_choices_t<S_ChoiceSearchEntry_DC_V3_C0>(c);
      break;
    case Version::PC_NTE:
    case Version::PC_V2:
    case Version::BB_V4:
      send_choice_search_choices_t<S_ChoiceSearchEntry_PC_BB_C0>(c);
      break;
    default:
      throw logic_error("unimplemented versioned command");
  }
}

template <typename CommandHeaderT, TextEncoding Encoding>
void send_card_search_result_t(
    shared_ptr<Client> c,
    shared_ptr<Client> result,
    shared_ptr<Lobby> result_lobby) {
  auto s = c->require_server_state();
  string port_name = lobby_port_name_for_version(c->version());

  S_GuildCardSearchResultT<CommandHeaderT, Encoding> cmd;
  cmd.player_tag = 0x00010000;
  cmd.searcher_guild_card_number = c->login->account->account_id;
  cmd.result_guild_card_number = result->login->account->account_id;
  cmd.reconnect_command_header.size = sizeof(cmd.reconnect_command_header) + sizeof(cmd.reconnect_command);
  cmd.reconnect_command_header.command = 0x19;
  cmd.reconnect_command_header.flag = 0x00;
  cmd.reconnect_command.address = s->connect_address_for_client(c);
  cmd.reconnect_command.port = s->name_to_port_config.at(port_name)->port;
  cmd.reconnect_command.unused = 0;

  string location_string;
  if (result_lobby->is_game()) {
    location_string = phosg::string_printf("%s,,BLOCK01,%s", result_lobby->name.c_str(), s->name.c_str());
  } else if (result_lobby->is_ep3()) {
    location_string = phosg::string_printf("BLOCK01-C%02" PRIu32 ",,BLOCK01,%s", result_lobby->lobby_id - 15, s->name.c_str());
  } else {
    location_string = phosg::string_printf("BLOCK01-%02" PRIu32 ",,BLOCK01,%s", result_lobby->lobby_id, s->name.c_str());
  }
  cmd.location_string.encode(location_string, c->language());
  cmd.extension.lobby_refs[0].menu_id = MenuID::LOBBY;
  cmd.extension.lobby_refs[0].item_id = result_lobby->lobby_id;
  auto rp = result->character(true, false);
  cmd.extension.player_name.encode(rp->disp.name.decode(rp->inventory.language), c->language());

  send_command_t(c, 0x41, 0x00, cmd);
}

void send_card_search_result(
    shared_ptr<Client> c,
    shared_ptr<Client> result,
    shared_ptr<Lobby> result_lobby) {
  switch (c->version()) {
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
    case Version::XB_V3:
      send_card_search_result_t<PSOCommandHeaderDCV3, TextEncoding::SJIS>(c, result, result_lobby);
      break;
    case Version::PC_NTE:
    case Version::PC_V2:
      send_card_search_result_t<PSOCommandHeaderPC, TextEncoding::UTF16>(c, result, result_lobby);
      break;
    case Version::BB_V4:
      send_card_search_result_t<PSOCommandHeaderBB, TextEncoding::UTF16>(c, result, result_lobby);
      break;
    default:
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
    case Version::DC_NTE:
      send_guild_card_dc_pc_gc_t<G_SendGuildCard_DCNTE_6x06>(
          ch, guild_card_number, name, description, language, section_id, char_class);
      break;
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
      send_guild_card_dc_pc_gc_t<G_SendGuildCard_DC_6x06>(
          ch, guild_card_number, name, description, language, section_id, char_class);
      break;
    case Version::PC_NTE:
    case Version::PC_V2:
      send_guild_card_dc_pc_gc_t<G_SendGuildCard_PC_6x06>(
          ch, guild_card_number, name, description, language, section_id, char_class);
      break;
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      send_guild_card_dc_pc_gc_t<G_SendGuildCard_GC_6x06>(
          ch, guild_card_number, name, description, language, section_id, char_class);
      break;
    case Version::XB_V3:
      send_guild_card_xb(
          ch, guild_card_number, xb_user_id, name, description, language, section_id, char_class);
      break;
    case Version::BB_V4:
      send_guild_card_bb(ch, guild_card_number, name, team_name, description, language, section_id, char_class);
      break;
    default:
      throw logic_error("unimplemented versioned command");
  }
}

void send_guild_card(shared_ptr<Client> c, shared_ptr<Client> source) {
  if (!source->login) {
    throw runtime_error("source player does not have an account");
  }

  auto source_p = source->character(true, false);
  auto source_team = source->team();

  uint64_t xb_user_id = (source->login->xb_license && source->login->xb_license->user_id)
      ? source->login->xb_license->user_id
      : (0xAE00000000000000ULL | source->login->account->account_id);

  send_guild_card(
      c->channel,
      source->login->account->account_id,
      xb_user_id,
      source_p->disp.name.decode(source->language()),
      source_team ? source_team->name : "",
      source_p->guild_card.description.decode(source->language()),
      source->language(),
      source_p->disp.visual.section_id,
      source_p->disp.visual.char_class);
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
    e.difficulty_tag = 0x04;
    e.name.encode(menu->name, c->language());
  }

  for (const auto& item : menu->items) {
    bool is_visible = true;
    switch (c->version()) {
      case Version::DC_NTE:
      case Version::DC_11_2000:
        is_visible &= !(item.flags & MenuItem::Flag::INVISIBLE_ON_DC_PROTOS);
        [[fallthrough]];
      case Version::DC_V1:
      case Version::DC_V2:
        is_visible &= !(item.flags & MenuItem::Flag::INVISIBLE_ON_DC);
        break;
      case Version::PC_NTE:
        is_visible &= !(item.flags & MenuItem::Flag::INVISIBLE_ON_PC_NTE);
        [[fallthrough]];
      case Version::PC_V2:
        is_visible &= !(item.flags & MenuItem::Flag::INVISIBLE_ON_PC);
        break;
      case Version::GC_NTE:
        is_visible &= !(item.flags & MenuItem::Flag::INVISIBLE_ON_GC_NTE);
        [[fallthrough]];
      case Version::GC_V3:
      case Version::GC_EP3_NTE:
      case Version::GC_EP3:
        is_visible &= !(item.flags & MenuItem::Flag::INVISIBLE_ON_GC);
        break;
      case Version::XB_V3:
        is_visible &= !(item.flags & MenuItem::Flag::INVISIBLE_ON_XB);
        break;
      case Version::BB_V4:
        is_visible &= !(item.flags & MenuItem::Flag::INVISIBLE_ON_BB);
        break;
      default:
        throw runtime_error("menus not supported for this game version");
    }
    if (item.flags & MenuItem::Flag::REQUIRES_MESSAGE_BOXES) {
      is_visible &= !c->config.check_flag(Client::Flag::NO_D6);
    }
    if (item.flags & MenuItem::Flag::REQUIRES_SEND_FUNCTION_CALL) {
      is_visible &= c->config.check_flag(Client::Flag::HAS_SEND_FUNCTION_CALL);
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
      e.name.encode(item.name, c->language());
      e.difficulty_tag = 0x04;
      e.num_players = (c->version() == Version::BB_V4) ? 0x00 : 0x0F;
    }
  }

  send_command_vt(c, is_info_menu ? 0x1F : 0x07, entries.size() - 1, entries);
  c->last_menu_sent = menu;
}

void send_menu(shared_ptr<Client> c, shared_ptr<const Menu> menu, bool is_info_menu) {
  if (uses_utf16(c->version())) {
    send_menu_t<S_MenuItem_PC_BB_08>(c, menu, is_info_menu);
  } else {
    send_menu_t<S_MenuItem_DC_V3_08_Ep3_E6>(c, menu, is_info_menu);
  }
}

template <TextEncoding Encoding>
void send_game_menu_t(
    shared_ptr<Client> c,
    bool is_spectator_team_list,
    bool show_tournaments_only) {
  auto s = c->require_server_state();

  vector<S_MenuItemT<Encoding>> entries;
  {
    auto& e = entries.emplace_back();
    e.menu_id = MenuID::GAME;
    e.item_id = 0x00000000;
    e.difficulty_tag = 0x00;
    e.num_players = 0x00;
    e.name.encode(s->name, c->language());
    e.episode = 0x00;
    e.flags = 0x04;
  }

  set<shared_ptr<const Lobby>, bool (*)(const shared_ptr<const Lobby>&, const shared_ptr<const Lobby>&)> games(Lobby::compare_shared);
  bool client_has_debug = c->config.check_flag(Client::Flag::DEBUG_ENABLED);
  for (shared_ptr<Lobby> l : s->all_lobbies()) {
    if (l->is_game() &&
        (client_has_debug || l->version_is_allowed(c->version())) &&
        (client_has_debug || (l->check_flag(Lobby::Flag::IS_CLIENT_CUSTOMIZATION) == c->config.check_flag(Client::Flag::IS_CLIENT_CUSTOMIZATION))) &&
        (l->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM) == is_spectator_team_list) &&
        (!show_tournaments_only || l->tournament_match)) {
      games.emplace(l);
    }
  }

  for (const auto& l : games) {
    if (entries.size() >= 0x41) {
      break;
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
    e.item_id = l->lobby_id;
    e.difficulty_tag = (is_ep3(c->version()) ? 0x0A : (l->difficulty + 0x22));
    e.num_players = l->count_clients();
    if (is_dc(c->version())) {
      e.episode = l->version_is_allowed(Version::DC_V1) ? 1 : 0;
    } else {
      e.episode = ((c->version() == Version::BB_V4) ? (l->max_clients << 4) : 0) | episode_num;
    }
    if (l->is_ep3()) {
      e.flags = (l->password.empty() ? 0 : 2) | (l->check_flag(Lobby::Flag::BATTLE_IN_PROGRESS) ? 4 : 0);
    } else {
      e.flags = (l->password.empty() ? 0 : 2);
      if ((c->version() == Version::GC_NTE) || !is_v1_or_v2(c->version())) {
        e.flags |= (episode_num << 6);
      }
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
          e.episode = 0x10 | episode_num;
          break;
        default:
          throw logic_error("invalid game mode");
      }
      // On v2, render name in orange if v1 is not allowed
      if (is_v2(c->version()) && !l->version_is_allowed(Version::DC_V1)) {
        e.flags |= 0x40;
      }
      // On BB, gray out games that can't be joined
      if ((c->version() == Version::BB_V4) && (l->join_error_for_client(c, nullptr) != Lobby::JoinError::ALLOWED)) {
        e.flags |= 0x04;
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
  if (is_v4(c->version())) {
    send_game_menu_t<TextEncoding::UTF16_ALWAYS_MARKED>(c, is_spectator_team_list, show_tournaments_only);
  } else if (uses_utf16(c->version())) {
    send_game_menu_t<TextEncoding::UTF16>(c, is_spectator_team_list, show_tournaments_only);
  } else {
    send_game_menu_t<TextEncoding::MARKED>(c, is_spectator_team_list, show_tournaments_only);
  }
}

template <typename EntryT>
void send_quest_menu_t(
    shared_ptr<Client> c,
    const vector<pair<QuestIndex::IncludeState, shared_ptr<const Quest>>>& quests,
    bool is_download_menu) {
  auto v = c->version();
  vector<EntryT> entries;
  for (const auto& it : quests) {
    auto vq = it.second->version(v, c->language());
    if (!vq) {
      continue;
    }

    auto& e = entries.emplace_back();
    e.menu_id = ((it.second->episode == Episode::EP1) || (it.second->episode == Episode::EP3)) ? MenuID::QUEST_EP1 : MenuID::QUEST_EP2;
    e.item_id = it.second->quest_number;
    e.name.encode(vq->name, c->language());
    e.short_description.encode(add_color(vq->short_description), c->language());
  }
  send_command_vt(c, is_download_menu ? 0xA4 : 0xA2, entries.size(), entries);
}

void send_quest_menu_bb(
    shared_ptr<Client> c,
    const vector<pair<QuestIndex::IncludeState, shared_ptr<const Quest>>>& quests,
    bool is_download_menu) {
  auto v = c->version();
  vector<S_QuestMenuEntry_BB_A2_A4> entries;
  for (const auto& it : quests) {
    auto vq = it.second->version(v, c->language());
    if (!vq) {
      continue;
    }

    auto& e = entries.emplace_back();
    e.menu_id = (it.second->episode == Episode::EP1) ? MenuID::QUEST_EP1 : MenuID::QUEST_EP2;
    e.item_id = it.second->quest_number;
    e.name.encode(vq->name, c->language());
    e.short_description.encode(add_color(vq->short_description), c->language());
    e.disabled = (it.first == QuestIndex::IncludeState::DISABLED) ? 1 : 0;
  }
  send_command_vt(c, is_download_menu ? 0xA4 : 0xA2, entries.size(), entries);
}

template <typename EntryT>
void send_quest_categories_menu_t(
    shared_ptr<Client> c,
    shared_ptr<const QuestIndex> quest_index,
    QuestMenuType menu_type,
    Episode episode) {
  QuestIndex::IncludeCondition include_condition = nullptr;
  if (!c->login->account->check_flag(Account::Flag::DISABLE_QUEST_REQUIREMENTS)) {
    auto l = c->lobby.lock();
    include_condition = l ? l->quest_include_condition() : nullptr;
  }

  uint16_t version_flags = (1 << static_cast<size_t>(c->version()));
  auto l = c->lobby.lock();
  if (l) {
    version_flags |= l->quest_version_flags();
  }

  vector<EntryT> entries;
  for (const auto& cat : quest_index->categories(menu_type, episode, version_flags, include_condition)) {
    auto& e = entries.emplace_back();
    e.menu_id = cat->use_ep2_icon() ? MenuID::QUEST_CATEGORIES_EP2 : MenuID::QUEST_CATEGORIES_EP1;
    e.item_id = cat->category_id;
    e.name.encode(cat->name, c->language());
    e.short_description.encode(add_color(cat->description), c->language());
  }

  bool is_download_menu = (menu_type == QuestMenuType::DOWNLOAD) || (menu_type == QuestMenuType::EP3_DOWNLOAD);
  send_command_vt(c, is_download_menu ? 0xA4 : 0xA2, entries.size(), entries);
}

void send_quest_menu(
    shared_ptr<Client> c,
    const vector<pair<QuestIndex::IncludeState, shared_ptr<const Quest>>>& quests,
    bool is_download_menu) {
  switch (c->version()) {
    case Version::PC_NTE:
    case Version::PC_V2:
      send_quest_menu_t<S_QuestMenuEntry_PC_A2_A4>(c, quests, is_download_menu);
      break;
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      send_quest_menu_t<S_QuestMenuEntry_DC_GC_A2_A4>(c, quests, is_download_menu);
      break;
    case Version::XB_V3:
      send_quest_menu_t<S_QuestMenuEntry_XB_A2_A4>(c, quests, is_download_menu);
      break;
    case Version::BB_V4:
      send_quest_menu_bb(c, quests, is_download_menu);
      break;
    default:
      throw logic_error("unimplemented versioned command");
  }
}

void send_quest_categories_menu(
    shared_ptr<Client> c,
    shared_ptr<const QuestIndex> quest_index,
    QuestMenuType menu_type,
    Episode episode) {
  switch (c->version()) {
    case Version::PC_NTE:
    case Version::PC_V2:
      send_quest_categories_menu_t<S_QuestMenuEntry_PC_A2_A4>(c, quest_index, menu_type, episode);
      break;
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      send_quest_categories_menu_t<S_QuestMenuEntry_DC_GC_A2_A4>(c, quest_index, menu_type, episode);
      break;
    case Version::XB_V3:
      send_quest_categories_menu_t<S_QuestMenuEntry_XB_A2_A4>(c, quest_index, menu_type, episode);
      break;
    case Version::BB_V4:
      send_quest_categories_menu_t<S_QuestMenuEntry_BB_A2_A4>(c, quest_index, menu_type, episode);
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
    if (!l->version_is_allowed(c->version())) {
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
    auto lp = lc->character(true, false);
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

template <typename LobbyDataT>
void populate_lobby_data_for_client(LobbyDataT& ret, shared_ptr<const Client> c, shared_ptr<const Client> viewer_c) {
  ret.player_tag = 0x00010000;
  ret.guild_card_number = c->login->account->account_id;
  ret.client_id = c->lobby_client_id;
  string name = c->character()->disp.name.decode(c->language());
  ret.name.encode(name, viewer_c->language());
}

template <>
void populate_lobby_data_for_client(PlayerLobbyDataXB& ret, shared_ptr<const Client> c, shared_ptr<const Client> viewer_c) {
  ret.player_tag = 0x00010000;
  ret.guild_card_number = c->login->account->account_id;
  if (c->xb_netloc) {
    ret.netloc = *c->xb_netloc;
  } else {
    ret.netloc.account_id = 0xAE00000000000000 | c->login->account->account_id;
  }
  ret.client_id = c->lobby_client_id;
  string name = c->character()->disp.name.decode(c->language());
  ret.name.encode(name, viewer_c->language());
}

template <>
void populate_lobby_data_for_client<PlayerLobbyDataBB>(PlayerLobbyDataBB& ret, shared_ptr<const Client> c, shared_ptr<const Client> viewer_c) {
  ret.player_tag = 0x00010000;
  ret.guild_card_number = c->login->account->account_id;
  ret.client_id = c->lobby_client_id;
  auto team = c->team();
  if (team) {
    ret.team_master_guild_card_number = team->master_account_id;
    ret.team_id = team->team_id;
  } else {
    ret.team_master_guild_card_number = 0;
    ret.team_id = 0;
  }
  string name = c->character()->disp.name.decode(c->language());
  ret.name.encode(name, viewer_c->language());
}

static void send_join_spectator_team(shared_ptr<Client> c, shared_ptr<Lobby> l) {
  if (!is_ep3(c->version())) {
    throw runtime_error("client is not Episode 3");
  }
  if (!l->is_ep3()) {
    throw runtime_error("lobby is not Episode 3");
  }
  if (!l->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM)) {
    throw runtime_error("lobby is not a spectator team");
  }

  auto s = c->require_server_state();

  S_JoinSpectatorTeam_Ep3_E8 cmd;

  cmd.variations = Variations();
  cmd.client_id = c->lobby_client_id;
  cmd.event = l->event;
  cmd.section_id = l->effective_section_id();
  cmd.random_seed = l->random_seed;
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
      auto wc_p = wc->character();
      auto& p = cmd.players[z];
      populate_lobby_data_for_client(p.lobby_data, wc, c);
      p.inventory = wc_p->inventory;
      p.inventory.encode_for_client(c->version(), s->item_parameter_table_for_encode(c->version()));
      p.disp = wc_p->disp.to_dcpcv3<false>(c->language(), p.inventory.language);
      p.disp.enforce_lobby_join_limits_for_version(c->version());

      auto& e = cmd.entries[z];
      e.player_tag = 0x00010000;
      e.guild_card_number = wc->login->account->account_id;
      e.name.encode(wc_p->disp.name.decode(wc_p->inventory.language), c->language());
      e.present = 1;
      e.level = wc->ep3_config
          ? (wc->ep3_config->online_clv_exp / 100)
          : wc_p->disp.stats.level.load();
      e.name_color = wc_p->disp.visual.name_color;

      uint32_t name_color = s->name_color_for_client(wc);
      if (name_color) {
        p.disp.visual.name_color = name_color;
        e.name_color = name_color;
      }

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
      p.inventory.encode_for_client(c->version(), s->item_parameter_table_for_encode(c->version()));
      p.disp = entry.disp;
      p.disp.enforce_lobby_join_limits_for_version(c->version());

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
      auto other_p = other_c->character();
      auto& cmd_p = cmd.spectator_players[z - 4];
      auto& cmd_e = cmd.entries[z];
      populate_lobby_data_for_client(cmd_p.lobby_data, other_c, c);
      cmd_p.inventory = other_p->inventory;
      cmd_p.disp = other_p->disp.to_dcpcv3<false>(c->language(), cmd_p.inventory.language);
      cmd_p.disp.enforce_lobby_join_limits_for_version(c->version());

      cmd_e.player_tag = 0x00010000;
      cmd_e.guild_card_number = other_c->login->account->account_id;
      cmd_e.name = cmd_p.lobby_data.name;
      cmd_e.present = 1;
      cmd_e.level = other_c->ep3_config
          ? (other_c->ep3_config->online_clv_exp / 100)
          : other_p->disp.stats.level.load();
      cmd_e.name_color = other_p->disp.visual.name_color;

      uint32_t name_color = s->name_color_for_client(other_c);
      if (name_color) {
        cmd_p.disp.visual.name_color = name_color;
        cmd_e.name_color = name_color;
      }

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
        populate_lobby_data_for_client(cmd.lobby_data[x], lc, c);
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
    cmd.section_id = l->effective_section_id();
    cmd.challenge_mode = (l->mode == GameMode::CHALLENGE) ? 1 : 0;
    cmd.random_seed = l->random_seed;
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
    case Version::DC_NTE:
    case Version::DC_11_2000: {
      S_JoinGame_DCNTE_64 cmd;
      cmd.client_id = c->lobby_client_id;
      cmd.leader_id = l->leader_id;
      cmd.disable_udp = 0x01;
      cmd.variations = l->variations;
      size_t player_count = populate_lobby_data(cmd);
      send_command_t(c, 0x64, player_count, cmd);
      break;
    }
    case Version::DC_V1:
    case Version::DC_V2: {
      S_JoinGame_DC_64 cmd;
      size_t player_count = populate_base_cmd(cmd);
      send_command_t(c, 0x64, player_count, cmd);
      break;
    }
    case Version::PC_NTE:
    case Version::PC_V2: {
      S_JoinGame_PC_64 cmd;
      size_t player_count = populate_base_cmd(cmd);
      send_command_t(c, 0x64, player_count, cmd);
      break;
    }
    case Version::GC_NTE:
    case Version::GC_V3: {
      S_JoinGame_GC_64 cmd;
      size_t player_count = populate_v3_cmd(cmd);
      send_command_t(c, 0x64, player_count, cmd);
      break;
    }
    case Version::GC_EP3_NTE:
    case Version::GC_EP3: {
      S_JoinGame_Ep3_64 cmd;
      size_t player_count = populate_v3_cmd(cmd);
      auto s = c->require_server_state();
      for (size_t x = 0; x < 4; x++) {
        auto lc = l->clients[x];
        if (lc) {
          auto other_p = lc->character();
          auto& cmd_p = cmd.players_ep3[x];
          cmd_p.inventory = other_p->inventory;
          cmd_p.inventory.encode_for_client(c->version(), s->item_parameter_table_for_encode(c->version()));
          cmd_p.disp = convert_player_disp_data<PlayerDispDataDCPCV3>(other_p->disp, c->language(), other_p->inventory.language);
          cmd_p.disp.enforce_lobby_join_limits_for_version(c->version());
          uint32_t name_color = s->name_color_for_client(lc);
          if (name_color) {
            cmd_p.disp.visual.name_color = name_color;
          }
        }
      }
      send_command_t(c, 0x64, player_count, cmd);
      break;
    }
    case Version::XB_V3: {
      S_JoinGame_XB_64 cmd;
      size_t player_count = populate_v3_cmd(cmd);
      send_command_t(c, 0x64, player_count, cmd);
      break;
    }
    case Version::BB_V4: {
      S_JoinGame_BB_64 cmd;
      size_t player_count = populate_v3_cmd(cmd);
      cmd.unused1 = 0;
      cmd.solo_mode = (l->mode == GameMode::SOLO) ? 1 : 0;
      cmd.unused2 = 0;
      send_command_t(c, 0x64, player_count, cmd);
      break;
    }
    default:
      throw logic_error("invalid game version");
  }

  c->log.info("Creating game join command queue");
  c->game_join_command_queue = make_unique<deque<Client::JoinCommand>>();
  send_command(c, 0x1D, 0x00);
}

template <typename LobbyDataT, typename DispDataT, typename RecordsT>
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

  if (!is_v1(c->version())) {
    send_player_records_t<RecordsT>(c, l, joining_client);
  }

  uint8_t lobby_type, lobby_block;
  if (l->is_game()) {
    lobby_type = 0;
    lobby_block = 0;
  } else {
    if (c->config.override_lobby_number != 0x80) {
      lobby_type = c->config.override_lobby_number;
    } else if (l->check_flag(Lobby::Flag::IS_OVERFLOW)) {
      lobby_type = is_ep3(c->version()) ? 15 : 0;
    } else {
      lobby_type = l->block - 1;
    }
    // Allow non-canonical lobby types on GC. They may work on other versions too,
    // but I haven't verified which values don't crash on each version.
    switch (c->version()) {
      case Version::GC_EP3_NTE:
      case Version::GC_EP3:
        if ((lobby_type > 0x14) && (lobby_type < 0xE9)) {
          lobby_type = l->block - 1;
        }
        break;
      case Version::GC_V3:
        if ((lobby_type > 0x11) && (lobby_type != 0x67) && (lobby_type != 0xD4) && (lobby_type < 0xFC)) {
          lobby_type = l->block - 1;
        }
        break;
      default:
        if (lobby_type > 0x0E) {
          lobby_type = l->block - 1;
        }
    }
    lobby_block = l->block;
  }

  S_JoinLobbyT<LobbyFlags, LobbyDataT, DispDataT> cmd;
  cmd.lobby_flags.client_id = c->lobby_client_id;
  cmd.lobby_flags.leader_id = l->leader_id;
  cmd.lobby_flags.disable_udp = 0x01;
  cmd.lobby_flags.lobby_number = lobby_type;
  cmd.lobby_flags.block_number = lobby_block;
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
    auto lp = lc->character();
    auto& e = cmd.entries[used_entries++];
    populate_lobby_data_for_client(e.lobby_data, lc, c);
    e.inventory = lp->inventory;
    e.inventory.encode_for_client(c->version(), s->item_parameter_table_for_encode(c->version()));
    if ((lc == c) && is_v1_or_v2(c->version()) && lc->v1_v2_last_reported_disp) {
      e.disp = convert_player_disp_data<DispDataT>(*lc->v1_v2_last_reported_disp, c->language(), lp->inventory.language);
    } else {
      e.disp = convert_player_disp_data<DispDataT>(lp->disp, c->language(), lp->inventory.language);
      e.disp.enforce_lobby_join_limits_for_version(c->version());
      uint32_t name_color = s->name_color_for_client(lc);
      if (name_color) {
        e.disp.visual.name_color = name_color;
        if (is_v1_or_v2(c->version())) {
          e.disp.visual.compute_name_color_checksum();
        }
      }
    }
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
    lobby_type = is_ep3(c->version()) ? 15 : 0;
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
    auto lp = lc->character();
    auto& e = cmd.entries[used_entries++];
    populate_lobby_data_for_client(e.lobby_data, lc, c);
    e.inventory = lp->inventory;
    e.inventory.encode_for_client(c->version(), s->item_parameter_table_for_encode(c->version()));
    e.disp = convert_player_disp_data<PlayerDispDataDCPCV3>(lp->disp, c->language(), lp->inventory.language);
    e.disp.enforce_lobby_join_limits_for_version(c->version());
    uint32_t name_color = s->name_color_for_client(lc);
    if (name_color) {
      e.disp.visual.name_color = name_color;
    }
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
    auto lp = lc->character();
    auto& e = cmd.entries[used_entries++];
    populate_lobby_data_for_client(e.lobby_data, lc, c);
    e.inventory = lp->inventory;
    e.inventory.encode_for_client(c->version(), s->item_parameter_table_for_encode(c->version()));
    if ((lc == c) && is_v1_or_v2(c->version()) && lc->v1_v2_last_reported_disp) {
      e.disp = convert_player_disp_data<PlayerDispDataDCPCV3>(*lc->v1_v2_last_reported_disp, c->language(), lp->inventory.language);
    } else {
      e.disp = convert_player_disp_data<PlayerDispDataDCPCV3>(lp->disp, c->language(), lp->inventory.language);
      e.disp.enforce_lobby_join_limits_for_version(c->version());
      uint32_t name_color = s->name_color_for_client(lc);
      if (name_color) {
        e.disp.visual.name_color = name_color;
        e.disp.visual.compute_name_color_checksum();
      }
    }
  }

  send_command(c, command, used_entries, &cmd, cmd.size(used_entries));
}

void send_join_lobby(shared_ptr<Client> c, shared_ptr<Lobby> l) {
  if (l->is_game()) {
    send_join_game(c, l);
  } else {
    switch (c->version()) {
      case Version::DC_NTE:
      case Version::DC_11_2000:
        send_join_lobby_dc_nte(c, l);
        break;
      case Version::DC_V1:
      case Version::DC_V2:
        send_join_lobby_t<PlayerLobbyDataDCGC, PlayerDispDataDCPCV3, PlayerRecordsEntry_DC>(c, l);
        break;
      case Version::PC_NTE:
      case Version::PC_V2:
        send_join_lobby_t<PlayerLobbyDataPC, PlayerDispDataDCPCV3, PlayerRecordsEntry_PC>(c, l);
        break;
      case Version::GC_NTE:
      case Version::GC_V3:
      case Version::GC_EP3_NTE:
      case Version::GC_EP3:
        send_join_lobby_t<PlayerLobbyDataDCGC, PlayerDispDataDCPCV3, PlayerRecordsEntry_V3>(c, l);
        break;
      case Version::XB_V3:
        send_join_lobby_xb(c, l);
        break;
      case Version::BB_V4:
        send_join_lobby_t<PlayerLobbyDataBB, PlayerDispDataBB, PlayerRecordsEntry_BB>(c, l);
        break;
      default:
        throw logic_error("unimplemented versioned command");
    }
  }

  // If the client will stop sending message box close confirmations after
  // joining any lobby, set the appropriate flag and update the client config
  if (c->config.check_flag(Client::Flag::NO_D6_AFTER_LOBBY) && !c->config.check_flag(Client::Flag::NO_D6)) {
    c->config.set_flag(Client::Flag::NO_D6);
    send_update_client_config(c, false);
  }
}

void send_player_join_notification(shared_ptr<Client> c,
    shared_ptr<Lobby> l, shared_ptr<Client> joining_client) {
  switch (c->version()) {
    case Version::DC_NTE:
    case Version::DC_11_2000:
      send_join_lobby_dc_nte(c, l, joining_client);
      break;
    case Version::DC_V1:
    case Version::DC_V2:
      send_join_lobby_t<PlayerLobbyDataDCGC, PlayerDispDataDCPCV3, PlayerRecordsEntry_DC>(c, l, joining_client);
      break;
    case Version::PC_NTE:
    case Version::PC_V2:
      send_join_lobby_t<PlayerLobbyDataPC, PlayerDispDataDCPCV3, PlayerRecordsEntry_PC>(c, l, joining_client);
      break;
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      send_join_lobby_t<PlayerLobbyDataDCGC, PlayerDispDataDCPCV3, PlayerRecordsEntry_V3>(c, l, joining_client);
      break;
    case Version::XB_V3:
      send_join_lobby_xb(c, l, joining_client);
      break;
    case Version::BB_V4:
      send_join_lobby_t<PlayerLobbyDataBB, PlayerDispDataBB, PlayerRecordsEntry_BB>(c, l, joining_client);
      break;
    default:
      throw logic_error("unimplemented versioned command");
  }
}

void send_update_lobby_data_bb(std::shared_ptr<Client> c) {
  auto l = c->require_lobby();
  for (auto lc : l->clients) {
    if (lc) {
      PlayerLobbyDataBB cmd;
      populate_lobby_data_for_client(cmd, c, lc);
      send_command_t(lc, 0x00F0, 0x00000000, cmd);
    }
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

void send_get_player_info(shared_ptr<Client> c, bool request_extended) {
  switch (c->version()) {
    case Version::DC_NTE:
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::PC_NTE:
    case Version::PC_V2:
    case Version::BB_V4:
      request_extended = false;
      break;
    case Version::DC_V2:
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
    case Version::XB_V3:
      break;
    default:
      throw logic_error("invalid version");
  }

  if (request_extended &&
      c->config.check_flag(Client::Flag::HAS_SEND_FUNCTION_CALL) &&
      !c->config.check_flag(Client::Flag::SEND_FUNCTION_CALL_CHECKSUM_ONLY)) {
    auto s = c->require_server_state();
    prepare_client_for_patches(c, [wc = weak_ptr<Client>(c)]() {
      auto c = wc.lock();
      if (!c) {
        return;
      }
      try {
        auto s = c->require_server_state();
        auto fn = s->function_code_index->get_patch("GetExtendedPlayerInfo", c->config.specific_version);
        send_function_call(c, fn);
        c->function_call_response_queue.emplace_back(empty_function_call_response_handler);
      } catch (const exception& e) {
        c->log.warning("Failed to send extended player info request: %s", e.what());
        send_get_player_info(c, false);
      }
    });
  } else {
    send_command(c, (c->version() == Version::DC_NTE) ? 0x8D : 0x95, 0x00);
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
  auto item_parameter_table = s->item_parameter_table_for_encode(c->version());
  for (size_t x = 0; x < items.size(); x++) {
    cmd.item_datas[x] = items[x];
    cmd.item_datas[x].encode_for_version(c->version(), item_parameter_table);
  }
  send_command_t(c, 0xD3, 0x00, cmd);
}

void send_execute_card_trade(shared_ptr<Client> c, const vector<pair<uint32_t, uint32_t>>& card_to_count) {
  if (!is_ep3(c->version())) {
    throw logic_error("cannot send trade cards command to non-Ep3 client");
  }

  SC_TradeCards_Ep3_EE_FlagD0_FlagD3 cmd;
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
    auto lc = l->clients[x];
    if (!lc) {
      continue;
    }
    auto& e = entries.emplace_back();
    e.player_tag = 0x00010000;
    e.guild_card_number = lc->login->account->account_id;
    e.arrow_color = lc->lobby_arrow_color;
  }

  for (size_t x = 0; x < l->max_clients; x++) {
    auto lc = l->clients[x];
    if (!lc || is_v1(lc->version())) {
      continue;
    }
    send_command_vt(lc, 0x88, entries.size(), entries);
  }
}

void send_unblock_join(shared_ptr<Client> c) {
  if (!is_pre_v1(c->version())) {
    static const be_uint32_t data = 0x71010000;
    send_command(c, 0x60, 0x00, &data, sizeof(be_uint32_t));
  }
}

void send_resume_game(shared_ptr<Lobby> l, shared_ptr<Client> ready_client) {
  for (auto lc : l->clients) {
    if (lc && (lc != ready_client) && !is_pre_v1(lc->version())) {
      static const be_uint32_t data = 0x72010000;
      send_command(lc, 0x60, 0x00, &data, sizeof(be_uint32_t));
    }
  }
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

void send_remove_negative_conditions(shared_ptr<Client> c) {
  G_AddStatusEffect_6x0C cmd;
  cmd.header = {0x0C, sizeof(G_AddStatusEffect_6x0C) >> 2, c->lobby_client_id};
  cmd.effect_type = 7; // Healing ring
  cmd.level = 0;
  send_protected_command(c, &cmd, sizeof(cmd), true);
}

void send_remove_negative_conditions(Channel& ch, uint16_t client_id) {
  G_AddStatusEffect_6x0C cmd;
  cmd.header = {0x0C, sizeof(G_AddStatusEffect_6x0C) >> 2, client_id};
  cmd.effect_type = 7; // Healing ring
  cmd.level = 0;
  ch.send(0x60, 0x00, &cmd, sizeof(cmd));
}

void send_warp(Channel& ch, uint8_t client_id, uint32_t floor, bool is_private) {
  G_InterLevelWarp_6x94 cmd = {{0x94, 0x02, 0}, floor, {}};
  ch.send(is_private ? 0x62 : 0x60, client_id, &cmd, sizeof(cmd));
}

void send_warp(shared_ptr<Client> c, uint32_t floor, bool is_private) {
  send_warp(c->channel, c->lobby_client_id, floor, is_private);
}

void send_warp(shared_ptr<Lobby> l, uint32_t floor, bool is_private) {
  for (const auto& c : l->clients) {
    if (c) {
      send_warp(c, floor, is_private);
    }
  }
}

void send_ep3_change_music(Channel& ch, uint32_t song) {
  G_ChangeLobbyMusic_Ep3_6xBF cmd = {{0xBF, 0x02, 0}, song};
  ch.send(0x60, 0x00, cmd);
}

void send_game_join_sync_command(
    shared_ptr<Client> c, const void* data, size_t size, uint8_t dc_nte_sc, uint8_t dc_11_2000_sc, uint8_t sc) {
  string compressed_data = bc0_compress(data, size);
  if (c->config.check_flag(Client::Flag::DEBUG_ENABLED)) {
    c->log.info("Compressed sync data from (%zX -> %zX bytes):", size, compressed_data.size());
    phosg::print_data(stderr, data, size);
  }
  send_game_join_sync_command_compressed(c, compressed_data.data(), compressed_data.size(), size, dc_nte_sc, dc_11_2000_sc, sc);
}

void send_game_join_sync_command(shared_ptr<Client> c, const string& data, uint8_t dc_nte_sc, uint8_t dc_11_2000_sc, uint8_t sc) {
  send_game_join_sync_command(c, data.data(), data.size(), dc_nte_sc, dc_11_2000_sc, sc);
}

void send_game_join_sync_command_compressed(
    shared_ptr<Client> c,
    const void* data,
    size_t size,
    size_t decompressed_size,
    uint8_t dc_nte_sc,
    uint8_t dc_11_2000_sc,
    uint8_t sc) {
  phosg::StringWriter w;
  if (is_pre_v1(c->version())) {
    G_SyncGameStateHeader_DCNTE_6x6B_6x6C_6x6D_6x6E compressed_header;
    compressed_header.header.basic_header.subcommand = (c->version() == Version::DC_NTE) ? dc_nte_sc : dc_11_2000_sc;
    compressed_header.header.basic_header.size = 0x00;
    compressed_header.header.basic_header.unused = 0x0000;
    compressed_header.header.size = (size + sizeof(G_SyncGameStateHeader_DCNTE_6x6B_6x6C_6x6D_6x6E) + 3) & (~3);
    compressed_header.decompressed_size = decompressed_size;
    w.put(compressed_header);
  } else {
    G_SyncGameStateHeader_6x6B_6x6C_6x6D_6x6E compressed_header;
    compressed_header.header.basic_header.subcommand = sc;
    compressed_header.header.basic_header.size = 0x00;
    compressed_header.header.basic_header.unused = 0x0000;
    compressed_header.header.size = (size + sizeof(G_SyncGameStateHeader_6x6B_6x6C_6x6D_6x6E) + 3) & (~3);
    compressed_header.decompressed_size = decompressed_size;
    compressed_header.compressed_size = size;
    w.put(compressed_header);
  }
  w.write(data, size);
  while (w.size() & 3) {
    w.put_u8(0x00);
  }

  if (c->game_join_command_queue) {
    c->log.info("Client not ready to receive game commands; adding to queue");
    auto& cmd = c->game_join_command_queue->emplace_back();
    cmd.command = 0x6D;
    cmd.flag = c->lobby_client_id;
    cmd.data = std::move(w.str());
  } else {
    send_command(c, 0x6D, c->lobby_client_id, w.str());
  }
}

void send_game_item_state(shared_ptr<Client> c) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    throw logic_error("cannot send item state in non-game lobby");
  }
  auto s = c->require_server_state();
  phosg::StringWriter floor_items_w;

  G_SyncItemState_6x6D_Decompressed decompressed_header;
  for (size_t z = 0; z < 12; z++) {
    decompressed_header.next_item_id_per_player[z] = l->next_item_id_for_client[z];
  }
  l->log.info("Sending next item IDs to client: %08" PRIX32 " %08" PRIX32 " %08" PRIX32 " %08" PRIX32,
      decompressed_header.next_item_id_per_player[0].load(),
      decompressed_header.next_item_id_per_player[1].load(),
      decompressed_header.next_item_id_per_player[2].load(),
      decompressed_header.next_item_id_per_player[3].load());

  for (size_t floor = 0; floor < 0x0F; floor++) {
    const auto& m = l->floor_item_managers.at(floor);
    // It's important that these are added in increasing order of item_id (hence
    // why items is a map and not an unordered_map), since the game uses binary
    // search to find floor items when picking them up. If items aren't in the
    // correct order, the game may fail to find an item when attempting to pick
    // it up, causing "ghost items" which are visible but can't be picked up.
    for (const auto& it : m.items) {
      const auto& item = it.second;
      if (!item->visible_to_client(c->lobby_client_id)) {
        continue;
      }

      FloorItem fi;
      fi.floor = floor;
      fi.source_type = 0;
      fi.entity_index = 0xFFFF;
      fi.pos = item->pos;
      fi.unknown_a2 = 0;
      fi.drop_number = (floor == 0) ? 0xFFFF : (decompressed_header.next_drop_number_per_floor.at(floor - 1)++);
      fi.item = item->data;
      fi.item.encode_for_version(c->version(), s->item_parameter_table_for_encode(c->version()));
      floor_items_w.put(fi);

      decompressed_header.floor_item_count_per_floor.at(floor)++;
    }
  }

  phosg::StringWriter decompressed_w;
  decompressed_w.put(decompressed_header);
  decompressed_w.write(floor_items_w.str());
  const auto& data = decompressed_w.str();
  send_game_join_sync_command(c, data.data(), data.size(), 0x5E, 0x65, 0x6D);

  // Items on floors 0x0F and above can't be sent in the 6x6D command, so we
  // manually send 6x5D commands to create them if needed
  phosg::StringWriter w;
  for (size_t floor = 0x0F; floor < l->floor_item_managers.size(); floor++) {
    const auto& m = l->floor_item_managers[floor];
    for (const auto& it : m.items) {
      const auto& item = it.second;
      if (!item->visible_to_client(c->lobby_client_id)) {
        continue;
      }
      uint8_t subcommand = get_pre_v1_subcommand(c->version(), 0x4F, 0x56, 0x5D);
      G_DropStackedItem_PC_V3_BB_6x5D cmd = {{{subcommand, 0x0A, 0x0000}, floor, 0, item->pos, item->data}, 0};
      cmd.item_data.encode_for_version(c->version(), s->item_parameter_table_for_encode(c->version()));
      w.put(cmd);
    }
  }
  if (!w.str().empty()) {
    send_command(c, 0x6D, c->lobby_client_id, w.str());
  }
}

void send_game_object_state(shared_ptr<Client> c) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    throw logic_error("cannot send object state in non-game lobby");
  }
  auto s = c->require_server_state();

  vector<SyncObjectStateEntry> entries;
  for (auto obj_st : l->map_state->iter_object_states(c->version())) {
    auto& entry = entries.emplace_back();
    entry.flags = obj_st->game_flags;
    entry.item_drop_id = (obj_st->item_drop_checked)
        ? 0xFFFF
        : (0x100 + l->map_state->index_for_object_state(c->version(), obj_st));
  }

  send_game_join_sync_command(c, entries.data(), entries.size() * sizeof(entries[0]), 0x5D, 0x64, 0x6C);
}

void send_game_enemy_state(shared_ptr<Client> c) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    throw logic_error("cannot send enemy state in non-game lobby");
  }
  auto s = c->require_server_state();

  vector<SyncEnemyStateEntry> entries;
  for (auto ene_st : l->map_state->iter_enemy_states(c->version())) {
    auto& entry = entries.emplace_back();
    entry.flags = ene_st->game_flags;
    entry.item_drop_id = (ene_st->server_flags & MapState::EnemyState::Flag::ITEM_DROPPED)
        ? 0xFFFF
        : (0xCA0 + l->map_state->index_for_enemy_state(c->version(), ene_st));
    entry.total_damage = ene_st->total_damage;
  }

  send_game_join_sync_command(c, entries.data(), entries.size() * sizeof(entries[0]), 0x5C, 0x63, 0x6B);
}

void send_game_set_state(shared_ptr<Client> c) {
  auto l = c->require_lobby();
  if (!l->is_game()) {
    throw logic_error("cannot send set state in non-game lobby");
  }

  size_t num_object_sets = 0;
  size_t num_enemy_sets = 0;
  size_t num_events = 0;
  for (const auto& fc : l->map_state->floor_config_entries) {
    if (fc.super_map) {
      const auto& entities = fc.super_map->version(c->version());
      num_object_sets += entities.objects.size();
      num_enemy_sets += entities.enemy_sets.size();
      num_events += entities.events.size();
    }
  }

  G_SyncSetFlagState_6x6E_Decompressed::EntitySetFlags entity_set_flags_header;
  entity_set_flags_header.object_set_flags_offset = sizeof(entity_set_flags_header);
  entity_set_flags_header.num_object_sets = num_object_sets;
  entity_set_flags_header.enemy_set_flags_offset = sizeof(entity_set_flags_header) + num_object_sets * sizeof(le_uint16_t);
  entity_set_flags_header.num_enemy_sets = num_enemy_sets;

  G_SyncSetFlagState_6x6E_Decompressed header;
  header.entity_set_flags_size = sizeof(entity_set_flags_header) + (num_object_sets + num_enemy_sets) * sizeof(le_uint16_t);
  header.event_set_flags_size = sizeof(le_uint16_t) * num_events;
  header.switch_flags_size = is_v1(c->version()) ? 0x200 : 0x240;
  header.total_size = header.entity_set_flags_size + header.event_set_flags_size + header.switch_flags_size;

  phosg::StringWriter w;
  w.put(header);
  w.put(entity_set_flags_header);

  {
    size_t size_before = w.size();
    for (const auto& obj_st : l->map_state->iter_object_states(c->version())) {
      w.put_u16l(obj_st->set_flags);
    }
    size_t bytes_added = w.size() - size_before;
    if (bytes_added != num_object_sets * sizeof(le_uint16_t)) {
      throw logic_error("incorrect number of object set flags added");
    }
  }

  {
    size_t size_before = w.size();
    for (const auto& ene_st : l->map_state->iter_enemy_set_states(c->version())) {
      w.put_u16l(ene_st->set_flags);
    }
    size_t bytes_added = w.size() - size_before;
    if (bytes_added != num_enemy_sets * sizeof(le_uint16_t)) {
      throw logic_error("incorrect number of enemy set flags added");
    }
  }

  {
    size_t size_before = w.size();
    for (const auto& ev_st : l->map_state->iter_event_states(c->version())) {
      w.put_u16l(ev_st->flags);
    }
    size_t bytes_added = w.size() - size_before;
    if (bytes_added != num_events * sizeof(le_uint16_t)) {
      throw logic_error("incorrect number of event flags added");
    }
  }

  if (l->switch_flags) {
    static_assert(sizeof(SwitchFlags) == 0x240, "switch_flags size is incorrect");
    w.write(l->switch_flags->data.data(), header.switch_flags_size);
  } else {
    w.extend_by(header.switch_flags_size, 0x00);
  }

  send_game_join_sync_command(c, w.str(), 0x5F, 0x66, 0x6E);
}

template <typename CmdT>
void send_game_flag_state_t(shared_ptr<Client> c) {
  auto l = c->require_lobby();

  if (l->quest_flags_known) { // Not all flags known; send multiple 6x75s
    phosg::StringWriter w;
    bool use_v3_cmd = !is_v1_or_v2(c->version()) || (c->version() == Version::GC_NTE);
    for (uint8_t difficulty = 0; difficulty < 4; difficulty++) {
      if ((difficulty != l->difficulty) && !use_v3_cmd) {
        continue;
      }
      const auto& diff_flags = l->quest_flag_values->data.at(difficulty);
      const auto& diff_known_flags = l->quest_flags_known->data.at(difficulty);
      for (uint8_t z = 0; z < diff_known_flags.data.size(); z++) {
        uint8_t known_flags = diff_known_flags.data[z];
        if (!known_flags) {
          continue;
        }
        uint8_t flag_values = diff_flags.data[z];
        for (uint8_t sh = 0; sh < 8; sh++) {
          if ((known_flags << sh) & 0x80) {
            uint16_t flag_num = ((z << 3) | sh);
            if (use_v3_cmd) {
              w.put(G_UpdateQuestFlag_V3_BB_6x75{
                  {{0x75, 0x03, 0x0000}, flag_num, (((flag_values << sh) & 0x80) ? 0 : 1)}, difficulty, 0});
            } else {
              w.put(G_UpdateQuestFlag_DC_PC_6x75{
                  {0x75, 0x02, 0x0000}, flag_num, (((flag_values << sh) & 0x80) ? 0 : 1)});
            }
          }
        }
      }
    }

    if (w.size() > 0) {
      if (c->game_join_command_queue) {
        c->log.info("Client not ready to receive join commands; adding to queue");
        auto& cmd = c->game_join_command_queue->emplace_back();
        cmd.command = 0x006D;
        cmd.flag = c->lobby_client_id;
        cmd.data = std::move(w.str());
      } else {
        send_command(c, 0x6D, c->lobby_client_id, w.str());
      }
    }

  } else { // All flags known; send 6x6F
    CmdT cmd;
    cmd.header.subcommand = 0x6F;
    cmd.header.size = sizeof(CmdT) >> 2;
    cmd.header.unused = 0x0000;
    cmd.quest_flags = (l && !l->quest_flags_known) ? *l->quest_flag_values : c->character()->quest_flags;

    if (c->game_join_command_queue) {
      c->log.info("Client not ready to receive join commands; adding to queue");
      auto& queue_cmd = c->game_join_command_queue->emplace_back();
      queue_cmd.command = 0x0062;
      queue_cmd.flag = c->lobby_client_id;
      queue_cmd.data.assign(reinterpret_cast<const char*>(&cmd), sizeof(cmd));
    } else {
      send_command_t(c, 0x62, c->lobby_client_id, cmd);
    }
  }
}

void send_game_flag_state(shared_ptr<Client> c) {
  // DC NTE and 11/2000 don't have this command at all; v1 has it but it doesn't
  // include flags for Ultimate.
  if (is_pre_v1(c->version())) {
    return;
  } else if (is_v1(c->version())) {
    send_game_flag_state_t<G_SetQuestFlags_DCv1_6x6F>(c);
  } else if (!is_v4(c->version())) {
    send_game_flag_state_t<G_SetQuestFlags_V2_V3_6x6F>(c);
  } else {
    send_game_flag_state_t<G_SetQuestFlags_BB_6x6F>(c);
  }
}

void send_game_player_state(shared_ptr<Client> to_c, shared_ptr<Client> from_c, bool apply_overrides) {
  if (!from_c->last_reported_6x70) {
    throw runtime_error("source client did not send a 6x70 command");
  }
  if (!from_c->login) {
    throw logic_error("source client is not logged in");
  }

  auto s = to_c->require_server_state();
  Parsed6x70Data to_send = *from_c->last_reported_6x70;

  to_send.base.client_id = from_c->lobby_client_id;
  to_send.player_tag = 0x00010000;
  to_send.guild_card_number = from_c->login->account->account_id;

  auto to_l = to_c->lobby.lock();
  if (to_l && (from_c->telepipe_lobby_id == to_l->lobby_id)) {
    to_send.telepipe.owner_client_id = from_c->telepipe_state.client_id2;
    to_send.telepipe.floor = from_c->telepipe_state.floor;
    to_send.telepipe.unknown_a1 = from_c->telepipe_state.unknown_b3;
    to_send.telepipe.pos = from_c->telepipe_state.pos;
    to_send.telepipe.unknown_a3 = from_c->telepipe_state.unknown_a3;
  }

  if (apply_overrides) {
    auto from_p = from_c->character();
    to_send.base.pos.x = from_c->pos.x;
    to_send.base.pos.y = 0.0;
    to_send.base.pos.z = from_c->pos.z;
    to_send.bonus_hp_from_materials = from_p->inventory.hp_from_materials;
    to_send.bonus_tp_from_materials = from_p->inventory.tp_from_materials;
    to_send.language = from_c->language();
    // TODO: Deal with telepipes. Probably we should track their state via the
    // subcommands sent when they're created/destroyed, but currently we don't.
    to_send.area = from_c->floor;
    to_send.technique_levels_v1 = from_p->disp.technique_levels_v1;
    to_send.visual = from_p->disp.visual;
    to_send.name = from_p->disp.name.decode(from_c->language());
    if (to_c->version() != Version::BB_V4) {
      to_send.visual.name.encode(to_send.name, to_c->language());
    }
    to_send.stats = from_p->disp.stats;
    to_send.num_items = from_p->inventory.num_items;
    to_send.items = from_p->inventory.items;
    to_send.item_version = Version::BB_V4; // Server-side items are stored in BB encoding
    to_send.floor = from_c->floor;
  }

  switch (to_c->version()) {
    case Version::DC_NTE:
      send_or_enqueue_command(to_c, 0x6D, to_c->lobby_client_id, to_send.as_dc_nte(s));
      break;
    case Version::DC_11_2000:
      send_or_enqueue_command(to_c, 0x6D, to_c->lobby_client_id, to_send.as_dc_112000(s));
      break;
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::PC_NTE:
    case Version::PC_V2:
      send_or_enqueue_command(to_c, 0x6D, to_c->lobby_client_id, to_send.as_dc_pc(s, to_c->version()));
      break;
    case Version::GC_NTE:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      send_or_enqueue_command(to_c, 0x6D, to_c->lobby_client_id, to_send.as_gc_gcnte(s, to_c->version()));
      break;
    case Version::XB_V3:
      send_or_enqueue_command(to_c, 0x6D, to_c->lobby_client_id, to_send.as_xb(s));
      break;
    case Version::BB_V4:
      send_or_enqueue_command(to_c, 0x6D, to_c->lobby_client_id, to_send.as_bb(s, to_c->language()));
      break;
    default:
      throw logic_error("attempting to send 6x70 command to unknown game version");
  }
}

void send_drop_item_to_channel(
    shared_ptr<ServerState> s,
    Channel& ch,
    const ItemData& item,
    uint8_t source_type,
    uint8_t floor,
    const VectorXZF& pos,
    uint16_t entity_index) {
  if (entity_index == 0xFFFF) {
    send_drop_stacked_item_to_channel(s, ch, item, floor, pos);
  } else {
    uint8_t subcommand = get_pre_v1_subcommand(ch.version, 0x51, 0x58, 0x5F);
    G_DropItem_PC_V3_BB_6x5F cmd = {
        {{subcommand, 0x0B, 0x0000}, {floor, source_type, entity_index, pos, 0, 0, item}}, 0};
    cmd.item.item.encode_for_version(ch.version, s->item_parameter_table_for_encode(ch.version));
    ch.send(0x60, 0x00, &cmd, sizeof(cmd));
  }
}

void send_drop_stacked_item_to_channel(
    shared_ptr<ServerState> s,
    Channel& ch,
    const ItemData& item,
    uint8_t floor,
    const VectorXZF& pos) {
  uint8_t subcommand = get_pre_v1_subcommand(ch.version, 0x4F, 0x56, 0x5D);
  G_DropStackedItem_PC_V3_BB_6x5D cmd = {{{subcommand, 0x0A, 0x0000}, floor, 0, pos, item}, 0};
  cmd.item_data.encode_for_version(ch.version, s->item_parameter_table_for_encode(ch.version));
  ch.send(0x60, 0x00, &cmd, sizeof(cmd));
}

void send_drop_stacked_item_to_lobby(shared_ptr<Lobby> l, const ItemData& item, uint8_t floor, const VectorXZF& pos) {
  auto s = l->require_server_state();
  for (auto& c : l->clients) {
    if (!c) {
      continue;
    }
    send_drop_stacked_item_to_channel(s, c->channel, item, floor, pos);
  }
}

void send_pick_up_item_to_client(shared_ptr<Client> c, uint8_t client_id, uint32_t item_id, uint8_t floor) {
  uint8_t subcommand = get_pre_v1_subcommand(c->version(), 0x4B, 0x52, 0x59);
  G_PickUpItem_6x59 cmd = {{subcommand, 0x03, client_id}, client_id, floor, item_id};
  send_command_t(c, 0x60, 0x00, cmd);
}

void send_create_inventory_item_to_client(shared_ptr<Client> c, uint8_t client_id, const ItemData& item) {
  if (c->version() == Version::BB_V4) {
    G_CreateInventoryItem_BB_6xBE cmd = {{0xBE, 0x07, client_id}, item, 0};
    send_command_t(c, 0x60, 0x00, cmd);
  } else {
    G_CreateInventoryItem_PC_V3_BB_6x2B cmd;
    cmd.header.subcommand = 0x2B;
    cmd.header.size = sizeof(cmd) >> 2;
    cmd.header.client_id = client_id;
    cmd.item_data = item;
    cmd.unused1 = 0;
    cmd.unknown_a2 = 0;
    cmd.unused2.clear(0);
    send_command_t(c, 0x60, 0x00, cmd);
  }
}

void send_create_inventory_item_to_lobby(shared_ptr<Client> c, uint8_t client_id, const ItemData& item, bool exclude_c) {
  auto l = c->require_lobby();
  for (const auto& lc : l->clients) {
    if (!lc) {
      continue;
    }
    if ((lc != c) || !exclude_c) {
      send_create_inventory_item_to_client(lc, client_id, item);
    }
  }
}

void send_destroy_item_to_lobby(shared_ptr<Client> c, uint32_t item_id, uint32_t amount, bool exclude_c) {
  auto l = c->require_lobby();
  uint16_t client_id = c->lobby_client_id;
  uint8_t subcommand = get_pre_v1_subcommand(c->version(), 0x25, 0x27, 0x29);
  G_DeleteInventoryItem_6x29 cmd = {{subcommand, 0x03, client_id}, item_id, amount};
  if (exclude_c) {
    send_command_excluding_client(l, c, 0x60, 0x00, &cmd, sizeof(cmd));
  } else {
    send_command_t(l, 0x60, 0x00, cmd);
  }
}

void send_destroy_floor_item_to_client(shared_ptr<Client> c, uint32_t item_id, uint32_t floor) {
  uint8_t subcommand = get_pre_v1_subcommand(c->version(), 0x55, 0x5C, 0x63);
  G_DestroyFloorItem_6x5C_6x63 cmd = {{subcommand, 0x03, 0x0000}, item_id, floor};
  send_command_t(c, 0x60, 0x00, cmd);
}

void send_item_identify_result(shared_ptr<Client> c) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw logic_error("cannot send item identify result to non-BB client");
  }
  G_IdentifyResult_BB_6xB9 res;
  res.header.subcommand = 0xB9;
  res.header.size = sizeof(res) / 4;
  res.header.client_id = c->lobby_client_id;
  res.item_data = c->bb_identify_result;
  send_command_t(l, 0x60, 0x00, res);
}

void send_bank(shared_ptr<Client> c) {
  if (c->version() != Version::BB_V4) {
    throw logic_error("6xBC can only be sent to BB clients");
  }

  auto p = c->character();
  auto& bank = c->current_bank();
  bank.sort();
  const auto* items_it = bank.items.data();
  vector<PlayerBankItem> items(items_it, items_it + bank.num_items);

  G_BankContentsHeader_BB_6xBC cmd = {
      {{0xBC, 0, 0}, sizeof(G_BankContentsHeader_BB_6xBC) + items.size() * sizeof(PlayerBankItem)},
      bank.checksum(), bank.num_items, bank.meseta};

  send_command_t_vt(c, 0x6C, 0x00, cmd, items);
}

void send_shop(shared_ptr<Client> c, uint8_t shop_type) {
  if (c->version() != Version::BB_V4) {
    throw logic_error("6xB6 can only be sent to BB clients");
  }

  const auto& contents = c->bb_shop_contents.at(shop_type);

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
  auto p = c->character();
  CharacterStats stats = p->disp.stats.char_stats;

  const ItemData* mag = nullptr;
  try {
    mag = &p->inventory.items[p->inventory.find_equipped_item(EquipSlot::MAG)].data;
  } catch (const out_of_range&) {
  }

  uint8_t subcommand = get_pre_v1_subcommand(c->version(), 0x2C, 0x2E, 0x30);
  G_ChangePlayerLevel_6x30 cmd = {
      {subcommand, sizeof(G_ChangePlayerLevel_6x30) / 4, c->lobby_client_id},
      stats.atp + (mag ? ((mag->data1w[3] / 100) * 2) : 0),
      stats.mst + (mag ? ((mag->data1w[5] / 100) * 2) : 0),
      stats.evp,
      stats.hp,
      stats.dfp + (mag ? (mag->data1w[2] / 100) : 0),
      stats.ata + (mag ? ((mag->data1w[4] / 100) / 2) : 0),
      p->disp.stats.level.load(),
      0};
  send_command_t(l, 0x60, 0x00, cmd);
}

void send_give_experience(shared_ptr<Client> c, uint32_t amount) {
  auto l = c->require_lobby();
  if (c->version() != Version::BB_V4) {
    throw logic_error("6xBF can only be sent to BB clients");
  }
  uint16_t client_id = c->lobby_client_id;
  G_GiveExperience_BB_6xBF cmd = {
      {0xBF, sizeof(G_GiveExperience_BB_6xBF) / 4, client_id}, amount};
  send_command_t(l, 0x60, 0x00, cmd);
}

void send_set_exp_multiplier(shared_ptr<Lobby> l) {
  if (!l->is_game()) {
    throw logic_error("6xDD can only be sent in games (not in lobbies)");
  }
  G_SetEXPMultiplier_BB_6xDD cmd = {{0xDD, sizeof(G_SetEXPMultiplier_BB_6xDD) / 4, (l->mode == GameMode::CHALLENGE) ? 1 : l->base_exp_multiplier}};
  for (auto lc : l->clients) {
    if (lc && (lc->version() == Version::BB_V4)) {
      send_command_t(lc, 0x60, 0x00, cmd);
    }
  }
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

void send_quest_function_call(Channel& ch, uint16_t label) {
  S_CallQuestFunction_V3_BB_AB cmd;
  cmd.label = label;
  ch.send(0xAB, 0x00, &cmd, sizeof(cmd));
}

void send_quest_function_call(shared_ptr<Client> c, uint16_t label) {
  send_quest_function_call(c->channel, label);
}

////////////////////////////////////////////////////////////////////////////////
// ep3 only commands

void send_ep3_card_list_update(shared_ptr<Client> c) {
  if (!c->config.check_flag(Client::Flag::HAS_EP3_CARD_DEFS)) {
    auto s = c->require_server_state();
    const auto& data = (c->version() == Version::GC_EP3_NTE)
        ? s->ep3_card_index_trial->get_compressed_definitions()
        : s->ep3_card_index->get_compressed_definitions();

    phosg::StringWriter w;
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
  phosg::StringWriter w;
  w.put<S_UpdateMediaHeader_Ep3_B9>({type, which, compressed_data.size(), 0});
  w.write(compressed_data);
  while (w.size() & 3) {
    w.put_u8(0);
  }
  send_command(c, 0xB9, 0x00, w.str());
}

void send_ep3_rank_update(shared_ptr<Client> c) {
  auto s = c->require_server_state();
  uint32_t current_meseta = s->ep3_infinite_meseta ? 1000000 : c->login->account->ep3_current_meseta;
  uint32_t total_meseta_earned = s->ep3_infinite_meseta ? 1000000 : c->login->account->ep3_total_meseta_earned;
  S_RankUpdate_Ep3_B7 cmd = {0, {}, current_meseta, total_meseta_earned, 0xFFFFFFFF};
  send_command_t(c, 0xB7, 0x00, cmd);
}

void send_ep3_card_battle_table_state(shared_ptr<Lobby> l, uint16_t table_number) {
  S_CardBattleTableState_Ep3_E4 cmd_nte;
  S_CardBattleTableState_Ep3_E4 cmd_final;

  set<shared_ptr<Client>> clients_nte;
  set<shared_ptr<Client>> clients_final;
  for (const auto& c : l->clients) {
    if (!c) {
      continue;
    }
    if (c->card_battle_table_number == table_number) {
      if (c->card_battle_table_seat_number > 3) {
        throw runtime_error("invalid battle table seat number");
      }

      bool is_nte = (c->version() == Version::GC_EP3_NTE);
      auto& e = is_nte ? cmd_nte.entries[c->card_battle_table_seat_number] : cmd_final.entries[c->card_battle_table_seat_number];
      if (e.state == 0) {
        e.state = c->card_battle_table_seat_state;
        e.guild_card_number = c->login->account->account_id;
        auto& clients = is_nte ? clients_nte : clients_final;
        clients.emplace(c);
      }
    }
  }

  for (const auto& c : clients_nte) {
    send_command_t(c, 0xE4, table_number, cmd_nte);
  }
  for (const auto& c : clients_final) {
    send_command_t(c, 0xE4, table_number, cmd_final);
  }
}

void send_ep3_set_context_token(shared_ptr<Client> c, uint32_t context_token) {
  G_SetContextToken_Ep3_6xB4x1F cmd;
  cmd.context_token = context_token;
  send_command_t(c, 0xC9, 0x00, cmd);
}

void send_ep3_confirm_tournament_entry(
    shared_ptr<Client> c,
    shared_ptr<const Episode3::Tournament> tourn) {
  if (c->version() == Version::GC_EP3_NTE) {
    throw runtime_error("cannot send tournament entry command to Episode 3 Trial Edition client");
  }

  S_ConfirmTournamentEntry_Ep3_CC cmd;
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

template <typename CmdT>
void send_ep3_tournament_list_t(shared_ptr<Client> c, bool is_for_spectator_team_create) {
  auto s = c->require_server_state();

  CmdT cmd;
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
    z++;
  }
  send_command_t(c, 0xE0, z, cmd);
}

void send_ep3_tournament_list(shared_ptr<Client> c, bool is_for_spectator_team_create) {
  if (c->version() == Version::GC_EP3_NTE) {
    send_ep3_tournament_list_t<S_TournamentList_Ep3NTE_E0>(c, is_for_spectator_team_create);
  } else {
    send_ep3_tournament_list_t<S_TournamentList_Ep3_E0>(c, is_for_spectator_team_create);
  }
}

void send_ep3_tournament_entry_list(
    shared_ptr<Client> c,
    shared_ptr<const Episode3::Tournament> tourn,
    bool is_for_spectator_team_create) {
  S_TournamentEntryList_Ep3_E2 cmd;
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

template <typename RulesT>
void send_ep3_tournament_details_t(
    shared_ptr<Client> c,
    shared_ptr<const Episode3::Tournament> tourn) {
  S_TournamentGameDetailsBaseT_Ep3_E3<RulesT> cmd;
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

void send_ep3_tournament_details(
    shared_ptr<Client> c,
    shared_ptr<const Episode3::Tournament> tourn) {
  if (c->version() == Version::GC_EP3_NTE) {
    send_ep3_tournament_details_t<Episode3::RulesTrial>(c, tourn);
  } else {
    send_ep3_tournament_details_t<Episode3::Rules>(c, tourn);
  }
}

string ep3_description_for_client(shared_ptr<Client> c) {
  if (!is_ep3(c->version())) {
    throw runtime_error("client is not Episode 3");
  }
  auto p = c->character();
  return phosg::string_printf(
      "%s CLv%" PRIu32 " %c",
      name_for_char_class(p->disp.visual.char_class),
      p->disp.stats.level + 1,
      char_for_language_code(p->inventory.language));
}

template <typename RulesT>
void send_ep3_game_details_t(shared_ptr<Client> c, shared_ptr<Lobby> l) {

  shared_ptr<Lobby> primary_lobby;
  if (l->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM)) {
    primary_lobby = l->watched_lobby.lock();
  } else {
    primary_lobby = l;
  }

  auto tourn_match = primary_lobby ? primary_lobby->tournament_match : nullptr;
  auto tourn = tourn_match ? tourn_match->tournament.lock() : nullptr;

  if (tourn) {
    S_TournamentGameDetailsBaseT_Ep3_E3<RulesT> cmd;
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
      auto account_id_to_client = primary_lobby->clients_by_account_id();
      using TeamEntryT = typename S_TournamentGameDetailsBaseT_Ep3_E3<RulesT>::TeamEntry;
      auto describe_team = [&](TeamEntryT& team_entry, shared_ptr<const Episode3::Tournament::Team> team) -> void {
        team_entry.team_name.encode(team->name, c->language());
        for (size_t z = 0; z < team->players.size(); z++) {
          auto& entry = team_entry.players[z];
          const auto& player = team->players[z];
          if (player.is_human()) {
            try {
              auto other_c = account_id_to_client.at(player.account_id);
              entry.name.encode(other_c->character()->disp.name.decode(other_c->language()), c->language());
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
          entry.name.encode(spec_c->character()->disp.name.decode(spec_c->language()), c->language());
          entry.description.encode(ep3_description_for_client(spec_c), c->language());
        }
      }
      flag = 0x05;
    } else {
      flag = 0x03;
    }
    send_command_t(c, 0xE3, flag, cmd);

  } else {
    S_GameInformationBaseT_Ep3_E1<RulesT> cmd;
    cmd.game_name.encode(l->name, c->language());
    if (primary_lobby) {
      size_t num_players = 0;
      for (const auto& opp_c : primary_lobby->clients) {
        if (opp_c) {
          cmd.player_entries[num_players].name.encode(opp_c->character()->disp.name.decode(opp_c->language()), c->language());
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
          entry.name.encode(spec_c->character()->disp.name.decode(spec_c->language()), c->language());
          entry.description.encode(ep3_description_for_client(spec_c), c->language());
        }
      }

      // There is a client bug that causes the spectators list to always be
      // empty when sent with E1, because there's no way for E1 to set the
      // spectator count in the info window object. To account for this, we send
      // a mostly-blank E3 to set the spectator count, followed by an E1 with
      // the correct data.
      S_TournamentGameDetailsBaseT_Ep3_E3<RulesT> cmd_E3;
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

void send_ep3_game_details(shared_ptr<Client> c, shared_ptr<Lobby> l) {
  if (c->version() == Version::GC_EP3_NTE) {
    send_ep3_game_details_t<Episode3::RulesTrial>(c, l);
  } else {
    send_ep3_game_details_t<Episode3::Rules>(c, l);
  }
}

template <typename CmdT>
void send_ep3_set_tournament_player_decks_t(shared_ptr<Client> c) {
  auto s = c->require_server_state();
  auto l = c->require_lobby();

  auto& match = l->tournament_match;
  auto tourn = match->tournament.lock();
  if (!tourn) {
    throw runtime_error("tournament is deleted");
  }

  CmdT cmd;
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
        if (player.account_id == c->login->account->account_id) {
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

  if ((c->version() != Version::GC_EP3_NTE) &&
      !(s->ep3_behavior_flags & Episode3::BehaviorFlag::DISABLE_MASKING)) {
    uint8_t mask_key = (phosg::random_object<uint32_t>() % 0xFF) + 1;
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

  auto account_id_to_client = l->clients_by_account_id();

  for (const auto& lc : l->clients) {
    if (!lc) {
      continue;
    }
    auto write_player_names = [&](G_TournamentMatchResult_Ep3_6xB4x51::NamesEntry& entry, shared_ptr<const Episode3::Tournament::Team> team) -> void {
      for (size_t z = 0; z < team->players.size(); z++) {
        const auto& player = team->players[z];
        if (player.is_human()) {
          try {
            auto pc = account_id_to_client.at(player.account_id);
            entry.player_names[z].encode(pc->character()->disp.name.decode(pc->language()), lc->language());
          } catch (const out_of_range&) {
            entry.player_names[z].encode(player.player_name, lc->language());
          }
        } else {
          entry.player_names[z].encode(player.com_deck->player_name, lc->language());
        }
      }
    };

    G_TournamentMatchResult_Ep3_6xB4x51 cmd;
    cmd.match_description.encode((match == tourn->get_final_match())
            ? phosg::string_printf("(%s) Final match", tourn->get_name().c_str())
            : phosg::string_printf("(%s) Round %zu", tourn->get_name().c_str(), match->round_num),
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
    if ((lc->version() != Version::GC_EP3_NTE) &&
        !(s->ep3_behavior_flags & Episode3::BehaviorFlag::DISABLE_MASKING)) {
      uint8_t mask_key = (phosg::random_object<uint32_t>() % 0xFF) + 1;
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

void send_ep3_set_tournament_player_decks(shared_ptr<Client> c) {
  if (c->version() == Version::GC_EP3_NTE) {
    send_ep3_set_tournament_player_decks_t<G_SetTournamentPlayerDecks_Ep3NTE_6xB4x3D>(c);
  } else {
    send_ep3_set_tournament_player_decks_t<G_SetTournamentPlayerDecks_Ep3_6xB4x3D>(c);
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
    G_SetGameMetadata_Ep3_6xB4x52 cmd;
    cmd.total_spectators = total_spectators;
    for (auto c : l->clients) {
      if (c) {
        if ((c->version() == Version::GC_EP3) &&
            !(s->ep3_behavior_flags & Episode3::BehaviorFlag::DISABLE_MASKING)) {
          G_SetGameMetadata_Ep3_6xB4x52 masked_cmd = cmd;
          uint8_t mask_key = (phosg::random_object<uint32_t>() % 0xFF) + 1;
          set_mask_for_ep3_game_command(&masked_cmd, sizeof(masked_cmd), mask_key);
          send_command_t(c, 0xC9, 0x00, masked_cmd);
        } else {
          send_command_t(c, 0xC9, 0x00, cmd);
        }
      }
    }
  }
  if (!l->watcher_lobbies.empty()) {
    string text;
    auto tourn = l->tournament_match ? l->tournament_match->tournament.lock() : 0;
    if (l->tournament_match && tourn) {
      if (tourn->get_final_match() == l->tournament_match) {
        text = phosg::string_printf("Viewing final match of tournament %s", tourn->get_name().c_str());
      } else {
        text = phosg::string_printf(
            "Viewing match in round %zu of tournament %s",
            l->tournament_match->round_num, tourn->get_name().c_str());
      }
    } else {
      text = "Viewing battle in game " + l->name;
    }
    add_color_inplace(text);
    for (auto watcher_l : l->watcher_lobbies) {
      G_SetGameMetadata_Ep3_6xB4x52 cmd;
      cmd.local_spectators = 0;
      for (auto c : watcher_l->clients) {
        cmd.local_spectators += (c.get() != nullptr);
      }
      cmd.total_spectators = total_spectators;
      cmd.text_size = text.size();
      cmd.text.encode(text, 1);
      for (auto c : watcher_l->clients) {
        if (c) {
          if ((c->version() == Version::GC_EP3) &&
              !(s->ep3_behavior_flags & Episode3::BehaviorFlag::DISABLE_MASKING)) {
            G_SetGameMetadata_Ep3_6xB4x52 masked_cmd = cmd;
            uint8_t mask_key = (phosg::random_object<uint32_t>() % 0xFF) + 1;
            set_mask_for_ep3_game_command(&masked_cmd, sizeof(masked_cmd), mask_key);
            send_command_t(c, 0xC9, 0x00, masked_cmd);
          } else {
            send_command_t(c, 0xC9, 0x00, cmd);
          }
        }
      }
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

  c->log.info("Sending quest file chunk %s:%zu", filename.c_str(), chunk_index);
  const auto& s = c->require_server_state();
  c->channel.send(is_download_quest ? 0xA7 : 0x13, chunk_index, &cmd, sizeof(cmd), s->hide_download_commands);
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
    case QuestFileType::DOWNLOAD_WITHOUT_PVR:
    case QuestFileType::DOWNLOAD_WITH_PVR:
      command_num = 0xA6;
      cmd.name.encode("PSO/" + quest_name);
      cmd.type = (type == QuestFileType::DOWNLOAD_WITH_PVR) ? 1 : 0;
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
  cmd.type = (type == QuestFileType::DOWNLOAD_WITH_PVR) ? 1 : 0;
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
    case Version::DC_11_2000:
    case Version::DC_V1:
    case Version::DC_V2:
    case Version::GC_NTE:
      send_open_quest_file_t<S_OpenFile_DC_44_A6>(c, quest_name, filename, xb_filename, contents->size(), quest_number, type);
      break;
    case Version::PC_NTE:
    case Version::PC_V2:
    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      send_open_quest_file_t<S_OpenFile_PC_GC_44_A6>(c, quest_name, filename, xb_filename, contents->size(), quest_number, type);
      break;
    case Version::XB_V3:
      send_open_quest_file_t<S_OpenFile_XB_44_A6>(c, quest_name, filename, xb_filename, contents->size(), quest_number, type);
      break;
    case Version::BB_V4:
      send_open_quest_file_t<S_OpenFile_BB_44_A6>(c, quest_name, filename, xb_filename, contents->size(), quest_number, type);
      break;
    default:
      throw logic_error("cannot send quest files to this version of client");
  }

  // On most versions, we can trust the TCP stack to do the right thing when we
  // send a lot of data at once, but on GC, the client will crash if too much
  // quest data is sent at once. This is likely a bug in the TCP stack, since
  // the client should apply backpressure to avoid bad situations, but we have
  // to deal with it here instead.
  size_t total_chunks = (contents->size() + 0x3FF) / 0x400;
  size_t chunks_to_send = is_v1_or_v2(c->version()) ? total_chunks : min<size_t>(V3_V4_QUEST_LOAD_MAX_CHUNKS_IN_FLIGHT, total_chunks);

  for (size_t z = 0; z < chunks_to_send; z++) {
    size_t offset = z * 0x400;
    size_t chunk_bytes = contents->size() - offset;
    if (chunk_bytes > 0x400) {
      chunk_bytes = 0x400;
    }
    send_quest_file_chunk(c, filename.c_str(), offset / 0x400,
        contents->data() + offset, chunk_bytes, (type != QuestFileType::ONLINE));
  }

  // If there are still chunks to send, track the file so the chunk
  // acknowledgement handler (13 or A7) cna know what to send next
  if (chunks_to_send < total_chunks) {
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

  for (auto& lc : l->clients) {
    if (lc) {
      if (!is_v1_or_v2(lc->version())) {
        send_command(lc, 0xAC, 0x00);
      }
      lc->disconnect_hooks.erase(QUEST_BARRIER_DISCONNECT_HOOK_NAME);
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
        (phosg::random_object<uint16_t>() % (s->ep3_card_auction_max_size - s->ep3_card_auction_min_size + 1));
  }
  num_cards = min<uint16_t>(num_cards, 0x14);

  auto card_index = l->is_ep3_nte() ? s->ep3_card_index_trial : s->ep3_card_index;

  uint64_t distribution_size = 0;
  for (const auto& e : s->ep3_card_auction_pool) {
    distribution_size += e.probability;
  }

  S_StartCardAuction_Ep3_EF cmd;
  cmd.points_available = s->ep3_card_auction_points;
  for (size_t z = 0; z < num_cards; z++) {
    uint64_t v = phosg::random_object<uint64_t>() % distribution_size;
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
  uint64_t t = phosg::now();

  time_t t_secs = t / 1000000;
  struct tm t_parsed;
  gmtime_r(&t_secs, &t_parsed);

  string time_str(128, 0);
  size_t len = strftime(time_str.data(), time_str.size(), "%Y:%m:%d: %H:%M:%S.000", &t_parsed);
  if (len == 0) {
    throw logic_error("strftime buffer too short");
  }
  time_str.resize(len);

  S_ServerTime_B1 cmd;
  cmd.time_str.encode(time_str);
  cmd.time_flags_low = 0x01;
  cmd.time_flags_mid = 0x00;
  cmd.time_flags_high = 0x00;
  send_command_t(c, 0xB1, 0x00, cmd);
}

void send_change_event(shared_ptr<Client> c, uint8_t new_event) {
  // This command isn't supported on versions before V3, nor on Trial Edition.
  if (!is_v1_or_v2(c->version())) {
    send_command(c, 0xDA, new_event);
  }
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

////////////////////////////////////////////////////////////////////////////////
// BB teams

void send_team_membership_info(shared_ptr<Client> c) {
  auto team = c->team();
  S_TeamMembershipInformation_BB_12EA cmd;
  if (team) {
    cmd.guild_card_number = c->login->account->account_id;
    cmd.team_id = team->team_id;
    cmd.privilege_level = team->members.at(c->login->account->account_id).privilege_level();
    cmd.team_member_count = min<size_t>(team->members.size(), 100);
    cmd.team_name.encode(team->name);
  }
  send_command_t(c, 0x12EA, 0x00000000, cmd);
}

static S_TeamInfoForPlayer_BB_13EA_15EA_Entry team_metadata_for_client(shared_ptr<Client> c) {
  auto team = c->team();
  S_TeamInfoForPlayer_BB_13EA_15EA_Entry cmd;
  cmd.lobby_client_id = c->lobby_client_id;
  cmd.guild_card_number2 = c->login->account->account_id;
  cmd.player_name = c->character()->disp.name;
  if (team) {
    cmd.guild_card_number = c->login->account->account_id;
    cmd.team_id = team->team_id;
    cmd.privilege_level = team->members.at(c->login->account->account_id).privilege_level();
    cmd.team_member_count = min<size_t>(team->members.size(), 100);
    cmd.team_name.encode(team->name);
    if (team->flag_data) {
      cmd.flag_data = *team->flag_data;
    }
  }
  return cmd;
}

void send_update_team_metadata_for_client(shared_ptr<Client> c) {
  auto l = c->require_lobby();
  send_command_t(l, 0x15EA, 0x00000001, team_metadata_for_client(c));
}

void send_all_nearby_team_metadatas_to_client(shared_ptr<Client> c, bool is_13EA) {
  auto l = c->require_lobby();

  vector<S_TeamInfoForPlayer_BB_13EA_15EA_Entry> entries;
  entries.reserve(l->count_clients());
  for (auto lc : l->clients) {
    if (lc) {
      entries.emplace_back(team_metadata_for_client(lc));
    }
  }
  send_command_vt(c, is_13EA ? 0x13EA : 0x15EA, entries.size(), entries);
}

void send_update_team_reward_flags(shared_ptr<Client> c) {
  auto team = c->team();
  send_command(c, 0x1DEA, team ? team->reward_flags : 0x00000000);
}

void send_team_member_list(shared_ptr<Client> c) {
  auto team = c->team();
  if (!team) {
    throw runtime_error("client is not in a team");
  }

  vector<const TeamIndex::Team::Member*> members;
  for (const auto& it : team->members) {
    members.emplace_back(&it.second);
  }
  auto rank_fn = +[](const TeamIndex::Team::Member* a, const TeamIndex::Team::Member* b) {
    return a->points > b->points;
  };
  sort(members.begin(), members.end(), rank_fn);

  S_TeamMemberList_BB_09EA header;
  header.entry_count = members.size();

  vector<S_TeamMemberList_BB_09EA::Entry> entries;
  entries.reserve(header.entry_count);
  for (size_t z = 0; z < members.size(); z++) {
    const auto* m = members[z];
    auto& e = entries.emplace_back();
    e.rank = z + 1;
    e.privilege_level = m->privilege_level();
    e.guild_card_number = m->account_id;
    e.name.encode(m->name, c->language());
  }

  send_command_t_vt(c, 0x09EA, 0x00000000, header, entries);
}

void send_intra_team_ranking(shared_ptr<Client> c) {
  auto team = c->team();
  if (!team) {
    throw runtime_error("client is not in a team");
  }

  // TODO: At some point we should maintain a sorted index instead of sorting
  // these on-demand.
  vector<const TeamIndex::Team::Member*> members;
  for (const auto& it : team->members) {
    members.emplace_back(&it.second);
  }
  auto rank_fn = +[](const TeamIndex::Team::Member* a, const TeamIndex::Team::Member* b) {
    return a->points > b->points;
  };
  sort(members.begin(), members.end(), rank_fn);

  S_IntraTeamRanking_BB_18EA cmd;
  cmd.points_remaining = team->points - team->spent_points;
  cmd.num_entries = members.size();

  vector<S_IntraTeamRanking_BB_18EA::Entry> entries;
  for (size_t z = 0; z < members.size(); z++) {
    const auto* m = members[z];
    cmd.ranking_points += m->points;
    auto& e = entries.emplace_back();
    e.rank = z + 1;
    e.privilege_level = m->privilege_level();
    e.guild_card_number = m->account_id;
    e.player_name.encode(m->name);
    e.points = m->points;
  }

  send_command_t_vt(c, 0x18EA, 0x00000000, cmd, entries);
}

void send_cross_team_ranking(shared_ptr<Client> c) {
  auto s = c->require_server_state();

  // TODO: At some point we should maintain a sorted index instead of sorting
  // these on-demand.
  auto teams = s->team_index->all();
  auto rank_fn = +[](const shared_ptr<const TeamIndex::Team>& a, const shared_ptr<const TeamIndex::Team>& b) {
    return a->points > b->points;
  };
  sort(teams.begin(), teams.end(), rank_fn);

  size_t num_to_send = min<size_t>(teams.size(), 0x300);

  S_CrossTeamRanking_BB_1CEA cmd;
  cmd.num_entries = num_to_send;

  vector<S_CrossTeamRanking_BB_1CEA::Entry> entries;
  for (size_t z = 0; z < num_to_send; z++) {
    auto t = teams[z];
    auto& e = entries.emplace_back();
    e.team_name.encode(t->name, c->language());
    e.team_points = t->points;
    e.unknown_a1 = 0x01020304;
  }

  send_command_t_vt(c, 0x1CEA, 0x00000000, cmd, entries);
}

void send_team_reward_list(shared_ptr<Client> c, bool show_purchased) {
  auto team = c->team();
  if (!team) {
    throw runtime_error("user is not in a team");
  }
  auto s = c->require_server_state();

  // Hide item rewards if the player's bank is full
  bool show_item_rewards = show_purchased || (c->current_bank().num_items < 200);

  vector<S_TeamRewardList_BB_19EA_1AEA::Entry> entries;
  for (const auto& reward : s->team_index->reward_definitions()) {
    // In the buy menu, hide rewards that can't be bought again (that is, unique
    // rewards that the team already has). In the bought menu, hide rewards that
    // the team does not have or that can be bought again.
    if (show_purchased != (team->has_reward(reward.key) && reward.is_unique)) {
      continue;
    }
    if (!show_item_rewards && !reward.reward_item.empty()) {
      continue;
    }
    bool has_all_prerequisites = true;
    for (const auto& key : reward.prerequisite_keys) {
      if (!team->has_reward(key)) {
        has_all_prerequisites = false;
        break;
      }
    }
    if (!has_all_prerequisites) {
      continue;
    }
    auto& e = entries.emplace_back();
    e.name.encode(reward.name, c->language());
    e.description.encode(reward.description, c->language());
    e.reward_id = reward.menu_item_id;
    e.team_points = reward.team_points;
  }

  S_TeamRewardList_BB_19EA_1AEA cmd;
  cmd.num_entries = entries.size();

  send_command_t_vt(c, show_purchased ? 0x19EA : 0x1AEA, 0x00000000, cmd, entries);
}
