#include "Loggers.hh"

#include <phosg/Strings.hh>

using namespace std;

phosg::PrefixedLogger channel_exceptions_log("[Channel] ", phosg::LogLevel::L_USE_DEFAULT);
phosg::PrefixedLogger client_log("", phosg::LogLevel::L_USE_DEFAULT);
phosg::PrefixedLogger command_data_log("[Commands] ", phosg::LogLevel::L_USE_DEFAULT);
phosg::PrefixedLogger config_log("[Config] ", phosg::LogLevel::L_USE_DEFAULT);
phosg::PrefixedLogger dns_server_log("[DNSServer] ", phosg::LogLevel::L_USE_DEFAULT);
phosg::PrefixedLogger function_compiler_log("[FunctionCompiler] ", phosg::LogLevel::L_USE_DEFAULT);
phosg::PrefixedLogger ip_stack_simulator_log("[IPStackSimulator] ", phosg::LogLevel::L_USE_DEFAULT);
phosg::PrefixedLogger lobby_log("", phosg::LogLevel::L_USE_DEFAULT);
phosg::PrefixedLogger patch_index_log("[PatchFileIndex] ", phosg::LogLevel::L_USE_DEFAULT);
phosg::PrefixedLogger player_data_log("", phosg::LogLevel::L_USE_DEFAULT);
phosg::PrefixedLogger proxy_server_log("[ProxyServer] ", phosg::LogLevel::L_USE_DEFAULT);
phosg::PrefixedLogger replay_log("[ReplaySession] ", phosg::LogLevel::L_USE_DEFAULT);
phosg::PrefixedLogger server_log("[Server] ", phosg::LogLevel::L_USE_DEFAULT);
phosg::PrefixedLogger static_game_data_log("[StaticGameData] ", phosg::LogLevel::L_USE_DEFAULT);

static void set_log_level_from_json(
    phosg::PrefixedLogger& log, const phosg::JSON& d, const char* json_key) {
  try {
    string name = phosg::toupper(d.at(json_key).as_string());
    log.min_level = phosg::enum_for_name<phosg::LogLevel>(name);
  } catch (const out_of_range&) {
  }
}

void set_all_log_levels(phosg::LogLevel level) {
  channel_exceptions_log.min_level = level;
  client_log.min_level = level;
  command_data_log.min_level = level;
  config_log.min_level = level;
  dns_server_log.min_level = level;
  function_compiler_log.min_level = level;
  ip_stack_simulator_log.min_level = level;
  lobby_log.min_level = level;
  patch_index_log.min_level = level;
  player_data_log.min_level = level;
  proxy_server_log.min_level = level;
  replay_log.min_level = level;
  server_log.min_level = level;
  static_game_data_log.min_level = level;
}

void set_log_levels_from_json(const phosg::JSON& json) {
  set_log_level_from_json(channel_exceptions_log, json, "ChannelExceptions");
  set_log_level_from_json(client_log, json, "Clients");
  set_log_level_from_json(command_data_log, json, "CommandData");
  set_log_level_from_json(config_log, json, "Config");
  set_log_level_from_json(dns_server_log, json, "DNSServer");
  set_log_level_from_json(function_compiler_log, json, "FunctionCompiler");
  set_log_level_from_json(ip_stack_simulator_log, json, "IPStackSimulator");
  set_log_level_from_json(lobby_log, json, "Lobbies");
  set_log_level_from_json(patch_index_log, json, "PatchFileIndex");
  set_log_level_from_json(player_data_log, json, "PlayerData");
  set_log_level_from_json(proxy_server_log, json, "ProxyServer");
  set_log_level_from_json(replay_log, json, "Replay");
  set_log_level_from_json(server_log, json, "GameServer");
  set_log_level_from_json(static_game_data_log, json, "StaticGameData");
}
