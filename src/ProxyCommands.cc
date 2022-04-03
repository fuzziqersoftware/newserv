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
#include <phosg/Network.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>
#include <phosg/Time.hh>

#include "PSOProtocol.hh"
#include "SendCommands.hh"
#include "ReceiveCommands.hh"
#include "ReceiveSubcommands.hh"

using namespace std;



static void forward_command(ProxyServer::LinkedSession& session, bool to_server,
    uint16_t command, uint32_t flag, string& data) {
  auto* bev = to_server ? session.server_bev.get() : session.client_bev.get();
  if (!bev) {
    session.log(WARNING, "No endpoint is present; dropping command");
  } else {
    // Note: we intentionally don't pass name_str here because we already
    // printed the command before calling the handler
    send_command(
        bev,
        session.version,
        to_server ? session.server_output_crypt.get() : session.client_output_crypt.get(),
        command,
        flag,
        data.data(),
        data.size());
  }
}

static void check_implemented_subcommand(uint64_t id, const string& data) {
  if (data.size() < 4) {
    log(WARNING, "[ProxyServer/%08" PRIX64 "] Received broadcast/target command with no contents", id);
  } else {
    if (!subcommand_is_implemented(data[0])) {
      log(WARNING, "[ProxyServer/%08" PRIX64 "] Received subcommand %02hhX which is not implemented on the server",
          id, data[0]);
    }
  }
}



// Command handlers. These are called to preprocess or react to specific
// commands in either direction. If they return true, the command (which the
// function may have modified) is forwarded to the other end; if they return
// false; it is not.

static bool process_default(shared_ptr<ServerState>,
    ProxyServer::LinkedSession&, uint16_t, uint32_t, string&) {
  return true;
}

static bool process_server_gc_9A(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string&) {
  if (!session.license) {
    return true;
  }

  C_LoginWithUnusedSpace_GC_9E cmd;
  if (session.remote_guild_card_number == 0) {
    cmd.player_tag = 0xFFFF0000;
    cmd.guild_card_number = 0xFFFFFFFF;
  } else {
    cmd.player_tag = 0x00010000;
    cmd.guild_card_number = session.remote_guild_card_number;
  }
  cmd.sub_version = session.sub_version;
  cmd.unused2.data()[1] = 1;
  cmd.serial_number = string_printf("%08" PRIX32 "", session.license->serial_number);
  cmd.access_key = session.license->access_key;
  cmd.serial_number2 = cmd.serial_number;
  cmd.access_key2 = cmd.access_key;
  cmd.name = session.character_name;
  cmd.client_config.data = session.remote_client_config_data;

  // If there's a guild card number, a shorter 9E is sent that ends
  // right after the client config data

  session.send_to_end(
      true, 0x9E, 0x01, &cmd, 
      sizeof(C_LoginWithUnusedSpace_GC_9E) - (session.remote_guild_card_number ? sizeof(cmd.unused_space) : 0));
  return false;
}

static bool process_server_pc_gc_patch_02_17(shared_ptr<ServerState> s,
    ProxyServer::LinkedSession& session, uint16_t command, uint32_t flag, string& data) {
  if (session.version == GameVersion::PATCH && command == 0x17) {
    throw invalid_argument("patch server sent 17 server init");
  }

  // Most servers don't include after_message or have a shorter
  // after_message than newserv does, so don't require it
  const auto& cmd = check_size_t<S_ServerInit_DC_PC_GC_02_17>(data,
      offsetof(S_ServerInit_DC_PC_GC_02_17, after_message),
      sizeof(S_ServerInit_DC_PC_GC_02_17));

  if (!session.license) {
    session.log(INFO, "No license in linked session");

    // We have to forward the command before setting up encryption, so the
    // client will be able to understand it.
    forward_command(session, false, command, flag, data);

    if (session.version == GameVersion::GC) {
      session.server_input_crypt.reset(new PSOGCEncryption(cmd.server_key));
      session.server_output_crypt.reset(new PSOGCEncryption(cmd.client_key));
      session.client_input_crypt.reset(new PSOGCEncryption(cmd.client_key));
      session.client_output_crypt.reset(new PSOGCEncryption(cmd.server_key));
    } else { // PC or patch server (they both use PC encryption)
      session.server_input_crypt.reset(new PSOPCEncryption(cmd.server_key));
      session.server_output_crypt.reset(new PSOPCEncryption(cmd.client_key));
      session.client_input_crypt.reset(new PSOPCEncryption(cmd.client_key));
      session.client_output_crypt.reset(new PSOPCEncryption(cmd.server_key));
    }

    return false;
  }

  session.log(INFO, "Existing license in linked session");

  // This isn't forwarded to the client, so don't recreate the client's crypts
  if (session.version == GameVersion::PATCH) {
    throw logic_error("patch session is indirect");
  } else if (session.version == GameVersion::PC) {
    session.server_input_crypt.reset(new PSOPCEncryption(cmd.server_key));
    session.server_output_crypt.reset(new PSOPCEncryption(cmd.client_key));
  } else if (session.version == GameVersion::GC) {
    session.server_input_crypt.reset(new PSOGCEncryption(cmd.server_key));
    session.server_output_crypt.reset(new PSOGCEncryption(cmd.client_key));
  } else {
    throw invalid_argument("unsupported version");
  }

  // Respond with an appropriate login command. We don't let the
  // client do this because it believes it already did (when it was
  // in an unlinked session).
  if (session.version == GameVersion::PC) {
    C_Login_PC_9D cmd;
    if (session.remote_guild_card_number == 0) {
      cmd.player_tag = 0xFFFF0000;
      cmd.guild_card_number = 0xFFFFFFFF;
    } else {
      cmd.player_tag = 0x00010000;
      cmd.guild_card_number = session.remote_guild_card_number;
    }
    cmd.unused = 0xFFFFFFFFFFFF0000;
    cmd.sub_version = session.sub_version;
    cmd.unused2.data()[1] = 1;
    cmd.serial_number = string_printf("%08" PRIX32 "",
        session.license->serial_number);
    cmd.access_key = session.license->access_key;
    cmd.serial_number2 = cmd.serial_number;
    cmd.access_key2 = cmd.access_key;
    cmd.name = session.character_name;
    session.send_to_end(true, 0x9D, 0x00, &cmd, sizeof(cmd));
    return false;

  } else if (session.version == GameVersion::GC) {
    if (command == 0x17) {
      C_VerifyLicense_GC_DB cmd;
      cmd.serial_number = string_printf("%08" PRIX32 "",
          session.license->serial_number);
      cmd.access_key = session.license->access_key;
      cmd.sub_version = session.sub_version;
      cmd.serial_number2 = cmd.serial_number;
      cmd.access_key2 = cmd.access_key;
      cmd.password = session.license->gc_password;
      session.send_to_end(true, 0xDB, 0x00, &cmd, sizeof(cmd));
      return false;

    } else {
      // For command 02, send the same as if we had received 9A from the server
      return process_server_gc_9A(s, session, command, flag, data);
    }

  } else {
    throw logic_error("invalid game version in server init handler");
  }
}

static bool process_server_dc_pc_gc_04(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  // Some servers send a short 04 command if they don't use all of the 0x20
  // bytes available. We should be prepared to handle that.
  auto& cmd = check_size_t<S_UpdateClientConfig_DC_PC_GC_04>(data,
      offsetof(S_UpdateClientConfig_DC_PC_GC_04, cfg),
      sizeof(S_UpdateClientConfig_DC_PC_GC_04));

  // If this is a licensed session, hide the guild card number assigned by the
  // remote server so the client doesn't see it change. If this is an unlicensed
  // session, then the client never received a guild card number from newserv
  // anyway, so we can let the client see the number from the remote server.
  bool had_guild_card_number = (session.remote_guild_card_number != 0);
  session.remote_guild_card_number = cmd.guild_card_number;
  session.log(INFO, "Remote guild card number set to %" PRIX32,
      session.remote_guild_card_number);
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
      min<size_t>(data.size() - sizeof(S_UpdateClientConfig_DC_PC_GC_04),
        session.remote_client_config_data.bytes()));

  // If the guild card number was not set, pretend (to the server) that this is
  // the first 04 command the client has received. The client responds with a 96
  // (checksum) in that case.
  if (!had_guild_card_number) {
    // We don't actually have a client checksum, of course... hopefully just
    // random data will do (probably no private servers check this at all)
    le_uint64_t checksum = random_object<uint64_t>() & 0x0000FFFFFFFFFFFF;
    session.send_to_end(true, 0x96, 0x00, &checksum, sizeof(checksum));
  }

  return true;
}

static bool process_server_dc_pc_gc_06(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  if (session.license) {
    auto& cmd = check_size_t<SC_TextHeader_01_06_11_B0>(data,
        sizeof(SC_TextHeader_01_06_11_B0), 0xFFFF);
    if (cmd.guild_card_number == session.remote_guild_card_number) {
      cmd.guild_card_number = session.license->serial_number;
    }
  }
  return true;
}

template <typename CmdT>
static bool process_server_41(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  if (session.license) {
    auto& cmd = check_size_t<CmdT>(data);
    if (cmd.searcher_guild_card_number == session.remote_guild_card_number) {
      cmd.searcher_guild_card_number = session.license->serial_number;
    }
    if (cmd.result_guild_card_number == session.remote_guild_card_number) {
      cmd.result_guild_card_number = session.license->serial_number;
    }
  }
  return true;
}

template <typename CmdT>
static bool process_server_81(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  if (session.license) {
    auto& cmd = check_size_t<CmdT>(data);
    if (cmd.from_guild_card_number == session.remote_guild_card_number) {
      cmd.from_guild_card_number = session.license->serial_number;
    }
    if (cmd.to_guild_card_number == session.remote_guild_card_number) {
      cmd.to_guild_card_number = session.license->serial_number;
    }
  }
  return true;
}

static bool process_server_88(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t flag, string& data) {
  if (session.license) {
    size_t expected_size = sizeof(S_ArrowUpdateEntry_88) * flag;
    auto* entries = &check_size_t<S_ArrowUpdateEntry_88>(data,
        expected_size, expected_size);
    for (size_t x = 0; x < flag; x++) {
      if (entries[x].guild_card_number == session.remote_guild_card_number) {
        entries[x].guild_card_number = session.license->serial_number;
      }
    }
  }
  return true;
}

template <typename CmdT>
static bool process_server_C4(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t flag, string& data) {
  if (session.license) {
    size_t expected_size = sizeof(CmdT) * flag;
    auto* entries = &check_size_t<CmdT>(data, expected_size, expected_size);
    for (size_t x = 0; x < flag; x++) {
      if (entries[x].guild_card_number == session.remote_guild_card_number) {
        entries[x].guild_card_number = session.license->serial_number;
      }
    }
  }
  return true;
}

static bool process_server_gc_E4(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  auto& cmd = check_size_t<S_CardLobbyGame_GC_E4>(data);
  for (size_t x = 0; x < 4; x++) {
    if (cmd.entries[x].guild_card_number == session.remote_guild_card_number) {
      cmd.entries[x].guild_card_number = session.license->serial_number;
    }
  }
  return true;
}

static bool process_server_19(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  // This weird maximum size is here to properly handle the version-split
  // command that some servers (including newserv) use on port 9100
  auto& cmd = check_size_t<S_Reconnect_19>(data, sizeof(S_Reconnect_19), 0xB0);
  memset(&session.next_destination, 0, sizeof(session.next_destination));
  struct sockaddr_in* sin = reinterpret_cast<struct sockaddr_in*>(
      &session.next_destination);
  sin->sin_family = AF_INET;
  sin->sin_addr.s_addr = cmd.address.load_raw();
  sin->sin_port = htons(cmd.port);

  if (!session.client_bev.get()) {
    session.log(WARNING, "Received reconnect command with no destination present");

  } else {
    // If the client is on a virtual connection (fd < 0), only change
    // the port (so we'll know which version to treat the next
    // connection as). It's better to leave the address as-is so we
    // can circumvent the Plus/Ep3 same-network-server check.
    int fd = bufferevent_getfd(session.client_bev.get());
    if (fd >= 0) {
      struct sockaddr_storage sockname_ss;
      socklen_t len = sizeof(sockname_ss);
      getsockname(fd, reinterpret_cast<struct sockaddr*>(&sockname_ss), &len);
      if (sockname_ss.ss_family != AF_INET) {
        throw logic_error("existing connection is not ipv4");
      }

      struct sockaddr_in* sockname_sin = reinterpret_cast<struct sockaddr_in*>(
          &sockname_ss);
      cmd.address.store_raw(sockname_sin->sin_addr.s_addr);
      cmd.port = ntohs(sockname_sin->sin_port);

    } else {
      cmd.port = session.local_port;
    }
  }
  return true;
}

static bool process_server_gc_1A_D5(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string&) {
  // If the client has the no-close-confirmation flag set in its
  // newserv client config, send a fake confirmation to the remote
  // server immediately.
  if (session.newserv_client_config.flags & Client::Flag::NO_MESSAGE_BOX_CLOSE_CONFIRMATION) {
    session.send_to_end(true, 0xD6, 0x00, "", 0);
  }
  return true;
}

static bool process_server_60_62_6C_6D_C9_CB(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  check_implemented_subcommand(session.id, data);
  return true;
}

template <typename T>
static bool process_server_44_A6(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t command, uint32_t, string& data) {
  if (session.save_files) {
    const auto& cmd = check_size_t<S_OpenFile_PC_GC_44_A6>(data);
    bool is_download_quest = (command == 0xA6);

    string output_filename = string_printf("%s.%s.%" PRIu64,
        cmd.filename.c_str(),
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
    session.log(INFO, "Opened file %s", output_filename.c_str());
  }
  return true;
}

static bool process_server_13_A7(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  if (session.save_files) {
    const auto& cmd = check_size_t<S_WriteFile_13_A7>(data);

    ProxyServer::LinkedSession::SavingFile* sf = nullptr;
    try {
      sf = &session.saving_files.at(cmd.filename);
    } catch (const out_of_range&) {
      session.log(WARNING, "Received data for non-open file %s", cmd.filename.c_str());
      return true;
    }

    size_t bytes_to_write = cmd.data_size;
    if (bytes_to_write > 0x400) {
      session.log(WARNING, "Chunk data size is invalid; truncating to 0x400");
      bytes_to_write = 0x400;
    }

    session.log(INFO, "Writing %zu bytes to %s", bytes_to_write, sf->output_filename.c_str());
    fwritex(sf->f.get(), cmd.data, bytes_to_write);
    if (bytes_to_write > sf->remaining_bytes) {
      session.log(WARNING, "Chunk size extends beyond original file size; file may be truncated");
      sf->remaining_bytes = 0;
    } else {
      sf->remaining_bytes -= bytes_to_write;
    }

    if (sf->remaining_bytes == 0) {
      session.log(INFO, "File %s is complete", sf->output_filename.c_str());
      session.saving_files.erase(cmd.filename);
    }
  }
  return true;
}

static bool process_server_gc_B8(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  if (session.save_files) {
    if (data.size() < 4) {
      session.log(WARNING, "Card list data size is too small; skipping file");
      return true;
    }

    StringReader r(data);
    size_t size = r.get_u32l();
    if (r.remaining() < size) {
      session.log(WARNING, "Card list data size extends beyond end of command; skipping file");
      return true;
    }

    string output_filename = string_printf("cardupdate.mnr.%" PRIu64, now());
    save_file(output_filename, r.read(size));

    session.log(INFO, "Wrote %zu bytes to %s", size, output_filename.c_str());
  }
  return true;
}

template <typename CmdT>
static bool process_server_65_67_68(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t command, uint32_t flag, string& data) {
  if (command == 0x67) {
    session.lobby_players.clear();
    session.lobby_players.resize(12);
    session.log(INFO, "Cleared lobby players");

    // This command can cause the client to no longer send D6 responses when
    // 1A/D5 large message boxes are closed. newserv keeps track of this
    // behavior in the client config, so if it happens during a proxy session,
    // update the client config that we'll restore if the client uses the change
    // ship or change block command.
    if (session.newserv_client_config.flags & Client::Flag::NO_MESSAGE_BOX_CLOSE_CONFIRMATION_AFTER_LOBBY_JOIN) {
      session.newserv_client_config.flags |= Client::Flag::NO_MESSAGE_BOX_CLOSE_CONFIRMATION;
    }
  }

  size_t expected_size = offsetof(CmdT, entries) + sizeof(typename CmdT::Entry) * flag;
  auto& cmd = check_size_t<CmdT>(data, expected_size, expected_size);

  session.lobby_client_id = cmd.client_id;
  for (size_t x = 0; x < flag; x++) {
    size_t index = cmd.entries[x].lobby_data.client_id;
    if (index >= session.lobby_players.size()) {
      session.log(WARNING, "Ignoring invalid player index %zu at position %zu", index, x);
    } else {
      if (session.license && (cmd.entries[x].lobby_data.guild_card == session.remote_guild_card_number)) {
        cmd.entries[x].lobby_data.guild_card = session.license->serial_number;
      }
      session.lobby_players[index].guild_card_number = cmd.entries[x].lobby_data.guild_card;
      ptext<char, 0x10> name = cmd.entries[x].disp.name;
      session.lobby_players[index].name = name;
      session.log(INFO, "Added lobby player: (%zu) %" PRIu32 " %s",
          index,
          session.lobby_players[index].guild_card_number,
          session.lobby_players[index].name.c_str());
    }
  }

  if (session.override_lobby_event >= 0) {
    cmd.event = session.override_lobby_event;
  }
  if (session.override_lobby_number >= 0) {
    cmd.lobby_number = session.override_lobby_number;
  }

  return true;
}

template <typename CmdT>
static bool process_server_64(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t flag, string& data) {
  // We don't need to clear lobby_players here because we always
  // overwrite all 4 entries for this command
  session.lobby_players.resize(4);
  session.log(INFO, "Cleared lobby players");

  const size_t expected_size = session.sub_version >= 0x40
      ? sizeof(CmdT)
      : offsetof(CmdT, players_ep3);
  auto& cmd = check_size_t<CmdT>(data, expected_size, expected_size);

  session.lobby_client_id = cmd.client_id;
  for (size_t x = 0; x < flag; x++) {
    if (cmd.lobby_data[x].guild_card == session.remote_guild_card_number) {
      cmd.lobby_data[x].guild_card = session.license->serial_number;
    }
    session.lobby_players[x].guild_card_number = cmd.lobby_data[x].guild_card;
    if (data.size() == sizeof(CmdT)) {
      ptext<char, 0x10> name = cmd.players_ep3[x].disp.name;
      session.lobby_players[x].name = name;
    } else {
      session.lobby_players[x].name.clear();
    }
    session.log(INFO, "Added lobby player: (%zu) %" PRIu32 " %s",
        x,
        session.lobby_players[x].guild_card_number,
        session.lobby_players[x].name.c_str());
  }

  if (session.override_section_id >= 0) {
    cmd.section_id = session.override_section_id;
  }
  if (session.override_lobby_event >= 0) {
    cmd.event = session.override_lobby_event;
  }

  return true;
}

static bool process_server_66_69(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  const auto& cmd = check_size_t<S_LeaveLobby_66_69>(data);
  size_t index = cmd.client_id;
  if (index >= session.lobby_players.size()) {
    session.log(WARNING, "Lobby leave command references missing position");
  } else {
    session.lobby_players[index].guild_card_number = 0;
    session.lobby_players[index].name.clear();
    session.log(INFO, "Removed lobby player (%zu)", index);
  }
  return true;
}





static bool process_client_06(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  if (data.size() >= 12) {
    // If this chat message looks like a newserv chat command, suppress it
    if (session.suppress_newserv_commands &&
        (data[8] == '$' || (data[8] == '\t' && data[9] != 'C' && data[10] == '$'))) {
      session.log(WARNING, "Chat message appears to be a server command; dropping it");
      return false;
    } else if (session.enable_chat_filter) {
      add_color_inplace(data.data() + 8, data.size() - 8);
    }
  }
  return true;
}

static bool process_client_40(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  if (session.license) {
    auto& cmd = check_size_t<C_GuildCardSearch_40>(data);
    if (cmd.searcher_guild_card_number == session.license->serial_number) {
      cmd.searcher_guild_card_number = session.remote_guild_card_number;
    }
    if (cmd.target_guild_card_number == session.license->serial_number) {
      cmd.target_guild_card_number = session.remote_guild_card_number;
    }
  }
  return true;
}

template <typename CmdT>
static bool process_client_81(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  if (session.license) {
    auto& cmd = check_size_t<SC_SimpleMail_GC_81>(data);
    if (cmd.from_guild_card_number == session.license->serial_number) {
      cmd.from_guild_card_number = session.remote_guild_card_number;
    }
    if (cmd.to_guild_card_number == session.license->serial_number) {
      cmd.to_guild_card_number = session.remote_guild_card_number;
    }
  }
  return true;
}

template <typename SendGuildCardCmdT>
static bool process_client_60_62_6C_6D_C9_CB(shared_ptr<ServerState> s,
    ProxyServer::LinkedSession& session, uint16_t command, uint32_t flag, string& data) {
  if (session.license && !data.empty() && (data[0] == 0x06)) {
    auto& cmd = check_size_t<SendGuildCardCmdT>(data);
    if (cmd.guild_card_number == session.license->serial_number) {
      cmd.guild_card_number = session.remote_guild_card_number;
    }
  }
  return process_client_60_62_6C_6D_C9_CB<void>(s, session, command, flag, data);
}

template <>
bool process_client_60_62_6C_6D_C9_CB<void>(shared_ptr<ServerState>,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string& data) {
  check_implemented_subcommand(session.id, data);

  if (!data.empty() && (data[0] == 0x05) && (data[data.size() - 1] == 0x01) && session.enable_switch_assist) {
    if (!session.last_switch_enabled_subcommand.empty()) {
      session.log(WARNING, "Switch assist: replaying previous enable subcommand");
      session.send_to_end(true, 0x60, 0x00, session.last_switch_enabled_subcommand);
      session.send_to_end(false, 0x60, 0x00, session.last_switch_enabled_subcommand);
    }
    session.last_switch_enabled_subcommand = data;
  }

  return true;
}

static bool process_client_dc_pc_gc_A0_A1(shared_ptr<ServerState> s,
    ProxyServer::LinkedSession& session, uint16_t, uint32_t, string&) {
  if (!session.license) {
    return true;
  }

  // For licensed sessions, send them back to newserv's main menu instead of
  // going to the remote server's ship/block select menu

  // Delete all the other players
  for (size_t x = 0; x < session.lobby_players.size(); x++) {
    if (session.lobby_players[x].guild_card_number == 0) {
      continue;
    }
    uint8_t leaving_id = x;
    uint8_t leader_id = session.lobby_client_id;
    S_LeaveLobby_66_69 cmd = {leaving_id, leader_id, 0};
    session.send_to_end(false, 0x69, leaving_id, &cmd, sizeof(cmd));
  }

  // Restore newserv_client_config, so the login server gets the client flags
  S_UpdateClientConfig_DC_PC_GC_04 update_client_config_cmd = {
    0x00010000,
    session.license->serial_number,
    session.newserv_client_config,
  };
  session.send_to_end(false, 0x04, 0x00, &update_client_config_cmd, sizeof(update_client_config_cmd));

  static const vector<string> version_to_port_name({
      "dc-login", "pc-login", "bb-patch", "gc-us3", "bb-login"});
  const auto& port_name = version_to_port_name.at(static_cast<size_t>(
      session.version));

  S_Reconnect_19 reconnect_cmd = {
      0, s->name_to_port_config.at(port_name)->port, 0};

  // If the client is on a virtual connection, we can use any address
  // here and they should be able to connect back to the game server. If
  // the client is on a real connection, we'll use the sockname of the
  // existing connection (like we do in the server 19 command handler).
  int fd = bufferevent_getfd(session.client_bev.get());
  if (fd < 0) {
    struct sockaddr_in* dest_sin = reinterpret_cast<struct sockaddr_in*>(&session.next_destination);
    if (dest_sin->sin_family != AF_INET) {
      throw logic_error("ss not AF_INET");
    }
    reconnect_cmd.address.store_raw(dest_sin->sin_addr.s_addr);
  } else {
    struct sockaddr_storage sockname_ss;
    socklen_t len = sizeof(sockname_ss);
    getsockname(fd, reinterpret_cast<struct sockaddr*>(&sockname_ss), &len);
    if (sockname_ss.ss_family != AF_INET) {
      throw logic_error("existing connection is not ipv4");
    }

    struct sockaddr_in* sockname_sin = reinterpret_cast<struct sockaddr_in*>(
        &sockname_ss);
    reconnect_cmd.address.store_raw(sockname_sin->sin_addr.s_addr);
  }

  session.send_to_end(false, 0x19, 0x00, &reconnect_cmd, sizeof(reconnect_cmd));

  return false;
}



typedef bool (*process_command_t)(
    shared_ptr<ServerState> s,
    ProxyServer::LinkedSession& session,
    uint16_t command,
    uint32_t flag,
    string& data);

// The entries in these arrays correspond to the ID of the command received. For
// instance, if a command 6C is received, the function at position 0x6C in the
// array corresponding to the client's version is called.
auto defh = process_default;

static process_command_t dc_server_handlers[0x100] = {
  /* 00 */ defh, defh, defh, defh, process_server_dc_pc_gc_04, defh, process_server_dc_pc_gc_06, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 10 */ defh, defh, defh, process_server_13_A7, defh, defh, defh, defh, defh, process_server_19, defh, defh, defh, defh, defh, defh,
  /* 20 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 30 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 40 */ defh, process_server_41<S_GuildCardSearchResult_DC_GC_41>, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 50 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 60 */ process_server_60_62_6C_6D_C9_CB, defh, process_server_60_62_6C_6D_C9_CB, defh, defh, defh, process_server_66_69, defh, defh, process_server_66_69, defh, defh, process_server_60_62_6C_6D_C9_CB, process_server_60_62_6C_6D_C9_CB, defh, defh,
  /* 70 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 80 */ defh, defh, defh, defh, defh, defh, defh, defh, process_server_88, defh, defh, defh, defh, defh, defh, defh,
  /* 90 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* A0 */ defh, defh, defh, defh, defh, defh, defh, process_server_13_A7, defh, defh, defh, defh, defh, defh, defh, defh,
  /* B0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* C0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* D0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* E0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* F0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
};
static process_command_t pc_server_handlers[0x100] = {
  /* 00 */ defh, defh, process_server_pc_gc_patch_02_17, defh, process_server_dc_pc_gc_04, defh, process_server_dc_pc_gc_06, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 10 */ defh, defh, defh, process_server_13_A7, defh, defh, defh, process_server_pc_gc_patch_02_17, defh, process_server_19, defh, defh, defh, defh, defh, defh,
  /* 20 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 30 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 40 */ defh, process_server_41<S_GuildCardSearchResult_PC_41>, defh, defh, process_server_44_A6<S_OpenFile_PC_GC_44_A6>, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 50 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 60 */ process_server_60_62_6C_6D_C9_CB, defh, process_server_60_62_6C_6D_C9_CB, defh, process_server_64<S_JoinGame_PC_64>, process_server_65_67_68<S_JoinLobby_PC_65_67_68>, process_server_66_69, process_server_65_67_68<S_JoinLobby_PC_65_67_68>, process_server_65_67_68<S_JoinLobby_PC_65_67_68>, process_server_66_69, defh, defh, process_server_60_62_6C_6D_C9_CB, process_server_60_62_6C_6D_C9_CB, defh, defh,
  /* 70 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 80 */ defh, defh, defh, defh, defh, defh, defh, defh, process_server_88, defh, defh, defh, defh, defh, defh, defh,
  /* 90 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* A0 */ defh, defh, defh, defh, defh, defh, process_server_44_A6<S_OpenFile_PC_GC_44_A6>, process_server_13_A7, defh, defh, defh, defh, defh, defh, defh, defh,
  /* B0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* C0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* D0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* E0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* F0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
};
static process_command_t gc_server_handlers[0x100] = {
  /* 00 */ defh, defh, process_server_pc_gc_patch_02_17, defh, process_server_dc_pc_gc_04, defh, process_server_dc_pc_gc_06, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 10 */ defh, defh, defh, process_server_13_A7, defh, defh, defh, process_server_pc_gc_patch_02_17, defh, process_server_19, process_server_gc_1A_D5, defh, defh, defh, defh, defh,
  /* 20 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 30 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 40 */ defh, process_server_41<S_GuildCardSearchResult_DC_GC_41>, defh, defh, process_server_44_A6<S_OpenFile_PC_GC_44_A6>, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 50 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 60 */ process_server_60_62_6C_6D_C9_CB, defh, process_server_60_62_6C_6D_C9_CB, defh, process_server_64<S_JoinGame_GC_64>, process_server_65_67_68<S_JoinLobby_GC_65_67_68>, process_server_66_69, process_server_65_67_68<S_JoinLobby_GC_65_67_68>, process_server_65_67_68<S_JoinLobby_GC_65_67_68>, process_server_66_69, defh, defh, process_server_60_62_6C_6D_C9_CB, process_server_60_62_6C_6D_C9_CB, defh, defh,
  /* 70 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 80 */ defh, process_server_81<SC_SimpleMail_GC_81>, defh, defh, defh, defh, defh, defh, process_server_88, defh, defh, defh, defh, defh, defh, defh,
  /* 90 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, process_server_gc_9A, defh, defh, defh, defh, defh,
  /* A0 */ defh, defh, defh, defh, defh, defh, process_server_44_A6<S_OpenFile_PC_GC_44_A6>, process_server_13_A7, defh, defh, defh, defh, defh, defh, defh, defh,
  /* B0 */ defh, defh, defh, defh, defh, defh, defh, defh, process_server_gc_B8, defh, defh, defh, defh, defh, defh, defh,
  /* C0 */ defh, defh, defh, defh, process_server_C4<S_ChoiceSearchResultEntry_GC_C4>, defh, defh, defh, defh, process_server_60_62_6C_6D_C9_CB, defh, process_server_60_62_6C_6D_C9_CB, defh, defh, defh, defh,
  /* D0 */ defh, defh, defh, defh, defh, process_server_gc_1A_D5, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* E0 */ defh, defh, defh, defh, process_server_gc_E4, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* F0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
};
static process_command_t bb_server_handlers[0x100] = {
  /* 00 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 10 */ defh, defh, defh, process_server_13_A7, defh, defh, defh, defh, defh, process_server_19, defh, defh, defh, defh, defh, defh,
  /* 20 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 30 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 40 */ defh, process_server_41<S_GuildCardSearchResult_BB_41>, defh, defh, process_server_44_A6<S_OpenFile_BB_44_A6>, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 50 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 60 */ process_server_60_62_6C_6D_C9_CB, defh, process_server_60_62_6C_6D_C9_CB, defh, process_server_64<S_JoinGame_BB_64>, process_server_65_67_68<S_JoinLobby_BB_65_67_68>, process_server_66_69, process_server_65_67_68<S_JoinLobby_BB_65_67_68>, process_server_65_67_68<S_JoinLobby_BB_65_67_68>, process_server_66_69, defh, defh, process_server_60_62_6C_6D_C9_CB, process_server_60_62_6C_6D_C9_CB, defh, defh,
  /* 70 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 80 */ defh, defh, defh, defh, defh, defh, defh, defh, process_server_88, defh, defh, defh, defh, defh, defh, defh,
  /* 90 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* A0 */ defh, defh, defh, defh, defh, defh, process_server_44_A6<S_OpenFile_BB_44_A6>, process_server_13_A7, defh, defh, defh, defh, defh, defh, defh, defh,
  /* B0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* C0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* D0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* E0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* F0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
};
static process_command_t patch_server_handlers[0x100] = {
  /* 00 */ defh, defh, process_server_pc_gc_patch_02_17, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 10 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 20 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 30 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 40 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 50 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 60 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 70 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 80 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 90 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* A0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* B0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* C0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* D0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* E0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* F0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
};



static process_command_t dc_client_handlers[0x100] = {
  /* 00 */ defh, defh, defh, defh, defh, defh, process_client_06, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 10 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 20 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 30 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 40 */ process_client_40, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 50 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 60 */ process_client_60_62_6C_6D_C9_CB<void>, defh, process_client_60_62_6C_6D_C9_CB<void>, defh, defh, defh, defh, defh, defh, defh, defh, defh, process_client_60_62_6C_6D_C9_CB<void>, process_client_60_62_6C_6D_C9_CB<void>, defh, defh,
  /* 70 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 80 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 90 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* A0 */ process_client_dc_pc_gc_A0_A1, process_client_dc_pc_gc_A0_A1, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* B0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* C0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* D0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* E0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* F0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
};
static process_command_t pc_client_handlers[0x100] = {
  /* 00 */ defh, defh, defh, defh, defh, defh, process_client_06, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 10 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 20 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 30 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 40 */ process_client_40, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 50 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 60 */ process_client_60_62_6C_6D_C9_CB<void>, defh, process_client_60_62_6C_6D_C9_CB<void>, defh, defh, defh, defh, defh, defh, defh, defh, defh, process_client_60_62_6C_6D_C9_CB<void>, process_client_60_62_6C_6D_C9_CB<void>, defh, defh,
  /* 70 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 80 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 90 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* A0 */ process_client_dc_pc_gc_A0_A1, process_client_dc_pc_gc_A0_A1, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* B0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* C0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* D0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* E0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* F0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
};
static process_command_t gc_client_handlers[0x100] = {
  /* 00 */ defh, defh, defh, defh, defh, defh, process_client_06, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 10 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 20 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 30 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 40 */ process_client_40, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 50 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 60 */ process_client_60_62_6C_6D_C9_CB<G_SendGuildCard_GC_6x06>, defh, process_client_60_62_6C_6D_C9_CB<G_SendGuildCard_GC_6x06>, defh, defh, defh, defh, defh, defh, defh, defh, defh, process_client_60_62_6C_6D_C9_CB<G_SendGuildCard_GC_6x06>, process_client_60_62_6C_6D_C9_CB<G_SendGuildCard_GC_6x06>, defh, defh,
  /* 70 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 80 */ defh, process_client_81<SC_SimpleMail_GC_81>, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 90 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* A0 */ process_client_dc_pc_gc_A0_A1, process_client_dc_pc_gc_A0_A1, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* B0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* C0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* D0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* E0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* F0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
};
static process_command_t bb_client_handlers[0x100] = {
  /* 00 */ defh, defh, defh, defh, defh, defh, process_client_06, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 10 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 20 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 30 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 40 */ process_client_40, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 50 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 60 */ process_client_60_62_6C_6D_C9_CB<G_SendGuildCard_BB_6x06>, defh, process_client_60_62_6C_6D_C9_CB<G_SendGuildCard_BB_6x06>, defh, defh, defh, defh, defh, defh, defh, defh, defh, process_client_60_62_6C_6D_C9_CB<G_SendGuildCard_BB_6x06>, process_client_60_62_6C_6D_C9_CB<G_SendGuildCard_BB_6x06>, defh, defh,
  /* 70 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 80 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 90 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* A0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* B0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* C0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* D0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* E0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* F0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
};
static process_command_t patch_client_handlers[0x100] = {
  /* 00 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 10 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 20 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 30 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 40 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 50 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 60 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 70 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 80 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* 90 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* A0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* B0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* C0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* D0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* E0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
  /* F0 */ defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh, defh,
};



static process_command_t* server_handlers[] = {
    dc_server_handlers, pc_server_handlers, patch_server_handlers, gc_server_handlers, bb_server_handlers};
static process_command_t* client_handlers[] = {
    dc_client_handlers, pc_client_handlers, patch_client_handlers, gc_client_handlers, bb_client_handlers};

static process_command_t get_handler(GameVersion version, bool from_server, uint8_t command) {
  size_t version_index = static_cast<size_t>(version);
  if (version_index >= 5) {
    throw logic_error("invalid game version on proxy server");
  }
  return (from_server ? server_handlers : client_handlers)[version_index][command];
}

void process_proxy_command(
    shared_ptr<ServerState> s,
    ProxyServer::LinkedSession& session,
    bool from_server,
    uint16_t command,
    uint32_t flag,
    string& data) {
  auto fn = get_handler(session.version, from_server, command);
  try {
    bool should_forward = fn(s, session, command, flag, data);
    if (should_forward) {
      forward_command(session, !from_server, command, flag, data);
    }
  } catch (const exception& e) {
    session.log(ERROR, "Failed to process command: %s", e.what());
    session.disconnect();
  }
}
