#pragma once

#include <phosg/JSON.hh>
#include <phosg/Strings.hh>

extern PrefixedLogger ax_messages_log;
extern PrefixedLogger channel_exceptions_log;
extern PrefixedLogger client_log;
extern PrefixedLogger command_data_log;
extern PrefixedLogger config_log;
extern PrefixedLogger dns_server_log;
extern PrefixedLogger function_compiler_log;
extern PrefixedLogger ip_stack_simulator_log;
extern PrefixedLogger license_log;
extern PrefixedLogger lobby_log;
extern PrefixedLogger patch_index_log;
extern PrefixedLogger player_data_log;
extern PrefixedLogger proxy_server_log;
extern PrefixedLogger replay_log;
extern PrefixedLogger server_log;
extern PrefixedLogger static_game_data_log;

void set_log_levels_from_json(std::shared_ptr<JSONObject> json);
