#include "PatchDownloadSession.hh"

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

PatchDownloadSession::PatchDownloadSession(
    std::shared_ptr<asio::io_context> io_context,
    const std::string& remote_host,
    uint16_t remote_port,
    const std::string& output_dir,
    Version version,
    const std::string& username,
    const std::string& password,
    const std::string& email,
    bool show_command_data)
    : remote_host(remote_host),
      remote_port(remote_port),
      output_dir(output_dir),
      version(version),
      username(username),
      password(password),
      email(email),
      show_command_data(show_command_data),
      log(std::format("[PatchDownloadSession:{}] ", phosg::name_for_enum(version)), proxy_server_log.min_level),
      io_context(io_context),
      current_file(nullptr, +[](FILE* f) -> void { fclose(f); }) {
  if (this->output_dir.empty()) {
    this->output_dir = ".";
  }
  if (!is_patch(this->version)) {
    throw std::logic_error("invalid version in PatchDownloadSession");
  }
}

asio::awaitable<void> PatchDownloadSession::run() {
  string netloc_str = std::format("{}:{}", this->remote_host, this->remote_port);
  this->log.info_f("Connecting to {}", netloc_str);
  auto sock = make_unique<asio::ip::tcp::socket>(co_await async_connect_tcp(this->remote_host, this->remote_port));
  this->channel = SocketChannel::create(
      this->io_context,
      std::move(sock),
      this->version,
      Language::ENGLISH,
      netloc_str,
      this->show_command_data ? phosg::TerminalFormat::FG_GREEN : phosg::TerminalFormat::END,
      this->show_command_data ? phosg::TerminalFormat::FG_YELLOW : phosg::TerminalFormat::END);
  this->log.info_f("Server channel connected");

  while (this->channel->connected()) {
    auto msg = co_await this->channel->recv();
    co_await this->on_message(msg);
  }
}

void PatchDownloadSession::check_path_token(const std::string& token) {
  if (token == "..") {
    throw std::runtime_error("parent directory token is not allowed");
  }
  if ((token.find('/') != string::npos) || (token.find('\\') != string::npos)) {
    throw std::runtime_error("directory token contains path separator");
  }
}

std::string PatchDownloadSession::resolve_filename(const std::string& filename) const {
  check_path_token(filename);
  string path = this->output_dir;
  for (const auto& dir_name : this->dir_path) {
    path.push_back('/');
    path += dir_name;
  }
  if (!filename.empty()) {
    path.push_back('/');
    path += filename;
  }
  return path;
}

asio::awaitable<void> PatchDownloadSession::on_message(Channel::Message& msg) {
  switch (msg.command) {
    case 0x02: {
      const auto& cmd = msg.check_size_t<S_ServerInit_Patch_02>();
      if (cmd.copyright.decode() != "Patch Server. Copyright SonicTeam, LTD. 2001") {
        throw std::runtime_error("incorrect copyright message");
      }
      this->channel->crypt_in = make_shared<PSOV2Encryption>(cmd.server_key);
      this->channel->crypt_out = make_shared<PSOV2Encryption>(cmd.client_key);
      this->channel->send(0x02);
      this->log.info_f("Enabled encryption");
      break;
    }

    case 0x04: {
      if (!msg.data.empty()) {
        throw std::runtime_error("invalid login request command");
      }
      C_Login_Patch_04 cmd;
      cmd.username.encode(this->username);
      cmd.password.encode(this->password);
      cmd.email_address.encode(this->email);
      this->channel->send(0x04, 0x00, &cmd, sizeof(cmd));
      this->log.info_f("Sent login credentials");
      break;
    }

    case 0x05: {
      this->log.info_f("Server sent disconnect command");
      this->channel->disconnect();
      break;
    }

    case 0x06: {
      if (this->current_file) {
        throw std::runtime_error("protocol violation: previous file was not closed before open file command");
      }
      const auto& cmd = msg.check_size_t<S_OpenFile_Patch_06>();
      this->current_file_bytes_remaining = cmd.size;
      auto filename = this->resolve_filename(cmd.filename.decode());
      this->current_file = phosg::fopen_unique(filename, "wb");
      this->log.info_f("Opened file {}", filename);
      break;
    }

    case 0x07: {
      if (!this->current_file) {
        throw std::runtime_error("protocol violation: no file is open; cannot write data");
      }

      const auto& cmd = msg.check_size_t<S_WriteFileHeader_Patch_07>(0xFFFF);
      const void* data = msg.data.data() + sizeof(cmd);
      if (cmd.chunk_size > msg.data.size() - sizeof(cmd)) {
        throw std::runtime_error("protocol violation: write command size is invalid");
      }
      if (cmd.chunk_size > this->current_file_bytes_remaining) {
        throw std::runtime_error("protocol violation: chunk would exceed file size specified in open command");
      }
      if (phosg::crc32(data, cmd.chunk_size) != cmd.chunk_checksum) {
        throw std::runtime_error("protocol violation: write command checksum is invalid");
      }

      phosg::fwritex(this->current_file.get(), data, cmd.chunk_size);
      this->current_file_bytes_remaining -= cmd.chunk_size;
      this->log.info_f("Wrote {} to file", phosg::format_size(cmd.chunk_size));
      break;
    }

    case 0x08: {
      if (!this->current_file) {
        throw std::runtime_error("protocol violation: no file is open; cannot close it");
      }
      this->current_file.reset();
      this->log.info_f("Closed file");
      break;
    }

    case 0x09: {
      if (this->current_file) {
        throw std::runtime_error("protocol violation: cannot enter directory with a file open");
      }

      const auto& cmd = msg.check_size_t<S_EnterDirectory_Patch_09>();
      string dirname = cmd.name.decode();
      check_path_token(dirname);
      this->dir_path.emplace_back(std::move(dirname));
      std::filesystem::create_directories(this->resolve_filename(""));
      this->log.info_f("Entered directory {}", dirname);
      break;
    }

    case 0x0A: {
      if (this->current_file) {
        throw std::runtime_error("protocol violation: cannot exit directory with a file open");
      }
      if (this->dir_path.empty()) {
        throw std::runtime_error("protocol violation: cannot exit directory with empty directory stack");
      }
      this->dir_path.pop_back();
      this->log.info_f("Left directory");
      break;
    }

    case 0x0B:
      if (this->current_file) {
        throw std::runtime_error("protocol violation: cannot start patch session when file is already open");
      }
      this->dir_path.clear();
      this->log.info_f("Started patch session");
      break;

    case 0x0C: {
      const auto& cmd = msg.check_size_t<S_FileChecksumRequest_Patch_0C>();
      auto filename = this->resolve_filename(cmd.filename.decode());
      uint32_t checksum = 0, size = 0;
      try {
        auto data = phosg::load_file(filename);
        checksum = phosg::crc32(data.data(), data.size());
        size = data.size();
      } catch (const phosg::cannot_open_file&) {
      }
      this->pending_checksum_results.emplace_back(C_FileInformation_Patch_0F{cmd.request_id, checksum, size});
      this->log.info_f("Checked file {}", filename);
      break;
    }

    case 0x0D:
      for (const auto& it : this->pending_checksum_results) {
        this->channel->send(0x0F, 0x00, &it, sizeof(it));
      }
      this->pending_checksum_results.clear();
      this->channel->send(0x10);
      this->log.info_f("Sent all checksum results");
      break;

    case 0x11: {
      const auto& cmd = msg.check_size_t<S_StartFileDownloads_Patch_11>();
      this->log.info_f("{} files ({}) to download", cmd.num_files.load(), phosg::format_size(cmd.total_bytes));
      break;
    }

    case 0x12:
      this->log.info_f("Patch session succeeded");
      this->channel->disconnect();
      break;

    case 0x13: {
      phosg::strip_trailing_zeroes(msg.data);
      if (msg.data.size() & 1) {
        msg.data.push_back(0);
      }
      this->log.info_f("Message from server:\n{}", strip_color(tt_utf16_to_utf8(msg.data)));
      break;
    }

    case 0x14: {
      const auto& cmd = msg.check_size_t<S_Reconnect_Patch_14>();

      auto new_ep = make_endpoint_ipv4(cmd.address, cmd.port);
      string netloc_str = str_for_endpoint(new_ep);
      this->log.info_f("Connecting to {}", netloc_str);
      auto sock = make_unique<asio::ip::tcp::socket>(co_await async_connect_tcp(new_ep));

      auto old_channel = this->channel;
      auto new_channel = SocketChannel::create(
          this->io_context,
          std::move(sock),
          this->channel->version,
          this->channel->language,
          netloc_str,
          this->channel->terminal_send_color,
          this->channel->terminal_recv_color);
      this->channel = new_channel;
      old_channel->disconnect();
      this->log.info_f("Server channel connected");
      break;
    }

    case 0x15:
      this->log.error_f("Server rejected login credentials");
      this->channel->disconnect();
      break;

    default:
      throw std::runtime_error("invalid command");
  }
}
