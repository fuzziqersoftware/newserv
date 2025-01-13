#pragma once

#include <phosg/JSON.hh>
#include <phosg/Strings.hh>

extern phosg::PrefixedLogger channel_exceptions_log;
extern phosg::PrefixedLogger client_log;
extern phosg::PrefixedLogger command_data_log;
extern phosg::PrefixedLogger config_log;
extern phosg::PrefixedLogger dns_server_log;
extern phosg::PrefixedLogger function_compiler_log;
extern phosg::PrefixedLogger ip_stack_simulator_log;
extern phosg::PrefixedLogger lobby_log;
extern phosg::PrefixedLogger patch_index_log;
extern phosg::PrefixedLogger player_data_log;
extern phosg::PrefixedLogger proxy_server_log;
extern phosg::PrefixedLogger replay_log;
extern phosg::PrefixedLogger server_log;
extern phosg::PrefixedLogger static_game_data_log;

void set_log_levels_from_json(const phosg::JSON& json);
