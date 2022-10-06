#pragma once

#include <string>
#include <stdexcept>
#include <phosg/Strings.hh>
#include <phosg/Encoding.hh>

#include "PSOProtocol.hh"
#include "Text.hh"
#include "Player.hh"



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

// The version tokens are as follows:
// DCv1 = PSO Dreamcast v1
// DCv2 = PSO Dreamcast v2
// DC = Both DCv1 and DCv2
// PC = PSO PC (v2)
// GC = PSO GC Episodes 1&2 and/or Episode 3
// XB = PSO XBOX Episodes 1&2
// BB = PSO Blue Burst
// V3 = PSO GC and PSO XBOX (these versions are similar and share many formats)

// For variable-length commands, generally a zero-length array is included on
// the end of the struct if the command is received by newserv, and is omitted
// if it's sent by newserv. In the latter case, we often use StringWriter to
// construct the command data instead.

// Structures are sorted by command number. Long BB commands are placed in order
// according to their low byte; for example, command 01EB is in position as EB.



// This is the format of newserv's security data, which we call the client
// config. This data is opaque to the client, so this structure is not
// technically part of the PSO protocol. Because it is opaque to the client, we
// can use the server's native-endian types instead of being explicit as we do
// for all the other structs in this file.
enum ClientStateBB : uint8_t {
  // Initial connection; server will redirect client to another port
  INITIAL_LOGIN = 0x00,
  // Second connection; server will send client game data and account data
  DOWNLOAD_DATA = 0x01,
  // Third connection; client will show the choose character menu
  CHOOSE_PLAYER = 0x02,
  // Fourth connection; used for saving characters only. If you do not create a
  // character, the server sets this state during the third connection so this
  // connection is effectively skipped.
  SAVE_PLAYER = 0x03,
  // Fifth connection; redirects client to login server
  SHIP_SELECT = 0x04,
  // All other connections
  IN_GAME = 0x05,
};

struct ClientConfig {
  uint64_t magic;
  uint16_t flags;
  uint32_t proxy_destination_address;
  uint16_t proxy_destination_port;
  parray<uint8_t, 0x10> unused;
};

struct ClientConfigBB {
  ClientConfig cfg;
  uint8_t bb_game_state;
  uint8_t bb_player_index;
  parray<uint8_t, 0x06> unused;
};



// Patch server commands

// The patch protocol is identical between PSO PC and PSO BB (the only versions
// on which it is used).

// A patch server session generally goes like this:
// Server: 02 (unencrypted)
// (all the following commands encrypted with PSO V2 encryption, even on BB)
// Client: 02
// Server: 04
// Client: 04
// If client's login information is wrong and server chooses to reject it:
//   Server: 15
//   Server disconnects
// Otherwise:
//   Server: 13 (if desired)
//   Server: 0B
//   Server: 09 (with directory name ".")
//   For each directory to be checked:
//     Server: 09
//     Server: (commands to check subdirectories - more 09/0A/0C)
//     For each file in the directory:
//       Server: 0C
//     Server: 0A
//   Server: 0D
//   For each 0C sent by the server earlier:
//     Client: 0F
//   Client: 10
//   If there are any files to be updated:
//     Server: 11
//     For each directory containing files to be updated:
//       Server: 09
//       Server: (commands to update subdirectories)
//       For each file to be updated in this directory:
//         Server: 06
//         Server: 07 (possibly multiple 07s if the file is large)
//         Server: 08
//       Server: 0A
//   Server: 12
//   Server disconnects

// 00: Invalid command
// 01: Invalid command

// 02 (S->C): Start encryption
// Client will respond with an 02 command.
// All commands after this command will be encrypted with PSO V2 encryption.
// If this command is sent during an encrypted session, the client will not
// reject it; it will simply re-initialize its encryption state and respond with
// an 02 as normal.
// The copyright field in the below structure must contain the following text:
// "Patch Server. Copyright SonicTeam, LTD. 2001"

struct S_ServerInit_Patch_02 {
  ptext<char, 0x40> copyright;
  le_uint32_t server_key; // Key for commands sent by server
  le_uint32_t client_key; // Key for commands sent by client
  // The client rejects the command if it's larger than this size, so we can't
  // add the after_message like we do in the other server init commands.
};

// 02 (C->S): Encryption started
// No arguments

// 03: Invalid command

// 04 (S->C): Request login information
// No arguments
// Client will respond with an 04 command.

// 04 (C->S): Log in (patch)
// The email field is always blank on BB. It may be blank on PC too, so this
// cannot be used to determine the game version used by a patch client.

struct C_Login_Patch_04 {
  parray<le_uint32_t, 3> unused;
  ptext<char, 0x10> username;
  ptext<char, 0x10> password;
  ptext<char, 0x40> email;
};

// 05 (S->C): Unknown
// This is probably the disconnect command, like on the game server. It seems
// the client never sends it though.
// No arguments

// 06 (S->C): Open file for writing

struct S_OpenFile_Patch_06 {
  le_uint32_t unknown; // Seems to always be zero
  le_uint32_t size;
  ptext<char, 0x30> filename;
};

// 07 (S->C): Write file
// The client's handler table says this command's maximum size is 0x6010
// including the header, but the only servers I've seen use this command limit
// chunks to 0x4010 (including the header). Unlike the game server's 13 and A7
// commands, the chunks do not need to be the same size - the game opens the
// file with the "a+b" mode each time it is written, so the new data is always
// appended to the end.

struct S_WriteFileHeader_Patch_07 {
  le_uint32_t chunk_index;
  le_uint32_t chunk_checksum; // CRC32 of the following chunk data
  le_uint32_t chunk_size;
  // The chunk data immediately follows here
};

// 08 (S->C): Close current file
// The unused field is optional. It's not clear whether this field was ever
// used; it could be a remnant from pre-release testing, or someone could have
// simply set the maximum size of this command incorrectly.

struct S_CloseCurrentFile_Patch_08 {
  le_uint32_t unused;
};

// 09 (S->C): Enter directory

struct S_EnterDirectory_Patch_09 {
  ptext<char, 0x40> name;
};

// 0A (S->C): Exit directory
// No arguments

// 0B (S->C): Start patch session and go to patch root directory
// No arguments

// 0C (S->C): File checksum request

struct S_FileChecksumRequest_Patch_0C {
  le_uint32_t request_id;
  ptext<char, 0x20> filename;
};

// 0D (S->C): End of file checksum requests
// No arguments

// 0E: Invalid command

// 0F (C->S): File information

struct C_FileInformation_Patch_0F {
  le_uint32_t request_id; // Matches a request ID from an earlier 0C command
  le_uint32_t checksum; // CRC32 of the file's data
  le_uint32_t size;
};

// 10 (C->S): End of file information command list
// No arguments

// 11 (S->C): Start file downloads

struct S_StartFileDownloads_Patch_11 {
  le_uint32_t total_bytes;
  le_uint32_t num_files;
};

// 12 (S->C): End patch session successfully
// No arguments

// 13 (S->C): Message box
// Same format and usage as commands 1A/D5 on the game server (described below).
// On PSOBB, the message box appears in the upper half of the screen and
// functions like a normal PSO message box - that is, you can use color escapes
// (\tCG, for example) and lines are terminated with \n. On PSOPC, the message
// appears in a Windows edit control, so the text functions differently: line
// breaks must be \r\n and standard PSO color escapes don't work. The maximum
// size of this command is 0x2004 bytes, including the header.

// 14 (S->C): Reconnect
// Same format and usage as command 19 on the game server (described below),
// except the port field is big-endian for some reason.

template <typename PortT>
struct S_Reconnect {
  be_uint32_t address;
  PortT port;
  le_uint16_t unused;
};

struct S_Reconnect_Patch_14 : S_Reconnect<be_uint16_t> { };

// 15 (S->C): Login failure
// No arguments
// The client shows a message like "Incorrect game ID or password" and
// disconnects.

// No commands beyond 15 are valid on the patch server.



// Game server commands

// 00: Invalid command

// 01 (S->C): Lobby message box
// A small message box appears in lower-right corner, and the player must press
// a key to continue. The maximum length of the message is 0x200 bytes.
// This format is shared by multiple commands; for all of them except 06 (S->C),
// the guild_card_number field is unused and should be 0.

struct SC_TextHeader_01_06_11_B0_EE {
  le_uint32_t unused;
  le_uint32_t guild_card_number;
  // Text immediately follows here (char[] on DC/V3, char16_t[] on PC/BB)
};

// 02 (S->C): Start encryption (except on BB)
// This command should be used for non-initial sessions (after the client has
// already selected a ship, for example). Command 17 should be used instead for
// the first connection.
// All commands after this command will be encrypted with PSO V2 encryption on
// DC, PC, and GC Episodes 1&2 Trial Edition, or PSO V3 encryption on other V3
// versions.
// DCv1 clients will respond with an (encrypted) 93 command.
// DCv2 and PC clients will respond with an (encrypted) 9A or 9D command.
// V3 clients will respond with an (encrypted) 9A or 9E command, except for GC
// Episodes 1&2 Trial Edition, which behaves like PC.
// The copyright field in the below structure must contain the following text:
// "DreamCast Lobby Server. Copyright SEGA Enterprises. 1999"
// (The above text is required on all versions that use this command, including
// those versions that don't run on the DreamCast.)

struct S_ServerInitDefault_DC_PC_V3_02_17_91_9B {
  ptext<char, 0x40> copyright;
  le_uint32_t server_key; // Key for data sent by server
  le_uint32_t client_key; // Key for data sent by client
};

template <size_t AfterBytes>
struct S_ServerInitWithAfterMessage_DC_PC_V3_02_17_91_9B : S_ServerInitDefault_DC_PC_V3_02_17_91_9B {
  // This field is not part of SEGA's implementation; the client ignores it.
  // newserv sends a message here disavowing the preceding copyright notice.
  ptext<char, AfterBytes> after_message;
};

// 03 (C->S): Legacy login (non-BB)
// TODO: Check if this command exists on DC v1/v2.

struct C_LegacyLogin_PC_V3_03 {
  le_uint64_t unused; // Same as unused field in 9D/9E
  le_uint32_t sub_version;
  uint8_t unknown_a1;
  uint8_t language; // Same as 9D/9E
  le_uint16_t unknown_a2;
  // Note: These are suffixed with 2 since they come from the same source data
  // as the corresponding fields in 9D/9E. (Even though serial_number and
  // serial_number2 have the same contents in 9E, they do not come from the same
  // field on the client's connection context object.)
  ptext<char, 0x10> serial_number2;
  ptext<char, 0x10> access_key2;
};

// 03 (S->C): Legacy password check result (non-BB)
// header.flag specifies if the password was correct. If header.flag is 0, the
// password saved to the memory card (if any) is deleted and the client is
// disconnected. If header.flag is nonzero, the client responds with an 04
// command. Curiously, it looks like even DCv1 doesn't use this command in its
// standard login sequence, so this may be a relic from very early development.
// No other arguments

// 03 (S->C): Start encryption (BB)
// Client will respond with an (encrypted) 93 command.
// All commands after this command will be encrypted with PSO BB encryption.
// The copyright field in the below structure must contain the following text:
// "Phantasy Star Online Blue Burst Game Server. Copyright 1999-2004 SONICTEAM."

struct S_ServerInitDefault_BB_03_9B {
  ptext<char, 0x60> copyright;
  parray<uint8_t, 0x30> server_key;
  parray<uint8_t, 0x30> client_key;
};

template <size_t AfterBytes>
struct S_ServerInitWithAfterMessage_BB_03_9B : S_ServerInitDefault_BB_03_9B {
  // As in 02, this field is not part of SEGA's implementation.
  ptext<char, AfterBytes> after_message;
};

// 04 (C->S): Legacy login
// See comments on non-BB 03 (S->C). This is likely a relic of an older,
// now-unused sequence. Like 03, this command isn't used by any PSO version that
// newserv supports.
// header.flag is nonzero, but it's not clear what it's used for.

struct C_LegacyLogin_PC_V3_04 {
  le_uint64_t unused1; // Same as unused field in 9D/9E
  le_uint32_t sub_version;
  uint8_t unknown_a1;
  uint8_t language; // Same as 9D/9E
  le_uint16_t unknown_a2;
  ptext<char, 0x10> serial_number;
  ptext<char, 0x10> access_key;
};

struct C_LegacyLogin_BB_04 {
  parray<le_uint32_t, 3> unknown_a1;
  ptext<char, 0x10> username;
  ptext<char, 0x10> password;
};

// 04 (S->C): Set guild card number and update client config ("security data")
// header.flag specifies an error code; the format described below is only used
// if this code is 0 (no error). Otherwise, the command has no arguments.
// Error codes (on GC):
//   01 = Line is busy (103)
//   02 = Already logged in (104)
//   03 = Incorrect password (106)
//   04 = Account suspended (107)
//   05 = Server down for maintenance (108)
//   06 = Incorrect password (127)
//   Any other nonzero value = Generic failure (101)
// The client config field in this command is ignored by pre-V3 clients as well
// as Episodes 1&2 Trial Edition. All other V3 clients save it as opaque data to
// be returned in a 9E or 9F command later. newserv sends the client config
// anyway to clients that ignore it.
// The client will respond with a 96 command, but only the first time it
// receives this command - for later 04 commands, the client will still update
// its client config but will not respond. Changing the security data at any
// time seems ok, but changing the guild card number of a client after it's
// initially set can confuse the client, and (on pre-V3 clients) possibly
// corrupt the character data. For this reason, newserv tries pretty hard to
// hide the remote guild card number when clients connect to the proxy server.
// BB clients have multiple client configs; this command sets the client config
// that is returned by the 9E and 9F commands, but does not affect the client
// config set by the E6 command (and returned in the 93 command). In most cases,
// E6 should be used for BB clients instead of 04.

template <typename ClientConfigT>
struct S_UpdateClientConfig {
  le_uint32_t player_tag;
  le_uint32_t guild_card_number;
  // The ClientConfig structure describes how newserv uses this command; other
  // servers do not use the same format for the following 0x20 or 0x28 bytes (or
  // may not use it at all). The cfg field is opaque to the client; it will send
  // back the contents verbatim in its next 9E command (or on request via 9F).
  ClientConfigT cfg;
};

struct S_UpdateClientConfig_DC_PC_V3_04 : S_UpdateClientConfig<ClientConfig> { };
struct S_UpdateClientConfig_BB_04 : S_UpdateClientConfig<ClientConfigBB> { };

// 05: Disconnect
// No arguments
// Sending this command to a client will cause it to disconnect. There's no
// advantage to doing this over simply closing the TCP connection. Clients will
// send this command to the server when they are about to disconnect, but the
// server does not need to close the connection when it receives this command
// (and in some cases, the client will send multiple 05 commands before actually
// disconnecting).

// 06: Chat
// Server->client format is same as 01 command. The maximum size of the message
// is 0x200 bytes.
// When sent by the client, the text field includes only the message. When sent
// by the server, the text field includes the origin player's name, followed by
// a tab character, followed by the message. During Episode 3 battles, chat
// messages can additionally be targeted to only your teammate; in this case,
// the message (at least, when seen by spectators) is of the form
// '<targetname>\tE\t<message>'. Messages sent to the entire battle group
// (including the opponents) are of the form '<targetname>\t@\t<message>'.
// Client->server format is very similar; we include a zero-length array in this
// struct to make parsing easier.

struct C_Chat_06 {
  parray<le_uint32_t, 2> unused;
  union {
    char dcv3[0];
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
struct S_MenuEntry_PC_BB_07_1F : S_MenuEntry<char16_t, 0x11> { };
struct S_MenuEntry_DC_V3_07_1F : S_MenuEntry<char, 0x12> { };

// 08 (C->S): Request game list
// No arguments

// 08 (S->C): Game list
// Client responds with 09 and 10 commands (or nothing if the player cancels).

// Command is a list of these; header.flag is the entry count. The first entry
// is not included in the count and does not appear on the client.
template <typename CharT>
struct S_GameMenuEntry {
  le_uint32_t menu_id;
  le_uint32_t game_id;
  uint8_t difficulty_tag; // 0x0A = Ep3; else difficulty + 0x22 (so 0x25 = Ult)
  uint8_t num_players;
  ptext<CharT, 0x10> name;
  // The episode field is used differently by different versions:
  // - On DCv1, PC, and GC Episode 3, the value is ignored.
  // - On DCv2, 1 means v1 players can't join the game, and 0 means they can.
  // - On GC Ep1&2, 0x40 means Episode 1, and 0x41 means Episode 2.
  // - On BB, 0x40/0x41 mean Episodes 1/2 as on GC, and 0x43 means Episode 4.
  uint8_t episode;
  uint8_t flags; // 02 = locked, 04 = disabled (BB), 10 = battle, 20 = challenge
};
struct S_GameMenuEntry_PC_BB_08 : S_GameMenuEntry<char16_t> { };
struct S_GameMenuEntry_DC_V3_08_Ep3_E6 : S_GameMenuEntry<char> { };

// 09 (C->S): Menu item info request
// Server will respond with an 11 command, or an A3 or A5 if the specified menu
// is the quest menu.

struct C_MenuItemInfoRequest_09 {
  le_uint32_t menu_id;
  le_uint32_t item_id;
};

// 0B: Invalid command

// 0C: Create game (DCv1)
// Same format as C1, but fields not supported by v1 (e.g. episode, v2 mode) are
// unused.

// 0D: Invalid command

// 0E (S->C): Unknown; possibly legacy join game (PC/V3)
// There is a failure mode in the command handlers on PC and V3 that causes the
// thread receiving the command to loop infinitely doing nothing, effectively
// softlocking the game.
// TODO: Check if this command exists on DC v1/v2.

struct S_Unknown_PC_0E {
  parray<uint8_t, 0x08> unknown_a1;
  parray<uint8_t, 0x18> unknown_a2[4];
  parray<uint8_t, 0x18> unknown_a3;
};

struct S_Unknown_GC_0E {
  PlayerLobbyDataDCGC lobby_data[4]; // This type is a guess
  struct UnknownA0 {
    uint8_t unknown_a1[2];
    le_uint16_t unknown_a2;
    le_uint32_t unknown_a3;
  };
  UnknownA0 unknown_a0[8];
  le_uint32_t unknown_a1;
  parray<uint8_t, 0x20> unknown_a2;
  uint8_t unknown_a3[4];
};

struct S_Unknown_XB_0E {
  parray<uint8_t, 0xE8> unknown_a1;
};

// 0F: Invalid command

// 10 (C->S): Menu selection
// header.flag contains two flags: 02 specifies if a password is present, and 01
// specifies... something else. These two bits directly correspond to the two
// lowest bits in the flags field of the game menu: 02 specifies that the game
// is locked, but the function of 01 is unknown.
// Annoyingly, the no-arguments form of the command can have any flag value, so
// it doesn't suffice to check the flag value to know which format is being
// used!

struct C_MenuSelection_10_Flag00 {
  le_uint32_t menu_id;
  le_uint32_t item_id;
};

template <typename CharT>
struct C_MenuSelection_10_Flag01 : C_MenuSelection_10_Flag00 {
  ptext<CharT, 0x10> unknown_a1;
};
struct C_MenuSelection_DC_V3_10_Flag01 : C_MenuSelection_10_Flag01<char> { };
struct C_MenuSelection_PC_BB_10_Flag01 : C_MenuSelection_10_Flag01<char16_t> { };

template <typename CharT>
struct C_MenuSelection_10_Flag02 : C_MenuSelection_10_Flag00 {
  ptext<CharT, 0x10> password;
};
struct C_MenuSelection_DC_V3_10_Flag02 : C_MenuSelection_10_Flag02<char> { };
struct C_MenuSelection_PC_BB_10_Flag02 : C_MenuSelection_10_Flag02<char16_t> { };

template <typename CharT>
struct C_MenuSelection_10_Flag03 : C_MenuSelection_10_Flag00 {
  ptext<CharT, 0x10> unknown_a1;
  ptext<CharT, 0x10> password;
};
struct C_MenuSelection_DC_V3_10_Flag03 : C_MenuSelection_10_Flag03<char> { };
struct C_MenuSelection_PC_BB_10_Flag03 : C_MenuSelection_10_Flag03<char16_t> { };

// 11 (S->C): Ship info
// Same format as 01 command.

// 12 (S->C): Valid but ignored (PC/V3/BB)
// TODO: Check if this command exists on DC v1/v2.

// 13 (S->C): Write online quest file
// Used for downloading online quests. For download quests (to be saved to the
// memory card), use A7 instead.
// All chunks except the last must have 0x400 data bytes. When downloading an
// online quest, the .bin and .dat chunks may be interleaved (although newserv
// currently sends them sequentially).

// header.flag = file chunk index (start offset / 0x400)
struct S_WriteFile_13_A7 {
  ptext<char, 0x10> filename;
  uint8_t data[0x400];
  le_uint32_t data_size;
};

// 13 (C->S): Confirm file write (V3/BB)
// Client sends this in response to each 13 sent by the server. It appears these
// are only sent by V3 and BB - PSO DC and PC do not send these.
// This structure is for documentation only; newserv ignores these.

// header.flag = file chunk index (same as in the 13/A7 sent by the server)
struct C_WriteFileConfirmation_V3_BB_13_A7 {
  ptext<char, 0x10> filename;
};

// 14 (S->C): Valid but ignored (PC/V3/BB)
// TODO: Check if this command exists on DC v1/v2.

// 15: Invalid command

// 16 (S->C): Valid but ignored (PC/V3/BB)
// TODO: Check if this command exists on DC v1/v2.

// 17 (S->C): Start encryption at login server (except on BB)
// Same format and usage as 02 command, but a different copyright string:
// "DreamCast Port Map. Copyright SEGA Enterprises. 1999"
// Unlike the 02 command, V3 clients will respond with a DB command when they
// receive a 17 command in any online session, with the exception of Episodes
// 1&2 trial edition (which responds with a 9A). DCv1 will respond with a 90.
// Other non-V3 clients will respond with a 9A or 9D.

// 18 (S->C): License verification result (PC/V3)
// Behaves exactly the same as 9A (S->C). No arguments except header.flag.
// TODO: Check if this command exists on DC v1/v2.

// 19 (S->C): Reconnect to different address
// Client will disconnect, and reconnect to the given address/port. Encryption
// will be disabled on the new connection; the server should send an appropriate
// command to enable it when the client connects.
// Note: PSO XB seems to ignore the address field, which makes sense given its
// networking architecture.

struct S_Reconnect_19 : S_Reconnect<le_uint16_t> { };

// Because PSO PC and some versions of PSO DC/GC use the same port but different
// protocols, we use a specially-crafted 19 command to send them to two
// different ports depending on the client version. I first saw this technique
// used by Schthack; I don't know if it was his original creation.

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

// 1A (S->C): Large message box
// On V3, client will sometimes respond with a D6 command (see D6 for more
// information).
// Contents are plain text (char on DC/V3, char16_t on PC/BB). There must be at
// least one null character ('\0') before the end of the command data.
// There is a bug in V3 (and possibly all versions) where if this command is
// sent after the client has joined a lobby, the chat log window contents will
// appear in the message box, prepended to the message text from the command.
// The maximum length of the message is 0x400 bytes. This is the only difference
// between this command and the D5 command.

// 1B (S->C): Valid but ignored (PC/V3)
// TODO: Check if this command exists on DC v1/v2.

// 1C (S->C): Valid but ignored (PC/V3)
// TODO: Check if this command exists on DC v1/v2.

// 1D: Ping
// No arguments
// When sent to the client, the client will respond with a 1D command. Data sent
// by the server is ignored; the client always sends a 1D command with no data.

// 1E: Invalid command

// 1F (C->S): Request information menu
// No arguments
// This command is used in PSO DC and PC. It exists in V3 as well but is
// apparently unused.

// 1F (S->C): Information menu
// Same format and usage as 07 command, except:
// - The menu title will say "Information" instead of "Ship Select".
// - There is no way to request details before selecting a menu item (the client
//   will not send 09 commands).
// - The player can press a button (B on GC, for example) to close the menu
//   without selecting anything, unlike the ship select menu. The client does
//   not send anything when this happens.

// 20: Invalid command

// 21: GameGuard control (old versions of BB)
// Format unknown

// 22: GameGuard check (BB)

// Command 0022 is a 16-byte challenge (sent in the data field) using the
// following structure.

struct SC_GameCardCheck_BB_0022 {
  parray<le_uint32_t, 4> data;
};

// Command 0122 uses a 4-byte challenge sent in the header.flag field instead.
// This version of the command has no other arguments.

// 23 (S->C): Unknown (BB)
// header.flag is used, but the command has no other arguments.

// 24 (S->C): Unknown (BB)

struct S_Unknown_BB_24 {
  le_uint16_t unknown_a1;
  le_uint16_t unknown_a2;
  parray<le_uint32_t, 8> values;
};

// 25 (S->C): Unknown (BB)

struct S_Unknown_BB_25 {
  le_uint16_t unknown_a1;
  uint8_t offset1;
  uint8_t value1;
  uint8_t offset2;
  uint8_t value2;
  le_uint16_t unused;
};

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

// 40 (C->S): Guild card search
// The server should respond with a 41 command if the target is online. If the
// target is not online, the server doesn't respond at all.

struct C_GuildCardSearch_40 {
  le_uint32_t player_tag;
  le_uint32_t searcher_guild_card_number;
  le_uint32_t target_guild_card_number;
};

// 41 (S->C): Guild card search result

template <typename CharT>
struct SC_MeetUserExtension {
  le_uint32_t menu_id;
  le_uint32_t lobby_id;
  parray<uint8_t, 0x3C> unknown_a1;
  ptext<CharT, 0x20> player_name;
};

template <typename HeaderT, typename CharT>
struct S_GuildCardSearchResult {
  le_uint32_t player_tag;
  le_uint32_t searcher_guild_card_number;
  le_uint32_t result_guild_card_number;
  HeaderT reconnect_command_header; // Ignored by the client
  S_Reconnect_19 reconnect_command;
  // The format of this string is "GAME-NAME,BLOCK##,SERVER-NAME". If the result
  // player is not in a game, GAME-NAME should be the lobby name - for standard
  // lobbies this is "BLOCK<blocknum>-<lobbynum>"; for CARD lobbies this is
  // "BLOCK<blocknum>-C<lobbynum>".
  ptext<CharT, 0x44> location_string;
  // If the player chooses to meet the user, this extension data is sent in the
  // login command (9D/9E) after connecting to the server designated in
  // reconnect_command. When processing the 9D/9E, newserv uses only the
  // lobby_id field within, but it fills in all fields when sengind a 41.
  SC_MeetUserExtension<CharT> extension;
};
struct S_GuildCardSearchResult_PC_41
    : S_GuildCardSearchResult<PSOCommandHeaderPC, char16_t> { };
struct S_GuildCardSearchResult_DC_V3_41
    : S_GuildCardSearchResult<PSOCommandHeaderDCV3, char> { };
struct S_GuildCardSearchResult_BB_41
    : S_GuildCardSearchResult<PSOCommandHeaderBB, char16_t> { };

// 42: Invalid command
// 43: Invalid command

// 44 (S->C): Open file for download
// Used for downloading online quests. For download quests (to be saved to the
// memory card), use A6 instead.
// Unlike the A6 command, the client will react to a 44 command only if the
// filename ends in .bin or .dat.

struct S_OpenFile_DC_44_A6 {
  ptext<char, 0x20> name; // Should begin with "PSO/"
  parray<uint8_t, 2> unused;
  uint8_t flags;
  ptext<char, 0x11> filename;
  le_uint32_t file_size;
};

struct S_OpenFile_PC_V3_44_A6 {
  ptext<char, 0x20> name; // Should begin with "PSO/"
  parray<uint8_t, 2> unused;
  le_uint16_t flags; // 0 = download quest, 2 = online quest, 3 = Episode 3
  ptext<char, 0x10> filename;
  le_uint32_t file_size;
};

// Curiously, PSO XB expects an extra 0x18 bytes at the end of this command, but
// those extra bytes are unused, and the client does not fail if they're
// omitted.
struct S_OpenFile_XB_44_A6 : S_OpenFile_PC_V3_44_A6 {
  parray<uint8_t, 0x18> unused2;
};

struct S_OpenFile_BB_44_A6 {
  parray<uint8_t, 0x22> unused;
  le_uint16_t flags;
  ptext<char, 0x10> filename;
  le_uint32_t file_size;
  ptext<char, 0x18> name;
};

// 44 (C->S): Confirm open file
// Client sends this in response to each 44 sent by the server.
// This structure is for documentation only; newserv ignores these.
// TODO: Is this command sent by DC/PC clients?

// header.flag = quest number (sort of - seems like the client just echoes
// whatever the server sent in its header.flag field. Also quest numbers can be
// > 0xFF so the flag is essentially meaningless)
struct C_OpenFileConfirmation_44_A6 {
  ptext<char, 0x10> filename;
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
// When a client sends this command, the server should forward it to all players
// in the same game/lobby, except the player who originally sent the command.
// See ReceiveSubcommands or the subcommand index below for details on contents.
// The data in this command may be up to 0x400 bytes in length. If it's larger,
// the client will exhibit undefined behavior.

// 61 (C->S): Player data
// See the PSOPlayerData structs in Player.hh for this command's format.
// header.flag specifies the format version, which is related to (but not
// identical to) the game's major version. For example, the format version is 01
// on DC v1, 02 on PSO PC, 03 on PSO GC, XB, and BB, and 04 on Ep3.
// Upon joining a game, the client assigns inventory item IDs sequentially as
// (0x00010000 + (0x00200000 * lobby_client_id) + x). So, for example, player
// 3's 8th item's ID would become 0x00610007. The item IDs from the last game
// the player was in will appear in their inventory in this command.
// Note: If the client is in a game at the time this command is received, the
// inventory sent by the client only includes items that would not disappear if
// the client crashes! Essentially, it reflects the saved state of the player's
// character rather than the live state.

// 62: Target command
// When a client sends this command, the server should forward it to the player
// identified by header.flag in the same game/lobby, even if that player is the
// player who originally sent it.
// See ReceiveSubcommands or the subcommand index below for details on contents.
// The data in this command may be up to 0x400 bytes in length. If it's larger,
// the client will exhibit undefined behavior.

// 63: Invalid command

// 64 (S->C): Join game
// This is sent to the joining player; the other players get a 65 instead.
// Note that (except on Episode 3) this command does not include the player's
// disp or inventory data. The clients in the game are responsible for sending
// that data to each other during the join process with 60/62/6C/6D commands.

// Header flag = entry count
template <typename LobbyDataT, typename DispDataT>
struct S_JoinGame {
  // Note: It seems Sega servers sent uninitialized memory in the variations
  // field when sending this command to start an Episode 3 tournament game. This
  // can be misleading when reading old logs from those days, but the Episode 3
  // client really does ignore it.
  parray<le_uint32_t, 0x20> variations;
  // Unlike lobby join commands, these are filled in in their slot positions.
  // That is, if there's only one player in a game with ID 2, then the first two
  // of these are blank and the player's data is in the third entry here.
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
  // Note: The 64 command for PSO DC ends here (the next 4 fields are ignored).
  // newserv sends them anyway for code simplicity reasons.
  uint8_t episode;
  // Similarly, PSO GC ignores the values in the following fields.
  uint8_t unused2; // Should be 1 for PSO PC?
  uint8_t solo_mode;
  uint8_t unused3;
};

struct S_JoinGame_PC_64 : S_JoinGame<PlayerLobbyDataPC, PlayerDispDataDCPCV3> { };
struct S_JoinGame_DC_GC_64 : S_JoinGame<PlayerLobbyDataDCGC, PlayerDispDataDCPCV3> { };

struct S_JoinGame_GC_Ep3_64 : S_JoinGame_DC_GC_64 {
  // This field is only present if the game (and client) is Episode 3. Similarly
  // to lobby_data in the base struct, all four of these are always present and
  // they are filled in in slot positions.
  struct {
    PlayerInventory inventory;
    PlayerDispDataDCPCV3 disp;
  } players_ep3[4];
};

struct S_JoinGame_XB_64 : S_JoinGame<PlayerLobbyDataXB, PlayerDispDataDCPCV3> {
  parray<le_uint32_t, 6> unknown_a1;
};

struct S_JoinGame_BB_64 : S_JoinGame<PlayerLobbyDataBB, PlayerDispDataBB> { };

// 65 (S->C): Add player to game
// When a player joins an existing game, the joining player receives a 64
// command (described above), and the players already in the game receive a 65
// command containing only the joining player's data.

// Header flag = entry count (always 1 for 65 and 68; up to 0x0C for 67)
template <typename LobbyDataT, typename DispDataT>
struct S_JoinLobby {
  uint8_t client_id;
  uint8_t leader_id;
  uint8_t disable_udp;
  uint8_t lobby_number;
  uint8_t block_number;
  uint8_t unknown_a1;
  uint8_t event;
  uint8_t unknown_a2;
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
struct S_JoinLobby_PC_65_67_68
    : S_JoinLobby<PlayerLobbyDataPC, PlayerDispDataDCPCV3> { };
struct S_JoinLobby_DC_GC_65_67_68_Ep3_EB
    : S_JoinLobby<PlayerLobbyDataDCGC, PlayerDispDataDCPCV3> { };
struct S_JoinLobby_BB_65_67_68
    : S_JoinLobby<PlayerLobbyDataBB, PlayerDispDataBB> { };

struct S_JoinLobby_XB_65_67_68 {
  uint8_t client_id;
  uint8_t leader_id;
  uint8_t disable_udp;
  uint8_t lobby_number;
  uint8_t block_number;
  uint8_t unknown_a1;
  uint8_t event;
  uint8_t unknown_a2;
  parray<uint8_t, 4> unknown_a3;
  parray<le_uint32_t, 6> unknown_a4;
  struct Entry {
    PlayerLobbyDataXB lobby_data;
    PlayerInventory inventory;
    PlayerDispDataDCPCV3 disp;
  };
  // Note: not all of these will be filled in and sent if the lobby isn't full
  // (the command size will be shorter than this struct's size)
  Entry entries[0x0C];

  static inline size_t size(size_t used_entries) {
    return offsetof(S_JoinLobby_XB_65_67_68, entries)
        + used_entries * sizeof(Entry);
  }
};

// 66 (S->C): Remove player from game
// This is sent to all players in a game except the leaving player.

// Header flag = leaving player ID (same as client_id);
struct S_LeaveLobby_66_69_Ep3_E9 {
  uint8_t client_id;
  uint8_t leader_id;
  // Note: disable_udp only has an effect for games; it is unused for lobbies
  // and spectator teams.
  uint8_t disable_udp;
  uint8_t unused;
};

// 67 (S->C): Join lobby
// This is sent to the joining player; the other players receive a 68 instead.
// Same format as 65 command, but used for lobbies instead of games.

// 68 (S->C): Add player to lobby
// Same format as 65 command, but used for lobbies instead of games.
// The command only includes the joining player's data.

// 69 (S->C): Remove player from lobby
// Same format as 66 command, but used for lobbies instead of games.

// 6A: Invalid command
// 6B: Invalid command

// 6C: Broadcast command
// Same format and usage as 60 command, but with no size limit.

// 6D: Target command
// Same format and usage as 62 command, but with no size limit.

// 6E: Invalid command

// 6F (C->S): Set game status
// This command is sent when a player is done loading and other players can then
// join the game. On BB, this command is sent as 016F if a quest is in progress
// and the game should not be joined by anyone else.

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

// 80 (S->C): Ignored (PC/V3)
// TODO: Check if this command exists on DC v1/v2.

struct S_Unknown_PC_V3_80 {
  le_uint32_t which; // Expected to be in the range 00-0B... maybe client ID?
  le_uint32_t unknown_a1; // Could be player_tag
  le_uint32_t unknown_a2; // Could be guild_card_number
};

// 81: Simple mail
// Format is the same in both directions. The server should forward the command
// to the player with to_guild_card_number, if they are online. If they are not
// online, the server may store it for later delivery, send their auto-reply
// message back to the original sender, or simply drop the message.
// On GC (and probably other versions too) the unused space after the text
// contains uninitialized memory when the client sends this command. newserv
// clears the uninitialized data for security reasons before forwarding.

template <typename CharT>
struct SC_SimpleMail_81 {
  le_uint32_t player_tag;
  le_uint32_t from_guild_card_number;
  ptext<CharT, 0x10> from_name;
  le_uint32_t to_guild_card_number;
  ptext<CharT, 0x200> text;
};

struct SC_SimpleMail_PC_81 : SC_SimpleMail_81<char16_t> { };
struct SC_SimpleMail_DC_V3_81 : SC_SimpleMail_81<char> { };

struct SC_SimpleMail_BB_81 {
  le_uint32_t player_tag;
  le_uint32_t from_guild_card_number;
  ptext<char16_t, 0x10> from_name;
  le_uint32_t to_guild_card_number;
  ptext<char16_t, 0x14> received_date;
  ptext<char16_t, 0x200> text;
};

// 82: Invalid command

// 83 (S->C): Lobby menu
// This sets the menu item IDs that the client uses for the lobby teleport menu.
// The client expects 15 items here; sending more or fewer items does not change
// the lobby count on the client. If fewer entries are sent, the menu item IDs
// for some lobbies will not be set, and the client will likely send 84 commands
// that don't make sense if the player chooses one of lobbies with unset IDs.
// On Episode 3, the client expects 20 entries instead of 15. The CARD lobbies
// are the last five entries, even though they appear at the top of the list on
// the player's screen.

// Command is a list of these; header.flag is the entry count (15 or 20)
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

// 88 (C->S): License check (DC NTE only)
// The server should respond with an 88 command.

struct C_Login_DCNTE_88 {
  ptext<char, 0x11> serial_number;
  ptext<char, 0x11> access_key;
};

// 88 (S->C): License check result (DC NTE only)
// No arguemnts except header.flag.
// If header.flag is zero, client will respond with an 8A command. Otherwise, it
// will respond with an 8B command.

// 88 (S->C): Update lobby arrows
// If this command is sent while a client is joining a lobby, the client may
// ignore it. For this reason, the server should wait a few seconds after a
// client joins a lobby before sending an 88 command.
// This command is not supported on DC v1.

// Command is a list of these; header.flag is the entry count. There should
// be an update for every player in the lobby in this command, even if their
// arrow color isn't being changed.
struct S_ArrowUpdateEntry_88 {
  le_uint32_t player_tag;
  le_uint32_t guild_card_number;
  le_uint32_t arrow_color;
};

// The arrow color values are:
// any number not specified below (including 00) - none
// 01 - red
// 02 - blue
// 03 - green
// 04 - yellow
// 05 - purple
// 06 - cyan
// 07 - orange
// 08 - pink
// 09 - white
// 0A - white
// 0B - white
// 0C - black

// 89 (C->S): Set lobby arrow
// header.flag = arrow color number (see above); no other arguments.
// Server should send an 88 command to all players in the lobby.

// 8A (C->S): Connection information (DC NTE only)
// The server should respond with an 8A command.

struct C_ConnectionInfo_DCNTE_8A {
  ptext<char, 0x08> hardware_id;
  le_uint32_t sub_version; // 0x20
  le_uint32_t unknown_a1;
  ptext<char, 0x30> username;
  ptext<char, 0x30> password;
  ptext<char, 0x30> email_address; // From Sylverant documentation
};

// 8A (S->C): Connection information result (DC NTE only)
// header.flag is a success flag. If 0 is sent, the client shows an error
// message and disconnects. Otherwise, the client responds with an 8B command.

// 8A (C->S): Request lobby/game name (except DC NTE)
// No arguments

// 8A (S->C): Lobby/game name (except DC NTE)
// Contents is a string (char16_t on PC/BB, char on DC/V3) containing the lobby
// or game name. The client generally only sends this immediately after joining
// a game, but Sega's servers also replied to it if it was sent in a lobby. They
// would return a string like "LOBBY01" even though this would never be used
// under normal circumstances.

// 8B: Log in (DC NTE only)

struct C_Login_DCNTE_8B {
  le_uint32_t player_tag;
  le_uint32_t guild_card_number;
  parray<uint8_t, 0x08> hardware_id;
  le_uint32_t sub_version;
  uint8_t is_extended;
  uint8_t language;
  parray<uint8_t, 2> unused1;
  ptext<char, 0x11> serial_number;
  ptext<char, 0x11> access_key;
  ptext<char, 0x30> username;
  ptext<char, 0x30> password;
  ptext<char, 0x10> name;
  parray<uint8_t, 2> unused;
  SC_MeetUserExtension<char> extension;
};

// 8C: Invalid command
// 8D: Invalid command
// 8E: Invalid command
// 8F: Invalid command

// 90 (C->S): V1 login (DC/PC/V3)
// This command is used during the DCv1 login sequence; a DCv1 client will
// respond to a 17 command with an (encrypted) 90. If a V3 client receives a 91
// command, however, it will also send a 90 in response, though the contents
// will be blank (all zeroes).

struct C_LoginV1_DC_PC_V3_90 {
  ptext<char, 0x11> serial_number;
  ptext<char, 0x11> access_key;
  parray<uint8_t, 2> unused;
};

// 90 (S->C): License verification result (V3)
// Behaves exactly the same as 9A (S->C). No arguments except header.flag.

// 91 (S->C): Start encryption at login server (legacy; non-BB only)
// Same format and usage as 17 command, except the client will respond with a 90
// command. On versions that support it, this is strictly less useful than the
// 17 command.

// 92 (C->S): Register (DC)

struct C_RegisterV1_DC_92 {
  parray<uint8_t, 0x0C> unknown_a1;
  uint8_t unknown_a2;
  uint8_t language; // TODO: This is a guess; verify it
  uint8_t unknown_a3[2];
  ptext<char, 0x10> hardware_id;
  parray<uint8_t, 0x50> unused1;
  ptext<char, 0x20> email; // According to Sylverant documentation
  parray<uint8_t, 0x10> unused2;
};

// 92 (S->C): Register result (non-BB)
// Same format and usage as 9C (S->C) command.

// 93 (C->S): Log in (DCv1)

struct C_LoginV1_DC_93 {
  le_uint32_t player_tag;
  le_uint32_t guild_card_number;
  le_uint32_t unknown_a1;
  le_uint32_t unknown_a2;
  le_uint32_t sub_version;
  uint8_t is_extended;
  uint8_t language;
  parray<uint8_t, 2> unused1;
  ptext<char, 0x11> serial_number;
  ptext<char, 0x11> access_key;
  // Note: The hardware_id field is likely shorter than this (only 8 bytes
  // appear to actually be used).
  ptext<char, 0x60> hardware_id;
  ptext<char, 0x10> name;
  parray<uint8_t, 2> unused2;
};

struct C_LoginExtendedV1_DC_93 : C_LoginV1_DC_93 {
  SC_MeetUserExtension<char> extension;
};

// 93 (C->S): Log in (BB)

struct C_Login_BB_93 {
  le_uint32_t player_tag;
  le_uint32_t guild_card_number;
  ptext<char, 0x08> unused;
  le_uint32_t team_id;
  ptext<char, 0x30> username;
  ptext<char, 0x30> password;

  // These fields map to the same fields in SC_MeetUserExtension. There is no
  // equivalent of the name field from that structure on BB (though newserv
  // doesn't use it anyway).
  le_uint32_t menu_id;
  le_uint32_t preferred_lobby_id;

  // Note: Unlike other versions, BB puts the version string in the client
  // config at connect time. So the first time the server gets this command, it
  // will be something like "Ver. 1.24.3". Note also that some old versions
  // (before 1.23.8?) omit the hardware_info field before the client config, so
  // the client config starts 8 bytes earlier on those versions and the entire
  // command is 8 bytes shorter, hence this odd-looking union.
  union VariableLengthSection {
    union ClientConfigFields {
      ClientConfigBB cfg;
      ptext<char, 0x28> version_string;
      le_uint32_t as_u32[10];
    };

    ClientConfigFields old_clients_cfg;
    struct NewFormat {
      le_uint32_t hardware_info[2];
      ClientConfigFields cfg;
    } new_clients;
  } var;
};

// 94: Invalid command

// 95 (S->C): Request player data
// No arguments
// For some reason, some servers send high values in the header.flag field here.
// From what I can tell, that field appears to be completely unused by the
// client - sending zero works just fine. The original Sega servers had some
// uninitialized memory bugs, of which that may have been one, and other private
// servers may have just duplicated Sega's behavior verbatim.
// Client will respond with a 61 command.

// 96 (C->S): Character save information
// TODO: Check if this command exists on DC v1/v2.

struct C_CharSaveInfo_V3_BB_96 {
  // This field appears to be a checksum or random stamp of some sort; it seems
  // to be unique and constant per character.
  le_uint32_t unknown_a1;
  // This field counts certain events on a per-character basis. One of the
  // relevant events is the act of sending a 96 command; another is the act of
  // receiving a 97 command (to which the client responds with a B1 command).
  // Presumably Sega's original implementation could keep track of this value
  // for each character and could therefore tell if a character had connected to
  // an unofficial server between connections to Sega's servers.
  le_uint32_t event_counter;
};

// 97 (S->C): Save to memory card
// No arguments
// Sending this command with header.flag == 0 will show a message saying that
// "character data was improperly saved", and will delete the character's items
// and challenge mode records. newserv (and all other unofficial servers) always
// send this command with flag == 1, which causes the client to save normally.
// Client will respond with a B1 command if header.flag is nonzero.

// 98 (C->S): Leave game
// Same format as 61 command.
// Client will send an 84 when it's ready to join a lobby.

// 99 (C->S): Server time accepted
// No arguments

// 9A (C->S): Initial login (no password or client config)
// Not used on DCv1 - that version uses 90 instead.

struct C_Login_DC_PC_V3_9A {
  ptext<char, 0x10> v1_serial_number;
  ptext<char, 0x10> v1_access_key;
  ptext<char, 0x10> serial_number;
  ptext<char, 0x10> access_key;
  le_uint32_t player_tag;
  le_uint32_t guild_card_number;
  le_uint32_t sub_version;
  ptext<char, 0x30> serial_number2; // On DCv2, this is the hardware ID
  ptext<char, 0x30> access_key2;
  ptext<char, 0x30> email_address;
};

// 9A (S->C): License verification result
// The result code is sent in the header.flag field. Result codes:
// 00 = license ok (don't save to memory card; client responds with 9D/9E)
// 01 = registration required (client responds with a 9C command)
// 02 = license ok (save to memory card; client responds with 9D/9E)
// 03 = access key invalid (125)
// 04 = serial number invalid (126)
// 07 = invalid Hunter's License (117)
// 08 = Hunter's License expired (116)
// 0B = HL not registered under this serial number/access key (112)
// 0C = HL not registered under this serial number/access key (113)
// 0D = HL not registered under this serial number/access key (114)
// 0E = connection error (115)
// 0F = connection suspended (111)
// 10 = connection suspended (111)
// 11 = Hunter's License expired (116)
// 12 = invalid Hunter's License (117)
// 13 = servers under maintenance (118)
// Seems like most (all?) of the rest of the codes are "network error" (119).

// 9B (S->C): Secondary server init (non-BB)
// Behaves exactly the same as 17 (S->C).
// TODO: Check if this command exists on DC v1/v2.

// 9B (S->C): Secondary server init (BB)
// Format is the same as 03 (and the client uses the same encryption afterward).
// The only differences that 9B has from 03:
// - 9B does not work during the data-server phase (before the client has
//   reached the ship select menu), whereas 03 does.
// - For command 9B, the copyright string must be
//   "PSO NEW PM Server. Copyright 1999-2002 SONICTEAM.".
// - The client will respond with a command DB instead of a command 93.

// 9C (C->S): Register
// It appears PSO GC sends uninitialized data in the header.flag field here.

struct C_Register_DC_PC_V3_9C {
  le_uint64_t unused;
  le_uint32_t sub_version;
  uint8_t unused1;
  uint8_t language;
  uint8_t unused2[2];
  ptext<char, 0x30> serial_number; // On XB, this is the XBL gamertag
  ptext<char, 0x30> access_key; // On XB, this is the XBL user ID
  ptext<char, 0x30> password; // On XB, this contains "xbox-pso"
};

struct C_Register_BB_9C {
  le_uint32_t sub_version;
  uint8_t unused1;
  uint8_t language;
  uint8_t unused2[2];
  ptext<char, 0x30> username;
  ptext<char, 0x30> password;
  ptext<char, 0x30> game_tag; // "psopc2" on BB
};

// 9C (S->C): Register result
// On GC, the only possible error here seems to be wrong password (127) which is
// displayed if the header.flag field is zero. On DCv2/PC, the error text says
// something like "registration failed" instead. If header.flag is nonzero, the
// client proceeds with the login procedure by sending a 9D or 9E.

// 9D (C->S): Log in without client config (DCv2/PC/GC)
// Not used on DCv1 - that version uses 93 instead.
// Not used on most versions of V3 - the client sends 9E instead. The one
// type of PSO V3 that uses 9D is the Trial Edition of Episodes 1&2.
// The extended version of this command is sent if the client has not yet
// received an 04 (in which case the extended fields are blank) or if the client
// selected the Meet User option, in which case it specifies the requested lobby
// by its menu ID and item ID.

struct C_Login_DC_PC_GC_9D {
  le_uint32_t player_tag; // 0x00010000 if guild card is set (via 04)
  le_uint32_t guild_card_number; // 0xFFFFFFFF if not set
  le_uint64_t unused;
  le_uint32_t sub_version;
  uint8_t is_extended; // If 1, structure has extended format
  uint8_t language; // 0 = JP, 1 = EN, 2 = DE (?), 3 = FR (?), 4 = ES
  parray<uint8_t, 0x2> unused3; // Always zeroes?
  ptext<char, 0x10> v1_serial_number;
  ptext<char, 0x10> v1_access_key;
  ptext<char, 0x10> serial_number; // On XB, this is the XBL gamertag
  ptext<char, 0x10> access_key; // On XB, this is the XBL user ID
  ptext<char, 0x30> serial_number2; // On XB, this is the XBL gamertag
  ptext<char, 0x30> access_key2; // On XB, this is the XBL user ID
  ptext<char, 0x10> name;
};
struct C_LoginExtended_DC_GC_9D : C_Login_DC_PC_GC_9D {
  SC_MeetUserExtension<char> extension;
};
struct C_LoginExtended_PC_9D : C_Login_DC_PC_GC_9D {
  SC_MeetUserExtension<char16_t> extension;
};

// 9E (C->S): Log in with client config (V3/BB)
// Not used on GC Episodes 1&2 Trial Edition.
// The extended version of this command is used in the same circumstances as
// when PSO PC uses the extended version of the 9D command.
// header.flag is 1 if the client has UDP disabled.

struct C_Login_GC_9E : C_Login_DC_PC_GC_9D {
  union ClientConfigFields {
    ClientConfig cfg;
    parray<uint8_t, 0x20> data;
    ClientConfigFields() : data() { }
  } client_config;
};
struct C_LoginExtended_GC_9E : C_Login_GC_9E {
  SC_MeetUserExtension<char> extension;
};

struct C_Login_XB_9E : C_Login_GC_9E {
  XBNetworkLocation netloc;
  parray<le_uint32_t, 6> unknown_a1;
};
struct C_LoginExtended_XB_9E : C_Login_XB_9E {
  SC_MeetUserExtension<char> extension;
};

struct C_LoginExtended_BB_9E {
  le_uint32_t player_tag;
  le_uint32_t guild_card_number; // == serial_number when on newserv
  le_uint32_t sub_version;
  le_uint32_t unknown_a1;
  le_uint32_t unknown_a2;
  ptext<char, 0x10> unknown_a3; // Always blank?
  ptext<char, 0x10> unknown_a4; // == "?"
  ptext<char, 0x10> unknown_a5; // Always blank?
  ptext<char, 0x10> unknown_a6; // Always blank?
  ptext<char, 0x30> username;
  ptext<char, 0x30> password;
  ptext<char, 0x10> guild_card_number_str;
  parray<le_uint32_t, 10> unknown_a7;
  SC_MeetUserExtension<char16_t> extension;
};

// 9F (S->C): Request client config / security data (V3/BB)
// This command is not valid on PSO GC Episodes 1&2 Trial Edition, nor any
// pre-V3 PSO versions.
// No arguments

// 9F (C->S): Client config / security data response (V3/BB)
// The data is opaque to the client, as described at the top of this file.
// If newserv ever sent a 9F command (it currently does not), the response
// format here would be ClientConfig (0x20 bytes) on V3, or ClientConfigBB (0x28
// bytes) on BB. However, on BB, this returns the client config that was set by
// a preceding 04 command, not the config set by a preceding E6 command.

// A0 (C->S): Change ship
// This structure is for documentation only; newserv ignores the arguments here.
// TODO: This structore is valid for GC clients; check if this command has the
// same arguments on DC/PC.

struct C_ChangeShipOrBlock_A0_A1 {
  le_uint32_t player_tag;
  le_uint32_t guild_card_number;
  parray<uint8_t, 0x10> unused;
};

// A0 (S->C): Ship select menu
// Same as 07 command.

// A1 (C->S): Change block
// Same format as A0. Like with A0, newserv ignores the arguments.

// A1 (S->C): Block select menu
// Same as 07 command.

// A2 (C->S): Request quest menu
// No arguments

// A2 (S->C): Quest menu
// Client will respond with an 09, 10, or A9 command. For 09, the server should
// send the category or quest description via an A3 response; for 10, the server
// should send another quest menu (if a category was chosen), or send the quest
// data with 44/13 commands; for A9, the server does not need to respond.

template <typename CharT, size_t ShortDescLength>
struct S_QuestMenuEntry {
  le_uint32_t menu_id;
  le_uint32_t item_id;
  ptext<CharT, 0x20> name;
  ptext<CharT, ShortDescLength> short_description;
};
struct S_QuestMenuEntry_PC_A2_A4 : S_QuestMenuEntry<char16_t, 0x70> { };
struct S_QuestMenuEntry_DC_GC_A2_A4 : S_QuestMenuEntry<char, 0x70> { };
struct S_QuestMenuEntry_XB_A2_A4 : S_QuestMenuEntry<char, 0x80> { };
struct S_QuestMenuEntry_BB_A2_A4 : S_QuestMenuEntry<char16_t, 0x7A> { };

// A3 (S->C): Quest information
// Same format as 1A/D5 command (plain text)

// A4 (S->C): Download quest menu
// Same format as A2, but can be used when not in a game. The client responds
// similarly as for command A2 with the following differences:
// - Descriptions should be sent with the A5 command instead of A3.
// - If a quest is chosen, it should be sent with A6/A7 commands rather than
//   44/13, and it must be in a different encrypted format. The download quest
//   format is documented in create_download_quest_file in Quest.cc.
// - After the download is done, or if the player cancels the menu, the client
//   sends an A0 command instead of A9.

// A5 (S->C): Download quest information
// Same format as 1A/D5 command (plain text)

// A6: Open file for download
// Same format as 44.
// Used for download quests and GBA games. The client will react to this command
// if the filename ends in .bin/.dat (download quests), .gba (GameBoy Advance
// games), and, curiously, .pvr (textures). To my knowledge, the .pvr handler in
// this command has never been used.
// It also appears that the .gba handler was not deleted in PSO XB, even though
// it doesn't make sense for an XB client to receive such a file.
// For .bin files, the flags field should be zero. For .pvr files, the flags
// field should be 1. For .dat and .gba files, it seems the value in the flags
// field does not matter.

// A7: Write download file
// Same format as 13.

// A8: Invalid command

// A9 (C->S): Quest menu closed (canceled)
// No arguments
// This command is sent when the in-game quest menu (A2) is closed. When the
// download quest menu is closed, either by downloading a quest or canceling,
// the client sends A0 instead. The existance of the A0 response on the download
// case makes sense, because the client may not be in a lobby and the server may
// need to send another menu or redirect the client. But for the online quest
// menu, the client is already in a game and can move normally after canceling
// the quest menu, so it's not obvious why A9 is needed at all. newserv (and
// probably all other private servers) ignores it.
// Curiously, PSO GC sends uninitialized data in the flag argument.

// AA (C->S): Update quest statistics (V3/BB)
// This command is used in Maximum Attack 2, but its format is unlikely to be
// specific to that quest. The structure here represents the only instance I've
// seen so far.
// The server will respond with an AB command.
// This command is likely never sent by PSO GC Episodes 1&2 Trial Edition,
// because the following command (AB) is definitely not valid on that version.

struct C_UpdateQuestStatistics_V3_BB_AA {
  le_uint16_t quest_internal_id;
  le_uint16_t unused;
  le_uint16_t request_token;
  le_uint16_t unknown_a1;
  le_uint32_t unknown_a2;
  le_uint32_t kill_count;
  le_uint32_t time_taken; // in seconds
  parray<le_uint32_t, 5> unknown_a3;
};

// AB (S->C): Confirm update quest statistics (V3/BB)
// This command is not valid on PSO GC Episodes 1&2 Trial Edition.
// TODO: Does this command have a different meaning in Episode 3? Is it used at
// all there, or is the handler an undeleted vestige from Episodes 1&2?

struct S_ConfirmUpdateQuestStatistics_V3_BB_AB {
  le_uint16_t unknown_a1; // 0
  be_uint16_t unknown_a2; // Probably actually unused
  le_uint16_t request_token; // Should match token sent in AA command
  le_uint16_t unknown_a3; // Schtserv always sends 0xBFFF here
};

// AC: Quest barrier (V3/BB)
// No arguments; header.flag must be 0 (or else the client disconnects)
// After a quest begins loading in a game (the server sends 44/13 commands to
// each player with the quest's data), each player will send an AC to the server
// when it has parsed the quest and is ready to start. When all players in a
// game have sent an AC to the server, the server should send them all an AC,
// which starts the quest for all players at (approximately) the same time.
// Sending this command to a GC client when it is not waiting to start a quest
// will cause it to crash.
// This command is not valid on PSO GC Episodes 1&2 Trial Edition.

// AD: Invalid command
// AE: Invalid command
// AF: Invalid command

// B0 (S->C): Text message
// Same format as 01 command.
// The message appears as an overlay on the right side of the screen. The player
// doesn't do anything to dismiss it; it will disappear after a few seconds.
// TODO: Check if this command exists on DC v1/v2.

// B1 (C->S): Request server time
// No arguments
// Server will respond with a B1 command.

// B1 (S->C): Server time
// Contents is a string like "%Y:%m:%d: %H:%M:%S.000" (the space is not a typo).
// For example: 2022:03:30: 15:36:42.000
// This command can be sent even if it's not requested by the client (with B1).
// For example, some servers send this every time a client joins a game.
// Client will respond with a 99 command.

// B2 (S->C): Execute code and/or checksum memory
// Client will respond with a B3 command with the same header.flag value as was
// sent in the B2.
// On PSO PC, the code section (if included in the B2 command) is parsed and
// relocated, but is not actually executed, so the return_value field in the
// resulting B3 command is always 0. The checksum functionality does work on PSO
// PC, just like the other versions.
// This command doesn't work on some PSO GC versions, namely the later JP PSO
// Plus (v1.05), US PSO Plus (v1.02), or US/EU Episode 3. Sega presumably
// removed it after taking heat from Nintendo about enabling homebrew on the
// GameCube. On the earlier JP PSO Plus (v1.04) and JP Episode 3, this command
// is implemented as described here, with some additional compression and
// encryption steps added, similarly to how download quests are encoded. See
// send_function_call in SendCommands.cc for more details on how this works.

struct S_ExecuteCode_B2 {
  // If code_size == 0, no code is executed, but checksumming may still occur.
  // In that case, this structure is the entire body of the command (no footer
  // is sent).
  le_uint32_t code_size; // Size of code (following this struct) and footer
  le_uint32_t checksum_start; // May be null if size is zero
  le_uint32_t checksum_size; // If zero, no checksum is computed
  // The code immediately follows, ending with an S_ExecuteCode_Footer_B2
};

template <typename LongT>
struct S_ExecuteCode_Footer_B2 {
  // Relocations is a list of words (le_uint16_t on DC/PC/XB/BB, be_uint16_t on
  // GC) containing the number of doublewords (uint32_t) to skip for each
  // relocation. The relocation pointer starts immediately after the
  // checksum_size field in the header, and advances by the value of one
  // relocation word (times 4) before each relocation. At each relocated
  // doubleword, the address of the first byte of the code (after checksum_size)
  // is added to the existing value.
  // For example, if the code segment contains the following data (where R
  // specifies doublewords to relocate):
  //   RR RR RR RR ?? ?? ?? ?? ?? ?? ?? ?? RR RR RR RR
  //   RR RR RR RR ?? ?? ?? ?? RR RR RR RR
  // then the relocation words should be 0000, 0003, 0001, and 0002.
  // If there is a small number of relocations, they may be placed in the unused
  // fields of this structure to save space and/or confuse reverse engineers.
  // The game never accesses the last 12 bytes of this structure unless
  // relocations_offset points there, so those 12 bytes may also be omitted from
  // the command entirely (without changing code_size - so code_size would
  // technically extend beyond the end of the B2 command).
  LongT relocations_offset; // Relative to code base (after checksum_size)
  LongT num_relocations;
  parray<LongT, 2> unused1;
  // entrypoint_offset is doubly indirect - it points to a pointer to a 32-bit
  // value that itself is the actual entrypoint. This is presumably done so the
  // entrypoint can be optionally relocated.
  LongT entrypoint_addr_offset; // Relative to code base (after checksum_size).
  parray<LongT, 3> unused2;
};

struct S_ExecuteCode_Footer_GC_B2 : S_ExecuteCode_Footer_B2<be_uint32_t> { };
struct S_ExecuteCode_Footer_DC_PC_XB_BB_B2
    : S_ExecuteCode_Footer_B2<le_uint32_t> { };

// B3 (C->S): Execute code and/or checksum memory result
// Not used on versions that don't support the B2 command (see above).

struct C_ExecuteCodeResult_B3 {
  // On DC, return_value has the value in r0 when the function returns.
  // On PC, return_value is always 0.
  // On GC, return_value has the value in r3 when the function returns.
  // On XB and BB, return_value has the value in eax when the function returns.
  // If code_size was 0 in the B2 command, return_value is always 0.
  le_uint32_t return_value;
  le_uint32_t checksum; // 0 if no checksum was computed
};

// B4: Invalid command
// B5: Invalid command
// B6: Invalid command

// B7 (S->C): Rank update (Episode 3)

struct S_RankUpdate_GC_Ep3_B7 {
  le_uint32_t rank;
  ptext<char, 0x0C> rank_text;
  le_uint32_t meseta;
  le_uint32_t max_meseta;
  le_uint32_t jukebox_songs_unlocked;
};

// B7 (C->S): Confirm rank update (Episode 3)
// No arguments
// The client sends this after it receives a B7 from the server.

// B8 (S->C): Update card definitions (Episode 3)
// Contents is a single little-endian le_uint32_t specifying the size of the
// (PRS-compressed) data, followed immediately by the data.
// Note: PSO BB accepts this command as well, but ignores it.

// B8 (C->S): Confirm updated card definitions (Episode 3)
// No arguments
// The client sends this after it receives a B8 from the server.

// B9 (S->C): Update media (Episode 3)
// This command is not valid on Episode 3 Trial Edition.

struct S_UpdateMediaHeader_GC_Ep3_B9 {
  // Valid values for the type field:
  // 1: GVM file
  // 2: Unknown; probably BML file
  // 3: Unknown; probably BML file
  // 4: Unknown; appears to be completely ignored
  // Any other value: entire command is ignored
  // For types 2 and 3, the game looks for various tokens in the decompressed
  // data; specifically '****', 'GCAM', 'GJBM', 'GJTL', 'GLIM', 'GMDM', 'GSSM',
  // 'NCAM', 'NJBM', 'NJCA', 'NLIM', 'NMDM', and 'NSSM'. Of these, 'GJTL',
  // 'NMDM', and 'NSSM' are found in some of the game's existing BML files, but
  // the others don't seem to be anywhere on the disc. 'NJBM' is found in
  // psohistory_e.sfd, but not in any other files.
  le_uint32_t type;
  // Valid values for the type field (at least, when type is 1):
  // 0: Unknown
  // 1: Set lobby banner 1 (in front of where player 0 enters)
  // 2: Set lobby banner 2 (left of banner 1)
  // 3: Unknown
  // 4: Set lobby banner 3 (left of banner 2; opposite where player 0 enters)
  // 5: Unknown
  // 6: Unknown
  // Any other value: entire command is ignored
  le_uint32_t which;
  le_uint16_t size;
  le_uint16_t unused;
  // The PRS-compressed data immediately follows this header. The maximum size
  // of the compressed data is 0x3800 bytes, and the data must decompress to
  // fewer than 0x37000 bytes of output. The size field above contains the
  // compressed size of this data (the decompressed size is not included
  // anywhere in the command).
};

// B9 (C->S): Confirm received B9 (Episode 3)
// No arguments
// This command is not valid on Episode 3 Trial Edition.

// BA: Meseta transaction (Episode 3)
// This command is not valid on Episode 3 Trial Edition.

struct C_Meseta_GC_Ep3_BA {
  le_uint32_t transaction_num;
  le_uint32_t value;
  le_uint32_t request_token;
};

struct S_Meseta_GC_Ep3_BA {
  le_uint32_t remaining_meseta;
  le_uint32_t total_meseta_awarded;
  le_uint32_t request_token; // Should match the token sent by the client
};

// BB (S->C): Unknown (Episode 3)
// header.flag is used, but it's not clear for what. It may be the number of
// valid entries, similarly to how command 07 is implemented.
// This command is not valid on Episode 3 Trial Edition.

struct S_Unknown_GC_Ep3_BB {
  struct Entry {
    uint8_t unknown_a1[0x20];
    le_uint16_t unknown_a2;
    le_uint16_t unknown_a3;
  };
  // The first entry here is probably fake, like for ship select menus (07)
  Entry entries[0x21];
  uint8_t unknown_a3[0x900];
};

// BC: Invalid command
// BD: Invalid command
// BE: Invalid command
// BF: Invalid command

// C0 (C->S): Request choice search options
// No arguments
// Server should respond with a C0 command (described below).

// C0 (S->C): Choice search options

// Command is a list of these; header.flag is the entry count (incl. top-level).
template <typename ItemIDT, typename CharT>
struct S_ChoiceSearchEntry {
  // Category IDs are nonzero; if the high byte of the ID is nonzero then the
  // category can be set by the user at any time; otherwise it can't.
  ItemIDT parent_category_id; // 0 for top-level categories
  ItemIDT category_id;
  ptext<CharT, 0x1C> text;
};
struct S_ChoiceSearchEntry_DC_C0 : S_ChoiceSearchEntry<le_uint32_t, char> { };
struct S_ChoiceSearchEntry_V3_C0 : S_ChoiceSearchEntry<le_uint16_t, char> { };
struct S_ChoiceSearchEntry_PC_BB_C0 : S_ChoiceSearchEntry<le_uint16_t, char16_t> { };

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
  parray<le_uint32_t, 2> unused;
  ptext<CharT, 0x10> name;
  ptext<CharT, 0x10> password;
  uint8_t difficulty; // 0-3 (always 0 on Episode 3)
  uint8_t battle_mode; // 0 or 1 (always 0 on Episode 3)
  // Note: Episode 3 uses the challenge mode flag for view battle permissions.
  // 0 = view battle allowed; 1 = not allowed
  uint8_t challenge_mode; // 0 or 1
  // Note: According to the Sylverant wiki, in v2-land, the episode field has a
  // different meaning: if set to 0, the game can be joined by v1 and v2
  // players; if set to 1, it's v2-only.
  uint8_t episode; // 1-4 on V3+ (3 on Episode 3); unused on DC/PC
};
struct C_CreateGame_DC_V3_0C_C1_Ep3_EC : C_CreateGame<char> { };
struct C_CreateGame_PC_C1 : C_CreateGame<char16_t> { };

struct C_CreateGame_BB_C1 : C_CreateGame<char16_t> {
  uint8_t solo_mode;
  uint8_t unused2[3];
};

// C2 (C->S): Set choice search parameters
// Server does not respond.

template <typename ItemIDT>
struct C_ChoiceSearchSelections_C2_C3 {
  le_uint16_t disabled; // 0 = enabled, 1 = disabled. Unused for command C3
  le_uint16_t unused;
  struct Entry {
    ItemIDT parent_category_id;
    ItemIDT category_id;
  };
  Entry entries[0];
};

struct C_ChoiceSearchSelections_DC_C2_C3 : C_ChoiceSearchSelections_C2_C3<le_uint32_t> { };
struct C_ChoiceSearchSelections_PC_V3_BB_C2_C3 : C_ChoiceSearchSelections_C2_C3<le_uint16_t> { };

// C3 (C->S): Execute choice search
// Same format as C2. The disabled field is unused.
// Server should respond with a C4 command.

// C4 (S->C): Choice search results

// Command is a list of these; header.flag is the entry count
struct S_ChoiceSearchResultEntry_V3_C4 {
  le_uint32_t guild_card_number;
  ptext<char, 0x10> name; // No language marker, as usual on V3
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

// C5 (S->C): Challenge rank update (V3/BB)
// header.flag = entry count
// The server sends this command when a player joins a lobby to update the
// challenge mode records of all the present players.
// Entry format is PlayerChallengeDataV3 or PlayerChallengeDataBB.
// newserv currently doesn't send this command at all because the V3 and
// BB formats aren't fully documented.
// TODO: Figure out where the text is in those formats, write appropriate
// conversion functions, and implement the command. Don't forget to override the
// client_id field in each entry before sending.

// C6 (C->S): Set blocked senders list (V3/BB)
// The command always contains the same number of entries, even if the entries
// at the end are blank (zero).

template <size_t Count>
struct C_SetBlockedSenders_C6 {
  parray<le_uint32_t, Count> blocked_senders;
};

struct C_SetBlockedSenders_V3_C6 : C_SetBlockedSenders_C6<30> { };
struct C_SetBlockedSenders_BB_C6 : C_SetBlockedSenders_C6<28> { };

// C7 (C->S): Enable simple mail auto-reply (V3/BB)
// Same format as 1A/D5 command (plain text).
// Server does not respond

// C8 (C->S): Disable simple mail auto-reply (V3/BB)
// No arguments
// Server does not respond

// C9 (C->S): Unknown (XB)
// No arguments except header.flag

// C9: Broadcast command (Episode 3)
// Same as 60, but only send to Episode 3 clients.

// CA (C->S): Server data request (Episode 3)
// The format is generally the same as the subcommand-based commands (60, 62,
// etc.), but the server is expected to respond to the command instead of
// forwarding it. (The client has no handler for CA commands at all.)
// Generally a CA command looks like this:
// CA 00 SS SS B3 TT 00 00 WW 00 00 00 ...
//   S = command size
//   T = subcommand size in uint32_ts (== (S / 4) - 1)
//   W = subcommand number
// We refer to the various Episode 3 server data commands as CAxWW, where W
// comes from the format above. The server data commands are:
//   CAx0B (T=05) - Unknown
//   CAx0C (T=05) - Unknown
//   CAx0D (T=07) - Unknown
//   CAx0E (T=05) - Unknown
//   CAx0F (T=07) - Unknown
//   CAx10 (T=06) - Unknown
//   CAx11 (T=1E) - Unknown
//   CAx12 (T=05) - Unknown
//   CAx13 (T=AF) - Update game state (?)
//   CAx14 (T=1B) - Update playfield state (?)
//   CAx1B (T=09) - Update names (?)
//   CAx1D (T=04) - Unknown
//   CAx21 (T=05) - Unknown
//   CAx28 (T=05) - Unknown
//   CAx2B (T=05) - Unknown
//   CAx34 (T=05) - Unknown
//   CAx3A (T=04) - Unknown
//   CAx40 (T=04) - Map list request. See send_ep3_map_list for server response.
//   CAx41 (T=05) - Map data request. See send_ep3_map_data for server response.
//   CAx48 (T=05) - Unknown
//   CAx49 (T=C1) - Unknown
// TODO: Document the above commands that are currently unknown.

// CB: Broadcast command (Episode 3)
// Same as 60, but only send to Episode 3 clients.
// This command is identical to C9, except that CB is not valid on Episode 3
// Trial Edition (whereas C9 is valid).
// TODO: What's the difference here? The client-side handlers are identical, so
// presumably the server is supposed to do something different between the two
// commands, but it's not clear what that difference should be.

// CC (S->C): Confirm tournament entry (Episode 3)
// This command is not valid on Episode 3 Trial Edition.
// header.flag determines the client's registration state - 1 if the client is
// registered for the tournament, 0 if not.

struct S_ConfirmTournamentEntry_GC_Ep3_CC {
  ptext<char, 0x40> tournament_name;
  parray<le_uint16_t, 4> unknown_a2;
  ptext<char, 0x20> server_name;
  ptext<char, 0x20> start_time; // e.g. "15:09:30" or "13:03 PST"
  struct Entry {
    le_uint16_t unknown_a1;
    le_uint16_t present; // 1 if team present, 0 otherwise
    ptext<char, 0x20> team_name;
  };
  Entry entries[0x20];
};

// CD: Invalid command
// CE: Invalid command
// CF: Invalid command

// D0 (C->S): Start trade sequence (V3/BB)
// The trade window sequence is a bit complicated. The normal flow is:
// - Clients sync trade state with 6xA6 commands
// - When both have confirmed, one client (the initiator) sends a D0
// - Server sends a D1 to the non-initiator
// - Non-initiator sends a D0
// - Server sends a D1 to both clients
// - Both clients delete the sent items from their inventories (and send the
//   appropriate subcommand)
// - Both clients send a D2 (similarly to how AC works, the server should not
//   proceed until both D2s are received)
// - Server sends a D3 to both clients with each other's data from their D0s,
//   followed immediately by a D4 01 to both clients, which completes the trade
// - Both clients send the appropriate subcommand to create inventory items
// TODO: On BB, is the server responsible for sending the appropriate item
// subcommands?
// At any point if an error occurs, either client may send a D4 00, which
// cancels the entire sequence. The server should then send D4 00 to both
// clients.
// TODO: The server should presumably also send a D4 00 if either client
// disconnects during the sequence.

struct SC_TradeItems_D0_D3 { // D0 when sent by client, D3 when sent by server
  le_uint16_t target_client_id;
  le_uint16_t item_count;
  // Note: PSO GC sends uninitialized data in the unused entries of this
  // command. newserv parses and regenerates the item data when sending D3,
  // which effectively erases the uninitialized data.
  ItemData items[0x20];
};

// D1 (S->C): Advance trade state (V3/BB)
// No arguments
// See D0 description for usage information.

// D2 (C->S): Trade can proceed (V3/BB)
// No arguments
// See D0 description for usage information.

// D3 (S->C): Execute trade (V3/BB)
// Same format as D0. See D0 description for usage information.

// D4 (C->S): Trade failed (V3/BB)
// No arguments
// See D0 description for usage information.

// D4 (S->C): Trade complete (V3/BB)
// header.flag must be 0 (trade failed) or 1 (trade complete).
// See D0 description for usage information.

// D5: Large message box (V3/BB)
// Same as 1A command, except the maximum length of the message is 0x1000 bytes.

// D6 (C->S): Large message box closed (V3)
// No arguments
// DC, PC, and BB do not send this command at all. GC US v1.00 and v1.01 will
// send this command when any large message box (1A/D5) is closed; GC Plus and
// Episode 3 will send D6 only for large message boxes that occur before the
// client has joined a lobby. (After joining a lobby, large message boxes will
// still be displayed if sent by the server, but the client won't send a D6 when
// they are closed.)

// D7 (C->S): Request GBA game file (V3)
// The server should send the requested file using A6/A7 commands.
// This command exists on XB as well, but it presumably is never sent by the
// client.

struct C_GBAGameRequest_V3_D7 {
  ptext<char, 0x10> filename;
};

// D7 (S->C): Unknown (V3/BB)
// No arguments
// This command is not valid on PSO GC Episodes 1&2 Trial Edition.
// On PSO V3, this command does... something. The command isn't *completely*
// ignored: it sets a global state variable, but it's not clear what that
// variable does. That variable is also set when a D7 is sent by the client, so
// it likely is related to GBA game loading in some way.
// PSO BB completely ignores this command.

// D8 (C->S): Info board request (V3/BB)
// No arguments
// The server should respond with a D8 command (described below).

// D8 (S->C): Info board contents (V3/BB)
// This command is not valid on PSO GC Episodes 1&2 Trial Edition.

// Command is a list of these; header.flag is the entry count. There should be
// one entry for each player in the current lobby/game.
template <typename CharT>
struct S_InfoBoardEntry_D8 {
  ptext<CharT, 0x10> name;
  ptext<CharT, 0xAC> message;
};
struct S_InfoBoardEntry_BB_D8 : S_InfoBoardEntry_D8<char16_t> { };
struct S_InfoBoardEntry_V3_D8 : S_InfoBoardEntry_D8<char> { };

// D9 (C->S): Write info board (V3/BB)
// Contents are plain text, like 1A/D5.
// Server does not respond

// DA (S->C): Change lobby event (V3/BB)
// header.flag = new event number; no other arguments.
// This command is not valid on PSO GC Episodes 1&2 Trial Edition.

// DB (C->S): Verify license (V3/BB)
// Server should respond with a 9A command.

struct C_VerifyLicense_V3_DB {
  ptext<char, 0x20> unused;
  ptext<char, 0x10> serial_number; // On XB, this is the XBL gamertag
  ptext<char, 0x10> access_key; // On XB, this is the XBL user ID
  ptext<char, 0x08> unused2;
  le_uint32_t sub_version;
  ptext<char, 0x30> serial_number2; // On XB, this is the XBL gamertag
  ptext<char, 0x30> access_key2; // On XB, this is the XBL user ID
  ptext<char, 0x30> password; // On XB, this contains "xbox-pso"
};

// Note: This login pathway generally isn't used on BB (and isn't supported at
// all during the data server phase). All current servers use 03/93 instead.
struct C_VerifyLicense_BB_DB {
  // Note: These four fields are likely the same as those used in BB's 9E
  ptext<char, 0x10> unknown_a3; // Always blank?
  ptext<char, 0x10> unknown_a4; // == "?"
  ptext<char, 0x10> unknown_a5; // Always blank?
  ptext<char, 0x10> unknown_a6; // Always blank?
  le_uint32_t sub_version;
  ptext<char, 0x30> username;
  ptext<char, 0x30> password;
  ptext<char, 0x30> game_tag; // "psopc2"
};

// DC: Player menu state (Episode 3)
// No arguments. It seems the client expects the server to respond with another
// DC command, the contents and flag of which are ignored entirely - all it does
// is set a global flag on the client. This could be the mechanism for waiting
// until all players are at the counter, like how AC (quest barrier) works. I
// haven't spent any time investigating what this actually does; newserv just
// immediately and unconditionally responds to any DC from an Episode 3 client.

// DC: Guild card data (BB)

struct S_GuildCardHeader_BB_01DC {
  le_uint32_t unknown; // should be 1
  le_uint32_t filesize; // 0x0000D590
  le_uint32_t checksum; // CRC32 of entire guild card file (0xD590 bytes)
};

struct S_GuildCardFileChunk_02DC {
  le_uint32_t unknown; // 0
  le_uint32_t chunk_index;
  uint8_t data[0x6800]; // Command may be shorter if this is the last chunk
};

struct C_GuildCardDataRequest_BB_03DC {
  le_uint32_t unknown;
  le_uint32_t chunk_index;
  le_uint32_t cont;
};

// DD (S->C): Send quest state to joining player (BB)
// When a player joins a game with a quest already in progress, the server
// should send this command to the leader. header.flag is the client ID that the
// leader should send quest state to; the leader will then send a series of
// target commands (62/6D) that the server can forward to the joining player.
// No other arguments

// DE (S->C): Rare monster configuration (BB)

struct S_RareMonsterConfig_BB_DE {
  le_uint16_t data[16];
};

// DF (C->S): Unknown (BB)
// This command has many subcommands. It's not clear what any of them do.

struct C_Unknown_BB_01DF {
  le_uint32_t unknown_a1;
};

struct C_Unknown_BB_02DF {
  le_uint32_t unknown_a1;
};

struct C_Unknown_BB_03DF {
  le_uint32_t unknown_a1;
};

struct C_Unknown_BB_04DF {
  le_uint32_t unknown_a1;
};

struct C_Unknown_BB_05DF {
  le_uint32_t unknown_a1;
  ptext<char16_t, 0x0C> unknown_a2;
};

struct C_Unknown_BB_06DF {
  parray<le_uint32_t, 3> unknown_a1;
};

struct C_Unknown_BB_07DF {
  le_uint32_t unused1; // Always 0xFFFFFFFF
  le_uint32_t unused2; // Always 0
  parray<le_uint32_t, 5> unknown_a1;
};

// E0 (S->C): Tournament list (Episode 3)
// The client will send 09 and 10 commands to inspect or enter a tournament. The
// server should respond to an 09 command with an E3 command; the server should
// respond to a 10 command with an E2 command.

// header.flag is the count of filled-in entries.
struct S_TournamentList_GC_Ep3_E0 {
  struct Entry {
    le_uint32_t menu_id;
    le_uint32_t item_id;
    parray<uint8_t, 4> unknown_a1;
    le_uint32_t start_time; // In seconds since Unix epoch
    ptext<char, 0x20> name;
    le_uint16_t num_teams;
    le_uint16_t max_teams;
    parray<le_uint16_t, 2> unknown_a3;
  };
  Entry entries[0x20];
};

// E0 (C->S): Request team and key config (BB)

// E1 (S->C): Battle information (Episode 3)

struct S_Unknown_GC_Ep3_E1 {
  /* 0004 */ parray<uint8_t, 0x20> game_name;
  struct Entry {
    ptext<char, 0x10> name;
    ptext<char, 0x20> description;
  };
  /* 0024 */ Entry entries[4];
  /* 00E4 */ parray<uint8_t, 0x20> unknown_a3;
  /* 0104 */ parray<uint8_t, 0x14> unknown_a4;
  /* 0118 */ parray<uint8_t, 0x180> unknown_a5;
};

// E2 (C->S): Tournament control (Episode 3)
// No arguments (in any of its forms) except header.flag, which determines ths
// command's meaning. Specifically:
//   header.flag = 00 => request tournament list (server responds with E0)
//   header.flag = 01 => check tournament (server responds with E2)
//   header.flag = 02 => cancel tournament entry (server responds with CC)
//   header.flag = 03 => create tournament spectator team (get battle list)
//   header.flag = 04 => join tournament spectator team (get team list)
// In case 02, the resulting CC command is apparently completely blank (all 0),
// and has header.flag = 0 to indicate the player isn't registered.
// In cases 03 and 04, it's not clear what the server should respond with.

// E2 (S->C): Tournament entry list (Episode 3)
// Client may send 09 commands if the player presses X. It's not clear what the
// server should respond with in this case.
// If the player selects an entry slot, client will respond with a long-form 10
// command (the Flag03 variant); in this case, unknown_a1 is the team name, and
// password is the team password. The server should respond to that with a CC
// command.

struct S_TournamentEntryList_GC_Ep3_E2 {
  le_uint16_t unknown_a1;
  le_uint16_t unknown_a2;
  struct Entry {
    le_uint32_t menu_id;
    le_uint32_t item_id;
    parray<uint8_t, 4> unknown_a1;
    ptext<char, 0x20> team_name;
  };
  Entry entries[0x20];
};

// E2 (S->C): Team and key config (BB)
// See KeyAndTeamConfigBB in Player.hh for format

// E3 (S->C): Tournament info (Episode 3)

struct S_TournamentInfo_GC_Ep3_E3 {
  struct Entry {
    le_uint16_t unknown_a1;
    le_uint16_t unknown_a2;
    ptext<char, 0x20> team_name;
  };
  ptext<char, 0x20> name;
  ptext<char, 0x20> map_name;
  Ep3BattleRules rules;
  Entry entries[0x20];
  parray<uint8_t, 0xE4> unknown_a2;
  le_uint16_t max_entries;
  le_uint16_t unknown_a3;
  le_uint16_t unknown_a4;
  le_uint16_t unknown_a5;
  parray<uint8_t, 0x180> unknown_a6;
};

// E3 (C->S): Player preview request (BB)

struct C_PlayerPreviewRequest_BB_E3 {
  le_uint32_t player_index;
  le_uint32_t unused;
};

// E4: CARD lobby battle table state (Episode 3)
// When client sends an E4, server should respond with another E4 (but these
// commands have different formats).

// Header flag = seated state (1 = present, 0 = leaving)
struct C_CardBattleTableState_GC_Ep3_E4 {
  le_uint16_t table_number;
  le_uint16_t seat_number;
};

// Header flag = table number
struct S_CardBattleTableState_GC_Ep3_E4 {
  struct Entry {
    le_uint16_t present; // 1 = player present, 0 = no player
    le_uint16_t unknown_a1;
    le_uint32_t guild_card_number;
  };
  Entry entries[4];
};

// E4 (S->C): Player choice or no player present (BB)

struct S_ApprovePlayerChoice_BB_00E4 {
  le_uint32_t player_index;
  le_uint32_t result; // 1 = approved
};

struct S_PlayerPreview_NoPlayer_BB_00E4 {
  le_uint32_t player_index;
  le_uint32_t error; // 2 = no player present
};

// E5 (C->S): Confirm CARD lobby battle table choice (Episode 3)
// header.flag specifies whether the client answered "Yes" (1) or "No" (0).

struct S_CardBattleTableConfirmation_GC_Ep3_E5 {
  le_uint16_t table_number;
  le_uint16_t seat_number;
};

// E5 (S->C): Player preview (BB)
// E5 (C->S): Create character (BB)

struct SC_PlayerPreview_CreateCharacter_BB_00E5 {
  le_uint32_t player_index;
  PlayerDispDataBBPreview preview;
};

// E6 (C->S): Spectator team control (Episode 3)

// With header.flag == 0, this command has no arguments and is used for
// requesting the spectator team list. The server responds with an E6 command.

// With header.flag == 1, this command is presumably used for joining a
// spectator team (TODO: verify this). The following arguments are given in this
// form:

struct C_JoinSpectatorTeam_GC_Ep3_E6_Flag01 {
  le_uint32_t menu_id;
  le_uint32_t item_id;
};

// E6 (S->C): Spectator team list (Episode 3)
// Same format as 08 command.

// E6 (S->C): Set guild card number and update client config (BB)
// BB clients have multiple client configs. This command sets the client config
// that is returned by the 93 commands, but does not affect the client config
// set by the 04 command (and returned in the 9E and 9F commands).

struct S_ClientInit_BB_00E6 {
  le_uint32_t error;
  le_uint32_t player_tag;
  le_uint32_t guild_card_number;
  le_uint32_t team_id;
  ClientConfigBB cfg;
  le_uint32_t caps; // should be 0x00000102
};

// E7 (C->S): Create spectator team (Episode 3)

struct C_CreateSpectatorTeam_GC_Ep3_E7 {
  le_uint32_t menu_id;
  le_uint32_t item_id;
  ptext<char, 0x10> name;
  ptext<char, 0x10> password;
  le_uint32_t unused;
};

// E7 (S->C): Unknown (Episode 3)
// Same format as E2 command.

// E7: Save or load full player data (BB)
// See export_bb_player_data() in Player.cc for format.
// TODO: Verify full breakdown from send_E7 in BB disassembly.

// E8 (S->C): Join spectator team (Episode 3)
// header.flag = player count (including spectators)

struct S_JoinSpectatorTeam_GC_Ep3_E8 {
  parray<le_uint32_t, 0x20> variations; // 04-84
  struct PlayerEntry {
    PlayerLobbyDataDCGC lobby_data; // 0x20 bytes
    PlayerInventory inventory; // 0x34C bytes
    PlayerDispDataDCPCV3 disp; // 0xD0 bytes
  }; // 0x43C bytes
  PlayerEntry players[4]; // 84-1174
  parray<uint8_t, 8> unknown_a2; // 1174-117C
  le_uint32_t unknown_a3; // 117C-1180
  parray<uint8_t, 4> unknown_a4; // 1180-1184
  struct SpectatorEntry {
    le_uint32_t player_tag;
    le_uint32_t guild_card_number;
    ptext<char, 0x20> name;
    uint8_t unknown_a3[2];
    le_uint16_t unknown_a4;
    parray<le_uint32_t, 2> unknown_a5;
    parray<le_uint16_t, 2> unknown_a6;
  }; // 0x38 bytes
  // Somewhat misleadingly, this array also includes the players actually in the
  // battle - they appear in the first positions. Presumably the first 4 are
  // always for battlers, and the last 8 are always for spectators.
  SpectatorEntry entries[12]; // 1184-1424
  ptext<uint8_t, 0x20> spectator_team_name;
  // This field doesn't appear to be actually used by the game, but some servers
  // send it anyway (and the game presumably ignores it)
  PlayerEntry spectator_players[8];
};

// E8 (C->S): Guild card commands (BB)

// 01E8 (C->S): Check guild card file checksum

// This struct is for documentation purposes only; newserv ignores the contents
// of this command.
struct C_GuildCardChecksum_01E8 {
  le_uint32_t checksum;
  le_uint32_t unused;
};

// 02E8 (S->C): Accept/decline guild card file checksum
// If needs_update is nonzero, the client will request the guild card file by
// sending an 03E8 command. If needs_update is zero, the client will skip
// downloading the guild card file and send a 04EB command (requesting the
// stream file) instead.

struct S_GuildCardChecksumResponse_BB_02E8 {
  le_uint32_t needs_update;
  le_uint32_t unused;
};

// 03E8 (C->S): Request guild card file
// No arguments
// Server should send the guild card file data using DC commands.

// 04E8 (C->S): Add guild card
// Format is GuildCardBB (see Player.hh)

// 05E8 (C->S): Delete guild card

struct C_DeleteGuildCard_BB_05E8_08E8 {
  le_uint32_t guild_card_number;
};

// 06E8 (C->S): Update (overwrite) guild card
// Note: This command is also sent when the player writes a comment on their own
// guild card.
// Format is GuildCardBB (see Player.hh)

// 07E8 (C->S): Add blocked user
// Format is GuildCardBB (see Player.hh)

// 08E8 (C->S): Delete blocked user
// Same format as 05E8.

// 09E8 (C->S): Write comment

struct C_WriteGuildCardComment_BB_09E8 {
  le_uint32_t guild_card_number;
  ptext<char16_t, 0x58> comment;
};

// 0AE8 (C->S): Set guild card position in list

struct C_MoveGuildCard_BB_0AE8 {
  le_uint32_t guild_card_number;
  le_uint32_t position;
};

// E9 (S->C): Remove player from spectator team (Episode 3)
// Same format as 66/69 commands. Like 69 (and unlike 66), the disable_udp field
// is unused in command E9.

// EA (S->C): Timed message box (Episode 3)
// The message appears in the upper half of the screen; the box is as wide as
// the 1A/D5 box but is vertically shorter. The box cannot be dismissed or
// interacted with by the player in any way; it disappears by itself after the
// given number of frames.
// header.flag appears to be relevant - the handler's behavior is different if
// it's 1 (vs. any other value). There don't seem to be any in-game behavioral
// differences though.

struct S_TimedMessageBoxHeader_GC_Ep3_EA {
  le_uint32_t duration; // In frames; 30 frames = 1 second
  // Message data follows here (up to 0x1000 chars)
};

// EA: Team control (BB)

// 01EA (C->S): Create team

struct C_CreateTeam_BB_01EA {
  ptext<char16_t, 0x10> name;
};

// 03EA (C->S): Add team member

struct C_AddOrRemoveTeamMember_BB_03EA_05EA {
  le_uint32_t guild_card_number;
};

// 05EA (C->S): Remove team member
// Same format as 03EA.

// 07EA (C->S): Team chat

// 08EA (C->S): Team admin
// No arguments

// 0DEA (C->S): Unknown
// No arguments

// 0EEA (S->C): Unknown

// 0FEA (C->S): Set team flag

struct C_SetTeamFlag_BB_0FEA {
  parray<uint8_t, 0x800> data;
};

// 10EA: Delete team
// No arguments

// 11EA (C->S): Promote team member
// TODO: header.flag is used for this command. Figure out what it's for.

struct C_PromoteTeamMember_BB_11EA {
  le_uint32_t unknown_a1;
};

// 12EA (S->C): Unknown

// 13EA: Unknown
// No arguments

// 14EA (C->S): Unknown
// No arguments. Client always sends 1 in the header.flag field.

// 15EA (S->C): Unknown

// 18EA: Membership information
// No arguments (C->S)
// TODO: Document S->C format

// 19EA: Privilege list
// No arguments (C->S)
// TODO: Document S->C format

// 1AEA: Unknown
// 1CEA (C->S): Ranking information

// 1BEA (C->S): Unknown
// header.flag is used, but no other arguments

// 1CEA (C->S): Unknown
// No arguments

// 1EEA (C->S): Unknown
// header.flag is used, but it's unknown what the value means.

struct C_Unknown_BB_1EEA {
  ptext<char16_t, 0x10> unknown_a1;
};

// 20EA (C->S): Unknown
// header.flag is used, but no other arguments

// EB (S->C): Add player to spectator team (Episode 3)
// Same format and usage as 65 and 68 commands, but sent to spectators in a
// spectator team.
// This command is used to add both primary players and spectators - if the
// client ID in .lobby_data is 0-3, it's a primary player, otherwise it's a
// spectator. (Of course, the other primary players in the game receive a 65
// command rather than an EB command to notify them of the joining
// player.)

// 01EB (S->C): Send stream file index (BB)

// Command is a list of these; header.flag is the entry count.
struct S_StreamFileIndexEntry_BB_01EB {
  le_uint32_t size;
  le_uint32_t checksum; // CRC32 of file data
  le_uint32_t offset; // offset in stream (== sum of all previous files' sizes)
  ptext<char, 0x40> filename;
};

// 02EB (S->C): Send stream file chunk (BB)

struct S_StreamFileChunk_BB_02EB {
  le_uint32_t chunk_index;
  uint8_t data[0x6800];
};

// 03EB (C->S): Request a specific stream file chunk
// header.flag is the chunk index. Server should respond with a 02EB command.

// 04EB (C->S): Request stream file header
// No arguments
// Server should respond with a 01EB command.

// EC: Create game (Episode 3)
// Same format as C1; some fields are unused (e.g. episode, difficulty).

// EC: Leave character select (BB)

struct C_LeaveCharacterSelect_BB_00EC {
  // Reason codes:
  // 0 = canceled
  // 1 = recreate character
  // 2 = dressing room
  le_uint32_t reason;
};

// ED (S->C): Force leave lobby/game (Episode 3)
// No arguments
// This command forces the client out of the game or lobby they're currently in
// and sends them to the lobby. If the client is in a lobby (and not a game),
// the client sends a 98 in response as if they were in a game. Curiously, the
// client also sends a meseta transaction (BA) with a value of zero before
// sending an 84 to be added to a lobby. This is used when a spectator team is
// disbanded because the target game ends.

// ED (C->S): Update account data (BB)
// There are several subcommands (noted in the union below) that each update a
// specific kind of account data.
// TODO: Actually define these structures and don't just treat them as raw data

union C_UpdateAccountData_BB_ED {
  le_uint32_t option; // 01ED
  parray<uint8_t, 0x4E0> symbol_chats; // 02ED
  parray<uint8_t, 0xA40> chat_shortcuts; // 03ED
  parray<uint8_t, 0x16C> key_config; // 04ED
  parray<uint8_t, 0x38> pad_config; // 05ED
  parray<uint8_t, 0x28> tech_menu; // 06ED
  parray<uint8_t, 0xE8> customize; // 07ED
  parray<uint8_t, 0x140> challenge_battle_config; // 08ED
};

// EE: Trade cards (Episode 3)
// This command has different forms depending on the header.flag value; the flag
// values match the command numbers from the Episodes 1&2 trade window sequence.
// The sequence of events with the EE command matches that of the Episodes 1&2
// trade window; see the description of the D0 command above for details.

// EE D0 (C->S): Begin trade
struct SC_TradeCards_GC_Ep3_EE_FlagD0_FlagD3 {
  le_uint16_t target_client_id;
  le_uint16_t entry_count;
  struct Entry {
    le_uint32_t card_type;
    le_uint32_t count;
  };
  Entry entries[4];
};

// EE D1 (S->C): Advance trade state
struct S_AdvanceCardTradeState_GC_Ep3_EE_FlagD1 {
  le_uint32_t unused;
};

// EE D2 (C->S): Trade can proceed
// No arguments

// EE D3 (S->C): Execute trade
// Same format as EE D0

// EE D4 (C->S): Trade failed
// EE D4 (S->C): Trade complete

struct S_CardTradeComplete_GC_Ep3_EE_FlagD4 {
  le_uint32_t success; // 0 = failed, 1 = success, anything else = invalid
};

// EE (S->C): Scrolling message (BB)
// Same format as 01. The message appears at the top of the screen and slowly
// scrolls to the left.

// EF (C->S): Unknown (Episode 3)
// No arguments

// EF (S->C): Unknown (Episode 3)

struct S_Unknown_GC_Ep3_EF {
  le_uint16_t unknown_a1;
  le_uint16_t unknown_a2;
  struct Entry {
    le_uint16_t unknown_a1;
    le_uint16_t unknown_a2;
  };
  Entry entries[0x14];
};

// EF (S->C): Unknown (BB)
// Has an unknown number of subcommands (00EF, 01EF, etc.)
// Contents are plain text (char).

// F0 (S->C): Unknown (BB)

struct S_Unknown_BB_F0 {
  le_uint32_t unknown_a1[7];
  le_uint32_t which; // Must be < 12
  ptext<char16_t, 0x10> unknown_a2;
  le_uint32_t unknown_a3;
};

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



// Now, the game subcommands (used in commands 60, 62, 6C, 6D, C9, and CB).
// These are laid out similarly as above. These structs start with G_ to
// indicate that they are (usually) bidirectional, and are (usually) generated
// by clients and consumed by clients. Generally in newserv source, these
// commands are referred to as (for example) 6x02, etc.

// All game subcommands have the same header format, which is one of:
// - XX SS ...
// - XX 00 ?? ?? TT TT TT TT ...
// where X is the subcommand number (e.g. in 6xA2, it would be A2), S is the
// size in words of the entire subcommand (that is, overall size in bytes / 4),
// and T is the overall size in bytes. The second form is generally only used
// when the overall size in bytes is 0x400 or longer (so the S field doesn't
// suffice to describe its length), but it may also be used in some cases where
// the subcommand is shorter.
// Multiple subcommands may be sent in the same 6x command. It seems the client
// never sends commands like this, but newserv generates commands containing
// multiple subcommands in some situations (for example, the implementation of
// infinite HP does this).
// If any subcommand or group thereof is longer than 0x400 bytes, the 6C or 6D
// commands must be used. The 60 and 62 commands exhibit undefined behavior if
// this limit is exceeded.

// These structures are used by many commands (noted below)
struct G_ItemSubcommand {
  uint8_t command;
  uint8_t size;
  le_uint16_t client_id;
  le_uint32_t item_id;
  le_uint32_t amount;
};

struct G_ItemIDSubcommand {
  uint8_t subcommand;
  uint8_t size;
  le_uint16_t unused;
  le_uint32_t item_id;
};




// 6x00: Invalid subcommand
// 6x01: Invalid subcommand
// 6x02: Unknown
// 6x03: Unknown (same handler as 02)
// 6x04: Unknown

// 6x05: Switch state changed
// Some things that don't look like switches are implemented as switches using
// this subcommand. For example, when all enemies in a room are defeated, this
// subcommand is used to unlock the doors.

struct G_SwitchStateChanged_6x05 {
  uint8_t subcommand;
  uint8_t size;
  le_uint16_t switch_id; // 0xFFFF for room clear when all enemies defeated
  parray<uint8_t, 6> unknown;
  uint8_t area;
  uint8_t enabled;
};

// 6x06: Send guild card

template <typename CharT, size_t UnusedLength>
struct G_SendGuildCard_DC_PC_V3 {
  uint8_t subcommand;
  uint8_t size;
  le_uint16_t unused;
  le_uint32_t player_tag;
  le_uint32_t guild_card_number;
  ptext<CharT, 0x18> name;
  ptext<CharT, 0x48> description;
  parray<uint8_t, UnusedLength> unused2;
  uint8_t present;
  uint8_t present2;
  uint8_t section_id;
  uint8_t char_class;
};

struct G_SendGuildCard_DC_6x06 : G_SendGuildCard_DC_PC_V3<char, 0x11> {
  parray<uint8_t, 3> unused3;
};
struct G_SendGuildCard_PC_6x06 : G_SendGuildCard_DC_PC_V3<char16_t, 0x24> { };
struct G_SendGuildCard_V3_6x06 : G_SendGuildCard_DC_PC_V3<char, 0x24> { };

struct G_SendGuildCard_BB_6x06 {
  uint8_t subcommand;
  uint8_t size;
  le_uint16_t unused;
  le_uint32_t guild_card_number;
  ptext<char16_t, 0x18> name;
  ptext<char16_t, 0x10> team_name;
  ptext<char16_t, 0x58> description;
  uint8_t present;
  uint8_t present2;
  uint8_t section_id;
  uint8_t char_class;
};

// 6x07: Symbol chat

struct G_SymbolChat_6x07 {
  uint8_t command;
  uint8_t size;
  le_uint16_t unused1;
  le_uint32_t client_id;
  // Bits: SSSCCCFF (S = sound, C = face color, F = face shape)
  uint8_t face_spec;
  uint8_t disable_sound; // If low bit is set, no sound is played
  le_uint16_t unused2;
  struct CornerObject {
    uint8_t type; // FF = no object in this slot
    // Bits: 000VHCCC (V = reverse vertical, H = reverse horizontal, C = color)
    uint8_t flags_color;
  };
  CornerObject corner_objects[4]; // In reading order (top-left is first)
  struct FacePart {
    uint8_t type; // FF = no part in this slot
    uint8_t x;
    uint8_t y;
    // Bits: 000000VH (V = reverse vertical, H = reverse horizontal)
    uint8_t flags;
  };
  FacePart face_parts[12];
};

// 6x08: Invalid subcommand
// 6x09: Unknown

// 6x0A: Enemy hit

struct G_EnemyHitByPlayer_6x0A {
  uint8_t command;
  uint8_t size;
  le_uint16_t enemy_id2;
  le_uint16_t enemy_id;
  le_uint16_t damage;
  le_uint32_t flags;
};

// 6x0B: Box destroyed
// 6x0C: Add condition (poison/slow/etc.)
// 6x0D: Remove condition (poison/slow/etc.)
// 6x0E: Unknown
// 6x0F: Invalid subcommand
// 6x10: Unknown (not valid on Episode 3)
// 6x11: Unknown (not valid on Episode 3)
// 6x12: Dragon boss actions (not valid on Episode 3)
// 6x13: Re Rol Le boss actions (not valid on Episode 3)
// 6x14: Unknown (supported; game only; not valid on Episode 3)
// 6x15: Vol Opt boss actions (not valid on Episode 3)
// 6x16: Vol Opt boss actions (not valid on Episode 3)
// 6x17: Unknown (supported; game only; not valid on Episode 3)
// 6x18: Unknown (supported; game only; not valid on Episode 3)
// 6x19: Dark Falz boss actions (not valid on Episode 3)
// 6x1A: Invalid subcommand
// 6x1B: Unknown (not valid on Episode 3)
// 6x1C: Unknown (supported; game only; not valid on Episode 3)
// 6x1D: Invalid subcommand
// 6x1E: Invalid subcommand
// 6x1F: Unknown (supported; lobby & game)
// 6x20: Set position (existing clients send when a new client joins a lobby/game)
// 6x21: Inter-level warp

// 6x22: Set player visibility
// 6x23: Set player visibility

struct G_SetPlayerVisibility_6x22_6x23 {
  uint8_t subcommand; // 22 = invisible, 23 = visible
  uint8_t size;
  le_uint16_t client_id;
};

// 6x24: Unknown (supported; game only)

// 6x25: Equip item

struct G_EquipItem_6x25 {
  uint8_t command;
  uint8_t size;
  le_uint16_t client_id;
  le_uint32_t item_id;
  le_uint32_t equip_slot;
};

// 6x26: Unequip item

struct G_UnequipItem_6x26 {
  uint8_t command;
  uint8_t size;
  le_uint16_t client_id;
  le_uint32_t item_id;
  le_uint32_t unused2;
};

// 6x27: Use item

struct G_UseItem_6x27 {
  uint8_t command;
  uint8_t size;
  le_uint16_t client_id;
  le_uint32_t item_id;
};

// 6x28: Feed MAG

struct G_FeedMAG_6x28 {
  uint8_t subcommand;
  uint8_t size;
  le_uint16_t client_id;
  le_uint32_t mag_item_id;
  le_uint32_t fed_item_id;
};

// 6x29: Delete inventory item (via bank deposit / sale / feeding MAG)
// This subcommand is also used for reducing the size of stacks - if amount is
// less than the stack count, the item is not deleted and its ID remains valid.
// Format is G_ItemSubcommand

// 6x2A: Drop item

struct G_PlayerDropItem_6x2A {
  uint8_t command;
  uint8_t size;
  le_uint16_t client_id;
  le_uint16_t unused2; // should be 1
  le_uint16_t area;
  le_uint32_t item_id;
  le_float x;
  le_float y;
  le_float z;
};

// 6x2B: Create item in inventory (e.g. via tekker or bank withdraw)

struct G_PlayerCreateInventoryItem_DC_6x2B {
  uint8_t command;
  uint8_t size;
  le_uint16_t client_id;
  ItemData item;
};

struct G_PlayerCreateInventoryItem_PC_V3_BB_6x2B : G_PlayerCreateInventoryItem_DC_6x2B {
  le_uint32_t unknown;
};

// 6x2C: Talk to NPC
// 6x2D: Done talking to NPC
// 6x2E: Unknown
// 6x2F: Hit by enemy

// 6x30: Level up

struct G_LevelUp_6x30 {
  uint8_t command;
  uint8_t size;
  le_uint16_t client_id;
  le_uint16_t atp;
  le_uint16_t mst;
  le_uint16_t evp;
  le_uint16_t hp;
  le_uint16_t dfp;
  le_uint16_t ata;
  le_uint32_t level;
};

// 6x31: Medical center
// 6x32: Medical center
// 6x33: Revive player (e.g. with moon atomizer)
// 6x34: Unknown
// 6x35: Invalid subcommand
// 6x36: Unknown (supported; game only)
// 6x37: Photon blast
// 6x38: Unknown
// 6x39: Photon blast ready
// 6x3A: Unknown (supported; game only)
// 6x3B: Unknown (supported; lobby & game)
// 6x3C: Invalid subcommand
// 6x3D: Invalid subcommand

// 6x3E: Stop moving

struct G_StopAtPosition_6x3E {
  uint8_t subcommand;
  uint8_t size;
  le_uint16_t client_id;
  uint64_t unknown;
  le_float x;
  le_float y;
  le_float z;
};

// 6x3F: Set position

struct G_SetPosition_6x3F {
  uint8_t subcommand;
  uint8_t size;
  le_uint16_t client_id;
  le_uint32_t unknown;
  le_uint32_t area;
  le_float x;
  le_float y;
  le_float z;
};

// 6x40: Walk

struct G_WalkToPosition_6x40 {
  uint8_t subcommand;
  uint8_t size;
  le_uint16_t client_id;
  le_float x;
  le_float z;
  le_uint32_t unused;
};

// 6x41: Unknown

// 6x42: Run

struct G_RunToPosition_6x42 {
  uint8_t subcommand;
  uint8_t size;
  le_uint16_t client_id;
  le_float x;
  le_float z;
};

// 6x43: First attack
// 6x44: Second attack
// 6x45: Third attack
// 6x46: Attack finished (sent after each of 43, 44, and 45)
// 6x47: Cast technique
// 6x48: Cast technique complete
// 6x49: Subtract PB energy
// 6x4A: Fully shield attack
// 6x4B: Hit by enemy
// 6x4C: Hit by enemy
// 6x4D: Unknown (supported; lobby & game)
// 6x4E: Unknown (supported; lobby & game)
// 6x4F: Unknown (supported; lobby & game)
// 6x50: Unknown (supported; lobby & game)
// 6x51: Invalid subcommand
// 6x52: Toggle shop/bank interaction
// 6x53: Unknown (supported; game only)
// 6x54: Unknown
// 6x55: Intra-map warp
// 6x56: Unknown (supported; lobby & game)
// 6x57: Unknown (supported; lobby & game)
// 6x58: Unknown (supported; game only)

// 6x59: Pick up item

struct G_PickUpItem_6x59 {
  uint8_t subcommand;
  uint8_t size;
  le_uint16_t client_id;
  le_uint16_t client_id2;
  le_uint16_t area;
  le_uint32_t item_id;
};

// 6x5A: Request to pick up item

struct G_PickUpItemRequest_6x5A {
  uint8_t command;
  uint8_t size;
  le_uint16_t client_id;
  le_uint32_t item_id;
  uint8_t area;
  uint8_t unused2[3];
};

// 6x5B: Invalid subcommand
// 6x5C: Unknown

// 6x5D: Drop meseta or stacked item

struct G_DropStackedItem_DC_6x5D {
  uint8_t subcommand;
  uint8_t size;
  le_uint16_t client_id;
  le_uint16_t area;
  le_uint16_t unused2;
  le_float x;
  le_float z;
  ItemData data;
};

struct G_DropStackedItem_PC_V3_BB_6x5D : G_DropStackedItem_DC_6x5D {
  le_uint32_t unused3;
};

// 6x5E: Buy item at shop

struct G_BuyShopItem_6x5E {
  uint8_t subcommand;
  uint8_t size;
  le_uint16_t client_id;
  ItemData item;
};

// 6x5F: Drop item from box/enemy

struct G_DropItem_DC_6x5F {
  uint8_t subcommand;
  uint8_t size;
  le_uint16_t unused;
  uint8_t area;
  uint8_t from_enemy;
  le_uint16_t request_id; // < 0x0B50 if from_enemy != 0; otherwise < 0x0BA0
  le_float x;
  le_float z;
  le_uint32_t unused2;
  ItemData data;
};

struct G_DropItem_PC_V3_BB_6x5F : G_DropItem_DC_6x5F {
  le_uint32_t unused3;
};

// 6x60: Request for item drop (handled by the server on BB)

struct G_EnemyDropItemRequest_DC_6x60 {
  uint8_t command;
  uint8_t size;
  le_uint16_t unused;
  uint8_t area;
  uint8_t enemy_id;
  le_uint16_t request_id;
  le_float x;
  le_float z;
  le_uint32_t unknown_a1;
};

struct G_EnemyDropItemRequest_PC_V3_BB_6x60 : G_EnemyDropItemRequest_DC_6x60 {
  le_uint32_t unknown_a2;
};

// 6x61: Feed MAG
// 6x62: Unknown
// 6x63: Destroy item on the ground (used when too many items have been dropped)
// 6x64: Unknown (not valid on Episode 3)
// 6x65: Unknown (not valid on Episode 3)
// 6x66: Use star atomizer
// 6x67: Create enemy set
// 6x68: Telepipe/Ryuker
// 6x69: Unknown (supported; game only)
// 6x6A: Unknown (supported; game only; not valid on Episode 3)

// 6x6B: Sync enemy state (used while loading into game; same header format as 6E)
// 6x6C: Sync object state (used while loading into game; same header format as 6E)
// 6x6D: Sync item state (used while loading into game; same header format as 6E)
// 6x6E: Sync flag state (used while loading into game)

struct G_SyncGameStateHeader_6x6B_6x6C_6x6D_6x6E {
  uint8_t subcommand;
  parray<uint8_t, 3> unused;
  le_uint32_t subcommand_size;
  le_uint32_t unknown_a1;
  le_uint32_t data_size; // Must be <= subcommand_size - 0x10
  // BC0-compressed data follows here (use bc0_decompress from Compression.hh)
};

// 6x6F: Unknown (used while loading into game)

// 6x70: Sync player disp data and inventory (used while loading into game)
// Annoyingly, they didn't use the same format as the 65/67/68 commands here,
// and instead rearranged a bunch of things.
// TODO: Some missing fields should be easy to find in the future (e.g. when the
// sending player doesn't have 0 meseta, for example)

struct G_Unknown_6x70 {
  // Offsets in this struct are relative to the overall command header
  /* 0004 */ uint8_t subcommand; // == 0x70
  /* 0005 */ uint8_t basic_size; // == 0
  /* 0006 */ le_uint16_t unused;
  /* 0008 */ le_uint32_t subcommand_size;
  /* 000C */ parray<le_uint16_t, 2> unknown_a1;
  // [1] and [3] in this array (and maybe [2] also) appear to be le_floats;
  // they could be the player's current (x, y, z) coords
  /* 0010 */ parray<le_uint32_t, 7> unknown_a2;
  /* 002C */ parray<le_uint16_t, 4> unknown_a3;
  /* 0034 */ parray<parray<le_uint32_t, 3>, 5> unknown_a4;
  /* 0070 */ le_uint32_t unknown_a5;
  /* 0074 */ le_uint32_t player_tag;
  /* 0078 */ le_uint32_t guild_card_number;
  /* 007C */ parray<le_uint32_t, 2> unknown_a6;
  /* 0084 */ struct {
    parray<le_uint16_t, 2> unknown_a1;
    parray<le_uint32_t, 6> unknown_a2;
  } unknown_a7;
  /* 00A0 */ le_uint32_t unknown_a8;
  /* 00A4 */ parray<uint8_t, 0x14> unknown_a9;
  /* 00B8 */ le_uint32_t unknown_a10;
  /* 00BC */ le_uint32_t unknown_a11;
  /* 00C0 */ parray<uint8_t, 0x14> technique_levels; // Last byte is uninitialized
  /* 00D4 */ struct {
    parray<uint8_t, 0x10> name;
    uint64_t unknown_a2; // Same as unknown_a2 in PlayerDispDataDCPCV3, presumably
    le_uint32_t name_color;
    uint8_t extra_model;
    parray<uint8_t, 0x0F> unused;
    le_uint32_t name_color_checksum;
    uint8_t section_id;
    uint8_t char_class;
    uint8_t v2_flags;
    uint8_t version;
    le_uint32_t v1_flags;
    le_uint16_t costume;
    le_uint16_t skin;
    le_uint16_t face;
    le_uint16_t head;
    le_uint16_t hair;
    le_uint16_t hair_r;
    le_uint16_t hair_g;
    le_uint16_t hair_b;
    le_uint32_t proportion_x;
    le_uint32_t proportion_y;
  } disp_part2;
  /* 0124 */ struct {
    PlayerStats stats;
    parray<uint8_t, 0x0A> unknown_a1;
    le_uint32_t level;
    le_uint32_t experience;
    le_uint32_t meseta;
  } disp_part1;
  /* 0148 */ struct {
    le_uint32_t num_items;
    // Entries >= num_items in this array contain uninitialized data (usually
    // the contents of a previous sync command)
    parray<PlayerInventoryItem, 0x1E> items;
  } inventory;
  /* 0494 */ le_uint32_t unknown_a15;
};

// 6x71: Unknown (used while loading into game)
// 6x72: Unknown (used while loading into game)
// 6x73: Invalid subcommand (but apparently valid on BB; function is unknown)
// 6x74: Word select
// 6x75: Unknown (supported; game only)
// 6x76: Enemy killed
// 6x77: Sync quest data
// 6x78: Unknown
// 6x79: Lobby 14/15 soccer game
// 6x7A: Unknown
// 6x7B: Unknown
// 6x7C: Unknown (supported; game only; not valid on Episode 3)
// 6x7D: Unknown (supported; game only; not valid on Episode 3)
// 6x7E: Unknown (not valid on Episode 3)
// 6x7F: Unknown (not valid on Episode 3)
// 6x80: Trigger trap (not valid on Episode 3)
// 6x81: Unknown
// 6x82: Unknown
// 6x83: Place trap
// 6x84: Unknown (supported; game only; not valid on Episode 3)
// 6x85: Unknown (supported; game only; not valid on Episode 3)
// 6x86: Hit destructible wall (not valid on Episode 3)
// 6x87: Unknown
// 6x88: Unknown (supported; game only)
// 6x89: Unknown (supported; game only)
// 6x8A: Unknown (not valid on Episode 3)
// 6x8B: Unknown (not valid on Episode 3)
// 6x8C: Unknown (not valid on Episode 3)
// 6x8D: Unknown (supported; lobby & game)
// 6x8E: Unknown (not valid on Episode 3)
// 6x8F: Unknown (not valid on Episode 3)
// 6x90: Unknown (not valid on Episode 3)
// 6x91: Unknown (supported; game only)
// 6x92: Unknown (not valid on Episode 3)
// 6x93: Timed switch activated (not valid on Episode 3)
// 6x94: Warp (not valid on Episode 3)
// 6x95: Unknown (not valid on Episode 3)
// 6x96: Unknown (not valid on Episode 3)
// 6x97: Unknown (not valid on Episode 3)
// 6x98: Unknown
// 6x99: Unknown
// 6x9A: Update player stat (not valid on Episode 3)
// 6x9B: Unknown
// 6x9C: Unknown (supported; game only; not valid on Episode 3)
// 6x9D: Unknown (not valid on Episode 3)
// 6x9E: Unknown (not valid on Episode 3)
// 6x9F: Gal Gryphon actions (not valid on PC or Episode 3)
// 6xA0: Gal Gryphon actions (not valid on PC or Episode 3)
// 6xA1: Unknown (not valid on PC)

// 6xA2: Request for item drop from box (not valid on PC; handled by server on BB)

struct G_BoxItemDropRequest_6xA2 {
  uint8_t command;
  uint8_t size;
  le_uint16_t unused;
  uint8_t area;
  uint8_t unused2;
  le_uint16_t request_id;
  le_float x;
  le_float z;
  le_uint32_t unknown[6];
};

// 6xA3: Episode 2 boss actions (not valid on PC or Episode 3)
// 6xA4: Olga Flow phase 1 actions (not valid on PC or Episode 3)
// 6xA5: Olga Flow phase 2 actions (not valid on PC or Episode 3)
// 6xA6: Modify trade proposal (not valid on PC)
// 6xA7: Unknown (not valid on PC)
// 6xA8: Gol Dragon actions (not valid on PC or Episode 3)
// 6xA9: Barba Ray actions (not valid on PC or Episode 3)
// 6xAA: Episode 2 boss actions (not valid on PC or Episode 3)
// 6xAB: Create lobby chair (not valid on PC)
// 6xAC: Unknown (not valid on PC)
// 6xAD: Unknown (not valid on PC, Episode 3, or GC Trial Edition)
// 6xAE: Set chair state? (sent by existing clients at join time; not valid on PC or GC Trial Edition)
// 6xAF: Turn in lobby chair (not valid on PC or GC Trial Edition)
// 6xB0: Move in lobby chair (not valid on PC or GC Trial Edition)
// 6xB1: Unknown (not valid on PC or GC Trial Edition)
// 6xB2: Unknown (not valid on PC or GC Trial Edition)
// 6xB3: Unknown (XBOX)

// 6xB3: CARD battle command (Episode 3)

// These commands have multiple subcommands; see the Episode 3 subsubcommand
// table after this table. The common format is:
struct G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  uint8_t subcommand; // 0xB3
  uint8_t size;
  le_uint16_t header_w1;
  uint8_t subsubcommand; // See 6xBx subcommand table (after this table)
  uint8_t header_b1;
  // If mask_key is nonzero, the remainder of the data is encrypted using a
  // simple algorithm. See set_mask_for_ep3_game_command in SendCommands.cc for
  // a description of this algorithm.
  uint8_t mask_key;
  uint8_t header_b2;
  // The subsubcommand arguments follow here
};

// 6xB4: Unknown (XBOX)
// 6xB4: CARD battle command (Episode 3) - see 6xB3 (above)
// 6xB5: CARD battle command (Episode 3) - see 6xB3 (above)
// 6xB5: BB shop request (handled by the server)

// 6xB6: Episode 3 map list and map contents (server->client only)
// Unlike 6xB3-6xB5, these commands cannot be masked. Also unlike 6xB3-6xB5,
// there are only two subsubcommands, so we list them inline here.
// These subcommands can be rather large, so they should be sent with the 6C
// command instead of the 60 command.

struct G_MapSubsubcommand_GC_Ep3_6xB6 {
  uint8_t subcommand; // 0xB6
  uint8_t size; // Unused; this command uses the extended (32-bit) size field
  le_uint16_t unused;
  le_uint32_t subcommand_size;
  le_uint32_t subsubcommand; // 0x40 or 0x41
};

struct G_MapList_GC_Ep3_6xB6x40 : G_MapSubsubcommand_GC_Ep3_6xB6 {
  le_uint32_t compressed_data_size;
  // PRS-compressed map list follows (see Ep3DataIndex::get_compressed_map_list)
};

struct G_MapData_GC_Ep3_6xB6x41 : G_MapSubsubcommand_GC_Ep3_6xB6 {
  le_int16_t map_number;
  le_uint16_t unused;
  le_uint32_t compressed_data_size;
  // PRS-compressed map data follows (which decompresses to an Ep3Map)
};

// 6xB6: BB shop contents (server->client only)

struct G_ShopContents_BB_6xB6 {
  uint8_t subcommand;
  uint8_t size; // always 2C regardless of the number of items??
  le_uint16_t params; // 037F
  uint8_t shop_type;
  uint8_t num_items;
  le_uint16_t unused;
  // Note: data2d of these entries should be the price
  ItemData entries[20];
};

// 6xB7: Unknown (Episode 3 Trial Edition)
// 6xB7: BB buy shop item (handled by the server)

struct G_BuyShopItem_BB_6xB7 {
  uint8_t subcommand;
  uint8_t size;
  le_uint16_t unused;
  le_uint32_t inventory_item_id;
  uint8_t shop_type;
  uint8_t item_index;
  uint8_t amount;
  uint8_t unknown_a1; // TODO: Probably actually unused; verify this
};

// 6xB8: Unknown (Episode 3 Trial Edition)
// 6xB8: BB accept tekker result (handled by the server)
// Format is G_ItemIDSubcommand

// 6xB9: Unknown (Episode 3 Trial Edition)
// 6xB9: BB provisional tekker result

struct G_IdentifyResult_BB_6xB9 {
  uint8_t subcommand;
  uint8_t size;
  le_uint16_t client_id;
  ItemData item;
};

// 6xBA: Unknown (Episode 3)

// 6xBA: BB accept tekker result (handled by the server)
// Format is G_ItemIDSubcommand

// 6xBB: Unknown (Episode 3)
// 6xBB: BB bank request (handled by the server)
// 6xBC: Unknown (Episode 3)
// 6xBC: BB bank contents (server->client only)

struct G_BankContentsHeader_BB_6xBC {
  uint8_t subcommand;
  uint8_t unused1;
  le_uint16_t unused2;
  le_uint32_t size; // same as size in overall command header
  le_uint32_t checksum; // can be random; client won't notice
  le_uint32_t numItems;
  le_uint32_t meseta;
  // Item data follows
};

// 6xBD: Unknown (Episode 3; not Trial Edition)
// 6xBD: BB bank action (take/deposit meseta/item) (handled by the server)

struct G_BankAction_BB_6xBD {
  uint8_t subcommand;
  uint8_t size;
  le_uint16_t unused;
  le_uint32_t item_id; // 0xFFFFFFFF = meseta; anything else = item
  le_uint32_t meseta_amount;
  uint8_t action; // 0 = deposit, 1 = take
  uint8_t item_amount;
  le_uint16_t unused2;
};

// 6xBE: Sound chat (Episode 3; not Trial Edition)
// 6xBE: BB create inventory item (server->client only)

struct G_CreateInventoryItem_BB_6xBE {
  uint8_t subcommand;
  uint8_t size;
  le_uint16_t client_id;
  ItemData item;
  le_uint32_t unused;
};

// 6xBF: Change lobby music (Episode 3; not Trial Edition)
// 6xBF: Give EXP (BB) (server->client only)

struct G_GiveExperience_BB_6xBF {
  uint8_t subcommand;
  uint8_t size;
  le_uint16_t client_id;
  le_uint32_t amount;
};

// 6xC0: BB sell item at shop
// Format is G_ItemSubcommand

// 6xC1: Unknown
// 6xC2: Unknown

// 6xC3: Split stacked item (handled by the server on BB)
// Note: This is not sent if an entire stack is dropped; in that case, a normal
// item drop subcommand is generated instead.

struct G_SplitStackedItem_6xC3 {
  uint8_t command;
  uint8_t size;
  le_uint16_t client_id;
  le_uint16_t area;
  le_uint16_t unused2;
  le_float x;
  le_float z;
  le_uint32_t item_id;
  le_uint32_t amount;
};

// 6xC4: Sort inventory (handled by the server on BB)

struct G_SortInventory_6xC4 {
  uint8_t command;
  uint8_t size;
  le_uint16_t unused;
  le_uint32_t item_ids[30];
};

// 6xC5: Medical center used
// 6xC6: Invalid subcommand
// 6xC7: Invalid subcommand

// 6xC8: Enemy killed (handled by the server on BB)

struct G_EnemyKilled_6xC8 {
  uint8_t command;
  uint8_t size;
  le_uint16_t enemy_id2;
  le_uint16_t enemy_id;
  le_uint16_t killer_client_id;
  le_uint32_t unused;
};

// 6xC9: Invalid subcommand
// 6xCA: Invalid subcommand
// 6xCB: Unknown
// 6xCC: Unknown
// 6xCD: Unknown
// 6xCE: Unknown
// 6xCF: Unknown (supported; game only; handled by the server on BB)
// 6xD0: Invalid subcommand
// 6xD1: Invalid subcommand
// 6xD2: Unknown
// 6xD3: Invalid subcommand
// 6xD4: Unknown
// 6xD5: Invalid subcommand
// 6xD6: Invalid subcommand
// 6xD7: Invalid subcommand
// 6xD8: Invalid subcommand
// 6xD9: Invalid subcommand
// 6xDA: Invalid subcommand
// 6xDB: Unknown
// 6xDC: Unknown
// 6xDD: Unknown
// 6xDE: Invalid subcommand
// 6xDF: Invalid subcommand
// 6xE0: Invalid subcommand
// 6xE1: Invalid subcommand
// 6xE2: Invalid subcommand
// 6xE3: Unknown
// 6xE4: Invalid subcommand
// 6xE5: Invalid subcommand
// 6xE6: Invalid subcommand
// 6xE7: Invalid subcommand
// 6xE8: Invalid subcommand
// 6xE9: Invalid subcommand
// 6xEA: Invalid subcommand
// 6xEB: Invalid subcommand
// 6xEC: Invalid subcommand
// 6xED: Invalid subcommand
// 6xEE: Invalid subcommand
// 6xEF: Invalid subcommand
// 6xF0: Invalid subcommand
// 6xF1: Invalid subcommand
// 6xF2: Invalid subcommand
// 6xF3: Invalid subcommand
// 6xF4: Invalid subcommand
// 6xF5: Invalid subcommand
// 6xF6: Invalid subcommand
// 6xF7: Invalid subcommand
// 6xF8: Invalid subcommand
// 6xF9: Invalid subcommand
// 6xFA: Invalid subcommand
// 6xFB: Invalid subcommand
// 6xFC: Invalid subcommand
// 6xFD: Invalid subcommand
// 6xFE: Invalid subcommand
// 6xFF: Invalid subcommand



// Now, the Episode 3 CARD battle subsubcommands (used in commands 6xB3, 6xB4,
// and 6xB5). Note that even though there's no overlap in the subsubcommand
// number space, the various subsubcommands must be used with the correct 6xBx
// subcommand - the client will ignore the command if sent via the wrong 6xBx
// subcommand. Unlike the above listings, invalid commands are not listed here,
// since this table is known to be complete.

struct DeckCardRef { // Note: The game treats these as le_uint16_ts
  uint8_t deck_index;
  uint8_t client_id;
};

// 6xB4x02: Update hands and equips

struct G_UpdateHand_GC_Ep3_6xB4x02 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  le_uint16_t client_id;
  le_uint16_t unused3;
  parray<uint8_t, 8> unknown_a1; // Dice values are in here somewhere
  le_uint32_t unknown_a2;
  // Empty slots in all of these arrays should be set to FFFF
  parray<DeckCardRef, 6> cards_in_hand;
  le_uint16_t unknown_a3;
  parray<DeckCardRef, 8> cards_equipped;
  // Note: The order of entries in this field matches the order of entries in
  // 6xB4x04's entries list (and the card refs should match exactly). The first
  // entry here is always the SC card (ref with deck_index=0).
  parray<DeckCardRef, 16> unknown_a4;
  parray<le_uint16_t, 2> unknown_a5; // {0, 0xFFFF} always?
  parray<uint8_t, 6> unknown_a6; // {0, 0, 0, 0, 0x08, 0x0D} always?
};

// 6xB4x03: Set state flags

struct G_SetStateFlags_GC_Ep3_6xB4x03 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  le_uint16_t unknown_a1;
  parray<uint8_t, 6> unknown_a2;
  parray<le_uint32_t, 2> unknown_a3;
  be_uint16_t unknown_a4;
  uint8_t unknown_a5;
  // If tournament_flag is 1, the player will start at the counter instead of in
  // the default position the next time they join a game, and they will be
  // unable to leave the counter menu.
  uint8_t tournament_flag;
  be_uint32_t unknown_a6;
};

// 6xB4x04: Update SC/FC stats

struct G_UpdateStats_GC_Ep3_6xB4x04 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  le_uint16_t client_id;
  le_uint16_t unused;
  struct Entry {
    DeckCardRef card; // FFFF if this entry is unused (and all other fields are 0)
    le_uint16_t hp;
    le_uint32_t unknown_a3;
    uint8_t card_hp;
    uint8_t card_tp;
    uint8_t card_ap;
    uint8_t card_mv;
    le_uint16_t unknown_a5;
    le_uint16_t hp2; // Seems unused by the client, but Sega duplicated HP here
  };
  Entry entries[0x10];
};

// 6xB4x05: Update map state

struct G_UpdateMap_GC_Ep3_6xB4x05 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  le_uint16_t width;
  le_uint16_t height;
  parray<uint8_t, 0x100> tiles;
  parray<uint8_t, 0x0C> unknown_a1;
  uint8_t num_players;
  uint8_t unknown_a2;
  uint8_t unknown_a3; // Handler logic branches on this
  parray<uint8_t, 0x03> unknown_a4;
  le_uint16_t unknown_a5;
  // 120 from subcommand start
  parray<uint8_t, 0x04> unknown_a6;
  le_uint32_t map_number;
  le_uint32_t unknown_a7; // Handler logic branches on this
  Ep3BattleRules rules;
  parray<uint8_t, 4> unknown_a8;
  uint8_t unknown_a9; // Handler logic branches on this
  parray<uint8_t, 3> unknown_a10;
};

// 6xB4x06: Unknown

struct G_Unknown_GC_Ep3_6xB4x06 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  le_uint16_t unknown_a1;
  le_uint16_t unknown_a2;
  parray<uint8_t, 8> unknown_a3;
};

// 6xB4x07: Update decks

struct G_UpdateDecks_GC_Ep3_6xB4x07 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  parray<uint8_t, 4> unknown_a1;
  struct Entry {
    ptext<char, 0x10> player_name;
    uint8_t present;
    parray<uint8_t, 3> unknown_a1;
    parray<le_uint16_t, 0x1F> card_ids;
    parray<uint8_t, 2> unknown_a2;
    le_uint16_t unknown_a3;
    be_uint16_t unknown_a4;
  };
  Entry entries[4];
};

// 6xB4x09: Unknown

struct G_Unknown_GC_Ep3_6xB4x09 {
  le_uint16_t unknown_a1; // Probably client_id; client ignores command if < 0
  parray<uint8_t, 2> unknown_a2;
  le_uint16_t unknown_a3; // Probably client_id
  be_uint16_t unknown_a4;
  le_uint16_t unknown_a5;
  le_uint16_t unknown_a6;
  parray<le_uint16_t, 0x24> unknown_a7;
  parray<le_uint16_t, 8> unknown_a8;
  be_uint16_t unknown_a9;
  le_uint16_t unknown_a10;
};

// 6xB4x0A: Unknown

struct G_Unknown_GC_Ep3_6xB4x0A : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  le_uint16_t client_id;
  int8_t unknown_a1; // must be between -1 and 8 inclusive
  uint8_t unknown_a2;

  parray<uint8_t, 4> unknown_a3;
  le_uint16_t unknown_a4;
  le_uint16_t unknown_a5;
  parray<le_uint16_t, 8> unknown_a6;
  parray<uint8_t, 0x0C> unknown_a7;
  le_uint32_t unknown_a8;
  parray<le_uint16_t, 0x24> unknown_a9;
  struct Entry {
    parray<uint8_t, 6> unknown_a1;
    parray<le_uint16_t, 3> unknown_a2;
    parray<uint8_t, 4> unknown_a3;
  };
  Entry entries[9];

  le_uint16_t unknown_a10;
  parray<uint8_t, 6> unknown_a11;
  le_uint32_t unknown_a12;
  parray<le_uint16_t, 0x24> unknown_a13;
  parray<le_uint16_t, 8> unknown_a14;
  parray<le_uint16_t, 8> unknown_a15;
};

// 6xB3x0B: Unknown

struct G_Unknown_GC_Ep3_6xB3x0B : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  parray<uint8_t, 8> unknown_a1;
  le_uint16_t client_id;
  parray<uint8_t, 2> unknown_a2;
  // Command may be longer than this structure
};

// 6xB3x0C: Unknown

struct G_Unknown_GC_Ep3_6xB3x0C : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  parray<uint8_t, 8> unknown_a1;
  le_uint16_t client_id;
  parray<uint8_t, 2> unknown_a2;
  // Command may be longer than this structure
};

// 6xB3x0D: Unknown

struct G_Unknown_GC_Ep3_6xB3x0D : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  be_uint32_t unknown_a1;
  parray<uint8_t, 4> unknown_a2;
  le_uint16_t client_id;
  parray<le_uint16_t, 5> unknown_a3;
  // Command may be longer than this structure
};

// 6xB3x0E: Unknown

struct G_Unknown_GC_Ep3_6xB3x0E : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  parray<uint8_t, 8> unknown_a1;
  le_uint16_t client_id;
  le_uint16_t unknown_a2;
  // Command may be longer than this structure
};

// 6xB3x0F: Unknown

struct G_Unknown_GC_Ep3_6xB3x0F : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  parray<uint8_t, 8> unknown_a1;
  le_uint16_t client_id;
  parray<le_uint16_t, 3> unknown_a2;
  parray<uint8_t, 2> unknown_a3;
  parray<uint8_t, 2> unused;
  // Command may be longer than this structure
};

// 6xB3x10: Unknown

struct G_Unknown_GC_Ep3_6xB3x10 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  parray<uint8_t, 8> unknown_a1;
  le_uint16_t client_id;
  le_uint16_t unknown_a2;
  // Note: This field's type is the same as the corresponding field in 6xB3x0F
  parray<uint8_t, 2> unknown_a3;
  parray<uint8_t, 2> unused;
  // Command may be longer than this structure
};

// 6xB3x11: Unknown

struct G_Unknown_GC_Ep3_6xB3x11 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  parray<uint8_t, 8> unknown_a1;
  le_uint16_t client_id;
  parray<uint8_t, 2> unknown_a2;

  // Note: This is byteswapped by the same function as the corresponding part of
  // 6xB4x29
  le_uint16_t unknown_a3;
  parray<uint8_t, 2> unknown_a4;
  le_uint16_t unknown_a5;
  le_uint16_t unknown_a6;
  parray<le_uint16_t, 0x24> unknown_a7;
  parray<le_uint16_t, 8> unknown_a8;
  parray<uint8_t, 2> unknown_a9;
  le_uint16_t unknown_a10;
};

// 6xB3x12: Unknown

struct G_Unknown_GC_Ep3_6xB3x12 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  be_uint32_t unknown_a1;
  parray<uint8_t, 4> unknown_a2;
  le_uint16_t client_id;
  parray<uint8_t, 2> unknown_a3;
};

// 6xB3x13: Unknown

struct G_Unknown_GC_Ep3_6xB3x13 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  parray<uint8_t, 8> unknown_a1;

  // Note: This section's format is a guess based on the fact that the client
  // calls the same function to byteswap it as for the update map command above.
  le_uint16_t width;
  le_uint16_t height;
  parray<uint8_t, 0x100> tiles;
  parray<uint8_t, 0x0C> unknown_a2;
  uint8_t num_players;
  uint8_t unknown_a3;
  uint8_t unknown_a4;
  parray<uint8_t, 0x03> unknown_a5;
  le_uint16_t unknown_a6;
  // 120 from subcommand start
  parray<uint8_t, 0x04> unknown_a7;
  le_uint32_t map_number;
  le_uint32_t unknown_a8;
  Ep3BattleRules rules;
  parray<uint8_t, 4> unknown_a9;

  parray<le_uint32_t, 5> unknown_a10;
  parray<le_uint32_t, 0x10> unknown_a11;
  parray<le_uint16_t, 0x10> unknown_a12;
};

// 6xB3x14: Unknown

struct G_Unknown_GC_Ep3_6xB3x14 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  parray<uint8_t, 8> unknown_a1;
  le_uint16_t client_id;
  parray<uint8_t, 2> unknown_a2;
  parray<uint8_t, 0x10> name;
  parray<le_uint16_t, 0x1F> card_ids;
  parray<uint8_t, 2> unknown_a3;
  le_uint16_t unknown_a4;
  parray<uint8_t, 2> unknown_a5;
};

// 6xB3x15: Unknown

struct G_Unknown_GC_Ep3_6xB3x15 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // No arguments
};

// 6xB5x17: Unknown

struct G_Unknown_GC_Ep3_6xB5x17 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // No arguments
};

// 6xB5x1A: Unknown

struct G_Unknown_GC_Ep3_6xB5x1A : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // No arguments
};

// 6xB3x1B: Unknown

struct G_Unknown_GC_Ep3_6xB3x1B : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // The handler reads a byte from offset 0x20 after the subcommand start (which
  // would be offset 0x18 in this struct), so the command must be at least 0x1C
  // bytes in total. It is unlikely to be longer, but I have no examples.
  parray<uint8_t, 0x1C> unknown_a1;
};

// 6xB4x1C: Set player names

struct G_Unknown_GC_Ep3_6xB4x1C : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  struct Entry {
    ptext<char, 0x10> name;
    uint8_t client_id; // 0xFF if slot is empty
    uint8_t present; // 1 if slot is occupied; 0 if empty
    parray<uint8_t, 2> unused;
  };
  Entry entries[4];
};

// 6xB3x1D: Unknown

struct G_Unknown_GC_Ep3_6xB3x1D : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // No arguments
};

// 6xB4x1E: Unknown

struct G_Unknown_GC_Ep3_6xB4x1E : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  be_uint32_t unknown_a1;
  uint8_t unknown_a2;
  uint8_t unknown_a3;
  parray<uint8_t, 2> unused;
};

// 6xB4x1F: Unknown

struct G_Unknown_GC_Ep3_6xB4x1F : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  le_uint32_t unknown_a1;
};

// 6xB5x20: Unknown

struct G_Unknown_GC_Ep3_6xB5x20 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  le_uint32_t player_tag;
  le_uint32_t guild_card_number;
  uint8_t client_id;
  parray<uint8_t, 3> unused;
};

// 6xB3x21: Unknown

struct G_Unknown_GC_Ep3_6xB3x21 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // No arguments
};

// 6xB4x22: Unknown

struct G_Unknown_GC_Ep3_6xB4x22 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // No arguments
};

// 6xB4x23: Unknown

struct G_Unknown_GC_Ep3_6xB4x23 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // This command does nothing at all. If unknown_a1 == 1, then it calls another
  // function, but that function also does nothing.
  uint8_t unknown_a1;
  uint8_t unknown_a2;
  parray<uint8_t, 2> unused;
};

// 6xB5x27: Unknown

struct G_Unknown_GC_Ep3_6xB5x27 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // Note: This command uses header_b1 as well, which looks like another client
  // ID (it must be < 4, though it does not always match unknown_a1 below).
  le_uint32_t unknown_a1; // Probably client ID (must be < 4)
  le_uint32_t unknown_a2; // Must be < 0x10
  le_uint32_t unknown_a3;
  le_uint32_t unused; // Curiously, this usually contains a memory address
};

// 6xB3x28: Unknown

struct G_Unknown_GC_Ep3_6xB3x28 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  be_uint32_t unknown_a1;
  parray<uint8_t, 5> unused1;
  int8_t unknown_a2;
  parray<uint8_t, 2> unused2;
};

// 6xB4x29: Unknown

struct G_Unknown_GC_Ep3_6xB4x29 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  parray<uint8_t, 4> unknown_a1;

  // Note: This is byteswapped by the same function as the corresponding part of
  // 6xB3x11
  le_uint16_t unknown_a2;
  parray<uint8_t, 2> unknown_a3;
  le_uint16_t unknown_a4;
  le_uint16_t unknown_a5;
  parray<le_uint16_t, 0x24> unknown_a6;
  parray<le_uint16_t, 8> unknown_a7;
  parray<uint8_t, 2> unknown_a8;
  le_uint16_t unknown_a9;
};

// 6xB4x2A: Unknown

struct G_Unknown_GC_Ep3_6xB4x2A : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  parray<uint8_t, 4> unknown_a1;
  le_uint16_t unknown_a2;
  parray<uint8_t, 2> unused;
};

// 6xB3x2B: Unknown

struct G_Unknown_GC_Ep3_6xB3x2B : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // This command is completely ignored by the client. Its structure (if any) is
  // unknown.
};

// 6xB4x2C: Unknown

struct G_Unknown_GC_Ep3_6xB4x2C : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  uint8_t unknown_a1;
  uint8_t unknown_a2; // Only used if the preceding field == 3
  parray<le_uint16_t, 3> unknown_a3;
  parray<uint8_t, 4> unknown_a4;
  parray<le_uint32_t, 2> unknown_a5;
};

// 6xB5x2D: Unknown

struct G_Unknown_GC_Ep3_6xB5x2D : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // This array is indexed into by a global variable. I don't have any examples
  // of this command, so I don't know how long the array should be - 4 is a
  // probably-incorrect guess.
  parray<uint8_t, 4> unknown_a1;
};

// 6xB5x2E: Unknown

struct G_Unknown_GC_Ep3_6xB5x2E : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  uint8_t unknown_a1; // Command ignored unless this is 0 or 1
  parray<uint8_t, 3> unused;
};

// 6xB5x2F: Unknown

struct G_Unknown_GC_Ep3_6xB5x2F : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  parray<uint8_t, 4> unknown_a1;

  parray<uint8_t, 0x18> unknown_a2;
  ptext<uint8_t, 0x10> deck_name;
  parray<uint8_t, 0x0E> unknown_a3;
  le_uint16_t unknown_a4;
  parray<le_uint32_t, 0x1F> card_ids;
  parray<uint8_t, 2> unused;
  le_uint32_t unknown_a5;
  le_uint16_t unknown_a6;
  le_uint16_t unknown_a7;
};

// 6xB5x30: Unknown

struct G_Unknown_GC_Ep3_6xB5x30 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // No arguments
};

// 6xB5x31: Unknown

struct G_Unknown_GC_Ep3_6xB5x31 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // Note: This command uses header_b1 for... something.
  uint8_t unknown_a1; // Must be 0 or 1
  uint8_t unknown_a2; // Must be < 4
  uint8_t unknown_a3; // Must be < 4
  uint8_t unknown_a4; // Must be < 0x14
  uint8_t unknown_a5; // Used as an array index
  parray<uint8_t, 3> unused;
};

// 6xB5x32: Unknown

struct G_Unknown_GC_Ep3_6xB5x32 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // Note: This command uses header_b1 for... something.
  le_uint16_t unknown_a1;
  le_uint16_t unknown_a2;
  parray<uint8_t, 8> unknown_a3;
};

// 6xB4x33: Unknown

struct G_Unknown_GC_Ep3_6xB4x33 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  uint8_t unknown_a1;
  uint8_t unused;
  le_uint16_t unknown_a2;
};

// 6xB3x34: Unknown

struct G_Unknown_GC_Ep3_6xB3x34 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  parray<uint8_t, 8> unknown_a1;
  uint8_t client_id;
  uint8_t unknown_a2;
  le_uint16_t unknown_a3; // Possibly DeckCardRef
};

// 6xB4x35: Unknown

struct G_Unknown_GC_Ep3_6xB4x35 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  uint8_t unknown_a1;
  uint8_t unknown_a2;
  le_uint16_t unknown_a3;
};

// 6xB5x36: Unknown

struct G_Unknown_GC_Ep3_6xB5x36 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  uint8_t unknown_a1; // Must be < 12 (maybe lobby or spectator team client ID)
  parray<uint8_t, 3> unused;
};

// 6xB3x37: Unknown

struct G_Unknown_GC_Ep3_6xB3x37 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  parray<uint8_t, 8> unused1;
  uint8_t unknown_a1;
  parray<uint8_t, 3> unused2;
};

// 6xB5x38: Unknown

struct G_Unknown_GC_Ep3_6xB5x38 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  uint8_t unknown_a1;
  uint8_t unknown_a2;
  parray<uint8_t, 2> unused;
};

// 6xB4x39: Unknown

struct G_Unknown_GC_Ep3_6xB4x39 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  struct Entry {
    parray<le_uint16_t, 0x13> unknown_a1;
    parray<uint8_t, 2> unknown_a2;
  };
  Entry entries[4];
};

// 6xB3x3A: Unknown

struct G_Unknown_GC_Ep3_6xB3x3A : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // This command is completely ignored by the client. Its structure (if any) is
  // unknown.
};

// 6xB4x3B: Unknown

struct G_Unknown_GC_Ep3_6xB4x3B : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  parray<uint8_t, 4> unused;
};

// 6xB5x3C: Unknown

struct G_Unknown_GC_Ep3_6xB5x3C : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // Note: This command uses header_b1 for... something.
  uint8_t unknown_a1;
  parray<uint8_t, 3> unused;
};

// 6xB4x3D: Unknown

struct G_Unknown_GC_Ep3_6xB4x3D : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  parray<uint8_t, 0x14> unknown_a1;
  struct Entry {
    uint8_t type; // 1 = human, 2 = COM
    ptext<char, 0x10> player_name;
    ptext<char, 0x10> deck_name; // Seems to only be used for COM players
    parray<uint8_t, 5> unknown_a1;
    parray<le_uint16_t, 0x1F> card_ids;
    parray<uint8_t, 2> unused;
    le_uint16_t unknown_a2;
    le_uint16_t unknown_a3;
  };
  Entry entries[4];
  le_uint32_t map_number;
  uint8_t unknown_a2;
  uint8_t unknown_a3;
  uint8_t unknown_a4;
  uint8_t unknown_a5;
};

// 6xB5x3E: Unknown

struct G_Unknown_GC_Ep3_6xB5x3E : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // Note: This command uses header_b1 for... something.
  uint8_t unknown_a1;
  uint8_t unknown_a2;
  parray<uint8_t, 2> unused;
};

// 6xB5x3F: Unknown

struct G_Unknown_GC_Ep3_6xB5x3F : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  int8_t unknown_a1; // Must be in the range [-1, 0x14]
  uint8_t unknown_a2; // Must be < 4
  parray<uint8_t, 2> unused1;
  le_uint32_t unknown_a3;
  parray<uint8_t, 4> unused2;
};

// 6xB3x40: Unknown

struct G_Unknown_GC_Ep3_6xB3x40 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // This command is completely ignored by the client. Its structure (if any) is
  // unknown. It's possible that this command was replaced by 6xB6x40.
};

// 6xB3x41: Unknown

struct G_Unknown_GC_Ep3_6xB3x41 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // This command is completely ignored by the client. Its structure (if any) is
  // unknown. It's possible that this command was replaced by 6xB6x41.
};

// 6xB5x42: Unknown

struct G_Unknown_GC_Ep3_6xB5x42 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // This command uses header_b1, but has no other arguments.
};

// 6xB5x43: Unknown

struct G_Unknown_GC_Ep3_6xB5x43 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  struct Entry {
    // Both fields here are masked. To get the actual values used by the game,
    // XOR the values here with 0x39AB.
    le_uint16_t masked_card_id; // Must be < 0x2F1 (when unmasked)
    le_uint16_t masked_unknown_a1; // Must be in [1, 99] (when unmasked)
  };
  Entry entries[0x14];
};

// 6xB5x44: Unknown

struct G_Unknown_GC_Ep3_6xB5x44 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // Note: This command uses header_b1, which must be < 4.
  parray<le_uint16_t, 8> unknown_a1;
};

// 6xB5x45: Unknown

struct G_Unknown_GC_Ep3_6xB5x45 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // Note: This command uses header_b1, which must be < 4.
  parray<parray<le_uint16_t, 4>, 8> unknown_a1;
};

// 6xB4x46: Start battle

struct G_Unknown_GC_Ep3_6xB4x46 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // In all of the examples (from Sega's servers) that I've seen of this
  // command, these fields have the following values:
  // version_signature = "[V1][FINAL2.0] 03/09/13 15:30 by K.Toya"
  // date_str1 = "Mar  7 2007 21:42:40"
  // date_str2 = "2005/03/04 17:31:59"
  ptext<char, 0x40> version_signature;
  ptext<char, 0x40> date_str1; // Possibly card definitions revision date
  ptext<char, 0x40> date_str2; // Possibly engine build date
  le_uint32_t unused;
};

// 6xB5x47: Unknown

struct G_Unknown_GC_Ep3_6xB5x47 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // Note: This command uses header_b1, which must be < 12.
  le_uint32_t unknown_a1;
};

// 6xB3x48: Unknown

struct G_Unknown_GC_Ep3_6xB3x48 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  be_uint32_t unknown_a1;
  parray<uint8_t, 4> unused1;
  uint8_t unknown_a2;
  parray<uint8_t, 3> unused2;
};

// 6xB3x49: Unknown

struct G_Unknown_GC_Ep3_6xB3x49 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // This command is completely ignored by the client. Its structure (if any) is
  // unknown. It's possible that this command was replaced by 6xB6x41.
};

// 6xB4x4A: Unknown

struct G_Unknown_GC_Ep3_6xB4x4A : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // Note: entry_count appears not to be bounds-checked; presumably the server
  // could send up to 0xFF entries, but those after the 8th would not be
  // byteswapped before the client handles them.
  uint8_t unknown_a1;
  uint8_t entry_count;
  le_uint16_t unknown_a2;
  parray<le_uint16_t, 8> entries;
};

// 6xB4x4B: Unknown

struct G_Unknown_GC_Ep3_6xB4x4B : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // If any of the entries [0][1] through [0][10] or [1][1] through [1][10] are
  // < -99 or > 99, the entire command is ignored.
  parray<parray<le_int16_t, 20>, 2> unknown_a1;
};

// 6xB4x4C: Unknown

struct G_Unknown_GC_Ep3_6xB4x4C : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  uint8_t unknown_a1; // Must be < 4
  int8_t unknown_a2; // Must be in [-1, 8]
  parray<uint8_t, 2> unknown_a3;

  parray<uint8_t, 4> unknown_a4;
  le_uint16_t unknown_a5;
  le_uint16_t unknown_a6;
  parray<le_uint16_t, 8> unknown_a7;
  parray<uint8_t, 0x0C> unknown_a8;
  le_uint32_t unknown_a9;
  parray<le_uint16_t, 0x24> unknown_a10;
};

// 6xB4x4D: Unknown

struct G_Unknown_GC_Ep3_6xB4x4D : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  uint8_t unknown_a1; // Must be < 4
  int8_t unknown_a2; // Must be in [-1, 8]
  parray<uint8_t, 2> unknown_a3;

  le_uint16_t unknown_a4;
  parray<uint8_t, 6> unknown_a5;
  le_uint32_t unknown_a6;
  parray<le_uint16_t, 0x24> unknown_a7;
  parray<le_uint16_t, 8> unknown_a8;
  parray<le_uint16_t, 8> unknown_a9;
};

// 6xB4x4E: Unknown

struct G_Unknown_GC_Ep3_6xB4x4E : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  uint8_t unknown_a1; // Must be < 4
  int8_t unknown_a2; // Must be in [-1, 8]
  parray<uint8_t, 2> unknown_a3;
  struct Entry {
    parray<uint8_t, 6> unknown_a1;
    parray<le_uint16_t, 3> unknown_a2;
    parray<uint8_t, 4> unknown_a3;
  };
  Entry entries[9];
};

// 6xB4x4F: Unknown

struct G_Unknown_GC_Ep3_6xB4x4F : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  uint8_t unknown_a1;
  uint8_t unused;
  le_uint16_t unknown_a2; // Bitmask; the 9 low-order bits are used
};

// 6xB4x50: Unknown

struct G_Unknown_GC_Ep3_6xB4x50 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  parray<uint8_t, 0x0C> unknown_a1;
};

// 6xB4x51: Tournament match info

struct G_Unknown_GC_Ep3_6xB4x51 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  ptext<char, 0x40> match_description;
  struct Entry {
    ptext<char, 0x20> team_name;
    parray<ptext<char, 0x10>, 2> player_names;
  };
  Entry teams[2];
  le_uint16_t unknown_a1;
  le_uint16_t unknown_a2;
  le_uint16_t unknown_a3;
  le_uint16_t unknown_a4;
  le_uint32_t meseta_amount;
  // This field apparently is supposed to contain a %s token (as for printf)
  // that is replaced with meseta_amount.
  ptext<char, 0x20> meseta_reward_text;
};

// 6xB4x52: Unknown

struct G_Unknown_GC_Ep3_6xB4x52 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  le_uint16_t unknown_a1;
  le_uint16_t unknown_a2;
  le_uint16_t unknown_a3;
  le_uint16_t size; // Number of valid bytes in the data field (clamped to 0xFF)
  parray<uint8_t, 0x100> data;
};

// 6xB4x53: Unknown

struct G_Unknown_GC_Ep3_6xB4x53 : G_CardBattleCommandHeader_GC_Ep3_6xB3_6xB4_6xB5 {
  // Note: It seems the client ignores everything in this structure. The command
  // just sets a global state flag, then returns immediately.

  parray<uint8_t, 4> unknown_a1;

  le_uint16_t width;
  le_uint16_t height;
  parray<uint8_t, 0x100> tiles;
  parray<uint8_t, 0x0C> unknown_a2;
  uint8_t num_players;
  uint8_t unknown_a3;
  uint8_t unknown_a4;
  parray<uint8_t, 0x03> unknown_a5;
  le_uint16_t unknown_a6;
  parray<uint8_t, 0x04> unknown_a7;
  le_uint32_t map_number;
  // Comamnd may be larger than this structure
};



#pragma pack(pop)
