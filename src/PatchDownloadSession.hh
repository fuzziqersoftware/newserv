#pragma once

#include <asio.hpp>
#include <map>
#include <memory>
#include <phosg/Filesystem.hh>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Channel.hh"
#include "CommandFormats.hh"
#include "PSOEncryption.hh"
#include "PSOProtocol.hh"

class PatchDownloadSession {
public:
  PatchDownloadSession(
      std::shared_ptr<asio::io_context> io_context,
      const std::string& remote_host,
      uint16_t remote_port,
      const std::string& output_dir,
      Version version,
      const std::string& username,
      const std::string& password,
      const std::string& email,
      bool show_command_data);
  PatchDownloadSession(const PatchDownloadSession&) = delete;
  PatchDownloadSession(PatchDownloadSession&&) = delete;
  PatchDownloadSession& operator=(const PatchDownloadSession&) = delete;
  PatchDownloadSession& operator=(PatchDownloadSession&&) = delete;
  virtual ~PatchDownloadSession() = default;

  asio::awaitable<void> run();

protected:
  // Config (must be set by caller)
  std::string remote_host;
  uint16_t remote_port;
  std::string output_dir;
  Version version;
  std::string username;
  std::string password;
  std::string email;
  bool show_command_data;

  // State (set during session)
  phosg::PrefixedLogger log;
  std::shared_ptr<asio::io_context> io_context;
  std::shared_ptr<Channel> channel;
  std::vector<std::string> dir_path;
  std::unique_ptr<FILE, void (*)(FILE*)> current_file;
  size_t current_file_bytes_remaining = 0;
  std::vector<C_FileInformation_Patch_0F> pending_checksum_results;

  static void check_path_token(const std::string& token);
  std::string resolve_filename(const std::string& filename) const;

  asio::awaitable<void> on_message(Channel::Message& msg);
};
