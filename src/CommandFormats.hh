#pragma once

#include <phosg/Encoding.hh>
#include <phosg/Strings.hh>
#include <stdexcept>
#include <string>

#include "Episode3/DataIndex.hh"
#include "Episode3/DeckState.hh"
#include "Episode3/MapState.hh"
#include "Episode3/PlayerStateSubordinates.hh"
#include "PSOProtocol.hh"
#include "Player.hh"
#include "Text.hh"

#define __packed__ __attribute__((packed))

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

// Text escape codes

// Most text fields allow the use of various escape codes to change decoding,
// change color, or create symbols. These escape codes are always preceded by a
// tab character (0x09, or '\t'). For brevity, we generally refer to them with $
// instead in newserv, since the server substitutes most usage of $ in player-
// provided text with \t. The escape codes are:
// - Language codes
// - - $E: Set text interpretation to English
// - - $J: Set text interpretation to Japanese
// - Color codes
// - - $C0: Black (000000)
// - - $C1: Blue (0000FF)
// - - $C2: Green (00FF00)
// - - $C3: Cyan (00FFFF)
// - - $C4: Red (FF0000)
// - - $C5: Magenta (FF00FF)
// - - $C6: Yellow (FFFF00)
// - - $C7: White (FFFFFF)
// - - $C8: Pink (FF8080)
// - - $C9: Violet (8080FF)
// - - $CG: Orange pulse (FFE000 + darkenings thereof)
// - - $Ca: Orange (F5A052; Episode 3 only)
// - Special character codes (Ep3 only)
// - - $B: Dash + small bullet
// - - $D: Large bullet
// - - $F: Female symbol
// - - $I: Infinity
// - - $M: Male symbol
// - - $O: Open circle
// - - $R: Solid circle
// - - $S: Star-like ability symbol
// - - $X: Cross
// - - $d: Down arrow
// - - $l: Left arrow
// - - $r: Right arrow
// - - $u: Up arrow

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
  uint32_t flags;
  uint32_t specific_version;
  uint32_t proxy_destination_address;
  uint16_t proxy_destination_port;
  parray<uint8_t, 0x0A> unused;
} __packed__;

struct ClientConfigBB {
  ClientConfig cfg;
  uint8_t bb_game_state;
  uint8_t bb_player_index;
  parray<uint8_t, 0x06> unused;
} __packed__;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// PATCH SERVER COMMANDS ///////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

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
  le_uint32_t server_key = 0; // Key for commands sent by server
  le_uint32_t client_key = 0; // Key for commands sent by client
  // The client rejects the command if it's larger than this size, so we can't
  // add the after_message like we do in the other server init commands.
} __packed__;

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
} __packed__;

// 05 (S->C): Disconnect
// No arguments
// This command is not used in the normal flow (described above). Generally the
// server should disconnect after sending a 12 or 15 command instead of an 05.

// 06 (S->C): Open file for writing

struct S_OpenFile_Patch_06 {
  le_uint32_t unknown_a1 = 0;
  le_uint32_t size = 0;
  ptext<char, 0x30> filename;
} __packed__;

// 07 (S->C): Write file
// The client's handler table says this command's maximum size is 0x6010
// including the header, but the only servers I've seen use this command limit
// chunks to 0x4010 (including the header). Unlike the game server's 13 and A7
// commands, the chunks do not need to be the same size - the game opens the
// file with the "a+b" mode each time it is written, so the new data is always
// appended to the end.

struct S_WriteFileHeader_Patch_07 {
  le_uint32_t chunk_index = 0;
  le_uint32_t chunk_checksum = 0; // CRC32 of the following chunk data
  le_uint32_t chunk_size = 0;
  // The chunk data immediately follows here
} __packed__;

// 08 (S->C): Close current file
// The unused field is optional. It's not clear whether this field was ever
// used; it could be a remnant from pre-release testing, or someone could have
// simply set the maximum size of this command incorrectly.

struct S_CloseCurrentFile_Patch_08 {
  le_uint32_t unused = 0;
} __packed__;

// 09 (S->C): Enter directory

struct S_EnterDirectory_Patch_09 {
  ptext<char, 0x40> name;
} __packed__;

// 0A (S->C): Exit directory
// No arguments

// 0B (S->C): Start patch session and go to patch root directory
// No arguments

// 0C (S->C): File checksum request

struct S_FileChecksumRequest_Patch_0C {
  le_uint32_t request_id = 0;
  ptext<char, 0x20> filename;
} __packed__;

// 0D (S->C): End of file checksum requests
// No arguments

// 0E: Invalid command

// 0F (C->S): File information

struct C_FileInformation_Patch_0F {
  le_uint32_t request_id = 0; // Matches request_id from an earlier 0C command
  le_uint32_t checksum = 0; // CRC32 of the file's data
  le_uint32_t size = 0;
} __packed__;

// 10 (C->S): End of file information command list
// No arguments

// 11 (S->C): Start file downloads

struct S_StartFileDownloads_Patch_11 {
  le_uint32_t total_bytes = 0;
  le_uint32_t num_files = 0;
} __packed__;

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
  be_uint32_t address = 0;
  PortT port = 0;
  le_uint16_t unused = 0;
} __packed__;

struct S_Reconnect_Patch_14 : S_Reconnect<be_uint16_t> {
} __packed__;

// 15 (S->C): Login failure
// No arguments
// The client shows a message like "Incorrect game ID or password" and
// disconnects.

// No commands beyond 15 are valid on the patch server.

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// GAME SERVER COMMANDS ////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// 00: Invalid command

// 01 (S->C): Lobby message box
// Internal name: RcvError
// A small message box appears in lower-right corner, and the player must press
// a key to continue. The maximum length of the message is 0x200 bytes.
// Internally, PSO calls this RcvError, since it's generally used to tell the
// player why they can't do something (e.g. join a full game).
// This format is shared by multiple commands; for all of them except 06 (S->C),
// the guild_card_number field is unused and should be 0.

struct SC_TextHeader_01_06_11_B0_EE {
  le_uint32_t unused = 0;
  le_uint32_t guild_card_number = 0;
  // Text immediately follows here (char[] on DC/V3, char16_t[] on PC/BB)
} __packed__;

// 02 (S->C): Start encryption (except on BB)
// Internal name: RcvPsoConnectV2
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
  le_uint32_t server_key = 0; // Key for data sent by server
  le_uint32_t client_key = 0; // Key for data sent by client
} __packed__;

template <size_t AfterBytes>
struct S_ServerInitWithAfterMessage_DC_PC_V3_02_17_91_9B {
  S_ServerInitDefault_DC_PC_V3_02_17_91_9B basic_cmd;
  // This field is not part of SEGA's implementation; the client ignores it.
  // newserv sends a message here disavowing the preceding copyright notice.
  ptext<char, AfterBytes> after_message;
} __packed__;

// 03 (C->S): Legacy register (non-BB)
// Internal name: SndRegist

struct C_LegacyLogin_PC_V3_03 {
  le_uint64_t unused = 0; // Same as unused field in 9D/9E
  le_uint32_t sub_version = 0;
  uint8_t is_extended = 0;
  uint8_t language = 0;
  le_uint16_t unknown_a2 = 0;
  // Note: These are suffixed with 2 since they come from the same source data
  // as the corresponding fields in 9D/9E. (Even though serial_number and
  // serial_number2 have the same contents in 9E, they do not come from the same
  // field on the client's connection context object.)
  ptext<char, 0x10> serial_number2;
  ptext<char, 0x10> access_key2;
} __packed__;

// 03 (S->C): Legacy register result (non-BB)
// Internal name: RcvRegist
// header.flag specifies if the password was correct. If header.flag is 0, the
// password saved to the memory card (if any) is deleted and the client is
// disconnected. If header.flag is nonzero, the client responds with an 04
// command. Curiously, it looks like even DCv1 doesn't use this command in its
// standard login sequence, so this may be a relic from very early development.
// Even more curiously, DCv2 (and no other PSO version) has a behavior that
// appears to be some kind of anti-cheating mechanism: if any byte in the memory
// range 8C004000-8C007FFF is not zero, the handler for this command loops
// infinitely doing nothing.
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
} __packed__;

template <size_t AfterBytes>
struct S_ServerInitWithAfterMessage_BB_03_9B {
  S_ServerInitDefault_BB_03_9B basic_cmd;
  // As in 02, this field is not part of SEGA's implementation.
  ptext<char, AfterBytes> after_message;
} __packed__;

// 04 (C->S): Legacy login
// Internal name: SndLogin2
// Curiously, there is a SndLogin3 function, but it does not send anything.
// See comments on non-BB 03 (S->C). This is likely a relic of an older,
// now-unused sequence. Like 03, this command isn't used by any PSO version that
// newserv supports.
// header.flag is nonzero, but it's not clear what it's used for.

struct C_LegacyLogin_PC_V3_04 {
  le_uint64_t unused1 = 0; // Same as unused field in 9D/9E
  le_uint32_t sub_version = 0;
  uint8_t is_extended = 0;
  uint8_t language = 0;
  le_uint16_t unknown_a2 = 0;
  ptext<char, 0x10> serial_number;
  ptext<char, 0x10> access_key;
} __packed__;

struct C_LegacyLogin_BB_04 {
  parray<le_uint32_t, 3> unknown_a1;
  ptext<char, 0x10> username;
  ptext<char, 0x10> password;
} __packed__;

// 04 (S->C): Set guild card number and update client config ("security data")
// Internal name: RcvLogin
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
  // Note: What we call player_tag here is actually three fields: two uint8_ts
  // followed by a le_uint16_t. It's unknown what the uint8_t fields are for
  // (they seem to always be zero), but the le_uint16_t is likely a boolean
  // which denotes whether the player is present or not (for example, in lobby
  // data structures). For historical and simplicity reasons, newserv combines
  // these three fields into one, which takes on the value 0x00010000 when a
  // player is present and zero when none is present.
  le_uint32_t player_tag = 0x00010000;
  le_uint32_t guild_card_number = 0;
  // The ClientConfig structure describes how newserv uses this command; other
  // servers do not use the same format for the following 0x20 or 0x28 bytes (or
  // may not use it at all). The cfg field is opaque to the client; it will send
  // back the contents verbatim in its next 9E command (or on request via 9F).
  ClientConfigT cfg;
} __packed__;

struct S_UpdateClientConfig_DC_PC_V3_04 : S_UpdateClientConfig<ClientConfig> {
} __packed__;
struct S_UpdateClientConfig_BB_04 : S_UpdateClientConfig<ClientConfigBB> {
} __packed__;

// 05: Disconnect
// Internal name: SndLogout
// No arguments
// Sending this command to a client will cause it to disconnect. There's no
// advantage to doing this over simply closing the TCP connection. Clients will
// send this command to the server when they are about to disconnect, but the
// server does not need to close the connection when it receives this command
// (and in some cases, the client will send multiple 05 commands before actually
// disconnecting).

// 06: Chat
// Internal name: RcvChat and SndChat
// Server->client format is same as 01 command. The maximum size of the message
// is 0x200 bytes.
// Client->server format is very similar; we include a zero-length array in this
// struct to make parsing easier.
// When sent by the client, the text field includes only the message. When sent
// by the server, the text field includes the origin player's name, followed by
// a tab character, followed by the message.
// During Episode 3 battles, the first byte of an inbound 06 command's message
// is interpreted differently. It should be treated as a bit field, with the low
// 4 bits intended as masks for who can see the message. If the low bit (1) is
// set, for example, then the chat message displays as " (whisper)" on player
// 0's screen regardless of the message contents. The next bit (2) hides the
// message from player 1, etc. The high 4 bits of this byte appear not to be
// used, but are often nonzero and set to the value 4. (This is probably done so
// that the field is always a valid ASCII character and also never terminates
// the chat string accidentally.) We call this byte private_flags in the places
// where newserv uses it.

struct C_Chat_06 {
  parray<le_uint32_t, 2> unused;
  union {
    char dcv3[0];
    char16_t pcbb[0];
  } __packed__ text;
} __packed__;

// 07 (S->C): Ship select menu
// Internal name: RcvDirList
// This command triggers a general form of blocking menu, which was used for
// both the ship select and block select menus by Sega (and all other private
// servers except newserv, it seems). Curiously, the string "RcvBlockList"
// appears in PSO v1 and v2, but it is not used, implying that at some point
// there was a separate command to send the block list, but it was scrapped.
// Perhaps this was used for command A1, which is identical to 07 and A0 in all
// versions of PSO (except DC NTE).

// Command is a list of these; header.flag is the entry count. The first entry
// is not included in the count and does not appear on the client. The text of
// the first entry becomes the ship name when the client joins a lobby.
template <typename CharT, int EntryLength>
struct S_MenuEntry {
  le_uint32_t menu_id = 0;
  le_uint32_t item_id = 0;
  le_uint16_t flags = 0x0F04; // Should be this value, apparently
  ptext<CharT, EntryLength> text;
} __packed__;
struct S_MenuEntry_PC_BB_07_1F : S_MenuEntry<char16_t, 0x11> {
} __packed__;
struct S_MenuEntry_DC_V3_07_1F : S_MenuEntry<char, 0x12> {
} __packed__;

// 08 (C->S): Request game list
// Internal name: SndGameList
// No arguments

// 08 (S->C): Game list
// Internal name: RcvGameList
// Client responds with 09 and 10 commands (or nothing if the player cancels).

// Command is a list of these; header.flag is the entry count. The first entry
// is not included in the count and does not appear on the client.
template <typename CharT>
struct S_GameMenuEntry {
  le_uint32_t menu_id = 0;
  le_uint32_t game_id = 0;
  // difficulty_tag is 0x0A on Episode 3; on all other versions, it's
  // difficulty + 0x22 (so 0x25 means Ultimate, for example)
  uint8_t difficulty_tag = 0;
  uint8_t num_players = 0;
  ptext<CharT, 0x10> name;
  // The episode field is used differently by different versions:
  // - On DCv1, PC, and GC Episode 3, the value is ignored.
  // - On DCv2, 1 means v1 players can't join the game, and 0 means they can.
  // - On GC Ep1&2, 0x40 means Episode 1, and 0x41 means Episode 2.
  // - On BB, 0x40/0x41 mean Episodes 1/2 as on GC, and 0x43 means Episode 4.
  uint8_t episode = 0;
  // Flags:
  // 02 = Locked (lock icon appears in menu; player is prompted for password if
  //      they choose this game)
  // 04 = In battle (Episode 3; a sword icon appears in menu)
  // 04 = Disabled (BB; used for solo games)
  // 10 = Is battle mode
  // 20 = Is challenge mode
  uint8_t flags = 0;
} __packed__;
struct S_GameMenuEntry_PC_BB_08 : S_GameMenuEntry<char16_t> {
} __packed__;
struct S_GameMenuEntry_DC_V3_08_Ep3_E6 : S_GameMenuEntry<char> {
} __packed__;

// 09 (C->S): Menu item info request
// Internal name: SndInfo
// Server will respond with an 11 command, or an A3 or A5 if the specified menu
// is the quest menu.

struct C_MenuItemInfoRequest_09 {
  le_uint32_t menu_id = 0;
  le_uint32_t item_id = 0;
} __packed__;

// 0B: Invalid command

// 0C (C->S): Create game (DCv1)
// Same format as C1, but fields not supported by v1 (e.g. episode, v2 mode)
// are unused.

// 0D: Invalid command

// 0E (S->C): Incomplete/legacy join game (non-BB)
// Internal name: RcvStartGame
// header.flag = number of valid entries in lobby_data

// This command appears to be a vestige of very early development; its
// second-phase handler is missing even in the earliest public prototype of PSO
// (DC NTE), and the command format is missing some important information
// necessary to start a game on any version.

// There is a failure mode in the 0E command handler that causes the thread
// receiving the command to loop infinitely doing nothing, effectively
// softlocking the game. This happens if the local player's Guild Card number
// doesn't match any of the lobby_data entries. (Notably, only the first
// (header.flag) entries are checked.)
// If the local player's Guild Card number does match one of the entries, the
// command does not softlock, but instead does nothing because the 0E
// second-phase handler is missing.

struct S_LegacyJoinGame_PC_0E {
  struct UnknownA1 {
    le_uint32_t player_tag;
    le_uint32_t guild_card_number;
    parray<uint8_t, 0x10> unknown_a1;
  } __packed__;
  parray<UnknownA1, 4> unknown_a1;
  parray<uint8_t, 0x20> unknown_a3;
} __packed__;

struct S_LegacyJoinGame_GC_0E {
  PlayerLobbyDataDCGC lobby_data[4];
  struct UnknownA0 {
    parray<uint8_t, 2> unknown_a1;
    le_uint16_t unknown_a2 = 0;
    le_uint32_t unknown_a3 = 0;
  } __packed__;
  parray<UnknownA0, 8> unknown_a0;
  le_uint32_t unknown_a1 = 0;
  parray<uint8_t, 0x20> unknown_a2;
  parray<uint8_t, 4> unknown_a3;
} __packed__;

struct S_LegacyJoinGame_XB_0E {
  struct UnknownA1 {
    le_uint32_t player_tag;
    le_uint32_t guild_card_number;
    parray<uint8_t, 0x18> unknown_a1;
  } __packed__;
  parray<UnknownA1, 4> unknown_a1;
  parray<uint8_t, 0x68> unknown_a2;
} __packed__;

// 0F: Invalid command

// 10 (C->S): Menu selection
// Internal name: SndAction
// header.flag contains two flags: 02 specifies if a password is present, and 01
// specifies... something else. These two bits directly correspond to the two
// lowest bits in the flags field of the game menu: 02 specifies that the game
// is locked, but the function of 01 is unknown.
// Annoyingly, the no-arguments form of the command can have any flag value, so
// it doesn't suffice to check the flag value to know which format is being
// used!

struct C_MenuSelection_10_Flag00 {
  le_uint32_t menu_id = 0;
  le_uint32_t item_id = 0;
} __packed__;

template <typename CharT>
struct C_MenuSelection_10_Flag01 {
  C_MenuSelection_10_Flag00 basic_cmd;
  ptext<CharT, 0x10> unknown_a1;
} __packed__;
struct C_MenuSelection_DC_V3_10_Flag01 : C_MenuSelection_10_Flag01<char> {
} __packed__;
struct C_MenuSelection_PC_BB_10_Flag01 : C_MenuSelection_10_Flag01<char16_t> {
} __packed__;

template <typename CharT>
struct C_MenuSelection_10_Flag02 {
  C_MenuSelection_10_Flag00 basic_cmd;
  ptext<CharT, 0x10> password;
} __packed__;
struct C_MenuSelection_DC_V3_10_Flag02 : C_MenuSelection_10_Flag02<char> {
} __packed__;
struct C_MenuSelection_PC_BB_10_Flag02 : C_MenuSelection_10_Flag02<char16_t> {
} __packed__;

template <typename CharT>
struct C_MenuSelection_10_Flag03 {
  C_MenuSelection_10_Flag00 basic_cmd;
  ptext<CharT, 0x10> unknown_a1;
  ptext<CharT, 0x10> password;
} __packed__;
struct C_MenuSelection_DC_V3_10_Flag03 : C_MenuSelection_10_Flag03<char> {
} __packed__;
struct C_MenuSelection_PC_BB_10_Flag03 : C_MenuSelection_10_Flag03<char16_t> {
} __packed__;

// 11 (S->C): Ship info
// Internal name: RcvMessage
// Same format as 01 command. The text appears in a small box in the lower-left
// corner (on V3/BB) or lower-right corner of the screen.

// 12 (S->C): Valid but ignored (all versions)
// Internal name: RcvBaner
// This command's internal name is possibly a misspelling of "banner", which
// could be an early version of the 1A/D5 (large message box) commands, or of
// BB's 00EE (scrolling message) command; however, the existence of
// RcvBanerHead (16) seems to contradict this hypothesis since a text message
// would not require a separate header command. Even on DC NTE, this command
// does nothing, so this must have been scrapped very early in development.

// 13 (S->C): Write online quest file
// Internal name: RcvDownLoad
// Used for downloading online quests. For download quests (to be saved to the
// memory card), use A7 instead.
// All chunks except the last must have 0x400 data bytes. When downloading an
// online quest, the .bin and .dat chunks may be interleaved (although newserv
// currently sends them sequentially).

// header.flag = file chunk index (start offset / 0x400)
struct S_WriteFile_13_A7 {
  ptext<char, 0x10> filename;
  parray<uint8_t, 0x400> data;
  le_uint32_t data_size = 0;
} __packed__;

// 13 (C->S): Confirm file write (V3/BB)
// Client sends this in response to each 13 sent by the server. It appears
// these are only sent by V3 and BB - PSO DC and PC do not send these.

// header.flag = file chunk index (same as in the 13/A7 sent by the server)
struct C_WriteFileConfirmation_V3_BB_13_A7 {
  ptext<char, 0x10> filename;
} __packed__;

// 14 (S->C): Valid but ignored (all versions)
// Internal name: RcvUpLoad
// Based on its internal name, this command seems like the logical opposite of
// 13 (quest file download, named RcvDownLoad internally). However, even in DC
// NTE, this command does nothing, so it must have been scrapped very early in
// development. There is a SndUpLoad string in the DC versions, but the
// corresponding function was deleted.

// 15: Invalid command

// 16 (S->C): Valid but ignored (all versions)
// Internal name: RcvBanerHead
// It's not clear what this command was supposed to do, but it's likely related
// to 12 in some way. Like 12, this command does nothing, even on DC NTE.

// 17 (S->C): Start encryption at login server (except on BB)
// Internal name: RcvPsoRegistConnectV2
// Same format and usage as 02 command, but a different copyright string:
// "DreamCast Port Map. Copyright SEGA Enterprises. 1999"
// Unlike the 02 command, V3 clients will respond with a DB command when they
// receive a 17 command in any online session, with the exception of Episodes
// 1&2 trial edition (which responds with a 9A). DCv1 will respond with a 90. DC
// NTE will respond with an 8B. Other non-V3 clients will respond with a 9A or
// 9D.

// 18 (S->C): License verification result (PC/V3)
// Behaves exactly the same as 9A (S->C). No arguments except header.flag.
// TODO: Check if this command exists on DC v1/v2.

// 19 (S->C): Reconnect to different address
// Internal name: RcvPort
// Client will disconnect, and reconnect to the given address/port. Encryption
// will be disabled on the new connection; the server should send an appropriate
// command to enable it when the client connects.
// Note: PSO XB seems to ignore the address field, which makes sense given its
// networking architecture.

struct S_Reconnect_19 : S_Reconnect<le_uint16_t> {
} __packed__;

// Because PSO PC and some versions of PSO DC/GC use the same port but different
// protocols, we use a specially-crafted 19 command to send them to two
// different ports depending on the client version. I first saw this technique
// used by Schthack; I don't know if it was his original creation.

struct S_ReconnectSplit_19 {
  be_uint32_t pc_address = 0;
  le_uint16_t pc_port = 0;
  parray<uint8_t, 0x0F> unused1;
  uint8_t gc_command = 0x19;
  uint8_t gc_flag = 0;
  le_uint16_t gc_size = 0x97;
  be_uint32_t gc_address = 0;
  le_uint16_t gc_port = 0;
  parray<uint8_t, 0xB0 - 0x23> unused2;
} __packed__;

// 1A (S->C): Large message box
// Internal name: RcvText
// On V3, client will sometimes respond with a D6 command (see D6 for more
// information).
// Contents are plain text (char on DC/V3, char16_t on PC/BB). There must be at
// least one null character ('\0') before the end of the command data.
// There is a bug in V3 (and possibly all versions) where if this command is
// sent after the client has joined a lobby, the chat log window contents will
// appear in the message box, prepended to the message text from the command.
// The maximum length of the message is 0x400 bytes. This is the only
// difference between this command and the D5 command.

// 1B (S->C): Valid but ignored (all versions)
// Internal name: RcvBattleData
// This command does nothing in all PSO versions. There is a SndBattleData
// string in the DC versions, but the corresponding function was deleted.

// 1C (S->C): Valid but ignored (all versions)
// Internal name: RcvSystemFile
// This command does nothing in all PSO versions.

// 1D: Ping
// Internal name: RcvPing
// No arguments
// When sent to the client, the client will respond with a 1D command. Data sent
// by the server is ignored; the client always sends a 1D command with no data.

// 1E: Invalid command

// 1F (C->S): Request information menu
// Internal name: SndTextList
// No arguments
// This command is used in PSO DC and PC. It exists in V3 as well but is
// apparently unused.

// 1F (S->C): Information menu
// Internal name: RcvTextList
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
} __packed__;

// Command 0122 uses a 4-byte challenge sent in the header.flag field instead.
// This version of the command has no other arguments.

// 23 (S->C): Unknown (BB)
// header.flag is used, but the command has no other arguments.

// 24 (S->C): Unknown (BB)

struct S_Unknown_BB_24 {
  le_uint16_t unknown_a1 = 0;
  le_uint16_t unknown_a2 = 0;
  parray<le_uint32_t, 8> values;
} __packed__;

// 25 (S->C): Unknown (BB)

struct S_Unknown_BB_25 {
  le_uint16_t unknown_a1 = 0;
  uint8_t offset1 = 0;
  uint8_t value1 = 0;
  uint8_t offset2 = 0;
  uint8_t value2 = 0;
  le_uint16_t unused = 0;
} __packed__;

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
// Internal name: SndFindUser
// There is an unused command named SndFavorite in the DC versions of PSO,
// which may have been related to this command. SndFavorite seems to be
// completely unused; its sender function was optimized out of all known
// builds, leaving only its name string remaining.
// The server should respond with a 41 command if the target is online. If the
// target is not online, the server doesn't respond at all.

struct C_GuildCardSearch_40 {
  le_uint32_t player_tag = 0x00010000;
  le_uint32_t searcher_guild_card_number = 0;
  le_uint32_t target_guild_card_number = 0;
} __packed__;

// 41 (S->C): Guild card search result
// Internal name: RcvUserAns

template <typename CharT>
struct SC_MeetUserExtension {
  struct LobbyReference {
    le_uint32_t menu_id = 0;
    le_uint32_t item_id = 0;
  } __packed__;
  parray<LobbyReference, 8> lobby_refs;
  le_uint32_t unknown_a2 = 0;
  ptext<CharT, 0x20> player_name;
} __packed__;

template <typename HeaderT, typename CharT>
struct S_GuildCardSearchResult {
  le_uint32_t player_tag = 0x00010000;
  le_uint32_t searcher_guild_card_number = 0;
  le_uint32_t result_guild_card_number = 0;
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
} __packed__;
struct S_GuildCardSearchResult_PC_41
    : S_GuildCardSearchResult<PSOCommandHeaderPC, char16_t> {
} __packed__;
struct S_GuildCardSearchResult_DC_V3_41
    : S_GuildCardSearchResult<PSOCommandHeaderDCV3, char> {
} __packed__;
struct S_GuildCardSearchResult_BB_41
    : S_GuildCardSearchResult<PSOCommandHeaderBB, char16_t> {
} __packed__;

// 42: Invalid command
// 43: Invalid command

// 44 (S->C): Open file for download
// Internal name: RcvDownLoadHead
// Used for downloading online quests. The client will react to a 44 command if
// the filename ends in .bin or .dat.
// For download quests (to be saved to the memory card) and GBA games, the A6
// command is used instead. The client will react to A6 if the filename ends in
// .bin/.dat (quests), .pvr (textures), or .gba (GameBoy Advance games).
// It appears that the .gba handler for A6 was not deleted in PSO XB, even
// though it doesn't make sense for an XB client to receive such a file.

struct S_OpenFile_DC_44_A6 {
  ptext<char, 0x22> name; // Should begin with "PSO/"
  // The type field is only used for download quests (A6); it is ignored for
  // online quests (44). The following values are valid for A6:
  //   0 = download quest (client expects .bin and .dat files)
  //   1 = download quest (client expects .bin, .dat, and .pvr files)
  //   2 = GBA game (GC only; client expects .gba file only)
  //   3 = Episode 3 download quest (Ep3 only; client expects .bin file only)
  // There is a bug in the type logic: an A6 command always overwrites the
  // current download type even if the filename doesn't end in .bin, .dat, .pvr,
  // or .gba. This may lead to a resource exhaustion bug if exploited carefully,
  // but I haven't verified this. Generally the server should send all files for
  // a given piece of content with the same type in each file's A6 command.
  uint8_t type = 0;
  ptext<char, 0x11> filename;
  le_uint32_t file_size = 0;
} __packed__;

struct S_OpenFile_PC_V3_44_A6 {
  ptext<char, 0x22> name; // Should begin with "PSO/"
  le_uint16_t type = 0;
  ptext<char, 0x10> filename;
  le_uint32_t file_size = 0;
} __packed__;

// Curiously, PSO XB expects an extra 0x18 bytes at the end of this command, but
// those extra bytes are unused, and the client does not fail if they're
// omitted.
struct S_OpenFile_XB_44_A6 : S_OpenFile_PC_V3_44_A6 {
  parray<uint8_t, 0x18> unused2;
} __packed__;

struct S_OpenFile_BB_44_A6 {
  parray<uint8_t, 0x22> unused;
  le_uint16_t type = 0;
  ptext<char, 0x10> filename;
  le_uint32_t file_size = 0;
  ptext<char, 0x18> name;
} __packed__;

// 44 (C->S): Confirm open file (V3/BB)
// Client sends this in response to each 44 sent by the server.

// header.flag = quest number (sort of - seems like the client just echoes
// whatever the server sent in its header.flag field. Also quest numbers can be
// > 0xFF so the flag is essentially meaningless)
struct C_OpenFileConfirmation_44_A6 {
  ptext<char, 0x10> filename;
} __packed__;

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
// Internal name: SndPsoData
// When a client sends this command, the server should forward it to all players
// in the same game/lobby, except the player who originally sent the command.
// See ReceiveSubcommands or the subcommand index below for details on contents.
// The data in this command may be up to 0x400 bytes in length. If it's larger,
// the client will exhibit undefined behavior.

// 61 (C->S): Player data
// Internal name: SndCharaDataV2 (SndCharaData in DCv1)
// See the PSOPlayerData structs in Player.hh for this command's format.
// header.flag specifies the format version, which is related to (but not
// identical to) the game's major version. For example, the format version is 01
// on DC v1, 02 on PSO PC, 03 on PSO GC, XB, and BB, and 04 on Episode 3.
// Upon joining a game, the client assigns inventory item IDs sequentially as
// (0x00010000 + (0x00200000 * lobby_client_id) + x). So, for example, player
// 3's 8th item's ID would become 0x00610007. The item IDs from the last game
// the player was in will appear in their inventory in this command.
// Note: If the client is in a game at the time this command is received, the
// inventory sent by the client only includes items that would not disappear if
// the client crashes! Essentially, it reflects the saved state of the player's
// character rather than the live state.

// 62: Target command
// Internal name: SndPsoData2
// When a client sends this command, the server should forward it to the player
// identified by header.flag in the same game/lobby, even if that player is the
// player who originally sent it.
// See ReceiveSubcommands or the subcommand index below for details on contents.
// The data in this command may be up to 0x400 bytes in length. If it's larger,
// the client will exhibit undefined behavior.

// 63: Invalid command

// 64 (S->C): Join game
// Internal name: RcvStartGame3

// This is sent to the joining player; the other players get a 65 instead.
// Note that (except on Episode 3) this command does not include the player's
// disp or inventory data. The clients in the game are responsible for sending
// that data to each other during the join process with 60/62/6C/6D commands.

// Curiously, this command is named RcvStartGame3 internally, while 0E is named
// RcvStartGame. The string RcvStartGame2 appears in the DC versions, but it
// seems the relevant code was deleted - there are no references to the string.
// Based on the large gap between commands 0E and 64, we can't guess at which
// command number RcvStartGame2 might have been.

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
  parray<LobbyDataT, 4> lobby_data;
  uint8_t client_id = 0;
  uint8_t leader_id = 0;
  uint8_t disable_udp = 1;
  uint8_t difficulty = 0;
  uint8_t battle_mode = 0;
  uint8_t event = 0;
  uint8_t section_id = 0;
  uint8_t challenge_mode = 0;
  le_uint32_t rare_seed = 0;
  // Note: The 64 command for PSO DC ends here (the next 4 fields are ignored).
  // newserv sends them anyway for code simplicity reasons.
  uint8_t episode = 0;
  // Similarly, PSO GC ignores the values in the following fields.
  uint8_t unused2 = 1; // Should be 1 for PSO PC?
  // Note: Only BB uses this field; it's unused on all other versions (since
  // only BB has solo mode).
  uint8_t solo_mode = 0;
  uint8_t unused3 = 0;
} __packed__;

struct S_JoinGame_DCNTE_64 {
  uint8_t client_id;
  uint8_t leader_id;
  uint8_t disable_udp;
  uint8_t unused;
  parray<le_uint32_t, 0x20> variations;
  parray<PlayerLobbyDataDCGC, 4> lobby_data;
} __packed__;

struct S_JoinGame_PC_64 : S_JoinGame<PlayerLobbyDataPC, PlayerDispDataDCPCV3> {
} __packed__;
struct S_JoinGame_DC_GC_64 : S_JoinGame<PlayerLobbyDataDCGC, PlayerDispDataDCPCV3> {
} __packed__;

struct S_JoinGame_GC_Ep3_64 : S_JoinGame_DC_GC_64 {
  // This field is only present if the game (and client) is Episode 3. Similarly
  // to lobby_data in the base struct, all four of these are always present and
  // they are filled in in slot positions.
  struct {
    PlayerInventory inventory;
    PlayerDispDataDCPCV3 disp;
  } __packed__ players_ep3[4];
} __packed__;

struct S_JoinGame_XB_64 : S_JoinGame<PlayerLobbyDataXB, PlayerDispDataDCPCV3> {
  parray<le_uint32_t, 6> unknown_a1;
} __packed__;

struct S_JoinGame_BB_64 : S_JoinGame<PlayerLobbyDataBB, PlayerDispDataBB> {
} __packed__;

// 65 (S->C): Add player to game
// Internal name: RcvBurstGame
// When a player joins an existing game, the joining player receives a 64
// command (described above), and the players already in the game receive a 65
// command containing only the joining player's data.

struct LobbyFlags_DCNTE {
  uint8_t client_id = 0;
  uint8_t leader_id = 0;
  uint8_t disable_udp = 1;
  uint8_t unused = 0;
} __packed__;

struct LobbyFlags {
  uint8_t client_id = 0;
  uint8_t leader_id = 0;
  uint8_t disable_udp = 1;
  uint8_t lobby_number = 0;
  uint8_t block_number = 0;
  uint8_t unknown_a1 = 0;
  uint8_t event = 0;
  uint8_t unknown_a2 = 0;
  le_uint32_t unused = 0;
} __packed__;

// Header flag = entry count (always 1 for 65 and 68; up to 0x0C for 67)
template <typename LobbyFlagsT, typename LobbyDataT, typename DispDataT>
struct S_JoinLobby {
  LobbyFlagsT lobby_flags;
  struct Entry {
    LobbyDataT lobby_data;
    PlayerInventory inventory;
    DispDataT disp;
  } __packed__;
  // Note: not all of these will be filled in and sent if the lobby isn't full
  // (the command size will be shorter than this struct's size)
  parray<Entry, 12> entries;

  static inline size_t size(size_t used_entries) {
    return offsetof(S_JoinLobby, entries) + used_entries * sizeof(Entry);
  }
} __packed__;

struct S_JoinLobby_DCNTE_65_67_68
    : S_JoinLobby<LobbyFlags_DCNTE, PlayerLobbyDataDCGC, PlayerDispDataDCPCV3> {
} __packed__;
struct S_JoinLobby_PC_65_67_68
    : S_JoinLobby<LobbyFlags, PlayerLobbyDataPC, PlayerDispDataDCPCV3> {
} __packed__;
struct S_JoinLobby_DC_GC_65_67_68_Ep3_EB
    : S_JoinLobby<LobbyFlags, PlayerLobbyDataDCGC, PlayerDispDataDCPCV3> {
} __packed__;
struct S_JoinLobby_BB_65_67_68
    : S_JoinLobby<LobbyFlags, PlayerLobbyDataBB, PlayerDispDataBB> {
} __packed__;

struct S_JoinLobby_XB_65_67_68 {
  LobbyFlags lobby_flags;
  parray<le_uint32_t, 6> unknown_a4;
  struct Entry {
    PlayerLobbyDataXB lobby_data;
    PlayerInventory inventory;
    PlayerDispDataDCPCV3 disp;
  } __packed__;
  // Note: not all of these will be filled in and sent if the lobby isn't full
  // (the command size will be shorter than this struct's size)
  parray<Entry, 12> entries;

  static inline size_t size(size_t used_entries) {
    return offsetof(S_JoinLobby_XB_65_67_68, entries) + used_entries * sizeof(Entry);
  }
} __packed__;

// 66 (S->C): Remove player from game
// Internal name: RcvExitGame
// This is sent to all players in a game except the leaving player.
// header.flag should be set to the leaving player ID (same as client_id).

struct S_LeaveLobby_66_69_Ep3_E9 {
  uint8_t client_id = 0;
  uint8_t leader_id = 0;
  // Note: disable_udp only has an effect for games; it is unused for lobbies
  // and spectator teams.
  uint8_t disable_udp = 1;
  uint8_t unused = 0;
} __packed__;

// 67 (S->C): Join lobby
// Internal name: RcvStartLobby2
// This is sent to the joining player; the other players receive a 68 instead.
// Same format as 65 command, but used for lobbies instead of games.

// Curiously, this command is named RcvStartLobby2 internally, but there is no
// command named RcvStartLobby. The string "RcvStartLobby" does appear in the DC
// game executable, but it appears the relevant code was deleted.

// 68 (S->C): Add player to lobby
// Internal name: RcvBurstLobby
// Same format as 65 command, but used for lobbies instead of games.
// The command only includes the joining player's data.

// 69 (S->C): Remove player from lobby
// Internal name: RcvExitLobby
// Same format as 66 command, but used for lobbies instead of games.

// 6A: Invalid command
// 6B: Invalid command

// 6C: Broadcast command
// Internal name: RcvPsoDataLong and SndPsoDataLong
// Same format and usage as 60 command, but with no size limit.

// 6D: Target command
// Internal name: RcvPsoDataLong and SndPsoDataLong2
// Same format and usage as 62 command, but with no size limit.

// 6E: Invalid command

// 6F (C->S): Set game status
// Internal name: SndBurstEnd
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

// 80: Valid but ignored (all versions)
// Internal names: RcvGenerateID and SndGenerateID
// This command appears to be used to set the next item ID for the given player
// slot. PSO V3 and later accept this command, but ignore it entirely. Notably,
// no version of PSO except for DC NTE ever sends this command - it's likely it
// was used to implement some item ID sync semantics that were later changed to
// use the leader as the source of truth.

struct C_GenerateID_DCNTE_80 {
  le_uint32_t id;
  uint8_t unused1; // Always 0
  uint8_t unused2; // Always 0
  le_uint16_t unused3; // Always 0
  parray<uint8_t, 4> unused4; // Client sends uninitialized data here
} __packed__;

struct S_GenerateID_DC_PC_V3_80 {
  le_uint32_t client_id = 0;
  le_uint32_t unused = 0;
  le_uint32_t next_item_id = 0;
} __packed__;

// 81: Simple mail
// Internal name: RcvChatMessage and SndChatMessage
// Format is the same in both directions. The server should forward the command
// to the player with to_guild_card_number, if they are online. If they are not
// online, the server may store it for later delivery, send their auto-reply
// message back to the original sender, or simply drop the message.
// On GC (and probably other versions too) the unused space after the text
// contains uninitialized memory when the client sends this command. newserv
// clears the uninitialized data for security reasons before forwarding.

template <typename CharT>
struct SC_SimpleMail_81 {
  le_uint32_t player_tag = 0x00010000;
  le_uint32_t from_guild_card_number = 0;
  ptext<CharT, 0x10> from_name;
  le_uint32_t to_guild_card_number = 0;
  ptext<CharT, 0x200> text;
} __packed__;

struct SC_SimpleMail_PC_81 : SC_SimpleMail_81<char16_t> {
} __packed__;
struct SC_SimpleMail_DC_V3_81 : SC_SimpleMail_81<char> {
} __packed__;

struct SC_SimpleMail_BB_81 {
  le_uint32_t player_tag = 0x00010000;
  le_uint32_t from_guild_card_number = 0;
  ptext<char16_t, 0x10> from_name;
  le_uint32_t to_guild_card_number = 0;
  ptext<char16_t, 0x14> received_date;
  ptext<char16_t, 0x200> text;
} __packed__;

// 82: Invalid command

// 83 (S->C): Lobby menu
// Internal name: RcvRoomInfo
// Curiously, there is a SndRoomInfo string in the DC versions. Perhaps in an
// early (pre-NTE) build, the client had to request the lobby menu from the
// server, and SndRoomInfo was the command to do so. The code to send this
// command must have been removed before DC NTE.
// This command sets the menu item IDs that the client uses for the lobby
// teleporter menu. On DCv1, the client expects 10 entries here; on all other
// versions except Episode 3, the client expects 15 items here; on Episode 3,
// the client expects 20 items here. Sending more or fewer items does not
// change the lobby count on the client. If fewer entries are sent, the menu
// item IDs for some lobbies will not be set, and the client will likely send
// 84 commands that don't make sense if the player chooses one of lobbies with
// unset IDs. On Episode 3, the CARD lobbies are the last five entries, even
// though they appear at the top of the list on the player's screen.

// Command is a list of these; header.flag is the entry count (10, 15 or 20)
struct S_LobbyListEntry_83 {
  le_uint32_t menu_id = 0;
  le_uint32_t item_id = 0;
  le_uint32_t unused = 0;
} __packed__;

// 84 (C->S): Choose lobby
// Internal name: SndRoomChange

struct C_LobbySelection_84 {
  le_uint32_t menu_id = 0;
  le_uint32_t item_id = 0;
} __packed__;

// 85: Invalid command
// 86: Invalid command
// 87: Invalid command

// 88 (C->S): License check (DC NTE only)
// The server should respond with an 88 command.

struct C_Login_DCNTE_88 {
  ptext<char, 0x11> serial_number;
  ptext<char, 0x11> access_key;
} __packed__;

// 88 (S->C): License check result (DC NTE only)
// No arguments except header.flag.
// If header.flag is zero, client will respond with an 8A command. Otherwise, it
// will respond with an 8B command. This is the same behavior as for the 18
// command (and in fact, the client handler is shared between both commands.)

// 88 (S->C): Update lobby arrows (except DC NTE)
// If this command is sent while a client is joining a lobby, the client may
// ignore it. For this reason, the server should wait a few seconds after a
// client joins a lobby before sending an 88 command.
// This command is not supported on DC v1.

// Command is a list of these; header.flag is the entry count. There should
// be an update for every player in the lobby in this command, even if their
// arrow color isn't being changed.
struct S_ArrowUpdateEntry_88 {
  le_uint32_t player_tag = 0x00010000;
  le_uint32_t guild_card_number = 0;
  // Values for arrow_color:
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
  le_uint32_t arrow_color = 0;
} __packed__;

// 89 (C->S): Set lobby arrow
// header.flag = arrow color number (see above); no other arguments.
// Server should send an 88 command to all players in the lobby.

// 89 (S->C): Start encryption at login server (DC NTE)
// Behaves exactly the same as the 17 command.

// 8A (C->S): Connection information (DC NTE only)
// The server should respond with an 8A command.

struct C_ConnectionInfo_DCNTE_8A {
  ptext<char, 0x08> hardware_id;
  le_uint32_t sub_version = 0x20;
  le_uint32_t unknown_a1 = 0;
  ptext<char, 0x30> username;
  ptext<char, 0x30> password;
  ptext<char, 0x30> email_address; // From Sylverant documentation
} __packed__;

// 8A (S->C): Connection information result (DC NTE only)
// header.flag is a success flag. If 0 is sent, the client shows an error
// message and disconnects. Otherwise, the client responds with an 8B command.

// 8A (C->S): Request lobby/game name (except DC NTE)
// No arguments

// 8A (S->C): Lobby/game name (except DC NTE)
// Contents is a string (char16_t on PC/BB, char on DC/V3) containing the lobby
// or game name. The client generally only sends this immediately after joining
// a game, but Sega's servers also replied to it if it was sent in a lobby. They
// would return a string like "LOBBY01" in that case even though this would
// never be used under normal circumstances.

// 8B: Log in (DC NTE only)

struct C_Login_DCNTE_8B {
  le_uint32_t player_tag = 0x00010000;
  le_uint32_t guild_card_number = 0;
  parray<uint8_t, 0x08> hardware_id;
  le_uint32_t sub_version = 0x20;
  uint8_t is_extended = 0;
  uint8_t language = 0;
  parray<uint8_t, 2> unused1;
  ptext<char, 0x11> serial_number;
  ptext<char, 0x11> access_key;
  ptext<char, 0x30> username;
  ptext<char, 0x30> password;
  ptext<char, 0x10> name;
  parray<uint8_t, 2> unused;
} __packed__;

struct C_LoginExtended_DCNTE_8B : C_Login_DCNTE_8B {
  SC_MeetUserExtension<char> extension;
} __packed__;

// 8C: Invalid command

// 8D (S->C): Request player data (DC NTE only)
// Behaves the same as 95 (S->C) on all other versions, but DC NTE crashes if it
// receives 95.

// 8E: Ship select menu (DC NTE)
// Behaves exactly the same as the A0 command (in both directions).

// 8F: Block select menu (DC NTE)
// Behaves exactly the same as the A1 command (in both directions).

// 90 (C->S): V1 login (DC/PC/V3)
// This command is used during the DCv1 login sequence; a DCv1 client will
// respond to a 17 command with an (encrypted) 90. If a V3 client receives a 91
// command, however, it will also send a 90 in response, though the contents
// will be blank (all zeroes).

struct C_LoginV1_DC_PC_V3_90 {
  ptext<char, 0x11> serial_number;
  ptext<char, 0x11> access_key;
  // Note: There is a bug in the Japanese and prototype versions of DCv1 that
  // cause the client to send this command despite its size not being a
  // multiple of 4. This is fixed in later versions, so we handle both cases in
  // the receive handler.
} __packed__;

// 90 (S->C): License verification result (V3)
// Behaves exactly the same as 9A (S->C). No arguments except header.flag.

// 91 (S->C): Start encryption at login server (legacy; non-BB only)
// Internal name: RcvPsoRegistConnect
// Same format and usage as 17 command, except the client will respond with a 90
// command. On versions that support it, this is strictly less useful than the
// 17 command. Curiously, this command appears to have been implemented after
// the 17 command since it's missing from the DC NTE version, but the 17 command
// is named RcvPsoRegistConnectV2 whereas 91 is simply RcvPsoRegistConnect. It's
// likely that after DC NTE, Sega simply changed the command numbers for this
// group of commands from 88-8F to 90-A1 (so DC NTE's 89 command became the 91
// command in all later versions).

// 92 (C->S): Register (DC)

struct C_RegisterV1_DC_92 {
  parray<uint8_t, 0x0C> unknown_a1;
  uint8_t is_extended = 0; // TODO: This is a guess
  uint8_t language = 0; // TODO: This is a guess; verify it
  parray<uint8_t, 2> unknown_a3;
  ptext<char, 0x10> hardware_id;
  parray<uint8_t, 0x50> unused1;
  ptext<char, 0x20> email; // According to Sylverant documentation
  parray<uint8_t, 0x10> unused2;
} __packed__;

// 92 (S->C): Register result (non-BB)
// Internal name: RcvPsoRegist
// Same format and usage as 9C (S->C) command.

// 93 (C->S): Log in (DCv1)

struct C_LoginV1_DC_93 {
  le_uint32_t player_tag = 0x00010000;
  le_uint32_t guild_card_number = 0;
  le_uint32_t unknown_a1 = 0;
  le_uint32_t unknown_a2 = 0;
  le_uint32_t sub_version = 0;
  uint8_t is_extended = 0;
  uint8_t language = 0;
  parray<uint8_t, 2> unused1;
  ptext<char, 0x11> serial_number;
  ptext<char, 0x11> access_key;
  ptext<char, 0x30> hardware_id;
  ptext<char, 0x30> unknown_a3;
  ptext<char, 0x10> name;
  parray<uint8_t, 2> unused2;
} __packed__;

struct C_LoginExtendedV1_DC_93 : C_LoginV1_DC_93 {
  SC_MeetUserExtension<char> extension;
} __packed__;

// 93 (C->S): Log in (BB)

struct C_Login_BB_93 {
  le_uint32_t player_tag = 0x00010000;
  le_uint32_t guild_card_number = 0;
  ptext<char, 0x08> unused;
  le_uint32_t team_id = 0;
  ptext<char, 0x30> username;
  ptext<char, 0x30> password;

  // These fields map to the same fields in SC_MeetUserExtension. There is no
  // equivalent of the name field from that structure on BB (though newserv
  // doesn't use it anyway).
  le_uint32_t menu_id = 0;
  le_uint32_t preferred_lobby_id = 0;

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
      parray<le_uint32_t, 10> as_u32;
    } __packed__;

    ClientConfigFields old_clients_cfg;
    struct NewFormat {
      parray<le_uint32_t, 2> hardware_info;
      ClientConfigFields cfg;
    } __packed__ new_clients;
  } __packed__ var;
} __packed__;

// 94: Invalid command

// 95 (S->C): Request player data
// Internal name: RcvRecognition
// No arguments
// For some reason, some servers send high values in the header.flag field here.
// The header.flag field is completely unused by the client, however - sending
// zero works just fine. The original Sega servers had some uninitialized memory
// bugs, of which that may have been one, and other private servers may have
// just duplicated Sega's behavior verbatim.
// Client will respond with a 61 command.

// 96 (C->S): Character save information
// Internal name: SndSaveCountCheck

struct C_CharSaveInfo_DCv2_PC_V3_BB_96 {
  // The creation timestamp is the number of seconds since 12:00AM on 1 January
  // 2000. Instead of computing this directly from the TBR (on PSO GC), the game
  // uses localtime(), then converts that to the desired timestamp. The leap
  // year correction in the latter phase of this computation seems incorrect; it
  // adds a day in 2002, 2006, etc. instead of 2004, 2008, etc. See
  // compute_psogc_timestamp in SaveFileFormats.cc for details.
  le_uint32_t creation_timestamp = 0;
  // This field counts certain events on a per-character basis. One of the
  // relevant events is the act of sending a 96 command; another is the act of
  // receiving a 97 command (to which the client responds with a B1 command).
  // Presumably Sega's original implementation could keep track of this value
  // for each character and could therefore tell if a character had connected to
  // an unofficial server between connections to Sega's servers.
  le_uint32_t event_counter = 0;
} __packed__;

// 97 (S->C): Save to memory card
// Internal name: RcvSaveCountCheck
// No arguments
// Internally, this command is called RcvSaveCountCheck, even though the counter
// in the 96 command (to which 97 is a reply) counts more events than saves.
// Sending this command with header.flag == 0 will show a message saying that
// "character data was improperly saved", and will delete the character's items
// and challenge mode records. newserv (and all other unofficial servers) always
// send this command with flag == 1, which causes the client to save normally.
// Client will respond with a B1 command if header.flag is nonzero.

// 98 (C->S): Leave game
// Internal name: SndUpdateCharaDataV2 (SndUpdateCharaData in DCv1)
// Same format as 61 command. The server should update its view of the client's
// player data and remove the client from the game it's in (if any), but should
// NOT assign it to an available lobby. The client will send an 84 when it's
// ready to join a lobby.

// 99 (C->S): Server time accepted
// Internal name: SndPsoDirList
// No arguments
// This command's internal name suggests that it's actually a request for the
// ship select menu, but it's only sent as the response to a B1 command (server
// time) and the client doesn't set any state to indicate it's waiting for a
// ship select menu, so we just treat it as confirmation of a received B1
// command instead.

// 9A (C->S): Initial login (no password or client config)
// Internal name: RcvPsoRegistCheck
// Not used on DCv1 - that version uses 90 instead.

struct C_Login_DC_PC_V3_9A {
  ptext<char, 0x10> v1_serial_number;
  ptext<char, 0x10> v1_access_key;
  ptext<char, 0x10> serial_number;
  ptext<char, 0x10> access_key;
  le_uint32_t player_tag = 0x00010000;
  le_uint32_t guild_card_number = 0;
  le_uint32_t sub_version = 0;
  ptext<char, 0x30> serial_number2; // On DCv2, this is the hardware ID
  ptext<char, 0x30> access_key2;
  ptext<char, 0x30> email_address;
} __packed__;

// 9A (S->C): License verification result
// Internal name: RcvPsoRegistCheckV2
// The result code is sent in the header.flag field. Result codes:
// 00 = license ok (don't save to memory card; client responds with 9D/9E)
// 01 = registration required (client responds with a 9C command)
// 02 = license ok (save to memory card; client responds with 9D/9E)
// For all of the below cases, the client doesn't respond and displays a message
// box describing the error. When the player presses a button, the client then
// disconnects.
// 03 = access key invalid (125) (client deletes saved license info)
// 04 = serial number invalid (126) (client deletes saved license info)
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

// 9B (S->C): Secondary server init (non-BB, non-DCv1)
// Behaves exactly the same as 17 (S->C).

// 9B (S->C): Secondary server init (BB)
// Format is the same as 03 (and the client uses the same encryption afterward).
// The only differences that 9B has from 03:
// - 9B does not work during the data-server phase (before the client has
//   reached the ship select menu), whereas 03 does.
// - For command 9B, the copyright string must be
//   "PSO NEW PM Server. Copyright 1999-2002 SONICTEAM.".
// - The client will respond with a command DB instead of a command 93.

// 9C (C->S): Register
// Internal name: SndPsoRegist
// It appears PSO GC sends uninitialized data in the header.flag field here.

struct C_Register_DC_PC_V3_9C {
  le_uint64_t unused = 0;
  le_uint32_t sub_version = 0;
  uint8_t unused1 = 0;
  uint8_t language = 0;
  parray<uint8_t, 2> unused2;
  ptext<char, 0x30> serial_number; // On XB, this is the XBL gamertag
  ptext<char, 0x30> access_key; // On XB, this is the XBL user ID
  ptext<char, 0x30> password; // On XB, this contains "xbox-pso"
} __packed__;

struct C_Register_BB_9C {
  le_uint32_t sub_version = 0;
  uint8_t unused1 = 0;
  uint8_t language = 0;
  parray<uint8_t, 2> unused2;
  ptext<char, 0x30> username;
  ptext<char, 0x30> password;
  ptext<char, 0x30> game_tag; // "psopc2" on BB
} __packed__;

// 9C (S->C): Register result
// Internal name: RcvPsoRegistV2
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
  le_uint32_t player_tag = 0x00010000; // 0x00010000 if guild card is set (via 04)
  le_uint32_t guild_card_number = 0; // 0xFFFFFFFF if not set
  le_uint32_t unused1 = 0;
  le_uint32_t unused2 = 0;
  le_uint32_t sub_version = 0;
  uint8_t is_extended = 0; // If 1, structure has extended format
  uint8_t language = 0; // 0 = JP, 1 = EN, 2 = DE, 3 = FR, 4 = ES
  parray<uint8_t, 0x2> unused3; // Always zeroes
  ptext<char, 0x10> v1_serial_number;
  ptext<char, 0x10> v1_access_key;
  ptext<char, 0x10> serial_number; // On XB, this is the XBL gamertag
  ptext<char, 0x10> access_key; // On XB, this is the XBL user ID
  ptext<char, 0x30> serial_number2; // On XB, this is the XBL gamertag
  ptext<char, 0x30> access_key2; // On XB, this is the XBL user ID
  ptext<char, 0x10> name;
} __packed__;
struct C_LoginExtended_DC_GC_9D : C_Login_DC_PC_GC_9D {
  SC_MeetUserExtension<char> extension;
} __packed__;
struct C_LoginExtended_PC_9D : C_Login_DC_PC_GC_9D {
  SC_MeetUserExtension<char16_t> extension;
} __packed__;

// 9E (C->S): Log in with client config (V3/BB)
// Not used on GC Episodes 1&2 Trial Edition.
// The extended version of this command is used in the same circumstances as
// when PSO PC uses the extended version of the 9D command.
// header.flag is 1 if the client has UDP disabled.

struct C_Login_GC_9E : C_Login_DC_PC_GC_9D {
  union ClientConfigFields {
    ClientConfig cfg;
    parray<uint8_t, 0x20> data;
    ClientConfigFields() : data() {}
  } __packed__ client_config;
} __packed__;
struct C_LoginExtended_GC_9E : C_Login_GC_9E {
  SC_MeetUserExtension<char> extension;
} __packed__;

struct C_Login_XB_9E : C_Login_GC_9E {
  XBNetworkLocation netloc;
  parray<le_uint32_t, 6> unknown_a1;
} __packed__;
struct C_LoginExtended_XB_9E : C_Login_XB_9E {
  SC_MeetUserExtension<char> extension;
} __packed__;

struct C_LoginExtended_BB_9E {
  le_uint32_t player_tag = 0x00010000;
  le_uint32_t guild_card_number = 0; // == serial_number when on newserv
  le_uint32_t sub_version = 0;
  le_uint32_t unknown_a1 = 0;
  le_uint32_t unknown_a2 = 0;
  ptext<char, 0x10> unknown_a3; // Always blank?
  ptext<char, 0x10> unknown_a4; // == "?"
  ptext<char, 0x10> unknown_a5; // Always blank?
  ptext<char, 0x10> unknown_a6; // Always blank?
  ptext<char, 0x30> username;
  ptext<char, 0x30> password;
  ptext<char, 0x10> guild_card_number_str;
  parray<le_uint32_t, 10> unknown_a7;
  SC_MeetUserExtension<char16_t> extension;
} __packed__;

// 9F (S->C): Request client config / security data (V3/BB)
// This command is not valid on PSO GC Episodes 1&2 Trial Edition, nor any
// pre-V3 PSO versions. Client will respond with a 9F command.
// No arguments

// 9F (C->S): Client config / security data response (V3/BB)
// The data is opaque to the client, as described at the top of this file.
// If newserv ever sent a 9F command (it currently does not), the response
// format here would be ClientConfig (0x20 bytes) on V3, or ClientConfigBB (0x28
// bytes) on BB. However, on BB, this returns the client config that was set by
// a preceding 04 command, not the config set by a preceding E6 command.

// A0 (C->S): Change ship
// Internal name: SndShipList
// This structure is for documentation only; newserv ignores the arguments here.

struct C_ChangeShipOrBlock_A0_A1 {
  le_uint32_t player_tag = 0x00010000;
  le_uint32_t guild_card_number = 0;
  parray<uint8_t, 0x10> unused;
} __packed__;

// A0 (S->C): Ship select menu
// Same as 07 command.

// A1 (C->S): Change block
// Internal name: SndBlockList
// Same format as A0. As with A0, newserv ignores the arguments.

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
  le_uint32_t menu_id = 0;
  le_uint32_t item_id = 0;
  ptext<CharT, 0x20> name;
  ptext<CharT, ShortDescLength> short_description;
} __packed__;
struct S_QuestMenuEntry_PC_A2_A4 : S_QuestMenuEntry<char16_t, 0x70> {
} __packed__;
struct S_QuestMenuEntry_DC_GC_A2_A4 : S_QuestMenuEntry<char, 0x70> {
} __packed__;
struct S_QuestMenuEntry_XB_A2_A4 : S_QuestMenuEntry<char, 0x80> {
} __packed__;
struct S_QuestMenuEntry_BB_A2_A4 : S_QuestMenuEntry<char16_t, 0x7A> {
} __packed__;

// A3 (S->C): Quest information
// Same format as 1A/D5 command (plain text)

// A4 (S->C): Download quest menu
// Internal name: RcvQuestList
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
// Internal name: RcvVMDownLoadHead
// Same format as 44. See the description of 44 for some notes on the
// differences between the two commands.
// Like the 44 command, the client->server form of this command is only used on
// V3 and BB.

// A7: Write download file
// Internal name: RcvVMDownLoad
// Same format as 13.
// Like the 13 command, the client->server form of this command is only used on
// V3 and BB.

// A8: Invalid command

// A9 (C->S): Quest menu closed (canceled)
// Internal name: SndQuestEnd
// No arguments
// This command is sent when the in-game quest menu (A2) is closed. When the
// download quest menu is closed, either by downloading a quest or canceling,
// the client sends A0 instead. The existence of the A0 response on the download
// case makes sense, because the client may not be in a lobby and the server may
// need to send another menu or redirect the client. But for the online quest
// menu, the client is already in a game and can move normally after canceling
// the quest menu, so it's not obvious why A9 is needed at all. newserv (and
// probably all other private servers) ignores it.
// Curiously, PSO GC sends uninitialized data in header.flag.

// AA (C->S): Update quest statistics (V3/BB)
// This command is used in Maximum Attack 2, but its format is unlikely to be
// specific to that quest. The structure here represents the only instance I've
// seen so far.
// The server should respond with an AB command.
// This command is likely never sent by PSO GC Episodes 1&2 Trial Edition,
// because the following command (AB) is definitely not valid on that version.

struct C_UpdateQuestStatistics_V3_BB_AA {
  le_uint16_t quest_internal_id = 0;
  le_uint16_t unused = 0;
  le_uint16_t request_token = 0;
  le_uint16_t unknown_a1 = 0;
  le_uint32_t unknown_a2 = 0;
  le_uint32_t kill_count = 0;
  le_uint32_t time_taken = 0; // in seconds
  parray<le_uint32_t, 5> unknown_a3;
} __packed__;

// AB (S->C): Confirm update quest statistics (V3/BB)
// This command is not valid on PSO GC Episodes 1&2 Trial Edition.

struct S_ConfirmUpdateQuestStatistics_V3_BB_AB {
  le_uint16_t unknown_a1 = 0; // 0
  be_uint16_t unknown_a2 = 0; // Probably actually unused
  le_uint16_t request_token = 0; // Should match token sent in AA command
  le_uint16_t unknown_a3 = 0; // Schtserv always sends 0xBFFF here
} __packed__;

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
// Internal name: RcvEmergencyCall
// Same format as 01 command. This command is supported on DCv1 and all later
// versions, but not on prototype versions or DC NTE.
// The message appears as an overlay on the right side of the screen. The player
// doesn't do anything to dismiss it; it will disappear after a few seconds.

// B1 (C->S): Request server time
// Internal name: GetServerTime
// No arguments
// Server will respond with a B1 command.

// B1 (S->C): Server time
// Internal name: RcvServerTime
// This command is supported on DCv1 and all later versions, but not on
// prototype versions or DC NTE.
// Contents is a string like "%Y:%m:%d: %H:%M:%S.000" (the space is not a typo).
// For example: 2022:03:30: 15:36:42.000
// It seems the client ignores the date part and the milliseconds part; only the
// hour, minute, and second fields are actually used.
// This command can be sent even if it's not requested by the client (with B1).
// For example, some servers send this command every time a client joins a game.
// Client will respond with a 99 command.

// B2 (S->C): Execute code and/or checksum memory (DCv2 and all later versions)
// Internal name: RcvProgramPatch
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

// newserv supports exploiting a bug in the USA version of Episode 3, which
// re-enables the use of this command on that version of the game. See
// system/ppc/Episode3USAQuestBufferOverflow.s for further details.

struct S_ExecuteCode_B2 {
  // If code_size == 0, no code is executed, but checksumming may still occur.
  // In that case, this structure is the entire body of the command (no footer
  // is sent).
  le_uint32_t code_size = 0; // Size of code (following this struct) and footer
  le_uint32_t checksum_start = 0; // May be null if size is zero
  le_uint32_t checksum_size = 0; // If zero, no checksum is computed
  // The code immediately follows, ending with an S_ExecuteCode_Footer_B2
} __packed__;

template <bool IsBigEndian>
struct S_ExecuteCode_Footer_B2 {
  using U16T = typename std::conditional<IsBigEndian, be_uint16_t, le_uint16_t>::type;
  using U32T = typename std::conditional<IsBigEndian, be_uint32_t, le_uint32_t>::type;

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
  U32T relocations_offset = 0; // Relative to code base (after checksum_size)
  U32T num_relocations = 0;
  parray<U32T, 2> unused1;
  // entrypoint_offset is doubly indirect - it points to a pointer to a 32-bit
  // value that itself is the actual entrypoint. This is presumably done so the
  // entrypoint can be optionally relocated.
  U32T entrypoint_addr_offset = 0; // Relative to code base (after checksum_size).
  parray<U32T, 3> unused2;
} __packed__;

struct S_ExecuteCode_Footer_GC_B2 : S_ExecuteCode_Footer_B2<true> {
} __packed__;
struct S_ExecuteCode_Footer_DC_PC_XB_BB_B2 : S_ExecuteCode_Footer_B2<false> {
} __packed__;

// B3 (C->S): Execute code and/or checksum memory result
// Not used on versions that don't support the B2 command (see above).

struct C_ExecuteCodeResult_B3 {
  // On DC, return_value has the value in r0 when the function returns.
  // On PC, return_value is always 0.
  // On GC, return_value has the value in r3 when the function returns.
  // On XB and BB, return_value has the value in eax when the function returns.
  // If code_size was 0 in the B2 command, return_value is always 0.
  le_uint32_t return_value = 0;
  le_uint32_t checksum = 0; // 0 if no checksum was computed
} __packed__;

// B4: Invalid command
// B5: Invalid command
// B6: Invalid command

// B7 (S->C): Rank update (Episode 3)

struct S_RankUpdate_GC_Ep3_B7 {
  le_uint32_t rank = 0;
  ptext<char, 0x0C> rank_text;
  le_uint32_t meseta = 0;
  le_uint32_t max_meseta = 0;
  le_uint32_t unlocked_jukebox_songs = 0xFFFFFFFF;
} __packed__;

// B7 (C->S): Confirm rank update (Episode 3)
// No arguments
// The client sends this after it receives a B7 from the server.

// B8 (S->C): Update card definitions (Episode 3)
// Contents is a single le_uint32_t specifying the size of the (PRS-compressed)
// data, followed immediately by the data. The maximum size of the compressed
// data is 0x9000 bytes, although the receive buffer size limit applies first in
// practice, which limits this to 0x7BF8 bytes. The maximum size of the
// decompressed data is 0x36EC0 bytes.
// Note: PSO BB accepts this command as well, but ignores it.

// B8 (C->S): Confirm updated card definitions (Episode 3)
// No arguments
// The client sends this after it receives a B8 from the server.

// B9 (S->C): Update CARD lobby media (Episode 3)
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
  le_uint32_t type = 0;
  // which is a bit field specifying which positions to set. The bits are:
  // 00000001: South above-counter banner (facing away from teleporters)
  // 00000002: West above-counter banner
  // 00000004: North above-counter banner (facing toward jukebox)
  // 00000008: East above-counter banner
  // 00000010: Banner above west (left) teleporter
  // 00000020: Banner above east (right) teleporter
  // 00000040: Banner at south end of lobby (opposite the jukebox)
  // 00000080: Immediately left of 00000040
  // 00000100: Immediately right of 00000040
  // 00000200: Same as 00000080, but further left and at a slight inward angle
  // 00000400: Same as 00000100, but further right and at a slight inward angle
  // 00000800: Banner at north end of lobby, above the jukebox
  // 00001000: Immediately right of 00000800
  // 00002000: Immediately left of 00000800
  // 00004000: Same as 00001000, but further right and at a slight inward angle
  // 00008000: Same as 00002000, but further left and at a slight inward angle
  // 00010000: Banners at west AND east ends of lobby, next to battle tables
  // 00020000: Immediately left of 00001000 (2 banners)
  // 00040000: Immediately right of 00001000 (2 banners)
  // 00080000: Banners on southwest AND southeast ends of the lobby
  // 00100000: Banners on south-southwest AND south-southeast ends of the lobby
  // 00200000: Floor banners in front of the counter (4 banners)
  // 00400000: Banners on both small walls in front of the battle tables
  // 00800000: On southern platform
  // 01000000: In front of jukebox
  // 02000000: In western battle table corner (next to 4-player tables)
  // 04000000: In eastern battle table corner (next to 2-player tables)
  // 08000000: In southeastern battle table corner (next to 2-player tables)
  // 10000000: In southwestern battle table corner (next to 4-player tables)
  // 20000000: Just north-northwest of the counter
  // 40000000: In front of the small wall in front of the 2-player battle tables
  // 80000000: Inside the lobby counter, facing southeast
  // Positions 00800000 and above appear to be intended for models and not
  // banners - if a banner is sent in these locations, it appears sideways and
  // halfway submerged in the floor, and has no collision. Furthermore, it seems
  // that up to 8 different banners or models may be set simultaneously (though
  // each may appear in more than one position). If 8 B9 commands have been
  // received, further B9 commands are ignored.
  le_uint32_t which = 0x00000000;
  // This field specifies the size of the compressed data. The uncompressed size
  // is not sent anywhere in this command.
  le_uint16_t size = 0;
  le_uint16_t unused = 0;
  // The PRS-compressed data immediately follows this header. The maximum size
  // of the compressed data is 0x3800 bytes, and it must decompress to fewer
  // than 0x37000 bytes of output. If either of these limits are violated, the
  // client ignores the command.
} __packed__;

// B9 (C->S): Confirm received B9 (Episode 3)
// No arguments
// This command is not valid on Episode 3 Trial Edition.
// The client sends this even if it ignores the contents of a B9 command (if
// the data size was too large, or if there were already 8 banners/models).

// BA: Meseta transaction (Episode 3)
// This command is not valid on Episode 3 Trial Edition.
// header.flag specifies the transaction purpose. Specific known values:
// 00 = unknown
// 01 = Lobby jukebox object created (C->S; always has a value of 0;
//      response request_token must match the last token sent by client)
// 02 = Spend meseta (at e.g. lobby jukebox or Pinz's shop) (C->S)
// 03 = Spend meseta response (S->C; request_token must match the last token
//      sent by client)
// 04 = unknown (C->S; request_token must match the last token sent by client)

struct C_Meseta_GC_Ep3_BA {
  le_uint32_t transaction_num = 0;
  le_uint32_t value = 0;
  le_uint32_t request_token = 0;
} __packed__;

struct S_Meseta_GC_Ep3_BA {
  le_uint32_t remaining_meseta = 0;
  le_uint32_t total_meseta_awarded = 0;
  le_uint32_t request_token = 0; // Should match the token sent by the client
} __packed__;

// BB (S->C): Tournament match information (Episode 3)
// This command is not valid on Episode 3 Trial Edition. Because of this, it
// must have been added fairly late in development, but it seems to be unused,
// perhaps because the E1/E3 commands are generally more useful... but the E1/E3
// commands exist in the Trial Edition! So why was this added? Was it just never
// finished? We may never know...
// header.flag is the number of valid match entries.

struct S_TournamentMatchInformation_GC_Ep3_BB {
  ptext<char, 0x20> tournament_name;
  struct TeamEntry {
    le_uint16_t win_count = 0;
    le_uint16_t is_active = 0;
    ptext<char, 0x20> name;
  } __packed__;
  parray<TeamEntry, 0x20> team_entries;
  le_uint16_t num_teams = 0;
  le_uint16_t unknown_a3 = 0; // Probably actually unused
  struct MatchEntry {
    parray<char, 0x20> name;
    uint8_t locked = 0;
    uint8_t count = 0;
    uint8_t max_count = 0;
    uint8_t unused = 0;
  } __packed__;
  parray<MatchEntry, 0x40> match_entries;
} __packed__;

// BC: Invalid command
// BD: Invalid command
// BE: Invalid command
// BF: Invalid command

// C0 (C->S): Request choice search options (DCv2 and later versions)
// Internal name: GetChoiceList
// No arguments
// Server should respond with a C0 command (described below).

// C0 (S->C): Choice search options (DCv2 and later versions)
// Internal name: RcvChoiceList

// Command is a list of these; header.flag is the entry count (incl. top-level).
template <typename ItemIDT, typename CharT>
struct S_ChoiceSearchEntry {
  // Category IDs are nonzero; if the high byte of the ID is nonzero then the
  // category can be set by the user at any time; otherwise it can't.
  ItemIDT parent_category_id = 0; // 0 for top-level categories
  ItemIDT category_id = 0;
  ptext<CharT, 0x1C> text;
} __packed__;
struct S_ChoiceSearchEntry_DC_C0 : S_ChoiceSearchEntry<le_uint32_t, char> {
} __packed__;
struct S_ChoiceSearchEntry_V3_C0 : S_ChoiceSearchEntry<le_uint16_t, char> {
} __packed__;
struct S_ChoiceSearchEntry_PC_BB_C0 : S_ChoiceSearchEntry<le_uint16_t, char16_t> {
} __packed__;

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

// C1 (C->S): Create game (DCv2 and later versions)
// Internal name: SndCreateGame

template <typename CharT>
struct C_CreateGame_DCNTE {
  // menu_id and item_id are only used for the E7 (create spectator team) form
  // of this command
  le_uint32_t menu_id;
  le_uint32_t item_id;
  ptext<CharT, 0x10> name;
  ptext<CharT, 0x10> password;
} __packed__;

template <typename CharT>
struct C_CreateGame : C_CreateGame_DCNTE<CharT> {
  uint8_t difficulty = 0; // 0-3 (always 0 on Episode 3)
  uint8_t battle_mode = 0; // 0 or 1 (always 0 on Episode 3)
  // Note: Episode 3 uses the challenge mode flag for view battle permissions.
  // 0 = view battle allowed; 1 = not allowed
  uint8_t challenge_mode = 0; // 0 or 1
  // Note: According to the Sylverant wiki, in v2-land, the episode field has a
  // different meaning: if set to 0, the game can be joined by v1 and v2
  // players; if set to 1, it's v2-only.
  uint8_t episode = 0; // 1-4 on V3+ (3 on Episode 3); unused on DC/PC
} __packed__;
struct C_CreateGame_DC_V3_0C_C1_Ep3_EC : C_CreateGame<char> {
} __packed__;
struct C_CreateGame_PC_C1 : C_CreateGame<char16_t> {
} __packed__;

struct C_CreateGame_BB_C1 : C_CreateGame<char16_t> {
  uint8_t solo_mode = 0;
  parray<uint8_t, 3> unused2;
} __packed__;

// C2 (C->S): Set choice search parameters (DCv2 and later versions)
// Internal name: PutChoiceList
// Server does not respond.
// The ChoiceSearchConfig structure is defined in Player.hh.

struct C_ChoiceSearchSelections_DC_C2_C3 : ChoiceSearchConfig<le_uint32_t> {
} __packed__;
struct C_ChoiceSearchSelections_PC_V3_BB_C2_C3 : ChoiceSearchConfig<le_uint16_t> {
} __packed__;

// C3 (C->S): Execute choice search (DCv2 and later versions)
// Internal name: SndChoiceSeq
// Same format as C2. The disabled field is unused.
// Server should respond with a C4 command.

// C4 (S->C): Choice search results (DCv2 and later versions)
// Internal name: RcvChoiceAns

// Command is a list of these; header.flag is the entry count
struct S_ChoiceSearchResultEntry_V3_C4 {
  le_uint32_t guild_card_number = 0;
  ptext<char, 0x10> name; // No language marker, as usual on V3
  ptext<char, 0x20> info_string; // Usually something like "<class> Lvl <level>"
  // Format is stricter here; this is "LOBBYNAME,BLOCKNUM,SHIPNAME"
  // If target is in game, for example, "Game Name,BLOCK01,Alexandria"
  // If target is in lobby, for example, "BLOCK01-1,BLOCK01,Alexandria"
  ptext<char, 0x34> locator_string;
  // Server IP and port for "meet user" option
  le_uint32_t server_ip = 0;
  le_uint16_t server_port = 0;
  le_uint16_t unused1 = 0;
  le_uint32_t menu_id = 0;
  le_uint32_t lobby_id = 0; // These two are guesses
  le_uint32_t game_id = 0; // Zero if target is in a lobby rather than a game
  parray<uint8_t, 0x58> unused2;
} __packed__;

// C5 (S->C): Challenge rank update (DCv2 and later versions)
// Internal name: RcvChallengeData
// header.flag = entry count
// The server sends this command when a player joins a lobby to update the
// challenge mode records of all the present players.
// Entry format is PlayerChallengeDataV3 or PlayerChallengeDataBB.
// newserv currently doesn't send this command at all because the V3 and
// BB formats aren't fully documented.
// TODO: Figure out where the text is in those formats, write appropriate
// conversion functions, and implement the command. Don't forget to overwrite
// the client_id field in each entry before sending.

// C6 (C->S): Set blocked senders list (V3/BB)
// The command always contains the same number of entries, even if the entries
// at the end are blank (zero).

template <size_t Count>
struct C_SetBlockedSenders_C6 {
  parray<le_uint32_t, Count> blocked_senders;
} __packed__;

struct C_SetBlockedSenders_V3_C6 : C_SetBlockedSenders_C6<30> {
} __packed__;
struct C_SetBlockedSenders_BB_C6 : C_SetBlockedSenders_C6<28> {
} __packed__;

// C7 (C->S): Enable simple mail auto-reply (V3/BB)
// Same format as 1A/D5 command (plain text).
// Server does not respond

// C8 (C->S): Disable simple mail auto-reply (V3/BB)
// No arguments
// Server does not respond

// C9 (C->S): Unknown (XB)
// No arguments except header.flag

// C9: Broadcast command (Episode 3)
// Same as 60, but should be forwarded only to Episode 3 clients.
// newserv uses this command for all server-generated events (in response to CA
// commands), except for map data requests. This differs from Sega's original
// implementation, which sent CA responses via 60 commands instead.

// CA (C->S): Server data request (Episode 3)
// The CA command format is the same as that of the 6xB3 commands, and the
// subsubcommands formats are shared as well. Unlike the 6x commands, the server
// is expected to respond to the command appropriately instead of forwarding it.
// Because the formats are shared, the 6xB3 commands are also referred to as CAx
// commands in the comments and structure names.

// CB: Broadcast command (Episode 3)
// Same as 60, but only send to Episode 3 clients.
// This command is identical to C9, except that CB is not valid on Episode 3
// Trial Edition (whereas C9 is valid).
// TODO: Presumably this command is meant to be forwarded from spectator teams
// to the primary team, whereas 6x and C9 commands are not. I haven't verified
// this or implemented this behavior yet though.

// CC (S->C): Confirm tournament entry (Episode 3)
// This command is not valid on Episode 3 Trial Edition.
// header.flag determines the client's registration state - 1 if the client is
// registered for the tournament, 0 if not.
// This command controls what's shown in the Check Tactics pane in the pause
// menu. If the client is registered (header.flag==1), the option is enabled and
// the bracket data in the command is shown there, and a third pane on the
// Status item shows the other information (tournament name, ship name, and
// start time). If the client is not registered, the Check Tactics option is
// disabled and the Status item has only two panes.

struct S_ConfirmTournamentEntry_GC_Ep3_CC {
  ptext<char, 0x40> tournament_name;
  le_uint16_t num_teams = 0;
  le_uint16_t unknown_a1 = 0;
  le_uint16_t unknown_a2 = 0;
  le_uint16_t unknown_a3 = 0;
  ptext<char, 0x20> server_name;
  ptext<char, 0x20> start_time; // e.g. "15:09:30" or "13:03 PST"
  struct TeamEntry {
    le_uint16_t win_count = 0;
    le_uint16_t is_active = 0;
    ptext<char, 0x20> name;
  } __packed__;
  parray<TeamEntry, 0x20> team_entries;
} __packed__;

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
// delete/create subcommands?
// At any point if an error occurs, either client may send a D4 00, which
// cancels the entire sequence. The server should then send D4 00 to both
// clients.
// TODO: The server should presumably also send a D4 00 if either client
// disconnects during the sequence.

struct SC_TradeItems_D0_D3 { // D0 when sent by client, D3 when sent by server
  le_uint16_t target_client_id = 0;
  le_uint16_t item_count = 0;
  // Note: PSO GC sends uninitialized data in the unused entries of this
  // command. newserv parses and regenerates the item data when sending D3,
  // which effectively erases the uninitialized data.
  parray<ItemData, 0x20> items;
} __packed__;

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
// they are closed.) In some of these versions, there is a bug that sets an
// incorrect interaction mode when the message box is closed while the player is
// in the lobby; some servers (e.g. Schtserv) send a lobby welcome message
// anyway, along with an 01 (lobby message box) which properly sets the
// interaction mode when closed.

// D7 (C->S): Request GBA game file (V3)
// header.flag is used, but it's not clear for what.
// The server should send the requested file using A6/A7 commands.
// This command exists on XB as well, but it presumably is never sent by the
// client.

struct C_GBAGameRequest_V3_D7 {
  ptext<char, 0x10> filename;
} __packed__;

// D7 (S->C): Unknown (V3/BB)
// No arguments
// This command is not valid on PSO GC Episodes 1&2 Trial Edition.
// On PSO V3, this command does... something. The command isn't *completely*
// ignored: it sets a global state variable, but it's not clear what that
// variable does. The variable is set to 0 when the client requests a GBA game
// (by sending a D7 command), and set to 1 when the client receives a D7
// command. The S->C D7 command may be used for declining a download or
// signaling an error of some sort.
// PSO BB accepts but completely ignores this command.

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
} __packed__;
struct S_InfoBoardEntry_BB_D8 : S_InfoBoardEntry_D8<char16_t> {
} __packed__;
struct S_InfoBoardEntry_V3_D8 : S_InfoBoardEntry_D8<char> {
} __packed__;

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
  le_uint32_t sub_version = 0;
  ptext<char, 0x30> serial_number2; // On XB, this is the XBL gamertag
  ptext<char, 0x30> access_key2; // On XB, this is the XBL user ID
  ptext<char, 0x30> password; // On XB, this contains "xbox-pso"
} __packed__;

// Note: This login pathway generally isn't used on BB (and isn't supported at
// all during the data server phase). All current servers use 03/93 instead.
struct C_VerifyLicense_BB_DB {
  // Note: These four fields are likely the same as those used in BB's 9E
  ptext<char, 0x10> unknown_a3; // Always blank?
  ptext<char, 0x10> unknown_a4; // == "?"
  ptext<char, 0x10> unknown_a5; // Always blank?
  ptext<char, 0x10> unknown_a6; // Always blank?
  le_uint32_t sub_version = 0;
  ptext<char, 0x30> username;
  ptext<char, 0x30> password;
  ptext<char, 0x30> game_tag; // "psopc2"
} __packed__;

// DC: Player menu state (Episode 3)
// No arguments. It seems the client expects the server to respond with another
// DC command, the contents and flag of which are ignored entirely - all it does
// is set a global flag on the client. This could be the mechanism for waiting
// until all players are at the counter, like how AC (quest barrier) works. I
// haven't spent any time investigating what this actually does; newserv just
// immediately responds to any DC from an Episode 3 client.

// DC: Guild card data (BB)

struct S_GuildCardHeader_BB_01DC {
  le_uint32_t unknown = 1;
  le_uint32_t filesize = 0x0000D590;
  le_uint32_t checksum = 0; // CRC32 of entire guild card file (0xD590 bytes)
} __packed__;

struct S_GuildCardFileChunk_02DC {
  le_uint32_t unknown = 0; // 0
  le_uint32_t chunk_index = 0;
  uint8_t data[0x6800]; // Command may be shorter if this is the last chunk
} __packed__;

struct C_GuildCardDataRequest_BB_03DC {
  le_uint32_t unknown = 0;
  le_uint32_t chunk_index = 0;
  le_uint32_t cont = 0;
} __packed__;

// DD (S->C): Send quest state to joining player (BB)
// When a player joins a game with a quest already in progress, the server
// should send this command to the leader. header.flag is the client ID that the
// leader should send quest state to; the leader will then send a series of
// target commands (62/6D) that the server can forward to the joining player.
// No other arguments

// DE (S->C): Rare monster list (BB)

struct S_RareMonsterList_BB_DE {
  // Unused entries are set to FFFF
  parray<le_uint16_t, 0x10> enemy_ids;
} __packed__;

// DF (C->S): Unknown (BB)
// This command has many subcommands. It's not clear what any of them do.

struct C_Unknown_BB_01DF {
  le_uint32_t unknown_a1 = 0;
} __packed__;

struct C_Unknown_BB_02DF {
  le_uint32_t unknown_a1 = 0;
} __packed__;

struct C_Unknown_BB_03DF {
  le_uint32_t unknown_a1 = 0;
} __packed__;

struct C_Unknown_BB_04DF {
  le_uint32_t unknown_a1 = 0;
} __packed__;

struct C_Unknown_BB_05DF {
  le_uint32_t unknown_a1 = 0;
  ptext<char16_t, 0x0C> unknown_a2;
} __packed__;

struct C_Unknown_BB_06DF {
  parray<le_uint32_t, 3> unknown_a1;
} __packed__;

struct C_Unknown_BB_07DF {
  le_uint32_t unused1 = 0xFFFFFFFF;
  le_uint32_t unused2 = 0; // ALways 0
  parray<le_uint32_t, 5> unknown_a1;
} __packed__;

// E0 (S->C): Tournament list (Episode 3)
// The client will send 09 and 10 commands to inspect or enter a tournament. The
// server should respond to an 09 command with an E3 command; the server should
// respond to a 10 command with an E2 command.

// header.flag is the count of filled-in entries.
struct S_TournamentList_GC_Ep3_E0 {
  struct Entry {
    le_uint32_t menu_id = 0;
    le_uint32_t item_id = 0;
    uint8_t unknown_a1 = 0;
    uint8_t locked = 0; // If nonzero, the lock icon appears in the menu
    // Values for the state field:
    // 00 = Preparing
    // 01 = 1st Round
    // 02 = 2nd Round
    // 03 = 3rd Round
    // 04 = Semifinals
    // 05 = Entries no longer accepted
    // 06 = Finals
    // 07 = Preparing for Battle
    // 08 = Battle in progress
    // 09 = Preparing to view Battle
    // 0A = Viewing a Battle
    // Values beyond 0A don't appear to cause problems, but cause strings to
    // appear that are obviously not intended to appear in the tournament list,
    // like "View the board" and "Board: Write". (In fact, some of the strings
    // listed above may be unintended for this menu as well.)
    uint8_t state = 0;
    uint8_t unknown_a2 = 0;
    le_uint32_t start_time = 0; // In seconds since Unix epoch
    ptext<char, 0x20> name;
    le_uint16_t num_teams = 0;
    le_uint16_t max_teams = 0;
    le_uint16_t unknown_a3 = 0;
    le_uint16_t unknown_a4 = 0;
  } __packed__;
  parray<Entry, 0x20> entries;
} __packed__;

// E0 (C->S): Request team and key config (BB)
// No arguments. The server should respond with an E1 or E2 command.

// E1 (S->C): Game information (Episode 3)
// The header.flag argument determines which fields are valid (and which panes
// should be shown in the information window). The values are the same as for
// the E3 command, but each value only makes sense for one command. That is, 00,
// 01, and 04 should be used with the E1 command, while 02, 03, and 05 should be
// used with the E3 command. See the E3 command for descriptions of what each
// flag value means.

struct S_GameInformation_GC_Ep3_E1 {
  /* 0004 */ ptext<char, 0x20> game_name;
  struct PlayerEntry {
    ptext<char, 0x10> name; // From disp.name
    ptext<char, 0x20> description; // Usually something like "FOmarl CLv30 J"
  } __packed__;
  /* 0024 */ parray<PlayerEntry, 4> player_entries;
  /* 00E4 */ parray<uint8_t, 0x20> unknown_a3;
  /* 0104 */ Episode3::Rules rules;
  /* 0114 */ parray<uint8_t, 4> unknown_a4;
  /* 0118 */ parray<PlayerEntry, 8> spectator_entries;
} __packed__;

// E1 (S->C): Team and key config missing? (BB)
// This seems to take the place of 00E2 in certain cases. Perhaps it was used
// when a client hadn't logged in before and didn't have a team or key config,
// so the client should use appropriate defaults.

struct S_TeamAndKeyConfigMissing_00E1_BB {
  // If success is not equal to 1, the client shows a message saying "Forced
  // server disconnect (907)" and disconnects. Otherwise, the client proceeeds
  // as if it had received an 00E2 command, and sends its first 00E3.
  le_uint32_t success;
} __packed__;

// E2 (C->S): Tournament control (Episode 3)
// No arguments (in any of its forms) except header.flag, which determines ths
// command's meaning. Specifically:
//   flag=00: request tournament list (server responds with E0)
//   flag=01: check tournament (server responds with E2)
//   flag=02: cancel tournament entry (server responds with CC)
//   flag=03: create tournament spectator team (server responds with E0)
//   flag=04: join tournament spectator team (server responds with E0)
// In case 02, the resulting CC command has header.flag = 0 to indicate the
// player is no longer registered.
// In cases 03 and 04, the client handles follow-ups differently from the 00
// case. In case 04, the client will send a 10 (to which the server responds
// with an E2), but when a choice is made from that menu, the client sends E6 01
// instead of 10. In case 03, the flow is similar, but the client sends E7
// instead of E6 01.
// newserv responds with a standard ship info box (11), but this seems not to be
// intended since it partially overlaps some windows.

// E2 (S->C): Tournament entry list (Episode 3)
// Client may send 09 commands if the player presses X. It's not clear what the
// server should respond with in this case.
// If the player selects an entry slot, client will respond with a long-form 10
// command (the Flag03 variant); in this case, unknown_a1 is the team name, and
// password is the team password. The server should respond to that with a CC
// command.

struct S_TournamentEntryList_GC_Ep3_E2 {
  le_uint16_t players_per_team = 0;
  le_uint16_t unused = 0;
  struct Entry {
    le_uint32_t menu_id = 0;
    le_uint32_t item_id = 0;
    uint8_t unknown_a1 = 0;
    // If locked is nonzero, a lock icon appears next to this team and the
    // player is prompted for a password if they select this team.
    uint8_t locked = 0;
    // State values:
    // 00 = empty (team_name is ignored; entry is selectable)
    // 01 = present, joinable (team_name renders in white)
    // 02 = present, finalized (team_name renders in yellow)
    // If state is any other value, the entry renders as if its state were 02,
    // but cannot be selected at all (the menu cursor simply skips over it).
    uint8_t state = 0;
    uint8_t unknown_a2 = 0;
    ptext<char, 0x20> name;
  } __packed__;
  parray<Entry, 0x20> entries;
} __packed__;

// E2 (S->C): Team and key config (BB)
// See KeyAndTeamConfigBB in Player.hh for format

// E3 (S->C): Game or tournament info (Episode 3)
// The header.flag argument determines which fields are valid (and which panes
// should be shown in the information window). The values are:
// flag=00: Opponents pane only
// flag=01: Opponents and Rules panes
// flag=02: Rules and bracket panes (the bracket pane uses the tournament's name
//          as its title)
// flag=03: Opponents, Rules, and bracket panes
// flag=04: Spectators and Opponents pane
// flag=05: Spectators, Opponents, Rules, and bracket panes
// Sending other values in the header.flag field results in a blank info window
// with unintended strings appearing in the window title.
// Presumably the cases above would be used in different scenarios, probably:
// 00: When inspecting a non-tournament game with no battle in progress
// 01: When inspecting a non-tournament game with a battle in progress
// 02: When inspecting tournaments that have not yet started
// 03: When inspecting a tournament match
// 04: When inspecting a non-tournament spectator team
// 05: When inspecting a tournament spectator team
// The 00, 01, and 04 cases don't really make sense, because the E1 command is
// more appropriate for inspecting non-tournament games.

struct S_TournamentGameDetails_GC_Ep3_E3 {
  // These fields are used only if the Rules pane is shown
  /* 0004/032C */ ptext<char, 0x20> name;
  /* 0024/034C */ ptext<char, 0x20> map_name;
  /* 0044/036C */ Episode3::Rules rules;

  /* 0054/037C */ parray<uint8_t, 4> unknown_a1;

  // This field is used only if the bracket pane is shown
  struct BracketEntry {
    le_uint16_t win_count = 0;
    le_uint16_t is_active = 0;
    ptext<char, 0x18> team_name;
    parray<uint8_t, 8> unused;
  } __packed__;
  /* 0058/0380 */ parray<BracketEntry, 0x20> bracket_entries;

  // This field is used only if the Opponents pane is shown. If players_per_team
  // is 2, all fields are shown; if player_per_team is 1, team_name and
  // players[1] is ignored (only players[0] is shown).
  struct PlayerEntry {
    ptext<char, 0x10> name;
    ptext<char, 0x20> description; // Usually something like "RAmarl CLv24 E"
  } __packed__;
  struct TeamEntry {
    ptext<char, 0x10> team_name;
    parray<PlayerEntry, 2> players;
  } __packed__;
  /* 04D8/0800 */ parray<TeamEntry, 2> team_entries;

  /* 05B8/08E0 */ le_uint16_t num_bracket_entries = 0;
  /* 05BA/08E2 */ le_uint16_t players_per_team = 0;
  /* 05BC/08E4 */ le_uint16_t unknown_a4 = 0;
  /* 05BE/08E6 */ le_uint16_t num_spectators = 0;
  /* 05C0/08E8 */ parray<PlayerEntry, 8> spectator_entries;
} __packed__;

// E3 (C->S): Player preview request (BB)

struct C_PlayerPreviewRequest_BB_E3 {
  le_uint32_t player_index = 0;
  le_uint32_t unused = 0;
} __packed__;

// E4: CARD lobby battle table state (Episode 3)
// When client sends an E4, server should respond with another E4 (but these
// commands have different formats).
// When the client has received an E4 command in which all entries have state 0
// or 2, the client will stop the player from moving and show a message saying
// that the game will begin shortly. The server should send a 64 command shortly
// thereafter.

// header.flag = seated state (1 = present, 0 = leaving)
struct C_CardBattleTableState_GC_Ep3_E4 {
  le_uint16_t table_number = 0;
  le_uint16_t seat_number = 0;
} __packed__;

// header.flag = table number
struct S_CardBattleTableState_GC_Ep3_E4 {
  struct Entry {
    // State values:
    // 0 = no player present
    // 1 = player present, not confirmed
    // 2 = player present, confirmed
    // 3 = player present, declined
    le_uint16_t state = 0;
    le_uint16_t unknown_a1 = 0;
    le_uint32_t guild_card_number = 0;
  } __packed__;
  parray<Entry, 4> entries;
} __packed__;

// E4 (S->C): Player choice or no player present (BB)

struct S_ApprovePlayerChoice_BB_00E4 {
  le_uint32_t player_index = 0;
  le_uint32_t result = 0; // 1 = approved
} __packed__;

struct S_PlayerPreview_NoPlayer_BB_00E4 {
  le_uint32_t player_index = 0;
  le_uint32_t error = 0; // 2 = no player present
} __packed__;

// E5 (C->S): Confirm CARD lobby battle table choice (Episode 3)
// header.flag specifies whether the client answered "Yes" (1) or "No" (0).

struct S_CardBattleTableConfirmation_GC_Ep3_E5 {
  le_uint16_t table_number = 0;
  le_uint16_t seat_number = 0;
} __packed__;

// E5 (S->C): Player preview (BB)
// E5 (C->S): Create character (BB)

struct SC_PlayerPreview_CreateCharacter_BB_00E5 {
  le_uint32_t player_index = 0;
  PlayerDispDataBBPreview preview;
} __packed__;

// E6 (C->S): Spectator team control (Episode 3)

// With header.flag == 0, this command has no arguments and is used for
// requesting the spectator team list. The server responds with an E6 command.

// With header.flag == 1, this command is used for joining a tournament
// spectator team. The following arguments are given in this form:

struct C_JoinSpectatorTeam_GC_Ep3_E6_Flag01 {
  le_uint32_t menu_id = 0;
  le_uint32_t item_id = 0;
} __packed__;

// E6 (S->C): Spectator team list (Episode 3)
// Same format as 08 command.

// E6 (S->C): Set guild card number and update client config (BB)
// BB clients have multiple client configs. This command sets the client config
// that is returned by the 93 commands, but does not affect the client config
// set by the 04 command (and returned in the 9E and 9F commands).

struct S_ClientInit_BB_00E6 {
  le_uint32_t error = 0;
  le_uint32_t player_tag = 0x00010000;
  le_uint32_t guild_card_number = 0;
  le_uint32_t team_id = 0;
  ClientConfigBB cfg;
  le_uint32_t caps = 0x00000102;
} __packed__;

// E7 (C->S): Create spectator team (Episode 3)
// This command is used to create speectator teams for both tournaments and
// regular games. The server should be able to tell these cases apart by the
// menu and/or item ID.

struct C_CreateSpectatorTeam_GC_Ep3_E7 {
  le_uint32_t menu_id = 0;
  le_uint32_t item_id = 0;
  ptext<char, 0x10> name;
  ptext<char, 0x10> password;
  le_uint32_t unused = 0;
} __packed__;

// E7 (S->C): Unknown (Episode 3)
// Same format as E2 command.

// E7: Save or load full player data (BB)
// See export_bb_player_data() in Player.cc for format.
// TODO: Verify full breakdown from send_E7 in BB disassembly.

// E8 (S->C): Join spectator team (Episode 3)
// header.flag = player count (including spectators)

struct S_JoinSpectatorTeam_GC_Ep3_E8 {
  parray<le_uint32_t, 0x20> variations; // 04-84; unused
  struct PlayerEntry {
    PlayerLobbyDataDCGC lobby_data; // 0x20 bytes
    PlayerInventory inventory; // 0x34C bytes
    PlayerDispDataDCPCV3 disp; // 0xD0 bytes
  } __packed__; // 0x43C bytes
  parray<PlayerEntry, 4> players; // 84-1174
  uint8_t client_id = 0;
  uint8_t leader_id = 0;
  uint8_t disable_udp = 1;
  uint8_t difficulty = 0;
  uint8_t battle_mode = 0;
  uint8_t event = 0;
  uint8_t section_id = 0;
  uint8_t challenge_mode = 0;
  le_uint32_t rare_seed = 0;
  uint8_t episode = 0;
  uint8_t unused2 = 1;
  uint8_t solo_mode = 0;
  uint8_t unused3 = 0;
  struct SpectatorEntry {
    le_uint32_t player_tag = 0;
    le_uint32_t guild_card_number = 0;
    ptext<char, 0x20> name;
    uint8_t present = 0;
    uint8_t unknown_a3 = 0;
    le_uint16_t level = 0;
    parray<le_uint32_t, 2> unknown_a5;
    parray<le_uint16_t, 2> unknown_a6;
  } __packed__; // 0x38 bytes
  // Somewhat misleadingly, this array also includes the players actually in the
  // battle - they appear in the first positions. Presumably the first 4 are
  // always for battlers, and the last 8 are always for spectators.
  parray<SpectatorEntry, 12> entries; // 1184-1424
  ptext<char, 0x20> spectator_team_name;
  // This field doesn't appear to be actually used by the game, but some servers
  // send it anyway (and the game presumably ignores it)
  parray<PlayerEntry, 8> spectator_players;
} __packed__;

// E8 (C->S): Guild card commands (BB)

// 01E8 (C->S): Check guild card file checksum

// This struct is for documentation purposes only; newserv ignores the contents
// of this command.
struct C_GuildCardChecksum_01E8 {
  le_uint32_t checksum = 0;
  le_uint32_t unused = 0;
} __packed__;

// 02E8 (S->C): Accept/decline guild card file checksum
// If needs_update is nonzero, the client will request the guild card file by
// sending an 03E8 command. If needs_update is zero, the client will skip
// downloading the guild card file and send a 04EB command (requesting the
// stream file) instead.

struct S_GuildCardChecksumResponse_BB_02E8 {
  le_uint32_t needs_update = 0;
  le_uint32_t unused = 0;
} __packed__;

// 03E8 (C->S): Request guild card file
// No arguments
// Server should send the guild card file data using DC commands.

// 04E8 (C->S): Add guild card
// Format is GuildCardBB (see Player.hh)

// 05E8 (C->S): Delete guild card

struct C_DeleteGuildCard_BB_05E8_08E8 {
  le_uint32_t guild_card_number = 0;
} __packed__;

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
  le_uint32_t guild_card_number = 0;
  ptext<char16_t, 0x58> comment;
} __packed__;

// 0AE8 (C->S): Set guild card position in list

struct C_MoveGuildCard_BB_0AE8 {
  le_uint32_t guild_card_number = 0;
  le_uint32_t position = 0;
} __packed__;

// E9 (S->C): Remove player from spectator team (Episode 3)
// Same format as 66/69 commands. Like 69 (and unlike 66), the disable_udp field
// is unused in command E9. When a spectator leaves a spectator team, the
// primary players should receive a 6xB4x52 command to update their spectator
// counts.

// EA (S->C): Timed message box (Episode 3)
// The message appears in the upper half of the screen; the box is as wide as
// the 1A/D5 box but is vertically shorter. The box cannot be dismissed or
// interacted with by the player in any way; it disappears by itself after the
// given number of frames.
// header.flag appears to be relevant - the handler's behavior is different if
// it's 1 (vs. any other value). There don't seem to be any in-game behavioral
// differences though.

struct S_TimedMessageBoxHeader_GC_Ep3_EA {
  le_uint32_t duration = 0; // In frames; 30 frames = 1 second
  // Message data follows here (up to 0x1000 chars)
} __packed__;

// EA: Team control (BB)

// 01EA (C->S): Create team

struct C_CreateTeam_BB_01EA {
  ptext<char16_t, 0x10> name;
} __packed__;

// 03EA (C->S): Add team member

struct C_AddOrRemoveTeamMember_BB_03EA_05EA {
  le_uint32_t guild_card_number = 0;
} __packed__;

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
} __packed__;

// 10EA: Delete team
// No arguments

// 11EA (C->S): Promote team member
// TODO: header.flag is used for this command. Figure out what it's for.

struct C_PromoteTeamMember_BB_11EA {
  le_uint32_t unknown_a1 = 0;
} __packed__;

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
} __packed__;

// 20EA (C->S): Unknown
// header.flag is used, but no other arguments

// EB (S->C): Add player to spectator team (Episode 3)
// Same format and usage as 65 and 68 commands, but sent to spectators in a
// spectator team.
// This command is used to add both primary players and spectators - if the
// client ID in .lobby_data is 0-3, it's a primary player, otherwise it's a
// spectator. (In the case of a primary player joining, the other primary
// players in the game receive a 65 command rather than an EB command to notify
// them of the joining player; in the case of a joining spectator, the primary
// players receive a 6xB4x52 instead.)

// 01EB (S->C): Send stream file index (BB)

// Command is a list of these; header.flag is the entry count.
struct S_StreamFileIndexEntry_BB_01EB {
  le_uint32_t size = 0;
  le_uint32_t checksum = 0; // CRC32 of file data
  le_uint32_t offset = 0; // offset in stream (== sum of all previous files' sizes)
  ptext<char, 0x40> filename;
} __packed__;

// 02EB (S->C): Send stream file chunk (BB)

struct S_StreamFileChunk_BB_02EB {
  le_uint32_t chunk_index = 0;
  uint8_t data[0x6800];
} __packed__;

// 03EB (C->S): Request a specific stream file chunk
// header.flag is the chunk index. Server should respond with a 02EB command.

// 04EB (C->S): Request stream file header
// No arguments
// Server should respond with a 01EB command.

// EC (C->S): Create game (Episode 3)
// Same format as C1; some fields are unused (e.g. episode, difficulty).

// EC (C->S): Leave character select (BB)

struct C_LeaveCharacterSelect_BB_00EC {
  // Reason codes:
  // 0 = canceled
  // 1 = recreate character
  // 2 = dressing room
  le_uint32_t reason = 0;
} __packed__;

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
  le_uint32_t option = 0; // 01ED
  parray<uint8_t, 0x4E0> symbol_chats; // 02ED
  parray<uint8_t, 0xA40> chat_shortcuts; // 03ED
  parray<uint8_t, 0x16C> key_config; // 04ED
  parray<uint8_t, 0x38> pad_config; // 05ED
  parray<uint8_t, 0x28> tech_menu; // 06ED
  parray<uint8_t, 0xE8> customize; // 07ED
  parray<uint8_t, 0x140> challenge_battle_config; // 08ED
} __packed__;

// EE: Trade cards (Episode 3)
// This command has different forms depending on the header.flag value; the flag
// values match the command numbers from the Episodes 1&2 trade window sequence.
// The sequence of events with the EE command also matches that of the Episodes
// 1&2 trade window; see the description of the D0 command above for details.

// EE D0 (C->S): Begin trade
struct SC_TradeCards_GC_Ep3_EE_FlagD0_FlagD3 {
  le_uint16_t target_client_id = 0;
  le_uint16_t entry_count = 0;
  struct Entry {
    le_uint32_t card_type = 0;
    le_uint32_t count = 0;
  } __packed__;
  parray<Entry, 4> entries;
} __packed__;

// EE D1 (S->C): Advance trade state
struct S_AdvanceCardTradeState_GC_Ep3_EE_FlagD1 {
  le_uint32_t unused = 0;
} __packed__;

// EE D2 (C->S): Trade can proceed
// No arguments

// EE D3 (S->C): Execute trade
// Same format as EE D0

// EE D4 (C->S): Trade failed
// EE D4 (S->C): Trade complete

struct S_CardTradeComplete_GC_Ep3_EE_FlagD4 {
  le_uint32_t success = 0; // 0 = failed, 1 = success, anything else = invalid
} __packed__;

// EE (S->C): Scrolling message (BB)
// Same format as 01. The message appears at the top of the screen and slowly
// scrolls to the left.

// EF (C->S): Join card auction (Episode 3)
// This command should be treated like AC (quest barrier); that is, when all
// players in the same game have sent an EF command, the server should send an
// EF back to them all at the same time to start the auction.

// EF (S->C): Start card auction (Episode 3)

struct S_StartCardAuction_GC_Ep3_EF {
  le_uint16_t points_available = 0;
  le_uint16_t unused = 0;
  struct Entry {
    le_uint16_t card_id = 0xFFFF; // Must be < 0x02F1
    le_uint16_t min_price = 0; // Must be > 0 and < 100
  } __packed__;
  parray<Entry, 0x14> entries;
} __packed__;

// EF (S->C): Set or disable shutdown command (BB)
// All variants of EF except 00EF cause the given Windows shell command to be
// run (via ShellExecuteA) just before the game exits normally. There can be at
// most one shutdown command at a time; a later EF command will overwrite the
// previous EF command's effects. The 00EF command deletes the previous shutdown
// command if any was present, causing no command to run when the game closes.
// There is no indication to the player when a shutdown command has been set.

// This command is likely just a vestigial debugging feature that Sega left in,
// but it presents a fairly obvious security risk. There is no way for the
// server to know whether an EF command it sent has actually executed on the
// client, so newserv's proxy unconditionally blocks this command.

struct S_SetShutdownCommand_BB_01EF {
  ptext<char, 0x200> command;
} __packed__;

// F0 (S->C): Force update player lobby data (BB)
// Format is PlayerLobbyDataBB (in Player.hh). This command overwrites the lobby
// data for the player given by .client_id without reloading the game or lobby.

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

// Removed commands

// There is evidence that some commands and features were fully removed from
// PSO at some point.

// There is a command named RcvGamePause in all DC versions of PSO, but its
// handler function is missing. It's likely there was a way to actually pause
// the game during early development, but it was removed, likely because it'd
// be a fairly poor player experience.

// There are two commands named SndGameStatus and SndGameCondition in the DC
// versions, but their sender functions are missing in all versions. It's not
// clear what exactly they would have sent, or when they would have been
// triggered.

// Finally, there is a function named SndPsoGetText which was in DCv1 and DCv2,
// but not in DC NTE or the December 2000 prototype. This may have been a way
// for the server to prompt the user to input some text. As with the other
// unused functions, the code was removed, leaving only the function name.

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// GAME SUBCOMMANDS ////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// The game subcommands are used in commands 60, 62, 6C, 6D, C9, and CB. These
// are laid out similarly as above. These structs start with G_ to indicate that
// they are (usually) bidirectional, and are (usually) generated by clients and
// consumed by clients. Generally in newserv source, these commands are referred
// to as (for example) 6x02, etc., referencing the fact that they are almost
// always sent via a command starting with the hex digit 6.

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

// These common structures are used my many subcommands.
struct G_ClientIDHeader {
  uint8_t subcommand = 0;
  uint8_t size = 0;
  le_uint16_t client_id = 0; // <= 12
} __packed__;
struct G_EnemyIDHeader {
  uint8_t subcommand = 0;
  uint8_t size = 0;
  le_uint16_t enemy_id = 0; // In range [0x1000, 0x4000)
} __packed__;
struct G_ObjectIDHeader {
  uint8_t subcommand = 0;
  uint8_t size = 0;
  le_uint16_t object_id = 0; // >= 0x4000, != 0xFFFF
} __packed__;
struct G_UnusedHeader {
  uint8_t subcommand = 0;
  uint8_t size = 0;
  le_uint16_t unused = 0;
} __packed__;

template <typename HeaderT>
struct G_ExtendedHeader {
  HeaderT basic_header;
  le_uint32_t size = 0;
} __packed__;

// 6x00: Invalid subcommand
// 6x01: Invalid subcommand

// 6x02: Unknown
// This subcommand is completely ignored (at least, by PSO GC).

// 6x03: Unknown (same handler as 02)
// This subcommand is completely ignored (at least, by PSO GC).

// 6x04: Unknown

struct G_Unknown_6x04 {
  G_ClientIDHeader header;
  le_uint16_t unknown_a1;
  le_uint16_t unused;
} __packed__;

// 6x05: Switch state changed
// Some things that don't look like switches are implemented as switches using
// this subcommand. For example, when all enemies in a room are defeated, this
// subcommand is used to unlock the doors.

struct G_SwitchStateChanged_6x05 {
  // Note: header.object_id is 0xFFFF for room clear when all enemies defeated
  G_ObjectIDHeader header;
  parray<uint8_t, 2> unknown_a1;
  le_uint16_t unknown_a2;
  parray<uint8_t, 2> unknown_a3;
  uint8_t area;
  uint8_t flags; // Bit field, with 2 lowest bits having meaning
} __packed__;

// 6x06: Send guild card

template <typename CharT, size_t UnusedLength>
struct G_SendGuildCard_DC_PC_V3 {
  G_UnusedHeader header;
  le_uint32_t player_tag;
  le_uint32_t guild_card_number;
  ptext<CharT, 0x18> name;
  ptext<CharT, 0x48> description;
  parray<uint8_t, UnusedLength> unused2;
  uint8_t present;
  uint8_t present2;
  uint8_t section_id;
  uint8_t char_class;
} __packed__;

struct G_SendGuildCard_DC_6x06 : G_SendGuildCard_DC_PC_V3<char, 0x11> {
  parray<uint8_t, 3> unused3;
} __packed__;
struct G_SendGuildCard_PC_6x06 : G_SendGuildCard_DC_PC_V3<char16_t, 0x24> {
} __packed__;
struct G_SendGuildCard_V3_6x06 : G_SendGuildCard_DC_PC_V3<char, 0x24> {
} __packed__;

struct G_SendGuildCard_BB_6x06 {
  G_UnusedHeader header;
  le_uint32_t guild_card_number;
  ptext<char16_t, 0x18> name;
  ptext<char16_t, 0x10> team_name;
  ptext<char16_t, 0x58> description;
  uint8_t present;
  uint8_t present2;
  uint8_t section_id;
  uint8_t char_class;
} __packed__;

// 6x07: Symbol chat

struct G_SymbolChat_6x07 {
  G_UnusedHeader header;
  // TODO: How does this format differ across PSO versions? The GC version
  // treats some fields as unexpectedly large values (for example, face_spec
  // through unused2 are byteswapped as an le_uint32_t), so we should verify
  // that the order of these fields is the same on other versions.
  le_uint32_t client_id;
  // Bits: SSSCCCFF (S = sound, C = face color, F = face shape)
  uint8_t face_spec;
  // Bits: 000000DM (D = capture, M = mute sound)
  uint8_t flags;
  le_uint16_t unused;
  struct CornerObject {
    uint8_t type; // FF = no object in this slot
    // Bits: 000VHCCC (V = reverse vertical, H = reverse horizontal, C = color)
    uint8_t flags_color;
  } __packed__;
  parray<CornerObject, 4> corner_objects; // In reading order; top-left is first
  struct FacePart {
    uint8_t type; // FF = no part in this slot
    uint8_t x;
    uint8_t y;
    // Bits: 000000VH (V = reverse vertical, H = reverse horizontal)
    uint8_t flags;
  } __packed__;
  parray<FacePart, 12> face_parts;
} __packed__;

// 6x08: Invalid subcommand

// 6x09: Unknown

struct G_Unknown_6x09 {
  G_EnemyIDHeader header;
} __packed__;

// 6x0A: Enemy hit

struct G_EnemyHitByPlayer_6x0A {
  G_EnemyIDHeader header;
  // Note: enemy_id (in header) is in the range [0x1000, 0x4000)
  le_uint16_t enemy_id;
  le_uint16_t damage;
  be_uint32_t flags;
} __packed__;

// 6x0B: Box destroyed

struct G_BoxDestroyed_6x0B {
  G_ClientIDHeader header;
  le_uint32_t unknown_a2;
  le_uint32_t unknown_a3;
} __packed__;

// 6x0C: Add condition (poison/slow/etc.)

struct G_AddOrRemoveCondition_6x0C_6x0D {
  G_ClientIDHeader header;
  le_uint32_t unknown_a1; // Probably condition type
  le_uint32_t unknown_a2;
} __packed__;

// 6x0D: Remove condition (poison/slow/etc.)
// Same format as 6x0C

// 6x0E: Unknown

struct G_Unknown_6x0E {
  G_ClientIDHeader header;
} __packed__;

// 6x0F: Invalid subcommand

// 6x10: Unknown (not valid on Episode 3)

struct G_Unknown_6x10_6x11_6x12_6x14 {
  G_EnemyIDHeader header;
  le_uint16_t unknown_a2;
  le_uint16_t unknown_a3;
  le_uint32_t unknown_a4;
} __packed__;

// 6x11: Unknown (not valid on Episode 3)
// Same format as 6x10

// 6x12: Dragon boss actions (not valid on Episode 3)
// Same format as 6x10

// 6x13: De Rol Le boss actions (not valid on Episode 3)

struct G_DeRolLeBossActions_6x13 {
  G_EnemyIDHeader header;
  le_uint16_t unknown_a2;
  le_uint16_t unknown_a3;
} __packed__;

// 6x14: Unknown (supported; game only; not valid on Episode 3)
// Same format as 6x10

// 6x15: Vol Opt boss actions (not valid on Episode 3)

struct G_VolOptBossActions_6x15 {
  G_EnemyIDHeader header;
  le_uint16_t unknown_a2;
  le_uint16_t unknown_a3;
  le_uint16_t unknown_a4;
  le_uint16_t unknown_a5;
} __packed__;

// 6x16: Vol Opt boss actions (not valid on Episode 3)

struct G_VolOptBossActions_6x16 {
  G_UnusedHeader header;
  parray<uint8_t, 6> unknown_a2;
  le_uint16_t unknown_a3;
} __packed__;

// 6x17: Unknown (supported; game only; not valid on Episode 3)

struct G_Unknown_6x17 {
  G_ClientIDHeader header;
  le_float unknown_a2;
  le_float unknown_a3;
  le_float unknown_a4;
  le_uint32_t unknown_a5;
} __packed__;

// 6x18: Unknown (supported; game only; not valid on Episode 3)

struct G_Unknown_6x18 {
  G_ClientIDHeader header;
  parray<le_uint16_t, 4> unknown_a2;
} __packed__;

// 6x19: Dark Falz boss actions (not valid on Episode 3)

struct G_DarkFalzActions_6x19 {
  G_EnemyIDHeader header;
  le_uint16_t unknown_a2;
  le_uint16_t unknown_a3;
  le_uint32_t unknown_a4;
  le_uint32_t unused;
} __packed__;

// 6x1A: Invalid subcommand

// 6x1B: Unknown (not valid on Episode 3)

struct G_Unknown_6x1B {
  G_ClientIDHeader header;
} __packed__;

// 6x1C: Unknown (supported; game only; not valid on Episode 3)

struct G_Unknown_6x1C {
  G_ClientIDHeader header;
} __packed__;

// 6x1D: Invalid subcommand
// 6x1E: Invalid subcommand

// 6x1F: Unknown (supported; lobby & game)

struct G_Unknown_6x1F {
  G_ClientIDHeader header;
  le_uint32_t area;
} __packed__;

// 6x20: Set position
// Existing clients send this when a new client joins a lobby/game, so the new
// client knows where to place them.

struct G_Unknown_6x20 {
  G_ClientIDHeader header;
  le_uint32_t area;
  le_float x;
  le_float y;
  le_float z;
  le_uint32_t unknown_a1;
} __packed__;

// 6x21: Inter-level warp

struct G_InterLevelWarp_6x21 {
  G_ClientIDHeader header;
  le_uint32_t area;
} __packed__;

// 6x22: Set player invisible
// 6x23: Set player visible

struct G_SetPlayerVisibility_6x22_6x23 {
  G_ClientIDHeader header;
} __packed__;

// 6x24: Unknown (supported; game only)

struct G_Unknown_6x24 {
  G_ClientIDHeader header;
  le_uint32_t unknown_a1;
  le_float x;
  le_float y;
  le_float z;
} __packed__;

// 6x25: Equip item

struct G_EquipOrUnequipItem_6x25_6x26 {
  G_ClientIDHeader header;
  le_uint32_t item_id;
  le_uint32_t equip_slot; // Unused for 6x26 (unequip item)
} __packed__;

// 6x26: Unequip item
// Same format as 6x25

// 6x27: Use item

struct G_UseItem_6x27 {
  G_ClientIDHeader header;
  le_uint32_t item_id;
} __packed__;

// 6x28: Feed MAG

struct G_FeedMAG_6x28 {
  G_ClientIDHeader header;
  le_uint32_t mag_item_id;
  le_uint32_t fed_item_id;
} __packed__;

// 6x29: Delete inventory item (via bank deposit / sale / feeding MAG)
// This subcommand is also used for reducing the size of stacks - if amount is
// less than the stack count, the item is not deleted and its ID remains valid.

struct G_DeleteInventoryItem_6x29 {
  G_ClientIDHeader header;
  le_uint32_t item_id;
  le_uint32_t amount;
} __packed__;

// 6x2A: Drop item

struct G_DropItem_6x2A {
  G_ClientIDHeader header;
  le_uint16_t unknown_a1; // Should be 1... maybe amount?
  le_uint16_t area;
  le_uint32_t item_id;
  le_float x;
  le_float y;
  le_float z;
} __packed__;

// 6x2B: Create item in inventory (e.g. via tekker or bank withdraw)

struct G_CreateInventoryItem_DC_6x2B {
  G_ClientIDHeader header;
  ItemData item;
} __packed__;

struct G_CreateInventoryItem_PC_V3_BB_6x2B {
  G_CreateInventoryItem_DC_6x2B basic_cmd;
  uint8_t unused1;
  uint8_t unknown_a2;
  le_uint16_t unused2;
} __packed__;

// 6x2C: Talk to NPC

struct G_TalkToNPC_6x2C {
  G_ClientIDHeader header;
  le_uint16_t unknown_a1;
  le_uint16_t unknown_a2;
  le_float unknown_a3;
  le_float unknown_a4;
  le_float unknown_a5;
} __packed__;

// 6x2D: Done talking to NPC

struct G_EndTalkToNPC_6x2D {
  G_ClientIDHeader header;
} __packed__;

// 6x2E: Set and/or clear player flags

struct G_SetOrClearPlayerFlags_6x2E {
  G_ClientIDHeader header;
  le_uint32_t and_mask;
  le_uint32_t or_mask;
} __packed__;

// 6x2F: Hit by enemy

struct G_HitByEnemy_6x2F {
  G_ClientIDHeader header;
  le_uint32_t hit_type; // 0 = set HP, 1 = add/subtract HP, 2 = add/sub fixed HP
  le_uint16_t damage;
  le_uint16_t client_id;
} __packed__;

// 6x30: Level up

struct G_LevelUp_6x30 {
  G_ClientIDHeader header;
  le_uint16_t atp;
  le_uint16_t mst;
  le_uint16_t evp;
  le_uint16_t hp;
  le_uint16_t dfp;
  le_uint16_t ata;
  le_uint16_t level;
  le_uint16_t unknown_a1; // Must be 0 or 1
} __packed__;

// 6x31: Medical center

struct G_UseMedicalCenter_6x31 {
  G_ClientIDHeader header;
} __packed__;

// 6x32: Unknown (occurs when using Medical Center)

struct G_Unknown_6x32 {
  G_UnusedHeader header;
} __packed__;

// 6x33: Revive player (e.g. with moon atomizer)

struct G_RevivePlayer_6x33 {
  G_ClientIDHeader header;
  le_uint16_t client_id2;
  le_uint16_t unused;
} __packed__;

// 6x34: Unknown
// This subcommand is completely ignored (at least, by PSO GC).

// 6x35: Invalid subcommand

// 6x36: Unknown (supported; game only)
// This subcommand is completely ignored (at least, by PSO GC).

// 6x37: Photon blast

struct G_PhotonBlast_6x37 {
  G_ClientIDHeader header;
  le_uint16_t unknown_a1;
  le_uint16_t unused;
} __packed__;

// 6x38: Unknown

struct G_Unknown_6x38 {
  G_ClientIDHeader header;
  le_uint16_t unknown_a1;
  le_uint16_t unused;
} __packed__;

// 6x39: Photon blast ready

struct G_PhotonBlastReady_6x38 {
  G_ClientIDHeader header;
} __packed__;

// 6x3A: Unknown (supported; game only)

struct G_Unknown_6x3A {
  G_ClientIDHeader header;
} __packed__;

// 6x3B: Unknown (supported; lobby & game)

struct G_Unknown_6x3B {
  G_ClientIDHeader header;
} __packed__;

// 6x3C: Invalid subcommand
// 6x3D: Invalid subcommand

// 6x3E: Stop moving

struct G_StopAtPosition_6x3E {
  G_ClientIDHeader header;
  le_uint16_t unknown_a1;
  le_uint16_t unknown_a2;
  le_uint16_t area;
  le_uint16_t unknown_a3;
  le_float x;
  le_float y;
  le_float z;
} __packed__;

// 6x3F: Set position

struct G_SetPosition_6x3F {
  G_ClientIDHeader header;
  le_uint16_t unknown_a1;
  le_uint16_t unknown_a2;
  le_uint16_t area;
  le_uint16_t unknown_a3;
  le_float x;
  le_float y;
  le_float z;
} __packed__;

// 6x40: Walk

struct G_WalkToPosition_6x40 {
  G_ClientIDHeader header;
  le_float x;
  le_float z;
  le_uint32_t unknown_a1;
} __packed__;

// 6x41: Unknown
// This subcommand is completely ignored (at least, by PSO GC).

// 6x42: Run

struct G_RunToPosition_6x42 {
  G_ClientIDHeader header;
  le_float x;
  le_float z;
} __packed__;

// 6x43: First attack

struct G_Attack_6x43_6x44_6x45 {
  G_ClientIDHeader header;
  le_uint16_t unknown_a1;
  le_uint16_t unknown_a2;
} __packed__;

// 6x44: Second attack
// Same format as 6x43

// 6x45: Third attack
// Same format as 6x43

// 6x46: Attack finished (sent after each of 43, 44, and 45)

struct G_AttackFinished_6x46 {
  G_ClientIDHeader header;
  le_uint32_t count;
  struct Entry {
    le_uint16_t unknown_a1;
    le_uint16_t unknown_a2;
  } __packed__;
  Entry entries[11];
} __packed__;

// 6x47: Cast technique

struct G_CastTechnique_6x47 {
  G_ClientIDHeader header;
  uint8_t technique_number;
  uint8_t unused;
  // Note: The level here isn't the actual tech level that was cast, if the
  // level is > 15. In that case, a 6x8D is sent first, which contains the
  // additional level which is added to this level at cast time. They probably
  // did this for legacy reasons when dealing with v1/v2 compatibility, and
  // never cleaned it up.
  uint8_t level;
  uint8_t target_count;
  struct TargetEntry {
    le_uint16_t client_id;
    le_uint16_t unknown_a2;
  } __packed__;
  TargetEntry targets[10];
} __packed__;

// 6x48: Cast technique complete

struct G_CastTechniqueComplete_6x48 {
  G_ClientIDHeader header;
  le_uint16_t technique_number;
  // This level matches the level sent in the 6x47 command, even if that level
  // was overridden by a preceding 6x8D command.
  le_uint16_t level;
} __packed__;

// 6x49: Subtract PB energy

struct G_SubtractPBEnergy_6x49 {
  G_ClientIDHeader header;
  uint8_t unknown_a1;
  uint8_t unknown_a2;
  le_uint16_t entry_count;
  le_uint16_t unknown_a3;
  le_uint16_t unknown_a4;
  struct Entry {
    le_uint16_t unknown_a1;
    le_uint16_t unknown_a2;
  } __packed__;
  Entry entries[14];
} __packed__;

// 6x4A: Fully shield attack

struct G_ShieldAttack_6x4A {
  G_ClientIDHeader header;
} __packed__;

// 6x4B: Hit by enemy

struct G_HitByEnemy_6x4B_6x4C {
  G_ClientIDHeader header;
  le_uint16_t angle;
  le_uint16_t damage;
  le_float x_velocity;
  le_float z_velocity;
} __packed__;

// 6x4C: Hit by enemy
// Same format as 6x4B

// 6x4D: Unknown (supported; lobby & game)

struct G_Unknown_6x4D {
  G_ClientIDHeader header;
  le_uint32_t unknown_a1;
} __packed__;

// 6x4E: Unknown (supported; lobby & game)

struct G_Unknown_6x4E {
  G_ClientIDHeader header;
} __packed__;

// 6x4F: Unknown (supported; lobby & game)

struct G_Unknown_6x4F {
  G_ClientIDHeader header;
} __packed__;

// 6x50: Unknown (supported; lobby & game)

struct G_Unknown_6x50 {
  G_ClientIDHeader header;
  le_uint32_t unknown_a1;
} __packed__;

// 6x51: Invalid subcommand

// 6x52: Toggle shop/bank interaction

struct G_Unknown_6x52 {
  G_ClientIDHeader header;
  le_uint16_t unknown_a1;
  le_uint16_t unknown_a2;
  le_uint32_t unknown_a3;
} __packed__;

// 6x53: Unknown (supported; game only)

struct G_Unknown_6x53 {
  G_ClientIDHeader header;
} __packed__;

// 6x54: Unknown
// This subcommand is completely ignored (at least, by PSO GC).

// 6x55: Intra-map warp

struct G_IntraMapWarp_6x55 {
  G_ClientIDHeader header;
  le_uint32_t unknown_a1;
  le_float x1;
  le_float y1;
  le_float z1;
  le_float x2;
  le_float y2;
  le_float z2;
} __packed__;

// 6x56: Unknown (supported; lobby & game)

struct G_Unknown_6x56 {
  G_ClientIDHeader header;
  le_uint32_t unknown_a1;
  le_float x;
  le_float y;
  le_float z;
} __packed__;

// 6x57: Unknown (supported; lobby & game)

struct G_Unknown_6x57 {
  G_ClientIDHeader header;
} __packed__;

// 6x58: Unknown (supported; game only)

struct G_Unknown_6x58 {
  G_ClientIDHeader header;
  le_uint16_t unknown_a1;
  le_uint16_t unused;
} __packed__;

// 6x59: Pick up item

struct G_PickUpItem_6x59 {
  G_ClientIDHeader header;
  le_uint16_t client_id2;
  le_uint16_t area;
  le_uint32_t item_id;
} __packed__;

// 6x5A: Request to pick up item

struct G_PickUpItemRequest_6x5A {
  G_ClientIDHeader header;
  le_uint32_t item_id;
  le_uint16_t area;
  le_uint16_t unused;
} __packed__;

// 6x5B: Invalid subcommand

// 6x5C: Unknown

struct G_Unknown_6x5C {
  G_UnusedHeader header;
  le_uint32_t unknown_a1;
  le_uint32_t unknown_a2;
} __packed__;

// 6x5D: Drop meseta or stacked item

struct G_DropStackedItem_DC_6x5D {
  G_ClientIDHeader header;
  le_uint16_t area;
  le_uint16_t unused2;
  le_float x;
  le_float z;
  ItemData data;
} __packed__;

struct G_DropStackedItem_PC_V3_BB_6x5D {
  G_DropStackedItem_DC_6x5D basic_cmd;
  le_uint32_t unused3;
} __packed__;

// 6x5E: Buy item at shop

struct G_BuyShopItem_6x5E {
  G_ClientIDHeader header;
  ItemData item;
} __packed__;

// 6x5F: Drop item from box/enemy

struct G_DropItem_DC_6x5F {
  G_UnusedHeader header;
  uint8_t area;
  uint8_t from_enemy;
  le_uint16_t request_id; // < 0x0B50 if from_enemy != 0; otherwise < 0x0BA0
  le_float x;
  le_float z;
  le_uint16_t unknown_a1;
  le_uint16_t unknown_a2;
  ItemData data;
} __packed__;

struct G_DropItem_PC_V3_BB_6x5F {
  G_DropItem_DC_6x5F basic_cmd;
  le_uint32_t unused3;
} __packed__;

// 6x60: Request for item drop (handled by the server on BB)

struct G_EnemyDropItemRequest_DC_6x60 {
  G_UnusedHeader header;
  uint8_t area;
  uint8_t rt_index;
  le_uint16_t enemy_id;
  le_float x;
  le_float z;
  le_uint16_t unknown_a1;
  le_uint16_t unknown_a2;
} __packed__;

struct G_EnemyDropItemRequest_PC_V3_BB_6x60 {
  G_EnemyDropItemRequest_DC_6x60 basic_cmd;
  le_uint32_t unknown_a2;
} __packed__;

// 6x61: Feed MAG

struct G_FeedMAG_6x61 {
  G_UnusedHeader header;
  le_uint32_t mag_item_id;
  le_uint32_t fed_item_id;
} __packed__;

// 6x62: Unknown
// This subcommand is completely ignored (at least, by PSO GC).

// 6x63: Destroy item on the ground (used when too many items have been dropped)

struct G_DestroyGroundItem_6x63 {
  G_UnusedHeader header;
  le_uint32_t item_id;
  le_uint32_t area;
} __packed__;

// 6x64: Unknown (not valid on Episode 3)
// This subcommand is completely ignored (at least, by PSO GC).

// 6x65: Unknown (not valid on Episode 3)
// This subcommand is completely ignored (at least, by PSO GC).

// 6x66: Use star atomizer

struct G_UseStarAtomizer_6x66 {
  G_UnusedHeader header;
  parray<le_uint16_t, 4> target_client_ids;
} __packed__;

// 6x67: Create enemy set

struct G_CreateEnemySet_6x67 {
  G_UnusedHeader header;
  // unused1 could be area; the client checks this againset a global but the
  // logic is the same in both branches
  le_uint32_t unused1;
  le_uint32_t unknown_a1;
  le_uint32_t unused2;
} __packed__;

// 6x68: Telepipe/Ryuker

struct G_CreateTelepipe_6x68 {
  G_UnusedHeader header;
  le_uint16_t client_id2;
  le_uint16_t unknown_a1;
  le_uint16_t unused1;
  parray<uint8_t, 2> unused2;
  le_float x;
  le_float y;
  le_float z;
  le_uint32_t unused3;
} __packed__;

// 6x69: Unknown (supported; game only)

struct G_Unknown_6x69 {
  G_UnusedHeader header;
  le_uint16_t client_id2;
  le_uint16_t unknown_a1;
  le_uint16_t what; // 0-3; logic is very different for each value
  le_uint16_t unknown_a2;
} __packed__;

// 6x6A: Unknown (supported; game only; not valid on Episode 3)

struct G_Unknown_6x6A {
  G_ClientIDHeader header;
  le_uint16_t unknown_a1;
  le_uint16_t unused;
} __packed__;

// 6x6B: Sync enemy state (used while loading into game; same header format as 6E)

struct G_SyncGameStateHeader_6x6B_6x6C_6x6D_6x6E {
  G_ExtendedHeader<G_UnusedHeader> header;
  le_uint32_t decompressed_size;
  le_uint32_t compressed_size; // Must be <= subcommand_size - 0x10
  uint8_t data[0]; // BC0-compressed data follows here (see bc0_decompress)
} __packed__;

// Decompressed format is a list of these
struct G_SyncEnemyState_6x6B_Entry_Decompressed {
  // TODO: Verify this format on DC and PC. It appears correct for GC and BB.
  le_uint32_t unknown_a1; // Possibly some kind of flags
  // enemy_index is not the same as enemy_id, unfortunately - the enemy_id sent
  // in the 6x76 command when an enemy is killed does not match enemy_index
  le_uint16_t enemy_index; // FFFF = enemy is dead
  le_uint16_t damage_taken;
  uint8_t unknown_a4;
  uint8_t unknown_a5;
  uint8_t unknown_a6;
  uint8_t unknown_a7;
} __packed__;

// 6x6C: Sync object state (used while loading into game; same header format as 6E)
// Compressed format is the same as 6x6B.

// Decompressed format is a list of these
struct G_SyncObjectState_6x6C_Entry_Decompressed {
  // TODO: Verify this format on DC and PC. It appears correct for GC and BB.
  le_uint16_t flags;
  le_uint16_t object_index;
} __packed__;

// 6x6D: Sync item state (used while loading into game; same header format as 6E)
// Compressed format is the same as 6x6B.

struct G_SyncItemState_6x6D_Decompressed {
  // TODO: Verify this format on DC and PC. It appears correct for GC and BB.
  // Note: 16 vs. 15 is not a bug here - there really is an extra field in the
  // total drop count vs. the floor item count. Despite this, Pioneer 2 or Lab
  // (area 0) isn't included in total_items_dropped_per_area (so Forest 1 is [0]
  // in that array) but it is included in floor_item_count_per_area (so Forest 1
  // is [1] there).
  parray<le_uint16_t, 16> total_items_dropped_per_area;
  // Only [0]-[3] in this array are ever actually used in normal gameplay, but
  // the client fills in all 12 of these with reasonable values.
  parray<le_uint32_t, 12> next_item_id_per_player;
  parray<le_uint32_t, 15> floor_item_count_per_area;
  struct FloorItem {
    le_uint16_t area;
    le_uint16_t unknown_a1;
    le_float x;
    le_float z;
    le_uint16_t unknown_a2;
    // The drop number is scoped to the area and increments by 1 each time an
    // item is dropped. The last item dropped in each area has drop_number equal
    // to total_items_dropped_per_area[area - 1] - 1.
    le_uint16_t drop_number;
    ItemData data;
  } __packed__;
  // Variable-length field follows:
  // FloorItem items[sum(floor_item_count_per_area)];
} __packed__;

// 6x6E: Sync flag state (used while loading into game)
// Compressed format is the same as 6x6B.

struct G_SyncFlagState_6x6E_Decompressed {
  // TODO: Verify this format on DC and PC. It appears correct for GC and BB.
  // The three unknowns here are the sizes (in bytes) of three fields
  // immediately following this structure. It is currently unknown what these
  // fields represent. The three unknown fields always sum to the size field.
  le_uint16_t size;
  le_uint16_t unknown_a1;
  le_uint16_t unknown_a2;
  le_uint16_t unknown_a3;
  // Three variable-length fields follow here. They are in the same order as the
  // unknown fields above.
} __packed__;

// 6x6F: Unknown (used while loading into game)

struct G_Unknown_6x6F {
  G_UnusedHeader header;
  parray<uint8_t, 0x200> unknown_a1;
} __packed__;

// 6x70: Sync player disp data and inventory (used while loading into game)
// Annoyingly, they didn't use the same format as the 65/67/68 commands here,
// and instead rearranged a bunch of things.

struct G_SyncPlayerDispAndInventory_6x70 {
  G_ExtendedHeader<G_UnusedHeader> header;
  // Offsets in this struct are relative to the overall command header
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
  } __packed__ unknown_a7;
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
  } __packed__ disp_part2;
  /* 0124 */ struct {
    PlayerStats stats;
    parray<uint8_t, 0x0A> unknown_a1;
    le_uint32_t level;
    le_uint32_t experience;
    le_uint32_t meseta;
  } __packed__ disp_part1;
  /* 0148 */ struct {
    le_uint32_t num_items;
    // Entries >= num_items in this array contain uninitialized data (usually
    // the contents of a previous sync command)
    parray<PlayerInventoryItem, 0x1E> items;
  } __packed__ inventory;
  /* 0494 */ le_uint32_t unknown_a15;
} __packed__;

// 6x71: Unknown (used while loading into game)

struct G_Unknown_6x71 {
  G_UnusedHeader header;
} __packed__;

// 6x72: Unknown (used while loading into game)

struct G_Unknown_6x72 {
  G_UnusedHeader header;
} __packed__;

// 6x73: Unknown

struct G_Unknown_6x73 {
  G_UnusedHeader header;
} __packed__;

// 6x74: Word select

struct G_WordSelect_6x74 {
  G_ClientIDHeader header;
  le_uint16_t unknown_a1;
  le_uint16_t unknown_a2;
  parray<le_uint16_t, 8> entries;
  le_uint32_t unknown_a3;
  le_uint32_t unknown_a4;
} __packed__;

// 6x75: Phase setup (supported; game only)

// TODO: Not sure about PC format here. Is this first struct DC-only?
struct G_PhaseSetup_DC_PC_6x75 {
  G_UnusedHeader header;
  le_uint16_t phase; // Must be < 0x400
  le_uint16_t unknown_a1; // Must be 0 or 1
} __packed__;

struct G_PhaseSetup_V3_BB_6x75 {
  G_PhaseSetup_DC_PC_6x75 basic_cmd;
  le_uint16_t difficulty;
  le_uint16_t unused;
} __packed__;

// 6x76: Enemy killed

struct G_EnemyKilled_6x76 {
  G_EnemyIDHeader header;
  le_uint16_t unknown_a1;
  le_uint16_t unknown_a2; // Flags of some sort
} __packed__;

// 6x77: Sync quest data

struct G_SyncQuestData_6x77 {
  G_UnusedHeader header;
  le_uint16_t register_number; // Must be < 0x100
  le_uint16_t unused;
  le_uint32_t value;
} __packed__;

// 6x78: Unknown

struct G_Unknown_6x78 {
  G_UnusedHeader header;
  le_uint16_t client_id; // Must be < 12
  le_uint16_t unused1;
  le_uint32_t unused2;
} __packed__;

// 6x79: Lobby 14/15 gogo ball (soccer game)

struct G_GogoBall_6x79 {
  G_UnusedHeader header;
  le_uint32_t unknown_a1;
  le_uint32_t unknown_a2;
  le_float unknown_a3;
  le_float unknown_a4;
  uint8_t unknown_a5;
  parray<uint8_t, 3> unused;
} __packed__;

// 6x7A: Unknown

struct G_Unknown_6x7A {
  G_ClientIDHeader header;
} __packed__;

// 6x7B: Unknown

struct G_Unknown_6x7B {
  G_ClientIDHeader header;
} __packed__;

// 6x7C: Unknown (supported; game only; not valid on Episode 3)

struct G_Unknown_6x7C {
  G_UnusedHeader header;
  le_uint16_t client_id;
  parray<uint8_t, 2> unknown_a1;
  le_uint16_t unknown_a2;
  parray<uint8_t, 2> unknown_a3;
  parray<le_uint32_t, 0x17> unknown_a4;
  parray<uint8_t, 4> unknown_a5;
  le_uint16_t unknown_a6;
  parray<uint8_t, 2> unknown_a7;
  le_uint32_t unknown_a8;
  le_uint32_t unknown_a9;
  le_uint32_t unknown_a10;
  le_uint32_t unknown_a11;
  le_uint32_t unknown_a12;
  parray<uint8_t, 0x34> unknown_a13;
  struct Entry {
    le_uint32_t unknown_a1;
    le_uint32_t unknown_a2;
  } __packed__;
  Entry entries[3];
} __packed__;

// 6x7D: Unknown (supported; game only; not valid on Episode 3)

struct G_Unknown_6x7D {
  G_UnusedHeader header;
  uint8_t unknown_a1; // Must be < 7; used in jump table
  parray<uint8_t, 3> unused;
  parray<le_uint32_t, 4> unknown_a2;
} __packed__;

// 6x7E: Unknown (not valid on Episode 3)
// This subcommand is completely ignored (at least, by PSO GC).

// 6x7F: Unknown (not valid on Episode 3)

struct G_Unknown_6x7F {
  G_UnusedHeader header;
  parray<uint8_t, 0x20> unknown_a1;
} __packed__;

// 6x80: Trigger trap (not valid on Episode 3)

struct G_TriggerTrap_6x80 {
  G_ClientIDHeader header;
  le_uint16_t unknown_a1;
  le_uint16_t unknown_a2;
} __packed__;

// 6x81: Unknown

struct G_Unknown_6x81 {
  G_ClientIDHeader header;
} __packed__;

// 6x82: Unknown

struct G_Unknown_6x82 {
  G_ClientIDHeader header;
} __packed__;

// 6x83: Place trap

struct G_PlaceTrap_6x83 {
  G_ClientIDHeader header;
  le_uint16_t unknown_a1;
  le_uint16_t unknown_a2;
} __packed__;

// 6x84: Unknown (supported; game only; not valid on Episode 3)

struct G_Unknown_6x84 {
  G_UnusedHeader header;
  parray<uint8_t, 6> unknown_a1;
  le_uint16_t unknown_a2;
  le_uint16_t unknown_a3;
  le_uint16_t unused;
} __packed__;

// 6x85: Unknown (supported; game only; not valid on Episode 3)

struct G_Unknown_6x85 {
  G_UnusedHeader header;
  le_uint16_t unknown_a1; // Command is ignored unless this is 0
  parray<le_uint16_t, 7> unknown_a2; // Only the first 3 appear to be used
} __packed__;

// 6x86: Hit destructible object (not valid on Episode 3)

struct G_HitDestructibleObject_6x86 {
  G_ObjectIDHeader header;
  le_uint32_t unknown_a1;
  le_uint32_t unknown_a2;
  le_uint16_t unknown_a3;
  le_uint16_t unknown_a4;
} __packed__;

// 6x87: Unknown

struct G_Unknown_6x87 {
  G_ClientIDHeader header;
  le_float unknown_a1;
} __packed__;

// 6x88: Unknown (supported; game only)

struct G_Unknown_6x88 {
  G_ClientIDHeader header;
} __packed__;

// 6x89: Unknown (supported; game only)

struct G_Unknown_6x89 {
  G_ClientIDHeader header;
  le_uint16_t unknown_a1;
  le_uint16_t unused;
} __packed__;

// 6x8A: Unknown (not valid on Episode 3)

struct G_Unknown_6x8A {
  G_ClientIDHeader header;
  le_uint32_t unknown_a1; // Must be < 0x11
} __packed__;

// 6x8B: Unknown (not valid on Episode 3)
// This subcommand is completely ignored (at least, by PSO GC).

// 6x8C: Unknown (not valid on Episode 3)
// This subcommand is completely ignored (at least, by PSO GC).

// 6x8D: Set technique level override

struct G_SetTechniqueLevelOverride_6x8D {
  G_ClientIDHeader header;
  uint8_t level_upgrade;
  uint8_t unused1;
  le_uint16_t unused2;
} __packed__;

// 6x8E: Unknown (not valid on Episode 3)
// This subcommand is completely ignored (at least, by PSO GC).

// 6x8F: Unknown (not valid on Episode 3)

struct G_Unknown_6x8F {
  G_ClientIDHeader header;
  le_uint16_t client_id2;
  le_uint16_t unknown_a1;
} __packed__;

// 6x90: Unknown (not valid on Episode 3)

struct G_Unknown_6x90 {
  G_ClientIDHeader header;
  le_uint32_t unknown_a1;
} __packed__;

// 6x91: Unknown (supported; game only)

struct G_Unknown_6x91 {
  G_ObjectIDHeader header;
  le_uint32_t unknown_a1;
  le_uint32_t unknown_a2;
  le_uint16_t unknown_a3;
  le_uint16_t unknown_a4;
  le_uint16_t unknown_a5;
  parray<uint8_t, 2> unknown_a6;
} __packed__;

// 6x92: Unknown (not valid on Episode 3)

struct G_Unknown_6x92 {
  G_UnusedHeader header;
  le_uint32_t unknown_a1;
  le_float unknown_a2;
} __packed__;

// 6x93: Timed switch activated (not valid on Episode 3)

struct G_TimedSwitchActivated_6x93 {
  G_UnusedHeader header;
  le_uint16_t area;
  le_uint16_t switch_id;
  uint8_t unknown_a1; // Logic is different if this is 1 vs. any other value
  parray<uint8_t, 3> unused;
} __packed__;

// 6x94: Warp (not valid on Episode 3)

struct G_InterLevelWarp_6x94 {
  G_UnusedHeader header;
  le_uint16_t area;
  parray<uint8_t, 2> unused;
} __packed__;

// 6x95: Unknown (not valid on Episode 3)

struct G_Unknown_6x95 {
  G_UnusedHeader header;
  le_uint32_t client_id;
  le_uint32_t unknown_a1;
  le_uint32_t unknown_a2;
  le_uint32_t unknown_a3;
} __packed__;

// 6x96: Unknown (not valid on Episode 3)
// This subcommand is completely ignored (at least, by PSO GC).

// 6x97: Unknown (not valid on Episode 3)

struct G_Unknown_6x97 {
  G_UnusedHeader header;
  le_uint32_t unused1;
  le_uint32_t unknown_a1; // Must be 0 or 1
  le_uint32_t unused2;
  le_uint32_t unused3;
} __packed__;

// 6x98: Unknown
// This subcommand is completely ignored (at least, by PSO GC).

// 6x99: Unknown
// This subcommand is completely ignored (at least, by PSO GC).

// 6x9A: Update player stat (not valid on Episode 3)

struct G_UpdatePlayerStat_6x9A {
  G_ClientIDHeader header;
  le_uint16_t client_id2;
  // Values for what:
  // 0 = subtract HP
  // 1 = subtract TP
  // 2 = subtract Meseta
  // 3 = add HP
  // 4 = add TP
  uint8_t what;
  uint8_t amount;
} __packed__;

// 6x9B: Unknown

struct G_Unknown_6x9B {
  G_UnusedHeader header;
  uint8_t unknown_a1;
  parray<uint8_t, 3> unused;
} __packed__;

// 6x9C: Unknown (supported; game only; not valid on Episode 3)

struct G_Unknown_6x9C {
  G_EnemyIDHeader header;
  le_uint32_t unknown_a1;
} __packed__;

// 6x9D: Unknown (not valid on Episode 3)

struct G_Unknown_6x9D {
  G_UnusedHeader header;
  le_uint32_t client_id2;
} __packed__;

// 6x9E: Unknown (not valid on Episode 3)
// This subcommand is completely ignored (at least, by PSO GC).

// 6x9F: Gal Gryphon actions (not valid on PC or Episode 3)

struct G_GalGryphonActions_6x9F {
  G_EnemyIDHeader header;
  le_uint32_t unknown_a1;
  le_float unknown_a2;
  le_float unknown_a3;
} __packed__;

// 6xA0: Gal Gryphon actions (not valid on PC or Episode 3)

struct G_GalGryphonActions_6xA0 {
  G_EnemyIDHeader header;
  le_float x;
  le_float y;
  le_float z;
  le_uint32_t unknown_a1;
  le_uint16_t unknown_a2;
  le_uint16_t unknown_a3;
  parray<le_uint32_t, 4> unknown_a4;
} __packed__;

// 6xA1: Unknown (not valid on PC)

struct G_Unknown_6xA1 {
  G_ClientIDHeader header;
} __packed__;

// 6xA2: Request for item drop from box (not valid on PC; handled by server on BB)

struct G_BoxItemDropRequest_6xA2 {
  G_UnusedHeader header;
  uint8_t area;
  uint8_t unknown_a1;
  le_uint16_t request_id;
  le_float x;
  le_float z;
  le_uint16_t unknown_a2;
  le_uint16_t unknown_a3;
  parray<uint8_t, 4> unknown_a4;
  le_uint32_t unknown_a5;
  le_uint32_t unknown_a6;
  le_uint32_t unknown_a7;
  le_uint32_t unknown_a8;
} __packed__;

// 6xA3: Episode 2 boss actions (not valid on PC or Episode 3)

struct G_Episode2BossActions_6xA3 {
  G_EnemyIDHeader header;
  uint8_t unknown_a1;
  uint8_t unknown_a2;
  parray<uint8_t, 2> unknown_a3;
} __packed__;

// 6xA4: Olga Flow phase 1 actions (not valid on PC or Episode 3)

struct G_OlgaFlowPhase1Actions_6xA4 {
  G_EnemyIDHeader header;
  uint8_t what;
  parray<uint8_t, 3> unknown_a3;
} __packed__;

// 6xA5: Olga Flow phase 2 actions (not valid on PC or Episode 3)

struct G_OlgaFlowPhase2Actions_6xA5 {
  G_EnemyIDHeader header;
  uint8_t what;
  parray<uint8_t, 3> unknown_a3;
} __packed__;

// 6xA6: Modify trade proposal (not valid on PC)

struct G_ModifyTradeProposal_6xA6 {
  G_ClientIDHeader header;
  uint8_t unknown_a1; // Must be < 8
  uint8_t unknown_a2;
  parray<uint8_t, 2> unknown_a3;
  le_uint32_t unknown_a4;
  le_uint32_t unknown_a5;
} __packed__;

// 6xA7: Unknown (not valid on PC)
// This subcommand is completely ignored (at least, by PSO GC).

// 6xA8: Gol Dragon actions (not valid on PC or Episode 3)

struct G_GolDragonActions_6xA8 {
  G_EnemyIDHeader header;
  le_uint16_t unknown_a1;
  le_uint16_t unknown_a2;
  le_uint32_t unknown_a3;
} __packed__;

// 6xA9: Barba Ray actions (not valid on PC or Episode 3)

struct G_BarbaRayActions_6xA9 {
  G_EnemyIDHeader header;
  le_uint16_t unknown_a1;
  le_uint16_t unknown_a2;
} __packed__;

// 6xAA: Episode 2 boss actions (not valid on PC or Episode 3)

struct G_Episode2BossActions_6xAA {
  G_EnemyIDHeader header;
  le_uint16_t unknown_a1;
  le_uint16_t unknown_a2;
  le_uint32_t unknown_a3;
} __packed__;

// 6xAB: Create lobby chair (not valid on PC)

struct G_CreateLobbyChair_6xAB {
  G_ClientIDHeader header;
  le_uint16_t unknown_a1;
  le_uint16_t unknown_a2;
} __packed__;

// 6xAC: Unknown (not valid on PC)

struct G_Unknown_6xAC {
  G_ClientIDHeader header;
  le_uint32_t num_items;
  parray<le_uint32_t, 0x1E> item_ids;
} __packed__;

// 6xAD: Unknown (not valid on PC, Episode 3, or GC Trial Edition)

struct G_Unknown_6xAD {
  G_UnusedHeader header;
  // The first byte in this array seems to have a special meaning
  parray<uint8_t, 0x40> unknown_a1;
} __packed__;

// 6xAE: Set lobby chair state (sent by existing clients at join time)
// This subcommand is not valid on DC, PC, or GC Trial Edition.

struct G_SetLobbyChairState_6xAE {
  G_ClientIDHeader header;
  le_uint16_t unknown_a1;
  le_uint16_t unknown_a2;
  le_uint32_t unknown_a3;
  le_uint32_t unknown_a4;
} __packed__;

// 6xAF: Turn lobby chair (not valid on PC or GC Trial Edition)

struct G_TurnLobbyChair_6xAF {
  G_ClientIDHeader header;
  le_uint32_t angle; // In range [0x0000, 0xFFFF]
} __packed__;

// 6xB0: Move lobby chair (not valid on PC or GC Trial Edition)

struct G_MoveLobbyChair_6xB0 {
  G_ClientIDHeader header;
  le_uint32_t unknown_a1;
} __packed__;

// 6xB1: Unknown (not valid on PC or GC Trial Edition)
// This subcommand is completely ignored (at least, by PSO GC).

// 6xB2: Unknown (not valid on PC or GC Trial Edition)
// TODO: It appears this command is sent when the snapshot file is written on
// PSO GC. Verify this.

struct G_Unknown_6xB2 {
  G_UnusedHeader header;
  uint8_t area;
  uint8_t unused;
  le_uint16_t client_id;
  le_uint32_t unknown_a3; // PSO GC puts 0x00051720 (333600) here
} __packed__;

// 6xB3: Unknown (XBOX)

// 6xB3: CARD battle server data request (Episode 3)

// CARD battle subcommands have multiple subsubcommands, which we name 6xBYxZZ,
// where Y = 3, 4, 5, or 6, and ZZ is any byte. The formats of these
// subsubcommands are described at the end of this file.

// Unlike all other 6x subcommands, the 6xB3 subcommand is sent to the server in
// a CA command instead of a 6x, C9, or CB command. (For this reason, we refer
// to 6xB3xZZ commands as CAxZZ commands as well.) The server is expected to
// reply to CA commands instead of forwarding them. The logic for doing so is
// primarily implemented in Episode3/Server.cc and the surrounding classes.

// The common format for CARD battle subcommand headers is:
struct G_CardBattleCommandHeader {
  uint8_t subcommand = 0x00;
  uint8_t size = 0x00;
  le_uint16_t unused1 = 0x0000;
  uint8_t subsubcommand = 0x00; // See 6xBx subcommand table (after this table)
  uint8_t sender_client_id = 0x00;
  // If mask_key is nonzero, the remainder of the data (after unused2 in this
  // struct) is encrypted using a simple algorithm, which is implemented in
  // set_mask_for_ep3_game_command in SendCommands.cc. The Episode 3 client
  // never sends commands that have a nonzero value in this field, but it does
  // properly handle received commands with nonzero values in this field.
  uint8_t mask_key = 0x00;
  uint8_t unused2 = 0x00;
} __packed__;

// The 6xB3 subcommand has a longer header, which is common to all CAx
// subcommands.
struct G_CardServerDataCommandHeader {
  uint8_t subcommand = 0x00;
  uint8_t size = 0x00;
  le_uint16_t unused1 = 0x0000;
  uint8_t subsubcommand = 0x00; // See 6xBx subcommand table (after this table)
  uint8_t sender_client_id = 0x00;
  uint8_t mask_key = 0x00; // Same meaning as in G_CardBattleCommandHeader
  uint8_t unused2 = 0x00;
  be_uint32_t sequence_num;
  be_uint32_t context_token;
} __packed__;

// 6xB4: Unknown (XBOX)
// 6xB4: CARD battle server response (Episode 3) - see 6xB3 (above)
// 6xB5: CARD battle client command (Episode 3) - see 6xB3 (above)
// 6xB5: BB shop request (handled by the server)

struct G_ShopContentsRequest_BB_6xB5 {
  G_UnusedHeader header;
  le_uint32_t shop_type;
} __packed__;

// 6xB6: Episode 3 map list and map contents (server->client only)
// Unlike 6xB3-6xB5, these commands cannot be masked. Also unlike 6xB3-6xB5,
// there are only two subsubcommands, so we list them inline here.
// These subcommands can be rather large, so they should be sent with the 6C
// command instead of the 60 command. (The difference in header format,
// including the extended size field, is likely the reason for 6xB6 being a
// separate subcommand from the other CARD battle subcommands.)

struct G_MapSubsubcommand_GC_Ep3_6xB6 {
  G_ExtendedHeader<G_UnusedHeader> header;
  uint8_t subsubcommand; // 0x40 or 0x41
  parray<uint8_t, 3> unused;
} __packed__;

struct G_MapList_GC_Ep3_6xB6x40 {
  G_MapSubsubcommand_GC_Ep3_6xB6 header;
  le_uint16_t compressed_data_size;
  le_uint16_t unused;
  // PRS-compressed map list data follows here. newserv generates this from the
  // map index at startup time; see the MapList struct in Episode3/DataIndex.hh
  // and Episode3::DataIndex::get_compressed_map_list for details on the format.
} __packed__;

struct G_MapData_GC_Ep3_6xB6x41 {
  G_MapSubsubcommand_GC_Ep3_6xB6 header;
  le_uint32_t map_number;
  le_uint16_t compressed_data_size;
  le_uint16_t unused;
  // PRS-compressed map data follows here (which decompresses to an
  // Episode3::MapDefinition).
} __packed__;

// 6xB6: BB shop contents (server->client only)

struct G_ShopContents_BB_6xB6 {
  G_UnusedHeader header;
  uint8_t shop_type;
  uint8_t num_items;
  le_uint16_t unused;
  // Note: data2d of these entries should be the price
  ItemData entries[20];
} __packed__;

// 6xB7: Alias for 6xB3 (Episode 3 Trial Edition)
// This command behaves exactly the same as 6xB3. This alias exists only in
// Episode 3 Trial Edition; it was removed in the final release.

// 6xB7: BB buy shop item (handled by the server)

struct G_BuyShopItem_BB_6xB7 {
  G_UnusedHeader header;
  le_uint32_t inventory_item_id;
  uint8_t shop_type;
  uint8_t item_index;
  uint8_t amount;
  uint8_t unknown_a1; // TODO: Probably actually unused; verify this
} __packed__;

// 6xB8: Alias for 6xB4 (Episode 3 Trial Edition)
// This command behaves exactly the same as 6xB4. This alias exists only in
// Episode 3 Trial Edition; it was removed in the final release.

// 6xB8: BB accept tekker result (handled by the server)

struct G_AcceptItemIdentification_BB_6xB8 {
  G_UnusedHeader header;
  le_uint32_t item_id;
} __packed__;

// 6xB9: Alias for 6xB5 (Episode 3 Trial Edition)
// This command behaves exactly the same as 6xB5. This alias exists only in
// Episode 3 Trial Edition; it was removed in the final release.

// 6xB9: BB provisional tekker result

struct G_IdentifyResult_BB_6xB9 {
  G_ClientIDHeader header;
  ItemData item;
} __packed__;

// 6xBA: Sync card trade state (Episode 3)
// This command calls various member functions in TCardTradeServer.

struct G_SyncCardTradeState_GC_Ep3_6xBA {
  G_ClientIDHeader header;
  le_uint16_t what; // Low byte must be < 9; this indexes into a jump table
  le_uint16_t unknown_a2;
  le_uint32_t unknown_a3;
  le_uint32_t unknown_a4;
} __packed__;

// 6xBA: BB accept tekker result (handled by the server)

struct G_AcceptItemIdentification_BB_6xBA {
  G_UnusedHeader header;
  le_uint32_t item_id;
} __packed__;

// 6xBB: Sync card trade state (Episode 3)
// This command calls various member functions in TCardTradeServer.
// TODO: Certain invalid values for slot/args in this command can crash the
// client (what is properly bounds-checked). Find out the actual limits for
// slot/args and make newserv enforce them.

struct G_SyncCardTradeState_GC_Ep3_6xBB {
  G_ClientIDHeader header;
  le_uint16_t what; // Must be < 5; this indexes into a jump table
  le_uint16_t slot;
  parray<le_uint32_t, 4> args;
} __packed__;

// 6xBB: BB bank request (handled by the server)

// 6xBC: Card counts (Episode 3)
// This is sent by the client in response to a 6xB5x38 command.
// It's possible that this is an early, now-unused implementation of the CAx49
// command. When the client receives this command, it copies the data into a
// globally-allocated array, but nothing reads from this array. Curiously, this
// command is smaller than 0x400 bytes, but uses the extended subcommand format
// anyway (and uses the 6D command rather than 62).

struct G_CardCounts_GC_Ep3_6xBC {
  G_UnusedHeader header;
  le_uint32_t size;
  parray<uint8_t, 0x2F1> unknown_a1;
  // The client sends uninitialized data in this field
  parray<uint8_t, 3> unused;
} __packed__;

// 6xBC: BB bank contents (server->client only)

struct G_BankContentsHeader_BB_6xBC {
  G_ExtendedHeader<G_UnusedHeader> header;
  le_uint32_t checksum; // can be random; client won't notice
  le_uint32_t numItems;
  le_uint32_t meseta;
  // Item data follows
} __packed__;

// 6xBD: Word select during battle (Episode 3; not Trial Edition)

// Note: This structure does not have a normal header - the client ID field is
// big-endian!
struct G_WordSelectDuringBattle_GC_Ep3_6xBD {
  G_ClientIDHeader header;
  le_uint16_t unknown_a1;
  le_uint16_t unknown_a2;
  parray<le_uint16_t, 8> entries;
  le_uint32_t unknown_a4;
  le_uint32_t unknown_a5;
  // This field has the same meaning as the first byte in an 06 command's
  // message when sent during an Episode 3 battle.
  uint8_t private_flags;
  parray<uint8_t, 3> unused;
} __packed__;

// 6xBD: BB bank action (take/deposit meseta/item) (handled by the server)

struct G_BankAction_BB_6xBD {
  G_UnusedHeader header;
  le_uint32_t item_id; // 0xFFFFFFFF = meseta; anything else = item
  le_uint32_t meseta_amount;
  uint8_t action; // 0 = deposit, 1 = take
  uint8_t item_amount;
  le_uint16_t unused2;
} __packed__;

// 6xBE: Sound chat (Episode 3; not Trial Edition)
// This is the only subcommand ever sent with the CB command.

struct G_SoundChat_GC_Ep3_6xBE {
  G_UnusedHeader header;
  le_uint32_t sound_id; // Must be < 0x27
  be_uint32_t unused;
} __packed__;

// 6xBE: BB create inventory item (server->client only)

struct G_CreateInventoryItem_BB_6xBE {
  G_ClientIDHeader header;
  ItemData item;
  le_uint32_t unused;
} __packed__;

// 6xBF: Change lobby music (Episode 3; not Trial Edition)

struct G_ChangeLobbyMusic_GC_Ep3_6xBF {
  G_UnusedHeader header;
  le_uint32_t song_number; // Must be < 0x34
} __packed__;

// 6xBF: Give EXP (BB) (server->client only)

struct G_GiveExperience_BB_6xBF {
  G_ClientIDHeader header;
  le_uint32_t amount;
} __packed__;

// 6xC0: BB sell item at shop

struct G_SellItemAtShop_BB_6xC0 {
  G_UnusedHeader header;
  le_uint32_t item_id;
  le_uint32_t amount;
} __packed__;

// 6xC1: Unknown (BB)
// 6xC2: Unknown (BB)

// 6xC3: Split stacked item (BB; handled by the server)
// Note: This is not sent if an entire stack is dropped; in that case, a normal
// item drop subcommand is generated instead.

struct G_SplitStackedItem_6xC3 {
  G_ClientIDHeader header;
  le_uint16_t area;
  le_uint16_t unused2;
  le_float x;
  le_float z;
  le_uint32_t item_id;
  le_uint32_t amount;
} __packed__;

// 6xC4: Sort inventory (BB; handled by the server)

struct G_SortInventory_6xC4 {
  G_UnusedHeader header;
  le_uint32_t item_ids[30];
} __packed__;

// 6xC5: Medical center used (BB)
// 6xC6: Invalid subcommand
// 6xC7: Invalid subcommand

// 6xC8: Enemy killed (BB; handled by the server)

struct G_EnemyKilled_6xC8 {
  G_EnemyIDHeader header;
  le_uint16_t enemy_id;
  le_uint16_t killer_client_id;
  le_uint32_t unused;
} __packed__;

// 6xC9: Invalid subcommand
// 6xCA: Invalid subcommand
// 6xCB: Unknown (BB)
// 6xCC: Unknown (BB)
// 6xCD: Unknown (BB)
// 6xCE: Unknown (BB)
// 6xCF: Unknown (BB; supported; game only; handled by the server)
// 6xD0: Invalid subcommand
// 6xD1: Invalid subcommand
// 6xD2: Unknown (BB)
// 6xD3: Invalid subcommand
// 6xD4: Unknown (BB)
// 6xD5: Invalid subcommand
// 6xD6: Invalid subcommand
// 6xD7: Invalid subcommand
// 6xD8: Invalid subcommand
// 6xD9: Invalid subcommand
// 6xDA: Invalid subcommand
// 6xDB: Unknown (BB)
// 6xDC: Unknown (BB)
// 6xDD: Unknown (BB)
// 6xDE: Invalid subcommand
// 6xDF: Invalid subcommand
// 6xE0: Invalid subcommand
// 6xE1: Invalid subcommand
// 6xE2: Invalid subcommand
// 6xE3: Unknown (BB)
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

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// EPISODE 3 CARD BATTLE SUBSUBCOMMANDS ////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// The Episode 3 CARD battle subsubcommands are used in commands 6xB3, 6xB4, and
// 6xB5. Note that even though there's no overlap in the subsubcommand number
// space, the various subsubcommands must be used with the correct 6xBx
// subcommand - the client will ignore the command if sent via the wrong 6xBx
// subcommand. (For example, sending a 6xB5x02 command will do nothing because
// subsubcommand 02 is only valid for the 6xB4 subcommand.) This table is known
// to be complete, so invalid commands are not listed.

// In general, 6xB3 (CAx) commands are sent by the client when it wants to take
// an action that affects game state held on the server side. The server will
// send one or more 6xB4 commands in response to update all clients' views of
// the game state. 6xB5 commands do not affect state held on the server side,
// and are generally only of concern on the client side.

// 6xB4x02: Update hand and equips

struct G_UpdateHand_GC_Ep3_6xB4x02 {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_UpdateHand_GC_Ep3_6xB4x02) / 4, 0, 0x02, 0, 0, 0};
  le_uint16_t client_id = 0;
  le_uint16_t unused = 0;
  Episode3::HandAndEquipState state;
} __packed__;

// 6xB4x03: Set state flags

struct G_SetStateFlags_GC_Ep3_6xB4x03 {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_SetStateFlags_GC_Ep3_6xB4x03) / 4, 0, 0x03, 0, 0, 0};
  Episode3::StateFlags state;
} __packed__;

// 6xB4x04: Update SC/FC short statuses

struct G_UpdateShortStatuses_GC_Ep3_6xB4x04 {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_UpdateShortStatuses_GC_Ep3_6xB4x04) / 4, 0, 0x04, 0, 0, 0};
  le_uint16_t client_id = 0;
  le_uint16_t unused = 0;
  // The slots in this array have heterogeneous meanings. Specifically:
  // [0] is the SC card status
  // [1] through [6] are hand cards
  // [7] through [14] are set FC cards (items/creatures)
  // [15] is the set assist card
  parray<Episode3::CardShortStatus, 0x10> card_statuses;
} __packed__;

// 6xB4x05: Update map state

struct G_UpdateMap_GC_Ep3_6xB4x05 {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_UpdateMap_GC_Ep3_6xB4x05) / 4, 0, 0x05, 0, 0, 0};
  Episode3::MapAndRulesState state;
  uint8_t unknown_a1 = 0;
  parray<uint8_t, 3> unused;
} __packed__;

// 6xB4x06: Apply condition effect

struct G_ApplyConditionEffect_GC_Ep3_6xB4x06 {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_ApplyConditionEffect_GC_Ep3_6xB4x06) / 4, 0, 0x06, 0, 0, 0};
  Episode3::EffectResult effect;
} __packed__;

// 6xB4x07: Set battle decks

struct G_UpdateDecks_GC_Ep3_6xB4x07 {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_UpdateDecks_GC_Ep3_6xB4x07) / 4, 0, 0x07, 0, 0, 0};
  parray<uint8_t, 4> entries_present;
  parray<Episode3::DeckEntry, 4> entries;
} __packed__;

// 6xB4x09: Set action state

struct G_SetActionState_GC_Ep3_6xB4x09 {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_SetActionState_GC_Ep3_6xB4x09) / 4, 0, 0x09, 0, 0, 0};
  le_uint16_t client_id = 0;
  parray<uint8_t, 2> unknown_a1;
  Episode3::ActionState state;
} __packed__;

// 6xB4x0A: Update action chain and metadata
// This command seems to be unused; Sega's server implementation never sends it.

struct G_UpdateActionChainAndMetadata_GC_Ep3_6xB4x0A {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_UpdateActionChainAndMetadata_GC_Ep3_6xB4x0A) / 4, 0, 0x0A, 0, 0, 0};
  le_uint16_t client_id = 0;
  // set_index must be 0xFF, or be in the range [0, 9]. If it's 0xFF, all nine
  // chains and metadatas are cleared for the client; otherwise, the provided
  // chain and metadata are copied into the slot specified by set_index.
  int8_t set_index = 0;
  uint8_t unused = 0;
  Episode3::ActionChainWithConds chain;
  Episode3::ActionMetadata metadata;
} __packed__;

// 6xB3x0B / CAx0B: Redraw initial hand (immediately before battle)

struct G_RedrawInitialHand_GC_Ep3_6xB3x0B_CAx0B {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_RedrawInitialHand_GC_Ep3_6xB3x0B_CAx0B) / 4, 0, 0x0B, 0, 0, 0, 0, 0};
  le_uint16_t client_id = 0;
  parray<uint8_t, 2> unused2;
} __packed__;

// 6xB3x0C / CAx0C: End initial redraw phase

struct G_EndInitialRedrawPhase_GC_Ep3_6xB3x0C_CAx0C {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_EndInitialRedrawPhase_GC_Ep3_6xB3x0C_CAx0C) / 4, 0, 0x0C, 0, 0, 0, 0, 0};
  le_uint16_t client_id = 0;
  parray<uint8_t, 2> unused2;
} __packed__;

// 6xB3x0D / CAx0D: End non-action phase
// This command is sent when the client has no more actions to take during the
// current phase. This command isn't used for ending the attack or defense
// phases; for those phases, CAx12 and CAx28 are used instead.

struct G_EndNonAttackPhase_GC_Ep3_6xB3x0D_CAx0D {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_EndNonAttackPhase_GC_Ep3_6xB3x0D_CAx0D) / 4, 0, 0x0D, 0, 0, 0, 0, 0};
  le_uint16_t client_id = 0;
  parray<le_uint16_t, 5> unused2;
} __packed__;

// 6xB3x0E / CAx0E: Discard card from hand

struct G_DiscardCardFromHand_GC_Ep3_6xB3x0E_CAx0E {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_DiscardCardFromHand_GC_Ep3_6xB3x0E_CAx0E) / 4, 0, 0x0E, 0, 0, 0, 0, 0};
  le_uint16_t client_id = 0;
  le_uint16_t card_ref = 0xFFFF;
} __packed__;

// 6xB3x0F / CAx0F: Set card from hand

struct G_SetCardFromHand_GC_Ep3_6xB3x0F_CAx0F {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_SetCardFromHand_GC_Ep3_6xB3x0F_CAx0F) / 4, 0, 0x0F, 0, 0, 0, 0, 0};
  le_uint16_t client_id = 0;
  le_uint16_t card_ref = 0xFFFF;
  le_uint16_t set_index = 0;
  le_uint16_t assist_target_player = 0;
  Episode3::Location loc;
} __packed__;

// 6xB3x10 / CAx10: Move field character

struct G_MoveFieldCharacter_GC_Ep3_6xB3x10_CAx10 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_MoveFieldCharacter_GC_Ep3_6xB3x10_CAx10) / 4, 0, 0x10, 0, 0, 0, 0, 0};
  le_uint16_t client_id = 0;
  le_uint16_t set_index = 0;
  Episode3::Location loc;
} __packed__;

// 6xB3x11 / CAx11: Enqueue action (play card(s) during action phase)
// This command is used for playing both attacks (and the associated action
// cards), and for playing defense cards. In the attack case, this command is
// sent once for each attack (even if it includes multiple cards); in the
// defense case, this command is sent once for each defense card.

struct G_EnqueueAttackOrDefense_GC_Ep3_6xB3x11_CAx11 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_EnqueueAttackOrDefense_GC_Ep3_6xB3x11_CAx11) / 4, 0, 0x11, 0, 0, 0, 0, 0};
  le_uint16_t client_id = 0;
  parray<uint8_t, 2> unused2;
  Episode3::ActionState entry;
} __packed__;

// 6xB3x12 / CAx12: End attack list (done playing cards during action phase)
// This command informs the server that the client is done playing attacks in
// the current round. (In the defense phase, CAx28 is used instead.)

struct G_EndAttackList_GC_Ep3_6xB3x12_CAx12 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_EndAttackList_GC_Ep3_6xB3x12_CAx12) / 4, 0, 0x12, 0, 0, 0, 0, 0};
  le_uint16_t client_id = 0;
  parray<uint8_t, 2> unused2;
} __packed__;

// 6xB3x13 / CAx13: Set map state during setup

struct G_SetMapState_GC_Ep3_6xB3x13_CAx13 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_SetMapState_GC_Ep3_6xB3x13_CAx13) / 4, 0, 0x13, 0, 0, 0, 0, 0};
  Episode3::MapAndRulesState map_and_rules_state;
  Episode3::OverlayState overlay_state;
} __packed__;

// 6xB3x14 / CAx14: Set player deck during setup

struct G_SetPlayerDeck_GC_Ep3_6xB3x14_CAx14 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_SetPlayerDeck_GC_Ep3_6xB3x14_CAx14) / 4, 0, 0x14, 0, 0, 0, 0, 0};
  le_uint16_t client_id = 0;
  uint8_t is_cpu_player = 0;
  uint8_t unused2 = 0;
  Episode3::DeckEntry entry;
} __packed__;

// 6xB3x15 / CAx15: Hard-reset server state
// This command appears to be completely unused; the client never sends it.

struct G_HardResetServerState_GC_Ep3_6xB3x15_CAx15 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_HardResetServerState_GC_Ep3_6xB3x15_CAx15) / 4, 0, 0x15, 0, 0, 0, 0, 0};
  // No arguments
} __packed__;

// 6xB5x17: Unknown
// TODO: Document this from Episode 3 client/server disassembly

struct G_Unknown_GC_Ep3_6xB5x17 {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_Unknown_GC_Ep3_6xB5x17) / 4, 0, 0x17, 0, 0, 0};
  // No arguments
} __packed__;

// 6xB5x1A: Force disconnect
// This command seems to cause the client to unconditionally disconnect. The
// player is returned to the main menu (the "The line was disconnected" message
// box is skipped). Unlike all other known ways to disconnect, the client does
// not save when it receives this command, and instead returns directly to the
// main menu.

struct G_ForceDisconnect_GC_Ep3_6xB5x1A {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_ForceDisconnect_GC_Ep3_6xB5x1A) / 4, 0, 0x1A, 0, 0, 0};
  // No arguments
} __packed__;

// 6xB3x1B / CAx1B: Set player name during setup
// Curiously, this command can be used during a non-setup phase; the server
// should ignore the command's contents but still send a 6xB4x1C in response.

struct G_SetPlayerName_GC_Ep3_6xB3x1B_CAx1B {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_SetPlayerName_GC_Ep3_6xB3x1B_CAx1B) / 4, 0, 0x1B, 0, 0, 0, 0, 0};
  Episode3::NameEntry entry;
} __packed__;

// 6xB4x1C: Set all player names

struct G_SetPlayerNames_GC_Ep3_6xB4x1C {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_SetPlayerNames_GC_Ep3_6xB4x1C) / 4, 0, 0x1C, 0, 0, 0};
  parray<Episode3::NameEntry, 4> entries;
} __packed__;

// 6xB3x1D / CAx1D: Request for battle start
// The battle actually begins when the server sends a state flags update (in
// response to this command) that includes RegistrationPhase::BATTLE_STARTED and
// a SetupPhase value other than REGISTRATION.

struct G_StartBattle_GC_Ep3_6xB3x1D_CAx1D {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_StartBattle_GC_Ep3_6xB3x1D_CAx1D) / 4, 0, 0x1D, 0, 0, 0, 0, 0};
} __packed__;

// 6xB4x1E: Action result

struct G_ActionResult_GC_Ep3_6xB4x1E {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_ActionResult_GC_Ep3_6xB4x1E) / 4, 0, 0x1E, 0, 0, 0};
  // TODO: Is this supposed to be big-endian or little-endian? The client makes
  // it look like it should be little-endian, but logs from the Sega servers
  // make it look like it should be big-endian.
  be_uint32_t sequence_num = 0;
  uint8_t error_code = 0;
  uint8_t response_phase = 0;
  parray<uint8_t, 2> unused;
} __packed__;

// 6xB4x1F: Set context token
// This token is sent back in the context_token field of all CA commands from
// the client. It seems Sega never used this functionality.

struct G_SetContextToken_GC_Ep3_6xB4x1F {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_SetContextToken_GC_Ep3_6xB4x1F) / 4, 0, 0x1F, 0, 0, 0};
  // Note that this field is little-endian, but the corresponding context_token
  // field in G_CardServerDataCommandHeader is big-endian!
  le_uint32_t context_token = 0;
} __packed__;

// 6xB5x20: Unknown
// TODO: Document this from Episode 3 client/server disassembly

struct G_Unknown_GC_Ep3_6xB5x20 {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_Unknown_GC_Ep3_6xB5x20) / 4, 0, 0x20, 0, 0, 0};
  le_uint32_t player_tag = 0x00010000;
  le_uint32_t guild_card_number = 0;
  uint8_t client_id = 0;
  parray<uint8_t, 3> unused;
} __packed__;

// 6xB3x21 / CAx21: End battle

struct G_EndBattle_GC_Ep3_6xB3x21_CAx21 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_EndBattle_GC_Ep3_6xB3x21_CAx21) / 4, 0, 0x21, 0, 0, 0, 0, 0};
  le_uint32_t unused2 = 0;
} __packed__;

// 6xB4x22: Unknown
// This command appears to be completely unused. The client's handler for this
// command sets a flag on some data structure if it exists, but it appears that
// that data structure is never allocated.

struct G_Unknown_GC_Ep3_6xB4x22 {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_Unknown_GC_Ep3_6xB4x22) / 4, 0, 0x22, 0, 0, 0};
  // No arguments
} __packed__;

// 6xB4x23: Unknown
// This command was actually sent by Sega's original servers, but it does
// nothing on the client.

struct G_Unknown_GC_Ep3_6xB4x23 {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_Unknown_GC_Ep3_6xB4x23) / 4, 0, 0x23, 0, 0, 0};
  uint8_t present = 0; // Handler expects this to be equal to 1
  uint8_t client_id = 0;
  parray<uint8_t, 2> unused;
} __packed__;

// 6xB5x27: Unknown
// TODO: Document this from Episode 3 client/server disassembly

struct G_Unknown_GC_Ep3_6xB5x27 {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_Unknown_GC_Ep3_6xB5x27) / 4, 0, 0x27, 0, 0, 0};
  // Note: This command uses header_b1 as well, which looks like another client
  // ID (it must be < 4, though it does not always match unknown_a1 below).
  le_uint32_t unknown_a1 = 0; // Probably client ID (must be < 4)
  le_uint32_t unknown_a2 = 0; // Must be < 0x10
  le_uint32_t unknown_a3 = 0;
  le_uint32_t unused = 0; // Curiously, this usually contains a memory address
} __packed__;

// 6xB3x28 / CAx28: End defense list
// This command informs the server that the client is done playing defense
// cards. (In the attack phase, CAx12 is used instead.)

struct G_EndDefenseList_GC_Ep3_6xB3x28_CAx28 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_EndDefenseList_GC_Ep3_6xB3x28_CAx28) / 4, 0, 0x28, 0, 0, 0, 0, 0};
  uint8_t unused1;
  uint8_t client_id = 0;
  parray<uint8_t, 2> unused2;
} __packed__;

// 6xB4x29: Set action state
// TODO: How is this different from 6xB4x09? It looks like the server never
// sends this.

struct G_SetActionState_GC_Ep3_6xB4x29 {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_SetActionState_GC_Ep3_6xB4x29) / 4, 0, 0x29, 0, 0, 0};
  uint8_t unknown_a1 = 0;
  parray<uint8_t, 3> unknown_a2;
  Episode3::ActionState state;
} __packed__;

// 6xB4x2A: Unknown
// TODO: Document this from Episode 3 client/server disassembly

struct G_Unknown_GC_Ep3_6xB4x2A {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_Unknown_GC_Ep3_6xB4x2A) / 4, 0, 0x2A, 0, 0, 0};
  parray<uint8_t, 4> unknown_a1;
  le_uint16_t unknown_a2 = 0;
  parray<uint8_t, 2> unused;
} __packed__;

// 6xB3x2B / CAx2B: Unknown
// It seems Sega's servers completely ignored this command.

struct G_Unknown_GC_Ep3_6xB3x2B_CAx2B {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_Unknown_GC_Ep3_6xB3x2B_CAx2B) / 4, 0, 0x2B, 0, 0, 0, 0, 0};
  le_uint16_t unused2 = 0;
  parray<uint8_t, 2> unused3;
} __packed__;

// 6xB4x2C: Unknown

struct G_Unknown_GC_Ep3_6xB4x2C {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_Unknown_GC_Ep3_6xB4x2C) / 4, 0, 0x2C, 0, 0, 0};
  uint8_t change_type = 0;
  uint8_t client_id = 0;
  parray<le_uint16_t, 3> card_refs;
  Episode3::Location loc;
  parray<le_uint32_t, 2> unknown_a2;
} __packed__;

// 6xB5x2D: Unknown
// TODO: Document this from Episode 3 client/server disassembly

struct G_Unknown_GC_Ep3_6xB5x2D {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_Unknown_GC_Ep3_6xB5x2D) / 4, 0, 0x2D, 0, 0, 0};
  // This array is indexed into by a global variable. I don't have any examples
  // of this command, so I don't know how long the array should be - 4 is a
  // probably-incorrect guess.
  parray<uint8_t, 4> unknown_a1;
} __packed__;

// 6xB5x2E: Notify other players that battle is about to end

struct G_BattleEndNotification_GC_Ep3_6xB5x2E {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_BattleEndNotification_GC_Ep3_6xB5x2E) / 4, 0, 0x2E, 0, 0, 0};
  uint8_t unknown_a1 = 0; // Command ignored unless this is 0 or 1
  parray<uint8_t, 3> unused;
} __packed__;

// 6xB5x2F: Unknown
// TODO: Document this from Episode 3 client/server disassembly

struct G_Unknown_GC_Ep3_6xB5x2F {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_Unknown_GC_Ep3_6xB5x2F) / 4, 0, 0x2F, 0, 0, 0};
  parray<uint8_t, 4> unknown_a1;

  parray<uint8_t, 0x18> unknown_a2;
  ptext<uint8_t, 0x10> deck_name;
  parray<uint8_t, 0x0E> unknown_a3;
  le_uint16_t unknown_a4 = 0;
  parray<le_uint32_t, 0x1F> card_ids;
  parray<uint8_t, 2> unused;
  le_uint32_t unknown_a5 = 0;
  le_uint16_t unknown_a6 = 0;
  le_uint16_t unknown_a7 = 0;
} __packed__;

// 6xB5x30: Unknown
// TODO: Document this from Episode 3 client/server disassembly

struct G_Unknown_GC_Ep3_6xB5x30 {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_Unknown_GC_Ep3_6xB5x30) / 4, 0, 0x30, 0, 0, 0};
  // No arguments
} __packed__;

// 6xB5x31: Unknown
// TODO: Document this from Episode 3 client/server disassembly

struct G_Unknown_GC_Ep3_6xB5x31 {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_Unknown_GC_Ep3_6xB5x31) / 4, 0, 0x31, 0, 0, 0};
  // Note: This command uses header_b1 for... something.
  uint8_t unknown_a1 = 0; // Must be 0 or 1
  uint8_t unknown_a2 = 0; // Must be < 4
  uint8_t unknown_a3 = 0; // Must be < 4
  uint8_t unknown_a4 = 0; // Must be < 0x14
  uint8_t unknown_a5 = 0; // Used as an array index
  parray<uint8_t, 3> unused;
} __packed__;

// 6xB5x32: Unknown
// TODO: Document this from Episode 3 client/server disassembly

struct G_Unknown_GC_Ep3_6xB5x32 {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_Unknown_GC_Ep3_6xB5x32) / 4, 0, 0x32, 0, 0, 0};
  // Note: This command uses header_b1 for... something.
  le_uint16_t unknown_a1 = 0;
  le_uint16_t unknown_a2 = 0;
  parray<uint8_t, 8> unknown_a3;
} __packed__;

// 6xB4x33: Subtract ally ATK points (e.g. for photon blast)

struct G_SubtractAllyATKPoints_GC_Ep3_6xB4x33 {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_SubtractAllyATKPoints_GC_Ep3_6xB4x33) / 4, 0, 0x33, 0, 0, 0};
  uint8_t client_id = 0;
  uint8_t ally_cost = 0;
  le_uint16_t card_ref = 0xFFFF;
} __packed__;

// 6xB3x34 / CAx34: Photon blast request

struct G_PhotonBlastRequest_GC_Ep3_6xB3x34_CAx34 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_PhotonBlastRequest_GC_Ep3_6xB3x34_CAx34) / 4, 0, 0x34, 0, 0, 0, 0, 0};
  uint8_t ally_client_id = 0;
  uint8_t reason = 0;
  le_uint16_t card_ref = 0xFFFF;
} __packed__;

// 6xB4x35: Update photon blast status

struct G_PhotonBlastStatus_GC_Ep3_6xB4x35 {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_PhotonBlastStatus_GC_Ep3_6xB4x35) / 4, 0, 0x35, 0, 0, 0};
  uint8_t client_id = 0;
  uint8_t accepted = 0;
  le_uint16_t card_ref = 0xFFFF;
} __packed__;

// 6xB5x36: Unknown
// TODO: Document this from Episode 3 client/server disassembly
// Setting unknown_a1 to a value 4 or greater while in a game causes the player
// to be temporarily replaced with a default HUmar and placed inside the central
// column in the Morgue, rendering them unable to move. The only ways out of
// this predicament appear to be either to disconnect (e.g. select Quit Game
// from the pause menu) or receive an ED (force leave game) command.

struct G_Unknown_GC_Ep3_6xB5x36 {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_Unknown_GC_Ep3_6xB5x36) / 4, 0, 0x36, 0, 0, 0};
  uint8_t unknown_a1 = 0; // Must be < 12 (maybe lobby or spectator team client ID)
  parray<uint8_t, 3> unused;
} __packed__;

// 6xB3x37 / CAx37: Ready to advance from starting rolls phase

struct G_AdvanceFromStartingRollsPhase_GC_Ep3_6xB3x37_CAx37 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_AdvanceFromStartingRollsPhase_GC_Ep3_6xB3x37_CAx37) / 4, 0, 0x37, 0, 0, 0, 0, 0};
  uint8_t client_id = 0;
  parray<uint8_t, 3> unused2;
} __packed__;

// 6xB5x38: Card counts request
// This command causes the client identified by requested_client_id to send a
// 6xBC command to the client identified by reply_to_client_id (privately, via
// the 6D command). This appears to be unused; it is likely superseded by the
// CAx49 command.

struct G_CardCountsRequest_GC_Ep3_6xB5x38 {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_CardCountsRequest_GC_Ep3_6xB5x38) / 4, 0, 0x38, 0, 0, 0};
  uint8_t requested_client_id = 0;
  uint8_t reply_to_client_id = 0;
  parray<uint8_t, 2> unused;
} __packed__;

// 6xB4x39: Update all player statistics

struct G_UpdateAllPlayerStatistics_GC_Ep3_6xB4x39 {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_UpdateAllPlayerStatistics_GC_Ep3_6xB4x39) / 4, 0, 0x39, 0, 0, 0};
  parray<Episode3::PlayerStats, 4> stats;
} __packed__;

// 6xB3x3A / CAx3A: Unknown
// It seems Sega's servers completely ignored this command.

struct G_Unknown_GC_Ep3_6xB3x3A_CAx3A {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_Unknown_GC_Ep3_6xB3x3A_CAx3A) / 4, 0, 0x3A, 0, 0, 0, 0, 0};
} __packed__;

// 6xB4x3B: Unknown
// TODO: Document this from Episode 3 client/server disassembly

struct G_Unknown_GC_Ep3_6xB4x3B {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_Unknown_GC_Ep3_6xB4x3B) / 4, 0, 0x3B, 0, 0, 0};
  parray<uint8_t, 4> unused;
} __packed__;

// 6xB5x3C: Set player substatus
// This command sets the text that appears under the player's name in the HUD.

struct G_SetPlayerSubstatus_GC_Ep3_6xB5x3C {
  // Note: header.sender_client_id specifies which client's status to update
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_SetPlayerSubstatus_GC_Ep3_6xB5x3C) / 4, 0, 0x3C, 0, 0, 0};
  // Status values:
  // 00 (or any value not listed below) = (nothing)
  // 01 = Editing
  // 02 = Trading...
  // 03 = At Counter
  uint8_t status = 0;
  parray<uint8_t, 3> unused;
} __packed__;

// 6xB4x3D: Set tournament player decks
// This is sent before the counter sequence in a tournament game, to reserve the
// player and COM slots and set the map number.

struct G_SetTournamentPlayerDecks_GC_Ep3_6xB4x3D {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_SetTournamentPlayerDecks_GC_Ep3_6xB4x3D) / 4, 0, 0x3D, 0, 0, 0};
  Episode3::Rules rules;
  parray<uint8_t, 4> unknown_a1;
  struct Entry {
    uint8_t type = 0; // 0 = no player, 1 = human, 2 = COM
    ptext<char, 0x10> player_name;
    ptext<char, 0x10> deck_name; // Only be used for COM players
    parray<uint8_t, 5> unknown_a1;
    parray<le_uint16_t, 0x1F> card_ids; // Can be blank for human players
    uint8_t client_id = 0; // Unused for COMs
    uint8_t unknown_a4 = 0;
    le_uint16_t unknown_a2 = 0;
    le_uint16_t unknown_a3 = 0;
  } __packed__;
  parray<Entry, 4> entries;
  le_uint32_t map_number = 0;
  uint8_t player_slot = 0; // Which deck slot is editable by the client
  uint8_t unknown_a3 = 0;
  uint8_t unknown_a4 = 0;
  uint8_t unknown_a5 = 0;
} __packed__;

// 6xB5x3E: Make card auction bid

struct G_MakeCardAuctionBid_GC_Ep3_6xB5x3E {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_MakeCardAuctionBid_GC_Ep3_6xB5x3E) / 4, 0, 0x3E, 0, 0, 0};
  // Note: This command uses header.unknown_a1 for the bidder's client ID.
  uint8_t card_index = 0; // Index of card in EF command
  uint8_t bid_value = 0; // 1-99
  parray<uint8_t, 2> unused;
} __packed__;

// 6xB5x3F: Open blocking menu
// This command opens a shared menu between all clients in a game. The client
// specified in .client_id is able to control the menu; the other clients see
// that player's actions but cannot control anything.

struct G_OpenBlockingMenu_GC_Ep3_6xB5x3F {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_OpenBlockingMenu_GC_Ep3_6xB5x3F) / 4, 0, 0x3F, 0, 0, 0};
  // Menu type should be one of these values:
  // 0x01/0x02 = battle prep menu
  // 0x11 = card auction counter menu (join or cancel)
  // 0x12 = go directly to card auction state (client sends EF command)
  // Other values will likely crash the client.
  int8_t menu_type = 0; // Must be in the range [-1, 0x14]
  uint8_t client_id = 0;
  parray<uint8_t, 2> unused1;
  le_uint32_t unknown_a3 = 0;
  parray<uint8_t, 4> unused2;
} __packed__;

// 6xB3x40 / CAx40: Request map list
// The server should respond with a 6xB6x40 command.

struct G_MapListRequest_GC_Ep3_6xB3x40_CAx40 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_MapListRequest_GC_Ep3_6xB3x40_CAx40) / 4, 0, 0x40, 0, 0, 0, 0, 0};
} __packed__;

// 6xB3x41 / CAx41: Request map data
// The server should respond with a 6xB6x41 command containing the definition of
// the specified map.

struct G_MapDataRequest_GC_Ep3_6xB3x41_CAx41 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_MapDataRequest_GC_Ep3_6xB3x41_CAx41) / 4, 0, 0x41, 0, 0, 0, 0, 0};
  le_uint32_t map_number = 0;
} __packed__;

// 6xB5x42: Initiate card auction
// Sending this command to a client has the same effect as sending a 6xB5x3F
// command to tell it to open the auction menu. (This works even if the client
// doesn't have a VIP card or there are fewer than 4 players in the current
// game.) Under normal operation, the server doesn't need to do this - the
// client sends this when all of the following conditions are met:
// 1. The client has a VIP card. (This is stored client-side in seq flag 7000.)
// 2. The client is in a game with 4 players.
// 3. All clients are at the auction counter.

struct G_InitiateCardAuction_GC_Ep3_6xB5x42 {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_InitiateCardAuction_GC_Ep3_6xB5x42) / 4, 0, 0x42, 0, 0, 0};
  // This command uses header.unknown_a1 (probably for the client's ID).
} __packed__;

// 6xB5x43: Unknown
// TODO: Document this from Episode 3 client/server disassembly

struct G_Unknown_GC_Ep3_6xB5x43 {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_Unknown_GC_Ep3_6xB5x43) / 4, 0, 0x43, 0, 0, 0};
  struct Entry {
    // Both fields here are masked. To get the actual values used by the game,
    // XOR the values here with 0x39AB.
    le_uint16_t masked_card_id = 0xFFFF; // Must be < 0x2F1 (when unmasked)
    le_uint16_t masked_unknown_a1 = 0; // Must be in [1, 99] (when unmasked)
  } __packed__;
  parray<Entry, 0x14> entries;
} __packed__;

// 6xB5x44: Card auction bid summary

struct G_CardAuctionBidSummary_GC_Ep3_6xB5x44 {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_CardAuctionBidSummary_GC_Ep3_6xB5x44) / 4, 0, 0x44, 0, 0, 0};
  // Note: This command uses header.unknown_a1 for the bidder's client ID.
  parray<le_uint16_t, 8> bids; // In same order as cards in the EF command
} __packed__;

// 6xB5x45: Card auction results

struct G_CardAuctionResults_GC_Ep3_6xB5x45 {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_CardAuctionResults_GC_Ep3_6xB5x45) / 4, 0, 0x45, 0, 0, 0};
  // Note: This command uses header.unknown_a1 for the sender's client ID.
  // This array is indexed by [card_index][client_id], and contains the final
  // bid for each player on each card (or 0 if they did not bid on that card).
  parray<parray<le_uint16_t, 4>, 8> bids_by_player;
} __packed__;

// 6xB4x46: Server version strings
// This command doesn't seem to be necessary to actually play the game; the
// client just copies the included strings to global buffers and then ignores
// them. Sega's servers sent this twice for each battle, however: once after the
// initial setup phase (before starter rolls) and once when the results screen
// appeared. The second instance of this command appears to be caused by them
// recreating the TCardServer object (implemented in newserv's Episode3::Server)
// in order to support multiple sequential battles in the same team.

struct G_ServerVersionStrings_GC_Ep3_6xB4x46 {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_ServerVersionStrings_GC_Ep3_6xB4x46) / 4, 0, 0x46, 0, 0, 0};
  // In all of the examples (from Sega's servers) that I've seen of this
  // command, these fields have the following values:
  //   version_signature = "[V1][FINAL2.0] 03/09/13 15:30 by K.Toya"
  //   date_str1 = "Mar  7 2007 21:42:40"
  // In the client, date_str1 is different:
  //   version_signature = "[V1][FINAL2.0] 03/09/13 15:30 by K.Toya"
  //   date_str1 = "Jan 21 2004 18:36:47'
  // Presumably if any logs exist from before 7 March 2007, they would have a
  // different date_str1, but the unchanged version_signature likely means that
  // Sega never made any code changes on the server side.
  ptext<char, 0x40> version_signature;
  ptext<char, 0x40> date_str1; // Probably card definitions revision date
  // In Sega's implementation, it seems this field is blank when starting a
  // battle, and contains the current time (in the format "YYYY/MM/DD hh:mm:ss")
  // when ending a battle. This may have been used for identifying debug logs.
  ptext<char, 0x40> date_str2;
  // It seems Sega used to send 0 here when starting a battle, and 0x04157580
  // when ending a battle. Since the field is unused by the client, it's not
  // clear what that value means, if anything. This behavior may be another
  // uninitialized memory bug in the server implementation (of which there are
  // many other examples).
  le_uint32_t unused = 0;
} __packed__;

// 6xB5x47: Unknown
// TODO: Document this from Episode 3 client/server disassembly

struct G_Unknown_GC_Ep3_6xB5x47 {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_Unknown_GC_Ep3_6xB5x47) / 4, 0, 0x47, 0, 0, 0};
  // Note: This command uses header_b1, which must be < 12.
  le_uint32_t unknown_a1 = 0;
} __packed__;

// 6xB3x48 / CAx48: End turn

struct G_EndTurn_GC_Ep3_6xB3x48_CAx48 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_EndTurn_GC_Ep3_6xB3x48_CAx48) / 4, 0, 0x48, 0, 0, 0, 0, 0};
  uint8_t client_id = 0;
  parray<uint8_t, 3> unused2;
} __packed__;

// 6xB3x49 / CAx49: Card counts
// This command is sent when a client joins a game, but it is completely ignored
// by the original Episode 3 server. Sega presumably could have used this to
// detect the presence of unreleased cards to ban cheaters, but the effects of
// the non-saveable Have All Cards AR code don't appear in this data, so this
// would have been ineffective. There appears to be a place where Sega's server
// intended to use this data, however - the deck verification function takes a
// pointer to the card counts array, but Sega's implementation always passes
// null there, which skips the owned card count check. newserv uses this data at
// that callsite to implement one of the deck validity checks.

struct G_CardCounts_GC_Ep3_6xB3x49_CAx49 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_CardCounts_GC_Ep3_6xB3x49_CAx49) / 4, 0, 0x49, 0, 0, 0, 0, 0};
  uint8_t basis = 0;
  parray<uint8_t, 3> unused;
  // This is encrypted with the trivial algorithm (see decrypt_trivial_gci_data)
  // using the basis in the preceding field
  parray<uint8_t, 0x2F0> card_id_to_count;
} __packed__;

// 6xB4x4A: Unknown
// TODO: Document this from Episode 3 client/server disassembly

struct G_Unknown_GC_Ep3_6xB4x4A {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_Unknown_GC_Ep3_6xB4x4A) / 4, 0, 0x4A, 0, 0, 0};
  // Note: entry_count appears not to be bounds-checked; presumably the server
  // could send up to 0xFF entries, but those after the 8th would not be
  // byteswapped before the client handles them.
  uint8_t client_id = 0;
  uint8_t entry_count = 0;
  le_uint16_t round_num = 0;
  parray<le_uint16_t, 8> card_refs;
} __packed__;

// 6xB4x4B: Set EX result values
// This command specifies how much EX the player should get based on the
// difference between their level and the levels of the players they defeated or
// were defeated by. (For multi-player opponent teams, the average of the
// opponents' levels is used.) The game scans the appropriate list for the entry
// whose threshold is less than or equal to than the level difference, and
// returns the corresponding value. For example, if the first two entries in the
// win list are {20, 40} and {10, 30}, and the player defeats an opponent who is
// 15 levels above the player's level, the player will get 30 EX when they win
// the battle. If all thresholds are greater than the level difference, the last
// entry's value is used. Finally, if the opponent team has no humans on it, the
// resulting EX values are divided by 2 (so in the example above, the player
// would only get 15 EX for defeating COMs).

// If any entry in either list has .value < -100 or > 100, the entire command is
// ignored and the EX thresholds and values are reset to their default values.
// These default values are:
// win_entries = {50, 100}, {30, 80}, {15, 70}, {10, 55}, {7, 45}, {4, 35},
//               {1, 25}, {-1, 20}, {-9, 15}, {0, 10}
// lose_entries = {1, 0}, {-2, 0}, {-3, 0}, {-4, 0}, {-5, 0}, {-6, 0}, {-7, 0},
//                {-10, -10}, {-30, -10}, {0, -15}

struct G_SetEXResultValues_GC_Ep3_6xB4x4B {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_SetEXResultValues_GC_Ep3_6xB4x4B) / 4, 0, 0x4B, 0, 0, 0};
  struct Entry {
    le_int16_t threshold;
    le_int16_t value;
  } __packed__;
  parray<Entry, 10> win_entries;
  parray<Entry, 10> lose_entries;
} __packed__;

// 6xB4x4C: Update action chain

struct G_UpdateActionChain_GC_Ep3_6xB4x4C {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_UpdateActionChain_GC_Ep3_6xB4x4C) / 4, 0, 0x4C, 0, 0, 0};
  uint8_t client_id = 0;
  int8_t index = 0;
  parray<uint8_t, 2> unused;
  Episode3::ActionChain chain;
} __packed__;

// 6xB4x4D: Update action metadata

struct G_UpdateActionMetadata_GC_Ep3_6xB4x4D {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_UpdateActionMetadata_GC_Ep3_6xB4x4D) / 4, 0, 0x4D, 0, 0, 0};
  uint8_t client_id = 0;
  int8_t index = 0;
  parray<uint8_t, 2> unused;
  Episode3::ActionMetadata metadata;
} __packed__;

// 6xB4x4E: Update card conditions

struct G_UpdateCardConditions_GC_Ep3_6xB4x4E {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_UpdateCardConditions_GC_Ep3_6xB4x4E) / 4, 0, 0x4E, 0, 0, 0};
  uint8_t client_id = 0;
  int8_t index = 0;
  parray<uint8_t, 2> unused;
  parray<Episode3::Condition, 9> conditions;
} __packed__;

// 6xB4x4F: Clear set card conditions

struct G_ClearSetCardConditions_GC_Ep3_6xB4x4F {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_ClearSetCardConditions_GC_Ep3_6xB4x4F) / 4, 0, 0x4F, 0, 0, 0};
  uint8_t client_id = 0;
  uint8_t unused = 0;
  // For each 1 bit in this mask, the conditions of the corresponding card
  // should be deleted. The low bit corresponds to the SC card; the next bit
  // corresponds to set slot 0, the next bit to set slot 1, etc. (The upper 7
  // bits of this field are unused.)
  le_uint16_t clear_mask = 0;
} __packed__;

// 6xB4x50: Set trap tile locations

struct G_SetTrapTileLocations_GC_Ep3_6xB4x50 {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_SetTrapTileLocations_GC_Ep3_6xB4x50) / 4, 0, 0x50, 0, 0, 0};
  // Each entry in this array corresponds to one of the 5 trap types, in order.
  // Each entry is an [x, y] pair; if that trap type is not present, its
  // location entry is FF FF.
  parray<parray<uint8_t, 2>, 5> locations;
  parray<uint8_t, 2> unused;
} __packed__;

// 6xB4x51: Tournament match result
// This is sent as soon as the battle result is determined (before the battle
// results screen). If the client is in tournament mode (tournament_flag is 1 in
// the StateFlags struct), then it will use this information to show the
// tournament match result screen before the battle results screen.

struct G_TournamentMatchResult_GC_Ep3_6xB4x51 {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_TournamentMatchResult_GC_Ep3_6xB4x51) / 4, 0, 0x51, 0, 0, 0};
  ptext<char, 0x40> match_description;
  struct NamesEntry {
    ptext<char, 0x20> team_name;
    parray<ptext<char, 0x10>, 2> player_names;
  } __packed__;
  parray<NamesEntry, 2> names_entries;
  le_uint16_t unused1 = 0;
  // If round_num is equal to 6, the "On to the next battle..." text is replaced
  // with "Congratulations!" and some flashier graphics. This is used for the
  // final match.
  le_uint16_t round_num = 0;
  le_uint16_t num_players_per_team = 0;
  le_uint16_t winner_team_id = 0;
  le_uint32_t meseta_amount = 0;
  // This field apparently is supposed to contain a %s token (as for printf)
  // which is replaced with meseta_amount. The results screen animates this text
  // counting up from 0 to meseta_amount.
  ptext<char, 0x20> meseta_reward_text;
} __packed__;

// 6xB4x52: Set game metadata
// This is sent to all players in a game and all attached spectator teams when
// any player joins or leaves any spectator team watching the same game.

struct G_SetGameMetadata_GC_Ep3_6xB4x52 {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_SetGameMetadata_GC_Ep3_6xB4x52) / 4, 0, 0x52, 0, 0, 0};
  // This field appears before the slash in the spectators' HUD. Presumably this
  // is used to indicate how many spectators are in the current spectator team.
  // In the primary game (watched lobby), this is presumably unused.
  le_uint16_t local_spectators = 0; // Clamped to [0, 999] by the client
  // This field appears after the slash in the spectators' HUD. This is used to
  // indicate how many spectators there are in all spectator teams attached to
  // the same battle.
  // This field also controls the icon shown in the primary game. If this field
  // is nonzero, an icon appears in the middle of the screen during battle when
  // the details view is enabled (by pressing Z). However, the number of people
  // visible in the icon doesn't match the value in this field. Specifically:
  // 0 = no icon
  // 1 = icon with a single spectator (green)
  // 2-4 = icon with 3 spectators (blue)
  // 5-10 = icon with 5 spectators (yellow)
  // 11-29 = icon with 8 spectators (purple)
  // 30+ = icon with 12 spectators (red)
  le_uint16_t total_spectators = 0; // Clamped to [0, 999] by the client
  le_uint16_t unused = 0;
  le_uint16_t size = 0; // Number of used bytes in unknown_a2 (clamped to 0xFF)
  parray<uint8_t, 0x100> unknown_a2;
} __packed__;

// 6xB4x53: Reject battle start request
// This is sent in response to a CAx1D command if setup isn't complete (e.g. if
// some names/decks are missing or invalid). Under normal operation, this should
// never happen.
// Note: It seems the client ignores everything in this structure; the command
// handler just sets a global state flag and returns immediately.

struct G_RejectBattleStartRequest_GC_Ep3_6xB4x53 {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_RejectBattleStartRequest_GC_Ep3_6xB4x53) / 4, 0, 0x53, 0, 0, 0};
  Episode3::SetupPhase setup_phase;
  Episode3::RegistrationPhase registration_phase;
  parray<uint8_t, 2> unused;
  Episode3::MapAndRulesState state;
} __packed__;
