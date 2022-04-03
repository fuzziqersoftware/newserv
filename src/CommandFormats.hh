#pragma once

#include <string>
#include <stdexcept>
#include <phosg/Strings.hh>
#include <phosg/Encoding.hh>

#include "PSOProtocol.hh"



#pragma pack(push)
#pragma pack(1)



// This file is newserv's canonical reference of the PSO client/server protocol.

// For the unfamiliar, the le_uint and be_uint types (from phosg/Encoding.hh)
// are the same as normal uint types, but are explicitly little-endian or
// big-endian. The parray and ptext types (from Text.hh) are the same as
// standard arrays, but have various safety and convenience features so we don't
// have to use easy-to-mess-up functions like memset/memcpy and strncpy.

// Struct names are like [S|C|SC]_CommandName_[Versions]_Numbers
// S/C denotes who sends the command (S = server, C = client, SC = both)
// If versions are not specified, the format is the same for all versions.

// For variable-length commands, generally a zero-length array is included on
// the end of the struct if the command is received by newserv, and is omitted
// if it's sent by newserv. In the latter case, we often use StringWriter to
// construct the command data instead.

// Structures are sorted by command number. Long BB commands are placed in order
// according to their low byte; for example, command 01EB is in position as EB.



// This function is used in a lot of places to check received command sizes and
// cast them to the appropriate type
template <typename T>
const T& check_size_t(
    const std::string& data,
    size_t min_size = sizeof(T),
    size_t max_size = sizeof(T)) {
  if (data.size() < min_size) {
    throw std::runtime_error(string_printf(
        "command too small (expected at least 0x%zX bytes, received 0x%zX bytes)",
        min_size, data.size()));
  }
  if (data.size() > max_size) {
    throw std::runtime_error(string_printf(
        "command too large (expected at most 0x%zX bytes, received 0x%zX bytes)",
        max_size, data.size()));
  }
  return *reinterpret_cast<const T*>(data.data());
}
template <typename T>
T& check_size_t(
    std::string& data,
    size_t min_size = sizeof(T),
    size_t max_size = sizeof(T)) {
  if (data.size() < min_size) {
    throw std::runtime_error(string_printf(
        "command too small (expected at least 0x%zX bytes, received 0x%zX bytes)",
        min_size, data.size()));
  }
  if (data.size() > max_size) {
    throw std::runtime_error(string_printf(
        "command too large (expected at most 0x%zX bytes, received 0x%zX bytes)",
        max_size, data.size()));
  }
  return *reinterpret_cast<T*>(data.data());
}



// 00: Invalid command

// 01 (S->C): Lobby message box
// Message box appears in lower-right corner; user must press a key to continue

struct SC_TextHeader_01_06_11_B0 {
  le_uint32_t unused;
  le_uint32_t guild_card_number;
  // Text immediately follows this header (char[] on DC/GC, char16_t[] on PC/BB)
};

// 02 (S->C): Start encryption (except on BB)
// Client will respond with an (encrypted) 9A, 9D, or 9E command

struct S_ServerInit_DC_PC_GC_02_17 {
  ptext<char, 0x40> copyright;
  le_uint32_t server_key; // Key for data sent by server
  le_uint32_t client_key; // Key for data sent by client
  // This field is not part of SEGA's original implementation
  ptext<char, 0xC0> after_message;
};

struct S_ServerInit_Patch_02 {
  ptext<char, 0x40> copyright;
  le_uint32_t server_key;
  le_uint32_t client_key;
  // BB rejects the command if it's not exactly this size, so we can't add the
  // after_message like we do in the other server init commands
};

// 03 (S->C): Start encryption (BB)
// Client will respond with a 93 command

struct S_ServerInit_BB_03 {
  ptext<char, 0x60> copyright;
  parray<uint8_t, 0x30> server_key;
  parray<uint8_t, 0x30> client_key;
  // This field is not part of SEGA's original implementation
  ptext<char, 0xC0> after_message;
};

// 04 (S->C): Set guild card number and update client config ("security data")
// Client will respond with a 96 command, but only the first time it receives
// this command (TODO: Is this true? It might instead be that it responds with a
// 96 only if encryption was started with a 17 rather than an 02)

struct S_UpdateClientConfig_DC_PC_GC_04 {
  le_uint32_t player_tag;
  le_uint32_t guild_card_number;
  // The ClientConfig structure (defined in Client.hh) describes how newserv
  // uses this command; other servers do not use the same format for the
  // following 0x20 bytes (or may not use it at all). The cfg field is opaque to
  // the client; it will send back the contents verbatim in its next 9E command.
  ClientConfig cfg;
};

// 04 (S->C): Request login information (patch server) (no arguments)
// 04 (C->S): Log in (patch server)

struct C_Login_Patch_04 {
  parray<le_uint32_t, 3> unused;
  ptext<char, 0x10> username;
  ptext<char, 0x10> password;
  ptext<char, 0x40> email; // Note: this field is not present on BB
};

// 05: Disconnect
// No arguments

// 06: Chat
// Server->client format is same as 01 command
// Client->server format is very similar; we include a zero-length array in this
// struct to make parsing easier
// The guild_card_number field is used only in server->client commands

struct C_Chat_06 {
  uint64_t unused;
  union {
    char dcgc[0];
    char16_t pcbb[0];
  } text;
};


// 07 (S->C): Ship select menu

// Command is a list of these; header.flag is the entry count. The first entry
// is not included in the count and does not appear on the client. The text of
// the first entry becomes the ship name when the client joins a lobby.
template <typename CharT, int EntryLength>
struct S_MenuEntry {
  le_uint32_t menu_id;
  le_uint32_t item_id;
  le_uint16_t flags; // should be 0x0F04
  ptext<CharT, EntryLength> text;
};
struct S_MenuEntry_PC_BB_07 : S_MenuEntry<char16_t, 17> { };
struct S_MenuEntry_DC_GC_07 : S_MenuEntry<char, 18> { };

// 08 (C->S): Request game list
// No arguments

// 08 (S->C): Game list
// Client responds with 09 and 10 commands

// Command is a list of these; header.flag is the entry count. The first entry
// is not included in the count and does not appear on the client.
template <typename CharT>
struct S_GameMenuEntry {
  le_uint32_t menu_id;
  le_uint32_t game_id;
  uint8_t difficulty_tag; // 0x0A = Ep3; else difficulty + 0x22 (so 0x25 = Ult)
  uint8_t num_players;
  ptext<CharT, 0x10> name;
  uint8_t episode; // 40 = Ep1, 41 = Ep2, 43 = Ep4. Ignored on Ep3
  uint8_t flags; // 02 = locked, 04 = disabled (BB), 10 = battle, 20 = challenge
};
struct S_GameMenuEntry_PC_BB_08 : S_GameMenuEntry<char16_t> { };
struct S_GameMenuEntry_GC_08 : S_GameMenuEntry<char> { };

// 09 (S->C): Check directory (patch server)

struct S_CheckDirectory_Patch_09 {
  ptext<char, 0x40> name;
};

// 09 (C->S): Menu item info request
// Server will respond with an 11 command, or an A3 if it's the quest menu

struct C_MenuItemInfoRequest_09 {
  le_uint32_t menu_id;
  le_uint32_t item_id;
};

// 0A: Done checking directory (patch server)
// No arguemnts

// 0B: Invalid command

// 0C: Create game (DCv1)
// Format unknown

// 0D: Invalid command
// 0E: Invalid command
// 0F: Invalid command

// 10 (C->S): Menu selection

struct C_MenuSelection {
  le_uint32_t menu_id;
  le_uint32_t item_id;
  // Password is only present when client attempts to join a locked game
  union {
    char dcgc[0];
    char16_t pcbb[0];
  } password;
};

// 11: Ship info
// Same format as 01 command

// 12: Session complete (patch server)
// No arguments

// 13 (S->C): Message box (patch server)
// Same as 1A/D5 command

// 13 (C->S): Confirm file write
// Client sends this in response to each 13 sent by the server
// Format not documented here

// 13 (S->C): Write online quest file
// Used for downloading online quests

// Header flag = file chunk index
struct S_WriteFile_13_A7 {
  ptext<char, 0x10> filename;
  uint8_t data[0x400];
  le_uint32_t data_size;
};

// 14: Invalid command
// 15: Invalid command
// 16: Invalid command

// 17 (S->C): Start encryption at login server (except on BB)
// Same format as 02 command, but a different copyright string
// Client will respond with a DB command

// 18: Invalid command

// 19 (S->C): Reconnect to different address
// Client will disconnect, and reconnect to the given address/port.

// Because PSO PC and some versions of PSO GC use the same port but different
// protocols, we use a specially-crafted 19 command to send them to two
// different ports depending on the client version.

struct S_Reconnect_19 {
  be_uint32_t address;
  le_uint16_t port;
  le_uint16_t unused;
};

struct S_ReconnectSplit_19 {
  be_uint32_t pc_address;
  le_uint16_t pc_port;
  parray<uint8_t, 0x0F> unused1;
  uint8_t gc_command;
  uint8_t gc_flag;
  le_uint16_t gc_size;
  be_uint32_t gc_address;
  le_uint16_t gc_port;
  parray<uint8_t, 0xB0 - 0x23> unused2;
};

// 1A: Large message box
// Client will usually respond with a D6 command (see D6 for more information)
// Contents are plain text (char on DC/GC, char16_t on PC/BB). There must be at
// least one null character ('\0') before the end of the command data.

// 1B: Invalid command
// 1C: Invalid command

// 1D: Ping
// No arguments
// When sent to the client, the client will respond with a 1D command

// 1E: Invalid command

// 1F (S->C): Information menu
// Same format and usage as 07 command

// 20: Invalid command

// 21: GameGuard control (BB)
// Format unknown

// 22: GameGuard check (BB)
// An older version of BB used a 4-byte challenge in the flag field (and the
// command had no payload). The latest version uses this 16-byte challenge.

struct SC_GameCardCheck_BB_22 {
  parray<uint8_t, 0x10> data;
};

// 23: Invalid command
// 24: Invalid command
// 25: Invalid command
// 26: Invalid command
// 27: Invalid command
// 28: Invalid command
// 29: Invalid command
// 2A: Invalid command
// 2B: Invalid command
// 2C: Invalid command
// 2D: Invalid command
// 2E: Invalid command
// 2F: Invalid command
// 30: Invalid command
// 31: Invalid command
// 32: Invalid command
// 33: Invalid command
// 34: Invalid command
// 35: Invalid command
// 36: Invalid command
// 37: Invalid command
// 38: Invalid command
// 39: Invalid command
// 3A: Invalid command
// 3B: Invalid command
// 3C: Invalid command
// 3D: Invalid command
// 3E: Invalid command
// 3F: Invalid command

// 40: Guild card search

struct C_GuildCardSearch_40 {
  le_uint32_t player_tag;
  le_uint32_t searcher_guild_card_number;
  le_uint32_t target_guild_card_number;
} __attribute__((packed));

template <typename HeaderT, typename CharT>
struct S_GuildCardSearchResult {
  le_uint32_t player_tag;
  le_uint32_t searcher_guild_card_number;
  le_uint32_t result_guild_card_number;
  HeaderT reconnect_command_header;
  S_Reconnect_19 reconnect_command;
  ptext<char, 0x44> location_string;
  le_uint32_t menu_id;
  le_uint32_t lobby_id;
  ptext<char, 0x3C> unused;
  ptext<CharT, 0x20> name;
};
struct S_GuildCardSearchResult_PC_41 : S_GuildCardSearchResult<PSOCommandHeaderPC, char16_t> { };
struct S_GuildCardSearchResult_DC_GC_41 : S_GuildCardSearchResult<PSOCommandHeaderDCGC, char> { };
struct S_GuildCardSearchResult_BB_41 : S_GuildCardSearchResult<PSOCommandHeaderBB, char16_t> { };

// 41: Invalid command
// 42: Invalid command
// 43: Invalid command

// 44 (C->S): Confirm open file
// The client sends a 44 to confirm each 44 sent by the server
// FOrmat not documented here

// 44 (S->C): Open file for download
// Used for downloading online quests

struct S_OpenFile_PC_GC_44_A6 {
  ptext<char, 0x20> name;
  le_uint16_t unused;
  le_uint16_t flags;
  ptext<char, 0x10> filename;
  le_uint32_t file_size;
};

struct S_OpenFile_BB_44_A6 {
  parray<uint8_t, 0x22> unused;
  le_uint16_t flags;
  ptext<char, 0x10> filename;
  le_uint32_t file_size;
  ptext<char, 0x18> name;
};

// 45: Invalid command
// 46: Invalid command
// 47: Invalid command
// 48: Invalid command
// 49: Invalid command
// 4A: Invalid command
// 4B: Invalid command
// 4C: Invalid command
// 4D: Invalid command
// 4E: Invalid command
// 4F: Invalid command
// 50: Invalid command
// 51: Invalid command
// 52: Invalid command
// 53: Invalid command
// 54: Invalid command
// 55: Invalid command
// 56: Invalid command
// 57: Invalid command
// 58: Invalid command
// 59: Invalid command
// 5A: Invalid command
// 5B: Invalid command
// 5C: Invalid command
// 5D: Invalid command
// 5E: Invalid command
// 5F: Invalid command

// 60: Broadcast command
// When client sends this command, the server should forward it to all players
// in the same game/lobby
// See ReceiveSubcommands for details on contents

// 61: Player data
// See PSOPlayerDataPC, PSOPlayerDataGC, PSOPlayerDataBB in Player.hh for this
// command's format

// 62: Target command
// When client sends this command, the server should forward it to the player
// identified by header.flag in the same game/lobby
// See ReceiveSubcommands for details on contents

// 63: Invalid command

// 64 (S->C): Join game
// This is sent to the joining player; the other players get a 65 instead

// Header flag = entry count
template <typename LobbyDataT, typename DispDataT>
struct S_JoinGame {
  parray<le_uint32_t, 0x20> variations;
  // Unlike lobby join commands, these are filled in in their slot positions.
  // That is, if there's one player in a game with ID 2, then the first two of
  // these are blank and the player's data is in the third entry here.
  LobbyDataT lobby_data[4];
  uint8_t client_id;
  uint8_t leader_id;
  uint8_t disable_udp;
  uint8_t difficulty;
  uint8_t battle_mode;
  uint8_t event;
  uint8_t section_id;
  uint8_t challenge_mode;
  le_uint32_t rare_seed;
  uint8_t episode;
  uint8_t unused2; // Should be 1 for PSO PC?
  uint8_t solo_mode;
  uint8_t unused3;
  struct PlayerEntry {
    PlayerInventory inventory;
    DispDataT disp;
  };
  // This field is only present if the game (+client) is Episode 3. Similarly to
  // lobby_data above, all four of these are always present and they are filled
  // in in slot positions.
  PlayerEntry players_ep3[4];
};
struct S_JoinGame_PC_64 : S_JoinGame<PlayerLobbyDataPC, PlayerDispDataPCGC> { };
struct S_JoinGame_GC_64 : S_JoinGame<PlayerLobbyDataGC, PlayerDispDataPCGC> { };
struct S_JoinGame_BB_64 : S_JoinGame<PlayerLobbyDataBB, PlayerDispDataBB> { };

// 65 (S->C): Other player joined game

// Header flag = entry count (always 1 for 65 and 68; up to 0x0C for 67)
template <typename LobbyDataT, typename DispDataT>
struct S_JoinLobby {
  uint8_t client_id;
  uint8_t leader_id;
  uint8_t disable_udp;
  uint8_t lobby_number;
  le_uint16_t block_number;
  le_uint16_t event;
  le_uint32_t unused;
  struct Entry {
    LobbyDataT lobby_data;
    PlayerInventory inventory;
    DispDataT disp;
  };
  // Note: not all of these will be filled in and sent if the lobby isn't full
  // (the command size will be shorter than this struct's size)
  Entry entries[0x0C];

  static inline size_t size(size_t used_entries) {
    return offsetof(S_JoinLobby, entries) + used_entries * sizeof(Entry);
  }
};
struct S_JoinLobby_PC_65_67_68 : S_JoinLobby<PlayerLobbyDataPC, PlayerDispDataPCGC> { };
struct S_JoinLobby_GC_65_67_68 : S_JoinLobby<PlayerLobbyDataGC, PlayerDispDataPCGC> { };
struct S_JoinLobby_BB_65_67_68 : S_JoinLobby<PlayerLobbyDataBB, PlayerDispDataBB> { };

// 66 (S->C): Other player left game
// Not sent to the leaving player

// Header flag = leaving player ID (same as client_id);
struct S_LeaveLobby_66_69 {
  uint8_t client_id;
  uint8_t leader_id;
  le_uint16_t unused;
};

// 67 (S->C): Join lobby
// This is sent to the joining player; the other players get a 68 instead
// Same format as 65 command

// 68 (S->C): Other player joined lobby
// Same format as 65 command

// 69 (S->C): Other player left lobby
// Not sent to the leaving player
// Same format as 66 command

// 6A: Invalid command
// 6B: Invalid command

// 6C: Broadcast command
// Same format and usage as 60 command

// 6D: Target command
// Same format and usage as 62 command

// 6E: Invalid command

// 6F: Set game status
// This command is sent when a player is done loading and other players can then
// join the game
// On BB, this command is sent as 016F if a quest is in progress and the game
// should not be joined by anyone else

// 70: Invalid command
// 71: Invalid command
// 72: Invalid command
// 73: Invalid command
// 74: Invalid command
// 75: Invalid command
// 76: Invalid command
// 77: Invalid command
// 78: Invalid command
// 79: Invalid command
// 7A: Invalid command
// 7B: Invalid command
// 7C: Invalid command
// 7D: Invalid command
// 7E: Invalid command
// 7F: Invalid command
// 80: Invalid command

// 81: Simple mail
// Format is the same in both directions. On GC (and probably other versions
// too) the unused space after the text contains uninitialized memory when the
// client sends this command

struct SC_SimpleMail_GC_81 {
  le_uint32_t player_tag;
  le_uint32_t from_guild_card_number;
  ptext<char, 0x10> from_name;
  le_uint32_t to_guild_card_number;
  ptext<char, 0x200> text;
};

// 82: Invalid command

// 83 (S->C): Lobby menu
// This sets the menu item IDs that the client uses for the lobby teleport menu

// Command is a list of these; header.flag is the entry count (15, or 20 on Ep3)
struct S_LobbyListEntry_83 {
  le_uint32_t menu_id;
  le_uint32_t item_id;
  le_uint32_t unused;
};

// 84 (C->S): Choose lobby

struct C_LobbySelection_84 {
  le_uint32_t menu_id;
  le_uint32_t item_id;
};

// 85: Invalid command
// 86: Invalid command
// 87: Invalid command

// 88 (S->C): Lobby arrows

// Command is a list of these; header.flag is the entry count
struct S_ArrowUpdateEntry_88 {
  le_uint32_t player_tag;
  le_uint32_t guild_card_number;
  le_uint32_t arrow_color;
};

// 89 (C->S): Set lobby arrow
// Header flag = arrow color number; no other arguments
// Server should send an 88 command to all players in the lobby

// 8A (C->S): Request lobby/game name
// No arguments

// 8A (S->C): Lobby/game name
// Contents is a string (char16_t on PC/BB, char on DC/GC) containing the lobby
// or game name

// 8B: Invalid command
// 8C: Invalid command
// 8D: Invalid command
// 8E: Invalid command
// 8F: Invalid command
// 90: Invalid command
// 91: Invalid command
// 92: Invalid command

// 93 (C->S): Log in (BB)

struct C_Login_BB_93 {
  ptext<char, 0x14> unused;
  ptext<char, 0x10> username;
  ptext<char, 0x20> unused2;
  ptext<char, 0x10> password;
  ptext<char, 0x30> unused3;
  ClientConfigBB client_config;
};

// 94: Invalid command

// 95 (S->C): Request player data
// No arguments
// Client will respond with a 61 command

// 96 (C->S): Client checksum

struct C_ClientChecksum_GC_96 {
  le_uint64_t checksum;
};

// 97 (S->C): Save to memory card
// No arguments

// 98 (C->S): Leave game
// Same format a 61 command
// Client will send an 84 when it's ready to join a lobby

// 99 (C->S): Server time accepted
// No arguments

// 9A (S->C): Verify result
// 02 = license ok
// TODO: figure out the other codes

// 9A (C->S): Initial login (no password or client config)

struct C_Login_DC_PC_GC_9A {
  ptext<char, 0x20> unused;
  ptext<char, 0x10> serial_number;
  ptext<char, 0x10> access_key;
  uint32_t player_tag;
  uint32_t guild_card_number;
  uint32_t sub_version;
  ptext<char, 0x30> serial_number2;
  ptext<char, 0x30> access_key2;
  ptext<char, 0x30> email_address;
};

// 9B: Invalid command

// 9C (C->S): Log in result
// 01 = license ok
// TODO: figure out the other codes

// 9C (C->S): Register

struct C_Register_DC_PC_GC_9C {
  ptext<char, 8> unused;
  le_uint32_t sub_version;
  le_uint32_t unused2;
  ptext<char, 0x30> serial_number;
  ptext<char, 0x30> access_key;
  ptext<char, 0x30> password;
};

// 9D (C->S): Log in

struct C_Login_PC_9D {
  le_uint32_t player_tag; // 00 00 01 00 if guild card is set (via 04)
  le_uint32_t guild_card_number; // FF FF FF FF if not set
  le_uint64_t unused;
  le_uint32_t sub_version;
  parray<uint8_t, 0x24> unused2; // 00 01 00 00 ... (rest is 00)
  ptext<char, 0x10> serial_number;
  ptext<char, 0x10> access_key;
  ptext<char, 0x30> serial_number2;
  ptext<char, 0x30> access_key2;
  ptext<char, 0x10> name;
};
struct C_LoginWithUnusedSpace_PC_9D : C_Login_PC_9D {
  parray<uint8_t, 0x84> unused_space;
};

// 9E (C->S): Log in with client config

// This struct is identical to PC's 9D command, but it has more data at the end
struct C_Login_GC_9E : C_Login_PC_9D {
  union ClientConfigFields {
    ClientConfig cfg;
    parray<uint8_t, 0x20> data;
    ClientConfigFields() : data() { }
  } client_config;
};
struct C_LoginWithUnusedSpace_GC_9E : C_Login_GC_9E {
  parray<uint8_t, 0x64> unused_space;
};

// 9F: Invalid command

// A0 (C->S): Change ship
// No arguments

// A0 (S->C): Ship select menu
// Same as 07 command

// A1 (C->S): Change block
// No arguments

// A1 (S->C): Block select menu
// Same as 07 command

// A2 (C->S): Request quest list
// No arguments

// A2 (S->C): Quest menu

template <typename CharT>
struct S_QuestMenuEntry {
  le_uint32_t menu_id;
  le_uint32_t item_id;
  ptext<CharT, 0x20> name;
  ptext<CharT, 0x70> short_desc;
};
struct S_QuestMenuEntry_PC_A2_A4 : S_QuestMenuEntry<char16_t> { };
struct S_QuestMenuEntry_GC_A2_A4 : S_QuestMenuEntry<char> { };

struct S_QuestMenuEntry_BB_A2_A4 {
  le_uint32_t menu_id;
  le_uint32_t item_id;
  ptext<char16_t, 0x20> name;
  // Why is this 10 characters longer than on other versions...?
  ptext<char16_t, 0x7A> short_desc;
};

// A3 (S->C): Quest information
// Same format as 1A/D5 command (plain text)

// A4 (S->C): Download quest menu
// Same format as A2

// A5: Invalid command

// A6: Open file for download
// Used for download quests and GBA games
// Same format as 44

// A7: Write download file
// Same format as 13

// A8: Invalid command

// A9: Quest menu closed
// No arguments

// AA: Invalid command
// AB: Invalid command

// AC (C->S): Ready to start quest
// AC (S->C): Start quest
// No arguments
// When all players in a game have sent an AC to the server, the server should
// send them all an AC back, which starts the quest for all players at
// (approximately) the same time
// Sending this command to a GC client when it is not waiting to start a quest
// will cause it to crash

// AD: Invalid command
// AE: Invalid command
// AF: Invalid command

// B0: Text message
// Same format as 01 command (plain text with unused header)

// B1 (C->S): Request server time
// No arguments

// B1 (S->C): Server time
// Contents is a string of format "%Y:%m:%d: %H:%M:%S.000"
// For example: 2022:03:30: 15:36:42.000

// B2 (S->C): Write memory
// GC v1.0 and v1.1 only
// Format unknown
// Client will respond with a B3 command

// B3 (C->S): Write memory response
// GC v1.0 and v1.1 only
// Format unknown

// B4: Invalid command
// B5: Invalid command
// B6: Invalid command

// B7: Rank update (Episode 3)

struct S_RankUpdate_GC_Ep3_B7 {
  le_uint32_t rank;
  ptext<char, 0x0C> rank_text;
  le_uint32_t meseta;
  le_uint32_t max_meseta;
  le_uint32_t jukebox_songs_unlocked;
};

// B8 (S->C): Send card definitions (Episode 3)
// Contents is a single little-endian le_uint32_t specifying the size of the
// (PRS-compressed) data, followed immediately by the data

// B9: Invalid command

// BA: Meseta transaction (Episode 3)

struct C_Meseta_GC_Ep3_BA {
  le_uint32_t transaction_num;
  le_uint32_t value;
  le_uint32_t unknown_token;
};

struct S_Meseta_GC_Ep3_BA {
  le_uint32_t remaining_meseta;
  le_uint32_t unknown;
  le_uint32_t unknown_token; // Should match the token sent by the client
};

// BB: Invalid command
// BC: Invalid command
// BD: Invalid command
// BE: Invalid command
// BF: Invalid command

// C0 (C->S): Request choice search options
// No arguments

// C0 (S->C): Choice search options

// Command is a list of these; header.flag is the entry count (incl. top-level)
template <typename CharT>
struct S_ChoiceSearchEntry {
  // category_ids are nonzero; if the high byte is nonzero then the category can
  // be set by the user at any time; otherwise it can't.
  le_uint16_t parent_category_id; // 0 for top-level categories
  le_uint16_t category_id;
  ptext<CharT, 0x1C> text;
};
struct S_ChoiceSearchEntry_DC_GC_C0 : S_ChoiceSearchEntry<char> { };
struct S_ChoiceSearchEntry_PC_BB_C0 : S_ChoiceSearchEntry<char16_t> { };

// Top-level categories are things like "Level", "Class", etc.
// Choices for each top-level category immediately follow the category, so
// a reasonable order of items is (for example):
//   00 00 11 01 "Preferred difficulty"
//   11 01 01 01 "Normal"
//   11 01 02 01 "Hard"
//   11 01 03 01 "Very Hard"
//   11 01 04 01 "Ultimate"
//   00 00 22 00 "Character class"
//   22 00 01 00 "HUmar"
//   22 00 02 00 "HUnewearl"
//   etc.

// C1 (C->S): Create game

template <typename CharT>
struct C_CreateGame {
  le_uint64_t unused;
  ptext<CharT, 0x10> name;
  ptext<CharT, 0x10> password;
  uint8_t difficulty;
  uint8_t battle_mode;
  uint8_t challenge_mode;
  uint8_t episode; // unused on DC/PC
};
struct C_CreateGame_DC_GC_C1_EC : C_CreateGame<char> { };
struct C_CreateGame_PC_C1 : C_CreateGame<char16_t> { };

struct C_CreateGame_BB_C1 : C_CreateGame<char16_t> {
  uint8_t solo_mode;
  uint8_t unused2[3];
};

// C2 (C->S): Set choice search parameters

struct C_SetChoiceSearchParameters_C2 {
  le_uint16_t disabled; // 0 = enabled, 1 = disabled
  le_uint16_t unused;
  struct Entry {
    le_uint16_t parent_category_id;
    le_uint16_t category_id;
  };
  Entry entries[0];
};

// C3 (C->S): Execute choice search

struct C_ExecuteChoiceSearch_C3 {
  le_uint32_t unknown;
  struct Entry {
    le_uint16_t parent_category_id;
    le_uint16_t category_id;
  };
  Entry entries[0];
};

// C4 (S->C): Choice search results

// Command is a list of these; header.flag is the entry count
struct S_ChoiceSearchResultEntry_GC_C4 {
  le_uint32_t guild_card_number;
  ptext<char, 0x10> name; // No language marker, as usual on GC
  ptext<char, 0x20> info_string; // Usually something like "<class> Lvl <level>"
  // Format is stricter here; this is "LOBBYNAME,BLOCKNUM,SHIPNAME"
  // If target is in game, for example, "Game Name,BLOCK01,Alexandria"
  // If target is in lobby, for example, "BLOCK01-1,BLOCK01,Alexandria"
  ptext<char, 0x34> locator_string;
  // Server IP and port for "meet user" option
  le_uint32_t server_ip;
  le_uint16_t server_port;
  le_uint16_t unused1;
  le_uint32_t menu_id;
  le_uint32_t lobby_id; // These two are guesses
  le_uint32_t game_id; // Zero if target is in a lobby rather than a game
  parray<uint8_t, 0x58> unused2;
};

// C5 (S->C): Challenge rank update
// TODO: Document format for this command

// C6 (C->S): Set blocked senders list

struct C_SetBlockedSenders_C6 {
  parray<le_uint32_t, 30> blocked_senders;
};

// C7 (C->S): Enable simple mail auto-reply
// Same format as 1A/D5 command (plain text)

// C8 (C->S): Disable simple mail auto-reply

// C9: Broadcast command (Episode 3)
// Same as 60, but only send to Episode 3 clients

// CA (C->S): Ep3 server data request

// CB: Broadcast command (Episode 3)
// Same as 60, but only send to Episode 3 clients

// CC: Invalid command
// CD: Invalid command
// CE: Invalid command
// CF: Invalid command

// D0 (C->S): Execute trade via trade window
// General sequence: client sends D0, server sends D1 to that client, server
// sends D3 to other client, server sends D4 to both (?) clients
// Format unknown

// D1 (S->C): Confirm trade to initiator
// No arguments

// D2: Invalid command

// D3 (S->C): Execute trade with accepter
// Format unknown; appears to be same as D0

// D4 (S->C): Close trade
// No arguments

// D5: Large message box
// Same as 1A command

// D6 (C->S): Large message box closed (GC)
// No arguments
// DC and PC do not send this command at all. GC v1.0 and v1.1 will send this
// command when any large message box is closed; GC Plus and Episode 3 will send
// this only for large message boxes that are sent before the client has joined
// a lobby.

// D7 (C->S): Request GBA game file

struct C_GBAGameRequest_GC_D7 {
  ptext<char, 0x10> filename;
};

// D8 (S->C): Info board

// Command is a list of these; header.flag is the entry count
template <typename CharT>
struct S_InfoBoardEntry_D8 {
  ptext<CharT, 0x10> name;
  ptext<CharT, 0xAC> message;
};
struct S_InfoBoardEntry_PC_BB_D8 : S_InfoBoardEntry_D8<char16_t> { };
struct S_InfoBoardEntry_DC_GC_D8 : S_InfoBoardEntry_D8<char> { };

// D9 (C->S): Write info board
// Contents are plain text, like 1A/D5

// DA (S->C): Change lobby event
// Header flag = new event number. No other arguments

// DB (S->C): Verify license (GC)

struct C_VerifyLicense_GC_DB {
  ptext<char, 0x20> unused;
  ptext<char, 0x10> serial_number;
  ptext<char, 0x10> access_key;
  ptext<char, 0x08> unused2;
  le_uint32_t sub_version;
  ptext<char, 0x30> serial_number2;
  ptext<char, 0x30> access_key2;
  ptext<char, 0x30> password;
};

// DC: Player menu state (Episode 3)
// No arguments. Client expects server to respond with command = DC, flag = 0

// DC: Guild card data (BB)

struct C_GuildCardDataRequest_BB_DC {
  le_uint32_t unknown;
  le_uint32_t chunk_index;
  le_uint32_t cont;
};

struct S_GuildCardHeader_BB_01DC {
  le_uint32_t unknown; // should be 1
  le_uint32_t filesize; // 0x0000490
  le_uint32_t checksum;
};

// Command 02DC is used to send the guild card file data. It goes like this:
// uint32_t unknown; // 0
// uint32_t chunk_index;
// uint8_t data[0x6800, or less if last chunk]

// DD: Invalid command
// DE: Unknown (used by BB)
// DF: Invalid command

// E0 (S->C): Tournament list (Episode 3)

// Command is a list of these; header.flag is the entry count. Some servers
// always send 0x740 bytes even if the entries don't fill the space.
struct S_TournamentEntry_GC_Ep3_E0 {
  le_uint32_t menu_id;
  le_uint32_t item_id;
  parray<uint8_t, 0x30> unknown;
};

// E0 (C->S): Request team and key config (BB)

// E1: Invalid command

// E2 (C->S): Tournament control (Episode 3)
// Flag = 0 => request tournament list (server responds with E0)
// Flag = 1 => check tournament
// Flag = 2 => cancel tournament entry
// Flag = 3 => create tournament spectator team
// Flag = 4 => join tournament spectator team

// E2 (S->C): Tournament entry control (Episode 3)

// E2 (S->C): Team and key config (BB)
// See KeyAndTeamConfigBB in Player.hh for format

// E3: Player previews (BB)

struct C_PlayerPreviewRequest_BB_E3 {
  le_uint32_t player_index;
  le_uint32_t unused;
};

struct S_PlayerPreview_BB_E3 {
  le_uint32_t player_index;
  PlayerDispDataBBPreview preview;
};

// E4: CARD lobby game (Episode 3)

// Header flag = seated state (1 = present, 0 = leaving)
struct C_CardLobbyGame_GC_E4 {
  le_uint16_t table_number;
  le_uint16_t seat_number;
};

// Header flag = 2
struct S_CardLobbyGame_GC_E4 {
  struct Entry {
    le_uint32_t present; // 1 = player present, 0 = no player
    le_uint32_t guild_card_number;
  };
  Entry entries[4];
};

// E4 (S->C): Player choice or no player present (BB)

struct S_ApprovePlayerChoice_BB_00E4 {
  le_uint32_t player_index;
  le_uint32_t unused;
};

struct S_PlayerPreview_NoPlayer_BB_E4 {
  le_uint32_t player_index;
  le_uint32_t error;
};

// E5 (C->S): Create character (BB)

struct C_CreateCharacter_BB_E5 {
  le_uint32_t player_index;
  PlayerDispDataBBPreview preview;
};

// E6: Spectator team list (Episode 3)
// Same format as 08 command

// E6 (S->C): Set guild card number and update client config (BB)

struct S_ClientInit_BB_E6 {
  le_uint32_t error;
  le_uint32_t player_tag;
  le_uint32_t guild_card_number;
  le_uint32_t team_id;
  ClientConfigBB cfg;
  le_uint32_t caps; // should be 0x00000102
};

// E7: Save or load full player data
// See export_bb_player_data() in Player.cc for format

// E8 (C->S): Client checksum (BB)
// E8 (S->C): Accept client checksum (BB)

struct S_AcceptClientChecksum_BB_02E8 {
  le_uint32_t verify;
  le_uint32_t unused;
};

// E9: Invalid command
// EA: Team control (BB)

// EB: Send stream file index and chunks

// Command is a list of these; header.flag is the entry count.
struct S_StreamFileIndexEntry_BB_01EB {
  le_uint32_t size;
  le_uint32_t checksum;
  le_uint32_t offset;
  ptext<char, 0x40> filename;
} __attribute__((packed));

struct S_StreamFileChunk_BB_02EB {
  le_uint32_t chunk_index;
  uint8_t data[0x6800];
};

// EC: Create game (Episode 3)
// Same format as C1; some fields are unused (e.g. episode, difficulty)
// TODO: Where does the "view battle allowed" flag end up?

// EC: Leave character select (BB)

// ED (C->S): Update account data (BB)
// TODO: Actually define these structures and don't just treat them as raw data

union C_UpdateAccountData_BB_ED {
  le_uint32_t option; // 01ED
  parray<uint8_t, 0x4E0> symbol_chats; // 02ED
  parray<uint8_t, 0xA40> chat_shortcuts; // 03ED
  parray<uint8_t, 0x16C> key_config; // 04ED
  parray<uint8_t, 0x38> pad_config; // 05ED
  parray<uint8_t, 0x28> tech_menu; // 06ED
  parray<uint8_t, 0xE8> customize; // 07ED
} __attribute__((packed));

// EE: Scrolling message (BB)
// Contents are plain text (char16_t)

// EF: Invalid command
// F0: Invalid command
// F1: Invalid command
// F2: Invalid command
// F3: Invalid command
// F4: Invalid command
// F5: Invalid command
// F6: Invalid command
// F7: Invalid command
// F8: Invalid command
// F9: Invalid command
// FA: Invalid command
// FB: Invalid command
// FC: Invalid command
// FD: Invalid command
// FE: Invalid command
// FF: Invalid command





struct S_SendGuildCard_GC {
  uint8_t subcommand;
  uint8_t subsize;
  le_uint16_t unused;
  le_uint32_t player_tag;
  le_uint32_t guild_card_number;
  ptext<char, 0x18> name;
  ptext<char, 0x6C> desc;
  uint8_t reserved1;
  uint8_t reserved2;
  uint8_t section_id;
  uint8_t char_class;
};

struct S_SendGuildCard_BB {
  uint8_t subcommand;
  uint8_t subsize;
  le_uint16_t unused;
  le_uint32_t guild_card_number;
  ptext<char16_t, 0x18> name;
  ptext<char16_t, 0x10> team_name;
  ptext<char16_t, 0x58> desc;
  uint8_t reserved1;
  uint8_t reserved2;
  uint8_t section_id;
  uint8_t char_class;
};

struct S_DropItem_BB {
  uint8_t subcommand;
  uint8_t subsize;
  le_uint16_t unused;
  uint8_t area;
  uint8_t dude;
  le_uint16_t request_id;
  float x;
  float y;
  le_uint32_t unused2;
  ItemData data;
};

struct S_DropStackedItem_BB {
  uint8_t subcommand;
  uint8_t subsize;
  le_uint16_t unused;
  le_uint16_t area;
  le_uint16_t unused2;
  float x;
  float y;
  le_uint32_t unused3;
  ItemData data;
};

struct S_PickUpItem_BB {
  uint8_t subcommand;
  uint8_t subsize;
  le_uint16_t client_id;
  le_uint16_t client_id2;
  le_uint16_t area;
  le_uint32_t item_id;
};

struct S_CreateInventoryItem_BB {
  uint8_t subcommand;
  uint8_t subsize;
  le_uint16_t client_id;
  ItemData item;
  le_uint32_t unused;
};

struct S_DestroyItem_BB {
  uint8_t subcommand;
  uint8_t subsize;
  le_uint16_t client_id;
  le_uint32_t item_id;
  le_uint32_t amount;
};

struct S_BankContentsHeader_BB {
  uint8_t subcommand;
  uint8_t unused1;
  le_uint16_t unused2;
  le_uint32_t size; // same as size in overall command header
  le_uint32_t checksum; // can be random; client won't notice
  le_uint32_t numItems;
  le_uint32_t meseta;
};

struct S_ShopContents_BB {
  uint8_t subcommand; // B6
  uint8_t size; // 2C regardless of the number of items??
  le_uint16_t params; // 037F
  uint8_t shop_type;
  uint8_t num_items;
  le_uint16_t unused;
  ItemData entries[20];
};



#pragma pack(pop)
