#include "ProxyCommands.hh"

#include <ctype.h>
#include <errno.h>
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
#include <resource_file/Emulators/PPC32Emulator.hh>
#include <resource_file/Emulators/SH4Emulator.hh>
#include <resource_file/Emulators/X86Emulator.hh>

#include "ChatCommands.hh"
#include "Compression.hh"
#include "ImageEncoder.hh"
#include "Loggers.hh"
#include "PSOProtocol.hh"
#include "ReceiveCommands.hh"
#include "ReceiveSubcommands.hh"
#include "SendCommands.hh"

using namespace std;

enum class HandlerResult {
  FORWARD = 0,
  SUPPRESS,
  MODIFIED,
};

typedef asio::awaitable<HandlerResult> (*on_message_t)(shared_ptr<Client> c, Channel::Message& msg);

static void forward_command(shared_ptr<Client> c, bool to_server, const Channel::Message& msg, bool print_contents = true) {
  auto ch = to_server ? (c->proxy_session ? c->proxy_session->server_channel : nullptr) : c->channel;
  if (!ch || !ch->connected()) {
    proxy_server_log.warning_f("No endpoint is present; dropping command");
  } else {
    ch->send(msg.command, msg.flag, msg.data, !print_contents);
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

static asio::awaitable<HandlerResult> default_handler(shared_ptr<Client>, Channel::Message&) {
  co_return HandlerResult::FORWARD;
}

static asio::awaitable<HandlerResult> S_invalid(shared_ptr<Client> c, Channel::Message& msg) {
  c->log.error_f("Server sent invalid command");
  string error_str = is_v4(c->version())
      ? std::format("Server sent invalid\ncommand: {:04X} {:08X}", msg.command, msg.flag)
      : std::format("Server sent invalid\ncommand: {:02X} {:02X}", msg.command, msg.flag);
  c->proxy_session->server_channel->disconnect();
  co_return HandlerResult::SUPPRESS;
}

static asio::awaitable<HandlerResult> C_1D(shared_ptr<Client> c, Channel::Message&) {
  if (c->ping_start_time) {
    uint64_t ping_usecs = phosg::now() - c->ping_start_time;
    c->ping_start_time = 0;
    double ping_ms = static_cast<double>(ping_usecs) / 1000.0;
    send_text_message_fmt(c->channel, "To proxy: {:g}ms", ping_ms);
  }
  co_return HandlerResult::SUPPRESS;
}

static asio::awaitable<HandlerResult> S_1D(shared_ptr<Client> c, Channel::Message&) {
  c->proxy_session->server_channel->send(0x1D);
  co_return HandlerResult::SUPPRESS;
}

static asio::awaitable<HandlerResult> S_97(shared_ptr<Client> c, Channel::Message&) {
  // We always assume a 97 has already been received by the client - we should
  // have sent 97 01 before sending the client to the proxy server.
  c->proxy_session->server_channel->send(0xB1, 0x00);
  co_return HandlerResult::SUPPRESS;
}

static void send_90_to_server(std::shared_ptr<Client> c) {
  C_LoginV1_DC_PC_V3_90 cmd;
  cmd.serial_number.encode(c->serial_number);
  cmd.access_key.encode(c->access_key);
  c->proxy_session->server_channel->send(0x90, 0x00, &cmd, sizeof(cmd));
}

static void send_93_to_server(std::shared_ptr<Client> c) {
  C_LoginV1_DC_93 cmd;
  if (c->proxy_session->remote_guild_card_number < 0) {
    cmd.player_tag = 0xFFFF0000;
    cmd.guild_card_number = 0xFFFFFFFF;
  } else {
    cmd.player_tag = 0x00010000;
    cmd.guild_card_number = c->proxy_session->remote_guild_card_number;
  }
  cmd.hardware_id = c->hardware_id;
  cmd.sub_version = c->sub_version;
  cmd.is_extended = 0;
  cmd.language = c->language();
  cmd.serial_number.encode(c->serial_number);
  cmd.access_key.encode(c->access_key);
  cmd.serial_number2.encode(c->serial_number2);
  cmd.access_key2.encode(c->access_key2);
  cmd.login_character_name.encode(c->login_character_name, c->language());
  c->proxy_session->server_channel->send(0x93, 0x00, &cmd, sizeof(cmd));
}

static void send_9A_to_server(std::shared_ptr<Client> c) {
  C_Login_DC_PC_V3_9A cmd;
  cmd.v1_serial_number.encode(c->v1_serial_number);
  cmd.v1_access_key.encode(c->v1_access_key);
  cmd.serial_number.encode(c->serial_number);
  cmd.access_key.encode(c->access_key);
  if (c->proxy_session->remote_guild_card_number < 0) {
    cmd.player_tag = 0xFFFF0000;
    cmd.guild_card_number = 0xFFFFFFFF;
  } else {
    cmd.player_tag = 0x00010000;
    cmd.guild_card_number = c->proxy_session->remote_guild_card_number;
  }
  cmd.sub_version = c->sub_version;
  cmd.serial_number2.encode(c->serial_number2);
  cmd.access_key2.encode(c->access_key2);
  cmd.email_address.encode(c->email_address);
  c->proxy_session->server_channel->send(0x9A, 0x00, &cmd, sizeof(cmd));
}

static void send_9D_to_server(std::shared_ptr<Client> c) {
  C_Login_DC_PC_GC_9D cmd;
  if (c->proxy_session->remote_guild_card_number < 0) {
    cmd.player_tag = 0xFFFF0000;
    cmd.guild_card_number = 0xFFFFFFFF;
  } else {
    cmd.player_tag = 0x00010000;
    cmd.guild_card_number = c->proxy_session->remote_guild_card_number;
  }
  cmd.hardware_id = c->hardware_id;
  cmd.sub_version = c->sub_version;
  cmd.is_extended = 0;
  cmd.language = c->language();
  cmd.v1_serial_number.encode(c->v1_serial_number);
  cmd.v1_access_key.encode(c->v1_access_key);
  cmd.serial_number.encode(c->serial_number);
  cmd.access_key.encode(c->access_key);
  cmd.serial_number2.encode(c->serial_number2);
  cmd.access_key2.encode(c->access_key2);
  cmd.login_character_name.encode(c->login_character_name, c->language());
  c->proxy_session->server_channel->send(0x9D, 0x00, &cmd, sizeof(cmd));
}

static void send_DB_to_server(std::shared_ptr<Client> c) {
  C_VerifyAccount_V3_DB cmd;
  cmd.v1_serial_number.encode(c->v1_serial_number);
  cmd.v1_access_key.encode(c->v1_access_key);
  cmd.serial_number.encode(c->serial_number);
  cmd.access_key.encode(c->access_key);
  cmd.hardware_id = c->hardware_id;
  cmd.sub_version = c->sub_version;
  cmd.serial_number2.encode(c->serial_number2);
  cmd.access_key2.encode(c->access_key2);
  cmd.password.encode(c->password);
  c->proxy_session->server_channel->send(0xDB, 0x00, &cmd, sizeof(cmd));
}

static void send_9E_XB_to_server(std::shared_ptr<Client> c) {
  C_LoginExtended_XB_9E cmd;
  if (c->proxy_session->remote_guild_card_number < 0) {
    cmd.player_tag = 0xFFFF0000;
    cmd.guild_card_number = 0xFFFFFFFF;
  } else {
    cmd.player_tag = 0x00010000;
    cmd.guild_card_number = c->proxy_session->remote_guild_card_number;
  }
  cmd.hardware_id = c->hardware_id;
  cmd.sub_version = c->sub_version;
  cmd.is_extended = (c->proxy_session->remote_guild_card_number < 0) ? 1 : 0;
  cmd.language = c->language();
  cmd.v1_serial_number.encode(c->v1_serial_number);
  cmd.v1_access_key.encode(c->v1_access_key);
  cmd.serial_number.encode(c->serial_number);
  cmd.access_key.encode(c->access_key);
  cmd.serial_number2.encode(c->serial_number2);
  cmd.access_key2.encode(c->access_key2);
  cmd.login_character_name.encode(c->login_character_name, c->language());
  cmd.xb_netloc = c->xb_netloc;
  cmd.xb_unknown_a1a = c->xb_unknown_a1a;
  cmd.xb_user_id_high = (c->xb_user_id >> 32) & 0xFFFFFFFF;
  cmd.xb_user_id_low = c->xb_user_id & 0xFFFFFFFF;
  cmd.xb_unknown_a1b = c->xb_unknown_a1b;
  c->proxy_session->server_channel->send(0x9E, 0x01, &cmd, sizeof(C_LoginExtended_XB_9E));
}

static asio::awaitable<HandlerResult> S_G_9A(shared_ptr<Client> c, Channel::Message&) {
  // TODO: Either delete this handler or finish implementing it (flag=00/02
  // should do the below, 01 should send 9C, anything else should end the
  // session)
  C_LoginExtended_GC_9E cmd;
  if (c->proxy_session->remote_guild_card_number < 0) {
    cmd.player_tag = 0xFFFF0000;
    cmd.guild_card_number = 0xFFFFFFFF;
  } else {
    cmd.player_tag = 0x00010000;
    cmd.guild_card_number = c->proxy_session->remote_guild_card_number;
  }
  cmd.hardware_id = c->hardware_id;
  cmd.sub_version = c->sub_version;
  cmd.is_extended = (c->proxy_session->remote_guild_card_number < 0) ? 1 : 0;
  cmd.language = c->language();
  cmd.v1_serial_number.encode(c->v1_serial_number);
  cmd.v1_access_key.encode(c->v1_access_key);
  cmd.serial_number.encode(c->serial_number);
  cmd.access_key.encode(c->access_key);
  cmd.serial_number2.encode(c->serial_number2);
  cmd.access_key2.encode(c->access_key2);
  cmd.login_character_name.encode(c->login_character_name, c->language());
  cmd.client_config = c->proxy_session->remote_client_config_data;

  // If there's a guild card number, a shorter 9E is sent that ends
  // right after the client config data
  c->proxy_session->server_channel->send(
      0x9E, 0x01, &cmd,
      cmd.is_extended ? sizeof(C_LoginExtended_GC_9E) : sizeof(C_Login_PC_GC_9E));

  co_return HandlerResult::SUPPRESS;
}

static asio::awaitable<HandlerResult> S_V123U_02_17(shared_ptr<Client> c, Channel::Message& msg) {
  if (is_patch(c->version()) && msg.command == 0x17) {
    throw invalid_argument("patch server sent 17 server init");
  }

  // Most servers don't include after_message or have a shorter
  // after_message than newserv does, so don't require it
  const auto& cmd = msg.check_size_t<S_ServerInitDefault_DC_PC_V3_02_17_91_9B>(0xFFFF);

  // This isn't forwarded to the client, so don't recreate the client's crypts
  if (uses_v3_encryption(c->version())) {
    c->proxy_session->server_channel->crypt_in = make_shared<PSOV3Encryption>(cmd.server_key);
    c->proxy_session->server_channel->crypt_out = make_shared<PSOV3Encryption>(cmd.client_key);
  } else {
    c->proxy_session->server_channel->crypt_in = make_shared<PSOV2Encryption>(cmd.server_key);
    c->proxy_session->server_channel->crypt_out = make_shared<PSOV2Encryption>(cmd.client_key);
  }

  // Respond with an appropriate login command. We don't let the client do this
  // because it believes it already did (when it was in an unlinked session, or
  // in the patch server case, during the current session due to a hidden
  // redirect).
  switch (c->version()) {
    case Version::PC_PATCH:
    case Version::BB_PATCH:
      c->proxy_session->server_channel->send(0x02);
      co_return HandlerResult::SUPPRESS;

    case Version::DC_NTE:
      // TODO
      throw runtime_error("DC NTE proxy is not implemented");

    case Version::DC_11_2000:
    case Version::DC_V1:
      if (msg.command == 0x17) {
        send_90_to_server(c);
        co_return HandlerResult::SUPPRESS;
      } else {
        send_93_to_server(c);
        co_return HandlerResult::SUPPRESS;
      }
      break;

    case Version::DC_V2:
    case Version::PC_NTE:
    case Version::PC_V2:
    case Version::GC_NTE: {
      if (msg.command == 0x17) {
        send_9A_to_server(c);
        co_return HandlerResult::SUPPRESS;
      } else {
        send_9D_to_server(c);
        co_return HandlerResult::SUPPRESS;
      }
      break;
    }

    case Version::GC_V3:
    case Version::GC_EP3_NTE:
    case Version::GC_EP3:
      if (msg.command == 0x17) {
        send_DB_to_server(c);
        co_return HandlerResult::SUPPRESS;
      } else {
        // For command 02, send the same as if we had received 9A from the server
        co_return co_await S_G_9A(c, msg);
      }
      throw logic_error("GC init command not handled");

    case Version::XB_V3: {
      send_9E_XB_to_server(c);
      co_return HandlerResult::SUPPRESS;
    }

    case Version::BB_V4:
      throw logic_error("v1/v2/v3 server init handler should not be called on BB");
    default:
      throw logic_error("invalid game version in server init handler");
  }
}

static asio::awaitable<HandlerResult> S_U_04(shared_ptr<Client> c, Channel::Message&) {
  C_Login_Patch_04 ret;
  ret.username.encode(c->username);
  ret.password.encode(c->password);
  ret.email_address.encode(c->email_address);
  c->proxy_session->server_channel->send(0x04, 0x00, &ret, sizeof(ret));
  co_return HandlerResult::SUPPRESS;
}

static asio::awaitable<HandlerResult> S_B_03(shared_ptr<Client> c, Channel::Message& msg) {
  // Most servers don't include after_message or have a shorter after_message
  // than newserv does, so don't require it
  const auto& cmd = msg.check_size_t<S_ServerInitDefault_BB_03_9B>(0xFFFF);

  // This isn't forwarded to the client, so only recreate the server's crypts.
  // Use the same crypt type as the client... the server has the luxury of
  // being able to try all the crypts it knows to detect what type the client
  // uses, but the client can't do this since it sends the first encrypted
  // data on the connection.
  if (!c->bb_detector_crypt) {
    throw logic_error("Client proxy session started with missing detector crypt");
  }
  c->proxy_session->server_channel->crypt_in = make_shared<PSOBBMultiKeyImitatorEncryption>(
      c->bb_detector_crypt, cmd.server_key.data(), sizeof(cmd.server_key), false);
  c->proxy_session->server_channel->crypt_out = make_shared<PSOBBMultiKeyImitatorEncryption>(
      c->bb_detector_crypt, cmd.client_key.data(), sizeof(cmd.client_key), false);

  C_LoginWithHardwareInfo_BB_93 resp;
  resp.guild_card_number = c->proxy_session->remote_guild_card_number;
  resp.sub_version = c->sub_version;
  resp.language = c->language();
  resp.character_slot = c->bb_character_index;
  resp.connection_phase = c->bb_connection_phase;
  resp.client_code = c->bb_client_code;
  resp.security_token = c->bb_security_token;
  resp.username.encode(c->username, c->language());
  resp.password.encode(c->password, c->language());
  resp.hardware_id = c->hardware_id;
  resp.client_config = c->bb_client_config;
  if (c->proxy_session->enable_remote_ip_crc_patch) {
    *reinterpret_cast<le_uint32_t*>(resp.client_config.data() + 0x10) =
        c->proxy_session->remote_ip_crc ^ (1309539928UL + 1248334810UL);
  }
  c->proxy_session->server_channel->send(0x93, 0x00, &resp, sizeof(resp));

  co_return HandlerResult::SUPPRESS;
}

static asio::awaitable<HandlerResult> S_B_E6(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& cmd = msg.check_size_t<S_ClientInit_BB_00E6>(0xFFFF);
  c->proxy_session->remote_guild_card_number = cmd.guild_card_number;
  c->bb_security_token = cmd.security_token;
  c->bb_client_config = cmd.client_config;

  auto s = c->require_server_state();
  auto& pc = s->proxy_persistent_configs[c->login->account->account_id];
  pc.account_id = c->login->account->account_id;
  pc.remote_guild_card_number = c->proxy_session->remote_guild_card_number;
  pc.enable_remote_ip_crc_patch = c->proxy_session->enable_remote_ip_crc_patch;
  c->log.info_f("Updated persistent config for proxy session");

  co_return HandlerResult::FORWARD;
}

static asio::awaitable<HandlerResult> S_V123_04(shared_ptr<Client> c, Channel::Message& msg) {
  // Suppress extremely short commands from the server instead of disconnecting
  if (msg.data.size() < offsetof(S_UpdateClientConfig_V3_04, client_config)) {
    le_uint64_t checksum = phosg::random_object<uint64_t>() & 0x0000FFFFFFFFFFFF;
    c->proxy_session->server_channel->send(0x96, 0x00, &checksum, sizeof(checksum));
    co_return HandlerResult::SUPPRESS;
  }

  // Some servers send a short 04 command if they don't use all of the 0x20
  // bytes available. We should be prepared to handle that.
  auto& cmd = msg.check_size_t<S_UpdateClientConfig_V3_04>(
      offsetof(S_UpdateClientConfig_V3_04, client_config),
      sizeof(S_UpdateClientConfig_V3_04));

  // If this is a logged-in session, hide the guild card number assigned by the
  // remote server so the client doesn't see it change. If this is a logged-out
  // session, then the client never received a guild card number from newserv
  // anyway, so we can let the client see the number from the remote server.
  bool had_guild_card_number = (c->proxy_session->remote_guild_card_number >= 0);
  if (c->proxy_session->remote_guild_card_number != cmd.guild_card_number) {
    c->proxy_session->remote_guild_card_number = cmd.guild_card_number;
    c->log.info_f("Remote guild card number set to {}", c->proxy_session->remote_guild_card_number);
    string message = std::format(
        "The remote server\nhas assigned your\nGuild Card number:\n$C6{}",
        c->proxy_session->remote_guild_card_number);
    send_ship_info(c->channel, message);
  }
  if (c->login) {
    cmd.guild_card_number = c->login->account->account_id;
  }

  // It seems the client ignores the length of the 04 command, and always copies
  // 0x20 bytes to its config data. So if the server sends a short 04 command,
  // part of the previous command ends up in the security data (usually part of
  // the copyright string from the server init command). We simulate that here.
  // If there was previously a guild card number, assume we got the lobby server
  // init text instead of the port map init text.
  memcpy(c->proxy_session->remote_client_config_data.data(),
      had_guild_card_number ? "t Lobby Server. Copyright SEGA E" : "t Port Map. Copyright SEGA Enter", 0x20);
  memcpy(c->proxy_session->remote_client_config_data.data(), &cmd.client_config,
      min<size_t>(msg.data.size() - offsetof(S_UpdateClientConfig_V3_04, client_config),
          c->proxy_session->remote_client_config_data.bytes()));

  // If the guild card number was not set, pretend (to the server) that this is
  // the first 04 command the client has received. The client responds with a 96
  // (checksum) in that case.
  if (!had_guild_card_number) {
    le_uint64_t checksum = phosg::random_object<uint64_t>() & 0x0000FFFFFFFFFFFF;
    c->proxy_session->server_channel->send(0x96, 0x00, &checksum, sizeof(checksum));
  }

  co_return c->login ? HandlerResult::MODIFIED : HandlerResult::FORWARD;
}

static asio::awaitable<HandlerResult> S_V123_06(shared_ptr<Client> c, Channel::Message& msg) {
  bool modified = false;
  if (c->login && c->login->account->account_id != c->proxy_session->remote_guild_card_number) {
    auto& cmd = msg.check_size_t<SC_TextHeader_01_06_11_B0_EE>(0xFFFF);
    if (cmd.guild_card_number == c->proxy_session->remote_guild_card_number) {
      cmd.guild_card_number = c->login->account->account_id;
      modified = true;
    }
  }

  // If the session is Ep3, and Unmask Whispers is on, and there's enough data,
  // and the message has private_flags, and the private_flags say that you
  // shouldn't see the message, then change the private_flags
  if (is_ep3(c->version()) &&
      c->check_flag(Client::Flag::PROXY_EP3_UNMASK_WHISPERS) &&
      (msg.data.size() >= 12) &&
      (msg.data[sizeof(SC_TextHeader_01_06_11_B0_EE)] != '\t') &&
      (msg.data[sizeof(SC_TextHeader_01_06_11_B0_EE)] & (1 << c->lobby_client_id))) {
    msg.data[sizeof(SC_TextHeader_01_06_11_B0_EE)] &= ~(1 << c->lobby_client_id);
    modified = true;
  }

  co_return modified ? HandlerResult::MODIFIED : HandlerResult::FORWARD;
}

template <typename CmdT>
static asio::awaitable<HandlerResult> S_41(shared_ptr<Client> c, Channel::Message& msg) {
  if (c->login) {
    auto& cmd = msg.check_size_t<CmdT>();
    if ((cmd.searcher_guild_card_number == c->proxy_session->remote_guild_card_number) &&
        (cmd.result_guild_card_number == c->proxy_session->remote_guild_card_number) &&
        c->proxy_session->server_ping_start_time) {
      uint64_t ping_usecs = phosg::now() - c->proxy_session->server_ping_start_time;
      c->proxy_session->server_ping_start_time = 0;
      double ping_ms = static_cast<double>(ping_usecs) / 1000.0;
      send_text_message_fmt(c->channel, "To server: {:g}ms", ping_ms);
      co_return HandlerResult::SUPPRESS;

    } else {
      bool modified = false;
      if (c->login->account->account_id != c->proxy_session->remote_guild_card_number) {
        if (cmd.searcher_guild_card_number == c->proxy_session->remote_guild_card_number) {
          cmd.searcher_guild_card_number = c->login->account->account_id;
          modified = true;
        }
        if (cmd.result_guild_card_number == c->proxy_session->remote_guild_card_number) {
          cmd.result_guild_card_number = c->login->account->account_id;
          modified = true;
        }
      }
      co_return modified ? HandlerResult::MODIFIED : HandlerResult::FORWARD;
    }
  } else {
    co_return HandlerResult::FORWARD;
  }
}

constexpr on_message_t S_DGX_41 = &S_41<S_GuildCardSearchResult_DC_V3_41>;
constexpr on_message_t S_P_41 = &S_41<S_GuildCardSearchResult_PC_41>;
constexpr on_message_t S_B_41 = &S_41<S_GuildCardSearchResult_BB_41>;

template <typename CmdT>
static asio::awaitable<HandlerResult> S_81(shared_ptr<Client> c, Channel::Message& msg) {
  bool modified = false;
  if (c->login && c->login->account->account_id != c->proxy_session->remote_guild_card_number) {
    auto& cmd = msg.check_size_t<CmdT>();
    if (cmd.from_guild_card_number == c->proxy_session->remote_guild_card_number) {
      cmd.from_guild_card_number = c->login->account->account_id;
      modified = true;
    }
    if (cmd.to_guild_card_number == c->proxy_session->remote_guild_card_number) {
      cmd.to_guild_card_number = c->login->account->account_id;
      modified = true;
    }
  }
  co_return modified ? HandlerResult::MODIFIED : HandlerResult::FORWARD;
}

constexpr on_message_t S_DGX_81 = &S_81<SC_SimpleMail_DC_V3_81>;
constexpr on_message_t S_P_81 = &S_81<SC_SimpleMail_PC_81>;
constexpr on_message_t S_B_81 = &S_81<SC_SimpleMail_BB_81>;

static asio::awaitable<HandlerResult> S_88(shared_ptr<Client> c, Channel::Message& msg) {
  // If the client isn't in the lobby, suppress the command (Ep3 can crash if
  // it receives this while loading; other versions probably also will crash)
  if (!c->proxy_session->is_in_lobby) {
    co_return HandlerResult::SUPPRESS;
  }

  bool modified = false;
  if (c->login && c->login->account->account_id != c->proxy_session->remote_guild_card_number) {
    size_t expected_size = sizeof(S_ArrowUpdateEntry_88) * msg.flag;
    auto* entries = &msg.check_size_t<S_ArrowUpdateEntry_88>(expected_size, expected_size);
    for (size_t x = 0; x < msg.flag; x++) {
      if (entries[x].guild_card_number == c->proxy_session->remote_guild_card_number) {
        entries[x].guild_card_number = c->login->account->account_id;
        modified = true;
      }
    }
  }
  co_return modified ? HandlerResult::MODIFIED : HandlerResult::FORWARD;
}

static asio::awaitable<HandlerResult> S_B1(shared_ptr<Client> c, Channel::Message&) {
  // Block all time updates from the remote server, so client's time remains
  // consistent
  c->proxy_session->server_channel->send(0x99, 0x00);
  co_return HandlerResult::SUPPRESS;
}

template <bool BE>
static asio::awaitable<HandlerResult> S_B2(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& cmd = msg.check_size_t<S_ExecuteCode_B2>(0xFFFF);

  if (cmd.code_size && c->check_flag(Client::Flag::PROXY_SAVE_FILES)) {
    uint64_t filename_timestamp = phosg::now();
    string code = msg.data.substr(sizeof(S_ExecuteCode_B2));

    if (c->check_flag(Client::Flag::ENCRYPTED_SEND_FUNCTION_CALL)) {
      phosg::StringReader r(code);
      bool is_big_endian = ::is_big_endian(c->version());
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
      code = msg.data.substr(sizeof(S_ExecuteCode_B2));
      if (code.size() < cmd.code_size) {
        code.resize(cmd.code_size);
      }
    }

    string output_filename = std::format("code.{}.bin", filename_timestamp);
    phosg::save_file(output_filename, msg.data);
    c->log.info_f("Wrote code from server to file {}", output_filename);

    using FooterT = RELFileFooterT<BE>;

    // TODO: Support SH-4 disassembly too
    bool is_ppc = ::is_ppc(c->version());
    bool is_x86 = ::is_x86(c->version());
    bool is_sh4 = ::is_sh4(c->version());
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
          labels.emplace(reloc_offset, std::format("reloc{}", x));
        }
        labels.emplace(footer.root_offset, "entry_ptr");
        labels.emplace(footer_offset, "footer");
        labels.emplace(r.pget<U32T<BE>>(footer.root_offset), "start");

        string disassembly;
        if (is_ppc) {
          disassembly = ResourceDASM::PPC32Emulator::disassemble(&r.pget<uint8_t>(0, code.size()), code.size(), 0, &labels);
        } else if (is_x86) {
          disassembly = ResourceDASM::X86Emulator::disassemble(&r.pget<uint8_t>(0, code.size()), code.size(), 0, &labels);
        } else if (is_sh4) {
          disassembly = ResourceDASM::SH4Emulator::disassemble(&r.pget<uint8_t>(0, code.size()), code.size(), 0, &labels);
        } else {
          // We shouldn't have entered the outer if statement if this happens
          throw logic_error("unsupported architecture");
        }

        output_filename = std::format("code.{}.txt", filename_timestamp);
        {
          auto f = phosg::fopen_unique(output_filename, "wt");
          phosg::fwrite_fmt(f.get(), "// code_size = 0x{:X}\n", cmd.code_size);
          phosg::fwrite_fmt(f.get(), "// checksum_addr = 0x{:X}\n", cmd.checksum_start);
          phosg::fwrite_fmt(f.get(), "// checksum_size = 0x{:X}\n", cmd.checksum_size);
          phosg::fwritex(f.get(), disassembly);
        }
        c->log.info_f("Wrote disassembly to file {}", output_filename);

      } catch (const exception& e) {
        c->log.info_f("Failed to disassemble code from server: {}", e.what());
      }
    }
  }

  if (c->check_flag(Client::Flag::PROXY_BLOCK_FUNCTION_CALLS)) {
    c->log.info_f("Blocking function call from server");
    C_ExecuteCodeResult_B3 cmd;
    cmd.return_value = 0xFFFFFFFF;
    cmd.checksum = 0x00000000;
    c->proxy_session->server_channel->send(0xB3, msg.flag, &cmd, sizeof(cmd));
    co_return HandlerResult::SUPPRESS;
  } else {
    c->function_call_response_queue.emplace_back(nullptr);
    co_return HandlerResult::FORWARD;
  }
}

static asio::awaitable<HandlerResult> C_B3(shared_ptr<Client> c, Channel::Message& msg) {
  auto cmd = msg.check_size_t<C_ExecuteCodeResult_B3>();

  shared_ptr<AsyncPromise<C_ExecuteCodeResult_B3>> promise;
  if (!c->function_call_response_queue.empty()) {
    promise = std::move(c->function_call_response_queue.front());
    c->function_call_response_queue.pop_front();
  }

  if (promise) {
    promise->set_value(std::move(cmd));
    co_return HandlerResult::SUPPRESS;
  } else {
    co_return HandlerResult::FORWARD;
  }
}

static asio::awaitable<HandlerResult> C_B_E0(shared_ptr<Client> c, Channel::Message&) {
  auto ret = c->proxy_session->bb_client_sent_E0 ? HandlerResult::FORWARD : HandlerResult::SUPPRESS;
  c->proxy_session->bb_client_sent_E0 = true;
  co_return ret;
}

static asio::awaitable<HandlerResult> S_B_E2(shared_ptr<Client> c, Channel::Message& msg) {
  if (c->check_flag(Client::Flag::PROXY_SAVE_FILES)) {
    string output_filename = std::format("system.{}.psosys", phosg::now());
    phosg::save_object_file<PSOBBBaseSystemFile>(output_filename, msg.check_size_t<PSOBBBaseSystemFile>());
    c->log.info_f("Wrote system file to {}", output_filename);
  }
  co_return HandlerResult::FORWARD;
}

static asio::awaitable<HandlerResult> S_B_E7(shared_ptr<Client> c, Channel::Message& msg) {
  if (c->check_flag(Client::Flag::PROXY_SAVE_FILES)) {
    string output_filename = std::format("player.{}.psochar", phosg::now());
    auto f = phosg::fopen_unique(output_filename, "wb");
    PSOCommandHeaderBB header = {msg.data.size() + sizeof(PSOCommandHeaderBB), msg.command, msg.flag};
    phosg::fwritex(f.get(), &header, sizeof(header));
    phosg::fwritex(f.get(), msg.data);
    c->log.info_f("Wrote player data to {}", output_filename);
  }
  co_return HandlerResult::FORWARD;
}

template <typename CmdT>
static asio::awaitable<HandlerResult> S_C4(shared_ptr<Client> c, Channel::Message& msg) {
  bool modified = false;
  if (c->login && c->login->account->account_id != c->proxy_session->remote_guild_card_number) {
    size_t expected_size = sizeof(CmdT) * msg.flag;
    // Some servers (e.g. Schtserv) send extra data on the end of this command;
    // the client ignores it so we can ignore it too
    auto* entries = &msg.check_size_t<CmdT>(expected_size, 0xFFFF);
    for (size_t x = 0; x < msg.flag; x++) {
      if (entries[x].guild_card_number == c->proxy_session->remote_guild_card_number) {
        entries[x].guild_card_number = c->login->account->account_id;
        modified = true;
      }
    }
  }
  co_return modified ? HandlerResult::MODIFIED : HandlerResult::FORWARD;
}

constexpr on_message_t S_DGX_C4 = &S_C4<S_ChoiceSearchResultEntry_DC_V3_C4>;
constexpr on_message_t S_P_C4 = &S_C4<S_ChoiceSearchResultEntry_PC_C4>;
constexpr on_message_t S_B_C4 = &S_C4<S_ChoiceSearchResultEntry_BB_C4>;

static asio::awaitable<HandlerResult> S_G_E4(shared_ptr<Client> c, Channel::Message& msg) {
  auto& cmd = msg.check_size_t<S_CardBattleTableState_Ep3_E4>();
  bool modified = false;
  if (c->login && c->login->account->account_id != c->proxy_session->remote_guild_card_number) {
    for (size_t x = 0; x < 4; x++) {
      if (cmd.entries[x].guild_card_number == c->proxy_session->remote_guild_card_number) {
        cmd.entries[x].guild_card_number = c->login->account->account_id;
        modified = true;
      }
    }
  }
  co_return modified ? HandlerResult::MODIFIED : HandlerResult::FORWARD;
}

static asio::awaitable<HandlerResult> S_B_22(shared_ptr<Client> c, Channel::Message& msg) {
  // We use this command (which is sent before the init encryption command) to
  // detect a particular server behavior that we'll have to work around later.
  // It looks like this command's existence is an anti-proxy measure, since
  // this command is 0x34 bytes in total, and the logic that adds padding bytes
  // when the command size isn't a multiple of 8 is only active when encryption
  // is enabled. Presumably some simpler proxies would get this wrong.
  // Editor's note: There's an unsavory message in this command's data field,
  // hence the hash here instead of a direct string comparison. I'd love to
  // hear the story behind why they put that string there.
  if ((msg.data.size() == 0x2C) && (phosg::fnv1a64(msg.data.data(), msg.data.size()) == 0x8AF8314316A27994)) {
    c->log.info_f("Enabling remote IP CRC patch");
    c->proxy_session->enable_remote_ip_crc_patch = true;
  }
  co_return HandlerResult::SUPPRESS;
}

static asio::awaitable<HandlerResult> S_19_U_14(shared_ptr<Client> c, Channel::Message& msg) {
  // If the command is shorter than 6 bytes, use the previous server command to
  // fill it in. This simulates a behavior used by some private servers where a
  // longer previous command is used to fill part of the client's receive buffer
  // with meaningful data, then an intentionally undersize 19 command is sent
  // which results in the client using the previous command's data as part of
  // the 19 command's contents. They presumably do this in an attempt to prevent
  // people from using proxies.
  if (msg.data.size() < sizeof(c->proxy_session->prev_server_command_bytes)) {
    msg.data.append(
        reinterpret_cast<const char*>(&c->proxy_session->prev_server_command_bytes[msg.data.size()]),
        sizeof(c->proxy_session->prev_server_command_bytes) - msg.data.size());
  }
  if (msg.data.size() < sizeof(S_Reconnect_19)) {
    msg.data.resize(sizeof(S_Reconnect_19), '\0');
  }

  c->proxy_session->received_reconnect = true;
  if (c->proxy_session->enable_remote_ip_crc_patch) {
    c->proxy_session->remote_ip_crc = phosg::crc32(msg.data.data(), 4);
  }

  // Get the new endpoint
  asio::ip::tcp::endpoint new_ep;
  if (is_patch(c->version())) {
    auto& cmd = msg.check_size_t<S_Reconnect_Patch_14>();
    new_ep = make_endpoint_ipv4(cmd.address, cmd.port);
  } else if (msg.flag == 6 && msg.data.size() >= sizeof(S_ReconnectIPv6_Extension_19)) {
    auto& cmd = msg.check_size_t<S_ReconnectIPv6_Extension_19>(0xFFFF);
    new_ep = make_endpoint_ipv6(cmd.address.data(), cmd.port);
  } else {
    // This weird maximum size is here to properly handle the version-split
    // command that some servers (including newserv) use on port 9100
    auto& cmd = msg.check_size_t<S_Reconnect_19>(0xFFFF);
    new_ep = make_endpoint_ipv4(cmd.address, cmd.port);
  }

  // Replace the server channel with a new channel to the new endpoint
  string netloc_str = str_for_endpoint(new_ep);
  c->log.info_f("Connecting to {}", netloc_str);
  auto sock = make_unique<asio::ip::tcp::socket>(co_await async_connect_tcp(new_ep));

  // Close the old channel only after replacing it with the new one
  auto s = c->require_server_state();
  auto old_channel = c->proxy_session->server_channel;
  auto new_channel = SocketChannel::create(
      s->io_context,
      std::move(sock),
      old_channel->version,
      old_channel->language,
      std::format("C-{} proxy remote server at {}", c->id, netloc_str),
      old_channel->terminal_send_color,
      old_channel->terminal_recv_color);
  c->proxy_session->server_channel = new_channel;
  asio::co_spawn(*s->io_context, handle_proxy_server_commands(c, c->proxy_session, new_channel), asio::detached);
  c->log.info_f("Server channel connected");
  old_channel->disconnect();

  // Hide redirects from the client completely
  co_return HandlerResult::SUPPRESS;
}

static asio::awaitable<HandlerResult> S_V3_1A_D5(shared_ptr<Client> c, Channel::Message&) {
  // If the client is a version that sends close confirmations and the client
  // has the no-close-confirmation flag set in its newserv client config, send a
  // fake confirmation to the remote server immediately.
  if (is_v3(c->version()) && c->check_flag(Client::Flag::NO_D6)) {
    c->proxy_session->server_channel->send(0xD6);
  }
  co_return HandlerResult::FORWARD;
}

static asio::awaitable<HandlerResult> S_V3_BB_DA(shared_ptr<Client> c, Channel::Message& msg) {
  // This command is supported on all V3 and V4 versions except Ep1&2 Trial
  if (c->version() == Version::GC_NTE) {
    co_return HandlerResult::SUPPRESS;
  } else if ((c->override_lobby_event != 0xFF) && (msg.flag != c->override_lobby_event)) {
    msg.flag = c->override_lobby_event;
    co_return HandlerResult::MODIFIED;
  } else {
    co_return HandlerResult::FORWARD;
  }
}

static asio::awaitable<HandlerResult> SC_6x60_6xA2(shared_ptr<Client> c, Channel::Message& msg) {
  if (!c->proxy_session->is_in_game) {
    co_return HandlerResult::FORWARD;
  }

  switch (c->proxy_session->drop_mode) {
    case ProxyDropMode::DISABLED:
      co_return HandlerResult::SUPPRESS;
    case ProxyDropMode::PASSTHROUGH:
      co_return HandlerResult::FORWARD;
    case ProxyDropMode::INTERCEPT:
      break;
    default:
      throw logic_error("invalid drop mode");
  }

  if (!c->proxy_session->item_creator) {
    c->log.warning_f("Session is in INTERCEPT drop mode, but item creator is missing");
    co_return HandlerResult::FORWARD;
  }
  if (!c->proxy_session->map_state) {
    c->log.warning_f("Session is in INTERCEPT drop mode, but map state is missing");
    co_return HandlerResult::FORWARD;
  }

  G_SpecializableItemDropRequest_6xA2 cmd = normalize_drop_request(msg.data.data(), msg.data.size());
  auto rec = reconcile_drop_request_with_map(
      c,
      cmd,
      c->proxy_session->lobby_episode,
      c->proxy_session->lobby_difficulty,
      c->proxy_session->lobby_event,
      c->proxy_session->map_state,
      false);

  ItemCreator::DropResult res;
  if (rec.obj_st) {
    if (rec.ignore_def) {
      c->log.info_f("Creating item from box {:04X} (area {:02X})", cmd.entity_index, cmd.effective_area);
      res = c->proxy_session->item_creator->on_box_item_drop(cmd.effective_area);
    } else {
      c->log.info_f("Creating item from box {:04X} (area {:02X}; specialized with {:g} {:08X} {:08X} {:08X})",
          cmd.entity_index, cmd.effective_area,
          cmd.param3, cmd.param4, cmd.param5, cmd.param6);
      res = c->proxy_session->item_creator->on_specialized_box_item_drop(
          cmd.effective_area, cmd.param3, cmd.param4, cmd.param5, cmd.param6);
    }
  } else {
    c->log.info_f("Creating item from enemy {:04X} (area {:02X})", cmd.entity_index, cmd.effective_area);
    res = c->proxy_session->item_creator->on_monster_item_drop(rec.effective_rt_index, cmd.effective_area);
  }

  if (res.item.empty()) {
    c->log.info_f("No item was created");
  } else {
    auto s = c->require_server_state();
    string name = s->describe_item(c->version(), res.item);
    c->log.info_f("Entity {:04X} (area {:02X}) created item {}", cmd.entity_index, cmd.effective_area, name);
    res.item.id = c->proxy_session->next_item_id++;
    c->log.info_f("Creating item {:08X} at {:02X}:{:g},{:g} for all clients",
        res.item.id, cmd.floor, cmd.pos.x, cmd.pos.z);
    send_drop_item_to_channel(s, c->channel, res.item, rec.obj_st ? 2 : 1, cmd.floor, cmd.pos, cmd.entity_index);
    send_drop_item_to_channel(s, c->proxy_session->server_channel, res.item, rec.obj_st ? 2 : 1, cmd.floor, cmd.pos, cmd.entity_index);
    send_item_notification_if_needed(c, res.item, res.is_from_rare_table);
  }
  co_return HandlerResult::SUPPRESS;
}

static asio::awaitable<HandlerResult> S_6x(shared_ptr<Client> c, Channel::Message& msg) {
  auto s = c->require_server_state();

  if (msg.data.size() < 4) {
    co_return HandlerResult::SUPPRESS;
  }

  bool modified = false;
  uint8_t subcommand = translate_subcommand_number(Version::BB_V4, c->version(), msg.data[0]);
  switch (subcommand) {
    case 0x00:
      c->log.warning_f("Blocking invalid subcommand from server");
      co_return HandlerResult::SUPPRESS;

    case 0x16:
    case 0x84: {
      const auto& cmd = msg.check_size_t<G_VolOptBossActions_6x16>(0xFFFF);
      if (cmd.entity_index_count > 6) {
        c->log.warning_f("Blocking subcommand 6x16/6x84 with invalid entity index count");
        co_return HandlerResult::SUPPRESS;
      }
      for (size_t z = 0; z < cmd.entity_index_table.size(); z++) {
        if (cmd.entity_index_table[z] >= 6) {
          c->log.warning_f("Blocking subcommand 6x16/6x84 with invalid entity index");
          co_return HandlerResult::SUPPRESS;
        }
      }
      break;
    }

    case 0x17: {
      const auto& cmd = msg.check_size_t<G_SetEntityPositionAndAngle_6x17>();
      if (cmd.header.entity_id == c->lobby_client_id) {
        c->log.warning_f("Blocking subcommand 6x17 targeting local client");
        co_return HandlerResult::SUPPRESS;
      }
      break;
    }

    case 0x2F: {
      const auto& cmd = msg.check_size_t<G_ChangePlayerHP_6x2F>();
      if (cmd.client_id == c->lobby_client_id) {
        c->log.warning_f("Blocking subcommand 6x2F targeting local player");
        co_return HandlerResult::SUPPRESS;
      }
      break;
    }

    case 0x46: {
      const auto& header = msg.check_size_t<G_AttackFinished_Header_6x46>(0xFFFF);
      if (header.target_count > min<size_t>(header.header.size - sizeof(G_AttackFinished_Header_6x46) / 4, 10)) {
        c->log.warning_f("Blocking subcommand 6x46 with invalid count");
        co_return HandlerResult::SUPPRESS;
      }
      break;
    }

    case 0x47: {
      const auto& header = msg.check_size_t<G_CastTechnique_Header_6x47>(0xFFFF);
      if (header.target_count > min<size_t>(header.header.size - sizeof(G_CastTechnique_Header_6x47) / 4, 10)) {
        c->log.warning_f("Blocking subcommand 6x47 with invalid count");
        co_return HandlerResult::SUPPRESS;
      }
      break;
    }

    case 0x49: {
      const auto& header = msg.check_size_t<G_ExecutePhotonBlast_Header_6x49>(0xFFFF);
      if (header.target_count > min<size_t>(header.header.size - sizeof(G_ExecutePhotonBlast_Header_6x49) / 4, 10)) {
        c->log.warning_f("Blocking subcommand 6x49 with invalid count");
        co_return HandlerResult::SUPPRESS;
      }
      break;
    }

    case 0x5F: {
      const auto& cmd = msg.check_size_t<G_DropItem_DC_6x5F>(sizeof(G_DropItem_PC_V3_BB_6x5F));
      ItemData item = cmd.item.item;
      item.decode_for_version(c->version());
      send_item_notification_if_needed(c, item, true);
      break;
    }

    case 0x60:
    case 0xA2:
      co_return co_await SC_6x60_6xA2(c, msg);

    case 0x6A: {
      auto& cmd = msg.check_size_t<G_SetBossWarpFlags_6x6A>();
      if (c->proxy_session->map_state) {
        shared_ptr<MapState::ObjectState> obj_st;
        try {
          obj_st = c->proxy_session->map_state->object_state_for_index(c->version(), cmd.header.entity_id - 0x4000);
        } catch (const exception& e) {
          c->log.warning_f("Invalid object reference ({})", e.what());
        }

        if (!obj_st || !obj_st->super_obj) {
          c->log.warning_f("Blocking subcommand 6x6A with missing object");
          co_return HandlerResult::SUPPRESS;
        }
        auto set_entry = obj_st->super_obj->version(c->version()).set_entry;
        if (!set_entry) {
          c->log.warning_f("Blocking subcommand 6x6A with missing set entry");
          co_return HandlerResult::SUPPRESS;
        }
        if (set_entry->base_type != 0x0019 && set_entry->base_type != 0x0055) {
          c->log.warning_f("Blocking subcommand 6x6A with incorrect object type");
          co_return HandlerResult::SUPPRESS;
        }
      }
      break;
    }

    case 0x7D: {
      const auto& cmd = msg.check_size_t<G_SetBattleModeData_6x7D>();
      if ((cmd.what == 3 || cmd.what == 4) && cmd.params[0] >= 4) {
        c->log.warning_f("Blocking subcommand 6x7D with invalid client ID");
        co_return HandlerResult::SUPPRESS;
      }
      break;
    }

    case 0xB3:
    case 0xB4:
    case 0xB5: {
      if (!is_ep3(c->version()) || (msg.data.size() < 8)) {
        break;
      }
      // Unmask any masked Episode 3 commands from the server
      const auto& header = msg.check_size_t<G_CardBattleCommandHeader>(0xFFFF);
      if (header.mask_key && (c->version() != Version::GC_EP3_NTE)) {
        set_mask_for_ep3_game_command(msg.data.data(), msg.data.size(), 0);
        modified = true;
      }

      if ((subcommand == 0xB4) && c->check_flag(Client::Flag::PROXY_EP3_INFINITE_TIME_ENABLED)) {
        if (header.subsubcommand == 0x05) {
          if (c->version() == Version::GC_EP3_NTE) {
            auto& cmd = msg.check_size_t<G_UpdateMap_Ep3NTE_6xB4x05>();
            if (cmd.state.rules.overall_time_limit || cmd.state.rules.phase_time_limit) {
              cmd.state.rules.overall_time_limit = 0;
              cmd.state.rules.phase_time_limit = 0;
              modified = true;
            }
          } else {
            auto& cmd = msg.check_size_t<G_UpdateMap_Ep3_6xB4x05>();
            if (cmd.state.rules.overall_time_limit || cmd.state.rules.phase_time_limit) {
              cmd.state.rules.overall_time_limit = 0;
              cmd.state.rules.phase_time_limit = 0;
              modified = true;
            }
          }
        } else if (header.subsubcommand == 0x3D) {
          if (c->version() == Version::GC_EP3_NTE) {
            auto& cmd = msg.check_size_t<G_SetTournamentPlayerDecks_Ep3NTE_6xB4x3D>();
            if (cmd.rules.overall_time_limit || cmd.rules.phase_time_limit) {
              cmd.rules.overall_time_limit = 0;
              cmd.rules.phase_time_limit = 0;
              modified = true;
            }
          } else {
            auto& cmd = msg.check_size_t<G_SetTournamentPlayerDecks_Ep3_6xB4x3D>();
            if (cmd.rules.overall_time_limit || cmd.rules.phase_time_limit) {
              cmd.rules.overall_time_limit = 0;
              cmd.rules.phase_time_limit = 0;
              modified = true;
            }
          }
        }

      } else if (subcommand == 0xB5) {
        set_mask_for_ep3_game_command(msg.data.data(), msg.data.size(), 0);
        if (msg.data[4] == 0x1A) {
          co_return HandlerResult::SUPPRESS;
        } else if (msg.data[4] == 0x20) {
          auto& cmd = msg.check_size_t<G_Unknown_Ep3_6xB5x20>();
          if (cmd.client_id >= 12) {
            c->log.warning_f("Blocking 6xB5x20 from server with invalid client ID");
            co_return HandlerResult::SUPPRESS;
          }
        } else if (msg.data[4] == 0x31) {
          auto& cmd = msg.check_size_t<G_ConfirmDeckSelection_Ep3_6xB5x31>();
          if (cmd.menu_type >= 0x15) {
            c->log.warning_f("Blocking 6xB5x31 from server with invalid menu type");
            co_return HandlerResult::SUPPRESS;
          }
        } else if (msg.data[4] == 0x32) {
          auto& cmd = msg.check_size_t<G_MoveSharedMenuCursor_Ep3_6xB5x32>();
          if (cmd.menu_type >= 0x15) {
            c->log.warning_f("Blocking 6xB5x32 from server with invalid menu type");
            co_return HandlerResult::SUPPRESS;
          }
        } else if (msg.data[4] == 0x36) {
          auto& cmd = msg.check_size_t<G_RecreatePlayer_Ep3_6xB5x36>();
          if (c->proxy_session->is_in_game && (cmd.client_id >= 4)) {
            c->log.warning_f("Blocking 6xB5x36 from server with invalid client ID");
            co_return HandlerResult::SUPPRESS;
          }
        }
      }
      break;
    }

    case 0xB6:
      if (is_ep3(c->version()) && (msg.data.size() >= 0x14) && c->check_flag(Client::Flag::PROXY_SAVE_FILES)) {
        const auto& header = msg.check_size_t<G_MapSubsubcommand_Ep3_6xB6>(0xFFFF);
        if (header.subsubcommand == 0x00000041) {
          const auto& cmd = msg.check_size_t<G_MapData_Ep3_6xB6x41>(0xFFFF);
          string filename = std::format("map{:08X}.{}.mnmd", cmd.map_number, phosg::now());
          string map_data = prs_decompress(msg.data.data() + sizeof(cmd), msg.data.size() - sizeof(cmd));
          phosg::save_file(filename, map_data);
          if ((map_data.size() != sizeof(Episode3::MapDefinition)) &&
              (map_data.size() != sizeof(Episode3::MapDefinitionTrial))) {
            c->log.warning_f("Wrote {} bytes to {} (expected {} or {} bytes; the file may be invalid)",
                map_data.size(), filename, sizeof(Episode3::MapDefinitionTrial), sizeof(Episode3::MapDefinition));
          } else {
            c->log.info_f("Wrote {} bytes to {}", map_data.size(), filename);
          }
        }
      }
      break;

    case 0xBB:
      if (is_ep3(c->version()) && !validate_6xBB(msg.check_size_t<G_SyncCardTradeServerState_Ep3_6xBB>())) {
        co_return HandlerResult::SUPPRESS;
      }
      break;

    case 0xBC:
      if (!c->check_flag(Client::Flag::EP3_ALLOW_6xBC)) {
        co_return HandlerResult::SUPPRESS;
      }
      break;

    case 0xBD:
      if (is_ep3(c->version()) && c->check_flag(Client::Flag::PROXY_EP3_UNMASK_WHISPERS)) {
        auto& cmd = msg.check_size_t<G_PrivateWordSelect_Ep3_6xBD>();
        if (cmd.private_flags & (1 << c->lobby_client_id)) {
          cmd.private_flags &= ~(1 << c->lobby_client_id);
          modified = true;
        }
      }
      break;

    default:
      break;
  }
  co_return modified ? HandlerResult::MODIFIED : HandlerResult::FORWARD;
}

static asio::awaitable<HandlerResult> C_GXB_61(shared_ptr<Client> c, Channel::Message& msg) {
  bool modified = false;
  // TODO: We should check if the info board text was actually modified and
  // return MODIFIED if so.

  if (is_v4(c->version())) {
    auto& pd = msg.check_size_t<C_CharacterData_BB_61_98>(0xFFFF);
    pd.info_board.encode(add_color(pd.info_board.decode(c->language())), c->language());

  } else {
    C_CharacterData_V3_61_98* pd;
    if (msg.flag == 4) { // Episode 3
      auto& ep3_pd = msg.check_size_t<C_CharacterData_Ep3_61_98>();
      // Technically we could decrypt the Ep3 config struct within the player
      // data, but this may confuse some non-newserv upstream servers if they
      // implement this structure incorrectly. The decryption would go like:
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
      if (is_ep3(c->version()) && (c->version() != Version::GC_EP3_NTE)) {
        c->log.info_f("Version changed to GC_EP3_NTE");
        c->channel->version = Version::GC_EP3_NTE;
        c->proxy_session->server_channel->version = Version::GC_EP3_NTE;
        c->specific_version = SPECIFIC_VERSION_GC_EP3_NTE;
      }
      pd = &msg.check_size_t<C_CharacterData_V3_61_98>(0xFFFF);
    }
    pd->info_board.encode(add_color(pd->info_board.decode(c->language())), c->language());
  }

  co_return modified ? HandlerResult::MODIFIED : HandlerResult::FORWARD;
}

static asio::awaitable<HandlerResult> C_GX_D9(shared_ptr<Client>, Channel::Message& msg) {
  phosg::strip_trailing_zeroes(msg.data);
  msg.data = add_color(msg.data);
  msg.data.push_back(0);
  while (msg.data.size() & 3) {
    msg.data.push_back(0);
  }
  // TODO: We should check if the info board text was actually modified and
  // return FORWARD if not.
  co_return HandlerResult::MODIFIED;
}

static asio::awaitable<HandlerResult> C_B_D9(shared_ptr<Client> c, Channel::Message& msg) {
  try {
    phosg::strip_trailing_zeroes(msg.data);
    if (msg.data.size() & 1) {
      msg.data.push_back(0);
    }
    string decoded = tt_utf16_to_utf8(msg.data.data(), msg.data.size());
    add_color_inplace(decoded);
    msg.data = tt_utf8_to_utf16(decoded.data(), decoded.size());
    while (msg.data.size() & 3) {
      msg.data.push_back(0);
    }
  } catch (const runtime_error& e) {
    c->log.warning_f("Failed to decode and unescape D9 command: {}", e.what());
  }
  // TODO: We should check if the info board text was actually modified and
  // return HandlerResult::FORWARD if not.
  co_return HandlerResult::MODIFIED;
}

template <typename T>
static asio::awaitable<HandlerResult> S_44_A6(shared_ptr<Client> c, Channel::Message& msg) {
  const auto& cmd = msg.check_size_t<T>();

  string filename = cmd.filename.decode();
  string output_filename;
  bool is_download = (msg.command == 0xA6);
  if (c->check_flag(Client::Flag::PROXY_SAVE_FILES)) {
    size_t extension_offset = filename.rfind('.');
    string basename, extension;
    if (extension_offset != string::npos) {
      basename = filename.substr(0, extension_offset);
      extension = filename.substr(extension_offset);
      if (extension == ".bin" && is_ep3(c->version())) {
        extension += ".mnm";
      }
    } else {
      basename = filename;
    }
    output_filename = std::format("{}.{}.{}{}",
        basename, is_download ? "download" : "online", phosg::now(), extension);

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
  bool decode_dlq = is_download && (c->version() != Version::GC_EP3);
  auto emplace_ret = c->proxy_session->saving_files.emplace(filename, ProxySession::SavingFile());
  auto& sf = emplace_ret.first->second;
  sf.basename = filename;
  sf.output_filename = output_filename;
  sf.total_size = cmd.file_size;
  sf.is_download = decode_dlq;
  if (c->check_flag(Client::Flag::PROXY_SAVE_FILES)) {
    c->log.info_f("Saving {} from server to {}", filename, output_filename);
  } else {
    c->log.info_f("Tracking file {}", filename);
  }

  co_return HandlerResult::FORWARD;
}

constexpr on_message_t S_D_44_A6 = &S_44_A6<S_OpenFile_DC_44_A6>;
constexpr on_message_t S_PG_44_A6 = &S_44_A6<S_OpenFile_PC_GC_44_A6>;
constexpr on_message_t S_X_44_A6 = &S_44_A6<S_OpenFile_XB_44_A6>;
constexpr on_message_t S_B_44_A6 = &S_44_A6<S_OpenFile_BB_44_A6>;

static asio::awaitable<HandlerResult> S_13_A7(shared_ptr<Client> c, Channel::Message& msg) {
  auto& cmd = msg.check_size_t<S_WriteFile_13_A7>();
  bool modified = false;

  ProxySession::SavingFile* sf = nullptr;
  try {
    sf = &c->proxy_session->saving_files.at(cmd.filename.decode());
  } catch (const out_of_range&) {
    string filename = cmd.filename.decode();
    c->log.warning_f("Received data for non-open file {}", filename);
  }
  if (!sf) {
    co_return HandlerResult::FORWARD;
  }

  bool is_last_block = (cmd.data_size != 0x400);
  size_t block_offset = msg.flag * 0x400;
  size_t allowed_block_size = (block_offset < sf->total_size)
      ? min<size_t>(sf->total_size - block_offset, 0x400)
      : 0;

  if (cmd.data_size > allowed_block_size) {
    c->log.warning_f("Block size extends beyond allowed size; truncating block");
    cmd.data_size = allowed_block_size;
    modified = true;
  }

  if (!sf->output_filename.empty()) {
    c->log.info_f("Adding {} bytes to {}:{:02X} => {}:{:X}",
        cmd.data_size, sf->basename, msg.flag, sf->output_filename, block_offset);
    if (c->check_flag(Client::Flag::PROXY_SAVE_FILES)) {
      size_t block_end_offset = block_offset + cmd.data_size;
      if (sf->data.size() < block_end_offset) {
        sf->data.resize(block_end_offset);
      }
      memcpy(sf->data.data() + block_offset, reinterpret_cast<const char*>(cmd.data.data()), cmd.data_size);
    }
  }

  if (is_last_block) {
    if (c->check_flag(Client::Flag::PROXY_SAVE_FILES)) {
      c->log.info_f("Writing file {} => {}", sf->basename, sf->output_filename);

      if (sf->is_download && (sf->basename.ends_with(".bin") || sf->basename.ends_with(".dat") || sf->basename.ends_with(".pvr"))) {
        sf->data = decode_dlq_data(sf->data);
      }
      phosg::save_file(sf->output_filename, sf->data);
    } else {
      c->log.info_f("Download complete for file {}", sf->basename);
    }

    if (!sf->is_download && sf->basename.ends_with(".dat")) {
      auto quest_dat_data = make_shared<std::string>(prs_decompress(sf->data));
      try {
        auto map_file = make_shared<MapFile>(quest_dat_data);
        auto materialized_map_file = map_file->materialize_random_sections(c->proxy_session->lobby_random_seed);

        array<shared_ptr<const MapFile>, NUM_VERSIONS> map_files;
        map_files.at(static_cast<size_t>(c->version())) = materialized_map_file;
        auto supermap = make_shared<SuperMap>(c->proxy_session->lobby_episode, map_files);

        c->proxy_session->map_state = make_shared<MapState>(
            c->id,
            c->proxy_session->lobby_difficulty,
            c->proxy_session->lobby_event,
            c->proxy_session->lobby_random_seed,
            MapState::DEFAULT_RARE_ENEMIES,
            make_shared<MT19937Generator>(c->proxy_session->lobby_random_seed),
            supermap);

      } catch (const exception& e) {
        c->log.warning_f("Failed to load quest map: {}", e.what());
        c->proxy_session->map_state.reset();
      }
    }

    c->proxy_session->saving_files.erase(cmd.filename.decode());
  }

  co_return modified ? HandlerResult::MODIFIED : HandlerResult::FORWARD;
}

static asio::awaitable<HandlerResult> S_G_B7(shared_ptr<Client> c, Channel::Message& msg) {
  if (is_ep3(c->version())) {
    if (c->check_flag(Client::Flag::PROXY_EP3_INFINITE_MESETA_ENABLED)) {
      auto& cmd = msg.check_size_t<S_RankUpdate_Ep3_B7>();
      if (cmd.current_meseta != 1000000) {
        cmd.current_meseta = 1000000;
        co_return HandlerResult::MODIFIED;
      }
    }
    co_return HandlerResult::FORWARD;
  } else {
    c->proxy_session->server_channel->send(0xB7, 0x00);
    co_return HandlerResult::SUPPRESS;
  }
}

static asio::awaitable<HandlerResult> S_G_B8(shared_ptr<Client> c, Channel::Message& msg) {
  if (c->check_flag(Client::Flag::PROXY_SAVE_FILES)) {
    if (msg.data.size() < 4) {
      c->log.warning_f("Card list data size is too small; not saving file");
      co_return HandlerResult::FORWARD;
    }

    phosg::StringReader r(msg.data);
    size_t size = r.get_u32l();
    if (r.remaining() < size) {
      c->log.warning_f("Card list data size extends beyond end of command; not saving file");
      co_return HandlerResult::FORWARD;
    }

    string output_filename = std::format("card-definitions.{}.mnr", phosg::now());
    phosg::save_file(output_filename, r.read(size));
    c->log.info_f("Wrote {} bytes to {}", size, output_filename);
  }

  // Unset the flag specifying that the client has newserv's card definitions,
  // so the file sill be sent again if the client returns to newserv.
  c->clear_flag(Client::Flag::HAS_EP3_CARD_DEFS);

  co_return is_ep3(c->version()) ? HandlerResult::FORWARD : HandlerResult::SUPPRESS;
}

static asio::awaitable<HandlerResult> S_G_B9(shared_ptr<Client> c, Channel::Message& msg) {
  if (c->check_flag(Client::Flag::PROXY_SAVE_FILES)) {
    try {
      const auto& header = msg.check_size_t<S_UpdateMediaHeader_Ep3_B9>(0xFFFF);

      if (msg.data.size() - sizeof(header) < header.size) {
        throw runtime_error("Media data size extends beyond end of command; not saving file");
      }

      string decompressed_data = prs_decompress(
          msg.data.data() + sizeof(header), msg.data.size() - sizeof(header));

      string output_filename = std::format("media-update.{}", phosg::now());
      if (header.type == 1) {
        output_filename += ".gvm";
      } else if (header.type == 2 || header.type == 3) {
        output_filename += ".bml";
      } else {
        output_filename += ".bin";
      }
      phosg::save_file(output_filename, decompressed_data);
      c->log.info_f("Wrote {} bytes to {}", decompressed_data.size(), output_filename);
    } catch (const exception& e) {
      c->log.warning_f("Failed to save file: {}", e.what());
    }
  }

  // This command exists only in final Episode 3 and not in Trial Edition
  // (hence not using is_ep3() here)
  co_return (c->version() == Version::GC_EP3) ? HandlerResult::FORWARD : HandlerResult::SUPPRESS;
}

static asio::awaitable<HandlerResult> C_G_B9(shared_ptr<Client> c, Channel::Message&) {
  if (c->proxy_session->suppress_next_ep3_media_update_confirmation) {
    c->proxy_session->suppress_next_ep3_media_update_confirmation = false;
    co_return HandlerResult::SUPPRESS;
  }
  co_return HandlerResult::FORWARD;
}

static asio::awaitable<HandlerResult> S_G_EF(shared_ptr<Client> c, Channel::Message& msg) {
  if (is_ep3(c->version())) {
    if (c->check_flag(Client::Flag::PROXY_EP3_INFINITE_MESETA_ENABLED)) {
      auto& cmd = msg.check_size_t<S_StartCardAuction_Ep3_EF>(offsetof(S_StartCardAuction_Ep3_EF, unused), 0xFFFF);
      if (cmd.points_available != 0x7FFF) {
        cmd.points_available = 0x7FFF;
        co_return HandlerResult::MODIFIED;
      }
    }
    co_return HandlerResult::FORWARD;
  } else {
    co_return HandlerResult::SUPPRESS;
  }
}

static asio::awaitable<HandlerResult> S_B_EF(shared_ptr<Client>, Channel::Message&) {
  // See the comments on EF in CommandFormats.hh for why we unconditionally
  // suppress these.
  co_return HandlerResult::SUPPRESS;
}

static asio::awaitable<HandlerResult> S_G_BA(shared_ptr<Client> c, Channel::Message& msg) {
  if (c->check_flag(Client::Flag::PROXY_EP3_INFINITE_MESETA_ENABLED)) {
    auto& cmd = msg.check_size_t<S_MesetaTransaction_Ep3_BA>();
    if (cmd.current_meseta != 1000000) {
      cmd.current_meseta = 1000000;
      co_return HandlerResult::MODIFIED;
    }
  }
  co_return HandlerResult::FORWARD;
}

static void update_leader_id(shared_ptr<Client> c, uint8_t leader_id) {
  if (c->proxy_session->leader_client_id != leader_id) {
    c->proxy_session->leader_client_id = leader_id;
    c->log.info_f("Changed room leader to {:X}", c->proxy_session->leader_client_id);
    if (c->check_flag(Client::Flag::PROXY_PLAYER_NOTIFICATIONS_ENABLED) &&
        (c->proxy_session->leader_client_id == c->lobby_client_id)) {
      send_text_message(c->channel, "$C6You are now the leader");
    }
  }
}

template <typename CmdT>
static asio::awaitable<HandlerResult> S_65_67_68_EB(shared_ptr<Client> c, Channel::Message& msg) {
  if (msg.command == 0x67) {
    c->proxy_session->clear_lobby_players(12);
    c->proxy_session->is_in_lobby = true;
    c->proxy_session->is_in_game = false;
    c->proxy_session->is_in_quest = false;
    c->floor = 0x0F;
    c->proxy_session->lobby_difficulty = Difficulty::NORMAL;
    c->proxy_session->lobby_section_id = 0;
    c->proxy_session->lobby_mode = GameMode::NORMAL;
    c->proxy_session->lobby_episode = Episode::EP1;
    c->proxy_session->lobby_random_seed = 0;
    c->proxy_session->item_creator.reset();
    c->proxy_session->map_state.reset();

    // This command can cause the client to no longer send D6 responses when
    // 1A/D5 large message boxes are closed. newserv keeps track of this
    // behavior in the client config, so if it happens during a proxy session,
    // update the client config that we'll restore if the client uses the change
    // ship or change block command.
    if (c->check_flag(Client::Flag::NO_D6_AFTER_LOBBY)) {
      c->set_flag(Client::Flag::NO_D6);
    }
  }

  size_t expected_size = offsetof(CmdT, entries) + sizeof(typename CmdT::Entry) * msg.flag;
  auto& cmd = msg.check_size_t<CmdT>(expected_size, 0xFFFF);
  bool modified = false;

  size_t num_replacements = 0;
  c->lobby_client_id = cmd.lobby_flags.client_id;
  update_leader_id(c, cmd.lobby_flags.leader_id);
  for (size_t x = 0; x < msg.flag; x++) {
    auto& entry = cmd.entries[x];
    size_t index = entry.lobby_data.client_id;
    if (index >= c->proxy_session->lobby_players.size()) {
      c->log.warning_f("Ignoring invalid player index {} at position {}", index, x);
    } else {
      string name = escape_player_name(entry.disp.visual.name.decode(entry.inventory.language));
      if (c->login && (entry.lobby_data.guild_card_number == c->proxy_session->remote_guild_card_number)) {
        num_replacements++;
        if (c->login->account->account_id != c->proxy_session->remote_guild_card_number) {
          entry.lobby_data.guild_card_number = c->login->account->account_id;
          modified = true;
        }
      } else if (c->check_flag(Client::Flag::PROXY_PLAYER_NOTIFICATIONS_ENABLED) && (msg.command != 0x67)) {
        send_text_message_fmt(c->channel, "$C6Join: {}/{}\n{}",
            index, entry.lobby_data.guild_card_number, name);
      }
      auto& p = c->proxy_session->lobby_players[index];
      p.guild_card_number = entry.lobby_data.guild_card_number;
      p.name = name;
      p.language = entry.inventory.language;
      p.section_id = entry.disp.visual.section_id;
      p.char_class = entry.disp.visual.char_class;
      c->log.info_f("Added lobby player: ({}) {} {}",
          index, p.guild_card_number, p.name);
    }
  }
  if (num_replacements > 1) {
    c->log.warning_f("Proxied player appears multiple times in lobby");
  }

  if constexpr (sizeof(cmd.lobby_flags) > sizeof(LobbyFlags_DCNTE)) {
    c->proxy_session->lobby_event = cmd.lobby_flags.event;
    if (c->override_lobby_event != 0xFF) {
      cmd.lobby_flags.event = c->override_lobby_event;
      modified = true;
    }
    if (c->override_lobby_number != 0x80) {
      cmd.lobby_flags.lobby_number = c->override_lobby_number;
      modified = true;
    }
  }

  co_return modified ? HandlerResult::MODIFIED : HandlerResult::FORWARD;
}

constexpr on_message_t S_N_65_67_68 = &S_65_67_68_EB<S_JoinLobby_DCNTE_65_67_68>;
constexpr on_message_t S_DG_65_67_68_EB = &S_65_67_68_EB<S_JoinLobby_DC_GC_65_67_68_Ep3_EB>;
constexpr on_message_t S_P_65_67_68 = &S_65_67_68_EB<S_JoinLobby_PC_65_67_68>;
constexpr on_message_t S_X_65_67_68 = &S_65_67_68_EB<S_JoinLobby_XB_65_67_68>;
constexpr on_message_t S_B_65_67_68 = &S_65_67_68_EB<S_JoinLobby_BB_65_67_68>;

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
static asio::awaitable<HandlerResult> S_64(shared_ptr<Client> c, Channel::Message& msg) {
  CmdT* cmd;
  S_JoinGame_Ep3_64* cmd_ep3 = nullptr;
  if ((c->sub_version >= 0x40) && is_v3(c->version())) {
    cmd = &msg.check_size_t<CmdT>(sizeof(S_JoinGame_Ep3_64));
    cmd_ep3 = &msg.check_size_t<S_JoinGame_Ep3_64>();
  } else if (c->version() == Version::XB_V3) {
    // Schtserv doesn't send the unknown_a1 field in this command, and we don't
    // use it here, so we allow it to be omitted.
    cmd = &msg.check_size_t<CmdT>(sizeof(CmdT) - 0x18, sizeof(CmdT));
  } else {
    cmd = &msg.check_size_t<CmdT>(0xFFFF);
  }

  bool modified = false;

  c->proxy_session->clear_lobby_players(4);
  c->floor = 0;
  c->proxy_session->is_in_lobby = false;
  c->proxy_session->is_in_game = true;
  c->proxy_session->is_in_quest = false;
  if constexpr (sizeof(*cmd) > sizeof(S_JoinGame_DCNTE_64)) {
    c->proxy_session->lobby_event = cmd->event;
    c->proxy_session->lobby_difficulty = cmd->difficulty;
    c->proxy_session->lobby_section_id = cmd->section_id;
    // We only need the game mode for overriding drops, and SOLO behaves the same
    // as NORMAL in that regard, so we can conveniently ignore SOLO here
    if (cmd->battle_mode) {
      c->proxy_session->lobby_mode = GameMode::BATTLE;
    } else if (cmd->challenge_mode) {
      c->proxy_session->lobby_mode = GameMode::CHALLENGE;
    } else {
      c->proxy_session->lobby_mode = GameMode::NORMAL;
    }
    c->proxy_session->lobby_random_seed = cmd->random_seed;

    if (c->override_section_id != 0xFF) {
      cmd->section_id = c->override_section_id;
      modified = true;
    }
    if (c->override_lobby_event != 0xFF) {
      cmd->event = c->override_lobby_event;
      modified = true;
    }
    if (c->override_random_seed >= 0) {
      cmd->random_seed = c->override_random_seed;
      modified = true;
    }

  } else {
    c->proxy_session->lobby_event = 0;
    c->proxy_session->lobby_difficulty = Difficulty::NORMAL;
    c->proxy_session->lobby_section_id = c->character_file()->disp.visual.section_id;
    c->proxy_session->lobby_mode = GameMode::NORMAL;
    c->proxy_session->lobby_random_seed = phosg::random_object<uint32_t>();
  }
  if (cmd_ep3) {
    c->proxy_session->lobby_episode = Episode::EP3;
  } else {
    c->proxy_session->lobby_episode = get_episode(*cmd);
  }

  if (c->version() == Version::GC_NTE) {
    // GC NTE ignores the variations field entirely, so clear the array to
    // ensure we'll load the correct maps
    cmd->variations = Variations();
  }

  // Recreate the item creator if needed, and load maps
  auto s = c->require_server_state();
  c->proxy_session->set_drop_mode(s, c->version(), c->override_random_seed, c->proxy_session->drop_mode);
  if (!is_ep3(c->version()) && (c->proxy_session->lobby_mode != GameMode::CHALLENGE)) {
    auto supermaps = s->supermaps_for_variations(
        c->proxy_session->lobby_episode, c->proxy_session->lobby_mode, c->proxy_session->lobby_difficulty, cmd->variations);
    c->proxy_session->map_state = make_shared<MapState>(
        c->id,
        c->proxy_session->lobby_difficulty,
        c->proxy_session->lobby_event,
        c->proxy_session->lobby_random_seed,
        MapState::DEFAULT_RARE_ENEMIES,
        make_shared<MT19937Generator>(c->proxy_session->lobby_random_seed),
        supermaps);
  } else {
    c->proxy_session->map_state.reset();
  }

  c->lobby_client_id = cmd->client_id;
  update_leader_id(c, cmd->leader_id);
  for (size_t x = 0; x < msg.flag; x++) {
    if (cmd->lobby_data[x].guild_card_number == c->proxy_session->remote_guild_card_number &&
        c->login &&
        c->login->account->account_id != c->proxy_session->remote_guild_card_number) {
      cmd->lobby_data[x].guild_card_number = c->login->account->account_id;
      modified = true;
    }
    auto& p = c->proxy_session->lobby_players[x];
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
    c->log.info_f("Added lobby player: ({}) {} {}", x, p.guild_card_number, p.name);
  }

  co_return modified ? HandlerResult::MODIFIED : HandlerResult::FORWARD;
}

constexpr on_message_t S_N_64 = &S_64<S_JoinGame_DCNTE_64>;
constexpr on_message_t S_D_64 = &S_64<S_JoinGame_DC_64>;
constexpr on_message_t S_P_64 = &S_64<S_JoinGame_PC_64>;
constexpr on_message_t S_G_64 = &S_64<S_JoinGame_GC_64>;
constexpr on_message_t S_X_64 = &S_64<S_JoinGame_XB_64>;
constexpr on_message_t S_B_64 = &S_64<S_JoinGame_BB_64>;

static asio::awaitable<HandlerResult> S_E8(shared_ptr<Client> c, Channel::Message& msg) {
  auto& cmd = msg.check_size_t<S_JoinSpectatorTeam_Ep3_E8>();

  c->floor = 0;
  c->proxy_session->is_in_lobby = false;
  c->proxy_session->is_in_game = true;
  c->proxy_session->is_in_quest = false;
  c->proxy_session->lobby_event = cmd.event;
  c->proxy_session->lobby_difficulty = Difficulty::NORMAL;
  c->proxy_session->lobby_section_id = cmd.section_id;
  c->proxy_session->lobby_mode = GameMode::NORMAL;
  c->proxy_session->lobby_random_seed = 0;
  c->proxy_session->lobby_episode = Episode::EP3;
  c->proxy_session->item_creator.reset();
  c->proxy_session->map_state.reset();
  c->proxy_session->clear_lobby_players(12);

  bool modified = false;

  c->lobby_client_id = cmd.client_id;
  update_leader_id(c, cmd.leader_id);

  for (size_t x = 0; x < 12; x++) {
    auto& player_entry = (x < 4) ? cmd.players[x] : cmd.spectator_players[x - 4];
    auto& spec_entry = cmd.entries[x];

    if (c->login && c->login->account->account_id != c->proxy_session->remote_guild_card_number) {
      if (player_entry.lobby_data.guild_card_number == c->proxy_session->remote_guild_card_number) {
        player_entry.lobby_data.guild_card_number = c->login->account->account_id;
        modified = true;
      }
      if (spec_entry.guild_card_number == c->proxy_session->remote_guild_card_number) {
        spec_entry.guild_card_number = c->login->account->account_id;
        modified = true;
      }
    }

    auto& p = c->proxy_session->lobby_players[x];
    p.guild_card_number = player_entry.lobby_data.guild_card_number;
    p.language = player_entry.inventory.language;
    p.name = player_entry.disp.visual.name.decode(p.language);
    p.section_id = player_entry.disp.visual.section_id;
    p.char_class = player_entry.disp.visual.char_class;
    c->log.info_f("Added lobby player: ({}) {} {}", x, p.guild_card_number, p.name);
  }

  if (c->override_section_id != 0xFF) {
    cmd.section_id = c->override_section_id;
    modified = true;
  }
  if (c->override_lobby_event != 0xFF) {
    cmd.event = c->override_lobby_event;
    modified = true;
  }
  if (c->override_random_seed >= 0) {
    cmd.random_seed = c->override_random_seed;
    modified = true;
  }

  co_return modified ? HandlerResult::MODIFIED : HandlerResult::FORWARD;
}

static asio::awaitable<HandlerResult> S_AC(shared_ptr<Client> c, Channel::Message&) {
  if (!c->proxy_session->is_in_game) {
    co_return HandlerResult::SUPPRESS;
  } else {
    c->proxy_session->is_in_quest = true;
    co_return HandlerResult::FORWARD;
  }
}

static asio::awaitable<HandlerResult> S_66_69_E9(shared_ptr<Client> c, Channel::Message& msg) {
  // Schtserv sends a large command here for unknown reasons. The client ignores
  // the extra data, so we allow the large command here.
  const auto& cmd = msg.check_size_t<S_LeaveLobby_66_69_Ep3_E9>(0xFFFF);
  size_t index = cmd.client_id;
  if (index >= c->proxy_session->lobby_players.size()) {
    c->log.warning_f("Lobby leave command references missing position");
  } else {
    auto& p = c->proxy_session->lobby_players[index];
    string name = escape_player_name(p.name);
    if (c->check_flag(Client::Flag::PROXY_PLAYER_NOTIFICATIONS_ENABLED)) {
      send_text_message_fmt(c->channel, "$C4Leave: {}/{}\n{}", index, p.guild_card_number, name);
    }
    p.guild_card_number = 0;
    p.name.clear();
    c->log.info_f("Removed lobby player ({})", index);
  }
  update_leader_id(c, cmd.leader_id);
  co_return HandlerResult::FORWARD;
}

static asio::awaitable<HandlerResult> C_98(shared_ptr<Client> c, Channel::Message& msg) {
  c->floor = 0x0F;
  c->proxy_session->is_in_lobby = false;
  c->proxy_session->is_in_game = false;
  c->proxy_session->is_in_quest = false;
  c->proxy_session->lobby_event = 0;
  c->proxy_session->lobby_difficulty = Difficulty::NORMAL;
  c->proxy_session->lobby_section_id = 0;
  c->proxy_session->lobby_episode = Episode::EP1;
  c->proxy_session->lobby_mode = GameMode::NORMAL;
  c->proxy_session->lobby_random_seed = 0;
  c->proxy_session->item_creator.reset();
  c->proxy_session->map_state.reset();
  c->proxy_session->clear_lobby_players(12);

  if (is_v3(c->version()) || is_v4(c->version())) {
    co_return co_await C_GXB_61(c, msg);
  } else {
    co_return HandlerResult::FORWARD;
  }
}

static asio::awaitable<HandlerResult> C_06(shared_ptr<Client> c, Channel::Message& msg) {
  if (msg.data.size() >= 0x0C) {
    const auto& cmd = msg.check_size_t<SC_TextHeader_01_06_11_B0_EE>(0xFFFF);

    string text = msg.data.substr(sizeof(cmd));
    phosg::strip_trailing_zeroes(text);

    uint8_t private_flags = 0;
    try {
      if (uses_utf16(c->version())) {
        if (text.size() & 1) {
          text.push_back(0);
        }
        text = tt_decode_marked(text, c->language(), true);
      } else if (!text.empty() && (text[0] != '\t') && is_ep3(c->version())) {
        private_flags = text[0];
        text = tt_decode_marked(text.substr(1), c->language(), false);
      } else {
        text = tt_decode_marked(text, c->language(), false);
      }
    } catch (const runtime_error& e) {
      c->log.warning_f("Failed to decode and unescape chat text: {}", e.what());
      text.clear();
    }

    if (text.empty()) {
      co_return HandlerResult::FORWARD;
    }

    char command_sentinel = (c->version() == Version::DC_11_2000) ? '@' : '$';
    bool is_command = (text[0] == command_sentinel) ||
        (text[0] == '\t' && text[1] != 'C' && text[2] == command_sentinel);
    if (is_command && c->check_flag(Client::Flag::PROXY_CHAT_COMMANDS_ENABLED)) {
      size_t offset = ((text[0] & 0xF0) == 0x40) ? 1 : 0;
      offset += (text[offset] == command_sentinel) ? 0 : 2;
      text = text.substr(offset);
      if (text.size() >= 2 && text[1] == command_sentinel) {
        send_chat_message_from_client(c->proxy_session->server_channel, text.substr(1), private_flags);
        co_return HandlerResult::SUPPRESS;
      } else {
        co_await on_chat_command(c, text, true);
        co_return HandlerResult::SUPPRESS;
      }
    } else {
      co_return HandlerResult::FORWARD;
    }
  } else {
    co_return HandlerResult::FORWARD;
  }
}

static asio::awaitable<HandlerResult> C_40(shared_ptr<Client> c, Channel::Message& msg) {
  bool modified = false;
  if (c->login && c->login->account->account_id != c->proxy_session->remote_guild_card_number) {
    auto& cmd = msg.check_size_t<C_GuildCardSearch_40>();
    if (cmd.searcher_guild_card_number == c->login->account->account_id) {
      cmd.searcher_guild_card_number = c->proxy_session->remote_guild_card_number;
      modified = true;
    }
    if (cmd.target_guild_card_number == c->login->account->account_id) {
      cmd.target_guild_card_number = c->proxy_session->remote_guild_card_number;
      modified = true;
    }
  }
  co_return modified ? HandlerResult::MODIFIED : HandlerResult::FORWARD;
}

template <typename CmdT>
static asio::awaitable<HandlerResult> C_81(shared_ptr<Client> c, Channel::Message& msg) {
  auto& cmd = msg.check_size_t<CmdT>();
  if (c->login && c->login->account->account_id != c->proxy_session->remote_guild_card_number) {
    if (cmd.from_guild_card_number == c->login->account->account_id) {
      cmd.from_guild_card_number = c->proxy_session->remote_guild_card_number;
    }
    if (cmd.to_guild_card_number == c->login->account->account_id) {
      cmd.to_guild_card_number = c->proxy_session->remote_guild_card_number;
    }
  }
  // GC clients send uninitialized memory here; don't forward it
  cmd.text.clear_after_bytes(cmd.text.used_chars_8());
  co_return HandlerResult::MODIFIED;
}

constexpr on_message_t C_DGX_81 = &C_81<SC_SimpleMail_DC_V3_81>;
constexpr on_message_t C_P_81 = &C_81<SC_SimpleMail_PC_81>;
constexpr on_message_t C_B_81 = &C_81<SC_SimpleMail_BB_81>;

template <typename SendGuildCardCmdT>
asio::awaitable<HandlerResult> C_6x(shared_ptr<Client> c, Channel::Message& msg) {
  if (msg.data.size() < 4) {
    co_return HandlerResult::FORWARD;
  }

  bool modified = false;
  uint8_t subcommand = translate_subcommand_number(Version::BB_V4, c->version(), msg.data[0]);
  switch (subcommand) {
    case 0x05:
      if (c->check_flag(Client::Flag::SWITCH_ASSIST_ENABLED)) {
        auto& cmd = msg.check_size_t<G_WriteSwitchFlag_6x05>();
        if (c->proxy_session->map_state && (cmd.flags & 1) && (cmd.header.entity_id != 0xFFFF)) {
          auto door_states = c->proxy_session->map_state->door_states_for_switch_flag(
              c->version(), cmd.switch_flag_floor, cmd.switch_flag_num);
          for (auto& door_state : door_states) {
            if (door_state->game_flags & 0x0001) {
              continue;
            }
            door_state->game_flags |= 1;

            uint16_t object_index = c->proxy_session->map_state->index_for_object_state(c->version(), door_state);
            G_UpdateObjectState_6x0B cmd0B;
            cmd0B.header.subcommand = 0x0B;
            cmd0B.header.size = sizeof(cmd0B) / 4;
            cmd0B.header.entity_id = object_index | 0x4000;
            cmd0B.flags = door_state->game_flags;
            cmd0B.object_index = object_index;
            c->channel->send(0x60, 0x00, &cmd0B, sizeof(cmd0B));
            c->proxy_session->server_channel->send(0x60, 0x00, &cmd0B, sizeof(cmd0B));
          }
        }
      }
      break;

    case 0x06:
      // On BB, the 6x06 command is blank - the server generates the actual
      // Guild Card contents and sends it to the target client, so we only
      // expect data here if the client isn't BB.
      if (!is_v4(c->version()) &&
          c->login &&
          c->login->account->account_id != c->proxy_session->remote_guild_card_number) {
        auto& cmd = msg.check_size_t<SendGuildCardCmdT>();
        if (cmd.guild_card.guild_card_number == c->login->account->account_id) {
          cmd.guild_card.guild_card_number = c->proxy_session->remote_guild_card_number;
          modified = true;
        }
      }
      break;

    case 0x0C:
      if (c->check_flag(Client::Flag::INFINITE_HP_ENABLED)) {
        co_await send_remove_negative_conditions(c);
        send_remove_negative_conditions(c->proxy_session->server_channel, c->lobby_client_id);
      }
      break;

    case 0x21:
      c->floor = msg.check_size_t<G_InterLevelWarp_6x21>().floor;
      break;

    case 0x2F:
    case 0x4A:
    case 0x4B:
    case 0x4C:
      if (c->check_flag(Client::Flag::INFINITE_HP_ENABLED)) {
        co_await send_change_player_hp(c, c->lobby_client_id, PlayerHPChange::MAXIMIZE_HP, 0);
        send_change_player_hp(c->proxy_session->server_channel, c->lobby_client_id, PlayerHPChange::MAXIMIZE_HP, 0);
      }
      break;

    case 0x3E:
      c->pos = msg.check_size_t<G_StopAtPosition_6x3E>().pos;
      break;

    case 0x3F:
      c->pos = msg.check_size_t<G_SetPosition_6x3F>().pos;
      break;

    case 0x40: {
      const auto& cmd = msg.check_size_t<G_WalkToPosition_6x40>();
      c->pos.x = cmd.pos.x;
      c->pos.z = cmd.pos.z;
      break;
    }

    case 0x41:
    case 0x42: {
      const auto& cmd = msg.check_size_t<G_MoveToPosition_6x41_6x42>();
      c->pos.x = cmd.pos.x;
      c->pos.z = cmd.pos.z;
      break;
    }

    case 0x48:
      if (!is_v1(c->version()) && c->check_flag(Client::Flag::INFINITE_TP_ENABLED)) {
        send_player_stats_change(c->channel, c->lobby_client_id, PlayerStatsChange::ADD_TP, 255);
        send_player_stats_change(c->proxy_session->server_channel, c->lobby_client_id, PlayerStatsChange::ADD_TP, 255);
      }
      break;

    case 0x4E: {
      if (c->check_flag(Client::Flag::INFINITE_HP_ENABLED)) {
        if (is_v1_or_v2(c->version()) && (c->version() != Version::GC_NTE)) {
          G_UseMedicalCenter_6x31 cmd = {0x31, 0x01, c->lobby_client_id};
          send_command_t(c->channel, 0x60, 0x00, cmd);
          send_command_t(c->proxy_session->server_channel, 0x60, 0x00, cmd);
        } else {
          G_RevivePlayer_V3_BB_6xA1 cmd = {0xA1, 0x01, c->lobby_client_id};
          co_await send_protected_command(c, &cmd, sizeof(cmd), true);
        }
      }
      break;
    }

    case 0x5F:
      send_item_notification_if_needed(
          c, msg.check_size_t<G_DropItem_DC_6x5F>(sizeof(G_DropItem_PC_V3_BB_6x5F)).item.item, true);
      break;

    case 0x60:
    case 0xA2:
      co_return co_await SC_6x60_6xA2(c, msg);
      break;

    case 0xB5:
      if (is_ep3(c->version()) && (msg.data.size() > 4)) {
        if (msg.data[4] == 0x38) {
          c->set_flag(Client::Flag::EP3_ALLOW_6xBC);
        } else if (msg.data[4] == 0x3C) {
          c->clear_flag(Client::Flag::EP3_ALLOW_6xBC);
        }
      }
      break;

    default:
      break;
  }

  co_return modified ? HandlerResult::MODIFIED : HandlerResult::FORWARD;
}

constexpr on_message_t C_N_6x = &C_6x<G_SendGuildCard_DCNTE_6x06>;
constexpr on_message_t C_D_6x = &C_6x<G_SendGuildCard_DC_6x06>;
constexpr on_message_t C_P_6x = &C_6x<G_SendGuildCard_PC_6x06>;
constexpr on_message_t C_G_6x = &C_6x<G_SendGuildCard_GC_6x06>;
constexpr on_message_t C_X_6x = &C_6x<G_SendGuildCard_XB_6x06>;
constexpr on_message_t C_B_6x = &C_6x<G_SendGuildCard_BB_6x06>;

static asio::awaitable<HandlerResult> C_V123_A0_A1(shared_ptr<Client> c, Channel::Message&) {
  // We override Change Ship and Change Block to send the player back to the
  // original server (ending the proxy session), except on BB.
  c->proxy_session->server_channel->disconnect();
  co_return HandlerResult::SUPPRESS;
}

// Indexed as [command][version][is_from_client]
static_assert(NUM_VERSIONS == 14, "Don\'t forget to update the ProxyCommands handlers table");
static on_message_t handlers[0x100][NUM_VERSIONS][2] = {
    // clang-format off
// CMD     S_PC_PATCH     C          S_BB_PATCH     C          S_DC_NTE       C           S_DC_12_2000   C              S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* 00 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 01 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr,       nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 02 */ {{S_V123U_02_17, nullptr}, {S_V123U_02_17, nullptr}, {S_V123U_02_17, nullptr},  {S_V123U_02_17, nullptr},     {S_V123U_02_17,    nullptr},     {S_V123U_02_17,    nullptr},      {S_V123U_02_17, nullptr},      {S_V123U_02_17, nullptr},      {S_V123U_02_17,    nullptr},     {S_V123U_02_17,    nullptr},      {S_V123U_02_17,    nullptr},      {S_V123U_02_17,    nullptr},      {S_V123U_02_17, nullptr},      {nullptr,      nullptr}},
/* 03 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr,       nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {S_B_03,       nullptr}},
/* 04 */ {{S_U_04,        nullptr}, {S_U_04,        nullptr}, {S_V123_04,     nullptr},  {S_V123_04,     nullptr},     {S_V123_04,        nullptr},     {S_V123_04,        nullptr},      {S_V123_04,     nullptr},      {S_V123_04,     nullptr},      {S_V123_04,        nullptr},     {S_V123_04,        nullptr},      {S_V123_04,        nullptr},      {S_V123_04,        nullptr},      {S_V123_04,     nullptr},      {nullptr,      nullptr}},
/* 05 */ {{nullptr,       nullptr}, {nullptr,       nullptr}, {nullptr,       nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 06 */ {{nullptr,       nullptr}, {nullptr,       nullptr}, {S_V123_06,     nullptr},  {S_V123_06,     C_06},        {S_V123_06,        C_06},        {S_V123_06,        C_06},         {S_V123_06,     C_06},         {S_V123_06,     C_06},         {S_V123_06,        C_06},        {S_V123_06,        C_06},         {S_V123_06,        C_06},         {S_V123_06,        C_06},         {S_V123_06,     C_06},         {nullptr,      C_06}},
/* 07 */ {{nullptr,       nullptr}, {nullptr,       nullptr}, {nullptr,       nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 08 */ {{nullptr,       nullptr}, {nullptr,       nullptr}, {nullptr,       nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 09 */ {{nullptr,       nullptr}, {nullptr,       nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 0A */ {{nullptr,       nullptr}, {nullptr,       nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 0B */ {{nullptr,       nullptr}, {nullptr,       nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 0C */ {{nullptr,       nullptr}, {nullptr,       nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 0D */ {{nullptr,       nullptr}, {nullptr,       nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 0E */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr,       nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 0F */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH     C          S_DC_NTE       C           S_DC_12_2000   C              S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* 10 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 11 */ {{nullptr,       nullptr}, {nullptr,       nullptr}, {nullptr,       nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 12 */ {{nullptr,       nullptr}, {nullptr,       nullptr}, {nullptr,       nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 13 */ {{nullptr,       nullptr}, {nullptr,       nullptr}, {S_13_A7,       nullptr},  {S_13_A7,       nullptr},     {S_13_A7,          nullptr},     {S_13_A7,          nullptr},      {S_13_A7,       nullptr},      {S_13_A7,       nullptr},      {S_13_A7,          nullptr},     {S_13_A7,          nullptr},      {S_13_A7,          nullptr},      {S_13_A7,          nullptr},      {S_13_A7,       nullptr},      {S_13_A7,      nullptr}},
/* 14 */ {{S_19_U_14,     nullptr}, {S_19_U_14,     nullptr}, {nullptr,       nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 15 */ {{nullptr,       nullptr}, {nullptr,       nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 16 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr,       nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 17 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_V123U_02_17, nullptr},  {S_V123U_02_17, nullptr},     {S_V123U_02_17,    nullptr},     {S_V123U_02_17,    nullptr},      {S_V123U_02_17, nullptr},      {S_V123U_02_17, nullptr},      {S_V123U_02_17,    nullptr},     {S_V123U_02_17,    nullptr},      {S_V123U_02_17,    nullptr},      {S_V123U_02_17,    nullptr},      {S_V123U_02_17, nullptr},      {S_invalid,    nullptr}},
/* 18 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr,       nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {S_invalid,    nullptr}},
/* 19 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_19_U_14,     nullptr},  {S_19_U_14,     nullptr},     {S_19_U_14,        nullptr},     {S_19_U_14,        nullptr},      {S_19_U_14,     nullptr},      {S_19_U_14,     nullptr},      {S_19_U_14,        nullptr},     {S_19_U_14,        nullptr},      {S_19_U_14,        nullptr},      {S_19_U_14,        nullptr},      {S_19_U_14,     nullptr},      {S_19_U_14,    nullptr}},
/* 1A */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr,       nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {S_V3_1A_D5,       nullptr},      {S_V3_1A_D5,       nullptr},      {S_V3_1A_D5,       nullptr},      {S_V3_1A_D5,    nullptr},      {nullptr,      nullptr}},
/* 1B */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr,       nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {S_invalid,    nullptr}},
/* 1C */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr,       nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {S_invalid,    nullptr}},
/* 1D */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_1D,          C_1D},     {S_1D,          C_1D},        {S_1D,             C_1D},        {S_1D,             C_1D},         {S_1D,          C_1D},         {S_1D,          C_1D},         {S_1D,             C_1D},        {S_1D,             C_1D},         {S_1D,             C_1D},         {S_1D,             C_1D},         {S_1D,          C_1D},         {S_1D,         C_1D}},
/* 1E */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 1F */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr,       nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH     C          S_DC_NTE       C           S_DC_12_2000   C              S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* 20 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 21 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 22 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_B_22,       nullptr}},
/* 23 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* 24 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* 25 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* 26 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 27 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 28 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 29 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 2A */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 2B */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 2C */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 2D */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 2E */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 2F */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH     C          S_DC_NTE       C           S_DC_12_2000   C              S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* 30 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 31 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 32 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 33 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 34 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 35 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 36 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 37 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 38 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 39 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 3A */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 3B */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 3C */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 3D */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 3E */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 3F */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH     C          S_DC_NTE       C           S_DC_12_2000   C              S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* 40 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     C_40},        {S_invalid,        C_40},        {S_invalid,        C_40},         {S_invalid,     C_40},         {S_invalid,     C_40},         {S_invalid,        C_40},        {S_invalid,        C_40},         {S_invalid,        C_40},         {S_invalid,        C_40},         {S_invalid,     C_40},         {S_invalid,    C_40}},
/* 41 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_DGX_41,      nullptr},  {S_DGX_41,      nullptr},     {S_DGX_41,         nullptr},     {S_DGX_41,         nullptr},      {S_P_41,        nullptr},      {S_P_41,        nullptr},      {S_DGX_41,         nullptr},     {S_DGX_41,         nullptr},      {S_DGX_41,         nullptr},      {S_DGX_41,         nullptr},      {S_DGX_41,      nullptr},      {S_B_41,       nullptr}},
/* 42 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 43 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 44 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_D_44_A6,     nullptr},     {S_D_44_A6,        nullptr},     {S_D_44_A6,        nullptr},      {S_PG_44_A6,    nullptr},      {S_PG_44_A6,    nullptr},      {S_D_44_A6,        nullptr},     {S_PG_44_A6,       nullptr},      {S_PG_44_A6,       nullptr},      {S_PG_44_A6,       nullptr},      {S_X_44_A6,     nullptr},      {S_B_44_A6,    nullptr}},
/* 45 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 46 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 47 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 48 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 49 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 4A */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 4B */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 4C */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 4D */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 4E */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 4F */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH     C          S_DC_NTE       C           S_DC_12_2000   C              S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* 50 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 51 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 52 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 53 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 54 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 55 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 56 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 57 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 58 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 59 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 5A */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 5B */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 5C */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 5D */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 5E */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 5F */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH     C          S_DC_NTE       C           S_DC_12_2000   C              S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* 60 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_6x,          C_N_6x},   {S_6x,          C_D_6x},      {S_6x,             C_D_6x},      {S_6x,             C_D_6x},       {S_6x,          C_P_6x},       {S_6x,          C_P_6x},       {S_6x,             C_D_6x},      {S_6x,             C_G_6x},       {S_6x,             C_G_6x},       {S_6x,             C_G_6x},       {S_6x,          C_X_6x},       {S_6x,         C_B_6x}},
/* 61 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        C_GXB_61},     {S_invalid,        C_GXB_61},     {S_invalid,        C_GXB_61},     {S_invalid,     C_GXB_61},     {S_invalid,    C_GXB_61}},
/* 62 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_6x,          C_N_6x},   {S_6x,          C_D_6x},      {S_6x,             C_D_6x},      {S_6x,             C_D_6x},       {S_6x,          C_P_6x},       {S_6x,          C_P_6x},       {S_6x,             C_D_6x},      {S_6x,             C_G_6x},       {S_6x,             C_G_6x},       {S_6x,             C_G_6x},       {S_6x,          C_X_6x},       {S_6x,         C_B_6x}},
/* 63 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 64 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_N_64,        nullptr},  {S_N_64,        nullptr},     {S_D_64,           nullptr},     {S_D_64,           nullptr},      {S_P_64,        nullptr},      {S_P_64,        nullptr},      {S_G_64,           nullptr},     {S_G_64,           nullptr},      {S_G_64,           nullptr},      {S_G_64,           nullptr},      {S_X_64,        nullptr},      {S_B_64,       nullptr}},
/* 65 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_N_65_67_68,  nullptr},  {S_N_65_67_68,  nullptr},      {S_DG_65_67_68_EB, nullptr},     {S_DG_65_67_68_EB, nullptr},      {S_P_65_67_68,  nullptr},      {S_P_65_67_68,  nullptr},      {S_DG_65_67_68_EB, nullptr},     {S_DG_65_67_68_EB, nullptr},      {S_DG_65_67_68_EB, nullptr},      {S_DG_65_67_68_EB, nullptr},      {S_X_65_67_68,  nullptr},      {S_B_65_67_68, nullptr}},
/* 66 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_66_69_E9,    nullptr},  {S_66_69_E9,    nullptr},     {S_66_69_E9,       nullptr},     {S_66_69_E9,       nullptr},      {S_66_69_E9,    nullptr},      {S_66_69_E9,    nullptr},      {S_66_69_E9,       nullptr},     {S_66_69_E9,       nullptr},      {S_66_69_E9,       nullptr},      {S_66_69_E9,       nullptr},      {S_66_69_E9,    nullptr},      {S_66_69_E9,   nullptr}},
/* 67 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_N_65_67_68,  nullptr},  {S_N_65_67_68,  nullptr},      {S_DG_65_67_68_EB, nullptr},     {S_DG_65_67_68_EB, nullptr},      {S_P_65_67_68,  nullptr},      {S_P_65_67_68,  nullptr},      {S_DG_65_67_68_EB, nullptr},     {S_DG_65_67_68_EB, nullptr},      {S_DG_65_67_68_EB, nullptr},      {S_DG_65_67_68_EB, nullptr},      {S_X_65_67_68,  nullptr},      {S_B_65_67_68, nullptr}},
/* 68 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_N_65_67_68,  nullptr},  {S_N_65_67_68,  nullptr},      {S_DG_65_67_68_EB, nullptr},     {S_DG_65_67_68_EB, nullptr},      {S_P_65_67_68,  nullptr},      {S_P_65_67_68,  nullptr},      {S_DG_65_67_68_EB, nullptr},     {S_DG_65_67_68_EB, nullptr},      {S_DG_65_67_68_EB, nullptr},      {S_DG_65_67_68_EB, nullptr},      {S_X_65_67_68,  nullptr},      {S_B_65_67_68, nullptr}},
/* 69 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_66_69_E9,    nullptr},  {S_66_69_E9,    nullptr},     {S_66_69_E9,       nullptr},     {S_66_69_E9,       nullptr},      {S_66_69_E9,    nullptr},      {S_66_69_E9,    nullptr},      {S_66_69_E9,       nullptr},     {S_66_69_E9,       nullptr},      {S_66_69_E9,       nullptr},      {S_66_69_E9,       nullptr},      {S_66_69_E9,    nullptr},      {S_66_69_E9,   nullptr}},
/* 6A */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 6B */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 6C */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_6x,          C_N_6x},   {S_6x,          C_D_6x},      {S_6x,             C_D_6x},      {S_6x,             C_D_6x},       {S_6x,          C_P_6x},       {S_6x,          C_P_6x},       {S_6x,             C_D_6x},      {S_6x,             C_G_6x},       {S_6x,             C_G_6x},       {S_6x,             C_G_6x},       {S_6x,          C_X_6x},       {S_6x,         C_B_6x}},
/* 6D */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_6x,          C_N_6x},   {S_6x,          C_D_6x},      {S_6x,             C_D_6x},      {S_6x,             C_D_6x},       {S_6x,          C_P_6x},       {S_6x,          C_P_6x},       {S_6x,             C_D_6x},      {S_6x,             C_G_6x},       {S_6x,             C_G_6x},       {S_6x,             C_G_6x},       {S_6x,          C_X_6x},       {S_6x,         C_B_6x}},
/* 6E */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 6F */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH     C          S_DC_NTE       C           S_DC_12_2000   C              S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* 70 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 71 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 72 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 73 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 74 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 75 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 76 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 77 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 78 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 79 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 7A */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 7B */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 7C */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 7D */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 7E */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 7F */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH     C          S_DC_NTE       C           S_DC_12_2000   C              S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* 80 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr,       nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 81 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_DGX_81,      C_DGX_81}, {S_DGX_81,      C_DGX_81},    {S_DGX_81,         C_DGX_81},    {S_DGX_81,         C_DGX_81},     {S_P_81,        C_P_81},       {S_P_81,        C_P_81},       {S_DGX_81,         C_DGX_81},    {S_DGX_81,         C_DGX_81},     {S_DGX_81,         C_DGX_81},     {S_DGX_81,         C_DGX_81},     {S_DGX_81,      C_DGX_81},     {S_B_81,       C_B_81}},
/* 82 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 83 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr,       nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 84 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 85 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 86 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 87 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 88 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr,       nullptr},  {S_88,          nullptr},     {S_88,             nullptr},     {S_88,             nullptr},      {S_88,          nullptr},      {S_88,          nullptr},      {S_88,             nullptr},     {S_88,             nullptr},      {S_88,             nullptr},      {S_88,             nullptr},      {S_88,          nullptr},      {S_88,         nullptr}},
/* 89 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr,       nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 8A */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr,       nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 8B */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 8C */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 8D */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr,       nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 8E */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr,       nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 8F */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {nullptr,       nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH     C          S_DC_NTE       C           S_DC_12_2000   C              S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* 90 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 91 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 92 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 93 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 94 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 95 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 96 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 97 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_97,          nullptr},     {S_97,             nullptr},     {S_97,             nullptr},      {S_97,          nullptr},      {S_97,          nullptr},      {S_97,             nullptr},     {S_97,             nullptr},      {S_97,             nullptr},      {S_97,             nullptr},      {S_97,          nullptr},      {nullptr,      nullptr}},
/* 98 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     C_98},        {S_invalid,        C_98},        {S_invalid,        C_98},         {S_invalid,     C_98},         {S_invalid,     C_98},         {S_invalid,        C_98},        {S_invalid,        C_98},         {S_invalid,        C_98},         {S_invalid,        C_98},         {S_invalid,     C_98},         {S_invalid,    C_98}},
/* 99 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 9A */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {S_G_9A,           nullptr},      {S_G_9A,           nullptr},      {S_G_9A,           nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 9B */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 9C */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* 9D */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 9E */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* 9F */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH     C          S_DC_NTE       C           S_DC_12_2000   C              S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* A0 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {nullptr,       C_V123_A0_A1},{nullptr,          C_V123_A0_A1},{nullptr,          C_V123_A0_A1}, {nullptr,       C_V123_A0_A1}, {nullptr,       C_V123_A0_A1}, {nullptr,          C_V123_A0_A1},{nullptr,          C_V123_A0_A1}, {nullptr,          C_V123_A0_A1}, {nullptr,          C_V123_A0_A1}, {nullptr,       C_V123_A0_A1}, {nullptr,      nullptr}},
/* A1 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {nullptr,       C_V123_A0_A1},{nullptr,          C_V123_A0_A1},{nullptr,          C_V123_A0_A1}, {nullptr,       C_V123_A0_A1}, {nullptr,       C_V123_A0_A1}, {nullptr,          C_V123_A0_A1},{nullptr,          C_V123_A0_A1}, {nullptr,          C_V123_A0_A1}, {nullptr,          C_V123_A0_A1}, {nullptr,       C_V123_A0_A1}, {nullptr,      nullptr}},
/* A2 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* A3 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* A4 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* A5 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* A6 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_D_44_A6,     nullptr},     {S_D_44_A6,        nullptr},     {S_D_44_A6,        nullptr},      {S_PG_44_A6,    nullptr},      {S_PG_44_A6,    nullptr},      {S_D_44_A6,        nullptr},     {S_PG_44_A6,       nullptr},      {S_PG_44_A6,       nullptr},      {S_PG_44_A6,       nullptr},      {S_X_44_A6,     nullptr},      {S_B_44_A6,    nullptr}},
/* A7 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_13_A7,       nullptr},     {S_13_A7,          nullptr},     {S_13_A7,          nullptr},      {S_13_A7,       nullptr},      {S_13_A7,       nullptr},      {S_13_A7,          nullptr},     {S_13_A7,          nullptr},      {S_13_A7,          nullptr},      {S_13_A7,          nullptr},      {S_13_A7,       nullptr},      {S_13_A7,      nullptr}},
/* A8 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* A9 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* AA */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* AB */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* AC */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_AC,             nullptr},      {S_AC,             nullptr},      {S_AC,             nullptr},      {S_AC,          nullptr},      {S_AC,         nullptr}},
/* AD */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* AE */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* AF */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH     C          S_DC_NTE       C           S_DC_12_2000   C              S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* B0 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* B1 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {nullptr,       nullptr},     {S_B1,             nullptr},     {S_B1,             nullptr},      {S_B1,          nullptr},      {S_B1,          nullptr},      {S_B1,             nullptr},     {S_B1,             nullptr},      {S_B1,             nullptr},      {S_B1,             nullptr},      {S_B1,          nullptr},      {S_B1,         nullptr}},
/* B2 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {S_B2<false>,      nullptr},      {S_B2<false>,   nullptr},      {S_B2<false>,   nullptr},      {S_B2<true>,       nullptr},     {S_B2<true>,       nullptr},      {S_B2<true>,       nullptr},      {S_B2<true>,       nullptr},      {S_B2<false>,   nullptr},      {S_B2<false>,  nullptr}},
/* B3 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        C_B3},         {S_invalid,     C_B3},         {S_invalid,     C_B3},         {S_invalid,        C_B3},        {S_invalid,        C_B3},         {S_invalid,        C_B3},         {S_invalid,        C_B3},         {S_invalid,     C_B3},         {S_invalid,    C_B3}},
/* B4 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* B5 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* B6 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* B7 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_G_B7,           nullptr},      {S_G_B7,           nullptr},      {S_G_B7,           nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* B8 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_G_B8,           nullptr},      {S_G_B8,           nullptr},      {S_G_B8,           nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* B9 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_G_B9,           C_G_B9},       {S_G_B9,           C_G_B9},       {S_G_B9,           C_G_B9},       {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* BA */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_G_BA,           nullptr},      {S_G_BA,           nullptr},      {S_G_BA,           nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* BB */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* BC */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* BD */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* BE */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* BF */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH     C          S_DC_NTE       C           S_DC_12_2000   C              S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* C0 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {nullptr,       nullptr},     {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* C1 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* C2 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* C3 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* C4 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_DGX_C4,         nullptr},      {S_P_C4,        nullptr},      {S_P_C4,        nullptr},      {S_DGX_C4,         nullptr},     {S_DGX_C4,         nullptr},      {S_DGX_C4,         nullptr},      {S_DGX_C4,         nullptr},      {S_DGX_C4,      nullptr},      {S_B_C4,       nullptr}},
/* C5 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,       nullptr},      {nullptr,          nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* C6 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* C7 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* C8 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* C9 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_6x,             nullptr},      {S_6x,             nullptr},      {S_6x,             nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* CA */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* CB */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_6x,             nullptr},      {S_6x,             nullptr},      {S_6x,             nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* CC */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* CD */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* CE */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* CF */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH     C          S_DC_NTE       C           S_DC_12_2000   C              S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* D0 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* D1 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* D2 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* D3 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* D4 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* D5 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_V3_1A_D5,       nullptr},      {S_V3_1A_D5,       nullptr},      {S_V3_1A_D5,       nullptr},      {S_V3_1A_D5,    nullptr},      {nullptr,      nullptr}},
/* D6 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* D7 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* D8 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,       nullptr},      {nullptr,      nullptr}},
/* D9 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        C_GX_D9},      {S_invalid,        C_GX_D9},      {S_invalid,        C_GX_D9},      {S_invalid,     C_GX_D9},      {S_invalid,    C_B_D9}},
/* DA */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_V3_BB_DA,       nullptr},      {S_V3_BB_DA,       nullptr},      {S_V3_BB_DA,       nullptr},      {S_V3_BB_DA,    nullptr},      {S_V3_BB_DA,   nullptr}},
/* DB */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* DC */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* DD */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* DE */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* DF */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH     C          S_DC_NTE       C           S_DC_12_2000   C              S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* E0 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {S_invalid,     nullptr},      {nullptr,      C_B_E0}},
/* E1 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* E2 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {S_invalid,     nullptr},      {S_B_E2,       nullptr}},
/* E3 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* E4 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_G_E4,           nullptr},      {S_G_E4,           nullptr},      {S_G_E4,           nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* E5 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* E6 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {S_invalid,     nullptr},      {S_B_E6,       nullptr}},
/* E7 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {S_invalid,     nullptr},      {S_B_E7,       nullptr}},
/* E8 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_E8,             nullptr},      {S_E8,             nullptr},      {S_E8,             nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* E9 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_66_69_E9,       nullptr},      {S_66_69_E9,       nullptr},      {S_66_69_E9,       nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* EA */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* EB */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_DG_65_67_68_EB, nullptr},      {S_DG_65_67_68_EB, nullptr},      {S_DG_65_67_68_EB, nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* EC */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* ED */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* EE */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {nullptr,          nullptr},      {nullptr,          nullptr},      {nullptr,          nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* EF */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_G_EF,           nullptr},      {S_G_EF,           nullptr},      {S_G_EF,           nullptr},      {S_invalid,     nullptr},      {S_B_EF,       nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH     C          S_DC_NTE       C           S_DC_12_2000   C              S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
/* F0 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {nullptr,      nullptr}},
/* F1 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* F2 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* F3 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* F4 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* F5 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* F6 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* F7 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* F8 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* F9 */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* FA */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* FB */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* FC */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* FD */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* FE */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
/* FF */ {{S_invalid,     nullptr}, {S_invalid,     nullptr}, {S_invalid,     nullptr},  {S_invalid,     nullptr},     {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,     nullptr},      {S_invalid,        nullptr},     {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,        nullptr},      {S_invalid,     nullptr},      {S_invalid,    nullptr}},
// CMD     S_PC_PATCH     C          S_BB_PATCH     C          S_DC_NTE       C           S_DC_12_2000   C              S_DC_V1           C              S_DC_V2           C               S_PC_NTE       C               S_PC_V2        C               S_GC_NTE          C              S_GC_V3           C               S_GC_EP3_NTE      C               S_GC_EP3          C               S_XB_V3        C               S_BB_V4       C
    // clang-format on
};

static on_message_t get_handler(Version version, bool from_server, uint8_t command) {
  size_t version_index = static_cast<size_t>(version);
  if (version_index >= sizeof(handlers[0]) / sizeof(handlers[0][0])) {
    throw logic_error("invalid game version on proxy server");
  }
  auto ret = handlers[command][version_index][!from_server];
  return ret ? ret : default_handler;
}

asio::awaitable<void> on_proxy_command(shared_ptr<Client> c, bool from_server, unique_ptr<Channel::Message> msg) {
  auto fn = get_handler(c->version(), from_server, msg->command);
  try {
    auto res = co_await fn(c, *msg);
    if (res == HandlerResult::FORWARD) {
      forward_command(c, !from_server, *msg, false);
    } else if (res == HandlerResult::MODIFIED) {
      c->log.info_f("The preceding command from the {} was modified in transit", from_server ? "server" : "client");
      forward_command(c, !from_server, *msg);
    } else if (res == HandlerResult::SUPPRESS) {
      c->log.info_f("The preceding command from the {} was not forwarded", from_server ? "server" : "client");
    } else {
      throw logic_error("invalid handler result");
    }
  } catch (const exception& e) {
    c->log.error_f("Error in proxy command handler: {}", e.what());
    if (c->proxy_session && c->proxy_session->server_channel) {
      c->proxy_session->server_channel->disconnect();
    }
  }
}

asio::awaitable<void> handle_proxy_server_commands(
    shared_ptr<Client> c, shared_ptr<ProxySession> ses, shared_ptr<Channel> channel) {
  std::string error_str;
  // server_channel can be changed by receiving a 19 command, hence the
  // exception handler is inside the loop here
  while ((c->proxy_session == ses) && (ses->server_channel == channel) && channel->connected()) {
    unique_ptr<Channel::Message> msg;
    try {
      msg = make_unique<Channel::Message>(co_await channel->recv());
      if (c->proxy_session == ses) {
        for (size_t z = 0; z < std::min<size_t>(c->proxy_session->prev_server_command_bytes.size(), msg->data.size()); z++) {
          c->proxy_session->prev_server_command_bytes[z] = msg->data[z];
        }
        asio::co_spawn(co_await asio::this_coro::executor, on_proxy_command(c, true, std::move(msg)), asio::detached);
      }
    } catch (const std::system_error& e) {
      c->log.info_f("Error in proxy server channel handler (command {:04X}): {}", msg ? msg->command : 0, e.what());
      const auto& ec = e.code();
      if (ec == asio::error::eof || ec == asio::error::connection_reset) {
        error_str = "Server channel\ndisconnected";
      } else if (ec == asio::error::operation_aborted) {
        // This happens when the player chooses Change Ship/Change Block, so we
        // don't show an error message
      } else {
        error_str = e.what();
      }
      channel->disconnect();
    } catch (const exception& e) {
      c->log.info_f("Error in proxy server channel handler (command {:04X}): {}", msg ? msg->command : 0, e.what());
      error_str = e.what();
      channel->disconnect();
    }
  }
  if (c->proxy_session == ses && ses->server_channel == channel) {
    c->log.info_f("Ending proxy session");
    co_await end_proxy_session(c, error_str);
  }
}
