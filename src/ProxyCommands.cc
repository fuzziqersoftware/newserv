#include "ProxyServer.hh"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <phosg/Encoding.hh>
#include <phosg/Filesystem.hh>
#include <phosg/Hash.hh>
#include <phosg/Network.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>
#include <phosg/Time.hh>
#ifdef HAVE_RESOURCE_FILE
#include <resource_file/Emulators/PPC32Emulator.hh>
#endif

#include "Loggers.hh"
#include "ChatCommands.hh"
#include "Compression.hh"
#include "PSOProtocol.hh"
#include "SendCommands.hh"
#include "ReceiveCommands.hh"
#include "ReceiveSubcommands.hh"

using namespace std;



static void forward_command(ProxyServer::LinkedSession& session, bool to_server,
    uint16_t command, uint32_t flag, string& data, bool print_contents = true) {
  auto& ch = to_server ? session.server_channel : session.client_channel;
  if (!ch.connected()) {
    proxy_server_log.warning("No endpoint is present; dropping command");
  } else {
    ch.send(command, flag, data, print_contents);
  }
}

static void check_implemented_subcommand(
    ProxyServer::LinkedSession& session, const string& data) {
  if (data.size() < 4) {
    session.log.warning("Received broadcast/target command with no contents");
  } else {
    if (!subcommand_is_implemented(data[0])) {
      session.log.warning("Received subcommand %02hhX which is not implemented on the server",
          data[0]);
    }
  }
}



static void send_text_message_to_client(
    ProxyServer::LinkedSession& session,
    uint8_t command,
    const std::string& message) {
  StringWriter w;
  w.put<SC_TextHeader_01_06_11_B0_EE>({0, 0});
  if ((session.version == GameVersion::PC) ||
      (session.version == GameVersion::BB)) {
    auto decoded = decode_sjis(message);
    w.write(decoded.data(), decoded.size() * sizeof(decoded[0]));
    w.put_u16l(0);
  } else {
    w.write(message);
    w.put_u8(0);
  }
  while (w.size() & 3) {
    w.put_u8(0);
  }
  session.client_channel.send(command, 0x00, w.str());
}



// Command handlers. These are called to preprocess or react to specific
// commands in either direction. If they return true, the command (which the
// function may have modified) is forwarded to the other end; if they return
// false; it is not.

struct HandlerResult {
  enum class Type {
    FORWARD = 0,
    SUPPRESS,
    MODIFIED,
  };
  Type type;
  // These are only used if Type is MODIFIED. If either are -1, then the
  // original command's value is used instead.
  int32_t new_command;
  int64_t new_flag;

  HandlerResult(Type type) : type(type), new_command(-1), new_flag(-1) { }
  HandlerResult(Type type, uint16_t new_command, uint32_t new_flag)
      : type(type), new_command(new_command), new_flag(new_flag) { }
};

static HandlerResult on_default(shared_ptr<ServerState>,
    ProxyServer::LinkedSession&, uint16_t, uint32_t, string&) {
  return HandlerResult::Type::FORWARD;
}

static HandlerResult on_server_97(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t flag, string&) {
  // Update the newserv client config so we'll know not to show the Programs
  // menu if they return to newserv
  session.newserv_client_config.cfg.flags |= Client::Flag::SAVE_ENABLED;
  // Trap any 97 command that would have triggered cheat protection, and always
  // send 97 01 04 00
  if (flag == 0) {
    return HandlerResult(HandlerResult::Type::MODIFIED, 0x97, 0x01);
  }
  return HandlerResult::Type::FORWARD;
}

static HandlerResult on_server_gc_9A(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string&) {
  if (!session.license) {
    return HandlerResult::Type::FORWARD;
  }

  C_LoginExtended_GC_9E cmd;
  if (session.remote_guild_card_number < 0) {
    cmd.player_tag = 0xFFFF0000;
    cmd.guild_card_number = 0xFFFFFFFF;
  } else {
    cmd.player_tag = 0x00010000;
    cmd.guild_card_number = session.remote_guild_card_number;
  }
  cmd.unused = 0;
  cmd.sub_version = session.sub_version;
  cmd.is_extended = (session.remote_guild_card_number < 0) ? 0 : 1;
  cmd.language = session.language;
  cmd.serial_number = string_printf("%08" PRIX32 "", session.license->serial_number);
  cmd.access_key = session.license->access_key;
  cmd.serial_number2 = cmd.serial_number;
  cmd.access_key2 = cmd.access_key;
  cmd.name = session.character_name;
  cmd.client_config.data = session.remote_client_config_data;

  // If there's a guild card number, a shorter 9E is sent that ends
  // right after the client config data

  session.server_channel.send(
      0x9E, 0x01, &cmd,
      cmd.is_extended ? sizeof(C_LoginExtended_GC_9E) : sizeof(C_Login_GC_9E));
  return HandlerResult::Type::SUPPRESS;
}

static HandlerResult on_server_dc_pc_v3_patch_02_17(
    shared_ptr<ServerState> s,
    ProxyServer::LinkedSession& session,
    uint16_t command,
    uint32_t flag,
    string& data) {
  if (session.version == GameVersion::PATCH && command == 0x17) {
    throw invalid_argument("patch server sent 17 server init");
  }

  // Most servers don't include after_message or have a shorter
  // after_message than newserv does, so don't require it
  const auto& cmd = check_size_t<S_ServerInit_DC_PC_V3_02_17_91_9B>(data,
      offsetof(S_ServerInit_DC_PC_V3_02_17_91_9B, after_message), 0xFFFF);

  if (!session.license) {
    session.log.info("No license in linked session");

    // We have to forward the command before setting up encryption, so the
    // client will be able to understand it.
    forward_command(session, false, command, flag, data);

    if ((session.version == GameVersion::GC) ||
        (session.version == GameVersion::XB)) {
      session.server_channel.crypt_in.reset(new PSOV3Encryption(cmd.server_key));
      session.server_channel.crypt_out.reset(new PSOV3Encryption(cmd.client_key));
      session.client_channel.crypt_in.reset(new PSOV3Encryption(cmd.client_key));
      session.client_channel.crypt_out.reset(new PSOV3Encryption(cmd.server_key));
    } else { // DC, PC, or patch server (they all use V2 encryption)
      session.server_channel.crypt_in.reset(new PSOV2Encryption(cmd.server_key));
      session.server_channel.crypt_out.reset(new PSOV2Encryption(cmd.client_key));
      session.client_channel.crypt_in.reset(new PSOV2Encryption(cmd.client_key));
      session.client_channel.crypt_out.reset(new PSOV2Encryption(cmd.server_key));
    }

    return HandlerResult::Type::SUPPRESS;
  }

  session.log.info("Existing license in linked session");

  // This isn't forwarded to the client, so don't recreate the client's crypts
  switch (session.version) {
    case GameVersion::DC:
    case GameVersion::PC:
    case GameVersion::PATCH:
      session.server_channel.crypt_in.reset(new PSOV2Encryption(cmd.server_key));
      session.server_channel.crypt_out.reset(new PSOV2Encryption(cmd.client_key));
      break;
    case GameVersion::GC:
    case GameVersion::XB:
      session.server_channel.crypt_in.reset(new PSOV3Encryption(cmd.server_key));
      session.server_channel.crypt_out.reset(new PSOV3Encryption(cmd.client_key));
      break;
    default:
      throw logic_error("unsupported version");
  }

  // Respond with an appropriate login command. We don't let the client do this
  // because it believes it already did (when it was in an unlinked session, or
  // in the patch server case, during the current session due to a hidden
  // redirect).
  if (session.version == GameVersion::PATCH) {
    session.server_channel.send(0x02);
    return HandlerResult::Type::SUPPRESS;

  } else if ((session.version == GameVersion::DC) ||
             (session.version == GameVersion::PC)) {
    if (session.newserv_client_config.cfg.flags & Client::Flag::IS_DC_V1) {
      if (command == 0x17) {
        C_LoginV1_DC_PC_V3_90 cmd;
        cmd.serial_number = string_printf("%08" PRIX32 "",
            session.license->serial_number);
        cmd.access_key = session.license->access_key;
        cmd.access_key.clear_after(8);
        session.server_channel.send(0x90, 0x00, &cmd, sizeof(cmd));
        return HandlerResult::Type::SUPPRESS;
      } else {
        C_LoginV1_DC_93 cmd;
        if (session.remote_guild_card_number < 0) {
          cmd.player_tag = 0xFFFF0000;
          cmd.guild_card_number = 0xFFFFFFFF;
        } else {
          cmd.player_tag = 0x00010000;
          cmd.guild_card_number = session.remote_guild_card_number;
        }
        cmd.unknown_a1 = 0;
        cmd.unknown_a2 = 0;
        cmd.sub_version = session.sub_version;
        cmd.is_extended = 0;
        cmd.language = session.language;
        cmd.serial_number = string_printf("%08" PRIX32 "",
            session.license->serial_number);
        cmd.access_key = session.license->access_key;
        cmd.access_key.clear_after(8);
        cmd.hardware_id = session.hardware_id;
        cmd.name = session.character_name;
        session.server_channel.send(0x93, 0x00, &cmd, sizeof(cmd));
        return HandlerResult::Type::SUPPRESS;
      }
    } else { // DCv2 or PC
      if (command == 0x17) {
        C_Login_DC_PC_V3_9A cmd;
        if (session.remote_guild_card_number < 0) {
          cmd.player_tag = 0xFFFF0000;
          cmd.guild_card_number = 0xFFFFFFFF;
        } else {
          cmd.player_tag = 0x00010000;
          cmd.guild_card_number = session.remote_guild_card_number;
        }
        cmd.sub_version = session.sub_version;
        cmd.serial_number = string_printf("%08" PRIX32 "",
            session.license->serial_number);
        cmd.access_key = session.license->access_key;
        cmd.access_key.clear_after(8);
        cmd.serial_number2 = cmd.serial_number;
        cmd.access_key2 = cmd.access_key;
        // TODO: We probably should set email_address, but we currently don't
        // keep that value anywhere in the session object, nor is it saved in
        // the License object.
        session.server_channel.send(0x9A, 0x00, &cmd, sizeof(cmd));
        return HandlerResult::Type::SUPPRESS;
      } else {
        C_Login_DC_PC_GC_9D cmd;
        if (session.remote_guild_card_number < 0) {
          cmd.player_tag = 0xFFFF0000;
          cmd.guild_card_number = 0xFFFFFFFF;
        } else {
          cmd.player_tag = 0x00010000;
          cmd.guild_card_number = session.remote_guild_card_number;
        }
        cmd.unused = 0;
        cmd.sub_version = session.sub_version;
        cmd.is_extended = 0;
        cmd.language = session.language;
        cmd.serial_number = string_printf("%08" PRIX32 "",
            session.license->serial_number);
        cmd.access_key = session.license->access_key;
        cmd.access_key.clear_after(8);
        cmd.serial_number2 = cmd.serial_number;
        cmd.access_key2 = cmd.access_key;
        cmd.name = session.character_name;
        session.server_channel.send(0x9D, 0x00, &cmd, sizeof(cmd));
        return HandlerResult::Type::SUPPRESS;
      }
    }

  } else if (session.version == GameVersion::GC) {
    if (command == 0x17) {
      C_VerifyLicense_V3_DB cmd;
      cmd.serial_number = string_printf("%08" PRIX32 "",
          session.license->serial_number);
      cmd.access_key = session.license->access_key;
      cmd.sub_version = session.sub_version;
      cmd.serial_number2 = cmd.serial_number;
      cmd.access_key2 = cmd.access_key;
      cmd.password = session.license->gc_password;
      session.server_channel.send(0xDB, 0x00, &cmd, sizeof(cmd));
      return HandlerResult::Type::SUPPRESS;

    } else {
      // For command 02, send the same as if we had received 9A from the server
      return on_server_gc_9A(s, session, command, flag, data);
    }

  } else if (session.version == GameVersion::XB) {
    throw runtime_error("xbox licenses are not implemented");

  } else {
    throw logic_error("invalid game version in server init handler");
  }
}

static HandlerResult on_server_bb_03(shared_ptr<ServerState> s,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  // Most servers don't include after_message or have a shorter after_message
  // than newserv does, so don't require it
  const auto& cmd = check_size_t<S_ServerInit_BB_03_9B>(data,
      offsetof(S_ServerInit_BB_03_9B, after_message), 0xFFFF);

  // If the session has a detector crypt, then it was resumed from an unlinked
  // session, during which we already sent an 03 command.
  if (session.detector_crypt.get()) {
    if (session.login_command_bb.empty()) {
      throw logic_error("linked BB session does not have a saved login command");
    }

    // This isn't forwarded to the client, so only recreate the server's crypts.
    // Use the same crypt type as the client... the server has the luxury of
    // being able to try all the crypts it knows to detect what type the client
    // uses, but the client can't do this since it sends the first encrypted
    // data on the connection.
    session.server_channel.crypt_in.reset(new PSOBBMultiKeyImitatorEncryption(
        session.detector_crypt, cmd.server_key.data(), sizeof(cmd.server_key), false));
    session.server_channel.crypt_out.reset(new PSOBBMultiKeyImitatorEncryption(
        session.detector_crypt, cmd.client_key.data(), sizeof(cmd.client_key), false));

    // Forward the login command we saved during the unlinked session.
    if (session.enable_remote_ip_crc_patch && (session.login_command_bb.size() >= 0x98)) {
      *reinterpret_cast<le_uint32_t*>(session.login_command_bb.data() + 0x94) =
          session.remote_ip_crc ^ (1309539928UL + 1248334810UL);
    }
    session.server_channel.send(0x93, 0x00, session.login_command_bb);

    return HandlerResult::Type::SUPPRESS;

  // If there's no detector crypt, then the session is new and was linked
  // immediately at connect time, and an 03 was not yet sent to the client, so
  // we should forward this one.
  } else {
    // Forward the command to the client before setting up the crypts, so the
    // client receives the unencrypted data
    session.client_channel.send(0x03, 0x00, data);

    session.detector_crypt.reset(new PSOBBMultiKeyDetectorEncryption(
        s->bb_private_keys, bb_crypt_initial_client_commands, cmd.client_key.data(), sizeof(cmd.client_key)));
    session.client_channel.crypt_in = session.detector_crypt;
    session.client_channel.crypt_out.reset(new PSOBBMultiKeyImitatorEncryption(
        session.detector_crypt, cmd.server_key.data(), sizeof(cmd.server_key), true));
    session.server_channel.crypt_in.reset(new PSOBBMultiKeyImitatorEncryption(
        session.detector_crypt, cmd.server_key.data(), sizeof(cmd.server_key), false));
    session.server_channel.crypt_out.reset(new PSOBBMultiKeyImitatorEncryption(
        session.detector_crypt, cmd.client_key.data(), sizeof(cmd.client_key), false));

    // We already forwarded the command, so don't do so again
    return HandlerResult::Type::SUPPRESS;
  }
}

static HandlerResult on_server_dc_pc_v3_04(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  // Some servers send a short 04 command if they don't use all of the 0x20
  // bytes available. We should be prepared to handle that.
  auto& cmd = check_size_t<S_UpdateClientConfig_DC_PC_V3_04>(data,
      offsetof(S_UpdateClientConfig_DC_PC_V3_04, cfg),
      sizeof(S_UpdateClientConfig_DC_PC_V3_04));

  // If this is a licensed session, hide the guild card number assigned by the
  // remote server so the client doesn't see it change. If this is an unlicensed
  // session, then the client never received a guild card number from newserv
  // anyway, so we can let the client see the number from the remote server.
  bool had_guild_card_number = (session.remote_guild_card_number >= 0);
  if (session.remote_guild_card_number != cmd.guild_card_number) {
    session.remote_guild_card_number = cmd.guild_card_number;
    session.log.info("Remote guild card number set to %" PRId64,
        session.remote_guild_card_number);
    send_text_message_to_client(session, 0x11, string_printf(
        "The remote server\nhas assigned your\nGuild Card number:\n\tC6%" PRId64,
        session.remote_guild_card_number));
  }
  if (session.license) {
    cmd.guild_card_number = session.license->serial_number;
  }

  // It seems the client ignores the length of the 04 command, and always copies
  // 0x20 bytes to its config data. So if the server sends a short 04 command,
  // part of the previous command ends up in the security data (usually part of
  // the copyright string from the server init command). We simulate that here.
  // If there was previously a guild card number, assume we got the lobby server
  // init text instead of the port map init text.
  memcpy(session.remote_client_config_data.data(),
      had_guild_card_number
        ? "t Lobby Server. Copyright SEGA E"
        : "t Port Map. Copyright SEGA Enter",
      session.remote_client_config_data.bytes());
  memcpy(session.remote_client_config_data.data(), &cmd.cfg,
      min<size_t>(data.size() - sizeof(S_UpdateClientConfig_DC_PC_V3_04),
        session.remote_client_config_data.bytes()));

  // If the guild card number was not set, pretend (to the server) that this is
  // the first 04 command the client has received. The client responds with a 96
  // (checksum) in that case.
  if (!had_guild_card_number) {
    // We don't actually have a client checksum, of course... hopefully just
    // random data will do (probably no private servers check this at all)
    // TODO: Presumably we can save these values from the client when they
    // connected to newserv originally, but I'm too lazy to do this right now
    le_uint64_t checksum = random_object<uint64_t>() & 0x0000FFFFFFFFFFFF;
    session.server_channel.send(0x96, 0x00, &checksum, sizeof(checksum));
  }

  return session.license ? HandlerResult::Type::MODIFIED : HandlerResult::Type::FORWARD;
}

static HandlerResult on_server_dc_pc_v3_06(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  if (session.license) {
    auto& cmd = check_size_t<SC_TextHeader_01_06_11_B0_EE>(data,
        sizeof(SC_TextHeader_01_06_11_B0_EE), 0xFFFF);
    if (cmd.guild_card_number == session.remote_guild_card_number) {
      cmd.guild_card_number = session.license->serial_number;
      return HandlerResult::Type::MODIFIED;
    }
  }
  return HandlerResult::Type::FORWARD;
}

template <typename CmdT>
static HandlerResult on_server_41(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  bool modified = false;
  if (session.license) {
    auto& cmd = check_size_t<CmdT>(data);
    if (cmd.searcher_guild_card_number == session.remote_guild_card_number) {
      cmd.searcher_guild_card_number = session.license->serial_number;
      modified = true;
    }
    if (cmd.result_guild_card_number == session.remote_guild_card_number) {
      cmd.result_guild_card_number = session.license->serial_number;
      modified = true;
    }
  }
  return modified ? HandlerResult::Type::MODIFIED : HandlerResult::Type::FORWARD;
}

template <typename CmdT>
static HandlerResult on_server_81(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  bool modified = false;
  if (session.license) {
    auto& cmd = check_size_t<CmdT>(data);
    if (cmd.from_guild_card_number == session.remote_guild_card_number) {
      cmd.from_guild_card_number = session.license->serial_number;
      modified = true;
    }
    if (cmd.to_guild_card_number == session.remote_guild_card_number) {
      cmd.to_guild_card_number = session.license->serial_number;
      modified = true;
    }
  }
  return modified ? HandlerResult::Type::MODIFIED : HandlerResult::Type::FORWARD;
}

static HandlerResult on_server_88(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t flag, string& data) {
  bool modified = false;
  if (session.license) {
    size_t expected_size = sizeof(S_ArrowUpdateEntry_88) * flag;
    auto* entries = &check_size_t<S_ArrowUpdateEntry_88>(data,
        expected_size, expected_size);
    for (size_t x = 0; x < flag; x++) {
      if (entries[x].guild_card_number == session.remote_guild_card_number) {
        entries[x].guild_card_number = session.license->serial_number;
        modified = true;
      }
    }
  }
  return modified ? HandlerResult::Type::MODIFIED : HandlerResult::Type::FORWARD;
}

static HandlerResult on_server_B2(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t flag, string& data) {
  const auto& cmd = check_size_t<S_ExecuteCode_B2>(data, sizeof(S_ExecuteCode_B2), 0xFFFF);

  if (cmd.code_size && session.save_files) {
    string code = data.substr(sizeof(S_ExecuteCode_B2));

    if (session.newserv_client_config.cfg.flags & Client::Flag::ENCRYPTED_SEND_FUNCTION_CALL) {
      StringReader r(code);
      bool is_big_endian = (session.version == GameVersion::GC || session.version == GameVersion::DC);
      uint32_t decompressed_size = is_big_endian ? r.get_u32b() : r.get_u32l();
      uint32_t key = is_big_endian ? r.get_u32b() : r.get_u32l();

      PSOV2Encryption crypt(key);
      string decrypted_data;
      if (is_big_endian) {
        StringWriter w;
        while (!r.eof()) {
          w.put_u32b(r.get_u32b() ^ crypt.next());
        }
        decrypted_data = move(w.str());
      } else {
        decrypted_data = r.read(r.remaining());
        crypt.decrypt(decrypted_data.data(), decrypted_data.size());
      }

      code = prs_decompress(decrypted_data);
      if (decompressed_size < code.size()) {
        code.resize(decompressed_size);
      } else if (decompressed_size > code.size()) {
        throw runtime_error("decompressed code smaller than expected");
      }

    } else {
      code = data.substr(sizeof(S_ExecuteCode_B2));
      if (code.size() < cmd.code_size) {
        code.resize(cmd.code_size);
      }
    }

    string output_filename = string_printf("code.%" PRId64 ".bin", now());
    save_file(output_filename, data);
    session.log.info("Wrote code from server to file %s", output_filename.c_str());

#ifdef HAVE_RESOURCE_FILE
    try {
      if (code.size() < sizeof(S_ExecuteCode_Footer_GC_B2)) {
        throw runtime_error("code section is too small");
      }

      size_t footer_offset = code.size() - sizeof(S_ExecuteCode_Footer_GC_B2);

      StringReader r(code.data(), code.size());
      const auto& footer = r.pget<S_ExecuteCode_Footer_GC_B2>(footer_offset);

      multimap<uint32_t, string> labels;
      r.go(footer.relocations_offset);
      uint32_t reloc_offset = 0;
      for (size_t x = 0; x < footer.num_relocations; x++) {
        reloc_offset += (r.get_u16b() * 4);
        labels.emplace(reloc_offset, string_printf("reloc%zu", x));
      }
      labels.emplace(footer.entrypoint_addr_offset.load(), "entry_ptr");
      labels.emplace(footer_offset, "footer");
      labels.emplace(r.pget_u32b(footer.entrypoint_addr_offset), "start");

      string disassembly = PPC32Emulator::disassemble(
          &r.pget<uint8_t>(0, code.size()),
          code.size(),
          0,
          &labels);

      output_filename = string_printf("code.%" PRId64 ".txt", now());
      {
        auto f = fopen_unique(output_filename, "wt");
        fprintf(f.get(), "// code_size = 0x%" PRIX32 "\n", cmd.code_size.load());
        fprintf(f.get(), "// checksum_addr = 0x%" PRIX32 "\n", cmd.checksum_start.load());
        fprintf(f.get(), "// checksum_size = 0x%" PRIX32 "\n", cmd.checksum_size.load());
        fwritex(f.get(), disassembly);
      }
      session.log.info("Wrote disassembly to file %s", output_filename.c_str());

    } catch (const exception& e) {
      session.log.info("Failed to disassemble code from server: %s", e.what());
    }
#endif
  }

  if (session.function_call_return_value >= 0) {
    session.log.info("Blocking function call from server");
    C_ExecuteCodeResult_B3 cmd;
    cmd.return_value = session.function_call_return_value;
    cmd.checksum = 0;
    session.server_channel.send(0xB3, flag, &cmd, sizeof(cmd));
    return HandlerResult::Type::SUPPRESS;
  } else {
    return HandlerResult::Type::FORWARD;
  }
}

static HandlerResult on_server_E7(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  if (session.save_files) {
    string output_filename = string_printf("player.%" PRId64 ".bin", now());
    save_file(output_filename, data);
    session.log.info("Wrote player data to file %s", output_filename.c_str());
  }
  return HandlerResult::Type::FORWARD;
}

template <typename CmdT>
static HandlerResult on_server_C4(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t flag, string& data) {
  bool modified = false;
  if (session.license) {
    size_t expected_size = sizeof(CmdT) * flag;
    // Some servers (e.g. Schtserv) send extra data on the end of this command;
    // the client ignores it so we can ignore it too
    auto* entries = &check_size_t<CmdT>(data, expected_size, 0xFFFF);
    for (size_t x = 0; x < flag; x++) {
      if (entries[x].guild_card_number == session.remote_guild_card_number) {
        entries[x].guild_card_number = session.license->serial_number;
        modified = true;
      }
    }
  }
  return modified ? HandlerResult::Type::MODIFIED : HandlerResult::Type::FORWARD;
}

static HandlerResult on_server_gc_E4(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  auto& cmd = check_size_t<S_CardLobbyGame_GC_Ep3_E4>(data);
  bool modified = false;
  for (size_t x = 0; x < 4; x++) {
    if (cmd.entries[x].guild_card_number == session.remote_guild_card_number) {
      cmd.entries[x].guild_card_number = session.license->serial_number;
      modified = true;
    }
  }
  return modified ? HandlerResult::Type::MODIFIED : HandlerResult::Type::FORWARD;
}

static HandlerResult on_server_bb_22(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  // We use this command (which is sent before the init encryption command) to
  // detect a particular server behavior that we'll have to work around later.
  // It looks like this command's existence is another anti-proxy measure, since
  // this command is 0x34 bytes in total, and the logic that adds padding bytes
  // when the command size isn't a multiple of 8 is only active when encryption
  // is enabled. Presumably some simpler proxies would get this wrong.
  // Editor's note: There's an unsavory message in this command's data field,
  // hence the hash here instead of a direct string comparison. I'd love to hear
  // the story behind why they put that string there.
  if ((data.size() == 0x2C) &&
      (fnv1a64(data.data(), data.size()) == 0x8AF8314316A27994)) {
    session.log.info("Enabling remote IP CRC patch");
    session.enable_remote_ip_crc_patch = true;
  }
  return HandlerResult::Type::FORWARD;
}

static HandlerResult on_server_game_19_patch_14(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  // If the command is shorter than 6 bytes, use the previous server command to
  // fill it in. This simulates a behavior used by some private servers where a
  // longer previous command is used to fill part of the client's receive buffer
  // with meaningful data, then an intentionally undersize 19 command is sent
  // which results in the client using the previous command's data as part of
  // the 19 command's contents. They presumably do this in an attempt to prevent
  // people from using proxies.
  if (data.size() < sizeof(session.prev_server_command_bytes)) {
    data.append(
        reinterpret_cast<const char*>(&session.prev_server_command_bytes[data.size()]),
        sizeof(session.prev_server_command_bytes) - data.size());
  }
  if (data.size() < sizeof(S_Reconnect_19)) {
    data.resize(sizeof(S_Reconnect_19), '\0');
  }

  if (session.enable_remote_ip_crc_patch) {
    session.remote_ip_crc = crc32(data.data(), 4);
  }

  // Set the destination netloc appropriately
  memset(&session.next_destination, 0, sizeof(session.next_destination));
  struct sockaddr_in* sin = reinterpret_cast<struct sockaddr_in*>(
      &session.next_destination);
  sin->sin_family = AF_INET;
  if (session.version == GameVersion::PATCH) {
    auto& cmd = check_size_t<S_Reconnect_Patch_14>(data);
    sin->sin_addr.s_addr = cmd.address.load_raw(); // Already big-endian
    sin->sin_port = htons(cmd.port);
  } else {
    // This weird maximum size is here to properly handle the version-split
    // command that some servers (including newserv) use on port 9100
    auto& cmd = check_size_t<S_Reconnect_19>(data, sizeof(S_Reconnect_19), 0xFFFF);
    sin->sin_addr.s_addr = cmd.address.load_raw(); // Already big-endian
    sin->sin_port = htons(cmd.port);
  }

  if (!session.client_channel.connected()) {
    session.log.warning("Received reconnect command with no destination present");
    return HandlerResult::Type::SUPPRESS;

  } else if (session.version != GameVersion::BB) {
    // Hide redirects from the client completely. The new destination server
    // will presumably send a new encryption init command, which the handlers
    // will appropriately respond to.
    session.server_channel.crypt_in.reset();
    session.server_channel.crypt_out.reset();

    // We already modified next_destination, so start the connection process
    session.connect();
    return HandlerResult::Type::SUPPRESS;

  } else {
    const struct sockaddr_in* sin = reinterpret_cast<const struct sockaddr_in*>(
        &session.client_channel.local_addr);
    if (sin->sin_family != AF_INET) {
      throw logic_error("existing connection is not ipv4");
    }
    auto& cmd = check_size_t<S_Reconnect_19>(data, sizeof(S_Reconnect_19), 0xFFFF);
    cmd.address.store_raw(sin->sin_addr.s_addr);
    cmd.port = ntohs(sin->sin_port);
    return HandlerResult::Type::MODIFIED;
  }
}

static HandlerResult on_server_v3_1A_D5(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string&) {
  // If the client is a version that sends close confirmations and the client
  // has the no-close-confirmation flag set in its newserv client config, send a
  // fake confirmation to the remote server immediately.
  if (((session.version == GameVersion::GC) || (session.version == GameVersion::XB)) &&
      (session.newserv_client_config.cfg.flags & Client::Flag::NO_D6)) {
    session.server_channel.send(0xD6);
  }
  return HandlerResult::Type::FORWARD;
}

static HandlerResult on_server_60_62_6C_6D_C9_CB(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  check_implemented_subcommand(session, data);

  if (session.save_files) {
    if ((session.version == GameVersion::GC) && (data.size() >= 0x14)) {
      PSOSubcommand* subs = &check_size_t<PSOSubcommand>(data, 0x14, 0xFFFF);
      if (subs[0].dword == 0x000000B6 && subs[2].dword == 0x00000041) {
        string filename = string_printf("map%08" PRIX32 ".%" PRIu64 ".mnmd",
            subs[3].dword.load(), now());
        string map_data = prs_decompress(data.substr(0x14));
        save_file(filename, map_data);
        session.log.warning("Wrote %zu bytes to %s", map_data.size(), filename.c_str());
      }
    }
  }

  if (!data.empty() &&
      session.next_drop_item.data.data1d[0] &&
      (session.version != GameVersion::BB)) {
    if (data[0] == 0x60) {
      const auto& cmd = check_size_t<G_EnemyDropItemRequest_DC_6x60>(data,
          sizeof(G_EnemyDropItemRequest_DC_6x60),
          sizeof(G_EnemyDropItemRequest_PC_V3_BB_6x60));
      session.next_drop_item.data.id = session.next_item_id++;
      send_drop_item(session.server_channel, session.next_drop_item.data,
          true, cmd.area, cmd.x, cmd.z, cmd.request_id);
      send_drop_item(session.client_channel, session.next_drop_item.data,
          true, cmd.area, cmd.x, cmd.z, cmd.request_id);
      session.next_drop_item.clear();
      return HandlerResult::Type::SUPPRESS;
    // Note: This static_cast is required to make compilers not complain that
    // the comparison is always false (which even happens in some environments
    // if we use -0x5E... apparently char is unsigned on some systems, or
    // std::string's char_type isn't char??)
    } else if (static_cast<uint8_t>(data[0]) == 0xA2) {
      const auto& cmd = check_size_t<G_BoxItemDropRequest_6xA2>(data);
      session.next_drop_item.data.id = session.next_item_id++;
      send_drop_item(session.server_channel, session.next_drop_item.data,
          false, cmd.area, cmd.x, cmd.z, cmd.request_id);
      send_drop_item(session.client_channel, session.next_drop_item.data,
          false, cmd.area, cmd.x, cmd.z, cmd.request_id);
      session.next_drop_item.clear();
      return HandlerResult::Type::SUPPRESS;
    }
  }

  return HandlerResult::Type::FORWARD;
}

template <typename T>
static HandlerResult on_server_44_A6(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t command, uint32_t, string& data) {
  if (session.save_files) {
    const auto& cmd = check_size_t<S_OpenFile_PC_V3_44_A6>(data);
    bool is_download_quest = (command == 0xA6);

    string filename = cmd.filename;
    string output_filename = string_printf("%s.%s.%" PRIu64,
        filename.c_str(),
        is_download_quest ? "download" : "online", now());
    for (size_t x = 0; x < output_filename.size(); x++) {
      if (output_filename[x] < 0x20 || output_filename[x] > 0x7E || output_filename[x] == '/') {
        output_filename[x] = '_';
      }
    }
    if (output_filename[0] == '.') {
      output_filename[0] = '_';
    }

    ProxyServer::LinkedSession::SavingFile sf(
        cmd.filename, output_filename, cmd.file_size);
    session.saving_files.emplace(cmd.filename, move(sf));
    session.log.info("Opened file %s", output_filename.c_str());
  }
  return HandlerResult::Type::FORWARD;
}

static HandlerResult on_server_13_A7(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  if (session.save_files) {
    const auto& cmd = check_size_t<S_WriteFile_13_A7>(data);

    ProxyServer::LinkedSession::SavingFile* sf = nullptr;
    try {
      sf = &session.saving_files.at(cmd.filename);
    } catch (const out_of_range&) {
      string filename = cmd.filename;
      session.log.warning("Received data for non-open file %s", filename.c_str());
      return HandlerResult::Type::FORWARD;
    }

    size_t bytes_to_write = cmd.data_size;
    if (bytes_to_write > 0x400) {
      session.log.warning("Chunk data size is invalid; truncating to 0x400");
      bytes_to_write = 0x400;
    }

    session.log.info("Writing %zu bytes to %s", bytes_to_write, sf->output_filename.c_str());
    fwritex(sf->f.get(), cmd.data, bytes_to_write);
    if (bytes_to_write > sf->remaining_bytes) {
      session.log.warning("Chunk size extends beyond original file size; file may be truncated");
      sf->remaining_bytes = 0;
    } else {
      sf->remaining_bytes -= bytes_to_write;
    }

    if (sf->remaining_bytes == 0) {
      session.log.info("File %s is complete", sf->output_filename.c_str());
      session.saving_files.erase(cmd.filename);
    }
  }
  return HandlerResult::Type::FORWARD;
}

static HandlerResult on_server_gc_B8(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  if (session.save_files) {
    if (data.size() < 4) {
      session.log.warning("Card list data size is too small; not saving file");
      return HandlerResult::Type::FORWARD;
    }

    StringReader r(data);
    size_t size = r.get_u32l();
    if (r.remaining() < size) {
      session.log.warning("Card list data size extends beyond end of command; not saving file");
      return HandlerResult::Type::FORWARD;
    }

    string output_filename = string_printf("cardupdate.%" PRIu64 ".mnr", now());
    save_file(output_filename, r.read(size));
    session.log.info("Wrote %zu bytes to %s", size, output_filename.c_str());
  }
  return HandlerResult::Type::FORWARD;
}

static void update_leader_id(ProxyServer::LinkedSession& session, uint8_t leader_id) {
  if (session.leader_client_id != leader_id) {
    session.leader_client_id = leader_id;
    session.log.info("Changed room leader to %zu", session.leader_client_id);
    if (session.leader_client_id == session.lobby_client_id) {
      send_text_message(session.client_channel, u"$C6You are now the leader");
    }
  }
}

template <typename CmdT>
static HandlerResult on_server_65_67_68(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t command, uint32_t flag, string& data) {
  if (command == 0x67) {
    session.lobby_players.clear();
    session.lobby_players.resize(12);
    session.log.info("Cleared lobby players");
    session.is_in_game = false;

    // This command can cause the client to no longer send D6 responses when
    // 1A/D5 large message boxes are closed. newserv keeps track of this
    // behavior in the client config, so if it happens during a proxy session,
    // update the client config that we'll restore if the client uses the change
    // ship or change block command.
    if (session.newserv_client_config.cfg.flags & Client::Flag::NO_D6_AFTER_LOBBY) {
      session.newserv_client_config.cfg.flags |= Client::Flag::NO_D6;
    }
  }

  size_t expected_size = offsetof(CmdT, entries) + sizeof(typename CmdT::Entry) * flag;
  auto& cmd = check_size_t<CmdT>(data, expected_size, expected_size);
  bool modified = false;

  session.lobby_client_id = cmd.client_id;
  update_leader_id(session, cmd.leader_id);
  for (size_t x = 0; x < flag; x++) {
    size_t index = cmd.entries[x].lobby_data.client_id;
    if (index >= session.lobby_players.size()) {
      session.log.warning("Ignoring invalid player index %zu at position %zu", index, x);
    } else {
      if (session.license && (cmd.entries[x].lobby_data.guild_card == session.remote_guild_card_number)) {
        cmd.entries[x].lobby_data.guild_card = session.license->serial_number;
        modified = true;
      }
      session.lobby_players[index].guild_card_number = cmd.entries[x].lobby_data.guild_card;
      ptext<char, 0x10> name = cmd.entries[x].disp.name;
      session.lobby_players[index].name = name;
      session.log.info("Added lobby player: (%zu) %" PRIu32 " %s",
          index,
          session.lobby_players[index].guild_card_number,
          session.lobby_players[index].name.c_str());
    }
  }

  if (session.override_lobby_event >= 0) {
    cmd.event = session.override_lobby_event;
    modified = true;
  }
  if (session.override_lobby_number >= 0) {
    cmd.lobby_number = session.override_lobby_number;
    modified = true;
  }

  return modified ? HandlerResult::Type::MODIFIED : HandlerResult::Type::FORWARD;
}

template <typename CmdT>
static HandlerResult on_server_64(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t flag, string& data) {
  // We don't need to clear lobby_players here because we always
  // overwrite all 4 entries for this command
  session.lobby_players.resize(4);
  session.log.info("Cleared lobby players");
  session.is_in_game = true;

  CmdT* cmd;
  S_JoinGame_GC_Ep3_64* cmd_ep3 = nullptr;
  if (session.sub_version >= 0x40) {
    cmd = &check_size_t<CmdT>(data, sizeof(S_JoinGame_GC_Ep3_64), sizeof(S_JoinGame_GC_Ep3_64));
    cmd_ep3 = &check_size_t<S_JoinGame_GC_Ep3_64>(data);
  } else {
    cmd = &check_size_t<CmdT>(data);
  }

  bool modified = false;

  session.lobby_client_id = cmd->client_id;
  update_leader_id(session, cmd->leader_id);
  for (size_t x = 0; x < flag; x++) {
    if (cmd->lobby_data[x].guild_card == session.remote_guild_card_number) {
      cmd->lobby_data[x].guild_card = session.license->serial_number;
      modified = true;
    }
    session.lobby_players[x].guild_card_number = cmd->lobby_data[x].guild_card;
    if (cmd_ep3) {
      ptext<char, 0x10> name = cmd_ep3->players_ep3[x].disp.name;
      session.lobby_players[x].name = name;
    } else {
      session.lobby_players[x].name.clear();
    }
    session.log.info("Added lobby player: (%zu) %" PRIu32 " %s",
        x,
        session.lobby_players[x].guild_card_number,
        session.lobby_players[x].name.c_str());
  }

  if (session.override_section_id >= 0) {
    cmd->section_id = session.override_section_id;
    modified = true;
  }
  if (session.override_lobby_event >= 0) {
    cmd->event = session.override_lobby_event;
    modified = true;
  }
  if (session.override_random_seed >= 0) {
    cmd->rare_seed = session.override_random_seed;
    modified = true;
  }

  return modified ? HandlerResult::Type::MODIFIED : HandlerResult::Type::FORWARD;
}

static HandlerResult on_server_66_69(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  const auto& cmd = check_size_t<S_LeaveLobby_66_69_Ep3_E9>(data);
  size_t index = cmd.client_id;
  if (index >= session.lobby_players.size()) {
    session.log.warning("Lobby leave command references missing position");
  } else {
    session.lobby_players[index].guild_card_number = 0;
    session.lobby_players[index].name.clear();
    session.log.info("Removed lobby player (%zu)", index);
  }
  update_leader_id(session, cmd.leader_id);
  return HandlerResult::Type::FORWARD;
}

static HandlerResult on_client_98(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string&) {
  session.is_in_game = false;
  return HandlerResult::Type::FORWARD;
}

static HandlerResult on_client_06(shared_ptr<ServerState> s,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  if (data.size() >= 12) {
    u16string text;
    if (session.version == GameVersion::PC || session.version == GameVersion::BB) {
      const auto& cmd = check_size_t<C_Chat_06>(data, sizeof(C_Chat_06), 0xFFFF);
      text = u16string(cmd.text.pcbb, (data.size() - sizeof(C_Chat_06)) / sizeof(char16_t));
    } else {
      const auto& cmd = check_size_t<C_Chat_06>(data, sizeof(C_Chat_06), 0xFFFF);
      text = decode_sjis(cmd.text.dcv3, data.size() - sizeof(C_Chat_06));
    }
    strip_trailing_zeroes(text);

    if (text.empty()) {
      return HandlerResult::Type::SUPPRESS;
    }

    bool is_command = (text[0] == '$' || (text[0] == '\t' && text[1] != 'C' && text[2] == '$'));
    if (is_command) {
      text = text.substr((text[0] == '$') ? 0 : 2);
      if (text.size() >= 2 && text[1] == '$') {
        send_chat_message(session.server_channel, text.substr(1));
        return HandlerResult::Type::SUPPRESS;
      } else {
        on_chat_command(s, session, text);
        return HandlerResult::Type::SUPPRESS;
      }

    } else if (session.enable_chat_filter) {
      add_color_inplace(data.data() + 8, data.size() - 8);
      // TODO: We should return MODIFIED here if the message was changed by
      // the add_color_inplace call
      return HandlerResult::Type::FORWARD;

    } else {
      return HandlerResult::Type::FORWARD;
    }

  } else {
    return HandlerResult::Type::FORWARD;
  }
}

static HandlerResult on_client_40(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  bool modified = false;
  if (session.license) {
    auto& cmd = check_size_t<C_GuildCardSearch_40>(data);
    if (cmd.searcher_guild_card_number == session.license->serial_number) {
      cmd.searcher_guild_card_number = session.remote_guild_card_number;
      modified = true;
    }
    if (cmd.target_guild_card_number == session.license->serial_number) {
      cmd.target_guild_card_number = session.remote_guild_card_number;
      modified = true;
    }
  }
  return modified ? HandlerResult::Type::MODIFIED : HandlerResult::Type::FORWARD;
}

template <typename CmdT>
static HandlerResult on_client_81(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  auto& cmd = check_size_t<CmdT>(data);
  if (session.license) {
    if (cmd.from_guild_card_number == session.license->serial_number) {
      cmd.from_guild_card_number = session.remote_guild_card_number;
    }
    if (cmd.to_guild_card_number == session.license->serial_number) {
      cmd.to_guild_card_number = session.remote_guild_card_number;
    }
  }
  // GC clients send uninitialized memory here; don't forward it
  cmd.text.clear_after(cmd.text.len());
  return HandlerResult::Type::MODIFIED;
}

template <typename SendGuildCardCmdT>
static HandlerResult on_client_60_62_6C_6D_C9_CB(shared_ptr<ServerState> s,
    ProxyServer::LinkedSession& session, uint16_t command, uint32_t flag, string& data) {
  if (session.license && !data.empty()) {
    if (data[0] == 0x06) {
      auto& cmd = check_size_t<SendGuildCardCmdT>(data);
      if (cmd.guild_card_number == session.license->serial_number) {
        cmd.guild_card_number = session.remote_guild_card_number;
      }
    }
  }

  if (!data.empty()) {
    if (data[0] == 0x2F || data[0] == 0x4C) {
      if (session.infinite_hp) {
        vector<PSOSubcommand> subs;
        for (size_t amount = 1020; amount > 0;) {
          auto& sub1 = subs.emplace_back();
          sub1.word[0] = 0x029A;
          sub1.byte[2] = session.lobby_client_id;
          sub1.byte[3] = 0x00;
          auto& sub2 = subs.emplace_back();
          sub2.word[0] = 0x0000;
          sub2.byte[2] = PlayerStatsChange::ADD_HP;
          sub2.byte[3] = (amount > 0xFF) ? 0xFF : amount;
          amount -= sub2.byte[3];
        }
        session.client_channel.send(0x60, 0x00, subs.data(), subs.size() * sizeof(PSOSubcommand));
      }
    } else if (data[0] == 0x48) {
      if (session.infinite_tp) {
        PSOSubcommand subs[2];
        subs[0].word[0] = 0x029A;
        subs[0].byte[2] = session.lobby_client_id;
        subs[0].byte[3] = 0x00;
        subs[1].word[0] = 0x0000;
        subs[1].byte[2] = PlayerStatsChange::ADD_TP;
        subs[1].byte[3] = 0xFF;
        session.client_channel.send(0x60, 0x00, &subs[0], sizeof(subs));
      }
    }
  }
  return on_client_60_62_6C_6D_C9_CB<void>(s, session, command, flag, data);
}

template <>
HandlerResult on_client_60_62_6C_6D_C9_CB<void>(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  check_implemented_subcommand(session, data);

  if (!data.empty() && (data[0] == 0x05) && session.switch_assist) {
    auto& cmd = check_size_t<G_SwitchStateChanged_6x05>(data);
    if (cmd.enabled && cmd.switch_id != 0xFFFF) {
      if (session.last_switch_enabled_command.subcommand == 0x05) {
        session.log.info("Switch assist: replaying previous enable command");
        session.server_channel.send(0x60, 0x00, &session.last_switch_enabled_command,
            sizeof(session.last_switch_enabled_command));
        session.client_channel.send(0x60, 0x00, &session.last_switch_enabled_command,
            sizeof(session.last_switch_enabled_command));
      }
      session.last_switch_enabled_command = cmd;
    }
  }

  return HandlerResult::Type::FORWARD;
}

static HandlerResult on_client_dc_pc_v3_A0_A1(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string&) {
  if (!session.license) {
    return HandlerResult::Type::FORWARD;
  }

  // For licensed sessions, send them back to newserv's main menu instead of
  // going to the remote server's ship/block select menu
  session.send_to_game_server();
  return HandlerResult::Type::SUPPRESS;
}



typedef HandlerResult (*on_command_t)(
    shared_ptr<ServerState> s,
    ProxyServer::LinkedSession& session,
    uint16_t command,
    uint32_t flag,
    string& data);

// [version][command][is_client]
static on_command_t handlers[6][0x100][2] = {
  {
    // PATCH  SERVER                          CLIENT
    /* 00 */ {nullptr,                        nullptr},
    /* 01 */ {nullptr,                        nullptr},
    /* 02 */ {on_server_dc_pc_v3_patch_02_17, nullptr},
    /* 03 */ {nullptr,                        nullptr},
    /* 04 */ {nullptr,                        nullptr},
    /* 05 */ {nullptr,                        nullptr},
    /* 06 */ {nullptr,                        nullptr},
    /* 07 */ {nullptr,                        nullptr},
    /* 08 */ {nullptr,                        nullptr},
    /* 09 */ {nullptr,                        nullptr},
    /* 0A */ {nullptr,                        nullptr},
    /* 0B */ {nullptr,                        nullptr},
    /* 0C */ {nullptr,                        nullptr},
    /* 0D */ {nullptr,                        nullptr},
    /* 0E */ {nullptr,                        nullptr},
    /* 0F */ {nullptr,                        nullptr},
    /* 10 */ {nullptr,                        nullptr},
    /* 11 */ {nullptr,                        nullptr},
    /* 12 */ {nullptr,                        nullptr},
    /* 13 */ {nullptr,                        nullptr},
    /* 14 */ {on_server_game_19_patch_14,     nullptr},
    /* 15 */ {nullptr,                        nullptr},
    /* 16 */ {nullptr,                        nullptr},
    /* 17 */ {nullptr,                        nullptr},
    /* 18 */ {nullptr,                        nullptr},
    /* 19 */ {nullptr,                        nullptr},
    /* 1A */ {nullptr,                        nullptr},
    /* 1B */ {nullptr,                        nullptr},
    /* 1C */ {nullptr,                        nullptr},
    /* 1D */ {nullptr,                        nullptr},
    /* 1E */ {nullptr,                        nullptr},
    /* 1F */ {nullptr,                        nullptr},
    // PATCH  SERVER                          CLIENT
    /* 20 */ {nullptr,                        nullptr},
    /* 21 */ {nullptr,                        nullptr},
    /* 22 */ {nullptr,                        nullptr},
    /* 23 */ {nullptr,                        nullptr},
    /* 24 */ {nullptr,                        nullptr},
    /* 25 */ {nullptr,                        nullptr},
    /* 26 */ {nullptr,                        nullptr},
    /* 27 */ {nullptr,                        nullptr},
    /* 28 */ {nullptr,                        nullptr},
    /* 29 */ {nullptr,                        nullptr},
    /* 2A */ {nullptr,                        nullptr},
    /* 2B */ {nullptr,                        nullptr},
    /* 2C */ {nullptr,                        nullptr},
    /* 2D */ {nullptr,                        nullptr},
    /* 2E */ {nullptr,                        nullptr},
    /* 2F */ {nullptr,                        nullptr},
    /* 30 */ {nullptr,                        nullptr},
    /* 31 */ {nullptr,                        nullptr},
    /* 32 */ {nullptr,                        nullptr},
    /* 33 */ {nullptr,                        nullptr},
    /* 34 */ {nullptr,                        nullptr},
    /* 35 */ {nullptr,                        nullptr},
    /* 36 */ {nullptr,                        nullptr},
    /* 37 */ {nullptr,                        nullptr},
    /* 38 */ {nullptr,                        nullptr},
    /* 39 */ {nullptr,                        nullptr},
    /* 3A */ {nullptr,                        nullptr},
    /* 3B */ {nullptr,                        nullptr},
    /* 3C */ {nullptr,                        nullptr},
    /* 3D */ {nullptr,                        nullptr},
    /* 3E */ {nullptr,                        nullptr},
    /* 3F */ {nullptr,                        nullptr},
    // PATCH  SERVER                          CLIENT
    /* 40 */ {nullptr,                        nullptr},
    /* 41 */ {nullptr,                        nullptr},
    /* 42 */ {nullptr,                        nullptr},
    /* 43 */ {nullptr,                        nullptr},
    /* 44 */ {nullptr,                        nullptr},
    /* 45 */ {nullptr,                        nullptr},
    /* 46 */ {nullptr,                        nullptr},
    /* 47 */ {nullptr,                        nullptr},
    /* 48 */ {nullptr,                        nullptr},
    /* 49 */ {nullptr,                        nullptr},
    /* 4A */ {nullptr,                        nullptr},
    /* 4B */ {nullptr,                        nullptr},
    /* 4C */ {nullptr,                        nullptr},
    /* 4D */ {nullptr,                        nullptr},
    /* 4E */ {nullptr,                        nullptr},
    /* 4F */ {nullptr,                        nullptr},
    /* 50 */ {nullptr,                        nullptr},
    /* 51 */ {nullptr,                        nullptr},
    /* 52 */ {nullptr,                        nullptr},
    /* 53 */ {nullptr,                        nullptr},
    /* 54 */ {nullptr,                        nullptr},
    /* 55 */ {nullptr,                        nullptr},
    /* 56 */ {nullptr,                        nullptr},
    /* 57 */ {nullptr,                        nullptr},
    /* 58 */ {nullptr,                        nullptr},
    /* 59 */ {nullptr,                        nullptr},
    /* 5A */ {nullptr,                        nullptr},
    /* 5B */ {nullptr,                        nullptr},
    /* 5C */ {nullptr,                        nullptr},
    /* 5D */ {nullptr,                        nullptr},
    /* 5E */ {nullptr,                        nullptr},
    /* 5F */ {nullptr,                        nullptr},
    // PATCH  SERVER                          CLIENT
    /* 60 */ {nullptr,                        nullptr},
    /* 61 */ {nullptr,                        nullptr},
    /* 62 */ {nullptr,                        nullptr},
    /* 63 */ {nullptr,                        nullptr},
    /* 64 */ {nullptr,                        nullptr},
    /* 65 */ {nullptr,                        nullptr},
    /* 66 */ {nullptr,                        nullptr},
    /* 67 */ {nullptr,                        nullptr},
    /* 68 */ {nullptr,                        nullptr},
    /* 69 */ {nullptr,                        nullptr},
    /* 6A */ {nullptr,                        nullptr},
    /* 6B */ {nullptr,                        nullptr},
    /* 6C */ {nullptr,                        nullptr},
    /* 6D */ {nullptr,                        nullptr},
    /* 6E */ {nullptr,                        nullptr},
    /* 6F */ {nullptr,                        nullptr},
    /* 70 */ {nullptr,                        nullptr},
    /* 71 */ {nullptr,                        nullptr},
    /* 72 */ {nullptr,                        nullptr},
    /* 73 */ {nullptr,                        nullptr},
    /* 74 */ {nullptr,                        nullptr},
    /* 75 */ {nullptr,                        nullptr},
    /* 76 */ {nullptr,                        nullptr},
    /* 77 */ {nullptr,                        nullptr},
    /* 78 */ {nullptr,                        nullptr},
    /* 79 */ {nullptr,                        nullptr},
    /* 7A */ {nullptr,                        nullptr},
    /* 7B */ {nullptr,                        nullptr},
    /* 7C */ {nullptr,                        nullptr},
    /* 7D */ {nullptr,                        nullptr},
    /* 7E */ {nullptr,                        nullptr},
    /* 7F */ {nullptr,                        nullptr},
    // PATCH  SERVER                          CLIENT
    /* 80 */ {nullptr,                        nullptr},
    /* 81 */ {nullptr,                        nullptr},
    /* 82 */ {nullptr,                        nullptr},
    /* 83 */ {nullptr,                        nullptr},
    /* 84 */ {nullptr,                        nullptr},
    /* 85 */ {nullptr,                        nullptr},
    /* 86 */ {nullptr,                        nullptr},
    /* 87 */ {nullptr,                        nullptr},
    /* 88 */ {nullptr,                        nullptr},
    /* 89 */ {nullptr,                        nullptr},
    /* 8A */ {nullptr,                        nullptr},
    /* 8B */ {nullptr,                        nullptr},
    /* 8C */ {nullptr,                        nullptr},
    /* 8D */ {nullptr,                        nullptr},
    /* 8E */ {nullptr,                        nullptr},
    /* 8F */ {nullptr,                        nullptr},
    /* 90 */ {nullptr,                        nullptr},
    /* 91 */ {nullptr,                        nullptr},
    /* 92 */ {nullptr,                        nullptr},
    /* 93 */ {nullptr,                        nullptr},
    /* 94 */ {nullptr,                        nullptr},
    /* 95 */ {nullptr,                        nullptr},
    /* 96 */ {nullptr,                        nullptr},
    /* 97 */ {nullptr,                        nullptr},
    /* 98 */ {nullptr,                        nullptr},
    /* 99 */ {nullptr,                        nullptr},
    /* 9A */ {nullptr,                        nullptr},
    /* 9B */ {nullptr,                        nullptr},
    /* 9C */ {nullptr,                        nullptr},
    /* 9D */ {nullptr,                        nullptr},
    /* 9E */ {nullptr,                        nullptr},
    /* 9F */ {nullptr,                        nullptr},
    // PATCH  SERVER                          CLIENT
    /* A0 */ {nullptr,                        nullptr},
    /* A1 */ {nullptr,                        nullptr},
    /* A2 */ {nullptr,                        nullptr},
    /* A3 */ {nullptr,                        nullptr},
    /* A4 */ {nullptr,                        nullptr},
    /* A5 */ {nullptr,                        nullptr},
    /* A6 */ {nullptr,                        nullptr},
    /* A7 */ {nullptr,                        nullptr},
    /* A8 */ {nullptr,                        nullptr},
    /* A9 */ {nullptr,                        nullptr},
    /* AA */ {nullptr,                        nullptr},
    /* AB */ {nullptr,                        nullptr},
    /* AC */ {nullptr,                        nullptr},
    /* AD */ {nullptr,                        nullptr},
    /* AE */ {nullptr,                        nullptr},
    /* AF */ {nullptr,                        nullptr},
    /* B0 */ {nullptr,                        nullptr},
    /* B1 */ {nullptr,                        nullptr},
    /* B2 */ {nullptr,                        nullptr},
    /* B3 */ {nullptr,                        nullptr},
    /* B4 */ {nullptr,                        nullptr},
    /* B5 */ {nullptr,                        nullptr},
    /* B6 */ {nullptr,                        nullptr},
    /* B7 */ {nullptr,                        nullptr},
    /* B8 */ {nullptr,                        nullptr},
    /* B9 */ {nullptr,                        nullptr},
    /* BA */ {nullptr,                        nullptr},
    /* BB */ {nullptr,                        nullptr},
    /* BC */ {nullptr,                        nullptr},
    /* BD */ {nullptr,                        nullptr},
    /* BE */ {nullptr,                        nullptr},
    /* BF */ {nullptr,                        nullptr},
    // PATCH  SERVER                          CLIENT
    /* C0 */ {nullptr,                        nullptr},
    /* C1 */ {nullptr,                        nullptr},
    /* C2 */ {nullptr,                        nullptr},
    /* C3 */ {nullptr,                        nullptr},
    /* C4 */ {nullptr,                        nullptr},
    /* C5 */ {nullptr,                        nullptr},
    /* C6 */ {nullptr,                        nullptr},
    /* C7 */ {nullptr,                        nullptr},
    /* C8 */ {nullptr,                        nullptr},
    /* C9 */ {nullptr,                        nullptr},
    /* CA */ {nullptr,                        nullptr},
    /* CB */ {nullptr,                        nullptr},
    /* CC */ {nullptr,                        nullptr},
    /* CD */ {nullptr,                        nullptr},
    /* CE */ {nullptr,                        nullptr},
    /* CF */ {nullptr,                        nullptr},
    /* D0 */ {nullptr,                        nullptr},
    /* D1 */ {nullptr,                        nullptr},
    /* D2 */ {nullptr,                        nullptr},
    /* D3 */ {nullptr,                        nullptr},
    /* D4 */ {nullptr,                        nullptr},
    /* D5 */ {nullptr,                        nullptr},
    /* D6 */ {nullptr,                        nullptr},
    /* D7 */ {nullptr,                        nullptr},
    /* D8 */ {nullptr,                        nullptr},
    /* D9 */ {nullptr,                        nullptr},
    /* DA */ {nullptr,                        nullptr},
    /* DB */ {nullptr,                        nullptr},
    /* DC */ {nullptr,                        nullptr},
    /* DD */ {nullptr,                        nullptr},
    /* DE */ {nullptr,                        nullptr},
    /* DF */ {nullptr,                        nullptr},
    // PATCH  SERVER                          CLIENT
    /* E0 */ {nullptr,                        nullptr},
    /* E1 */ {nullptr,                        nullptr},
    /* E2 */ {nullptr,                        nullptr},
    /* E3 */ {nullptr,                        nullptr},
    /* E4 */ {nullptr,                        nullptr},
    /* E5 */ {nullptr,                        nullptr},
    /* E6 */ {nullptr,                        nullptr},
    /* E7 */ {nullptr,                        nullptr},
    /* E8 */ {nullptr,                        nullptr},
    /* E9 */ {nullptr,                        nullptr},
    /* EA */ {nullptr,                        nullptr},
    /* EB */ {nullptr,                        nullptr},
    /* EC */ {nullptr,                        nullptr},
    /* ED */ {nullptr,                        nullptr},
    /* EE */ {nullptr,                        nullptr},
    /* EF */ {nullptr,                        nullptr},
    /* F0 */ {nullptr,                        nullptr},
    /* F1 */ {nullptr,                        nullptr},
    /* F2 */ {nullptr,                        nullptr},
    /* F3 */ {nullptr,                        nullptr},
    /* F4 */ {nullptr,                        nullptr},
    /* F5 */ {nullptr,                        nullptr},
    /* F6 */ {nullptr,                        nullptr},
    /* F7 */ {nullptr,                        nullptr},
    /* F8 */ {nullptr,                        nullptr},
    /* F9 */ {nullptr,                        nullptr},
    /* FA */ {nullptr,                        nullptr},
    /* FB */ {nullptr,                        nullptr},
    /* FC */ {nullptr,                        nullptr},
    /* FD */ {nullptr,                        nullptr},
    /* FE */ {nullptr,                        nullptr},
    /* FF */ {nullptr,                        nullptr},
    // PATCH  SERVER                          CLIENT
  }, {
    // (DC)   SERVER                                          CLIENT
    /* 00 */ {nullptr,                                        nullptr},
    /* 01 */ {nullptr,                                        nullptr},
    /* 02 */ {on_server_dc_pc_v3_patch_02_17,                 nullptr},
    /* 03 */ {nullptr,                                        nullptr},
    /* 04 */ {on_server_dc_pc_v3_04,                          nullptr},
    /* 05 */ {nullptr,                                        nullptr},
    /* 06 */ {on_server_dc_pc_v3_06,                          on_client_06},
    /* 07 */ {nullptr,                                        nullptr},
    /* 08 */ {nullptr,                                        nullptr},
    /* 09 */ {nullptr,                                        nullptr},
    /* 0A */ {nullptr,                                        nullptr},
    /* 0B */ {nullptr,                                        nullptr},
    /* 0C */ {nullptr,                                        nullptr},
    /* 0D */ {nullptr,                                        nullptr},
    /* 0E */ {nullptr,                                        nullptr},
    /* 0F */ {nullptr,                                        nullptr},
    /* 10 */ {nullptr,                                        nullptr},
    /* 11 */ {nullptr,                                        nullptr},
    /* 12 */ {nullptr,                                        nullptr},
    /* 13 */ {on_server_13_A7,                                nullptr},
    /* 14 */ {nullptr,                                        nullptr},
    /* 15 */ {nullptr,                                        nullptr},
    /* 16 */ {nullptr,                                        nullptr},
    /* 17 */ {on_server_dc_pc_v3_patch_02_17,                 nullptr},
    /* 18 */ {nullptr,                                        nullptr},
    /* 19 */ {on_server_game_19_patch_14,                     nullptr},
    /* 1A */ {nullptr,                                        nullptr},
    /* 1B */ {nullptr,                                        nullptr},
    /* 1C */ {nullptr,                                        nullptr},
    /* 1D */ {nullptr,                                        nullptr},
    /* 1E */ {nullptr,                                        nullptr},
    /* 1F */ {nullptr,                                        nullptr},
    // (DC)   SERVER                                          CLIENT
    /* 20 */ {nullptr,                                        nullptr},
    /* 21 */ {nullptr,                                        nullptr},
    /* 22 */ {nullptr,                                        nullptr},
    /* 23 */ {nullptr,                                        nullptr},
    /* 24 */ {nullptr,                                        nullptr},
    /* 25 */ {nullptr,                                        nullptr},
    /* 26 */ {nullptr,                                        nullptr},
    /* 27 */ {nullptr,                                        nullptr},
    /* 28 */ {nullptr,                                        nullptr},
    /* 29 */ {nullptr,                                        nullptr},
    /* 2A */ {nullptr,                                        nullptr},
    /* 2B */ {nullptr,                                        nullptr},
    /* 2C */ {nullptr,                                        nullptr},
    /* 2D */ {nullptr,                                        nullptr},
    /* 2E */ {nullptr,                                        nullptr},
    /* 2F */ {nullptr,                                        nullptr},
    /* 30 */ {nullptr,                                        nullptr},
    /* 31 */ {nullptr,                                        nullptr},
    /* 32 */ {nullptr,                                        nullptr},
    /* 33 */ {nullptr,                                        nullptr},
    /* 34 */ {nullptr,                                        nullptr},
    /* 35 */ {nullptr,                                        nullptr},
    /* 36 */ {nullptr,                                        nullptr},
    /* 37 */ {nullptr,                                        nullptr},
    /* 38 */ {nullptr,                                        nullptr},
    /* 39 */ {nullptr,                                        nullptr},
    /* 3A */ {nullptr,                                        nullptr},
    /* 3B */ {nullptr,                                        nullptr},
    /* 3C */ {nullptr,                                        nullptr},
    /* 3D */ {nullptr,                                        nullptr},
    /* 3E */ {nullptr,                                        nullptr},
    /* 3F */ {nullptr,                                        nullptr},
    // (DC)   SERVER                                          CLIENT
    /* 40 */ {nullptr,                                        on_client_40},
    /* 41 */ {on_server_41<S_GuildCardSearchResult_DC_V3_41>, nullptr},
    /* 42 */ {nullptr,                                        nullptr},
    /* 43 */ {nullptr,                                        nullptr},
    /* 44 */ {on_server_44_A6<S_OpenFile_DC_44_A6>,           nullptr},
    /* 45 */ {nullptr,                                        nullptr},
    /* 46 */ {nullptr,                                        nullptr},
    /* 47 */ {nullptr,                                        nullptr},
    /* 48 */ {nullptr,                                        nullptr},
    /* 49 */ {nullptr,                                        nullptr},
    /* 4A */ {nullptr,                                        nullptr},
    /* 4B */ {nullptr,                                        nullptr},
    /* 4C */ {nullptr,                                        nullptr},
    /* 4D */ {nullptr,                                        nullptr},
    /* 4E */ {nullptr,                                        nullptr},
    /* 4F */ {nullptr,                                        nullptr},
    /* 50 */ {nullptr,                                        nullptr},
    /* 51 */ {nullptr,                                        nullptr},
    /* 52 */ {nullptr,                                        nullptr},
    /* 53 */ {nullptr,                                        nullptr},
    /* 54 */ {nullptr,                                        nullptr},
    /* 55 */ {nullptr,                                        nullptr},
    /* 56 */ {nullptr,                                        nullptr},
    /* 57 */ {nullptr,                                        nullptr},
    /* 58 */ {nullptr,                                        nullptr},
    /* 59 */ {nullptr,                                        nullptr},
    /* 5A */ {nullptr,                                        nullptr},
    /* 5B */ {nullptr,                                        nullptr},
    /* 5C */ {nullptr,                                        nullptr},
    /* 5D */ {nullptr,                                        nullptr},
    /* 5E */ {nullptr,                                        nullptr},
    /* 5F */ {nullptr,                                        nullptr},
    // (DC)   SERVER                                          CLIENT
    /* 60 */ {on_server_60_62_6C_6D_C9_CB,                    on_client_60_62_6C_6D_C9_CB<void>},
    /* 61 */ {nullptr,                                        nullptr},
    /* 62 */ {on_server_60_62_6C_6D_C9_CB,                    on_client_60_62_6C_6D_C9_CB<void>},
    /* 63 */ {nullptr,                                        nullptr},
    /* 64 */ {on_server_64<S_JoinGame_DC_GC_64>,              nullptr},
    /* 65 */ {on_server_65_67_68<S_JoinLobby_DC_GC_65_67_68>, nullptr},
    /* 66 */ {on_server_66_69,                                nullptr},
    /* 67 */ {on_server_65_67_68<S_JoinLobby_DC_GC_65_67_68>, nullptr},
    /* 68 */ {on_server_65_67_68<S_JoinLobby_DC_GC_65_67_68>, nullptr},
    /* 69 */ {on_server_66_69,                                nullptr},
    /* 6A */ {nullptr,                                        nullptr},
    /* 6B */ {nullptr,                                        nullptr},
    /* 6C */ {on_server_60_62_6C_6D_C9_CB,                    on_client_60_62_6C_6D_C9_CB<void>},
    /* 6D */ {on_server_60_62_6C_6D_C9_CB,                    on_client_60_62_6C_6D_C9_CB<void>},
    /* 6E */ {nullptr,                                        nullptr},
    /* 6F */ {nullptr,                                        nullptr},
    /* 70 */ {nullptr,                                        nullptr},
    /* 71 */ {nullptr,                                        nullptr},
    /* 72 */ {nullptr,                                        nullptr},
    /* 73 */ {nullptr,                                        nullptr},
    /* 74 */ {nullptr,                                        nullptr},
    /* 75 */ {nullptr,                                        nullptr},
    /* 76 */ {nullptr,                                        nullptr},
    /* 77 */ {nullptr,                                        nullptr},
    /* 78 */ {nullptr,                                        nullptr},
    /* 79 */ {nullptr,                                        nullptr},
    /* 7A */ {nullptr,                                        nullptr},
    /* 7B */ {nullptr,                                        nullptr},
    /* 7C */ {nullptr,                                        nullptr},
    /* 7D */ {nullptr,                                        nullptr},
    /* 7E */ {nullptr,                                        nullptr},
    /* 7F */ {nullptr,                                        nullptr},
    // (DC)   SERVER                                          CLIENT
    /* 80 */ {nullptr,                                        nullptr},
    /* 81 */ {nullptr,                                        nullptr},
    /* 82 */ {nullptr,                                        nullptr},
    /* 83 */ {nullptr,                                        nullptr},
    /* 84 */ {nullptr,                                        nullptr},
    /* 85 */ {nullptr,                                        nullptr},
    /* 86 */ {nullptr,                                        nullptr},
    /* 87 */ {nullptr,                                        nullptr},
    /* 88 */ {on_server_88,                                   nullptr},
    /* 89 */ {nullptr,                                        nullptr},
    /* 8A */ {nullptr,                                        nullptr},
    /* 8B */ {nullptr,                                        nullptr},
    /* 8C */ {nullptr,                                        nullptr},
    /* 8D */ {nullptr,                                        nullptr},
    /* 8E */ {nullptr,                                        nullptr},
    /* 8F */ {nullptr,                                        nullptr},
    /* 90 */ {nullptr,                                        nullptr},
    /* 91 */ {nullptr,                                        nullptr},
    /* 92 */ {nullptr,                                        nullptr},
    /* 93 */ {nullptr,                                        nullptr},
    /* 94 */ {nullptr,                                        nullptr},
    /* 95 */ {nullptr,                                        nullptr},
    /* 96 */ {nullptr,                                        nullptr},
    /* 97 */ {on_server_97,                                   nullptr},
    /* 98 */ {nullptr,                                        on_client_98},
    /* 99 */ {nullptr,                                        nullptr},
    /* 9A */ {nullptr,                                        nullptr},
    /* 9B */ {nullptr,                                        nullptr},
    /* 9C */ {nullptr,                                        nullptr},
    /* 9D */ {nullptr,                                        nullptr},
    /* 9E */ {nullptr,                                        nullptr},
    /* 9F */ {nullptr,                                        nullptr},
    // (DC)   SERVER                                          CLIENT
    /* A0 */ {nullptr,                                        on_client_dc_pc_v3_A0_A1},
    /* A1 */ {nullptr,                                        on_client_dc_pc_v3_A0_A1},
    /* A2 */ {nullptr,                                        nullptr},
    /* A3 */ {nullptr,                                        nullptr},
    /* A4 */ {nullptr,                                        nullptr},
    /* A5 */ {nullptr,                                        nullptr},
    /* A6 */ {on_server_44_A6<S_OpenFile_DC_44_A6>,           nullptr},
    /* A7 */ {on_server_13_A7,                                nullptr},
    /* A8 */ {nullptr,                                        nullptr},
    /* A9 */ {nullptr,                                        nullptr},
    /* AA */ {nullptr,                                        nullptr},
    /* AB */ {nullptr,                                        nullptr},
    /* AC */ {nullptr,                                        nullptr},
    /* AD */ {nullptr,                                        nullptr},
    /* AE */ {nullptr,                                        nullptr},
    /* AF */ {nullptr,                                        nullptr},
    /* B0 */ {nullptr,                                        nullptr},
    /* B1 */ {nullptr,                                        nullptr},
    /* B2 */ {nullptr,                                        nullptr},
    /* B3 */ {nullptr,                                        nullptr},
    /* B4 */ {nullptr,                                        nullptr},
    /* B5 */ {nullptr,                                        nullptr},
    /* B6 */ {nullptr,                                        nullptr},
    /* B7 */ {nullptr,                                        nullptr},
    /* B8 */ {nullptr,                                        nullptr},
    /* B9 */ {nullptr,                                        nullptr},
    /* BA */ {nullptr,                                        nullptr},
    /* BB */ {nullptr,                                        nullptr},
    /* BC */ {nullptr,                                        nullptr},
    /* BD */ {nullptr,                                        nullptr},
    /* BE */ {nullptr,                                        nullptr},
    /* BF */ {nullptr,                                        nullptr},
    // (DC)   SERVER                                          CLIENT
    /* C0 */ {nullptr,                                        nullptr},
    /* C1 */ {nullptr,                                        nullptr},
    /* C2 */ {nullptr,                                        nullptr},
    /* C3 */ {nullptr,                                        nullptr},
    /* C4 */ {nullptr,                                        nullptr},
    /* C5 */ {nullptr,                                        nullptr},
    /* C6 */ {nullptr,                                        nullptr},
    /* C7 */ {nullptr,                                        nullptr},
    /* C8 */ {nullptr,                                        nullptr},
    /* C9 */ {nullptr,                                        nullptr},
    /* CA */ {nullptr,                                        nullptr},
    /* CB */ {nullptr,                                        nullptr},
    /* CC */ {nullptr,                                        nullptr},
    /* CD */ {nullptr,                                        nullptr},
    /* CE */ {nullptr,                                        nullptr},
    /* CF */ {nullptr,                                        nullptr},
    /* D0 */ {nullptr,                                        nullptr},
    /* D1 */ {nullptr,                                        nullptr},
    /* D2 */ {nullptr,                                        nullptr},
    /* D3 */ {nullptr,                                        nullptr},
    /* D4 */ {nullptr,                                        nullptr},
    /* D5 */ {nullptr,                                        nullptr},
    /* D6 */ {nullptr,                                        nullptr},
    /* D7 */ {nullptr,                                        nullptr},
    /* D8 */ {nullptr,                                        nullptr},
    /* D9 */ {nullptr,                                        nullptr},
    /* DA */ {nullptr,                                        nullptr},
    /* DB */ {nullptr,                                        nullptr},
    /* DC */ {nullptr,                                        nullptr},
    /* DD */ {nullptr,                                        nullptr},
    /* DE */ {nullptr,                                        nullptr},
    /* DF */ {nullptr,                                        nullptr},
    // (DC)   SERVER                                          CLIENT
    /* E0 */ {nullptr,                                        nullptr},
    /* E1 */ {nullptr,                                        nullptr},
    /* E2 */ {nullptr,                                        nullptr},
    /* E3 */ {nullptr,                                        nullptr},
    /* E4 */ {nullptr,                                        nullptr},
    /* E5 */ {nullptr,                                        nullptr},
    /* E6 */ {nullptr,                                        nullptr},
    /* E7 */ {nullptr,                                        nullptr},
    /* E8 */ {nullptr,                                        nullptr},
    /* E9 */ {nullptr,                                        nullptr},
    /* EA */ {nullptr,                                        nullptr},
    /* EB */ {nullptr,                                        nullptr},
    /* EC */ {nullptr,                                        nullptr},
    /* ED */ {nullptr,                                        nullptr},
    /* EE */ {nullptr,                                        nullptr},
    /* EF */ {nullptr,                                        nullptr},
    /* F0 */ {nullptr,                                        nullptr},
    /* F1 */ {nullptr,                                        nullptr},
    /* F2 */ {nullptr,                                        nullptr},
    /* F3 */ {nullptr,                                        nullptr},
    /* F4 */ {nullptr,                                        nullptr},
    /* F5 */ {nullptr,                                        nullptr},
    /* F6 */ {nullptr,                                        nullptr},
    /* F7 */ {nullptr,                                        nullptr},
    /* F8 */ {nullptr,                                        nullptr},
    /* F9 */ {nullptr,                                        nullptr},
    /* FA */ {nullptr,                                        nullptr},
    /* FB */ {nullptr,                                        nullptr},
    /* FC */ {nullptr,                                        nullptr},
    /* FD */ {nullptr,                                        nullptr},
    /* FE */ {nullptr,                                        nullptr},
    /* FF */ {nullptr,                                        nullptr},
    // (DC)   SERVER                                          CLIENT
  }, {
    // (PC)   SERVER                                       CLIENT
    /* 00 */ {nullptr,                                     nullptr},
    /* 01 */ {nullptr,                                     nullptr},
    /* 02 */ {on_server_dc_pc_v3_patch_02_17,              nullptr},
    /* 03 */ {nullptr,                                     nullptr},
    /* 04 */ {on_server_dc_pc_v3_04,                       nullptr},
    /* 05 */ {nullptr,                                     nullptr},
    /* 06 */ {on_server_dc_pc_v3_06,                       on_client_06},
    /* 07 */ {nullptr,                                     nullptr},
    /* 08 */ {nullptr,                                     nullptr},
    /* 09 */ {nullptr,                                     nullptr},
    /* 0A */ {nullptr,                                     nullptr},
    /* 0B */ {nullptr,                                     nullptr},
    /* 0C */ {nullptr,                                     nullptr},
    /* 0D */ {nullptr,                                     nullptr},
    /* 0E */ {nullptr,                                     nullptr},
    /* 0F */ {nullptr,                                     nullptr},
    /* 10 */ {nullptr,                                     nullptr},
    /* 11 */ {nullptr,                                     nullptr},
    /* 12 */ {nullptr,                                     nullptr},
    /* 13 */ {on_server_13_A7,                             nullptr},
    /* 14 */ {nullptr,                                     nullptr},
    /* 15 */ {nullptr,                                     nullptr},
    /* 16 */ {nullptr,                                     nullptr},
    /* 17 */ {on_server_dc_pc_v3_patch_02_17,              nullptr},
    /* 18 */ {nullptr,                                     nullptr},
    /* 19 */ {on_server_game_19_patch_14,                  nullptr},
    /* 1A */ {nullptr,                                     nullptr},
    /* 1B */ {nullptr,                                     nullptr},
    /* 1C */ {nullptr,                                     nullptr},
    /* 1D */ {nullptr,                                     nullptr},
    /* 1E */ {nullptr,                                     nullptr},
    /* 1F */ {nullptr,                                     nullptr},
    // (PC)   SERVER                                       CLIENT
    /* 20 */ {nullptr,                                     nullptr},
    /* 21 */ {nullptr,                                     nullptr},
    /* 22 */ {nullptr,                                     nullptr},
    /* 23 */ {nullptr,                                     nullptr},
    /* 24 */ {nullptr,                                     nullptr},
    /* 25 */ {nullptr,                                     nullptr},
    /* 26 */ {nullptr,                                     nullptr},
    /* 27 */ {nullptr,                                     nullptr},
    /* 28 */ {nullptr,                                     nullptr},
    /* 29 */ {nullptr,                                     nullptr},
    /* 2A */ {nullptr,                                     nullptr},
    /* 2B */ {nullptr,                                     nullptr},
    /* 2C */ {nullptr,                                     nullptr},
    /* 2D */ {nullptr,                                     nullptr},
    /* 2E */ {nullptr,                                     nullptr},
    /* 2F */ {nullptr,                                     nullptr},
    /* 30 */ {nullptr,                                     nullptr},
    /* 31 */ {nullptr,                                     nullptr},
    /* 32 */ {nullptr,                                     nullptr},
    /* 33 */ {nullptr,                                     nullptr},
    /* 34 */ {nullptr,                                     nullptr},
    /* 35 */ {nullptr,                                     nullptr},
    /* 36 */ {nullptr,                                     nullptr},
    /* 37 */ {nullptr,                                     nullptr},
    /* 38 */ {nullptr,                                     nullptr},
    /* 39 */ {nullptr,                                     nullptr},
    /* 3A */ {nullptr,                                     nullptr},
    /* 3B */ {nullptr,                                     nullptr},
    /* 3C */ {nullptr,                                     nullptr},
    /* 3D */ {nullptr,                                     nullptr},
    /* 3E */ {nullptr,                                     nullptr},
    /* 3F */ {nullptr,                                     nullptr},
    // (PC)   SERVER                                       CLIENT
    /* 40 */ {nullptr,                                     on_client_40},
    /* 41 */ {on_server_41<S_GuildCardSearchResult_PC_41>, nullptr},
    /* 42 */ {nullptr,                                     nullptr},
    /* 43 */ {nullptr,                                     nullptr},
    /* 44 */ {on_server_44_A6<S_OpenFile_PC_V3_44_A6>,     nullptr},
    /* 45 */ {nullptr,                                     nullptr},
    /* 46 */ {nullptr,                                     nullptr},
    /* 47 */ {nullptr,                                     nullptr},
    /* 48 */ {nullptr,                                     nullptr},
    /* 49 */ {nullptr,                                     nullptr},
    /* 4A */ {nullptr,                                     nullptr},
    /* 4B */ {nullptr,                                     nullptr},
    /* 4C */ {nullptr,                                     nullptr},
    /* 4D */ {nullptr,                                     nullptr},
    /* 4E */ {nullptr,                                     nullptr},
    /* 4F */ {nullptr,                                     nullptr},
    /* 50 */ {nullptr,                                     nullptr},
    /* 51 */ {nullptr,                                     nullptr},
    /* 52 */ {nullptr,                                     nullptr},
    /* 53 */ {nullptr,                                     nullptr},
    /* 54 */ {nullptr,                                     nullptr},
    /* 55 */ {nullptr,                                     nullptr},
    /* 56 */ {nullptr,                                     nullptr},
    /* 57 */ {nullptr,                                     nullptr},
    /* 58 */ {nullptr,                                     nullptr},
    /* 59 */ {nullptr,                                     nullptr},
    /* 5A */ {nullptr,                                     nullptr},
    /* 5B */ {nullptr,                                     nullptr},
    /* 5C */ {nullptr,                                     nullptr},
    /* 5D */ {nullptr,                                     nullptr},
    /* 5E */ {nullptr,                                     nullptr},
    /* 5F */ {nullptr,                                     nullptr},
    // (PC)   SERVER                                       CLIENT
    /* 60 */ {on_server_60_62_6C_6D_C9_CB,                 on_client_60_62_6C_6D_C9_CB<void>},
    /* 61 */ {nullptr,                                     nullptr},
    /* 62 */ {on_server_60_62_6C_6D_C9_CB,                 on_client_60_62_6C_6D_C9_CB<void>},
    /* 63 */ {nullptr,                                     nullptr},
    /* 64 */ {on_server_64<S_JoinGame_PC_64>,              nullptr},
    /* 65 */ {on_server_65_67_68<S_JoinLobby_PC_65_67_68>, nullptr},
    /* 66 */ {on_server_66_69,                             nullptr},
    /* 67 */ {on_server_65_67_68<S_JoinLobby_PC_65_67_68>, nullptr},
    /* 68 */ {on_server_65_67_68<S_JoinLobby_PC_65_67_68>, nullptr},
    /* 69 */ {on_server_66_69,                             nullptr},
    /* 6A */ {nullptr,                                     nullptr},
    /* 6B */ {nullptr,                                     nullptr},
    /* 6C */ {on_server_60_62_6C_6D_C9_CB,                 on_client_60_62_6C_6D_C9_CB<void>},
    /* 6D */ {on_server_60_62_6C_6D_C9_CB,                 on_client_60_62_6C_6D_C9_CB<void>},
    /* 6E */ {nullptr,                                     nullptr},
    /* 6F */ {nullptr,                                     nullptr},
    /* 70 */ {nullptr,                                     nullptr},
    /* 71 */ {nullptr,                                     nullptr},
    /* 72 */ {nullptr,                                     nullptr},
    /* 73 */ {nullptr,                                     nullptr},
    /* 74 */ {nullptr,                                     nullptr},
    /* 75 */ {nullptr,                                     nullptr},
    /* 76 */ {nullptr,                                     nullptr},
    /* 77 */ {nullptr,                                     nullptr},
    /* 78 */ {nullptr,                                     nullptr},
    /* 79 */ {nullptr,                                     nullptr},
    /* 7A */ {nullptr,                                     nullptr},
    /* 7B */ {nullptr,                                     nullptr},
    /* 7C */ {nullptr,                                     nullptr},
    /* 7D */ {nullptr,                                     nullptr},
    /* 7E */ {nullptr,                                     nullptr},
    /* 7F */ {nullptr,                                     nullptr},
    // (PC)   SERVER                                       CLIENT
    /* 80 */ {nullptr,                                     nullptr},
    /* 81 */ {nullptr,                                     nullptr},
    /* 82 */ {nullptr,                                     nullptr},
    /* 83 */ {nullptr,                                     nullptr},
    /* 84 */ {nullptr,                                     nullptr},
    /* 85 */ {nullptr,                                     nullptr},
    /* 86 */ {nullptr,                                     nullptr},
    /* 87 */ {nullptr,                                     nullptr},
    /* 88 */ {on_server_88,                                nullptr},
    /* 89 */ {nullptr,                                     nullptr},
    /* 8A */ {nullptr,                                     nullptr},
    /* 8B */ {nullptr,                                     nullptr},
    /* 8C */ {nullptr,                                     nullptr},
    /* 8D */ {nullptr,                                     nullptr},
    /* 8E */ {nullptr,                                     nullptr},
    /* 8F */ {nullptr,                                     nullptr},
    /* 90 */ {nullptr,                                     nullptr},
    /* 91 */ {nullptr,                                     nullptr},
    /* 92 */ {nullptr,                                     nullptr},
    /* 93 */ {nullptr,                                     nullptr},
    /* 94 */ {nullptr,                                     nullptr},
    /* 95 */ {nullptr,                                     nullptr},
    /* 96 */ {nullptr,                                     nullptr},
    /* 97 */ {on_server_97,                                nullptr},
    /* 98 */ {nullptr,                                     on_client_98},
    /* 99 */ {nullptr,                                     nullptr},
    /* 9A */ {nullptr,                                     nullptr},
    /* 9B */ {nullptr,                                     nullptr},
    /* 9C */ {nullptr,                                     nullptr},
    /* 9D */ {nullptr,                                     nullptr},
    /* 9E */ {nullptr,                                     nullptr},
    /* 9F */ {nullptr,                                     nullptr},
    // (PC)   SERVER                                       CLIENT
    /* A0 */ {nullptr,                                     on_client_dc_pc_v3_A0_A1},
    /* A1 */ {nullptr,                                     on_client_dc_pc_v3_A0_A1},
    /* A2 */ {nullptr,                                     nullptr},
    /* A3 */ {nullptr,                                     nullptr},
    /* A4 */ {nullptr,                                     nullptr},
    /* A5 */ {nullptr,                                     nullptr},
    /* A6 */ {on_server_44_A6<S_OpenFile_PC_V3_44_A6>,     nullptr},
    /* A7 */ {on_server_13_A7,                             nullptr},
    /* A8 */ {nullptr,                                     nullptr},
    /* A9 */ {nullptr,                                     nullptr},
    /* AA */ {nullptr,                                     nullptr},
    /* AB */ {nullptr,                                     nullptr},
    /* AC */ {nullptr,                                     nullptr},
    /* AD */ {nullptr,                                     nullptr},
    /* AE */ {nullptr,                                     nullptr},
    /* AF */ {nullptr,                                     nullptr},
    /* B0 */ {nullptr,                                     nullptr},
    /* B1 */ {nullptr,                                     nullptr},
    /* B2 */ {nullptr,                                     nullptr},
    /* B3 */ {nullptr,                                     nullptr},
    /* B4 */ {nullptr,                                     nullptr},
    /* B5 */ {nullptr,                                     nullptr},
    /* B6 */ {nullptr,                                     nullptr},
    /* B7 */ {nullptr,                                     nullptr},
    /* B8 */ {nullptr,                                     nullptr},
    /* B9 */ {nullptr,                                     nullptr},
    /* BA */ {nullptr,                                     nullptr},
    /* BB */ {nullptr,                                     nullptr},
    /* BC */ {nullptr,                                     nullptr},
    /* BD */ {nullptr,                                     nullptr},
    /* BE */ {nullptr,                                     nullptr},
    /* BF */ {nullptr,                                     nullptr},
    // (PC)   SERVER                                       CLIENT
    /* C0 */ {nullptr,                                     nullptr},
    /* C1 */ {nullptr,                                     nullptr},
    /* C2 */ {nullptr,                                     nullptr},
    /* C3 */ {nullptr,                                     nullptr},
    /* C4 */ {nullptr,                                     nullptr},
    /* C5 */ {nullptr,                                     nullptr},
    /* C6 */ {nullptr,                                     nullptr},
    /* C7 */ {nullptr,                                     nullptr},
    /* C8 */ {nullptr,                                     nullptr},
    /* C9 */ {nullptr,                                     nullptr},
    /* CA */ {nullptr,                                     nullptr},
    /* CB */ {nullptr,                                     nullptr},
    /* CC */ {nullptr,                                     nullptr},
    /* CD */ {nullptr,                                     nullptr},
    /* CE */ {nullptr,                                     nullptr},
    /* CF */ {nullptr,                                     nullptr},
    /* D0 */ {nullptr,                                     nullptr},
    /* D1 */ {nullptr,                                     nullptr},
    /* D2 */ {nullptr,                                     nullptr},
    /* D3 */ {nullptr,                                     nullptr},
    /* D4 */ {nullptr,                                     nullptr},
    /* D5 */ {nullptr,                                     nullptr},
    /* D6 */ {nullptr,                                     nullptr},
    /* D7 */ {nullptr,                                     nullptr},
    /* D8 */ {nullptr,                                     nullptr},
    /* D9 */ {nullptr,                                     nullptr},
    /* DA */ {nullptr,                                     nullptr},
    /* DB */ {nullptr,                                     nullptr},
    /* DC */ {nullptr,                                     nullptr},
    /* DD */ {nullptr,                                     nullptr},
    /* DE */ {nullptr,                                     nullptr},
    /* DF */ {nullptr,                                     nullptr},
    // (PC)   SERVER                                       CLIENT
    /* E0 */ {nullptr,                                     nullptr},
    /* E1 */ {nullptr,                                     nullptr},
    /* E2 */ {nullptr,                                     nullptr},
    /* E3 */ {nullptr,                                     nullptr},
    /* E4 */ {nullptr,                                     nullptr},
    /* E5 */ {nullptr,                                     nullptr},
    /* E6 */ {nullptr,                                     nullptr},
    /* E7 */ {nullptr,                                     nullptr},
    /* E8 */ {nullptr,                                     nullptr},
    /* E9 */ {nullptr,                                     nullptr},
    /* EA */ {nullptr,                                     nullptr},
    /* EB */ {nullptr,                                     nullptr},
    /* EC */ {nullptr,                                     nullptr},
    /* ED */ {nullptr,                                     nullptr},
    /* EE */ {nullptr,                                     nullptr},
    /* EF */ {nullptr,                                     nullptr},
    /* F0 */ {nullptr,                                     nullptr},
    /* F1 */ {nullptr,                                     nullptr},
    /* F2 */ {nullptr,                                     nullptr},
    /* F3 */ {nullptr,                                     nullptr},
    /* F4 */ {nullptr,                                     nullptr},
    /* F5 */ {nullptr,                                     nullptr},
    /* F6 */ {nullptr,                                     nullptr},
    /* F7 */ {nullptr,                                     nullptr},
    /* F8 */ {nullptr,                                     nullptr},
    /* F9 */ {nullptr,                                     nullptr},
    /* FA */ {nullptr,                                     nullptr},
    /* FB */ {nullptr,                                     nullptr},
    /* FC */ {nullptr,                                     nullptr},
    /* FD */ {nullptr,                                     nullptr},
    /* FE */ {nullptr,                                     nullptr},
    /* FF */ {nullptr,                                     nullptr},
    // (PC)   SERVER                                       CLIENT
  }, {
    // (GC)   SERVER                                          CLIENT
    /* 00 */ {nullptr,                                        nullptr},
    /* 01 */ {nullptr,                                        nullptr},
    /* 02 */ {on_server_dc_pc_v3_patch_02_17,                 nullptr},
    /* 03 */ {nullptr,                                        nullptr},
    /* 04 */ {on_server_dc_pc_v3_04,                          nullptr},
    /* 05 */ {nullptr,                                        nullptr},
    /* 06 */ {on_server_dc_pc_v3_06,                          on_client_06},
    /* 07 */ {nullptr,                                        nullptr},
    /* 08 */ {nullptr,                                        nullptr},
    /* 09 */ {nullptr,                                        nullptr},
    /* 0A */ {nullptr,                                        nullptr},
    /* 0B */ {nullptr,                                        nullptr},
    /* 0C */ {nullptr,                                        nullptr},
    /* 0D */ {nullptr,                                        nullptr},
    /* 0E */ {nullptr,                                        nullptr},
    /* 0F */ {nullptr,                                        nullptr},
    /* 10 */ {nullptr,                                        nullptr},
    /* 11 */ {nullptr,                                        nullptr},
    /* 12 */ {nullptr,                                        nullptr},
    /* 13 */ {on_server_13_A7,                                nullptr},
    /* 14 */ {nullptr,                                        nullptr},
    /* 15 */ {nullptr,                                        nullptr},
    /* 16 */ {nullptr,                                        nullptr},
    /* 17 */ {on_server_dc_pc_v3_patch_02_17,                 nullptr},
    /* 18 */ {nullptr,                                        nullptr},
    /* 19 */ {on_server_game_19_patch_14,                     nullptr},
    /* 1A */ {on_server_v3_1A_D5,                             nullptr},
    /* 1B */ {nullptr,                                        nullptr},
    /* 1C */ {nullptr,                                        nullptr},
    /* 1D */ {nullptr,                                        nullptr},
    /* 1E */ {nullptr,                                        nullptr},
    /* 1F */ {nullptr,                                        nullptr},
    // (GC)   SERVER                                          CLIENT
    /* 20 */ {nullptr,                                        nullptr},
    /* 21 */ {nullptr,                                        nullptr},
    /* 22 */ {nullptr,                                        nullptr},
    /* 23 */ {nullptr,                                        nullptr},
    /* 24 */ {nullptr,                                        nullptr},
    /* 25 */ {nullptr,                                        nullptr},
    /* 26 */ {nullptr,                                        nullptr},
    /* 27 */ {nullptr,                                        nullptr},
    /* 28 */ {nullptr,                                        nullptr},
    /* 29 */ {nullptr,                                        nullptr},
    /* 2A */ {nullptr,                                        nullptr},
    /* 2B */ {nullptr,                                        nullptr},
    /* 2C */ {nullptr,                                        nullptr},
    /* 2D */ {nullptr,                                        nullptr},
    /* 2E */ {nullptr,                                        nullptr},
    /* 2F */ {nullptr,                                        nullptr},
    /* 30 */ {nullptr,                                        nullptr},
    /* 31 */ {nullptr,                                        nullptr},
    /* 32 */ {nullptr,                                        nullptr},
    /* 33 */ {nullptr,                                        nullptr},
    /* 34 */ {nullptr,                                        nullptr},
    /* 35 */ {nullptr,                                        nullptr},
    /* 36 */ {nullptr,                                        nullptr},
    /* 37 */ {nullptr,                                        nullptr},
    /* 38 */ {nullptr,                                        nullptr},
    /* 39 */ {nullptr,                                        nullptr},
    /* 3A */ {nullptr,                                        nullptr},
    /* 3B */ {nullptr,                                        nullptr},
    /* 3C */ {nullptr,                                        nullptr},
    /* 3D */ {nullptr,                                        nullptr},
    /* 3E */ {nullptr,                                        nullptr},
    /* 3F */ {nullptr,                                        nullptr},
    // (GC)   SERVER                                          CLIENT
    /* 40 */ {nullptr,                                        on_client_40},
    /* 41 */ {on_server_41<S_GuildCardSearchResult_DC_V3_41>, nullptr},
    /* 42 */ {nullptr,                                        nullptr},
    /* 43 */ {nullptr,                                        nullptr},
    /* 44 */ {on_server_44_A6<S_OpenFile_PC_V3_44_A6>,        nullptr},
    /* 45 */ {nullptr,                                        nullptr},
    /* 46 */ {nullptr,                                        nullptr},
    /* 47 */ {nullptr,                                        nullptr},
    /* 48 */ {nullptr,                                        nullptr},
    /* 49 */ {nullptr,                                        nullptr},
    /* 4A */ {nullptr,                                        nullptr},
    /* 4B */ {nullptr,                                        nullptr},
    /* 4C */ {nullptr,                                        nullptr},
    /* 4D */ {nullptr,                                        nullptr},
    /* 4E */ {nullptr,                                        nullptr},
    /* 4F */ {nullptr,                                        nullptr},
    /* 50 */ {nullptr,                                        nullptr},
    /* 51 */ {nullptr,                                        nullptr},
    /* 52 */ {nullptr,                                        nullptr},
    /* 53 */ {nullptr,                                        nullptr},
    /* 54 */ {nullptr,                                        nullptr},
    /* 55 */ {nullptr,                                        nullptr},
    /* 56 */ {nullptr,                                        nullptr},
    /* 57 */ {nullptr,                                        nullptr},
    /* 58 */ {nullptr,                                        nullptr},
    /* 59 */ {nullptr,                                        nullptr},
    /* 5A */ {nullptr,                                        nullptr},
    /* 5B */ {nullptr,                                        nullptr},
    /* 5C */ {nullptr,                                        nullptr},
    /* 5D */ {nullptr,                                        nullptr},
    /* 5E */ {nullptr,                                        nullptr},
    /* 5F */ {nullptr,                                        nullptr},
    // (GC)   SERVER                                          CLIENT
    /* 60 */ {on_server_60_62_6C_6D_C9_CB,                    on_client_60_62_6C_6D_C9_CB<G_SendGuildCard_V3_6x06>},
    /* 61 */ {nullptr,                                        nullptr},
    /* 62 */ {on_server_60_62_6C_6D_C9_CB,                    on_client_60_62_6C_6D_C9_CB<G_SendGuildCard_V3_6x06>},
    /* 63 */ {nullptr,                                        nullptr},
    /* 64 */ {on_server_64<S_JoinGame_DC_GC_64>,              nullptr},
    /* 65 */ {on_server_65_67_68<S_JoinLobby_DC_GC_65_67_68>, nullptr},
    /* 66 */ {on_server_66_69,                                nullptr},
    /* 67 */ {on_server_65_67_68<S_JoinLobby_DC_GC_65_67_68>, nullptr},
    /* 68 */ {on_server_65_67_68<S_JoinLobby_DC_GC_65_67_68>, nullptr},
    /* 69 */ {on_server_66_69,                                nullptr},
    /* 6A */ {nullptr,                                        nullptr},
    /* 6B */ {nullptr,                                        nullptr},
    /* 6C */ {on_server_60_62_6C_6D_C9_CB,                    on_client_60_62_6C_6D_C9_CB<G_SendGuildCard_V3_6x06>},
    /* 6D */ {on_server_60_62_6C_6D_C9_CB,                    on_client_60_62_6C_6D_C9_CB<G_SendGuildCard_V3_6x06>},
    /* 6E */ {nullptr,                                        nullptr},
    /* 6F */ {nullptr,                                        nullptr},
    /* 70 */ {nullptr,                                        nullptr},
    /* 71 */ {nullptr,                                        nullptr},
    /* 72 */ {nullptr,                                        nullptr},
    /* 73 */ {nullptr,                                        nullptr},
    /* 74 */ {nullptr,                                        nullptr},
    /* 75 */ {nullptr,                                        nullptr},
    /* 76 */ {nullptr,                                        nullptr},
    /* 77 */ {nullptr,                                        nullptr},
    /* 78 */ {nullptr,                                        nullptr},
    /* 79 */ {nullptr,                                        nullptr},
    /* 7A */ {nullptr,                                        nullptr},
    /* 7B */ {nullptr,                                        nullptr},
    /* 7C */ {nullptr,                                        nullptr},
    /* 7D */ {nullptr,                                        nullptr},
    /* 7E */ {nullptr,                                        nullptr},
    /* 7F */ {nullptr,                                        nullptr},
    // (GC)   SERVER                                          CLIENT
    /* 80 */ {nullptr,                                        nullptr},
    /* 81 */ {on_server_81<SC_SimpleMail_DC_V3_81>,           on_client_81<SC_SimpleMail_DC_V3_81>},
    /* 82 */ {nullptr,                                        nullptr},
    /* 83 */ {nullptr,                                        nullptr},
    /* 84 */ {nullptr,                                        nullptr},
    /* 85 */ {nullptr,                                        nullptr},
    /* 86 */ {nullptr,                                        nullptr},
    /* 87 */ {nullptr,                                        nullptr},
    /* 88 */ {on_server_88,                                   nullptr},
    /* 89 */ {nullptr,                                        nullptr},
    /* 8A */ {nullptr,                                        nullptr},
    /* 8B */ {nullptr,                                        nullptr},
    /* 8C */ {nullptr,                                        nullptr},
    /* 8D */ {nullptr,                                        nullptr},
    /* 8E */ {nullptr,                                        nullptr},
    /* 8F */ {nullptr,                                        nullptr},
    /* 90 */ {nullptr,                                        nullptr},
    /* 91 */ {nullptr,                                        nullptr},
    /* 92 */ {nullptr,                                        nullptr},
    /* 93 */ {nullptr,                                        nullptr},
    /* 94 */ {nullptr,                                        nullptr},
    /* 95 */ {nullptr,                                        nullptr},
    /* 96 */ {nullptr,                                        nullptr},
    /* 97 */ {on_server_97,                                   nullptr},
    /* 98 */ {nullptr,                                        on_client_98},
    /* 99 */ {nullptr,                                        nullptr},
    /* 9A */ {on_server_gc_9A,                                nullptr},
    /* 9B */ {nullptr,                                        nullptr},
    /* 9C */ {nullptr,                                        nullptr},
    /* 9D */ {nullptr,                                        nullptr},
    /* 9E */ {nullptr,                                        nullptr},
    /* 9F */ {nullptr,                                        nullptr},
    // (GC)   SERVER                                          CLIENT
    /* A0 */ {nullptr,                                        on_client_dc_pc_v3_A0_A1},
    /* A1 */ {nullptr,                                        on_client_dc_pc_v3_A0_A1},
    /* A2 */ {nullptr,                                        nullptr},
    /* A3 */ {nullptr,                                        nullptr},
    /* A4 */ {nullptr,                                        nullptr},
    /* A5 */ {nullptr,                                        nullptr},
    /* A6 */ {on_server_44_A6<S_OpenFile_PC_V3_44_A6>,        nullptr},
    /* A7 */ {on_server_13_A7,                                nullptr},
    /* A8 */ {nullptr,                                        nullptr},
    /* A9 */ {nullptr,                                        nullptr},
    /* AA */ {nullptr,                                        nullptr},
    /* AB */ {nullptr,                                        nullptr},
    /* AC */ {nullptr,                                        nullptr},
    /* AD */ {nullptr,                                        nullptr},
    /* AE */ {nullptr,                                        nullptr},
    /* AF */ {nullptr,                                        nullptr},
    /* B0 */ {nullptr,                                        nullptr},
    /* B1 */ {nullptr,                                        nullptr},
    /* B2 */ {on_server_B2,                                   nullptr},
    /* B3 */ {nullptr,                                        nullptr},
    /* B4 */ {nullptr,                                        nullptr},
    /* B5 */ {nullptr,                                        nullptr},
    /* B6 */ {nullptr,                                        nullptr},
    /* B7 */ {nullptr,                                        nullptr},
    /* B8 */ {on_server_gc_B8,                                nullptr},
    /* B9 */ {nullptr,                                        nullptr},
    /* BA */ {nullptr,                                        nullptr},
    /* BB */ {nullptr,                                        nullptr},
    /* BC */ {nullptr,                                        nullptr},
    /* BD */ {nullptr,                                        nullptr},
    /* BE */ {nullptr,                                        nullptr},
    /* BF */ {nullptr,                                        nullptr},
    // (GC)   SERVER                                          CLIENT
    /* C0 */ {nullptr,                                        nullptr},
    /* C1 */ {nullptr,                                        nullptr},
    /* C2 */ {nullptr,                                        nullptr},
    /* C3 */ {nullptr,                                        nullptr},
    /* C4 */ {on_server_C4<S_ChoiceSearchResultEntry_V3_C4>,  nullptr},
    /* C5 */ {nullptr,                                        nullptr},
    /* C6 */ {nullptr,                                        nullptr},
    /* C7 */ {nullptr,                                        nullptr},
    /* C8 */ {nullptr,                                        nullptr},
    /* C9 */ {on_server_60_62_6C_6D_C9_CB,                    nullptr},
    /* CA */ {nullptr,                                        nullptr},
    /* CB */ {on_server_60_62_6C_6D_C9_CB,                    nullptr},
    /* CC */ {nullptr,                                        nullptr},
    /* CD */ {nullptr,                                        nullptr},
    /* CE */ {nullptr,                                        nullptr},
    /* CF */ {nullptr,                                        nullptr},
    /* D0 */ {nullptr,                                        nullptr},
    /* D1 */ {nullptr,                                        nullptr},
    /* D2 */ {nullptr,                                        nullptr},
    /* D3 */ {nullptr,                                        nullptr},
    /* D4 */ {nullptr,                                        nullptr},
    /* D5 */ {on_server_v3_1A_D5,                             nullptr},
    /* D6 */ {nullptr,                                        nullptr},
    /* D7 */ {nullptr,                                        nullptr},
    /* D8 */ {nullptr,                                        nullptr},
    /* D9 */ {nullptr,                                        nullptr},
    /* DA */ {nullptr,                                        nullptr},
    /* DB */ {nullptr,                                        nullptr},
    /* DC */ {nullptr,                                        nullptr},
    /* DD */ {nullptr,                                        nullptr},
    /* DE */ {nullptr,                                        nullptr},
    /* DF */ {nullptr,                                        nullptr},
    // (GC)   SERVER                                          CLIENT
    /* E0 */ {nullptr,                                        nullptr},
    /* E1 */ {nullptr,                                        nullptr},
    /* E2 */ {nullptr,                                        nullptr},
    /* E3 */ {nullptr,                                        nullptr},
    /* E4 */ {on_server_gc_E4,                                nullptr},
    /* E5 */ {nullptr,                                        nullptr},
    /* E6 */ {nullptr,                                        nullptr},
    /* E7 */ {nullptr,                                        nullptr},
    /* E8 */ {nullptr,                                        nullptr},
    /* E9 */ {nullptr,                                        nullptr},
    /* EA */ {nullptr,                                        nullptr},
    /* EB */ {nullptr,                                        nullptr},
    /* EC */ {nullptr,                                        nullptr},
    /* ED */ {nullptr,                                        nullptr},
    /* EE */ {nullptr,                                        nullptr},
    /* EF */ {nullptr,                                        nullptr},
    /* F0 */ {nullptr,                                        nullptr},
    /* F1 */ {nullptr,                                        nullptr},
    /* F2 */ {nullptr,                                        nullptr},
    /* F3 */ {nullptr,                                        nullptr},
    /* F4 */ {nullptr,                                        nullptr},
    /* F5 */ {nullptr,                                        nullptr},
    /* F6 */ {nullptr,                                        nullptr},
    /* F7 */ {nullptr,                                        nullptr},
    /* F8 */ {nullptr,                                        nullptr},
    /* F9 */ {nullptr,                                        nullptr},
    /* FA */ {nullptr,                                        nullptr},
    /* FB */ {nullptr,                                        nullptr},
    /* FC */ {nullptr,                                        nullptr},
    /* FD */ {nullptr,                                        nullptr},
    /* FE */ {nullptr,                                        nullptr},
    /* FF */ {nullptr,                                        nullptr},
    // (GC)   SERVER                                          CLIENT
  }, {
    // (XB)   SERVER                                          CLIENT
    /* 00 */ {nullptr,                                        nullptr},
    /* 01 */ {nullptr,                                        nullptr},
    /* 02 */ {on_server_dc_pc_v3_patch_02_17,                 nullptr},
    /* 03 */ {nullptr,                                        nullptr},
    /* 04 */ {on_server_dc_pc_v3_04,                          nullptr},
    /* 05 */ {nullptr,                                        nullptr},
    /* 06 */ {on_server_dc_pc_v3_06,                          on_client_06},
    /* 07 */ {nullptr,                                        nullptr},
    /* 08 */ {nullptr,                                        nullptr},
    /* 09 */ {nullptr,                                        nullptr},
    /* 0A */ {nullptr,                                        nullptr},
    /* 0B */ {nullptr,                                        nullptr},
    /* 0C */ {nullptr,                                        nullptr},
    /* 0D */ {nullptr,                                        nullptr},
    /* 0E */ {nullptr,                                        nullptr},
    /* 0F */ {nullptr,                                        nullptr},
    /* 10 */ {nullptr,                                        nullptr},
    /* 11 */ {nullptr,                                        nullptr},
    /* 12 */ {nullptr,                                        nullptr},
    /* 13 */ {on_server_13_A7,                                nullptr},
    /* 14 */ {nullptr,                                        nullptr},
    /* 15 */ {nullptr,                                        nullptr},
    /* 16 */ {nullptr,                                        nullptr},
    /* 17 */ {on_server_dc_pc_v3_patch_02_17,                 nullptr},
    /* 18 */ {nullptr,                                        nullptr},
    /* 19 */ {on_server_game_19_patch_14,                     nullptr},
    /* 1A */ {on_server_v3_1A_D5,                             nullptr},
    /* 1B */ {nullptr,                                        nullptr},
    /* 1C */ {nullptr,                                        nullptr},
    /* 1D */ {nullptr,                                        nullptr},
    /* 1E */ {nullptr,                                        nullptr},
    /* 1F */ {nullptr,                                        nullptr},
    // (XB)   SERVER                                          CLIENT
    /* 20 */ {nullptr,                                        nullptr},
    /* 21 */ {nullptr,                                        nullptr},
    /* 22 */ {nullptr,                                        nullptr},
    /* 23 */ {nullptr,                                        nullptr},
    /* 24 */ {nullptr,                                        nullptr},
    /* 25 */ {nullptr,                                        nullptr},
    /* 26 */ {nullptr,                                        nullptr},
    /* 27 */ {nullptr,                                        nullptr},
    /* 28 */ {nullptr,                                        nullptr},
    /* 29 */ {nullptr,                                        nullptr},
    /* 2A */ {nullptr,                                        nullptr},
    /* 2B */ {nullptr,                                        nullptr},
    /* 2C */ {nullptr,                                        nullptr},
    /* 2D */ {nullptr,                                        nullptr},
    /* 2E */ {nullptr,                                        nullptr},
    /* 2F */ {nullptr,                                        nullptr},
    /* 30 */ {nullptr,                                        nullptr},
    /* 31 */ {nullptr,                                        nullptr},
    /* 32 */ {nullptr,                                        nullptr},
    /* 33 */ {nullptr,                                        nullptr},
    /* 34 */ {nullptr,                                        nullptr},
    /* 35 */ {nullptr,                                        nullptr},
    /* 36 */ {nullptr,                                        nullptr},
    /* 37 */ {nullptr,                                        nullptr},
    /* 38 */ {nullptr,                                        nullptr},
    /* 39 */ {nullptr,                                        nullptr},
    /* 3A */ {nullptr,                                        nullptr},
    /* 3B */ {nullptr,                                        nullptr},
    /* 3C */ {nullptr,                                        nullptr},
    /* 3D */ {nullptr,                                        nullptr},
    /* 3E */ {nullptr,                                        nullptr},
    /* 3F */ {nullptr,                                        nullptr},
    // (XB)   SERVER                                          CLIENT
    /* 40 */ {nullptr,                                        on_client_40},
    /* 41 */ {on_server_41<S_GuildCardSearchResult_DC_V3_41>, nullptr},
    /* 42 */ {nullptr,                                        nullptr},
    /* 43 */ {nullptr,                                        nullptr},
    /* 44 */ {on_server_44_A6<S_OpenFile_PC_V3_44_A6>,        nullptr},
    /* 45 */ {nullptr,                                        nullptr},
    /* 46 */ {nullptr,                                        nullptr},
    /* 47 */ {nullptr,                                        nullptr},
    /* 48 */ {nullptr,                                        nullptr},
    /* 49 */ {nullptr,                                        nullptr},
    /* 4A */ {nullptr,                                        nullptr},
    /* 4B */ {nullptr,                                        nullptr},
    /* 4C */ {nullptr,                                        nullptr},
    /* 4D */ {nullptr,                                        nullptr},
    /* 4E */ {nullptr,                                        nullptr},
    /* 4F */ {nullptr,                                        nullptr},
    /* 50 */ {nullptr,                                        nullptr},
    /* 51 */ {nullptr,                                        nullptr},
    /* 52 */ {nullptr,                                        nullptr},
    /* 53 */ {nullptr,                                        nullptr},
    /* 54 */ {nullptr,                                        nullptr},
    /* 55 */ {nullptr,                                        nullptr},
    /* 56 */ {nullptr,                                        nullptr},
    /* 57 */ {nullptr,                                        nullptr},
    /* 58 */ {nullptr,                                        nullptr},
    /* 59 */ {nullptr,                                        nullptr},
    /* 5A */ {nullptr,                                        nullptr},
    /* 5B */ {nullptr,                                        nullptr},
    /* 5C */ {nullptr,                                        nullptr},
    /* 5D */ {nullptr,                                        nullptr},
    /* 5E */ {nullptr,                                        nullptr},
    /* 5F */ {nullptr,                                        nullptr},
    // (XB)   SERVER                                          CLIENT
    /* 60 */ {on_server_60_62_6C_6D_C9_CB,                    on_client_60_62_6C_6D_C9_CB<G_SendGuildCard_V3_6x06>},
    /* 61 */ {nullptr,                                        nullptr},
    /* 62 */ {on_server_60_62_6C_6D_C9_CB,                    on_client_60_62_6C_6D_C9_CB<G_SendGuildCard_V3_6x06>},
    /* 63 */ {nullptr,                                        nullptr},
    /* 64 */ {on_server_64<S_JoinGame_XB_64>,                 nullptr},
    /* 65 */ {on_server_65_67_68<S_JoinLobby_XB_65_67_68>,    nullptr},
    /* 66 */ {on_server_66_69,                                nullptr},
    /* 67 */ {on_server_65_67_68<S_JoinLobby_XB_65_67_68>,    nullptr},
    /* 68 */ {on_server_65_67_68<S_JoinLobby_XB_65_67_68>,    nullptr},
    /* 69 */ {on_server_66_69,                                nullptr},
    /* 6A */ {nullptr,                                        nullptr},
    /* 6B */ {nullptr,                                        nullptr},
    /* 6C */ {on_server_60_62_6C_6D_C9_CB,                    on_client_60_62_6C_6D_C9_CB<G_SendGuildCard_V3_6x06>},
    /* 6D */ {on_server_60_62_6C_6D_C9_CB,                    on_client_60_62_6C_6D_C9_CB<G_SendGuildCard_V3_6x06>},
    /* 6E */ {nullptr,                                        nullptr},
    /* 6F */ {nullptr,                                        nullptr},
    /* 70 */ {nullptr,                                        nullptr},
    /* 71 */ {nullptr,                                        nullptr},
    /* 72 */ {nullptr,                                        nullptr},
    /* 73 */ {nullptr,                                        nullptr},
    /* 74 */ {nullptr,                                        nullptr},
    /* 75 */ {nullptr,                                        nullptr},
    /* 76 */ {nullptr,                                        nullptr},
    /* 77 */ {nullptr,                                        nullptr},
    /* 78 */ {nullptr,                                        nullptr},
    /* 79 */ {nullptr,                                        nullptr},
    /* 7A */ {nullptr,                                        nullptr},
    /* 7B */ {nullptr,                                        nullptr},
    /* 7C */ {nullptr,                                        nullptr},
    /* 7D */ {nullptr,                                        nullptr},
    /* 7E */ {nullptr,                                        nullptr},
    /* 7F */ {nullptr,                                        nullptr},
    // (XB)   SERVER                                          CLIENT
    /* 80 */ {nullptr,                                        nullptr},
    /* 81 */ {on_server_81<SC_SimpleMail_DC_V3_81>,           on_client_81<SC_SimpleMail_DC_V3_81>},
    /* 82 */ {nullptr,                                        nullptr},
    /* 83 */ {nullptr,                                        nullptr},
    /* 84 */ {nullptr,                                        nullptr},
    /* 85 */ {nullptr,                                        nullptr},
    /* 86 */ {nullptr,                                        nullptr},
    /* 87 */ {nullptr,                                        nullptr},
    /* 88 */ {on_server_88,                                   nullptr},
    /* 89 */ {nullptr,                                        nullptr},
    /* 8A */ {nullptr,                                        nullptr},
    /* 8B */ {nullptr,                                        nullptr},
    /* 8C */ {nullptr,                                        nullptr},
    /* 8D */ {nullptr,                                        nullptr},
    /* 8E */ {nullptr,                                        nullptr},
    /* 8F */ {nullptr,                                        nullptr},
    /* 90 */ {nullptr,                                        nullptr},
    /* 91 */ {nullptr,                                        nullptr},
    /* 92 */ {nullptr,                                        nullptr},
    /* 93 */ {nullptr,                                        nullptr},
    /* 94 */ {nullptr,                                        nullptr},
    /* 95 */ {nullptr,                                        nullptr},
    /* 96 */ {nullptr,                                        nullptr},
    /* 97 */ {on_server_97,                                   nullptr},
    /* 98 */ {nullptr,                                        on_client_98},
    /* 99 */ {nullptr,                                        nullptr},
    /* 9A */ {nullptr,                                        nullptr},
    /* 9B */ {nullptr,                                        nullptr},
    /* 9C */ {nullptr,                                        nullptr},
    /* 9D */ {nullptr,                                        nullptr},
    /* 9E */ {nullptr,                                        nullptr},
    /* 9F */ {nullptr,                                        nullptr},
    // (XB)   SERVER                                          CLIENT
    /* A0 */ {nullptr,                                        on_client_dc_pc_v3_A0_A1},
    /* A1 */ {nullptr,                                        on_client_dc_pc_v3_A0_A1},
    /* A2 */ {nullptr,                                        nullptr},
    /* A3 */ {nullptr,                                        nullptr},
    /* A4 */ {nullptr,                                        nullptr},
    /* A5 */ {nullptr,                                        nullptr},
    /* A6 */ {on_server_44_A6<S_OpenFile_PC_V3_44_A6>,        nullptr},
    /* A7 */ {on_server_13_A7,                                nullptr},
    /* A8 */ {nullptr,                                        nullptr},
    /* A9 */ {nullptr,                                        nullptr},
    /* AA */ {nullptr,                                        nullptr},
    /* AB */ {nullptr,                                        nullptr},
    /* AC */ {nullptr,                                        nullptr},
    /* AD */ {nullptr,                                        nullptr},
    /* AE */ {nullptr,                                        nullptr},
    /* AF */ {nullptr,                                        nullptr},
    /* B0 */ {nullptr,                                        nullptr},
    /* B1 */ {nullptr,                                        nullptr},
    /* B2 */ {on_server_B2,                                   nullptr},
    /* B3 */ {nullptr,                                        nullptr},
    /* B4 */ {nullptr,                                        nullptr},
    /* B5 */ {nullptr,                                        nullptr},
    /* B6 */ {nullptr,                                        nullptr},
    /* B7 */ {nullptr,                                        nullptr},
    /* B8 */ {nullptr,                                        nullptr},
    /* B9 */ {nullptr,                                        nullptr},
    /* BA */ {nullptr,                                        nullptr},
    /* BB */ {nullptr,                                        nullptr},
    /* BC */ {nullptr,                                        nullptr},
    /* BD */ {nullptr,                                        nullptr},
    /* BE */ {nullptr,                                        nullptr},
    /* BF */ {nullptr,                                        nullptr},
    // (XB)   SERVER                                          CLIENT
    /* C0 */ {nullptr,                                        nullptr},
    /* C1 */ {nullptr,                                        nullptr},
    /* C2 */ {nullptr,                                        nullptr},
    /* C3 */ {nullptr,                                        nullptr},
    /* C4 */ {on_server_C4<S_ChoiceSearchResultEntry_V3_C4>,  nullptr},
    /* C5 */ {nullptr,                                        nullptr},
    /* C6 */ {nullptr,                                        nullptr},
    /* C7 */ {nullptr,                                        nullptr},
    /* C8 */ {nullptr,                                        nullptr},
    /* C9 */ {on_server_60_62_6C_6D_C9_CB,                    nullptr},
    /* CA */ {nullptr,                                        nullptr},
    /* CB */ {on_server_60_62_6C_6D_C9_CB,                    nullptr},
    /* CC */ {nullptr,                                        nullptr},
    /* CD */ {nullptr,                                        nullptr},
    /* CE */ {nullptr,                                        nullptr},
    /* CF */ {nullptr,                                        nullptr},
    /* D0 */ {nullptr,                                        nullptr},
    /* D1 */ {nullptr,                                        nullptr},
    /* D2 */ {nullptr,                                        nullptr},
    /* D3 */ {nullptr,                                        nullptr},
    /* D4 */ {nullptr,                                        nullptr},
    /* D5 */ {on_server_v3_1A_D5,                             nullptr},
    /* D6 */ {nullptr,                                        nullptr},
    /* D7 */ {nullptr,                                        nullptr},
    /* D8 */ {nullptr,                                        nullptr},
    /* D9 */ {nullptr,                                        nullptr},
    /* DA */ {nullptr,                                        nullptr},
    /* DB */ {nullptr,                                        nullptr},
    /* DC */ {nullptr,                                        nullptr},
    /* DD */ {nullptr,                                        nullptr},
    /* DE */ {nullptr,                                        nullptr},
    /* DF */ {nullptr,                                        nullptr},
    // (XB)   SERVER                                          CLIENT
    /* E0 */ {nullptr,                                        nullptr},
    /* E1 */ {nullptr,                                        nullptr},
    /* E2 */ {nullptr,                                        nullptr},
    /* E3 */ {nullptr,                                        nullptr},
    /* E4 */ {nullptr,                                        nullptr},
    /* E5 */ {nullptr,                                        nullptr},
    /* E6 */ {nullptr,                                        nullptr},
    /* E7 */ {nullptr,                                        nullptr},
    /* E8 */ {nullptr,                                        nullptr},
    /* E9 */ {nullptr,                                        nullptr},
    /* EA */ {nullptr,                                        nullptr},
    /* EB */ {nullptr,                                        nullptr},
    /* EC */ {nullptr,                                        nullptr},
    /* ED */ {nullptr,                                        nullptr},
    /* EE */ {nullptr,                                        nullptr},
    /* EF */ {nullptr,                                        nullptr},
    /* F0 */ {nullptr,                                        nullptr},
    /* F1 */ {nullptr,                                        nullptr},
    /* F2 */ {nullptr,                                        nullptr},
    /* F3 */ {nullptr,                                        nullptr},
    /* F4 */ {nullptr,                                        nullptr},
    /* F5 */ {nullptr,                                        nullptr},
    /* F6 */ {nullptr,                                        nullptr},
    /* F7 */ {nullptr,                                        nullptr},
    /* F8 */ {nullptr,                                        nullptr},
    /* F9 */ {nullptr,                                        nullptr},
    /* FA */ {nullptr,                                        nullptr},
    /* FB */ {nullptr,                                        nullptr},
    /* FC */ {nullptr,                                        nullptr},
    /* FD */ {nullptr,                                        nullptr},
    /* FE */ {nullptr,                                        nullptr},
    /* FF */ {nullptr,                                        nullptr},
    // (XB)   SERVER                                          CLIENT
  }, {
    // (BB)   SERVER                                       CLIENT
    /* 00 */ {nullptr,                                     nullptr},
    /* 01 */ {nullptr,                                     nullptr},
    /* 02 */ {nullptr,                                     nullptr},
    /* 03 */ {on_server_bb_03,                             nullptr},
    /* 04 */ {nullptr,                                     nullptr},
    /* 05 */ {nullptr,                                     nullptr},
    /* 06 */ {nullptr,                                     on_client_06},
    /* 07 */ {nullptr,                                     nullptr},
    /* 08 */ {nullptr,                                     nullptr},
    /* 09 */ {nullptr,                                     nullptr},
    /* 0A */ {nullptr,                                     nullptr},
    /* 0B */ {nullptr,                                     nullptr},
    /* 0C */ {nullptr,                                     nullptr},
    /* 0D */ {nullptr,                                     nullptr},
    /* 0E */ {nullptr,                                     nullptr},
    /* 0F */ {nullptr,                                     nullptr},
    /* 10 */ {nullptr,                                     nullptr},
    /* 11 */ {nullptr,                                     nullptr},
    /* 12 */ {nullptr,                                     nullptr},
    /* 13 */ {on_server_13_A7,                             nullptr},
    /* 14 */ {nullptr,                                     nullptr},
    /* 15 */ {nullptr,                                     nullptr},
    /* 16 */ {nullptr,                                     nullptr},
    /* 17 */ {nullptr,                                     nullptr},
    /* 18 */ {nullptr,                                     nullptr},
    /* 19 */ {on_server_game_19_patch_14,                  nullptr},
    /* 1A */ {nullptr,                                     nullptr},
    /* 1B */ {nullptr,                                     nullptr},
    /* 1C */ {nullptr,                                     nullptr},
    /* 1D */ {nullptr,                                     nullptr},
    /* 1E */ {nullptr,                                     nullptr},
    /* 1F */ {nullptr,                                     nullptr},
    // (BB)   SERVER                                       CLIENT
    /* 20 */ {nullptr,                                     nullptr},
    /* 21 */ {nullptr,                                     nullptr},
    /* 22 */ {on_server_bb_22,                             nullptr},
    /* 23 */ {nullptr,                                     nullptr},
    /* 24 */ {nullptr,                                     nullptr},
    /* 25 */ {nullptr,                                     nullptr},
    /* 26 */ {nullptr,                                     nullptr},
    /* 27 */ {nullptr,                                     nullptr},
    /* 28 */ {nullptr,                                     nullptr},
    /* 29 */ {nullptr,                                     nullptr},
    /* 2A */ {nullptr,                                     nullptr},
    /* 2B */ {nullptr,                                     nullptr},
    /* 2C */ {nullptr,                                     nullptr},
    /* 2D */ {nullptr,                                     nullptr},
    /* 2E */ {nullptr,                                     nullptr},
    /* 2F */ {nullptr,                                     nullptr},
    /* 30 */ {nullptr,                                     nullptr},
    /* 31 */ {nullptr,                                     nullptr},
    /* 32 */ {nullptr,                                     nullptr},
    /* 33 */ {nullptr,                                     nullptr},
    /* 34 */ {nullptr,                                     nullptr},
    /* 35 */ {nullptr,                                     nullptr},
    /* 36 */ {nullptr,                                     nullptr},
    /* 37 */ {nullptr,                                     nullptr},
    /* 38 */ {nullptr,                                     nullptr},
    /* 39 */ {nullptr,                                     nullptr},
    /* 3A */ {nullptr,                                     nullptr},
    /* 3B */ {nullptr,                                     nullptr},
    /* 3C */ {nullptr,                                     nullptr},
    /* 3D */ {nullptr,                                     nullptr},
    /* 3E */ {nullptr,                                     nullptr},
    /* 3F */ {nullptr,                                     nullptr},
    // (BB)   SERVER                                       CLIENT
    /* 40 */ {nullptr,                                     on_client_40},
    /* 41 */ {on_server_41<S_GuildCardSearchResult_BB_41>, nullptr},
    /* 42 */ {nullptr,                                     nullptr},
    /* 43 */ {nullptr,                                     nullptr},
    /* 44 */ {on_server_44_A6<S_OpenFile_BB_44_A6>,        nullptr},
    /* 45 */ {nullptr,                                     nullptr},
    /* 46 */ {nullptr,                                     nullptr},
    /* 47 */ {nullptr,                                     nullptr},
    /* 48 */ {nullptr,                                     nullptr},
    /* 49 */ {nullptr,                                     nullptr},
    /* 4A */ {nullptr,                                     nullptr},
    /* 4B */ {nullptr,                                     nullptr},
    /* 4C */ {nullptr,                                     nullptr},
    /* 4D */ {nullptr,                                     nullptr},
    /* 4E */ {nullptr,                                     nullptr},
    /* 4F */ {nullptr,                                     nullptr},
    /* 50 */ {nullptr,                                     nullptr},
    /* 51 */ {nullptr,                                     nullptr},
    /* 52 */ {nullptr,                                     nullptr},
    /* 53 */ {nullptr,                                     nullptr},
    /* 54 */ {nullptr,                                     nullptr},
    /* 55 */ {nullptr,                                     nullptr},
    /* 56 */ {nullptr,                                     nullptr},
    /* 57 */ {nullptr,                                     nullptr},
    /* 58 */ {nullptr,                                     nullptr},
    /* 59 */ {nullptr,                                     nullptr},
    /* 5A */ {nullptr,                                     nullptr},
    /* 5B */ {nullptr,                                     nullptr},
    /* 5C */ {nullptr,                                     nullptr},
    /* 5D */ {nullptr,                                     nullptr},
    /* 5E */ {nullptr,                                     nullptr},
    /* 5F */ {nullptr,                                     nullptr},
    // (BB)   SERVER                                       CLIENT
    /* 60 */ {on_server_60_62_6C_6D_C9_CB,                 on_client_60_62_6C_6D_C9_CB<G_SendGuildCard_BB_6x06>},
    /* 61 */ {nullptr,                                     nullptr},
    /* 62 */ {on_server_60_62_6C_6D_C9_CB,                 on_client_60_62_6C_6D_C9_CB<G_SendGuildCard_BB_6x06>},
    /* 63 */ {nullptr,                                     nullptr},
    /* 64 */ {on_server_64<S_JoinGame_BB_64>,              nullptr},
    /* 65 */ {on_server_65_67_68<S_JoinLobby_BB_65_67_68>, nullptr},
    /* 66 */ {on_server_66_69,                             nullptr},
    /* 67 */ {on_server_65_67_68<S_JoinLobby_BB_65_67_68>, nullptr},
    /* 68 */ {on_server_65_67_68<S_JoinLobby_BB_65_67_68>, nullptr},
    /* 69 */ {on_server_66_69,                             nullptr},
    /* 6A */ {nullptr,                                     nullptr},
    /* 6B */ {nullptr,                                     nullptr},
    /* 6C */ {on_server_60_62_6C_6D_C9_CB,                 on_client_60_62_6C_6D_C9_CB<G_SendGuildCard_BB_6x06>},
    /* 6D */ {on_server_60_62_6C_6D_C9_CB,                 on_client_60_62_6C_6D_C9_CB<G_SendGuildCard_BB_6x06>},
    /* 6E */ {nullptr,                                     nullptr},
    /* 6F */ {nullptr,                                     nullptr},
    /* 70 */ {nullptr,                                     nullptr},
    /* 71 */ {nullptr,                                     nullptr},
    /* 72 */ {nullptr,                                     nullptr},
    /* 73 */ {nullptr,                                     nullptr},
    /* 74 */ {nullptr,                                     nullptr},
    /* 75 */ {nullptr,                                     nullptr},
    /* 76 */ {nullptr,                                     nullptr},
    /* 77 */ {nullptr,                                     nullptr},
    /* 78 */ {nullptr,                                     nullptr},
    /* 79 */ {nullptr,                                     nullptr},
    /* 7A */ {nullptr,                                     nullptr},
    /* 7B */ {nullptr,                                     nullptr},
    /* 7C */ {nullptr,                                     nullptr},
    /* 7D */ {nullptr,                                     nullptr},
    /* 7E */ {nullptr,                                     nullptr},
    /* 7F */ {nullptr,                                     nullptr},
    // (BB)   SERVER                                       CLIENT
    /* 80 */ {nullptr,                                     nullptr},
    /* 81 */ {nullptr,                                     nullptr},
    /* 82 */ {nullptr,                                     nullptr},
    /* 83 */ {nullptr,                                     nullptr},
    /* 84 */ {nullptr,                                     nullptr},
    /* 85 */ {nullptr,                                     nullptr},
    /* 86 */ {nullptr,                                     nullptr},
    /* 87 */ {nullptr,                                     nullptr},
    /* 88 */ {on_server_88,                                nullptr},
    /* 89 */ {nullptr,                                     nullptr},
    /* 8A */ {nullptr,                                     nullptr},
    /* 8B */ {nullptr,                                     nullptr},
    /* 8C */ {nullptr,                                     nullptr},
    /* 8D */ {nullptr,                                     nullptr},
    /* 8E */ {nullptr,                                     nullptr},
    /* 8F */ {nullptr,                                     nullptr},
    /* 90 */ {nullptr,                                     nullptr},
    /* 91 */ {nullptr,                                     nullptr},
    /* 92 */ {nullptr,                                     nullptr},
    /* 93 */ {nullptr,                                     nullptr},
    /* 94 */ {nullptr,                                     nullptr},
    /* 95 */ {nullptr,                                     nullptr},
    /* 96 */ {nullptr,                                     nullptr},
    /* 97 */ {nullptr,                                     nullptr},
    /* 98 */ {nullptr,                                     on_client_98},
    /* 99 */ {nullptr,                                     nullptr},
    /* 9A */ {nullptr,                                     nullptr},
    /* 9B */ {nullptr,                                     nullptr},
    /* 9C */ {nullptr,                                     nullptr},
    /* 9D */ {nullptr,                                     nullptr},
    /* 9E */ {nullptr,                                     nullptr},
    /* 9F */ {nullptr,                                     nullptr},
    // (BB)   SERVER                                       CLIENT
    /* A0 */ {nullptr,                                     nullptr},
    /* A1 */ {nullptr,                                     nullptr},
    /* A2 */ {nullptr,                                     nullptr},
    /* A3 */ {nullptr,                                     nullptr},
    /* A4 */ {nullptr,                                     nullptr},
    /* A5 */ {nullptr,                                     nullptr},
    /* A6 */ {on_server_44_A6<S_OpenFile_BB_44_A6>,        nullptr},
    /* A7 */ {on_server_13_A7,                             nullptr},
    /* A8 */ {nullptr,                                     nullptr},
    /* A9 */ {nullptr,                                     nullptr},
    /* AA */ {nullptr,                                     nullptr},
    /* AB */ {nullptr,                                     nullptr},
    /* AC */ {nullptr,                                     nullptr},
    /* AD */ {nullptr,                                     nullptr},
    /* AE */ {nullptr,                                     nullptr},
    /* AF */ {nullptr,                                     nullptr},
    /* B0 */ {nullptr,                                     nullptr},
    /* B1 */ {nullptr,                                     nullptr},
    /* B2 */ {on_server_B2,                                nullptr},
    /* B3 */ {nullptr,                                     nullptr},
    /* B4 */ {nullptr,                                     nullptr},
    /* B5 */ {nullptr,                                     nullptr},
    /* B6 */ {nullptr,                                     nullptr},
    /* B7 */ {nullptr,                                     nullptr},
    /* B8 */ {nullptr,                                     nullptr},
    /* B9 */ {nullptr,                                     nullptr},
    /* BA */ {nullptr,                                     nullptr},
    /* BB */ {nullptr,                                     nullptr},
    /* BC */ {nullptr,                                     nullptr},
    /* BD */ {nullptr,                                     nullptr},
    /* BE */ {nullptr,                                     nullptr},
    /* BF */ {nullptr,                                     nullptr},
    // (BB)   SERVER                                       CLIENT
    /* C0 */ {nullptr,                                     nullptr},
    /* C1 */ {nullptr,                                     nullptr},
    /* C2 */ {nullptr,                                     nullptr},
    /* C3 */ {nullptr,                                     nullptr},
    /* C4 */ {nullptr,                                     nullptr},
    /* C5 */ {nullptr,                                     nullptr},
    /* C6 */ {nullptr,                                     nullptr},
    /* C7 */ {nullptr,                                     nullptr},
    /* C8 */ {nullptr,                                     nullptr},
    /* C9 */ {nullptr,                                     nullptr},
    /* CA */ {nullptr,                                     nullptr},
    /* CB */ {nullptr,                                     nullptr},
    /* CC */ {nullptr,                                     nullptr},
    /* CD */ {nullptr,                                     nullptr},
    /* CE */ {nullptr,                                     nullptr},
    /* CF */ {nullptr,                                     nullptr},
    /* D0 */ {nullptr,                                     nullptr},
    /* D1 */ {nullptr,                                     nullptr},
    /* D2 */ {nullptr,                                     nullptr},
    /* D3 */ {nullptr,                                     nullptr},
    /* D4 */ {nullptr,                                     nullptr},
    /* D5 */ {nullptr,                                     nullptr},
    /* D6 */ {nullptr,                                     nullptr},
    /* D7 */ {nullptr,                                     nullptr},
    /* D8 */ {nullptr,                                     nullptr},
    /* D9 */ {nullptr,                                     nullptr},
    /* DA */ {nullptr,                                     nullptr},
    /* DB */ {nullptr,                                     nullptr},
    /* DC */ {nullptr,                                     nullptr},
    /* DD */ {nullptr,                                     nullptr},
    /* DE */ {nullptr,                                     nullptr},
    /* DF */ {nullptr,                                     nullptr},
    // (BB)   SERVER                                       CLIENT
    /* E0 */ {nullptr,                                     nullptr},
    /* E1 */ {nullptr,                                     nullptr},
    /* E2 */ {nullptr,                                     nullptr},
    /* E3 */ {nullptr,                                     nullptr},
    /* E4 */ {nullptr,                                     nullptr},
    /* E5 */ {nullptr,                                     nullptr},
    /* E6 */ {nullptr,                                     nullptr},
    /* E7 */ {on_server_E7,                                nullptr},
    /* E8 */ {nullptr,                                     nullptr},
    /* E9 */ {nullptr,                                     nullptr},
    /* EA */ {nullptr,                                     nullptr},
    /* EB */ {nullptr,                                     nullptr},
    /* EC */ {nullptr,                                     nullptr},
    /* ED */ {nullptr,                                     nullptr},
    /* EE */ {nullptr,                                     nullptr},
    /* EF */ {nullptr,                                     nullptr},
    /* F0 */ {nullptr,                                     nullptr},
    /* F1 */ {nullptr,                                     nullptr},
    /* F2 */ {nullptr,                                     nullptr},
    /* F3 */ {nullptr,                                     nullptr},
    /* F4 */ {nullptr,                                     nullptr},
    /* F5 */ {nullptr,                                     nullptr},
    /* F6 */ {nullptr,                                     nullptr},
    /* F7 */ {nullptr,                                     nullptr},
    /* F8 */ {nullptr,                                     nullptr},
    /* F9 */ {nullptr,                                     nullptr},
    /* FA */ {nullptr,                                     nullptr},
    /* FB */ {nullptr,                                     nullptr},
    /* FC */ {nullptr,                                     nullptr},
    /* FD */ {nullptr,                                     nullptr},
    /* FE */ {nullptr,                                     nullptr},
    /* FF */ {nullptr,                                     nullptr},
    // (BB)   SERVER                                       CLIENT
  },
};

static on_command_t get_handler(GameVersion version, bool from_server, uint8_t command) {
  size_t version_index = static_cast<size_t>(version);
  if (version_index >= sizeof(handlers) / sizeof(handlers[0])) {
    throw logic_error("invalid game version on proxy server");
  }
  auto ret = handlers[version_index][command][!from_server];
  return ret ? ret : on_default;
}

void on_proxy_command(
    shared_ptr<ServerState> s,
    ProxyServer::LinkedSession& session,
    bool from_server,
    uint16_t command,
    uint32_t flag,
    string& data) {
  try {
    auto fn = get_handler(session.version, from_server, command);
    auto res = fn(s, session, command, flag, data);
    if (res.type == HandlerResult::Type::FORWARD) {
      forward_command(session, !from_server, command, flag, data, false);
    } else if (res.type == HandlerResult::Type::MODIFIED) {
      session.log.info("The preceding command from the %s was modified in transit",
          from_server ? "server" : "client");
      forward_command(
          session,
          !from_server,
          res.new_command >= 0 ? res.new_command : command,
          res.new_flag >= 0 ? res.new_flag : flag,
          data);
    } else if (res.type == HandlerResult::Type::SUPPRESS) {
      session.log.info("The preceding command from the %s was not forwarded",
          from_server ? "server" : "client");
    } else {
      throw logic_error("invalid handler result");
    }
  } catch (const exception& e) {
    session.log.error("Failed to process command: %s", e.what());
    if (from_server) {
      string error_str = "Error: ";
      error_str += e.what();
      session.send_to_game_server(error_str.c_str());
    } else {
      session.disconnect();
    }
  }
}
