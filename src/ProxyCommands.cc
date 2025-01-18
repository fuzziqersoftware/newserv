#include "ProxyServer.hh"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
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
#include <resource_file/Emulators/SH4Emulator.hh>
#include <resource_file/Emulators/X86Emulator.hh>
#endif

#include "ChatCommands.hh"
#include "Compression.hh"
#include "GVMEncoder.hh"
#include "Loggers.hh"
#include "PSOProtocol.hh"
#include "ReceiveCommands.hh"
#include "ReceiveSubcommands.hh"
#include "SendCommands.hh"

using namespace std;

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

  HandlerResult(Type type)
      : type(type),
        new_command(-1),
        new_flag(-1) {}
  HandlerResult(Type type, uint16_t new_command, uint32_t new_flag)
      : type(type),
        new_command(new_command),
        new_flag(new_flag) {}
};

typedef HandlerResult (*on_command_t)(
    shared_ptr<ProxyServer::LinkedSession> ses,
    uint16_t command,
    uint32_t flag,
    string& data);

static void forward_command(shared_ptr<ProxyServer::LinkedSession> ses, bool to_server,
    uint16_t command, uint32_t flag, string& data, bool print_contents = true) {
  auto& ch = to_server ? ses->server_channel : ses->client_channel;
  if (!ch.connected()) {
    proxy_server_log.warning("No endpoint is present; dropping command");
  } else {
    ch.send(command, flag, data, !print_contents);
  }
}

// Command handlers. These are called to preprocess or react to specific
// commands in either direction. The functions have abbreviated names in order
// to make the massive table more readable. The functions' names are, in
// general, <SC>_[VERSIONS]_<COMMAND-NUMBERS>, where <SC> denotes who sent the
// command, VERSIONS denotes which versions this handler is for (with shortcuts
// - so v123 refers to all non-BB versions, for example, and DGX refers to all
// console versions), and COMMAND-NUMBERS are the hexadecimal value in the
// command header field that this handler is called for. If VERSIONS is omitted,
// the command handler is for all versions (for example, the 97 handler is like
// this).

static HandlerResult default_handler(shared_ptr<ProxyServer::LinkedSession>, uint16_t, uint32_t, string&) {
  return HandlerResult::Type::FORWARD;
}

static HandlerResult S_invalid(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t command, uint32_t flag, string&) {
  ses->log.error("Server sent invalid command");
  string error_str = is_v4(ses->version())
      ? phosg::string_printf("Server sent invalid\ncommand: %04hX %08" PRIX32, command, flag)
      : phosg::string_printf("Server sent invalid\ncommand: %02hX %02" PRIX32, command, flag);
  ses->send_to_game_server(error_str.c_str());
  return HandlerResult::Type::SUPPRESS;
}

static HandlerResult C_05(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string&) {
  ses->disconnect_action = is_v4(ses->version())
      ? ProxyServer::LinkedSession::DisconnectAction::MEDIUM_TIMEOUT
      : ProxyServer::LinkedSession::DisconnectAction::SHORT_TIMEOUT;
  return HandlerResult::Type::FORWARD;
}

static HandlerResult C_1D(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string&) {
  if (ses->client_ping_start_time) {
    uint64_t ping_usecs = phosg::now() - ses->client_ping_start_time;
    ses->client_ping_start_time = 0;
    double ping_ms = static_cast<double>(ping_usecs) / 1000.0;
    send_text_message_printf(ses->client_channel, "To proxy: %gms", ping_ms);
  }
  return ses->config.check_flag(Client::Flag::PROXY_SUPPRESS_CLIENT_PINGS)
      ? HandlerResult::Type::SUPPRESS
      : HandlerResult::Type::FORWARD;
}

static HandlerResult S_1D(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string&) {
  if (ses->config.check_flag(Client::Flag::PROXY_SUPPRESS_CLIENT_PINGS)) {
    ses->server_channel.send(0x1D);
    return HandlerResult::Type::SUPPRESS;
  } else {
    return HandlerResult::Type::FORWARD;
  }
}

static HandlerResult S_97(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string&) {
  // We always assume a 97 has already been received by the client - we should
  // have sent 97 01 before sending the client to the proxy server.
  ses->server_channel.send(0xB1, 0x00);
  return HandlerResult::Type::SUPPRESS;
}

static HandlerResult C_G_9E(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string&) {
  if (ses->config.check_flag(Client::Flag::PROXY_SUPPRESS_REMOTE_LOGIN) && ses->login) {
    le_uint64_t checksum = phosg::random_object<uint64_t>() & 0x0000FFFFFFFFFFFF;
    ses->server_channel.send(0x96, 0x00, &checksum, sizeof(checksum));

    S_UpdateClientConfig_V3_04 cmd;
    cmd.player_tag = 0x00010000;
    cmd.guild_card_number = ses->login->account->account_id;
    cmd.client_config.clear(0xFF);
    ses->client_channel.send(0x04, 0x00, &cmd, sizeof(cmd));

    return HandlerResult::Type::SUPPRESS;

  } else {
    return HandlerResult::Type::FORWARD;
  }
}

static HandlerResult S_G_9A(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string&) {
  if (ses->config.check_flag(Client::Flag::PROXY_SUPPRESS_REMOTE_LOGIN) || !ses->login || !ses->login->gc_license) {
    return HandlerResult::Type::FORWARD;
  }

  C_LoginExtended_GC_9E cmd;
  if (ses->remote_guild_card_number < 0) {
    cmd.player_tag = 0xFFFF0000;
    cmd.guild_card_number = 0xFFFFFFFF;
  } else {
    cmd.player_tag = 0x00010000;
    cmd.guild_card_number = ses->remote_guild_card_number;
  }
  cmd.hardware_id = ses->hardware_id;
  cmd.sub_version = ses->effective_sub_version();
  cmd.is_extended = (ses->remote_guild_card_number < 0) ? 1 : 0;
  cmd.language = ses->language();
  if (ses->login->gc_license) {
    cmd.serial_number.encode(phosg::string_printf("%08" PRIX32 "", ses->login->gc_license->serial_number));
  } else {
    cmd.serial_number.encode(phosg::string_printf("%08" PRIX32 "", ses->login->account->account_id));
  }
  cmd.access_key.encode(ses->login->gc_license->access_key);
  cmd.serial_number2 = cmd.serial_number;
  cmd.access_key2 = cmd.access_key;
  if (ses->config.check_flag(Client::Flag::PROXY_BLANK_NAME_ENABLED)) {
    cmd.name.encode(" ", ses->language());
  } else {
    cmd.name.encode(ses->character_name, ses->language());
  }
  cmd.client_config = ses->remote_client_config_data;

  // If there's a guild card number, a shorter 9E is sent that ends
  // right after the client config data

  ses->server_channel.send(
      0x9E, 0x01, &cmd,
      cmd.is_extended ? sizeof(C_LoginExtended_GC_9E) : sizeof(C_Login_GC_9E));
  return HandlerResult::Type::SUPPRESS;
}

static HandlerResult S_V123P_02_17(
    shared_ptr<ProxyServer::LinkedSession> ses,
    uint16_t command,
    uint32_t flag,
    string& data) {
  if (is_patch(ses->version()) && command == 0x17) {
    throw invalid_argument("patch server sent 17 server init");
  }

  // Most servers don't include after_message or have a shorter
  // after_message than newserv does, so don't require it
  const auto& cmd = check_size_t<S_ServerInitDefault_DC_PC_V3_02_17_91_9B>(data, 0xFFFF);

  if (!ses->login) {
    ses->log.info("Linked session is not logged in");

    // We have to forward the command BEFORE setting up encryption, so the
    // client will be able to understand what we sent.
    forward_command(ses, false, command, flag, data);

    if (uses_v3_encryption(ses->version())) {
      ses->server_channel.crypt_in = make_shared<PSOV3Encryption>(cmd.server_key);
      ses->server_channel.crypt_out = make_shared<PSOV3Encryption>(cmd.client_key);
      ses->client_channel.crypt_in = make_shared<PSOV3Encryption>(cmd.client_key);
      ses->client_channel.crypt_out = make_shared<PSOV3Encryption>(cmd.server_key);
    } else { // DC, PC, or patch server (they all use V2 encryption)
      ses->server_channel.crypt_in = make_shared<PSOV2Encryption>(cmd.server_key);
      ses->server_channel.crypt_out = make_shared<PSOV2Encryption>(cmd.client_key);
      ses->client_channel.crypt_in = make_shared<PSOV2Encryption>(cmd.client_key);
      ses->client_channel.crypt_out = make_shared<PSOV2Encryption>(cmd.server_key);
    }

    return HandlerResult::Type::SUPPRESS;
  }

  ses->log.info("Linked session is logged in");

  // This isn't forwarded to the client, so don't recreate the client's crypts
  if (uses_v3_encryption(ses->version())) {
    ses->server_channel.crypt_in = make_shared<PSOV3Encryption>(cmd.server_key);
    ses->server_channel.crypt_out = make_shared<PSOV3Encryption>(cmd.client_key);
  } else {
    ses->server_channel.crypt_in = make_shared<PSOV2Encryption>(cmd.server_key);
    ses->server_channel.crypt_out = make_shared<PSOV2Encryption>(cmd.client_key);
  }

  // Respond with an appropriate login command. We don't let the client do this
  // because it believes it already did (when it was in an unlinked session, or
  // in the patch server case, during the current session due to a hidden
  // redirect).
  switch (ses->version()) {
    case Version::PC_PATCH:
    case Version::BB_PATCH:
      ses->server_channel.send(0x02);
      return HandlerResult::Type::SUPPRESS;

    case Version::DC_NTE:
      // TODO
      throw runtime_error("DC NTE proxy is not implemented");

    case Version::DC_11_2000:
    case Version::DC_V1:
      if (!ses->login->dc_license) {
        throw runtime_error("DC license missing from login");
      }
      if (command == 0x17) {
        C_LoginV1_DC_PC_V3_90 cmd;
        cmd.serial_number.encode(phosg::string_printf("%08" PRIX32 "", ses->login->dc_license->serial_number));
        cmd.access_key.encode(ses->login->dc_license->access_key);
        cmd.access_key.clear_after_bytes(8);
        ses->server_channel.send(0x90, 0x00, &cmd, sizeof(cmd));
        return HandlerResult::Type::SUPPRESS;
      } else {
        C_LoginV1_DC_93 cmd;
        if (ses->remote_guild_card_number < 0) {
          cmd.player_tag = 0xFFFF0000;
          cmd.guild_card_number = 0xFFFFFFFF;
        } else {
          cmd.player_tag = 0x00010000;
          cmd.guild_card_number = ses->remote_guild_card_number;
        }
        cmd.hardware_id = ses->hardware_id;
        cmd.sub_version = ses->effective_sub_version();
        cmd.is_extended = 0;
        cmd.language = ses->language();
        cmd.serial_number.encode(phosg::string_printf("%08" PRIX32 "", ses->login->dc_license->serial_number));
        cmd.access_key.encode(ses->login->dc_license->access_key);
        cmd.access_key.clear_after_bytes(8);
        cmd.serial_number2.encode(ses->serial_number2);
        cmd.name.encode(ses->character_name);
        ses->server_channel.send(0x93, 0x00, &cmd, sizeof(cmd));
        return HandlerResult::Type::SUPPRESS;
      }
      break;

    case Version::DC_V2:
    case Version::PC_NTE:
    case Version::PC_V2:
    case Version::GC_NTE: {
      uint32_t serial_number;
      const string* access_key;
      if (ses->version() == Version::DC_V2) {
        if (!ses->login->dc_license) {
          throw runtime_error("incorrect login type");
        }
        serial_number = ses->login->dc_license->serial_number;
        access_key = &ses->login->dc_license->access_key;
      } else if (ses->version() != Version::GC_NTE) {
        if (!ses->login->pc_license) {
          throw runtime_error("incorrect login type");
        }
        serial_number = ses->login->pc_license->serial_number;
        access_key = &ses->login->pc_license->access_key;
      } else {
        if (!ses->login->gc_license) {
          throw runtime_error("incorrect login type");
        }
        serial_number = ses->login->gc_license->serial_number;
        access_key = &ses->login->gc_license->access_key;
      }

      if (command == 0x17) {
        C_Login_DC_PC_V3_9A cmd;
        if (ses->remote_guild_card_number < 0) {
          cmd.player_tag = 0xFFFF0000;
          cmd.guild_card_number = 0xFFFFFFFF;
        } else {
          cmd.player_tag = 0x00010000;
          cmd.guild_card_number = ses->remote_guild_card_number;
        }
        cmd.sub_version = ses->effective_sub_version();
        cmd.serial_number.encode(phosg::string_printf("%08" PRIX32 "", serial_number));
        cmd.access_key.encode(*access_key);
        if (ses->version() != Version::GC_NTE) {
          cmd.access_key.clear_after_bytes(8);
        }
        if (is_dc(ses->version())) {
          cmd.serial_number2.encode(ses->serial_number2);
        } else {
          cmd.serial_number2 = cmd.serial_number;
        }
        cmd.access_key2 = cmd.access_key;
        // TODO: We probably should set email_address, but we currently don't
        // keep that value anywhere in the session object, nor is it saved in
        // the Account object.
        ses->server_channel.send(0x9A, 0x00, &cmd, sizeof(cmd));
        return HandlerResult::Type::SUPPRESS;
      } else {
        C_Login_DC_PC_GC_9D cmd;
        if (ses->remote_guild_card_number < 0) {
          cmd.player_tag = 0xFFFF0000;
          cmd.guild_card_number = 0xFFFFFFFF;
        } else {
          cmd.player_tag = 0x00010000;
          cmd.guild_card_number = ses->remote_guild_card_number;
        }
        cmd.hardware_id = ses->hardware_id;
        cmd.sub_version = ses->effective_sub_version();
        cmd.is_extended = 0;
        cmd.language = ses->language();
        cmd.serial_number.encode(phosg::string_printf("%08" PRIX32 "", serial_number));
        cmd.access_key.encode(*access_key);
        if (ses->version() != Version::GC_NTE) {
          cmd.access_key.clear_after_bytes(8);
        }
        if (is_dc(ses->version())) {
          cmd.serial_number2.encode(ses->serial_number2);
        } else {
          cmd.serial_number2 = cmd.serial_number;
        }
        cmd.access_key2 = cmd.access_key;
        if (ses->config.check_flag(Client::Flag::PROXY_BLANK_NAME_ENABLED)) {
          cmd.name.encode(" ", ses->language());
        } else {
          cmd.name.encode(ses->character_name);
        }
        ses->server_channel.send(0x9D, 0x00, &cmd, sizeof(cmd));
        return HandlerResult::Type::SUPPRESS;
      }
      break;
    }

    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      if (!ses->login->gc_license) {
        throw runtime_error("GC license missing from login");
      }
      if (command == 0x17) {
        C_VerifyAccount_V3_DB cmd;
        cmd.serial_number.encode(phosg::string_printf("%08" PRIX32 "", ses->login->gc_license->serial_number));
        cmd.access_key.encode(ses->login->gc_license->access_key);
        cmd.sub_version = ses->effective_sub_version();
        cmd.serial_number2 = cmd.serial_number;
        cmd.access_key2 = cmd.access_key;
        cmd.password.encode(ses->login->gc_license->password);
        ses->server_channel.send(0xDB, 0x00, &cmd, sizeof(cmd));
        return HandlerResult::Type::SUPPRESS;

      } else if (ses->config.check_flag(Client::Flag::PROXY_SUPPRESS_REMOTE_LOGIN)) {
        uint32_t guild_card_number;
        if (ses->remote_guild_card_number >= 0) {
          guild_card_number = ses->remote_guild_card_number;
          ses->log.info("Using Guild Card number %" PRIu32 " from session", guild_card_number);
        } else {
          guild_card_number = phosg::random_object<uint32_t>();
          ses->log.info("Using Guild Card number %" PRIu32 " from random generator", guild_card_number);
        }

        uint32_t fake_serial_number = phosg::random_object<uint32_t>() & 0x7FFFFFFF;
        uint64_t fake_access_key = phosg::random_object<uint64_t>();
        string fake_access_key_str = phosg::string_printf("00000000000%" PRIu64, fake_access_key);
        if (fake_access_key_str.size() > 12) {
          fake_access_key_str = fake_access_key_str.substr(fake_access_key_str.size() - 12);
        }

        C_LoginExtended_GC_9E cmd;
        cmd.player_tag = 0x00010000;
        cmd.guild_card_number = guild_card_number;
        cmd.hardware_id = ses->hardware_id;
        cmd.sub_version = ses->effective_sub_version();
        cmd.is_extended = 0;
        cmd.language = ses->language();
        cmd.serial_number.encode(phosg::string_printf("%08" PRIX32, fake_serial_number));
        cmd.access_key.encode(fake_access_key_str);
        cmd.serial_number2 = cmd.serial_number;
        cmd.access_key2 = cmd.access_key;
        if (ses->config.check_flag(Client::Flag::PROXY_BLANK_NAME_ENABLED)) {
          cmd.name.encode(" ", ses->language());
        } else {
          cmd.name.encode(ses->character_name, ses->language());
        }
        cmd.client_config = ses->remote_client_config_data;
        ses->server_channel.send(0x9E, 0x01, &cmd, sizeof(C_Login_GC_9E));
        return HandlerResult::Type::SUPPRESS;

      } else {
        // For command 02, send the same as if we had received 9A from the server
        return S_G_9A(ses, command, flag, data);
      }
      throw logic_error("GC init command not handled");

    case Version::XB_V3: {
      if (!ses->login->xb_license) {
        throw runtime_error("XB license missing from login");
      }
      C_LoginExtended_XB_9E cmd;
      if (ses->remote_guild_card_number < 0) {
        cmd.player_tag = 0xFFFF0000;
        cmd.guild_card_number = 0xFFFFFFFF;
      } else {
        cmd.player_tag = 0x00010000;
        cmd.guild_card_number = ses->remote_guild_card_number;
      }
      cmd.hardware_id = ses->hardware_id;
      cmd.sub_version = ses->effective_sub_version();
      cmd.is_extended = (ses->remote_guild_card_number < 0) ? 1 : 0;
      cmd.language = ses->language();
      cmd.serial_number.encode(ses->login->xb_license->gamertag);
      cmd.access_key.encode(phosg::string_printf("%016" PRIX64, ses->login->xb_license->user_id));
      cmd.serial_number2 = cmd.serial_number;
      cmd.access_key2 = cmd.access_key;
      if (ses->config.check_flag(Client::Flag::PROXY_BLANK_NAME_ENABLED)) {
        cmd.name.encode(" ", ses->language());
      } else {
        cmd.name.encode(ses->character_name, ses->language());
      }
      cmd.netloc = ses->xb_netloc;
      cmd.unknown_a1a = ses->xb_9E_unknown_a1a;
      cmd.xb_user_id_high = (ses->login->xb_license->user_id >> 32) & 0xFFFFFFFF;
      cmd.xb_user_id_low = ses->login->xb_license->user_id & 0xFFFFFFFF;
      ses->server_channel.send(0x9E, 0x01, &cmd, sizeof(C_LoginExtended_XB_9E));
      return HandlerResult::Type::SUPPRESS;
    }

    case Version::BB_V4:
      throw logic_error("v1/v2/v3 server init handler should not be called on BB");
    default:
      throw logic_error("invalid game version in server init handler");
  }
}

static HandlerResult S_B_03(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string& data) {
  // Most servers don't include after_message or have a shorter after_message
  // than newserv does, so don't require it
  const auto& cmd = check_size_t<S_ServerInitDefault_BB_03_9B>(data, 0xFFFF);

  // If the session has a detector crypt, then it was resumed from an unlinked
  // session, during which we already sent an 03 command.
  if (ses->detector_crypt.get()) {
    if (ses->login_command_bb.empty()) {
      throw logic_error("linked BB session does not have a saved login command");
    }

    // This isn't forwarded to the client, so only recreate the server's crypts.
    // Use the same crypt type as the client... the server has the luxury of
    // being able to try all the crypts it knows to detect what type the client
    // uses, but the client can't do this since it sends the first encrypted
    // data on the connection.
    ses->server_channel.crypt_in = make_shared<PSOBBMultiKeyImitatorEncryption>(
        ses->detector_crypt, cmd.server_key.data(), sizeof(cmd.server_key), false);
    ses->server_channel.crypt_out = make_shared<PSOBBMultiKeyImitatorEncryption>(
        ses->detector_crypt, cmd.client_key.data(), sizeof(cmd.client_key), false);

    // Forward the login command we saved during the unlinked ses->
    if (ses->enable_remote_ip_crc_patch && (ses->login_command_bb.size() >= 0x98)) {
      *reinterpret_cast<le_uint32_t*>(ses->login_command_bb.data() + 0x94) =
          ses->remote_ip_crc ^ (1309539928UL + 1248334810UL);
    }
    ses->server_channel.send(0x93, 0x00, ses->login_command_bb);

    return HandlerResult::Type::SUPPRESS;

    // If there's no detector crypt, then the session is new and was linked
    // immediately at connect time, and an 03 was not yet sent to the client, so
    // we should forward this one.
  } else {
    // Forward the command to the client before setting up the crypts, so the
    // client receives the unencrypted data
    ses->client_channel.send(0x03, 0x00, data);

    ses->detector_crypt = make_shared<PSOBBMultiKeyDetectorEncryption>(
        ses->require_server_state()->bb_private_keys,
        bb_crypt_initial_client_commands,
        cmd.client_key.data(),
        sizeof(cmd.client_key));
    ses->client_channel.crypt_in = ses->detector_crypt;
    ses->client_channel.crypt_out = make_shared<PSOBBMultiKeyImitatorEncryption>(
        ses->detector_crypt, cmd.server_key.data(), sizeof(cmd.server_key), true);
    ses->server_channel.crypt_in = make_shared<PSOBBMultiKeyImitatorEncryption>(
        ses->detector_crypt, cmd.server_key.data(), sizeof(cmd.server_key), false);
    ses->server_channel.crypt_out = make_shared<PSOBBMultiKeyImitatorEncryption>(
        ses->detector_crypt, cmd.client_key.data(), sizeof(cmd.client_key), false);

    // We already forwarded the command, so don't do so again
    return HandlerResult::Type::SUPPRESS;
  }
}

static HandlerResult S_V123_04(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string& data) {
  // Suppress extremely short commands from the server instead of disconnecting.
  if (data.size() < offsetof(S_UpdateClientConfig_V3_04, client_config)) {
    le_uint64_t checksum = phosg::random_object<uint64_t>() & 0x0000FFFFFFFFFFFF;
    ses->server_channel.send(0x96, 0x00, &checksum, sizeof(checksum));
    return HandlerResult::Type::SUPPRESS;
  }

  // Some servers send a short 04 command if they don't use all of the 0x20
  // bytes available. We should be prepared to handle that.
  auto& cmd = check_size_t<S_UpdateClientConfig_V3_04>(data,
      offsetof(S_UpdateClientConfig_V3_04, client_config),
      sizeof(S_UpdateClientConfig_V3_04));

  // If this is a logged-in session, hide the guild card number assigned by the
  // remote server so the client doesn't see it change. If this is a logged-out
  // session, then the client never received a guild card number from newserv
  // anyway, so we can let the client see the number from the remote server.
  bool had_guild_card_number = (ses->remote_guild_card_number >= 0);
  if (ses->remote_guild_card_number != cmd.guild_card_number) {
    ses->remote_guild_card_number = cmd.guild_card_number;
    ses->log.info("Remote guild card number set to %" PRId64,
        ses->remote_guild_card_number);
    string message = phosg::string_printf(
        "The remote server\nhas assigned your\nGuild Card number:\n$C6%" PRId64,
        ses->remote_guild_card_number);
    send_ship_info(ses->client_channel, message);
  }
  if (ses->login) {
    cmd.guild_card_number = ses->login->account->account_id;
  }

  // It seems the client ignores the length of the 04 command, and always copies
  // 0x20 bytes to its config data. So if the server sends a short 04 command,
  // part of the previous command ends up in the security data (usually part of
  // the copyright string from the server init command). We simulate that here.
  // If there was previously a guild card number, assume we got the lobby server
  // init text instead of the port map init text.
  memcpy(ses->remote_client_config_data.data(),
      had_guild_card_number
          ? "t Lobby Server. Copyright SEGA E"
          : "t Port Map. Copyright SEGA Enter",
      ses->remote_client_config_data.bytes());
  memcpy(ses->remote_client_config_data.data(), &cmd.client_config,
      min<size_t>(data.size() - offsetof(S_UpdateClientConfig_V3_04, client_config),
          ses->remote_client_config_data.bytes()));

  // If the guild card number was not set, pretend (to the server) that this is
  // the first 04 command the client has received. The client responds with a 96
  // (checksum) in that case.
  if (!had_guild_card_number) {
    le_uint64_t checksum = phosg::random_object<uint64_t>() & 0x0000FFFFFFFFFFFF;
    ses->server_channel.send(0x96, 0x00, &checksum, sizeof(checksum));
  }

  return ses->login ? HandlerResult::Type::MODIFIED : HandlerResult::Type::FORWARD;
}

static HandlerResult S_V123_06(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string& data) {
  bool modified = false;
  if (ses->login) {
    auto& cmd = check_size_t<SC_TextHeader_01_06_11_B0_EE>(data, 0xFFFF);
    if (cmd.guild_card_number == ses->remote_guild_card_number) {
      cmd.guild_card_number = ses->login->account->account_id;
      modified = true;
    }
  }

  // If the session is Ep3, and Unmask Whispers is on, and there's enough data,
  // and the message has private_flags, and the private_flags say that you
  // shouldn't see the message, then change the private_flags
  if (is_ep3(ses->version()) &&
      ses->config.check_flag(Client::Flag::PROXY_EP3_UNMASK_WHISPERS) &&
      (data.size() >= 12) &&
      (data[sizeof(SC_TextHeader_01_06_11_B0_EE)] != '\t') &&
      (data[sizeof(SC_TextHeader_01_06_11_B0_EE)] & (1 << ses->lobby_client_id))) {
    data[sizeof(SC_TextHeader_01_06_11_B0_EE)] &= ~(1 << ses->lobby_client_id);
    modified = true;
  }

  return modified ? HandlerResult::Type::MODIFIED : HandlerResult::Type::FORWARD;
}

template <typename CmdT>
static HandlerResult S_41(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string& data) {
  if (ses->login) {
    auto& cmd = check_size_t<CmdT>(data);
    if ((cmd.searcher_guild_card_number == ses->remote_guild_card_number) &&
        (cmd.result_guild_card_number == ses->remote_guild_card_number) &&
        ses->server_ping_start_time) {
      uint64_t ping_usecs = phosg::now() - ses->server_ping_start_time;
      ses->server_ping_start_time = 0;
      double ping_ms = static_cast<double>(ping_usecs) / 1000.0;
      send_text_message_printf(ses->client_channel, "To server: %gms", ping_ms);
      return HandlerResult::Type::SUPPRESS;

    } else {
      bool modified = false;
      if (cmd.searcher_guild_card_number == ses->remote_guild_card_number) {
        cmd.searcher_guild_card_number = ses->login->account->account_id;
        modified = true;
      }
      if (cmd.result_guild_card_number == ses->remote_guild_card_number) {
        cmd.result_guild_card_number = ses->login->account->account_id;
        modified = true;
      }
      return modified ? HandlerResult::Type::MODIFIED : HandlerResult::Type::FORWARD;
    }
  } else {
    return HandlerResult::Type::FORWARD;
  }
}

constexpr on_command_t S_DGX_41 = &S_41<S_GuildCardSearchResult_DC_V3_41>;
constexpr on_command_t S_P_41 = &S_41<S_GuildCardSearchResult_PC_41>;
constexpr on_command_t S_B_41 = &S_41<S_GuildCardSearchResult_BB_41>;

template <typename CmdT>
static HandlerResult S_81(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string& data) {
  bool modified = false;
  if (ses->login) {
    auto& cmd = check_size_t<CmdT>(data);
    if (cmd.from_guild_card_number == ses->remote_guild_card_number) {
      cmd.from_guild_card_number = ses->login->account->account_id;
      modified = true;
    }
    if (cmd.to_guild_card_number == ses->remote_guild_card_number) {
      cmd.to_guild_card_number = ses->login->account->account_id;
      modified = true;
    }
  }
  return modified ? HandlerResult::Type::MODIFIED : HandlerResult::Type::FORWARD;
}

constexpr on_command_t S_DGX_81 = &S_81<SC_SimpleMail_DC_V3_81>;
constexpr on_command_t S_P_81 = &S_81<SC_SimpleMail_PC_81>;
constexpr on_command_t S_B_81 = &S_81<SC_SimpleMail_BB_81>;

static HandlerResult S_88(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t flag, string& data) {
  bool modified = false;
  if (ses->login) {
    size_t expected_size = sizeof(S_ArrowUpdateEntry_88) * flag;
    auto* entries = &check_size_t<S_ArrowUpdateEntry_88>(
        data, expected_size, expected_size);
    for (size_t x = 0; x < flag; x++) {
      if (entries[x].guild_card_number == ses->remote_guild_card_number) {
        entries[x].guild_card_number = ses->login->account->account_id;
        modified = true;
      }
    }
  }
  return modified ? HandlerResult::Type::MODIFIED : HandlerResult::Type::FORWARD;
}

static HandlerResult S_B1(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string&) {
  // Block all time updates from the remote server, so client's time remains
  // consistent
  ses->server_channel.send(0x99, 0x00);
  return HandlerResult::Type::SUPPRESS;
}

template <bool BE>
static HandlerResult S_B2(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t flag, string& data) {
  const auto& cmd = check_size_t<S_ExecuteCode_B2>(data, 0xFFFF);

  if (cmd.code_size && ses->config.check_flag(Client::Flag::PROXY_SAVE_FILES)) {
    uint64_t filename_timestamp = phosg::now();
    string code = data.substr(sizeof(S_ExecuteCode_B2));

    if (ses->config.check_flag(Client::Flag::ENCRYPTED_SEND_FUNCTION_CALL)) {
      phosg::StringReader r(code);
      bool is_big_endian = ::is_big_endian(ses->version());
      uint32_t decompressed_size = is_big_endian ? r.get_u32b() : r.get_u32l();
      uint32_t key = is_big_endian ? r.get_u32b() : r.get_u32l();

      PSOV2Encryption crypt(key);
      string decrypted_data;
      if (is_big_endian) {
        phosg::StringWriter w;
        while (!r.eof()) {
          w.put_u32b(r.get_u32b() ^ crypt.next());
        }
        decrypted_data = std::move(w.str());
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

    string output_filename = phosg::string_printf("code.%" PRId64 ".bin", filename_timestamp);
    phosg::save_file(output_filename, data);
    ses->log.info("Wrote code from server to file %s", output_filename.c_str());

#ifdef HAVE_RESOURCE_FILE
    using FooterT = RELFileFooterT<BE>;

    // TODO: Support SH-4 disassembly too
    bool is_ppc = ::is_ppc(ses->version());
    bool is_x86 = ::is_x86(ses->version());
    bool is_sh4 = ::is_sh4(ses->version());
    if (is_ppc || is_x86 || is_sh4) {
      try {
        if (code.size() < sizeof(FooterT)) {
          throw runtime_error("code section is too small");
        }

        size_t footer_offset = code.size() - sizeof(FooterT);

        phosg::StringReader r(code.data(), code.size());
        const auto& footer = r.pget<FooterT>(footer_offset);

        multimap<uint32_t, string> labels;
        r.go(footer.relocations_offset);
        uint32_t reloc_offset = 0;
        for (size_t x = 0; x < footer.num_relocations; x++) {
          reloc_offset += (r.get<U16T<BE>>() * 4);
          labels.emplace(reloc_offset, phosg::string_printf("reloc%zu", x));
        }
        labels.emplace(footer.root_offset.load(), "entry_ptr");
        labels.emplace(footer_offset, "footer");
        labels.emplace(r.pget<U32T<BE>>(footer.root_offset), "start");

        string disassembly;
        if (is_ppc) {
          disassembly = ResourceDASM::PPC32Emulator::disassemble(
              &r.pget<uint8_t>(0, code.size()),
              code.size(),
              0,
              &labels);
        } else if (is_x86) {
          disassembly = ResourceDASM::X86Emulator::disassemble(
              &r.pget<uint8_t>(0, code.size()),
              code.size(),
              0,
              &labels);
        } else if (is_sh4) {
          disassembly = ResourceDASM::SH4Emulator::disassemble(
              &r.pget<uint8_t>(0, code.size()),
              code.size(),
              0,
              &labels);
        } else {
          // We shouldn't have entered the outer if statement if this happens
          throw logic_error("unsupported architecture");
        }

        output_filename = phosg::string_printf("code.%" PRId64 ".txt", filename_timestamp);
        {
          auto f = phosg::fopen_unique(output_filename, "wt");
          fprintf(f.get(), "// code_size = 0x%" PRIX32 "\n", cmd.code_size.load());
          fprintf(f.get(), "// checksum_addr = 0x%" PRIX32 "\n", cmd.checksum_start.load());
          fprintf(f.get(), "// checksum_size = 0x%" PRIX32 "\n", cmd.checksum_size.load());
          phosg::fwritex(f.get(), disassembly);
        }
        ses->log.info("Wrote disassembly to file %s", output_filename.c_str());

      } catch (const exception& e) {
        ses->log.info("Failed to disassemble code from server: %s", e.what());
      }
    }
#endif
  }

  if (ses->config.check_flag(Client::Flag::PROXY_BLOCK_FUNCTION_CALLS)) {
    ses->log.info("Blocking function call from server");
    C_ExecuteCodeResult_B3 cmd;
    cmd.return_value = 0xFFFFFFFF;
    cmd.checksum = 0x00000000;
    ses->server_channel.send(0xB3, flag, &cmd, sizeof(cmd));
    return HandlerResult::Type::SUPPRESS;
  } else {
    ses->function_call_return_handler_queue.emplace_back(nullptr);
    return HandlerResult::Type::FORWARD;
  }
}

static HandlerResult C_B3(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string& data) {
  auto cmd = check_size_t<C_ExecuteCodeResult_B3>(data);
  if (ses->function_call_return_handler_queue.empty()) {
    ses->log.warning("Received function call result with empty result queue");
    return HandlerResult::Type::FORWARD;
  }

  auto handler = std::move(ses->function_call_return_handler_queue.front());
  ses->function_call_return_handler_queue.pop_front();
  if (handler != nullptr) {
    handler(cmd.return_value, cmd.checksum);
    return HandlerResult::Type::SUPPRESS;
  } else {
    return HandlerResult::Type::FORWARD;
  }
}

static HandlerResult S_B_E7(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string& data) {
  if (ses->config.check_flag(Client::Flag::PROXY_SAVE_FILES)) {
    string output_filename = phosg::string_printf("player.%" PRId64 ".bin", phosg::now());
    phosg::save_file(output_filename, data);
    ses->log.info("Wrote player data to file %s", output_filename.c_str());
  }
  return HandlerResult::Type::FORWARD;
}

template <typename CmdT>
static HandlerResult S_C4(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t flag, string& data) {
  bool modified = false;
  if (ses->login) {
    size_t expected_size = sizeof(CmdT) * flag;
    // Some servers (e.g. Schtserv) send extra data on the end of this command;
    // the client ignores it so we can ignore it too
    auto* entries = &check_size_t<CmdT>(data, expected_size, 0xFFFF);
    for (size_t x = 0; x < flag; x++) {
      if (entries[x].guild_card_number == ses->remote_guild_card_number) {
        entries[x].guild_card_number = ses->login->account->account_id;
        modified = true;
      }
    }
  }
  return modified ? HandlerResult::Type::MODIFIED : HandlerResult::Type::FORWARD;
}

constexpr on_command_t S_DGX_C4 = &S_C4<S_ChoiceSearchResultEntry_DC_V3_C4>;
constexpr on_command_t S_P_C4 = &S_C4<S_ChoiceSearchResultEntry_PC_C4>;
constexpr on_command_t S_B_C4 = &S_C4<S_ChoiceSearchResultEntry_BB_C4>;

static HandlerResult S_G_E4(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string& data) {
  auto& cmd = check_size_t<S_CardBattleTableState_Ep3_E4>(data);
  bool modified = false;
  for (size_t x = 0; x < 4; x++) {
    if (cmd.entries[x].guild_card_number == ses->remote_guild_card_number) {
      cmd.entries[x].guild_card_number = ses->login->account->account_id;
      modified = true;
    }
  }
  return modified ? HandlerResult::Type::MODIFIED : HandlerResult::Type::FORWARD;
}

static HandlerResult S_B_22(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string& data) {
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
      (phosg::fnv1a64(data.data(), data.size()) == 0x8AF8314316A27994)) {
    ses->log.info("Enabling remote IP CRC patch");
    ses->enable_remote_ip_crc_patch = true;
  }
  return HandlerResult::Type::FORWARD;
}

static HandlerResult S_19_P_14(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string& data) {
  // If the command is shorter than 6 bytes, use the previous server command to
  // fill it in. This simulates a behavior used by some private servers where a
  // longer previous command is used to fill part of the client's receive buffer
  // with meaningful data, then an intentionally undersize 19 command is sent
  // which results in the client using the previous command's data as part of
  // the 19 command's contents. They presumably do this in an attempt to prevent
  // people from using proxies.
  if (data.size() < sizeof(ses->prev_server_command_bytes)) {
    data.append(
        reinterpret_cast<const char*>(&ses->prev_server_command_bytes[data.size()]),
        sizeof(ses->prev_server_command_bytes) - data.size());
  }
  if (data.size() < sizeof(S_Reconnect_19)) {
    data.resize(sizeof(S_Reconnect_19), '\0');
  }

  if (ses->enable_remote_ip_crc_patch) {
    ses->remote_ip_crc = phosg::crc32(data.data(), 4);
  }

  // Set the destination netloc appropriately
  memset(&ses->next_destination, 0, sizeof(ses->next_destination));
  struct sockaddr_in* sin = reinterpret_cast<struct sockaddr_in*>(
      &ses->next_destination);
  sin->sin_family = AF_INET;
  if (is_patch(ses->version())) {
    auto& cmd = check_size_t<S_Reconnect_Patch_14>(data);
    sin->sin_addr.s_addr = cmd.address.load_raw(); // Already big-endian
    sin->sin_port = htons(cmd.port);
  } else {
    // This weird maximum size is here to properly handle the version-split
    // command that some servers (including newserv) use on port 9100
    auto& cmd = check_size_t<S_Reconnect_19>(data, 0xFFFF);
    sin->sin_addr.s_addr = cmd.address.load_raw(); // Already big-endian
    sin->sin_port = htons(cmd.port);
  }

  if (!ses->client_channel.connected()) {
    ses->log.warning("Received reconnect command with no destination present");
    return HandlerResult::Type::SUPPRESS;

  } else if (!is_v4(ses->version())) {
    // Hide redirects from the client completely. The new destination server
    // will presumably send a new encryption init command, which the handlers
    // will appropriately respond to.
    ses->server_channel.crypt_in.reset();
    ses->server_channel.crypt_out.reset();

    // We already modified next_destination, so start the connection process
    ses->connect();
    return HandlerResult::Type::SUPPRESS;

  } else {
    const struct sockaddr_in* sin = reinterpret_cast<const struct sockaddr_in*>(
        &ses->client_channel.local_addr);
    if (sin->sin_family != AF_INET) {
      throw logic_error("existing connection is not ipv4");
    }
    auto& cmd = check_size_t<S_Reconnect_19>(data, 0xFFFF);
    cmd.address.store_raw(sin->sin_addr.s_addr);
    cmd.port = ntohs(sin->sin_port);
    return HandlerResult::Type::MODIFIED;
  }
}

static HandlerResult S_V3_1A_D5(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string&) {
  // If the client is a version that sends close confirmations and the client
  // has the no-close-confirmation flag set in its newserv client config, send a
  // fake confirmation to the remote server immediately.
  if (is_v3(ses->version()) && ses->config.check_flag(Client::Flag::NO_D6)) {
    ses->server_channel.send(0xD6);
  }
  return HandlerResult::Type::FORWARD;
}

static HandlerResult S_V3_BB_DA(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t flag, string&) {
  // This command is supported on all V3 and V4 versions except Ep1&2 Trial
  if (ses->version() == Version::GC_NTE) {
    return HandlerResult::Type::SUPPRESS;
  } else if ((ses->config.override_lobby_event != 0xFF) && (flag != ses->config.override_lobby_event)) {
    return HandlerResult(HandlerResult::Type::MODIFIED, 0xDA, ses->config.override_lobby_event);
  } else {
    return HandlerResult::Type::FORWARD;
  }
}

static HandlerResult SC_6x60_6xA2(shared_ptr<ProxyServer::LinkedSession> ses, const string& data) {
  if (!ses->is_in_game) {
    return HandlerResult::Type::FORWARD;
  }

  if (ses->next_drop_item.data1d[0]) {
    G_SpecializableItemDropRequest_6xA2 cmd = normalize_drop_request(data.data(), data.size());
    auto s = ses->require_server_state();
    ses->next_drop_item.id = ses->next_item_id++;
    uint8_t source_type = (cmd.rt_index == 0x30) ? 2 : 1;
    send_drop_item_to_channel(s, ses->server_channel, ses->next_drop_item, source_type, cmd.floor, cmd.pos, cmd.entity_index);
    send_drop_item_to_channel(s, ses->client_channel, ses->next_drop_item, source_type, cmd.floor, cmd.pos, cmd.entity_index);
    ses->next_drop_item.clear();
    return HandlerResult::Type::SUPPRESS;
  }

  using DropMode = ProxyServer::LinkedSession::DropMode;
  switch (ses->drop_mode) {
    case DropMode::DISABLED:
      return HandlerResult::Type::SUPPRESS;
    case DropMode::PASSTHROUGH:
      return HandlerResult::Type::FORWARD;
    case DropMode::INTERCEPT:
      break;
    default:
      throw logic_error("invalid drop mode");
  }

  if (!ses->item_creator) {
    ses->log.warning("Session is in INTERCEPT drop mode, but item creator is missing");
    return HandlerResult::Type::FORWARD;
  }
  if (!ses->map_state) {
    ses->log.warning("Session is in INTERCEPT drop mode, but map state is missing");
    return HandlerResult::Type::FORWARD;
  }

  G_SpecializableItemDropRequest_6xA2 cmd = normalize_drop_request(data.data(), data.size());
  auto rec = reconcile_drop_request_with_map(
      ses->log,
      ses->client_channel,
      cmd,
      ses->lobby_episode,
      ses->lobby_event,
      ses->config,
      ses->map_state,
      false);

  ItemCreator::DropResult res;
  if (rec.obj_st) {
    if (rec.ignore_def) {
      ses->log.info("Creating item from box %04hX (area %02hX)", cmd.entity_index.load(), cmd.effective_area);
      res = ses->item_creator->on_box_item_drop(cmd.effective_area);
    } else {
      ses->log.info("Creating item from box %04hX (area %02hX; specialized with %g %08" PRIX32 " %08" PRIX32 " %08" PRIX32 ")",
          cmd.entity_index.load(), cmd.effective_area, cmd.param3.load(), cmd.param4.load(), cmd.param5.load(), cmd.param6.load());
      res = ses->item_creator->on_specialized_box_item_drop(cmd.effective_area, cmd.param3, cmd.param4, cmd.param5, cmd.param6);
    }
  } else {
    ses->log.info("Creating item from enemy %04hX (area %02hX)", cmd.entity_index.load(), cmd.effective_area);
    res = ses->item_creator->on_monster_item_drop(rec.effective_rt_index, cmd.effective_area);
  }

  if (res.item.empty()) {
    ses->log.info("No item was created");
  } else {
    auto s = ses->require_server_state();
    string name = s->describe_item(ses->version(), res.item, false);
    ses->log.info("Entity %04hX (area %02hX) created item %s", cmd.entity_index.load(), cmd.effective_area, name.c_str());
    res.item.id = ses->next_item_id++;
    ses->log.info("Creating item %08" PRIX32 " at %02hhX:%g,%g for all clients",
        res.item.id.load(), cmd.floor, cmd.pos.x.load(), cmd.pos.z.load());
    send_drop_item_to_channel(s, ses->client_channel, res.item, rec.obj_st ? 2 : 1, cmd.floor, cmd.pos, cmd.entity_index);
    send_drop_item_to_channel(s, ses->server_channel, res.item, rec.obj_st ? 2 : 1, cmd.floor, cmd.pos, cmd.entity_index);
    send_item_notification_if_needed(s, ses->client_channel, ses->config, res.item, res.is_from_rare_table);
  }
  return HandlerResult::Type::SUPPRESS;
}

static HandlerResult S_6x(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string& data) {
  auto s = ses->require_server_state();

  if (ses->config.check_flag(Client::Flag::PROXY_SAVE_FILES)) {
    if (is_ep3(ses->version()) && (data.size() >= 0x14)) {
      if (static_cast<uint8_t>(data[0]) == 0xB6) {
        const auto& header = check_size_t<G_MapSubsubcommand_Ep3_6xB6>(data, 0xFFFF);
        if (header.subsubcommand == 0x00000041) {
          const auto& cmd = check_size_t<G_MapData_Ep3_6xB6x41>(data, 0xFFFF);
          string filename = phosg::string_printf("map%08" PRIX32 ".%" PRIu64 ".mnmd",
              cmd.map_number.load(), phosg::now());
          string map_data = prs_decompress(
              data.data() + sizeof(cmd), data.size() - sizeof(cmd));
          phosg::save_file(filename, map_data);
          if (map_data.size() != sizeof(Episode3::MapDefinition) && map_data.size() != sizeof(Episode3::MapDefinitionTrial)) {
            ses->log.warning("Wrote %zu bytes to %s (expected %zu or %zu bytes; the file may be invalid)",
                map_data.size(), filename.c_str(), sizeof(Episode3::MapDefinitionTrial), sizeof(Episode3::MapDefinition));
          } else {
            ses->log.info("Wrote %zu bytes to %s", map_data.size(), filename.c_str());
          }
        }
      }
    }
  }

  bool modified = false;
  if (!data.empty()) {
    // Unmask any masked Episode 3 commands from the server
    if (is_ep3(ses->version()) && (data.size() > 8) &&
        ((static_cast<uint8_t>(data[0]) == 0xB3) ||
            (static_cast<uint8_t>(data[0]) == 0xB4) ||
            (static_cast<uint8_t>(data[0]) == 0xB5))) {
      const auto& header = check_size_t<G_CardBattleCommandHeader>(data, 0xFFFF);
      if (header.mask_key && (ses->version() != Version::GC_EP3_NTE)) {
        set_mask_for_ep3_game_command(data.data(), data.size(), 0);
        modified = true;
      }

      if (ses->config.check_flag(Client::Flag::PROXY_EP3_INFINITE_TIME_ENABLED) && (header.subcommand == 0xB4)) {
        if (header.subsubcommand == 0x3D) {
          if (ses->version() == Version::GC_EP3_NTE) {
            auto& cmd = check_size_t<G_SetTournamentPlayerDecks_Ep3NTE_6xB4x3D>(data);
            if (cmd.rules.overall_time_limit || cmd.rules.phase_time_limit) {
              cmd.rules.overall_time_limit = 0;
              cmd.rules.phase_time_limit = 0;
              modified = true;
            }
          } else {
            auto& cmd = check_size_t<G_SetTournamentPlayerDecks_Ep3_6xB4x3D>(data);
            if (cmd.rules.overall_time_limit || cmd.rules.phase_time_limit) {
              cmd.rules.overall_time_limit = 0;
              cmd.rules.phase_time_limit = 0;
              modified = true;
            }
          }
        } else if (header.subsubcommand == 0x05) {
          if (ses->version() == Version::GC_EP3_NTE) {
            auto& cmd = check_size_t<G_UpdateMap_Ep3NTE_6xB4x05>(data);
            if (cmd.state.rules.overall_time_limit || cmd.state.rules.phase_time_limit) {
              cmd.state.rules.overall_time_limit = 0;
              cmd.state.rules.phase_time_limit = 0;
              modified = true;
            }
          } else {
            auto& cmd = check_size_t<G_UpdateMap_Ep3_6xB4x05>(data);
            if (cmd.state.rules.overall_time_limit || cmd.state.rules.phase_time_limit) {
              cmd.state.rules.overall_time_limit = 0;
              cmd.state.rules.phase_time_limit = 0;
              modified = true;
            }
          }
        }
      }
    }

    if (data[0] == 0x46) {
      const auto& cmd = check_size_t<G_AttackFinished_6x46>(data,
          offsetof(G_AttackFinished_6x46, targets),
          sizeof(G_AttackFinished_6x46));
      if (cmd.target_count > min<size_t>(cmd.header.size - 2, cmd.targets.size())) {
        ses->log.warning("Blocking subcommand 6x46 with invalid count");
        return HandlerResult::Type::SUPPRESS;
      }
    } else if (data[0] == 0x47) {
      const auto& cmd = check_size_t<G_CastTechnique_6x47>(data,
          offsetof(G_CastTechnique_6x47, targets),
          sizeof(G_CastTechnique_6x47));
      if (cmd.target_count > min<size_t>(cmd.header.size - 2, cmd.targets.size())) {
        ses->log.warning("Blocking subcommand 6x47 with invalid count");
        return HandlerResult::Type::SUPPRESS;
      }
    } else if (data[0] == 0x49) {
      const auto& cmd = check_size_t<G_ExecutePhotonBlast_6x49>(data,
          offsetof(G_ExecutePhotonBlast_6x49, targets),
          sizeof(G_ExecutePhotonBlast_6x49));
      if (cmd.target_count > min<size_t>(cmd.header.size - 3, cmd.targets.size())) {
        ses->log.warning("Blocking subcommand 6x49 with invalid count");
        return HandlerResult::Type::SUPPRESS;
      }

    } else if (data[0] == 0x5F) {
      const auto& cmd = check_size_t<G_DropItem_DC_6x5F>(data, sizeof(G_DropItem_PC_V3_BB_6x5F));
      ItemData item = cmd.item.item;
      item.decode_for_version(ses->version());
      send_item_notification_if_needed(ses->require_server_state(), ses->client_channel, ses->config, item, true);

    } else if ((data[0] == 0x60) || (static_cast<uint8_t>(data[0]) == 0xA2)) {
      return SC_6x60_6xA2(ses, data);

    } else if ((static_cast<uint8_t>(data[0]) == 0xB5) && is_ep3(ses->version()) && (data.size() >= 8)) {
      set_mask_for_ep3_game_command(data.data(), data.size(), 0);
      if (data[4] == 0x1A) {
        return HandlerResult::Type::SUPPRESS;
      } else if (data[4] == 0x20) {
        auto& cmd = check_size_t<G_Unknown_Ep3_6xB5x20>(data);
        if (cmd.client_id >= 12) {
          return HandlerResult::Type::SUPPRESS;
        }
      } else if (data[4] == 0x31) {
        auto& cmd = check_size_t<G_ConfirmDeckSelection_Ep3_6xB5x31>(data);
        if (cmd.menu_type >= 0x15) {
          return HandlerResult::Type::SUPPRESS;
        }
      } else if (data[4] == 0x32) {
        auto& cmd = check_size_t<G_MoveSharedMenuCursor_Ep3_6xB5x32>(data);
        if (cmd.menu_type >= 0x15) {
          return HandlerResult::Type::SUPPRESS;
        }
      } else if (data[4] == 0x36) {
        auto& cmd = check_size_t<G_RecreatePlayer_Ep3_6xB5x36>(data);
        if (ses->is_in_game && (cmd.client_id >= 4)) {
          return HandlerResult::Type::SUPPRESS;
        }
      }

    } else if ((static_cast<uint8_t>(data[0]) == 0xBB) && is_ep3(ses->version())) {
      if (!validate_6xBB(check_size_t<G_SyncCardTradeServerState_Ep3_6xBB>(data))) {
        return HandlerResult::Type::SUPPRESS;
      }
      return HandlerResult::Type::MODIFIED;

    } else if ((static_cast<uint8_t>(data[0]) == 0xBC) && !ses->config.check_flag(Client::Flag::EP3_ALLOW_6xBC)) {
      return HandlerResult::Type::SUPPRESS;

    } else if ((static_cast<uint8_t>(data[0]) == 0xBD) &&
        ses->config.check_flag(Client::Flag::PROXY_EP3_UNMASK_WHISPERS) &&
        is_ep3(ses->version())) {
      auto& cmd = check_size_t<G_PrivateWordSelect_Ep3_6xBD>(data);
      if (cmd.private_flags & (1 << ses->lobby_client_id)) {
        cmd.private_flags &= ~(1 << ses->lobby_client_id);
        modified = true;
      }
    }
  }

  return modified ? HandlerResult::Type::MODIFIED : HandlerResult::Type::FORWARD;
}

static HandlerResult C_GXB_61(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t flag, string& data) {
  bool modified = false;
  // TODO: We should check if the info board text was actually modified and
  // return MODIFIED if so.

  if (is_v4(ses->version())) {
    auto& pd = check_size_t<C_CharacterData_BB_61_98>(data, 0xFFFF);
    pd.info_board.encode(add_color(pd.info_board.decode(ses->language())), ses->language());
    if (ses->config.check_flag(Client::Flag::PROXY_BLANK_NAME_ENABLED)) {
      pd.disp.name.encode(" ", ses->language());
      modified = true;
    }
    if (ses->config.check_flag(Client::Flag::PROXY_RED_NAME_ENABLED) && pd.disp.visual.name_color != 0xFFFF0000) {
      pd.disp.visual.name_color = 0xFFFF0000;
      pd.records.challenge.title_color = 0x7C00;
      modified = true;
    } else if (ses->config.check_flag(Client::Flag::PROXY_BLANK_NAME_ENABLED) && pd.disp.visual.name_color != 0x00000000) {
      pd.disp.visual.name_color = 0x00000000;
      modified = true;
    }
    if (!ses->challenge_rank_title_override.empty()) {
      pd.records.challenge.title_color = encode_rgba8888_to_argb1555(ses->challenge_rank_color_override);
      pd.records.challenge.rank_title.encode(ses->challenge_rank_title_override, ses->language());
    }

  } else {
    C_CharacterData_V3_61_98* pd;
    if (flag == 4) { // Episode 3
      auto& ep3_pd = check_size_t<C_CharacterData_Ep3_61_98>(data);
      // if (ep3_pd.ep3_config.is_encrypted) {
      //   decrypt_trivial_gci_data(
      //       &ep3_pd.ep3_config.card_counts,
      //       offsetof(Episode3::PlayerConfig, decks) - offsetof(Episode3::PlayerConfig, card_counts),
      //       ep3_pd.ep3_config.basis);
      //   ep3_pd.ep3_config.is_encrypted = 0;
      //   ep3_pd.ep3_config.basis = 0;
      //   modified = true;
      // }
      pd = reinterpret_cast<C_CharacterData_V3_61_98*>(&ep3_pd);
    } else {
      if (is_ep3(ses->version()) && (ses->version() != Version::GC_EP3_NTE)) {
        ses->log.info("Version changed to GC_EP3_NTE");
        ses->set_version(Version::GC_EP3_NTE);
        ses->config.specific_version = SPECIFIC_VERSION_GC_EP3_NTE;
      }
      pd = &check_size_t<C_CharacterData_V3_61_98>(data, 0xFFFF);
    }
    pd->info_board.encode(add_color(pd->info_board.decode(ses->language())), ses->language());
    if (ses->config.check_flag(Client::Flag::PROXY_BLANK_NAME_ENABLED)) {
      pd->disp.visual.name.encode(" ", ses->language());
      modified = true;
    }
    if (ses->config.check_flag(Client::Flag::PROXY_RED_NAME_ENABLED) && pd->disp.visual.name_color != 0xFFFF0000) {
      pd->disp.visual.name_color = 0xFFFF0000;
      pd->records.challenge.stats.title_color = 0x7C00;
      modified = true;
    } else if (ses->config.check_flag(Client::Flag::PROXY_BLANK_NAME_ENABLED) && pd->disp.visual.name_color != 0x00000000) {
      pd->disp.visual.name_color = 0x00000000;
      modified = true;
    }
    if (!ses->challenge_rank_title_override.empty()) {
      pd->records.challenge.stats.title_color = encode_rgba8888_to_argb1555(ses->challenge_rank_color_override);
      pd->records.challenge.rank_title.encode(ses->challenge_rank_title_override, ses->language());
    }
  }

  return modified ? HandlerResult::Type::MODIFIED : HandlerResult::Type::FORWARD;
}

static HandlerResult C_GX_D9(shared_ptr<ProxyServer::LinkedSession>, uint16_t, uint32_t, string& data) {
  data = add_color(data);
  // TODO: We should check if the info board text was actually modified and
  // return FORWARD if not.
  return HandlerResult::Type::MODIFIED;
}

static HandlerResult C_B_D9(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string& data) {
  try {
    string decoded = tt_utf16_to_utf8(data.data(), data.size());
    add_color_inplace(decoded);
    data = tt_utf8_to_utf16(data.data(), data.size());
  } catch (const runtime_error& e) {
    ses->log.warning("Failed to decode and unescape D9 command: %s", e.what());
  }
  // TODO: We should check if the info board text was actually modified and
  // return HandlerResult::FORWARD if not.
  return HandlerResult::Type::MODIFIED;
}

template <typename T>
static HandlerResult S_44_A6(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t command, uint32_t, string& data) {
  const auto& cmd = check_size_t<T>(data);

  string filename = cmd.filename.decode();
  string output_filename;
  bool is_download = (command == 0xA6);
  if (ses->config.check_flag(Client::Flag::PROXY_SAVE_FILES)) {
    size_t extension_offset = filename.rfind('.');
    string basename, extension;
    if (extension_offset != string::npos) {
      basename = filename.substr(0, extension_offset);
      extension = filename.substr(extension_offset);
      if (extension == ".bin" && is_ep3(ses->version())) {
        extension += ".mnm";
      }
    } else {
      basename = filename;
    }
    output_filename = phosg::string_printf("%s.%s.%" PRIu64 "%s",
        basename.c_str(),
        is_download ? "download" : "online",
        phosg::now(),
        extension.c_str());

    for (size_t x = 0; x < output_filename.size(); x++) {
      if (output_filename[x] < 0x20 || output_filename[x] > 0x7E || output_filename[x] == '/') {
        output_filename[x] = '_';
      }
    }
    if (output_filename[0] == '.') {
      output_filename[0] = '_';
    }
  }

  // Episode 3 download quests aren't DLQ-encoded (but they are on Trial Edition)
  bool decode_dlq = is_download && (ses->version() != Version::GC_EP3);
  ProxyServer::LinkedSession::SavingFile sf(filename, output_filename, cmd.file_size, decode_dlq);
  ses->saving_files.emplace(filename, std::move(sf));
  if (ses->config.check_flag(Client::Flag::PROXY_SAVE_FILES)) {
    ses->log.info("Saving %s from server to %s", filename.c_str(), output_filename.c_str());
  } else {
    ses->log.info("Tracking file %s", filename.c_str());
  }

  return HandlerResult::Type::FORWARD;
}

constexpr on_command_t S_D_44_A6 = &S_44_A6<S_OpenFile_DC_44_A6>;
constexpr on_command_t S_PG_44_A6 = &S_44_A6<S_OpenFile_PC_GC_44_A6>;
constexpr on_command_t S_X_44_A6 = &S_44_A6<S_OpenFile_XB_44_A6>;
constexpr on_command_t S_B_44_A6 = &S_44_A6<S_OpenFile_BB_44_A6>;

static HandlerResult S_13_A7(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t flag, string& data) {
  auto& cmd = check_size_t<S_WriteFile_13_A7>(data);
  bool modified = false;

  ProxyServer::LinkedSession::SavingFile* sf = nullptr;
  try {
    sf = &ses->saving_files.at(cmd.filename.decode());
  } catch (const out_of_range&) {
    string filename = cmd.filename.decode();
    ses->log.warning("Received data for non-open file %s", filename.c_str());
    return HandlerResult::Type::FORWARD;
  }

  bool is_last_block = (cmd.data_size != 0x400);
  size_t block_offset = flag * 0x400;
  size_t allowed_block_size = (block_offset < sf->total_size)
      ? min<size_t>(sf->total_size - block_offset, 0x400)
      : 0;

  if (cmd.data_size > allowed_block_size) {
    ses->log.warning("Block size extends beyond allowed size; truncating block");
    cmd.data_size = allowed_block_size;
    modified = true;
  }

  if (!sf->output_filename.empty()) {
    ses->log.info("Adding %" PRIu32 " bytes to %s:%02" PRIX32 " => %s:%zX",
        cmd.data_size.load(), sf->basename.c_str(), flag, sf->output_filename.c_str(), block_offset);
    if (ses->config.check_flag(Client::Flag::PROXY_SAVE_FILES)) {
      size_t block_end_offset = block_offset + cmd.data_size;
      if (sf->data.size() < block_end_offset) {
        sf->data.resize(block_end_offset);
      }
      memcpy(sf->data.data() + block_offset, reinterpret_cast<const char*>(cmd.data.data()), cmd.data_size);
    }
  }

  if (is_last_block) {
    if (ses->config.check_flag(Client::Flag::PROXY_SAVE_FILES)) {
      ses->log.info("Writing file %s => %s", sf->basename.c_str(), sf->output_filename.c_str());

      if (sf->is_download && (phosg::ends_with(sf->basename, ".bin") || phosg::ends_with(sf->basename, ".dat") || phosg::ends_with(sf->basename, ".pvr"))) {
        sf->data = decode_dlq_data(sf->data);
      }
      phosg::save_file(sf->output_filename, sf->data);
      if (phosg::ends_with(sf->basename, ".bin")) {
        try {
          string decompressed = prs_decompress(sf->data);
          auto disassembly = disassemble_quest_script(decompressed.data(), decompressed.size(), ses->version(), ses->language(), false);
          phosg::save_file(sf->output_filename + ".txt", disassembly);
        } catch (const exception& e) {
          ses->log.warning("Failed to disassemble quest file: %s", e.what());
        }
      }
    } else {
      ses->log.info("Download complete for file %s", sf->basename.c_str());
    }

    if (!sf->is_download && phosg::ends_with(sf->basename, ".dat")) {
      auto quest_dat_data = make_shared<std::string>(prs_decompress(sf->data));
      try {
        auto map_file = make_shared<MapFile>(quest_dat_data);
        auto materialized_map_file = map_file->materialize_random_sections(ses->lobby_random_seed);

        array<shared_ptr<const MapFile>, NUM_VERSIONS> map_files;
        map_files.at(static_cast<size_t>(ses->version())) = materialized_map_file;
        auto supermap = make_shared<SuperMap>(ses->lobby_episode, map_files);

        ses->map_state = make_shared<MapState>(
            ses->id,
            ses->lobby_difficulty,
            ses->lobby_event,
            ses->lobby_random_seed,
            MapState::DEFAULT_RARE_ENEMIES,
            make_shared<PSOV2Encryption>(ses->lobby_random_seed),
            supermap);

      } catch (const exception& e) {
        ses->log.warning("Failed to load quest map: %s", e.what());
        ses->map_state.reset();
      }
    }

    ses->saving_files.erase(cmd.filename.decode());
  }

  return modified ? HandlerResult::Type::MODIFIED : HandlerResult::Type::FORWARD;
}

static HandlerResult S_G_B7(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string& data) {
  if (is_ep3(ses->version())) {
    if (ses->config.check_flag(Client::Flag::PROXY_EP3_INFINITE_MESETA_ENABLED)) {
      auto& cmd = check_size_t<S_RankUpdate_Ep3_B7>(data);
      if (cmd.current_meseta != 1000000) {
        cmd.current_meseta = 1000000;
        return HandlerResult::Type::MODIFIED;
      }
    }
    return HandlerResult::Type::FORWARD;
  } else {
    ses->server_channel.send(0xB7, 0x00);
    return HandlerResult::Type::SUPPRESS;
  }
}

static HandlerResult S_G_B8(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string& data) {
  if (ses->config.check_flag(Client::Flag::PROXY_SAVE_FILES)) {
    if (data.size() < 4) {
      ses->log.warning("Card list data size is too small; not saving file");
      return HandlerResult::Type::FORWARD;
    }

    phosg::StringReader r(data);
    size_t size = r.get_u32l();
    if (r.remaining() < size) {
      ses->log.warning("Card list data size extends beyond end of command; not saving file");
      return HandlerResult::Type::FORWARD;
    }

    string output_filename = phosg::string_printf("card-definitions.%" PRIu64 ".mnr", phosg::now());
    phosg::save_file(output_filename, r.read(size));
    ses->log.info("Wrote %zu bytes to %s", size, output_filename.c_str());
  }

  // Unset the flag specifying that the client has newserv's card definitions,
  // so the file sill be sent again if the client returns to newserv.
  ses->config.clear_flag(Client::Flag::HAS_EP3_CARD_DEFS);

  return is_ep3(ses->version()) ? HandlerResult::Type::FORWARD : HandlerResult::Type::SUPPRESS;
}

static HandlerResult S_G_B9(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string& data) {
  if (ses->config.check_flag(Client::Flag::PROXY_SAVE_FILES)) {
    try {
      const auto& header = check_size_t<S_UpdateMediaHeader_Ep3_B9>(data, 0xFFFF);

      if (data.size() - sizeof(header) < header.size) {
        throw runtime_error("Media data size extends beyond end of command; not saving file");
      }

      string decompressed_data = prs_decompress(
          data.data() + sizeof(header), data.size() - sizeof(header));

      string output_filename = phosg::string_printf("media-update.%" PRIu64, phosg::now());
      if (header.type == 1) {
        output_filename += ".gvm";
      } else if (header.type == 2 || header.type == 3) {
        output_filename += ".bml";
      } else {
        output_filename += ".bin";
      }
      phosg::save_file(output_filename, decompressed_data);
      ses->log.info("Wrote %zu bytes to %s",
          decompressed_data.size(), output_filename.c_str());
    } catch (const exception& e) {
      ses->log.warning("Failed to save file: %s", e.what());
    }
  }

  // This command exists only in final Episode 3 and not in Trial Edition
  // (hence not using is_ep3() here)
  return (ses->version() == Version::GC_EP3)
      ? HandlerResult::Type::FORWARD
      : HandlerResult::Type::SUPPRESS;
}

static HandlerResult S_G_EF(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string& data) {
  if (is_ep3(ses->version())) {
    if (ses->config.check_flag(Client::Flag::PROXY_EP3_INFINITE_MESETA_ENABLED)) {
      auto& cmd = check_size_t<S_StartCardAuction_Ep3_EF>(data,
          offsetof(S_StartCardAuction_Ep3_EF, unused), 0xFFFF);
      if (cmd.points_available != 0x7FFF) {
        cmd.points_available = 0x7FFF;
        return HandlerResult::Type::MODIFIED;
      }
    }
    return HandlerResult::Type::FORWARD;
  } else {
    return HandlerResult::Type::SUPPRESS;
  }
}

static HandlerResult S_B_EF(shared_ptr<ProxyServer::LinkedSession>, uint16_t, uint32_t, string&) {
  return HandlerResult::Type::SUPPRESS;
}

static HandlerResult S_G_BA(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string& data) {
  if (ses->config.check_flag(Client::Flag::PROXY_EP3_INFINITE_MESETA_ENABLED)) {
    auto& cmd = check_size_t<S_MesetaTransaction_Ep3_BA>(data);
    if (cmd.current_meseta != 1000000) {
      cmd.current_meseta = 1000000;
      return HandlerResult::Type::MODIFIED;
    }
  }
  return HandlerResult::Type::FORWARD;
}

static void update_leader_id(shared_ptr<ProxyServer::LinkedSession> ses, uint8_t leader_id) {
  if (ses->leader_client_id != leader_id) {
    ses->leader_client_id = leader_id;
    ses->log.info("Changed room leader to %zu", ses->leader_client_id);
    if (ses->config.check_flag(Client::Flag::PROXY_PLAYER_NOTIFICATIONS_ENABLED) && (ses->leader_client_id == ses->lobby_client_id)) {
      send_text_message(ses->client_channel, "$C6You are now the leader");
    }
  }
}

template <typename CmdT>
static HandlerResult S_65_67_68_EB(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t command, uint32_t flag, string& data) {
  if (command == 0x67) {
    ses->clear_lobby_players(12);
    ses->is_in_game = false;
    ses->is_in_quest = false;
    ses->floor = 0x0F;
    ses->lobby_difficulty = 0;
    ses->lobby_section_id = 0;
    ses->lobby_mode = GameMode::NORMAL;
    ses->lobby_episode = Episode::EP1;
    ses->lobby_random_seed = 0;
    ses->item_creator.reset();
    ses->map_state.reset();

    // This command can cause the client to no longer send D6 responses when
    // 1A/D5 large message boxes are closed. newserv keeps track of this
    // behavior in the client config, so if it happens during a proxy session,
    // update the client config that we'll restore if the client uses the change
    // ship or change block command.
    if (ses->config.check_flag(Client::Flag::NO_D6_AFTER_LOBBY)) {
      ses->config.set_flag(Client::Flag::NO_D6);
    }
  }

  size_t expected_size = offsetof(CmdT, entries) + sizeof(typename CmdT::Entry) * flag;
  auto& cmd = check_size_t<CmdT>(data, expected_size, 0xFFFF);
  bool modified = false;

  size_t num_replacements = 0;
  ses->lobby_client_id = cmd.lobby_flags.client_id;
  ses->lobby_event = cmd.lobby_flags.event;
  update_leader_id(ses, cmd.lobby_flags.leader_id);
  for (size_t x = 0; x < flag; x++) {
    auto& entry = cmd.entries[x];
    size_t index = entry.lobby_data.client_id;
    if (index >= ses->lobby_players.size()) {
      ses->log.warning("Ignoring invalid player index %zu at position %zu", index, x);
    } else {
      string name = escape_player_name(entry.disp.visual.name.decode(entry.inventory.language));
      if (ses->login && (entry.lobby_data.guild_card_number == ses->remote_guild_card_number)) {
        entry.lobby_data.guild_card_number = ses->login->account->account_id;
        num_replacements++;
        modified = true;
      } else if (ses->config.check_flag(Client::Flag::PROXY_PLAYER_NOTIFICATIONS_ENABLED) &&
          (command != 0x67)) {
        send_text_message_printf(ses->client_channel, "$C6Join: %zu/%" PRIu32 "\n%s",
            index, entry.lobby_data.guild_card_number.load(), name.c_str());
      }
      auto& p = ses->lobby_players[index];
      p.guild_card_number = entry.lobby_data.guild_card_number;
      p.name = name;
      p.language = entry.inventory.language;
      p.section_id = entry.disp.visual.section_id;
      p.char_class = entry.disp.visual.char_class;
      ses->log.info("Added lobby player: (%zu) %" PRIu32 " %s",
          index, p.guild_card_number, p.name.c_str());
    }
  }
  if (num_replacements > 1) {
    ses->log.warning("Proxied player appears multiple times in lobby");
  }

  if (ses->config.override_lobby_event != 0xFF) {
    cmd.lobby_flags.event = ses->config.override_lobby_event;
    modified = true;
  }
  if (ses->config.override_lobby_number != 0x80) {
    cmd.lobby_flags.lobby_number = ses->config.override_lobby_number;
    modified = true;
  }

  return modified ? HandlerResult::Type::MODIFIED : HandlerResult::Type::FORWARD;
}

constexpr on_command_t S_DG_65_67_68_EB = &S_65_67_68_EB<S_JoinLobby_DC_GC_65_67_68_Ep3_EB>;
constexpr on_command_t S_P_65_67_68 = &S_65_67_68_EB<S_JoinLobby_PC_65_67_68>;
constexpr on_command_t S_X_65_67_68 = &S_65_67_68_EB<S_JoinLobby_XB_65_67_68>;
constexpr on_command_t S_B_65_67_68 = &S_65_67_68_EB<S_JoinLobby_BB_65_67_68>;

template <typename CmdT>
Episode get_episode(const CmdT&) {
  return Episode::EP1;
}
template <>
Episode get_episode<S_JoinGame_GC_64>(const S_JoinGame_GC_64& cmd) {
  switch (cmd.episode) {
    case 1:
      return Episode::EP1;
    case 2:
      return Episode::EP2;
    default:
      return Episode::NONE;
  }
}
template <>
Episode get_episode<S_JoinGame_XB_64>(const S_JoinGame_XB_64& cmd) {
  switch (cmd.episode) {
    case 1:
      return Episode::EP1;
    case 2:
      return Episode::EP2;
    default:
      return Episode::NONE;
  }
}
template <>
Episode get_episode<S_JoinGame_BB_64>(const S_JoinGame_BB_64& cmd) {
  switch (cmd.episode) {
    case 1:
      return Episode::EP1;
    case 2:
      return Episode::EP2;
    case 3:
      return Episode::EP4;
    default:
      return Episode::NONE;
  }
}
template <>
Episode get_episode<S_JoinGame_Ep3_64>(const S_JoinGame_Ep3_64&) {
  return Episode::EP3;
}

template <typename CmdT>
static HandlerResult S_64(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t flag, string& data) {
  CmdT* cmd;
  S_JoinGame_Ep3_64* cmd_ep3 = nullptr;
  if (ses->sub_version >= 0x40) {
    cmd = &check_size_t<CmdT>(data, sizeof(S_JoinGame_Ep3_64));
    cmd_ep3 = &check_size_t<S_JoinGame_Ep3_64>(data);
  } else if (ses->version() == Version::XB_V3) {
    // Schtserv doesn't send the unknown_a1 field in this command, and we don't
    // use it here, so we allow it to be omitted.
    cmd = &check_size_t<CmdT>(data.data(), data.size(), sizeof(CmdT) - 0x18, sizeof(CmdT));
  } else {
    cmd = &check_size_t<CmdT>(data, 0xFFFF);
  }

  ses->clear_lobby_players(4);
  ses->floor = 0;
  ses->is_in_game = true;
  ses->is_in_quest = false;
  ses->lobby_event = cmd->event;
  ses->lobby_difficulty = cmd->difficulty;
  ses->lobby_section_id = cmd->section_id;
  // We only need the game mode for overriding drops, and SOLO behaves the same
  // as NORMAL in that regard, so we can conveniently ignore SOLO here
  if (cmd->battle_mode) {
    ses->lobby_mode = GameMode::BATTLE;
  } else if (cmd->challenge_mode) {
    ses->lobby_mode = GameMode::CHALLENGE;
  } else {
    ses->lobby_mode = GameMode::NORMAL;
  }
  ses->lobby_random_seed = cmd->random_seed;
  if (cmd_ep3) {
    ses->lobby_episode = Episode::EP3;
  } else {
    ses->lobby_episode = get_episode(*cmd);
  }

  if (ses->version() == Version::GC_NTE) {
    // GC NTE ignores the variations field entirely, so clear the array to
    // ensure we'll load the correct maps
    cmd->variations = Variations();
  }

  // Recreate the item creator if needed, and load maps
  auto s = ses->require_server_state();
  ses->set_drop_mode(ses->drop_mode);
  if (!is_ep3(ses->version()) && (ses->lobby_mode != GameMode::CHALLENGE)) {
    auto s = ses->require_server_state();
    ses->map_state = make_shared<MapState>(
        ses->id,
        ses->lobby_difficulty,
        ses->lobby_event,
        ses->lobby_random_seed,
        MapState::DEFAULT_RARE_ENEMIES,
        make_shared<PSOV2Encryption>(ses->lobby_random_seed),
        s->supermaps_for_variations(ses->lobby_episode, ses->lobby_mode, ses->lobby_difficulty, cmd->variations));
  } else {
    ses->map_state.reset();
  }

  bool modified = false;

  ses->lobby_client_id = cmd->client_id;
  update_leader_id(ses, cmd->leader_id);
  for (size_t x = 0; x < flag; x++) {
    if (cmd->lobby_data[x].guild_card_number == ses->remote_guild_card_number) {
      cmd->lobby_data[x].guild_card_number = ses->login->account->account_id;
      modified = true;
    }
    auto& p = ses->lobby_players[x];
    p.guild_card_number = cmd->lobby_data[x].guild_card_number;
    if (cmd_ep3) {
      const auto& p_ep3 = cmd_ep3->players_ep3[x];
      p.language = p_ep3.inventory.language;
      p.name = p_ep3.disp.visual.name.decode(p.language);
      p.section_id = p_ep3.disp.visual.section_id;
      p.char_class = p_ep3.disp.visual.char_class;
    } else {
      p.name.clear();
    }
    ses->log.info("Added lobby player: (%zu) %" PRIu32 " %s", x, p.guild_card_number, p.name.c_str());
  }

  if (ses->config.override_section_id != 0xFF) {
    cmd->section_id = ses->config.override_section_id;
    modified = true;
  }
  if (ses->config.override_lobby_event != 0xFF) {
    cmd->event = ses->config.override_lobby_event;
    modified = true;
  }
  if (ses->config.check_flag(Client::Flag::USE_OVERRIDE_RANDOM_SEED)) {
    cmd->random_seed = ses->config.override_random_seed;
    modified = true;
  }

  return modified ? HandlerResult::Type::MODIFIED : HandlerResult::Type::FORWARD;
}

constexpr on_command_t S_D_64 = &S_64<S_JoinGame_DC_64>;
constexpr on_command_t S_P_64 = &S_64<S_JoinGame_PC_64>;
constexpr on_command_t S_G_64 = &S_64<S_JoinGame_GC_64>;
constexpr on_command_t S_X_64 = &S_64<S_JoinGame_XB_64>;
constexpr on_command_t S_B_64 = &S_64<S_JoinGame_BB_64>;

static HandlerResult S_E8(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string& data) {
  auto& cmd = check_size_t<S_JoinSpectatorTeam_Ep3_E8>(data);

  ses->clear_lobby_players(12);
  ses->floor = 0;
  ses->is_in_game = true;
  ses->is_in_quest = false;
  ses->lobby_event = cmd.event;
  ses->lobby_difficulty = 0;
  ses->lobby_section_id = cmd.section_id;
  ses->lobby_mode = GameMode::NORMAL;
  ses->lobby_random_seed = 0;
  ses->lobby_episode = Episode::EP3;
  ses->item_creator.reset();
  ses->map_state.reset();

  bool modified = false;

  ses->lobby_client_id = cmd.client_id;
  update_leader_id(ses, cmd.leader_id);

  for (size_t x = 0; x < 12; x++) {
    auto& player_entry = (x < 4) ? cmd.players[x] : cmd.spectator_players[x - 4];
    auto& spec_entry = cmd.entries[x];

    if (player_entry.lobby_data.guild_card_number == ses->remote_guild_card_number) {
      player_entry.lobby_data.guild_card_number = ses->login->account->account_id;
      modified = true;
    }
    if (spec_entry.guild_card_number == ses->remote_guild_card_number) {
      spec_entry.guild_card_number = ses->login->account->account_id;
      modified = true;
    }

    auto& p = ses->lobby_players[x];
    p.guild_card_number = player_entry.lobby_data.guild_card_number;
    p.language = player_entry.inventory.language;
    p.name = player_entry.disp.visual.name.decode(p.language);
    p.section_id = player_entry.disp.visual.section_id;
    p.char_class = player_entry.disp.visual.char_class;
    ses->log.info("Added lobby player: (%zu) %" PRIu32 " %s", x, p.guild_card_number, p.name.c_str());
  }

  if (ses->config.override_section_id != 0xFF) {
    cmd.section_id = ses->config.override_section_id;
    modified = true;
  }
  if (ses->config.override_lobby_event != 0xFF) {
    cmd.event = ses->config.override_lobby_event;
    modified = true;
  }
  if (ses->config.check_flag(Client::Flag::USE_OVERRIDE_RANDOM_SEED)) {
    cmd.random_seed = ses->config.override_random_seed;
    modified = true;
  }

  return modified ? HandlerResult::Type::MODIFIED : HandlerResult::Type::FORWARD;
}

static HandlerResult S_AC(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string&) {
  if (!ses->is_in_game) {
    return HandlerResult::Type::SUPPRESS;
  } else {
    ses->is_in_quest = true;
    return HandlerResult::Type::FORWARD;
  }
}

static HandlerResult S_66_69_E9(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string& data) {
  // Schtserv sends a large command here for unknown reasons. The client ignores
  // the extra data, so we allow the large command here.
  const auto& cmd = check_size_t<S_LeaveLobby_66_69_Ep3_E9>(data, 0xFFFF);
  size_t index = cmd.client_id;
  if (index >= ses->lobby_players.size()) {
    ses->log.warning("Lobby leave command references missing position");
  } else {
    auto& p = ses->lobby_players[index];
    string name = escape_player_name(p.name);
    if (ses->config.check_flag(Client::Flag::PROXY_PLAYER_NOTIFICATIONS_ENABLED)) {
      send_text_message_printf(ses->client_channel, "$C4Leave: %zu/%" PRIu32 "\n%s",
          index, p.guild_card_number, name.c_str());
    }
    p.guild_card_number = 0;
    p.name.clear();
    ses->log.info("Removed lobby player (%zu)", index);
  }
  update_leader_id(ses, cmd.leader_id);
  return HandlerResult::Type::FORWARD;
}

static HandlerResult C_98(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t command, uint32_t flag, string& data) {
  ses->floor = 0x0F;
  ses->is_in_game = false;
  ses->is_in_quest = false;
  ses->lobby_event = 0;
  ses->lobby_difficulty = 0;
  ses->lobby_section_id = 0;
  ses->lobby_episode = Episode::EP1;
  ses->lobby_mode = GameMode::NORMAL;
  ses->lobby_random_seed = 0;
  ses->item_creator.reset();
  ses->map_state.reset();

  if (is_v3(ses->version()) || is_v4(ses->version())) {
    return C_GXB_61(ses, command, flag, data);
  } else {
    return HandlerResult::Type::FORWARD;
  }
}

static HandlerResult C_06(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string& data) {
  if (data.size() >= 12) {
    const auto& cmd = check_size_t<SC_TextHeader_01_06_11_B0_EE>(data, 0xFFFF);

    string text = data.substr(sizeof(cmd));
    phosg::strip_trailing_zeroes(text);

    uint8_t private_flags = 0;
    try {
      if (uses_utf16(ses->version())) {
        if (text.size() & 1) {
          text.push_back(0);
        }
        text = tt_decode_marked(text, ses->language(), true);
      } else if (!text.empty() && (text[0] != '\t') && is_ep3(ses->version())) {
        private_flags = text[0];
        text = tt_decode_marked(text.substr(1), ses->language(), false);
      } else {
        text = tt_decode_marked(text, ses->language(), false);
      }
    } catch (const runtime_error& e) {
      ses->log.warning("Failed to decode and unescape chat text: %s", e.what());
      return HandlerResult::Type::FORWARD;
    }

    if (text.empty()) {
      return HandlerResult::Type::SUPPRESS;
    }

    char command_sentinel = (ses->version() == Version::DC_11_2000) ? '@' : '$';
    bool is_command = (text[0] == command_sentinel) ||
        (text[0] == '\t' && text[1] != 'C' && text[2] == command_sentinel);
    if (is_command && ses->config.check_flag(Client::Flag::PROXY_CHAT_COMMANDS_ENABLED)) {
      size_t offset = ((text[0] & 0xF0) == 0x40) ? 1 : 0;
      offset += (text[offset] == command_sentinel) ? 0 : 2;
      text = text.substr(offset);
      if (text.size() >= 2 && text[1] == command_sentinel) {
        send_chat_message_from_client(ses->server_channel, text.substr(1), private_flags);
        return HandlerResult::Type::SUPPRESS;
      } else {
        on_chat_command(ses, text, true);
        return HandlerResult::Type::SUPPRESS;
      }

    } else {
      return HandlerResult::Type::FORWARD;
    }

  } else {
    return HandlerResult::Type::FORWARD;
  }
}

static HandlerResult C_40(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string& data) {
  bool modified = false;
  if (ses->login) {
    auto& cmd = check_size_t<C_GuildCardSearch_40>(data);
    if (cmd.searcher_guild_card_number == ses->login->account->account_id) {
      cmd.searcher_guild_card_number = ses->remote_guild_card_number;
      modified = true;
    }
    if (cmd.target_guild_card_number == ses->login->account->account_id) {
      cmd.target_guild_card_number = ses->remote_guild_card_number;
      modified = true;
    }
  }
  return modified ? HandlerResult::Type::MODIFIED : HandlerResult::Type::FORWARD;
}

template <typename CmdT>
static HandlerResult C_81(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string& data) {
  auto& cmd = check_size_t<CmdT>(data);
  if (ses->login) {
    if (cmd.from_guild_card_number == ses->login->account->account_id) {
      cmd.from_guild_card_number = ses->remote_guild_card_number;
    }
    if (cmd.to_guild_card_number == ses->login->account->account_id) {
      cmd.to_guild_card_number = ses->remote_guild_card_number;
    }
  }
  // GC clients send uninitialized memory here; don't forward it
  cmd.text.clear_after_bytes(cmd.text.used_chars_8());
  return HandlerResult::Type::MODIFIED;
}

constexpr on_command_t C_DGX_81 = &C_81<SC_SimpleMail_DC_V3_81>;
constexpr on_command_t C_P_81 = &C_81<SC_SimpleMail_PC_81>;
constexpr on_command_t C_B_81 = &C_81<SC_SimpleMail_BB_81>;

template <typename CmdT>
void C_6x_movement(shared_ptr<ProxyServer::LinkedSession> ses, const string& data) {
  ses->pos = check_size_t<CmdT>(data).pos;
}

template <typename SendGuildCardCmdT>
static HandlerResult C_6x(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t command, uint32_t flag, string& data) {
  if (ses->login && !data.empty()) {
    // On BB, the 6x06 command is blank - the server generates the actual Guild
    // Card contents and sends it to the target client.
    if ((data[0] == 0x06) && !is_v4(ses->version())) {
      auto& cmd = check_size_t<SendGuildCardCmdT>(data);
      if (cmd.guild_card.guild_card_number == ses->login->account->account_id) {
        cmd.guild_card.guild_card_number = ses->remote_guild_card_number;
      }
    }
  }
  return C_6x<void>(ses, command, flag, data);
}

constexpr on_command_t C_D_6x = &C_6x<G_SendGuildCard_DC_6x06>;
constexpr on_command_t C_P_6x = &C_6x<G_SendGuildCard_PC_6x06>;
constexpr on_command_t C_G_6x = &C_6x<G_SendGuildCard_GC_6x06>;
constexpr on_command_t C_X_6x = &C_6x<G_SendGuildCard_XB_6x06>;
constexpr on_command_t C_B_6x = &C_6x<G_SendGuildCard_BB_6x06>;

template <>
HandlerResult C_6x<void>(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string& data) {
  if (!data.empty()) {
    if ((data[0] == 0x05) && ses->config.check_flag(Client::Flag::SWITCH_ASSIST_ENABLED)) {
      auto& cmd = check_size_t<G_SwitchStateChanged_6x05>(data);
      if (ses->map_state && (cmd.flags & 1) && (cmd.header.entity_id != 0xFFFF)) {
        auto door_states = ses->map_state->door_states_for_switch_flag(
            ses->version(), cmd.switch_flag_floor, cmd.switch_flag_num);
        for (auto& door_state : door_states) {
          if (door_state->game_flags & 0x0001) {
            continue;
          }
          door_state->game_flags |= 1;

          uint16_t object_index = ses->map_state->index_for_object_state(ses->version(), door_state);
          G_UpdateObjectState_6x0B cmd0B;
          cmd0B.header.subcommand = 0x0B;
          cmd0B.header.size = sizeof(cmd0B) / 4;
          cmd0B.header.entity_id = object_index | 0x4000;
          cmd0B.flags = door_state->game_flags;
          cmd0B.object_index = object_index;
          ses->client_channel.send(0x60, 0x00, &cmd0B, sizeof(cmd0B));
          ses->server_channel.send(0x60, 0x00, &cmd0B, sizeof(cmd0B));
        }
      }

    } else if (data[0] == 0x21) {
      const auto& cmd = check_size_t<G_InterLevelWarp_6x21>(data);
      ses->floor = cmd.floor;

    } else if (data[0] == 0x0C) {
      if (ses->config.check_flag(Client::Flag::INFINITE_HP_ENABLED)) {
        send_remove_negative_conditions(ses->client_channel, ses->lobby_client_id);
        send_remove_negative_conditions(ses->server_channel, ses->lobby_client_id);
      }

    } else if (data[0] == 0x2F || data[0] == 0x4B || data[0] == 0x4C) {
      if (ses->config.check_flag(Client::Flag::INFINITE_HP_ENABLED)) {
        send_player_stats_change(ses->client_channel,
            ses->lobby_client_id, PlayerStatsChange::ADD_HP, 2550);
        send_player_stats_change(ses->server_channel,
            ses->lobby_client_id, PlayerStatsChange::ADD_HP, 2550);
      }

    } else if (data[0] == 0x3E) {
      C_6x_movement<G_StopAtPosition_6x3E>(ses, data);

    } else if (data[0] == 0x3F) {
      C_6x_movement<G_SetPosition_6x3F>(ses, data);

    } else if (data[0] == 0x40) {
      C_6x_movement<G_WalkToPosition_6x40>(ses, data);

    } else if ((data[0] == 0x41) || (data[0] == 0x42)) {
      C_6x_movement<G_MoveToPosition_6x41_6x42>(ses, data);

    } else if (data[0] == 0x48) {
      if (ses->config.check_flag(Client::Flag::INFINITE_TP_ENABLED)) {
        send_player_stats_change(ses->client_channel, ses->lobby_client_id, PlayerStatsChange::ADD_TP, 255);
        send_player_stats_change(ses->server_channel, ses->lobby_client_id, PlayerStatsChange::ADD_TP, 255);
      }

    } else if (data[0] == 0x5F) {
      const auto& cmd = check_size_t<G_DropItem_DC_6x5F>(data, sizeof(G_DropItem_PC_V3_BB_6x5F));
      send_item_notification_if_needed(ses->require_server_state(), ses->client_channel, ses->config, cmd.item.item, true);

    } else if (data[0] == 0x60 || static_cast<uint8_t>(data[0]) == 0xA2) {
      return SC_6x60_6xA2(ses, data);

    } else if (is_ep3(ses->version()) && (data.size() > 4) && (static_cast<uint8_t>(data[0]) == 0xB5)) {
      if (data[4] == 0x38) {
        ses->config.set_flag(Client::Flag::EP3_ALLOW_6xBC);
      } else if (data[4] == 0x3C) {
        ses->config.clear_flag(Client::Flag::EP3_ALLOW_6xBC);
      }
    }
  }

  return HandlerResult::Type::FORWARD;
}

static HandlerResult C_V123_A0_A1(shared_ptr<ProxyServer::LinkedSession> ses, uint16_t, uint32_t, string&) {
  if (!ses->login) {
    return HandlerResult::Type::FORWARD;
  }

  // For logged-in sessions, send them back to newserv's main menu instead of
  // going to the remote server's ship/block select menu
  ses->send_to_game_server();
  ses->disconnect_action = ProxyServer::LinkedSession::DisconnectAction::CLOSE_IMMEDIATELY;
  return HandlerResult::Type::SUPPRESS;
}

// Indexed as [command][version][is_from_client]
static_assert(NUM_VERSIONS == 14, "Don\'t forget to update the ProxyCommands handlers table");
static on_command_t handlers[0x100][NUM_VERSIONS][2] = {
    // clang-format off
// CMD     S_PC_PATCH     C          S_BB_PATCH        C       S_DC_NTE C          S_DC_V1_12_2000_PROTO C          S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* 00 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 01 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 02 */ {{S_V123P_02_17, nullptr}, {S_V123P_02_17, nullptr}, {nullptr, nullptr}, {S_V123P_02_17,    nullptr},     {S_V123P_02_17,    nullptr},     {S_V123P_02_17,    nullptr},      {S_V123P_02_17, nullptr},      {S_V123P_02_17, nullptr},      {S_V123P_02_17,    nullptr},     {S_V123P_02_17,    nullptr},      {S_V123P_02_17,    nullptr},      {S_V123P_02_17,    nullptr},      {S_V123P_02_17, nullptr},      {nullptr,      nullptr}},
/* 03 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {S_B_03,       nullptr}},
/* 04 */ {{nullptr,       nullptr}, {nullptr,       nullptr}, {nullptr, nullptr}, {S_V123_04,        nullptr},     {S_V123_04,        nullptr},     {S_V123_04,        nullptr},      {S_V123_04,     nullptr},      {S_V123_04,     nullptr},      {S_V123_04,        nullptr},     {S_V123_04,        nullptr},      {S_V123_04,        nullptr},      {S_V123_04,        nullptr},      {S_V123_04,     nullptr},      {nullptr,      nullptr}},
/* 05 */ {{nullptr,       C_05},    {nullptr,       C_05},    {nullptr, nullptr}, {nullptr,          C_05},        {nullptr,          C_05},        {nullptr,          C_05},         {nullptr,       C_05},         {nullptr,       C_05},         {nullptr,          C_05},        {nullptr,          C_05},         {nullptr,          C_05},         {nullptr,          C_05},         {nullptr,       C_05},         {nullptr,      C_05}},
/* 06 */ {{nullptr,       nullptr}, {nullptr,       nullptr}, {nullptr, nullptr}, {S_V123_06,        C_06},        {S_V123_06,        C_06},        {S_V123_06,        C_06},         {S_V123_06,     C_06},         {S_V123_06,     C_06},         {S_V123_06,        C_06},        {S_V123_06,        C_06},         {S_V123_06,        C_06},         {S_V123_06,        C_06},         {S_V123_06,     C_06},         {nullptr,      C_06}},
/* 07 */ {{nullptr,       nullptr}, {nullptr,       nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 08 */ {{nullptr,       nullptr}, {nullptr,       nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 09 */ {{nullptr,       nullptr}, {nullptr,       nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 0A */ {{nullptr,       nullptr}, {nullptr,       nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 0B */ {{nullptr,       nullptr}, {nullptr,       nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 0C */ {{nullptr,       nullptr}, {nullptr,       nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 0D */ {{nullptr,       nullptr}, {nullptr,       nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 0E */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 0F */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH        C       S_DC_NTE C          S_DC_V1_12_2000_PROTO C          S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* 10 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 11 */ {{nullptr,       nullptr}, {nullptr,       nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 12 */ {{nullptr,       nullptr}, {nullptr,       nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 13 */ {{nullptr,       nullptr}, {nullptr,       nullptr}, {nullptr, nullptr}, {S_13_A7,          nullptr},     {S_13_A7,          nullptr},     {S_13_A7,          nullptr},      {S_13_A7,       nullptr},      {S_13_A7,       nullptr},      {S_13_A7,          nullptr},     {S_13_A7,          nullptr},      {S_13_A7,          nullptr},      {S_13_A7,          nullptr},      {S_13_A7,       nullptr},      {S_13_A7,      nullptr}},
/* 14 */ {{S_19_P_14,     nullptr}, {S_19_P_14,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 15 */ {{nullptr,       nullptr}, {nullptr,       nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 16 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 17 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_V123P_02_17,    nullptr},     {S_V123P_02_17,    nullptr},     {S_V123P_02_17,    nullptr},      {S_V123P_02_17, nullptr},      {S_V123P_02_17, nullptr},      {S_V123P_02_17,    nullptr},     {S_V123P_02_17,    nullptr},      {S_V123P_02_17,    nullptr},      {S_V123P_02_17,    nullptr},      {S_V123P_02_17, nullptr},      {S_invalid,    nullptr}},
/* 18 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {S_invalid,    nullptr}},
/* 19 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_19_P_14,        nullptr},     {S_19_P_14,        nullptr},     {S_19_P_14,        nullptr},      {S_19_P_14,     nullptr},      {S_19_P_14,     nullptr},      {S_19_P_14,        nullptr},     {S_19_P_14,        nullptr},      {S_19_P_14,        nullptr},      {S_19_P_14,        nullptr},      {S_19_P_14,     nullptr},      {S_19_P_14,    nullptr}},
/* 1A */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {S_V3_1A_D5,       nullptr},      {S_V3_1A_D5,       nullptr},      {S_V3_1A_D5,       nullptr},      {S_V3_1A_D5,    nullptr},      {nullptr,      nullptr}},
/* 1B */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {S_invalid,    nullptr}},
/* 1C */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {S_invalid,    nullptr}},
/* 1D */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_1D,             C_1D},        {S_1D,             C_1D},        {S_1D,             C_1D},         {S_1D,          C_1D},         {S_1D,          C_1D},         {S_1D,             C_1D},        {S_1D,             C_1D},         {S_1D,             C_1D},         {S_1D,             C_1D},         {S_1D,          C_1D},         {S_1D,         C_1D}},
/* 1E */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 1F */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH        C       S_DC_NTE C          S_DC_V1_12_2000_PROTO C          S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* 20 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 21 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 22 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_B_22,       nullptr}},
/* 23 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* 24 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* 25 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* 26 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 27 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 28 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 29 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 2A */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 2B */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 2C */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 2D */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 2E */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 2F */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH        C       S_DC_NTE C          S_DC_V1_12_2000_PROTO C          S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* 30 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 31 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 32 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 33 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 34 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 35 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 36 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 37 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 38 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 39 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 3A */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 3B */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 3C */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 3D */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 3E */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 3F */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH        C       S_DC_NTE C          S_DC_V1_12_2000_PROTO C          S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* 40 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        C_40},        {S_invalid,        C_40},        {S_invalid,        C_40},         {S_invalid,     C_40},         {S_invalid,     C_40},         {S_invalid,        C_40},        {S_invalid,        C_40},         {S_invalid,        C_40},         {S_invalid,        C_40},         {S_invalid,     C_40},         {S_invalid,    C_40}},
/* 41 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_DGX_41,         nullptr},     {S_DGX_41,         nullptr},     {S_DGX_41,         nullptr},      {S_P_41,        nullptr},      {S_P_41,        nullptr},      {S_DGX_41,         nullptr},     {S_DGX_41,         nullptr},      {S_DGX_41,         nullptr},      {S_DGX_41,         nullptr},      {S_DGX_41,      nullptr},      {S_B_41,       nullptr}},
/* 42 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 43 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 44 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_D_44_A6,        nullptr},     {S_D_44_A6,        nullptr},     {S_D_44_A6,        nullptr},      {S_PG_44_A6,    nullptr},      {S_PG_44_A6,    nullptr},      {S_D_44_A6,        nullptr},     {S_PG_44_A6,       nullptr},      {S_PG_44_A6,       nullptr},      {S_PG_44_A6,       nullptr},      {S_X_44_A6,     nullptr},      {S_B_44_A6,    nullptr}},
/* 45 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 46 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 47 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 48 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 49 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 4A */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 4B */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 4C */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 4D */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 4E */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 4F */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH        C       S_DC_NTE C          S_DC_V1_12_2000_PROTO C          S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* 50 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 51 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 52 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 53 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 54 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 55 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 56 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 57 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 58 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 59 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 5A */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 5B */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 5C */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 5D */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 5E */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 5F */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH        C       S_DC_NTE C          S_DC_V1_12_2000_PROTO C          S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* 60 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_6x,             C_D_6x},      {S_6x,             C_D_6x},      {S_6x,             C_D_6x},       {S_6x,          C_P_6x},       {S_6x,          C_P_6x},       {S_6x,             C_D_6x},      {S_6x,             C_G_6x},       {S_6x,             C_G_6x},       {S_6x,             C_G_6x},       {S_6x,          C_X_6x},       {S_6x,         C_B_6x}},
/* 61 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        C_GXB_61},     {S_invalid,        C_GXB_61},     {S_invalid,        C_GXB_61},     {S_invalid,     C_GXB_61},     {S_invalid,    C_GXB_61}},
/* 62 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_6x,             C_D_6x},      {S_6x,             C_D_6x},      {S_6x,             C_D_6x},       {S_6x,          C_P_6x},       {S_6x,          C_P_6x},       {S_6x,             C_D_6x},      {S_6x,             C_G_6x},       {S_6x,             C_G_6x},       {S_6x,             C_G_6x},       {S_6x,          C_X_6x},       {S_6x,         C_B_6x}},
/* 63 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 64 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_D_64,           nullptr},     {S_D_64,           nullptr},     {S_D_64,           nullptr},      {S_P_64,        nullptr},      {S_P_64,        nullptr},      {S_G_64,           nullptr},     {S_G_64,           nullptr},      {S_G_64,           nullptr},      {S_G_64,           nullptr},      {S_X_64,        nullptr},      {S_B_64,       nullptr}},
/* 65 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_DG_65_67_68_EB, nullptr},     {S_DG_65_67_68_EB, nullptr},     {S_DG_65_67_68_EB, nullptr},      {S_P_65_67_68,  nullptr},      {S_P_65_67_68,  nullptr},      {S_DG_65_67_68_EB, nullptr},     {S_DG_65_67_68_EB, nullptr},      {S_DG_65_67_68_EB, nullptr},      {S_DG_65_67_68_EB, nullptr},      {S_X_65_67_68,  nullptr},      {S_B_65_67_68, nullptr}},
/* 66 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_66_69_E9,       nullptr},     {S_66_69_E9,       nullptr},     {S_66_69_E9,       nullptr},      {S_66_69_E9,    nullptr},      {S_66_69_E9,    nullptr},      {S_66_69_E9,       nullptr},     {S_66_69_E9,       nullptr},      {S_66_69_E9,       nullptr},      {S_66_69_E9,       nullptr},      {S_66_69_E9,    nullptr},      {S_66_69_E9,   nullptr}},
/* 67 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_DG_65_67_68_EB, nullptr},     {S_DG_65_67_68_EB, nullptr},     {S_DG_65_67_68_EB, nullptr},      {S_P_65_67_68,  nullptr},      {S_P_65_67_68,  nullptr},      {S_DG_65_67_68_EB, nullptr},     {S_DG_65_67_68_EB, nullptr},      {S_DG_65_67_68_EB, nullptr},      {S_DG_65_67_68_EB, nullptr},      {S_X_65_67_68,  nullptr},      {S_B_65_67_68, nullptr}},
/* 68 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_DG_65_67_68_EB, nullptr},     {S_DG_65_67_68_EB, nullptr},     {S_DG_65_67_68_EB, nullptr},      {S_P_65_67_68,  nullptr},      {S_P_65_67_68,  nullptr},      {S_DG_65_67_68_EB, nullptr},     {S_DG_65_67_68_EB, nullptr},      {S_DG_65_67_68_EB, nullptr},      {S_DG_65_67_68_EB, nullptr},      {S_X_65_67_68,  nullptr},      {S_B_65_67_68, nullptr}},
/* 69 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_66_69_E9,       nullptr},     {S_66_69_E9,       nullptr},     {S_66_69_E9,       nullptr},      {S_66_69_E9,    nullptr},      {S_66_69_E9,    nullptr},      {S_66_69_E9,       nullptr},     {S_66_69_E9,       nullptr},      {S_66_69_E9,       nullptr},      {S_66_69_E9,       nullptr},      {S_66_69_E9,    nullptr},      {S_66_69_E9,   nullptr}},
/* 6A */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 6B */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 6C */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_6x,             C_D_6x},      {S_6x,             C_D_6x},      {S_6x,             C_D_6x},       {S_6x,          C_P_6x},       {S_6x,          C_P_6x},       {S_6x,             C_D_6x},      {S_6x,             C_G_6x},       {S_6x,             C_G_6x},       {S_6x,             C_G_6x},       {S_6x,          C_X_6x},       {S_6x,         C_B_6x}},
/* 6D */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_6x,             C_D_6x},      {S_6x,             C_D_6x},      {S_6x,             C_D_6x},       {S_6x,          C_P_6x},       {S_6x,          C_P_6x},       {S_6x,             C_D_6x},      {S_6x,             C_G_6x},       {S_6x,             C_G_6x},       {S_6x,             C_G_6x},       {S_6x,          C_X_6x},       {S_6x,         C_B_6x}},
/* 6E */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 6F */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH        C       S_DC_NTE C          S_DC_V1_12_2000_PROTO C          S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* 70 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 71 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 72 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 73 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 74 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 75 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 76 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 77 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 78 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 79 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 7A */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 7B */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 7C */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 7D */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 7E */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 7F */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH        C       S_DC_NTE C          S_DC_V1_12_2000_PROTO C          S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* 80 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 81 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_DGX_81,         C_DGX_81},    {S_DGX_81,         C_DGX_81},    {S_DGX_81,         C_DGX_81},     {S_P_81,        C_P_81},       {S_P_81,        C_P_81},       {S_DGX_81,         C_DGX_81},    {S_DGX_81,         C_DGX_81},     {S_DGX_81,         C_DGX_81},     {S_DGX_81,         C_DGX_81},     {S_DGX_81,      C_DGX_81},     {S_B_81,       C_B_81}},
/* 82 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 83 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 84 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 85 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 86 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 87 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 88 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_88,             nullptr},     {S_88,             nullptr},     {S_88,             nullptr},      {S_88,          nullptr},      {S_88,          nullptr},      {S_88,             nullptr},     {S_88,             nullptr},      {S_88,             nullptr},      {S_88,             nullptr},      {S_88,          nullptr},      {S_88,         nullptr}},
/* 89 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 8A */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 8B */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 8C */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 8D */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 8E */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 8F */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH        C       S_DC_NTE C          S_DC_V1_12_2000_PROTO C          S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* 90 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 91 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 92 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 93 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 94 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 95 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 96 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 97 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_97,             nullptr},     {S_97,             nullptr},     {S_97,             nullptr},      {S_97,          nullptr},      {S_97,          nullptr},      {S_97,             nullptr},     {S_97,             nullptr},      {S_97,             nullptr},      {S_97,             nullptr},      {S_97,          nullptr},      {nullptr,      nullptr}},
/* 98 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        C_98},        {S_invalid,        C_98},        {S_invalid,        C_98},         {S_invalid,     C_98},         {S_invalid,     C_98},         {S_invalid,        C_98},        {S_invalid,        C_98},         {S_invalid,        C_98},         {S_invalid,        C_98},         {S_invalid,     C_98},         {S_invalid,    C_98}},
/* 99 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 9A */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {S_G_9A,           nullptr},      {S_G_9A,           nullptr},      {S_G_9A,           nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 9B */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 9C */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 9D */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 9E */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        C_G_9E},       {S_invalid,        C_G_9E},       {S_invalid,        C_G_9E},       {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 9F */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH        C       S_DC_NTE C          S_DC_V1_12_2000_PROTO C          S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* A0 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          C_V123_A0_A1},{nullptr,          C_V123_A0_A1},{nullptr,          C_V123_A0_A1}, {nullptr,       C_V123_A0_A1}, {nullptr,       C_V123_A0_A1}, {nullptr,          C_V123_A0_A1},{nullptr,          C_V123_A0_A1}, {nullptr,          C_V123_A0_A1}, {nullptr,          C_V123_A0_A1}, {nullptr,       C_V123_A0_A1}, {nullptr,      nullptr}},
/* A1 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          C_V123_A0_A1},{nullptr,          C_V123_A0_A1},{nullptr,          C_V123_A0_A1}, {nullptr,       C_V123_A0_A1}, {nullptr,       C_V123_A0_A1}, {nullptr,          C_V123_A0_A1},{nullptr,          C_V123_A0_A1}, {nullptr,          C_V123_A0_A1}, {nullptr,          C_V123_A0_A1}, {nullptr,       C_V123_A0_A1}, {nullptr,      nullptr}},
/* A2 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* A3 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* A4 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* A5 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* A6 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_D_44_A6,        nullptr},     {S_D_44_A6,        nullptr},     {S_D_44_A6,        nullptr},      {S_PG_44_A6,    nullptr},      {S_PG_44_A6,    nullptr},      {S_D_44_A6,        nullptr},     {S_PG_44_A6,       nullptr},      {S_PG_44_A6,       nullptr},      {S_PG_44_A6,       nullptr},      {S_X_44_A6,     nullptr},      {S_B_44_A6,    nullptr}},
/* A7 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_13_A7,          nullptr},     {S_13_A7,          nullptr},     {S_13_A7,          nullptr},      {S_13_A7,       nullptr},      {S_13_A7,       nullptr},      {S_13_A7,          nullptr},     {S_13_A7,          nullptr},      {S_13_A7,          nullptr},      {S_13_A7,          nullptr},      {S_13_A7,       nullptr},      {S_13_A7,      nullptr}},
/* A8 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* A9 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* AA */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* AB */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* AC */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_AC,             nullptr},      {S_AC,             nullptr},      {S_AC,             nullptr},      {S_AC,          nullptr},      {S_AC,         nullptr}},
/* AD */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* AE */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* AF */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH        C       S_DC_NTE C          S_DC_V1_12_2000_PROTO C          S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* B0 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* B1 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {S_B1,             nullptr},     {S_B1,             nullptr},      {S_B1,          nullptr},      {S_B1,          nullptr},      {S_B1,             nullptr},     {S_B1,             nullptr},      {S_B1,             nullptr},      {S_B1,             nullptr},      {S_B1,          nullptr},      {S_B1,         nullptr}},
/* B2 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {S_B2<false>,      nullptr},      {S_B2<false>,   nullptr},      {S_B2<false>,   nullptr},      {S_B2<true>,       nullptr},     {S_B2<true>,       nullptr},      {S_B2<true>,       nullptr},      {S_B2<true>,       nullptr},      {S_B2<false>,   nullptr},      {S_B2<false>,  nullptr}},
/* B3 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        C_B3},         {S_invalid,     C_B3},         {S_invalid,     C_B3},         {S_invalid,        C_B3},        {S_invalid,        C_B3},         {S_invalid,        C_B3},         {S_invalid,        C_B3},         {S_invalid,     C_B3},         {S_invalid,    C_B3}},
/* B4 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* B5 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* B6 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* B7 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_G_B7,           nullptr},      {S_G_B7,           nullptr},      {S_G_B7,           nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* B8 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_G_B8,           nullptr},      {S_G_B8,           nullptr},      {S_G_B8,           nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* B9 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_G_B9,           nullptr},      {S_G_B9,           nullptr},      {S_G_B9,           nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* BA */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_G_BA,           nullptr},      {S_G_BA,           nullptr},      {S_G_BA,           nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* BB */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* BC */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* BD */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* BE */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* BF */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH        C       S_DC_NTE C          S_DC_V1_12_2000_PROTO C          S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* C0 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {nullptr,          nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* C1 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* C2 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* C3 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* C4 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_DGX_C4,         nullptr},      {S_P_C4,        nullptr},      {S_P_C4,        nullptr},      {S_DGX_C4,         nullptr},     {S_DGX_C4,         nullptr},      {S_DGX_C4,         nullptr},      {S_DGX_C4,         nullptr},      {S_DGX_C4,      nullptr},      {S_B_C4,       nullptr}},
/* C5 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* C6 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* C7 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* C8 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* C9 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_6x,             nullptr},      {S_6x,             nullptr},      {S_6x,             nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* CA */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* CB */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_6x,             nullptr},      {S_6x,             nullptr},      {S_6x,             nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* CC */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* CD */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* CE */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* CF */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH        C       S_DC_NTE C          S_DC_V1_12_2000_PROTO C          S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* D0 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* D1 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* D2 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* D3 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* D4 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* D5 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_V3_1A_D5,       nullptr},      {S_V3_1A_D5,       nullptr},      {S_V3_1A_D5,       nullptr},      {S_V3_1A_D5,    nullptr},      {nullptr,      nullptr}},
/* D6 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* D7 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* D8 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* D9 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        C_GX_D9},      {S_invalid,        C_GX_D9},      {S_invalid,        C_GX_D9},      {S_invalid,     C_GX_D9},      {S_invalid,    C_B_D9}},
/* DA */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_V3_BB_DA,       nullptr},      {S_V3_BB_DA,       nullptr},      {S_V3_BB_DA,       nullptr},      {S_V3_BB_DA,    nullptr},      {S_V3_BB_DA,   nullptr}},
/* DB */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* DC */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* DD */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* DE */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* DF */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH        C       S_DC_NTE C          S_DC_V1_12_2000_PROTO C          S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* E0 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* E1 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* E2 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* E3 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* E4 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_G_E4,           nullptr},      {S_G_E4,           nullptr},      {S_G_E4,           nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* E5 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* E6 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* E7 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {S_invalid,     nullptr},      {S_B_E7,       nullptr}},
/* E8 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_E8,             nullptr},      {S_E8,             nullptr},      {S_E8,             nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* E9 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_66_69_E9,       nullptr},      {S_66_69_E9,       nullptr},      {S_66_69_E9,       nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* EA */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* EB */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_DG_65_67_68_EB, nullptr},      {S_DG_65_67_68_EB, nullptr},      {S_DG_65_67_68_EB, nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* EC */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* ED */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* EE */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* EF */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_G_EF,           nullptr},      {S_G_EF,           nullptr},      {S_G_EF,           nullptr},      {S_invalid,     nullptr},      {S_B_EF,       nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH        C       S_DC_NTE C          S_DC_V1_12_2000_PROTO C          S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* F0 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* F1 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* F2 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* F3 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* F4 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* F5 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* F6 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* F7 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* F8 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* F9 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* FA */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* FB */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* FC */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* FD */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* FE */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* FF */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr, nullptr}, {S_invalid,        nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH        C       S_DC_NTE C          S_DC_V1_12_2000_PROTO C          S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
    // clang-format on
};

static on_command_t get_handler(Version version, bool from_server, uint8_t command) {
  size_t version_index = static_cast<size_t>(version);
  if (version_index >= sizeof(handlers[0]) / sizeof(handlers[0][0])) {
    throw logic_error("invalid game version on proxy server");
  }
  auto ret = handlers[command][version_index][!from_server];
  return ret ? ret : default_handler;
}

void on_proxy_command(
    shared_ptr<ProxyServer::LinkedSession> ses,
    bool from_server,
    uint16_t command,
    uint32_t flag,
    string& data) {
  try {
    auto fn = get_handler(ses->version(), from_server, command);
    auto res = fn(ses, command, flag, data);
    if (res.type == HandlerResult::Type::FORWARD) {
      forward_command(ses, !from_server, command, flag, data, false);
    } else if (res.type == HandlerResult::Type::MODIFIED) {
      ses->log.info("The preceding command from the %s was modified in transit",
          from_server ? "server" : "client");
      forward_command(
          ses,
          !from_server,
          res.new_command >= 0 ? res.new_command : command,
          res.new_flag >= 0 ? res.new_flag : flag,
          data);
    } else if (res.type == HandlerResult::Type::SUPPRESS) {
      ses->log.info("The preceding command from the %s was not forwarded",
          from_server ? "server" : "client");
    } else {
      throw logic_error("invalid handler result");
    }
  } catch (const exception& e) {
    ses->log.error("Failed to process command: %s", e.what());
    if (from_server) {
      string error_str = "Error: ";
      error_str += e.what();
      ses->send_to_game_server(error_str.c_str());
    } else {
      ses->disconnect();
    }
  }
}
