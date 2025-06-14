#include "DownloadSession.hh"

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
#include <phosg/Network.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>
#include <phosg/Time.hh>

#include "Loggers.hh"
#include "PSOProtocol.hh"
#include "ReceiveCommands.hh"
#include "ReceiveSubcommands.hh"
#include "SendCommands.hh"

using namespace std;

static string random_name() {
  string ret;
  size_t length = (phosg::random_object<size_t>() % 12) + 4;
  static const string alphabet = "QWERTYUIOPASDFGHJKLZXCVBNMqwertyuiopasdfghjklzxcvbnm1234567890-+<>:\"\',.";
  while (ret.size() < length) {
    ret.push_back(alphabet[phosg::random_object<size_t>() % alphabet.size()]);
  }
  return ret;
}

DownloadSession::DownloadSession(
    std::shared_ptr<asio::io_context> io_context,
    const std::string& remote_host,
    uint16_t remote_port,
    const std::string& output_dir,
    Version version,
    uint8_t language,
    std::shared_ptr<const PSOBBEncryption::KeyFile> bb_key_file,
    uint32_t serial_number2,
    uint32_t serial_number,
    const std::string& access_key,
    const std::string& username,
    const std::string& password,
    const std::string& xb_gamertag,
    uint64_t xb_user_id,
    uint64_t xb_account_id,
    std::shared_ptr<PSOBBCharacterFile> character,
    const std::unordered_set<std::string>& ship_menu_selections,
    const std::vector<std::string>& on_request_complete_commands,
    bool interactive,
    bool show_command_data)
    : remote_host(remote_host),
      remote_port(remote_port),
      output_dir(output_dir),
      version(version),
      language(language),
      show_command_data(show_command_data),
      bb_key_file(bb_key_file),
      serial_number(serial_number),
      serial_number2(serial_number2),
      access_key(access_key),
      username(username),
      password(password),
      xb_gamertag(xb_gamertag),
      xb_user_id(xb_user_id),
      xb_account_id(xb_account_id),
      character(character),
      ship_menu_selections(ship_menu_selections),
      on_request_complete_commands(on_request_complete_commands),
      interactive(interactive),
      log(std::format("[DownloadSession:{}] ", phosg::name_for_enum(version)), proxy_server_log.min_level),
      io_context(io_context),
      hardware_id(generate_random_hardware_id(version)),
      prev_cmd_data(0),
      client_config(0) {
  if (this->output_dir.empty()) {
    this->output_dir = ".";
  }

  switch (version) {
    case Version::DC_V1:
    case Version::DC_V2:
      if (this->serial_number2 == 0 || this->serial_number == 0 || this->access_key.empty()) {
        throw runtime_error("missing credentials");
      }
      break;
    case Version::PC_V2:
      if (this->serial_number == 0 || this->access_key.empty()) {
        throw runtime_error("missing credentials");
      }
      break;
    case Version::GC_V3:
      if (this->serial_number == 0 || this->access_key.empty() || this->password.empty()) {
        throw runtime_error("missing credentials");
      }
      break;
    case Version::XB_V3:
      if (this->xb_gamertag.empty() || this->xb_user_id == 0 || this->xb_account_id == 0) {
        throw runtime_error("missing credentials");
      }
      break;
    case Version::BB_V4:
      if (this->username.empty() || this->password.empty()) {
        throw runtime_error("missing credentials");
      }
      break;
    default:
      throw runtime_error("unsupported version");
  }

  this->character->inventory.language = language;
}

asio::awaitable<void> DownloadSession::run() {
  string netloc_str = std::format("{}:{}", this->remote_host, this->remote_port);
  this->log.info_f("Connecting to {}", netloc_str);
  auto sock = make_unique<asio::ip::tcp::socket>(co_await async_connect_tcp(this->remote_host, this->remote_port));
  this->channel = SocketChannel::create(
      this->io_context,
      std::move(sock),
      this->version,
      this->language,
      netloc_str,
      this->show_command_data ? phosg::TerminalFormat::FG_GREEN : phosg::TerminalFormat::END,
      this->show_command_data ? phosg::TerminalFormat::FG_YELLOW : phosg::TerminalFormat::END);
  this->log.info_f("Server channel connected");

  while (this->channel->connected()) {
    auto msg = co_await this->channel->recv();
    co_await this->on_message(msg);
  }
}

void DownloadSession::send_93_9D_9E(bool extended) {
  if (is_v1(this->version)) {
    C_LoginExtendedV1_DC_93 ret;
    ret.player_tag = this->guild_card_number ? 0xFFFF0000 : 0x00010000;
    ret.guild_card_number = this->guild_card_number;
    ret.hardware_id = this->hardware_id;
    ret.sub_version = default_sub_version_for_version(this->version);
    ret.is_extended = extended ? 1 : 0;
    ret.language = this->language;
    ret.serial_number.encode(std::format("{:08X}", this->serial_number));
    ret.access_key.encode(this->access_key);
    ret.serial_number2.encode(std::format("{:08X}", this->serial_number2));
    ret.login_character_name.encode(this->character->disp.name.decode());
    this->channel->send(0x93, 0x01, &ret, extended ? sizeof(ret) : sizeof(C_LoginV1_DC_93));

  } else if (is_v2(this->version)) {
    C_LoginExtended_PC_9D ret;
    ret.player_tag = this->guild_card_number ? 0xFFFF0000 : 0x00010000;
    ret.guild_card_number = this->guild_card_number;
    ret.hardware_id = this->hardware_id;
    ret.sub_version = default_sub_version_for_version(this->version);
    ret.is_extended = extended ? 1 : 0;
    ret.language = this->language;
    ret.serial_number.encode(std::format("{:08X}", this->serial_number));
    ret.access_key.encode(this->access_key);
    ret.serial_number2 = ret.serial_number;
    ret.access_key2 = ret.access_key;
    ret.login_character_name.encode(this->character->disp.name.decode());
    size_t data_size = extended
        ? ((this->version == Version::PC_V2) ? sizeof(ret) : sizeof(C_LoginExtended_DC_GC_9D))
        : sizeof(C_Login_DC_PC_GC_9D);
    this->channel->send(0x9D, 0x01, &ret, data_size);

  } else if (this->version == Version::GC_V3) {
    C_LoginExtended_GC_9E ret;
    ret.player_tag = this->guild_card_number ? 0xFFFF0000 : 0x00010000;
    ret.guild_card_number = this->guild_card_number;
    ret.hardware_id = this->hardware_id;
    ret.sub_version = default_sub_version_for_version(this->version);
    ret.is_extended = extended ? 1 : 0;
    ret.language = this->language;
    ret.serial_number.encode(std::format("{:08X}", this->serial_number));
    ret.access_key.encode(this->access_key);
    ret.serial_number2 = ret.serial_number;
    ret.access_key2 = ret.access_key;
    ret.login_character_name.encode(this->character->disp.name.decode());
    ret.client_config = this->client_config;
    this->channel->send(0x9E, 0x01, &ret, extended ? sizeof(ret) : sizeof(C_Login_PC_GC_9E));

  } else if (this->version == Version::XB_V3) {
    C_LoginExtended_XB_9E ret;
    ret.player_tag = this->guild_card_number ? 0xFFFF0000 : 0x00010000;
    ret.guild_card_number = this->guild_card_number;
    ret.hardware_id = this->hardware_id;
    ret.sub_version = default_sub_version_for_version(this->version);
    ret.is_extended = extended ? 1 : 0;
    ret.language = this->language;
    ret.serial_number.encode(this->xb_gamertag);
    ret.access_key.encode(std::format("{:016X}", this->xb_user_id));
    ret.serial_number2 = ret.serial_number;
    ret.access_key2 = ret.access_key;
    ret.login_character_name.encode(this->character->disp.name.decode());
    ret.xb_netloc.internal_ipv4_address = phosg::random_object<uint32_t>();
    ret.xb_netloc.external_ipv4_address = phosg::random_object<uint32_t>();
    ret.xb_netloc.port = 9500;
    phosg::random_data(&ret.xb_netloc.mac_address, sizeof(ret.xb_netloc.mac_address));
    ret.xb_netloc.sg_ip_address = phosg::random_object<uint32_t>();
    ret.xb_netloc.spi = phosg::random_object<uint32_t>();
    ret.xb_netloc.account_id = this->xb_account_id;
    ret.xb_netloc.unknown_a3.clear(0);
    ret.xb_user_id_high = this->xb_user_id >> 32;
    ret.xb_user_id_low = this->xb_user_id;
    this->channel->send(0x9E, 0x01, &ret, extended ? sizeof(ret) : sizeof(C_Login_DC_PC_GC_9D));

  } else {
    throw runtime_error("unsupported version");
  }
}

void DownloadSession::send_61_98(bool is_98) {
  uint8_t command = is_98 ? 0x98 : 0x61;

  if (is_v1(this->version)) {
    C_CharacterData_DCv1_61_98 ret;
    ret.inventory = this->character->inventory;
    ret.disp = convert_player_disp_data<PlayerDispDataDCPCV3, PlayerDispDataBB>(this->character->disp, 1, 1);
    this->channel->send(command, 0x01, ret);

  } else if (this->version == Version::DC_V2) {
    C_CharacterData_DCv2_61_98 ret;
    ret.inventory = this->character->inventory;
    ret.disp = convert_player_disp_data<PlayerDispDataDCPCV3, PlayerDispDataBB>(this->character->disp, 1, 1);
    ret.records.challenge = this->character->challenge_records;
    ret.records.battle = this->character->battle_records;
    ret.choice_search_config = this->character->choice_search_config;
    this->channel->send(command, 0x02, ret);

  } else if (this->version == Version::PC_V2) {
    C_CharacterData_PC_61_98 ret;
    ret.inventory = this->character->inventory;
    ret.disp = convert_player_disp_data<PlayerDispDataDCPCV3, PlayerDispDataBB>(this->character->disp, 1, 1);
    ret.records.challenge = this->character->challenge_records;
    ret.records.battle = this->character->battle_records;
    ret.choice_search_config = this->character->choice_search_config;
    this->channel->send(command, 0x02, ret);

  } else if (is_v3(this->version)) {
    C_CharacterData_V3_61_98 ret;
    ret.inventory = this->character->inventory;
    ret.disp = convert_player_disp_data<PlayerDispDataDCPCV3, PlayerDispDataBB>(this->character->disp, 1, 1);
    ret.records.challenge = this->character->challenge_records;
    ret.records.battle = this->character->battle_records;
    ret.choice_search_config = this->character->choice_search_config;
    ret.info_board.encode(this->character->info_board.decode());
    this->channel->send(command, 0x03, ret);

  } else if (this->version == Version::BB_V4) {
    C_CharacterData_BB_61_98 ret;
    ret.inventory = this->character->inventory;
    ret.disp = this->character->disp;
    ret.records.challenge = this->character->challenge_records;
    ret.records.battle = this->character->battle_records;
    ret.choice_search_config = this->character->choice_search_config;
    ret.info_board.encode(this->character->info_board.decode());
    this->channel->send(command, 0x04, ret);

  } else {
    throw runtime_error("unsupported version");
  }
}

asio::awaitable<void> DownloadSession::on_message(Channel::Message& msg) {
  for (size_t z = 0; z < 0x28 && z < msg.data.size(); z++) {
    this->prev_cmd_data[z] = msg.data[z];
  }

  switch (msg.command) {
    case 0x03: {
      if (this->version != Version::BB_V4) {
        throw runtime_error("BB server sent non-BB encryption command");
      }
      if (!this->bb_key_file) {
        throw runtime_error("BB encryption requires a key file");
      }
      const auto& cmd = msg.check_size_t<S_ServerInitDefault_BB_03_9B>(0xFFFF);
      this->channel->crypt_in = make_shared<PSOBBEncryption>(*this->bb_key_file, &cmd.server_key[0], sizeof(cmd.server_key));
      this->channel->crypt_out = make_shared<PSOBBEncryption>(*this->bb_key_file, &cmd.client_key[0], sizeof(cmd.client_key));
      this->log.info_f("Enabled BB encryption");
      throw runtime_error("not yet implemented"); // Send 93
      break;
    }

    case 0x02:
    case 0x17:
    case 0x91:
    case 0x9B: {
      const auto& cmd = msg.check_size_t<S_ServerInitDefault_DC_PC_V3_02_17_91_9B>(0xFFFF);
      if (uses_v3_encryption(this->version)) {
        this->channel->crypt_in = make_shared<PSOV3Encryption>(cmd.server_key);
        this->channel->crypt_out = make_shared<PSOV3Encryption>(cmd.client_key);
        this->log.info_f("Enabled V3 encryption (server key {:08X}, client key {:08X})",
            cmd.server_key, cmd.client_key);
      } else if (!uses_v4_encryption(this->version)) {
        this->channel->crypt_in = make_shared<PSOV2Encryption>(cmd.server_key);
        this->channel->crypt_out = make_shared<PSOV2Encryption>(cmd.client_key);
        this->log.info_f("Enabled V2 encryption (server key {:08X}, client key {:08X})",
            cmd.server_key, cmd.client_key);
      } else {
        throw runtime_error("BB server sent non-BB encryption command");
      }

      if (msg.command == 0x02) {
        bool is_extended = (this->version == Version::XB_V3);
        this->send_93_9D_9E(is_extended);

      } else {
        if (is_v1(this->version)) {
          C_LoginV1_DC_PC_V3_90 ret;
          ret.serial_number.encode(std::format("{:08X}", this->serial_number));
          ret.access_key.encode(this->access_key);
          this->channel->send(0x90, 0x00, ret);

        } else if (is_v2(this->version)) {
          C_Login_DC_PC_V3_9A ret;
          ret.serial_number.encode(std::format("{:08X}", this->serial_number));
          ret.access_key.encode(this->access_key);
          ret.player_tag = this->guild_card_number ? 0xFFFF0000 : 0x00010000;
          ret.guild_card_number = this->guild_card_number;
          ret.sub_version = default_sub_version_for_version(this->version);
          ret.serial_number2 = ret.serial_number;
          ret.access_key2 = ret.access_key;
          this->channel->send(0x9A, 0x00, ret);

        } else if (this->version == Version::GC_V3) {
          C_VerifyAccount_V3_DB ret;
          ret.serial_number.encode(std::format("{:08X}", this->serial_number));
          ret.access_key.encode(this->access_key);
          ret.sub_version = default_sub_version_for_version(this->version);
          ret.serial_number2 = ret.serial_number;
          ret.access_key2 = ret.access_key;
          ret.password.encode(this->password);
          this->channel->send(0xDB, 0x00, ret);

        } else if (this->version == Version::XB_V3) {
          this->send_93_9D_9E(true);

        } else {
          throw runtime_error("unsupported version");
        }
      }

      break;
    }

    case 0x90:
    case 0x9A: {
      if (msg.flag == 1) {
        if (is_v1(this->version)) {
          C_RegisterV1_DC_92 ret;
          ret.hardware_id = this->hardware_id;
          ret.sub_version = default_sub_version_for_version(this->version);
          ret.language = this->language;
          ret.serial_number2.encode(std::format("{:08X}", this->serial_number2));
          this->channel->send(0x92, 0x00, ret);

        } else if (!is_v4(this->version)) {
          C_Register_DC_PC_V3_9C ret;
          ret.hardware_id = this->hardware_id;
          ret.sub_version = default_sub_version_for_version(this->version);
          ret.language = this->language;
          if (this->version == Version::XB_V3) {
            ret.serial_number.encode(this->xb_gamertag);
            ret.access_key.encode(std::format("{:016X}", this->xb_user_id));
            ret.password.encode("xbox-pso");
          } else {
            ret.serial_number.encode(std::format("{:08X}", this->serial_number));
            ret.access_key.encode(this->access_key);
            ret.password.encode(this->password);
          }
          this->channel->send(0x9C, 0x00, ret);

        } else {
          throw runtime_error("unsupported version");
        }

      } else if (msg.flag == 0 || msg.flag == 2) {
        this->send_93_9D_9E(true);

      } else {
        throw runtime_error("login failed");
      }
      break;
    }

    case 0x92:
    case 0x9C:
      if (msg.flag == 0) {
        throw runtime_error("server rejected login credentials");
      }
      this->send_93_9D_9E(true);
      break;

    case 0x9F: {
      if (is_v1_or_v2(this->version)) {
        throw runtime_error("invalid command");
      }
      this->channel->send(0x9F, 0x00, this->client_config);
      break;
    }

    case 0xB2: {
      C_ExecuteCodeResult_B3 ret;
      ret.checksum = 0;
      ret.return_value = 0;
      this->channel->send(0xB3, 0x00, ret);
      break;
    }

    case 0x04: {
      const auto& cmd = msg.check_size_t<S_UpdateClientConfig_V3_04>(0x08, sizeof(S_UpdateClientConfig_V3_04));
      if (!is_v1_or_v2(this->version)) {
        for (size_t z = 0; z < 0x20; z++) {
          size_t read_index = z + 8;
          this->client_config[z] = (read_index < msg.data.size()) ? msg.data[read_index] : this->prev_cmd_data[read_index];
        }
      }
      this->guild_card_number = cmd.guild_card_number;
      if (!this->sent_96) {
        C_CharSaveInfo_DCv2_PC_V3_BB_96 ret;
        ret.creation_timestamp = this->character->creation_timestamp;
        ret.event_counter = this->character->save_count;
        this->channel->send(0x96, 0x00, ret);
        this->sent_96 = true;
      }
      break;
    }

    case 0x97:
      this->channel->send(0xB1, 0x00);
      break;

    case 0x95:
      this->send_61_98(false);
      break;

    case 0xB1:
      this->channel->send(0x99, 0x00);
      break;

    case 0x1A:
    case 0xD5:
      if (is_v3(this->version)) {
        this->channel->send(0xD6, 0x00);
      }
      break;

    case 0x07:
    case 0x1F:
    case 0xA0:
    case 0xA1: {
      C_MenuSelection_10_Flag00 ret;

      auto handle_command = [&]<typename CmdT>() {
        const auto* items = check_size_vec_t<CmdT>(msg.data, msg.flag + 1);
        size_t item_index;
        this->log.info_f("Ship Select menu:");
        for (item_index = 1; item_index <= msg.flag; item_index++) {
          const auto& item = items[item_index];
          auto text = strip_color(item.name.decode());
          this->log.info_f("{}: ({:08X} {:08X}) {}", item_index, item.menu_id, item.item_id, text);
          if (this->ship_menu_selections.count(text)) {
            break;
          }
        }
        if (item_index > msg.flag) {
          if (this->interactive) {
            while (item_index == 0 || item_index > msg.flag) {
              this->log.info_f("Choose response index:");
              string input = phosg::fgets(stdin);
              item_index = stoul(input, nullptr, 0);
            }
          } else {
            throw runtime_error("unhandled menu selection");
          }
        }
        ret.menu_id = items[item_index].menu_id;
        ret.item_id = items[item_index].item_id;
      };

      if (uses_utf16(this->version)) {
        handle_command.operator()<S_MenuItem_PC_BB_08>();
      } else {
        handle_command.operator()<S_MenuItem_DC_V3_08_Ep3_E6>();
      }

      this->channel->send(0x10, 0x00, ret);
      break;
    }

    case 0x01:
    case 0x11:
    case 0x60:
    case 0x62:
    case 0x68:
    case 0x69:
    case 0x6C:
    case 0x6D:
    case 0x88:
    case 0x8A:
    case 0xB0:
    case 0xC5:
    case 0xDA:
      break;

    case 0x1D:
      this->channel->send(0x1D, 0x00);
      break;

    case 0x19: {
      const auto& cmd = msg.check_size_t<S_Reconnect_19>(sizeof(S_Reconnect_19), 0xFFFF);

      auto new_ep = make_endpoint_ipv4(cmd.address, cmd.port);
      string netloc_str = str_for_endpoint(new_ep);
      this->log.info_f("Connecting to {}", netloc_str);
      auto sock = make_unique<asio::ip::tcp::socket>(co_await async_connect_tcp(new_ep));
      this->channel = SocketChannel::create(
          this->io_context,
          std::move(sock),
          this->version,
          this->language,
          netloc_str,
          this->show_command_data ? phosg::TerminalFormat::FG_GREEN : phosg::TerminalFormat::END,
          this->show_command_data ? phosg::TerminalFormat::FG_YELLOW : phosg::TerminalFormat::END);
      this->log.info_f("Server channel connected");
      break;
    }

    case 0x83: {
      const auto* items = check_size_vec_t<S_LobbyListEntry_83>(msg.data, msg.flag, true);
      this->lobby_menu_items.clear();
      for (size_t z = 0; z < msg.flag; z++) {
        this->lobby_menu_items.emplace_back(items[z]);
      }
      break;
    }

    case 0x67: {
      // Technically we should assign item IDs here, but the server will never
      // be able to see that we didn't, so we don't bother

      const auto& game_config = this->game_configs[this->current_game_config_index];
      if (this->version == Version::PC_V2) {
        C_CreateGame_PC_C1 ret;
        ret.name.encode(random_name());
        ret.password.encode(random_name());
        ret.difficulty = 0;
        ret.battle_mode = (game_config.mode == GameMode::BATTLE);
        ret.challenge_mode = (game_config.mode == GameMode::CHALLENGE);
        ret.episode = 1;
        this->channel->send(0xC1, 0x00, ret);

      } else if (!is_v4(this->version)) {
        C_CreateGame_DC_V3_0C_C1_Ep3_EC ret;
        ret.name.encode(random_name());
        ret.password.encode(random_name());
        ret.difficulty = 0;
        ret.battle_mode = (game_config.mode == GameMode::BATTLE);
        ret.challenge_mode = (game_config.mode == GameMode::CHALLENGE);
        if (is_v1(this->version)) {
          ret.episode = 0;
        } else if (game_config.episode == Episode::EP1) {
          ret.episode = 1;
        } else if (game_config.episode == Episode::EP2) {
          ret.episode = 2;
        } else if (game_config.episode == Episode::EP4) {
          ret.episode = 4;
        } else {
          throw std::logic_error("invalid episode");
        }
        this->channel->send(is_v1(this->version) ? 0x0C : 0xC1, 0x00, ret);

      } else {
        C_CreateGame_BB_C1 ret;
        ret.name.encode(random_name());
        ret.password.encode(random_name());
        ret.difficulty = 0;
        ret.battle_mode = (game_config.mode == GameMode::BATTLE);
        ret.challenge_mode = (game_config.mode == GameMode::CHALLENGE);
        if (game_config.episode == Episode::EP1) {
          ret.episode = 1;
        } else if (game_config.episode == Episode::EP2) {
          ret.episode = 2;
        } else if (game_config.episode == Episode::EP4) {
          ret.episode = 4;
        } else {
          throw std::logic_error("invalid episode");
        }
        ret.solo_mode = (game_config.mode == GameMode::SOLO);
        this->channel->send(is_v1(this->version) ? 0x0C : 0xC1, 0x00, ret);
      }
      break;
    }

    case 0x64: {
      this->in_game = true;
      this->bin_complete = false;
      this->dat_complete = false;
      for (size_t z = 0; z < this->character->inventory.num_items; z++) {
        this->character->inventory.items[z].data.id = 0x00010000 + z;
      }

      if (!is_v1(this->version)) {
        this->channel->send(0x8A, 0x00);
      }
      this->channel->send(0x6F, 0x00);
      this->send_next_request();
      break;
    }

    case 0xA2: {
      auto handle_command = [&]<typename CmdT>() {
        const auto* items = check_size_vec_t<CmdT>(msg.data, msg.flag);
        for (size_t z = 0; z < msg.flag; z++) {
          const auto& item = items[z];
          uint64_t request = (static_cast<uint64_t>(item.menu_id) << 32) | static_cast<uint64_t>(item.item_id);
          if (!this->done_requests.count(request)) {
            this->log.info_f("Adding request {:016X}", request);
            this->pending_requests.emplace(request, item.name.decode());
          }
        }
      };

      if (this->version == Version::PC_V2) {
        handle_command.operator()<S_QuestMenuEntry_PC_A2_A4>();
      } else if (this->version == Version::XB_V3) {
        handle_command.operator()<S_QuestMenuEntry_XB_A2_A4>();
      } else if (this->version == Version::BB_V4) {
        handle_command.operator()<S_QuestMenuEntry_BB_A2_A4>();
      } else {
        handle_command.operator()<S_QuestMenuEntry_DC_GC_A2_A4>();
      }
      this->send_next_request();
      break;
    }
    case 0x44:
    case 0xA6: {
      auto handle_command = [&]<typename CmdT>() {
        const auto& cmd = msg.check_size_t<CmdT>(0xFFFF);
        string internal_name = cmd.filename.decode();
        string filtered_name;
        for (char ch : internal_name) {
          filtered_name.push_back((isalnum(ch) || (ch == '-') || (ch == '.') || (ch == '_')) ? ch : '_');
        }
        string local_filename = std::format(
            "{}/{:016X}_{}_{}_{}_{}",
            this->output_dir,
            this->current_request,
            phosg::now(),
            phosg::name_for_enum(this->version),
            char_for_language_code(this->language),
            filtered_name);
        this->open_files.emplace(internal_name, OpenFile{.request = this->current_request, .filename = local_filename, .total_size = cmd.file_size, .data = ""});
      };

      if (is_dc(this->version)) {
        handle_command.operator()<S_OpenFile_DC_44_A6>();
      } else if (!is_v4(this->version)) {
        handle_command.operator()<S_OpenFile_PC_GC_44_A6>();
      } else {
        handle_command.operator()<S_OpenFile_BB_44_A6>();
      }
      break;
    }
    case 0x13:
    case 0xA7: {
      const auto& cmd = msg.check_size_t<S_WriteFile_13_A7>();
      string internal_filename = cmd.filename.decode();

      if (!is_v1_or_v2(this->version)) {
        C_WriteFileConfirmation_V3_BB_13_A7 ret;
        ret.filename.encode(internal_filename);
        this->channel->send(msg.command, msg.flag, ret);
      }

      auto f_it = this->open_files.find(internal_filename);
      if (f_it == this->open_files.end()) {
        this->log.warning_f("Received data for non-open file {}", internal_filename);
        break;
      }
      auto& f = this->open_files.at(cmd.filename.decode());
      size_t block_offset = msg.flag * 0x400;
      size_t allowed_block_size = (block_offset < f.total_size)
          ? min<size_t>(f.total_size - block_offset, 0x400)
          : 0;
      size_t data_size = min<size_t>(cmd.data_size, allowed_block_size);
      size_t block_end_offset = block_offset + data_size;
      if (block_end_offset > f.data.size()) {
        f.data.resize(block_end_offset);
      }
      memcpy(f.data.data() + block_offset, cmd.data.data(), data_size);
      if (cmd.data_size != 0x400) {
        phosg::save_file(f.filename, f.data);
        this->log.info_f("Wrote file {} ({} bytes)", f.filename, f.data.size());
        this->open_files.erase(internal_filename);
        if (internal_filename.ends_with(".bin")) {
          this->bin_complete = true;
        } else if (internal_filename.ends_with(".dat")) {
          this->dat_complete = true;
        }
        if (this->open_files.empty() && this->bin_complete && this->dat_complete) {
          if (is_v1_or_v2(this->version)) {
            this->on_request_complete();
          } else {
            this->channel->send(0xAC, 0x00);
          }
        }
      }
      break;
    }
    case 0xAC: {
      if (is_v1_or_v2(this->version)) {
        throw runtime_error("unsupported version");
      }
      this->on_request_complete();
      break;
    }
  }
}

void DownloadSession::send_next_request() {
  if (should_request_category_list) {
    this->log.info_f("Requesting quest list");
    this->channel->send(0xA2, 0x00);
    if (is_v4(this->version)) {
      this->channel->send(0xA2, 0x01);
    }
    this->should_request_category_list = false;

  } else if (!this->pending_requests.empty()) {
    if (interactive) {
      const auto& config = this->game_configs[this->current_game_config_index];
      this->log.info_f("Items available to expand (mode={}, episode={}):", name_for_mode(config.mode), name_for_episode(config.episode));
      for (const auto& it : this->pending_requests) {
        this->log.info_f("{:016X}: {}", it.first, it.second);
      }
      this->log.info_f("Choose item to expand by ID (q to quit; s to skip to next config):");
      string input = phosg::fgets(stdin);
      if (input.empty() || (input == "q\n")) {
        this->channel->disconnect();
        return;
      } else if (input == "s\n") {
        this->pending_requests.clear();
        this->on_request_complete();
      } else {
        this->current_request = stoull(input, nullptr, 16);
        this->done_requests.emplace(this->current_request);
        this->pending_requests.erase(this->current_request);
      }

    } else {
      auto item_it = this->pending_requests.begin();
      this->current_request = item_it->first;
      this->done_requests.emplace(this->current_request);
      this->pending_requests.erase(item_it);
      this->log.info_f("Sending request {:016X}", this->current_request);
    }

    C_MenuSelection_10_Flag00 cmd;
    cmd.menu_id = (this->current_request >> 32) & 0xFFFFFFFF;
    cmd.item_id = this->current_request & 0xFFFFFFFF;
    this->channel->send(0x10, 0x00, cmd);
  } else {
    this->log.info_f("No pending requests with current parameters");
    this->on_request_complete();
  }
}

void DownloadSession::on_request_complete() {
  for (const auto& data : this->on_request_complete_commands) {
    this->channel->send(data);
  }

  this->send_61_98(true);
  this->in_game = false;

  const auto& item = this->lobby_menu_items.at(this->lobby_menu_items.size() / 2);
  C_LobbySelection_84 ret84;
  ret84.menu_id = item.menu_id;
  ret84.item_id = item.item_id;
  this->channel->send(0x84, 0x00, ret84);

  if (this->pending_requests.empty()) {
    // Advance to next mode/episode combination
    this->current_game_config_index++;
    bool v1 = is_v1(this->version);
    bool v2 = is_v2(this->version);
    bool v3 = is_v3(this->version);
    while ((this->current_game_config_index < this->game_configs.size()) &&
        ((v1 && !this->game_configs[this->current_game_config_index].v1) ||
            (v2 && !this->game_configs[this->current_game_config_index].v2) ||
            (v3 && !this->game_configs[this->current_game_config_index].v3))) {
      this->current_game_config_index++;
    }
    if (this->current_game_config_index >= this->game_configs.size()) {
      this->log.info_f("All modes complete");
      this->channel->disconnect();
    } else {
      const auto& config = this->game_configs[this->current_game_config_index];
      this->log.info_f("Advancing to {} mode in {}", name_for_mode(config.mode), name_for_episode(config.episode));
      this->should_request_category_list = true;
    }
  }
}

const std::vector<DownloadSession::GameConfig> DownloadSession::game_configs({
    {.mode = GameMode::NORMAL, .episode = Episode::EP1, .v1 = true, .v2 = true, .v3 = true},
    {.mode = GameMode::NORMAL, .episode = Episode::EP2, .v1 = false, .v2 = false, .v3 = true},
    {.mode = GameMode::NORMAL, .episode = Episode::EP4, .v1 = false, .v2 = false, .v3 = false},
    {.mode = GameMode::BATTLE, .episode = Episode::EP1, .v1 = false, .v2 = true, .v3 = true},
    {.mode = GameMode::CHALLENGE, .episode = Episode::EP1, .v1 = false, .v2 = true, .v3 = true},
    {.mode = GameMode::CHALLENGE, .episode = Episode::EP2, .v1 = false, .v2 = false, .v3 = true},
    {.mode = GameMode::SOLO, .episode = Episode::EP1, .v1 = false, .v2 = false, .v3 = false},
    {.mode = GameMode::SOLO, .episode = Episode::EP2, .v1 = false, .v2 = false, .v3 = false},
    {.mode = GameMode::SOLO, .episode = Episode::EP4, .v1 = false, .v2 = false, .v3 = false},
});
