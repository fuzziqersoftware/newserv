#pragma once

#include <phosg/Encoding.hh>
#include <phosg/Strings.hh>
#include <stdexcept>
#include <string>

#include "Episode3/DataIndexes.hh"
#include "Episode3/DeckState.hh"
#include "Episode3/MapState.hh"
#include "Episode3/PlayerStateSubordinates.hh"
#include "Map.hh"
#include "PSOProtocol.hh"
#include "PlayerSubordinates.hh"
#include "SaveFileFormats.hh"
#include "Text.hh"

// This file is newserv's canonical reference of the PSO client/server protocol.

// For the unfamiliar, the le_uint and be_uint types (from phosg/Encoding.hh)
// are the same as normal uint types, but are explicitly little-endian or
// big-endian. The parray type (from Text.hh) is the same as a standard array,
// but has various safety and convenience features so we don't have to use
// easy-to-mess-up functions like memset/memcpy and strncpy. The pstring types
// (also from Text.hh) are like std::strings, but have an explicit encoding.
// They can be implicitly converted to and from std::strings, and will encode
// or decode their specified encoding when doing so. (The default encoding is
// UTF-8 everywhere in the server code.)

// Struct names are like [S|C|SC]_CommandName_[Versions]_Numbers
// S/C denotes who sends the command (S = server, C = client, SC = both)
// If versions are not specified, the format is the same for all versions.

// The version tokens are as follows:
// DCv1 = PSO Dreamcast v1
// DCv2 = PSO Dreamcast v2
// DC = Both DCv1 and DCv2
// PC = PSO PC (v2)
// GC = PSO GC Episodes 1&2 and/or Episode 3
// XB = PSO Xbox Episodes 1&2
// BB = PSO Blue Burst
// V3 = PSO GC and PSO Xbox (these versions are similar and share many formats)

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
// - - $E: Set text interpretation to English / use Roman font
// - - $J: Set text interpretation to Japanese / use Japanese font
// - - $B: Use Simplified Chinese font (PC/BB)
// - - $T: Use Traditional Chinese font (PC/BB)
// - - $K: Use Korean font (PC/BB)
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
// - - $CG: Orange pulse (FFE000 + darkenings thereof; v2 and later only)
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
  pstring<TextEncoding::ASCII, 0x40> copyright;
  le_uint32_t server_key = 0; // Key for commands sent by server
  le_uint32_t client_key = 0; // Key for commands sent by client
  // The client rejects the command if it's larger than this size, so we can't
  // add the after_message like we do in the other server init commands.
} __packed_ws__(S_ServerInit_Patch_02, 0x48);

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
  pstring<TextEncoding::ASCII, 0x10> username;
  pstring<TextEncoding::ASCII, 0x10> password;
  pstring<TextEncoding::ASCII, 0x40> email;
} __packed_ws__(C_Login_Patch_04, 0x6C);

// 05 (S->C): Disconnect
// No arguments
// This command is not used in the normal flow (described above). Generally the
// server should disconnect after sending a 12 or 15 command instead of an 05.

// 06 (S->C): Open file for writing

struct S_OpenFile_Patch_06 {
  le_uint32_t unused = 0;
  le_uint32_t size = 0;
  pstring<TextEncoding::ASCII, 0x30> filename;
} __packed_ws__(S_OpenFile_Patch_06, 0x38);

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
} __packed_ws__(S_WriteFileHeader_Patch_07, 0x0C);

// 08 (S->C): Close current file
// The unused field is optional. It's not clear whether this field was ever
// used; it could be a remnant from pre-release testing, or someone could have
// simply set the maximum size of this command incorrectly.

struct S_CloseCurrentFile_Patch_08 {
  le_uint32_t unused = 0;
} __packed_ws__(S_CloseCurrentFile_Patch_08, 4);

// 09 (S->C): Enter directory

struct S_EnterDirectory_Patch_09 {
  pstring<TextEncoding::ASCII, 0x40> name;
} __packed_ws__(S_EnterDirectory_Patch_09, 0x40);

// 0A (S->C): Exit directory
// No arguments

// 0B (S->C): Start patch session and go to patch root directory
// No arguments

// 0C (S->C): File checksum request

struct S_FileChecksumRequest_Patch_0C {
  le_uint32_t request_id = 0;
  pstring<TextEncoding::ASCII, 0x20> filename;
} __packed_ws__(S_FileChecksumRequest_Patch_0C, 0x24);

// 0D (S->C): End of file checksum requests
// No arguments

// 0E: Invalid command

// 0F (C->S): File information

struct C_FileInformation_Patch_0F {
  le_uint32_t request_id = 0; // Matches request_id from an earlier 0C command
  le_uint32_t checksum = 0; // CRC32 of the file's data
  le_uint32_t size = 0;
} __packed_ws__(C_FileInformation_Patch_0F, 0x0C);

// 10 (C->S): End of file information command list
// No arguments

// 11 (S->C): Start file downloads

struct S_StartFileDownloads_Patch_11 {
  le_uint32_t total_bytes = 0;
  le_uint32_t num_files = 0;
} __packed_ws__(S_StartFileDownloads_Patch_11, 0x08);

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
struct S_ReconnectT {
  be_uint32_t address = 0;
  PortT port = 0;
  le_uint16_t unused = 0;
} __packed__;
using S_Reconnect_Patch_14 = S_ReconnectT<be_uint16_t>;
check_struct_size(S_Reconnect_Patch_14, 0x08);

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
// On BB, this command may be sent as 0001 or 0101; in the latter case, the
// message box appears in the lower-left corner instead.

struct SC_TextHeader_01_06_11_B0_EE {
  le_uint32_t unused = 0;
  le_uint32_t guild_card_number = 0;
  // Text immediately follows here
} __packed_ws__(SC_TextHeader_01_06_11_B0_EE, 8);

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
  pstring<TextEncoding::ASCII, 0x40> copyright;
  le_uint32_t server_key = 0; // Key for data sent by server
  le_uint32_t client_key = 0; // Key for data sent by client
} __packed_ws__(S_ServerInitDefault_DC_PC_V3_02_17_91_9B, 0x48);

template <size_t AfterBytes>
struct S_ServerInitWithAfterMessageT_DC_PC_V3_02_17_91_9B {
  S_ServerInitDefault_DC_PC_V3_02_17_91_9B basic_cmd;
  // This field is not part of SEGA's implementation; the client ignores it.
  // newserv sends a message here disavowing the preceding copyright notice.
  pstring<TextEncoding::ASCII, AfterBytes> after_message;
} __packed__;

// 03 (C->S): Legacy register (non-BB)
// Internal name: SndRegist
// TODO: Are the DCv1 and DCv2 formats the same as this structure?

struct C_LegacyLogin_PC_V3_03 {
  /* 00 */ be_uint64_t hardware_id;
  /* 08 */ le_uint32_t sub_version = 0;
  /* 0C */ uint8_t is_extended = 0;
  /* 0D */ uint8_t language = 0;
  /* 0E */ le_uint16_t unknown_a2 = 0;
  // Note: These are suffixed with 2 since they come from the same source data
  // as the corresponding fields in 9D/9E. (Even though serial_number and
  // serial_number2 have the same contents in 9E, they do not come from the same
  // field on the client's connection context object.)
  /* 10 */ pstring<TextEncoding::ASCII, 0x10> serial_number2;
  /* 20 */ pstring<TextEncoding::ASCII, 0x10> access_key2;
  /* 30 */
} __packed_ws__(C_LegacyLogin_PC_V3_03, 0x30);

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
  pstring<TextEncoding::ASCII, 0x60> copyright;
  parray<uint8_t, 0x30> server_key;
  parray<uint8_t, 0x30> client_key;
} __packed_ws__(S_ServerInitDefault_BB_03_9B, 0xC0);

template <size_t AfterBytes>
struct S_ServerInitWithAfterMessageT_BB_03_9B {
  S_ServerInitDefault_BB_03_9B basic_cmd;
  // As in 02, this field is not part of SEGA's implementation.
  pstring<TextEncoding::ASCII, AfterBytes> after_message;
} __packed__;

// 04 (C->S): Legacy login
// Internal name: SndLogin2
// Curiously, there is a SndLogin3 function, but it does not send anything.
// See comments on non-BB 03 (S->C). This is likely a relic of an older,
// now-unused sequence. Like 03, this command isn't used by any known PSO
// version.
// header.flag is 1 if the client has UDP disabled.
// TODO: Are the DCv1 and DCv2 formats the same as this structure?

struct C_LegacyLogin_PC_V3_04 {
  /* 00 */ be_uint64_t hardware_id;
  /* 08 */ le_uint32_t sub_version = 0;
  /* 0C */ uint8_t is_extended = 0;
  /* 0D */ uint8_t language = 0;
  /* 0E */ le_uint16_t unknown_a2 = 0;
  /* 10 */ pstring<TextEncoding::ASCII, 0x10> serial_number;
  /* 20 */ pstring<TextEncoding::ASCII, 0x10> access_key;
  /* 30 */
} __packed_ws__(C_LegacyLogin_PC_V3_04, 0x30);

struct C_LegacyLogin_BB_04 {
  /* 00 */ le_uint32_t sub_version = 0;
  /* 04 */ uint8_t is_extended = 0;
  /* 05 */ uint8_t language = 0;
  /* 06 */ le_uint16_t unused = 0;
  /* 08 */ pstring<TextEncoding::ASCII, 0x10> username;
  /* 18 */ pstring<TextEncoding::ASCII, 0x10> password;
  /* 28 */
} __packed_ws__(C_LegacyLogin_BB_04, 0x28);

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
// be returned in a 9E or 9F command later.
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

struct S_UpdateClientConfig_DC_PC_04 {
  // Note: What we call player_tag here is actually three fields: two uint8_ts
  // followed by a le_uint16_t. It's unknown what the uint8_t fields are for
  // (they seem to always be zero), but the le_uint16_t is likely a boolean
  // which denotes whether the player is present or not (for example, in lobby
  // data structures). For historical and simplicity reasons, newserv combines
  // these three fields into one, which takes on the value 0x00010000 when a
  // player is present and zero when none is present.
  le_uint32_t player_tag = 0x00010000;
  le_uint32_t guild_card_number = 0;
} __packed_ws__(S_UpdateClientConfig_DC_PC_04, 8);

struct S_UpdateClientConfig_V3_04 {
  le_uint32_t player_tag = 0x00010000;
  le_uint32_t guild_card_number = 0;
  // This field is opaque to the client; it will send back the contents verbatim
  // in its next 9E command (or on request via 9F).
  parray<uint8_t, 0x20> client_config;
} __packed_ws__(S_UpdateClientConfig_V3_04, 0x28);

struct S_UpdateClientConfig_BB_04 {
  le_uint32_t player_tag = 0x00010000;
  le_uint32_t guild_card_number = 0;
  parray<uint8_t, 0x28> client_config;
} __packed_ws__(S_UpdateClientConfig_BB_04, 0x30);

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
// Server->client format is the same as the 01 command; guild_card_number
// is unused and set to zero. The maximum size of the message is 0x200 bytes.
// Client->server format is the same as the 01 command also.
// When sent by the client, the text field includes only the message. When sent
// by the server, the text field includes the origin player's name, followed by
// a tab character (\x09), followed by the message.
// During Episode 3 battles, the first byte of an inbound 06 command's message
// is interpreted differently. It should be treated as a bit field, with the low
// 4 bits intended as masks for who can see the message. If the low bit (1) is
// set, for example, then the chat message displays as " (whisper)" on player
// 0's screen regardless of the message contents. The next bit (2) hides the
// message from player 1, etc. The high 4 bits of this byte appear not to be
// used, but are often nonzero and set to the value 4. (This is probably done so
// that the field is always a valid ASCII character and also never terminates
// the chat string accidentally.) We call this byte private_flags in the places
// where newserv uses it; there is a similar byte in the 6xBD (private word
// select) command.

// 07 (S->C): Ship or block select menu
// Internal name: RcvDirList
// This command triggers a general form of blocking menu, which was used for
// both the ship select and block select menus by Sega (and all other private
// servers except newserv, it seems). Curiously, the string "RcvBlockList"
// appears in PSO v1 and v2, but it is not used, implying that at some point
// there was a separate command to send the block list, but it was scrapped.
// Perhaps this was used for command A1, which is identical to 07 and A0 in all
// versions of PSO (except DC NTE).

// This command sets the interaction mode, which affects which objects can be
// interacted with and what certain controls do. It also affects some in-game
// behaviors; for example, if the leader's interaction mode is set incorrectly
// in a game, the leader will not send the game state to joining players, so
// they will wait forever and not be able to actually join.

// The menu is titled "Ship Select" unless the first menu item begins with the
// text "BLOCK" (all caps), in which case it is titled "Block Select".

// Command is a list of these; header.flag is the entry count. The first entry
// is not included in the count and does not appear on the client. The first
// entry's text becomes the ship name displayed in the corner of the lobby HUD,
// unless it begins with "BLOCK", in which case it is ignored.
template <TextEncoding Encoding>
struct S_MenuItemT {
  le_uint32_t menu_id = 0;
  le_uint32_t item_id = 0;

  // The following two fields are only used for the game menu; for Ship Select,
  // Block Select, and Information, they are unused.
  // difficulty_tag is 0x0A on Episode 3; on all other versions, it's
  // difficulty + 0x22 (so 0x25 means Ultimate, for example).
  uint8_t difficulty_tag = 0;
  uint8_t num_players = 0;

  pstring<Encoding, 0x10> name;

  // The episode field is only used for the game menu; for Ship Select, Block
  // Select, and Information, it is unused.
  // The episode field is used differently by different versions:
  // - On DCv1, PC, and GC Episode 3, the value is ignored.
  // - On DCv2, 1 means v1 players can't join the game, and 0 means they can.
  // - On GC Ep1&2, 0x40 means Episode 1, and 0x41 means Episode 2.
  // - On BB, 0x40/0x41 mean Episodes 1/2 as on GC, and 0x43 means Episode 4.
  uint8_t episode = 0;
  // Flags (01 and 02 are used for all menus; the rest are only used for the
  // game menu):
  // 01 = Send name? (client sends the name field in the 10 command if this
  //      item is chosen, but it's blank)
  // 02 = Locked (lock icon appears in menu; player is prompted for password if
  //      they choose this game)
  // 04 = In battle (Episode 3; a sword icon appears in menu)
  // 04 = Disabled (BB; used for solo games)
  // 10 = Is battle mode
  // 20 = Is challenge mode
  // 40 = Is v2 only (DCv2/PC); game name renders in orange
  // 40 = Is Episode 1 (V3/BB)
  // 80 = Is Episode 2 (V3/BB)
  // C0 = Is Episode 4 (BB)
  uint8_t flags = 0;
} __packed__;
using S_MenuItem_PC_BB_08 = S_MenuItemT<TextEncoding::UTF16>;
using S_MenuItem_DC_V3_08_Ep3_E6 = S_MenuItemT<TextEncoding::MARKED>;
check_struct_size(S_MenuItem_PC_BB_08, 0x2C);
check_struct_size(S_MenuItem_DC_V3_08_Ep3_E6, 0x1C);

// 08 (C->S): Request game list
// Internal name: SndGameList
// No arguments

// 08 (S->C): Game list
// Internal name: RcvGameList
// Client responds with 09 and 10 commands (or nothing if the player cancels).
// Command format is the same as 07. Like 07, this command also sets the
// interaction mode, so it should not be used within a game.

// 09 (C->S): Menu item info request
// Internal name: SndInfo
// Server will respond with an 11 command, or an A3 or A5 if the specified menu
// is the quest menu.

struct C_MenuItemInfoRequest_09 {
  le_uint32_t menu_id = 0;
  le_uint32_t item_id = 0;
} __packed_ws__(C_MenuItemInfoRequest_09, 8);

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

template <TextEncoding Encoding>
struct SC_MeetUserExtensionT {
  struct LobbyReference {
    le_uint32_t menu_id = 0;
    le_uint32_t item_id = 0;
  } __packed_ws__(LobbyReference, 8);
  /* 00 */ parray<LobbyReference, 8> lobby_refs;
  /* 40 */ le_uint32_t unknown_a2 = 0;
  /* 44 */ pstring<Encoding, 0x20> player_name;
  /* 64 (or 84 on UTF16 versions) */
} __packed__;
using SC_MeetUserExtension_DC_V3 = SC_MeetUserExtensionT<TextEncoding::MARKED>;
using SC_MeetUserExtension_PC_BB = SC_MeetUserExtensionT<TextEncoding::UTF16>;
check_struct_size(SC_MeetUserExtension_DC_V3, 0x64);
check_struct_size(SC_MeetUserExtension_PC_BB, 0x84);

struct S_LegacyJoinGame_PC_0E {
  struct LobbyData {
    le_uint32_t player_tag = 0;
    le_uint32_t guild_card_number = 0;
    pstring<TextEncoding::ASCII, 0x10> name;
  } __packed_ws__(LobbyData, 0x18);

  parray<LobbyData, 4> lobby_data;
  parray<uint8_t, 0x20> unknown_a3;
} __packed_ws__(S_LegacyJoinGame_PC_0E, 0x80);

struct S_LegacyJoinGame_GC_0E {
  parray<PlayerLobbyDataDCGC, 4> lobby_data;
  SC_MeetUserExtension_DC_V3 meet_user_extension;
  parray<uint8_t, 4> unknown_a3;
} __packed_ws__(S_LegacyJoinGame_GC_0E, 0xE8);

struct S_LegacyJoinGame_XB_0E {
  struct LobbyData {
    le_uint32_t player_tag = 0;
    le_uint32_t guild_card_number = 0;
    pstring<TextEncoding::ASCII, 0x18> name;
  } __packed_ws__(LobbyData, 0x20);
  parray<LobbyData, 4> lobby_data;
  SC_MeetUserExtension_DC_V3 meet_user_extension;
  parray<uint8_t, 4> unknown_a3;
} __packed_ws__(S_LegacyJoinGame_XB_0E, 0xE8);

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
} __packed_ws__(C_MenuSelection_10_Flag00, 8);

template <TextEncoding Encoding>
struct C_MenuSelectionT_10_Flag01 {
  C_MenuSelection_10_Flag00 basic_cmd;
  pstring<Encoding, 0x10> unknown_a1;
} __packed__;
using C_MenuSelection_DC_V3_10_Flag01 = C_MenuSelectionT_10_Flag01<TextEncoding::MARKED>;
using C_MenuSelection_PC_BB_10_Flag01 = C_MenuSelectionT_10_Flag01<TextEncoding::UTF16>;
check_struct_size(C_MenuSelection_DC_V3_10_Flag01, 0x18);
check_struct_size(C_MenuSelection_PC_BB_10_Flag01, 0x28);

template <TextEncoding Encoding>
struct C_MenuSelectionT_10_Flag02 {
  C_MenuSelection_10_Flag00 basic_cmd;
  pstring<Encoding, 0x10> password;
} __packed__;
using C_MenuSelection_DC_V3_10_Flag02 = C_MenuSelectionT_10_Flag02<TextEncoding::MARKED>;
using C_MenuSelection_PC_BB_10_Flag02 = C_MenuSelectionT_10_Flag02<TextEncoding::UTF16>;
check_struct_size(C_MenuSelection_DC_V3_10_Flag02, 0x18);
check_struct_size(C_MenuSelection_PC_BB_10_Flag02, 0x28);

template <TextEncoding Encoding>
struct C_MenuSelectionT_10_Flag03 {
  C_MenuSelection_10_Flag00 basic_cmd;
  pstring<Encoding, 0x10> unknown_a1;
  pstring<Encoding, 0x10> password;
} __packed__;
using C_MenuSelection_DC_V3_10_Flag03 = C_MenuSelectionT_10_Flag03<TextEncoding::MARKED>;
using C_MenuSelection_PC_BB_10_Flag03 = C_MenuSelectionT_10_Flag03<TextEncoding::UTF16>;
check_struct_size(C_MenuSelection_DC_V3_10_Flag03, 0x28);
check_struct_size(C_MenuSelection_PC_BB_10_Flag03, 0x48);

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
// This command exists on DC NTE, but it does nothing. DC NTE does not have the
// 44 command, which would also be required for loading quests, so online
// quests canot be loaded on DC NTE.
// All chunks except the last must have 0x400 data bytes. When downloading an
// online quest, the .bin and .dat chunks may be interleaved. There is a client
// bug in BB (and probably all other versions) where if the quest file's size
// is a multiple of 0x400, the last chunk will have size 0x400, and the client
// will never consider the download complete since it only checks if the last
// chunk has size < 0x400; it does not check if all expected bytes have been
// received. To work around this, newserv appends an extra zero byte if the
// quest file's size is a multiple of 0x400; this byte will be ignored since
// the PRS decompression algorithm contains a stop command, so it will never
// read it.

// header.flag = file chunk index (start offset / 0x400)
struct S_WriteFile_13_A7 {
  pstring<TextEncoding::ASCII, 0x10> filename;
  parray<uint8_t, 0x400> data;
  le_uint32_t data_size = 0;
} __packed_ws__(S_WriteFile_13_A7, 0x414);

// 13 (C->S): Confirm file write (V3/BB)
// Client sends this in response to each 13 sent by the server. It appears
// these are only sent by V3 and BB - PSO DC and PC do not send these.

// header.flag = file chunk index (same as in the 13/A7 sent by the server)
struct C_WriteFileConfirmation_V3_BB_13_A7 {
  pstring<TextEncoding::ASCII, 0x10> filename;
} __packed_ws__(C_WriteFileConfirmation_V3_BB_13_A7, 0x10);

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

// 18 (S->C): Account verification result (PC/V3)
// Behaves exactly the same as 9A (S->C). No arguments except header.flag.

// 19 (S->C): Reconnect to different address
// Internal name: RcvPort
// Client will disconnect, and reconnect to the given address/port. Encryption
// will be disabled on the new connection; the server should send an appropriate
// command to enable it when the client connects.
// Note: PSO XB seems to ignore the address field, which makes sense given its
// networking architecture.

using S_Reconnect_19 = S_ReconnectT<le_uint16_t>;
check_struct_size(S_Reconnect_19, 8);

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
} __packed_ws__(S_ReconnectSplit_19, 0xAC);

// 1A (S->C): Large message box
// Internal name: RcvText
// On V3, client will sometimes respond with a D6 command (see D6 for more
// information).
// Contents are plain text. There must be at least one null character ('\0')
// before the end of the command data. There is a bug in V3 (and possibly all
// versions) where if this command is sent after the client has joined a lobby,
// the chat log window contents will appear in the message box, prepended to
// the message text from the command.
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

// 0022: GameGuard check (BB)

// Command 0022 is a 16-byte challenge (sent in the data field) using the
// following structure.

struct SC_GameGuardCheck_BB_0022 {
  parray<le_uint32_t, 4> data;
} __packed_ws__(SC_GameGuardCheck_BB_0022, 0x10);

// 0122 (C->S): Time deviation (BB)
// This command is sent when the client executes a quest opcode 5D (gettime) and
// the returned timestamp is before the previous timestamp returned, but not by
// too much - it seems the game only considers deltas between 3 seconds and 30
// minutes suspicious for these purposes.

// 23 (S->C): Momoka Item Exchange result (BB)
// Sent in response to a 6xD9 command from the client.
// header.flag indicates if an item was exchanged: 0 means success, 1 means
// failure.

// 24 (S->C): Secret Lottery Ticket exchange result (BB)
// Sent in response to a 6xDE command from the client.
// header.flag indicates whether the client had any Secret Lottery Tickets in
// their inventory (and hence could participate): 0 means success, 1 means
// failure.

struct S_ExchangeSecretLotteryTicketResult_BB_24 {
  // These fields map to unknown_a1 and unknown_a2 in the 6xDE command (but
  // their order is swapped here).
  le_uint16_t label = 0;
  uint8_t start_index = 0;
  uint8_t unused = 0;
  parray<le_uint32_t, 8> unknown_a3;
} __packed_ws__(S_ExchangeSecretLotteryTicketResult_BB_24, 0x24);

// 25 (S->C): Gallon's Plan result (BB)
// Sent in response to a 6xE1 command from the client.

struct S_GallonPlanResult_BB_25 {
  le_uint16_t label = 0;
  uint8_t offset1 = 0;
  uint8_t offset2 = 0;
  uint8_t value1 = 0;
  uint8_t value2 = 0;
  le_uint16_t unused = 0;
} __packed_ws__(S_GallonPlanResult_BB_25, 8);

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
} __packed_ws__(C_GuildCardSearch_40, 0x0C);

// 41 (S->C): Guild card search result
// Internal name: RcvUserAns

template <typename HeaderT, TextEncoding Encoding>
struct S_GuildCardSearchResultT {
  le_uint32_t player_tag = 0x00010000;
  le_uint32_t searcher_guild_card_number = 0;
  le_uint32_t result_guild_card_number = 0;
  HeaderT reconnect_command_header; // Ignored by the client
  S_Reconnect_19 reconnect_command;
  // The format of this string is "GAME-NAME,BLOCK##,SERVER-NAME". If the result
  // player is not in a game, GAME-NAME should be the lobby name - for standard
  // lobbies this is "BLOCK<blocknum>-<lobbynum>"; for CARD lobbies this is
  // "BLOCK<blocknum>-C<lobbynum>".
  pstring<Encoding, 0x44> location_string;
  // If the player chooses to meet the user, this extension data is sent in the
  // login command (9D/9E) after connecting to the server designated in
  // reconnect_command. When processing the 9D/9E, newserv uses only the
  // lobby_id field within, but it fills in all fields when sending a 41.
  SC_MeetUserExtensionT<Encoding> extension;
} __packed__;
using S_GuildCardSearchResult_PC_41 = S_GuildCardSearchResultT<PSOCommandHeaderPC, TextEncoding::UTF16>;
using S_GuildCardSearchResult_DC_V3_41 = S_GuildCardSearchResultT<PSOCommandHeaderDCV3, TextEncoding::MARKED>;
using S_GuildCardSearchResult_BB_41 = S_GuildCardSearchResultT<PSOCommandHeaderBB, TextEncoding::UTF16>;
check_struct_size(S_GuildCardSearchResult_PC_41, 0x124);
check_struct_size(S_GuildCardSearchResult_DC_V3_41, 0xC0);
check_struct_size(S_GuildCardSearchResult_BB_41, 0x128);

// 42: Invalid command
// 43: Invalid command

// 44 (S->C): Open file for download
// Internal name: RcvDownLoadHead
// Used for downloading online quests. The client will react to a 44 command if
// the filename ends in .bin or .dat.
// This command is not implemented on DC NTE, so DC NTE cannot receive online
// quest files.
// For download quests (to be saved to the memory card) and GBA games, the A6
// command is used instead. The client will react to A6 if the filename ends in
// .bin/.dat (quests), .pvr (textures), or .gba (GameBoy Advance games).
// It appears that the .gba handler for A6 was not deleted in PSO XB, even
// though it doesn't make sense for an XB client to receive such a file.

struct S_OpenFile_DC_44_A6 {
  pstring<TextEncoding::MARKED, 0x22> name; // Should begin with "PSO/"
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
  pstring<TextEncoding::ASCII, 0x11> filename;
  le_uint32_t file_size = 0;
} __packed_ws__(S_OpenFile_DC_44_A6, 0x38);

struct S_OpenFile_PC_GC_44_A6 {
  pstring<TextEncoding::MARKED, 0x22> name; // Should begin with "PSO/"
  le_uint16_t type = 0;
  pstring<TextEncoding::ASCII, 0x10> filename;
  le_uint32_t file_size = 0;
} __packed_ws__(S_OpenFile_PC_GC_44_A6, 0x38);

// Curiously, PSO XB expects an extra 0x18 bytes at the end of this command, but
// those extra bytes are unused, and the client does not fail if they're
// omitted.
struct S_OpenFile_XB_44_A6 : S_OpenFile_PC_GC_44_A6 {
  pstring<TextEncoding::ASCII, 0x10> xb_filename;
  le_uint32_t content_meta;
  parray<uint8_t, 4> unused2;
} __packed_ws__(S_OpenFile_XB_44_A6, 0x50);

struct S_OpenFile_BB_44_A6 {
  parray<uint8_t, 0x22> unused;
  le_uint16_t type = 0;
  pstring<TextEncoding::ASCII, 0x10> filename;
  le_uint32_t file_size = 0;
  pstring<TextEncoding::MARKED, 0x18> name;
} __packed_ws__(S_OpenFile_BB_44_A6, 0x50);

// 44 (C->S): Confirm open file (V3/BB)
// Client sends this in response to each 44 sent by the server.

// header.flag = quest number (sort of - seems like the client just echoes
// whatever the server sent in its header.flag field. Also quest numbers can be
// > 0xFF so the flag is essentially meaningless)
struct C_OpenFileConfirmation_44_A6 {
  pstring<TextEncoding::ASCII, 0x10> filename;
} __packed_ws__(C_OpenFileConfirmation_44_A6, 0x10);

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

struct PlayerRecordsEntry_DC {
  /* 00 */ le_uint32_t client_id = 0;
  /* 04 */ PlayerRecordsChallengeDC challenge;
  /* A4 */ PlayerRecordsBattle battle;
  /* BC */
} __packed_ws__(PlayerRecordsEntry_DC, 0xBC);

struct PlayerRecordsEntry_PC {
  /* 00 */ le_uint32_t client_id = 0;
  /* 04 */ PlayerRecordsChallengePC challenge;
  /* DC */ PlayerRecordsBattle battle;
  /* F4 */
} __packed_ws__(PlayerRecordsEntry_PC, 0xF4);

struct PlayerRecordsEntry_V3 {
  /* 0000 */ le_uint32_t client_id = 0;
  /* 0004 */ PlayerRecordsChallengeV3 challenge;
  /* 0104 */ PlayerRecordsBattle battle;
  /* 011C */
} __packed_ws__(PlayerRecordsEntry_V3, 0x011C);

struct PlayerRecordsEntry_BB {
  /* 0000 */ le_uint32_t client_id = 0;
  /* 0004 */ PlayerRecordsChallengeBB challenge;
  /* 0144 */ PlayerRecordsBattle battle;
  /* 015C */
} __packed_ws__(PlayerRecordsEntry_BB, 0x015C);

struct C_CharacterData_DCv1_61_98 {
  /* 0000 */ PlayerInventory inventory;
  /* 034C */ PlayerDispDataDCPCV3 disp;
  /* 041C */
} __packed_ws__(C_CharacterData_DCv1_61_98, 0x041C);

struct C_CharacterData_DCv2_61_98 {
  /* 0000 */ PlayerInventory inventory;
  /* 034C */ PlayerDispDataDCPCV3 disp;
  /* 041C */ PlayerRecordsEntry_DC records;
  /* 04D8 */ ChoiceSearchConfig choice_search_config;
  /* 04F0 */
} __packed_ws__(C_CharacterData_DCv2_61_98, 0x04F0);

struct C_CharacterData_PC_61_98 {
  /* 0000 */ PlayerInventory inventory;
  /* 034C */ PlayerDispDataDCPCV3 disp;
  /* 041C */ PlayerRecordsEntry_PC records;
  /* 0510 */ ChoiceSearchConfig choice_search_config;
  /* 0528 */ parray<le_uint32_t, 0x1E> blocked_senders;
  /* 05A0 */ le_uint32_t auto_reply_enabled = 0;
  // The auto-reply message can be up to 0x200 characters. If it's shorter than
  // that, the client truncates the command after the first null value (rounded
  // up to the next 4-byte boundary).
  /* 05A4 */ // uint16_t auto_reply[...EOF];
} __packed_ws__(C_CharacterData_PC_61_98, 0x5A4);

struct C_CharacterData_GCNTE_61_98 {
  /* 0000 */ PlayerInventory inventory;
  /* 034C */ PlayerDispDataDCPCV3 disp;
  /* 041C */ PlayerRecordsEntry_DC records;
  /* 04D8 */ ChoiceSearchConfig choice_search_config;
  /* 04F0 */ parray<le_uint32_t, 0x1E> blocked_senders;
  /* 0568 */ le_uint32_t auto_reply_enabled = 0;
  // The auto-reply message can be up to 0x200 bytes. If it's shorter than that,
  // the client truncates the command after the first zero byte (rounded up to
  // the next 4-byte boundary).
  /* 056C */ // char auto_reply[...EOF];
} __packed_ws__(C_CharacterData_GCNTE_61_98, 0x56C);

struct C_CharacterData_V3_61_98 {
  /* 0000 */ PlayerInventory inventory;
  /* 034C */ PlayerDispDataDCPCV3 disp;
  /* 041C */ PlayerRecordsEntry_V3 records;
  /* 0538 */ ChoiceSearchConfig choice_search_config;
  /* 0550 */ pstring<TextEncoding::MARKED, 0xAC> info_board;
  /* 05FC */ parray<le_uint32_t, 0x1E> blocked_senders;
  /* 0674 */ le_uint32_t auto_reply_enabled = 0;
  // The auto-reply message can be up to 0x200 bytes. If it's shorter than that,
  // the client truncates the command after the first zero byte (rounded up to
  // the next 4-byte boundary).
  /* 0678 */ // char auto_reply[...EOF];
} __packed_ws__(C_CharacterData_V3_61_98, 0x678);

struct C_CharacterData_Ep3_61_98 {
  /* 0000 */ PlayerInventory inventory;
  /* 034C */ PlayerDispDataDCPCV3 disp;
  /* 041C */ PlayerRecordsEntry_V3 records;
  /* 0538 */ ChoiceSearchConfig choice_search_config;
  /* 0550 */ pstring<TextEncoding::MARKED, 0xAC> info_board;
  /* 05FC */ parray<le_uint32_t, 0x1E> blocked_senders;
  /* 0674 */ le_uint32_t auto_reply_enabled = 0;
  /* 0678 */ pstring<TextEncoding::MARKED, 0xAC> auto_reply;
  /* 0724 */ Episode3::PlayerConfig ep3_config;
  /* 2A74 */
} __packed_ws__(C_CharacterData_Ep3_61_98, 0x2A74);

struct C_CharacterData_BB_61_98 {
  /* 0000 */ PlayerInventory inventory;
  /* 034C */ PlayerDispDataBB disp;
  /* 04DC */ PlayerRecordsEntry_BB records;
  /* 0638 */ ChoiceSearchConfig choice_search_config;
  /* 0650 */ pstring<TextEncoding::UTF16, 0xAC> info_board;
  /* 07A8 */ parray<le_uint32_t, 0x1E> blocked_senders;
  /* 0820 */ le_uint32_t auto_reply_enabled = 0;
  // Like on V3, the client truncates the command if the auto reply message is
  // shorter than 0x200 bytes.
  /* 0824 */ // uint16_t auto_reply[...EOF];
} __packed_ws__(C_CharacterData_BB_61_98, 0x824);

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
template <typename LobbyDataT>
struct S_JoinGameT_DC_PC {
  // Note: It seems Sega servers sent uninitialized memory in the variations
  // field when sending this command to start an Episode 3 tournament game. This
  // can be misleading when reading old logs from those days, but the Episode 3
  // client really does ignore it.
  /* 0004 */ Variations variations;
  // Unlike lobby join commands, these are filled in in their slot positions.
  // That is, if there's only one player in a game with ID 2, then the first two
  // of these are blank and the player's data is in the third entry here.
  /* 0084 */ parray<LobbyDataT, 4> lobby_data;
  /* 0104 */ uint8_t client_id = 0;
  /* 0105 */ uint8_t leader_id = 0;
  /* 0106 */ uint8_t disable_udp = 1;
  /* 0107 */ uint8_t difficulty = 0;
  /* 0108 */ uint8_t battle_mode = 0;
  /* 0109 */ uint8_t event = 0;
  /* 010A */ uint8_t section_id = 0;
  /* 010B */ uint8_t challenge_mode = 0;
  /* 010C */ le_uint32_t random_seed = 0;
  /* 0110 */
} __packed__;

struct S_JoinGame_DCNTE_64 {
  uint8_t client_id = 0;
  uint8_t leader_id = 0;
  uint8_t disable_udp = 1;
  uint8_t unused = 0;
  Variations variations;
  parray<PlayerLobbyDataDCGC, 4> lobby_data;
} __packed_ws__(S_JoinGame_DCNTE_64, 0x104);
using S_JoinGame_DC_64 = S_JoinGameT_DC_PC<PlayerLobbyDataDCGC>;
using S_JoinGame_PC_64 = S_JoinGameT_DC_PC<PlayerLobbyDataPC>;
check_struct_size(S_JoinGame_DC_64, 0x10C);
check_struct_size(S_JoinGame_PC_64, 0x14C);

struct S_JoinGame_GC_64 : S_JoinGameT_DC_PC<PlayerLobbyDataDCGC> {
  uint8_t episode = 0;
  parray<uint8_t, 3> unused;
} __packed_ws__(S_JoinGame_GC_64, 0x110);

struct S_JoinGame_Ep3_64 : S_JoinGame_GC_64 {
  // This field is only present if the game (and client) is Episode 3. Similarly
  // to lobby_data in the base struct, all four of these are always present and
  // they are filled in in slot positions.
  struct Ep3PlayerEntry {
    PlayerInventory inventory;
    PlayerDispDataDCPCV3 disp;
  } __packed_ws__(Ep3PlayerEntry, 0x41C);
  parray<Ep3PlayerEntry, 4> players_ep3;
} __packed_ws__(S_JoinGame_Ep3_64, 0x1180);

struct S_JoinGame_XB_64 : S_JoinGameT_DC_PC<PlayerLobbyDataXB> {
  uint8_t episode = 0;
  parray<uint8_t, 3> unused;
  parray<le_uint32_t, 6> unknown_a1;
} __packed_ws__(S_JoinGame_XB_64, 0x1D8);

struct S_JoinGame_BB_64 : S_JoinGameT_DC_PC<PlayerLobbyDataBB> {
  uint8_t episode = 0;
  uint8_t unused1 = 1;
  uint8_t solo_mode = 0;
  uint8_t unused2 = 0;
} __packed_ws__(S_JoinGame_BB_64, 0x1A0);

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
} __packed_ws__(LobbyFlags_DCNTE, 4);

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
} __packed_ws__(LobbyFlags, 0x0C);

// Header flag = entry count (always 1 for 65 and 68; up to 0x0C for 67)
template <typename LobbyFlagsT, typename LobbyDataT, typename DispDataT>
struct S_JoinLobbyT {
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
    return offsetof(S_JoinLobbyT, entries) + used_entries * sizeof(Entry);
  }
} __packed__;
using S_JoinLobby_DCNTE_65_67_68 = S_JoinLobbyT<LobbyFlags_DCNTE, PlayerLobbyDataDCGC, PlayerDispDataDCPCV3>;
using S_JoinLobby_PC_65_67_68 = S_JoinLobbyT<LobbyFlags, PlayerLobbyDataPC, PlayerDispDataDCPCV3>;
using S_JoinLobby_DC_GC_65_67_68_Ep3_EB = S_JoinLobbyT<LobbyFlags, PlayerLobbyDataDCGC, PlayerDispDataDCPCV3>;
using S_JoinLobby_BB_65_67_68 = S_JoinLobbyT<LobbyFlags, PlayerLobbyDataBB, PlayerDispDataBB>;
check_struct_size(S_JoinLobby_DCNTE_65_67_68, 0x32D4);
check_struct_size(S_JoinLobby_PC_65_67_68, 0x339C);
check_struct_size(S_JoinLobby_DC_GC_65_67_68_Ep3_EB, 0x32DC);
check_struct_size(S_JoinLobby_BB_65_67_68, 0x3D8C);

struct S_JoinLobby_XB_65_67_68 {
  LobbyFlags lobby_flags;
  parray<le_uint32_t, 6> unknown_a4;
  struct Entry {
    PlayerLobbyDataXB lobby_data;
    PlayerInventory inventory;
    PlayerDispDataDCPCV3 disp;
  } __packed_ws__(Entry, 0x468);
  // Note: not all of these will be filled in and sent if the lobby isn't full
  // (the command size will be shorter than this struct's size)
  parray<Entry, 12> entries;

  static inline size_t size(size_t used_entries) {
    return offsetof(S_JoinLobby_XB_65_67_68, entries) + used_entries * sizeof(Entry);
  }
} __packed_ws__(S_JoinLobby_XB_65_67_68, 0x3504);

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
} __packed_ws__(S_LeaveLobby_66_69_Ep3_E9, 4);

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

// 6F (C->S): Done loading
// Internal name: SndBurstEnd
// This command is sent when a player is done loading and other players can then
// join the game. On BB, this command is sent a 006F after loading into a game,
// or as 016F after loading a joinable quest. (This means when a BB client joins
// a game with a quest in progress, they will send 006F when they're ready to
// receive the quest files, and 016F when they're actually ready to play.)

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

// 80: Valid but ignored (all versions except BB)
// Internal names: RcvGenerateID and SndGenerateID
// This command appears to be used to set the next item ID for the given player
// slot. PSO V3 and later accept this command, but ignore it entirely. Notably,
// no version of PSO except for DC NTE ever sends this command - it's likely it
// was used to implement some item ID sync semantics that were later changed to
// use the leader as the source of truth.

struct C_GenerateID_DCNTE_80 {
  le_uint32_t id = 0;
  uint8_t unused1 = 0; // Always 0
  uint8_t unused2 = 0; // Always 0
  le_uint16_t unused3 = 0; // Always 0
  parray<uint8_t, 4> unused4; // Client sends uninitialized data here
} __packed_ws__(C_GenerateID_DCNTE_80, 0x0C);

struct S_GenerateID_DC_PC_V3_80 {
  le_uint32_t client_id = 0;
  le_uint32_t unused = 0;
  le_uint32_t next_item_id = 0;
} __packed_ws__(S_GenerateID_DC_PC_V3_80, 0x0C);

// 81: Simple mail
// Internal name: RcvChatMessage and SndChatMessage
// Format is the same in both directions. The server should forward the command
// to the player with to_guild_card_number, if they are online. If they are not
// online, the server may store it for later delivery, send their auto-reply
// message back to the original sender, or simply drop the message.
// On GC (and probably other versions too) the unused space after the text
// contains uninitialized memory when the client sends this command. newserv
// clears the uninitialized data for security reasons before forwarding.
// The maximum length of the message is 170 characters, despite the field being
// able to hold triple that amount.

struct SC_SimpleMail_PC_81 {
  // If player_tag and from_guild_card_number are zero, the message cannot be
  // replied to.
  le_uint32_t player_tag = 0x00010000;
  le_uint32_t from_guild_card_number = 0;
  pstring<TextEncoding::UTF16, 0x0F> from_name;
  le_uint16_t from_name_term = 0;
  le_uint32_t to_guild_card_number = 0;
  pstring<TextEncoding::UTF16, 0x1FF> text;
  le_uint16_t text_term = 0;
} __packed_ws__(SC_SimpleMail_PC_81, 0x42C);

struct SC_SimpleMail_DC_V3_81 {
  le_uint32_t player_tag = 0x00010000;
  le_uint32_t from_guild_card_number = 0;
  pstring<TextEncoding::MARKED, 0x0F> from_name;
  uint8_t from_name_term = 0;
  le_uint32_t to_guild_card_number = 0;
  pstring<TextEncoding::MARKED, 0x1FF> text;
  uint8_t text_term = 0;
} __packed_ws__(SC_SimpleMail_DC_V3_81, 0x21C);

struct SC_SimpleMail_BB_81 {
  le_uint32_t player_tag = 0x00010000;
  le_uint32_t from_guild_card_number = 0;
  pstring<TextEncoding::UTF16_ALWAYS_MARKED, 0x0F> from_name;
  le_uint16_t from_name_term = 0;
  le_uint32_t to_guild_card_number = 0;
  pstring<TextEncoding::UTF16, 0x13> received_date;
  le_uint16_t received_date_term = 0;
  pstring<TextEncoding::UTF16, 0x1FF> text;
  le_uint16_t text_term = 0;
} __packed_ws__(SC_SimpleMail_BB_81, 0x454);

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
} __packed_ws__(S_LobbyListEntry_83, 0x0C);

// 84 (C->S): Choose lobby
// Internal name: SndRoomChange

struct C_LobbySelection_84 {
  le_uint32_t menu_id = 0;
  le_uint32_t item_id = 0;
} __packed_ws__(C_LobbySelection_84, 8);

// 85: Invalid command
// 86: Invalid command
// 87: Invalid command

// 88 (C->S): Account check (DC NTE only)
// The server should respond with an 88 command.

struct C_Login_DCNTE_88 {
  pstring<TextEncoding::ASCII, 0x11> serial_number;
  pstring<TextEncoding::ASCII, 0x11> access_key;
} __packed_ws__(C_Login_DCNTE_88, 0x22);

// 88 (S->C): Account check result (DC NTE only)
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
} __packed_ws__(S_ArrowUpdateEntry_88, 0x0C);

// 89 (C->S): Set lobby arrow
// header.flag = arrow color number (see above); no other arguments.
// Server should send an 88 command to all players in the lobby.

// 89 (S->C): Start encryption at login server (DC NTE)
// Behaves exactly the same as the 17 command.

// 8A (C->S): Connection information (DC NTE only)
// The server should respond with an 8A command.

struct C_ConnectionInfo_DCNTE_8A {
  be_uint64_t hardware_id;
  le_uint32_t sub_version = 0x20;
  le_uint32_t unknown_a1 = 0;
  pstring<TextEncoding::ASCII, 0x30> username;
  pstring<TextEncoding::ASCII, 0x30> password;
  pstring<TextEncoding::ASCII, 0x30> email_address; // From Sylverant documentation
} __packed_ws__(C_ConnectionInfo_DCNTE_8A, 0xA0);

// 8A (S->C): Connection information result (DC NTE only)
// header.flag is a success flag. If 0 is sent, the client shows an error
// message and disconnects. Otherwise, the client responds with an 8B command.

// 8A (C->S): Request lobby/game name (except DC NTE)
// No arguments

// 8A (S->C): Lobby/game name (except DC NTE)
// Contents is a string containing the lobby or game name. All versions after
// DCv1 (including the August 2001 DCv2 prototype) send an 8A command to request
// the team name after joining a game. The response is used to handle the
// team_name token in quest strings, and appears in some Challenge Mode
// information windows.
// Even though this was only ever used to retrieve the game name, Sega's
// original servers also replied to 8A if it was sent in a lobby. They would
// return a string like "LOBBY01" in that case.

// 8B: Log in (DC NTE only)

struct C_Login_DCNTE_8B {
  le_uint32_t player_tag = 0x00010000;
  le_uint32_t guild_card_number = 0;
  be_uint64_t hardware_id;
  le_uint32_t sub_version = 0x20;
  uint8_t is_extended = 0;
  uint8_t language = 0;
  parray<uint8_t, 2> unused1;
  pstring<TextEncoding::ASCII, 0x11> serial_number;
  pstring<TextEncoding::ASCII, 0x11> access_key;
  pstring<TextEncoding::ASCII, 0x30> username;
  pstring<TextEncoding::ASCII, 0x30> password;
  pstring<TextEncoding::ASCII, 0x10> name;
  parray<uint8_t, 2> unused;
} __packed_ws__(C_Login_DCNTE_8B, 0xAC);

struct C_LoginExtended_DCNTE_8B : C_Login_DCNTE_8B {
  SC_MeetUserExtension_DC_V3 extension;
} __packed_ws__(C_LoginExtended_DCNTE_8B, 0x110);

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
  pstring<TextEncoding::ASCII, 0x11> serial_number;
  pstring<TextEncoding::ASCII, 0x11> access_key;
  // Note: There is a bug in the Japanese and prototype versions of DCv1 that
  // cause the client to send this command despite its size not being a
  // multiple of 4. This is fixed in later versions, so we handle both cases in
  // the receive handler.
} __packed_ws__(C_LoginV1_DC_PC_V3_90, 0x22);

// 90 (S->C): Account verification result (V3)
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
  be_uint64_t hardware_id;
  le_uint32_t sub_version;
  uint8_t is_extended = 0; // TODO: This is a guess
  uint8_t language = 0; // TODO: This is a guess; verify it
  parray<uint8_t, 2> unknown_a3;
  pstring<TextEncoding::ASCII, 0x30> serial_number2;
  pstring<TextEncoding::ASCII, 0x30> access_key2;
  pstring<TextEncoding::ASCII, 0x30> email; // According to Sylverant documentation
} __packed_ws__(C_RegisterV1_DC_92, 0xA0);

// 92 (S->C): Register result (non-BB)
// Internal name: RcvPsoRegist
// Same format and usage as 9C (S->C) command.

// 93 (C->S): Log in (DCv1)

struct C_LoginV1_DC_93 {
  /* 00 */ le_uint32_t player_tag = 0x00010000;
  /* 04 */ le_uint32_t guild_card_number = 0;
  /* 08 */ be_uint64_t hardware_id;
  /* 10 */ le_uint32_t sub_version = 0;
  /* 14 */ uint8_t is_extended = 0;
  /* 15 */ uint8_t language = 0;
  /* 16 */ parray<uint8_t, 2> unused1;
  /* 18 */ pstring<TextEncoding::ASCII, 0x11> serial_number;
  /* 29 */ pstring<TextEncoding::ASCII, 0x11> access_key;
  /* 3A */ pstring<TextEncoding::ASCII, 0x30> serial_number2;
  /* 6A */ pstring<TextEncoding::ASCII, 0x30> access_key2;
  /* 9A */ pstring<TextEncoding::ASCII, 0x10> name;
  /* AA */ parray<uint8_t, 2> unused2;
  /* AC */
} __packed_ws__(C_LoginV1_DC_93, 0xAC);

struct C_LoginExtendedV1_DC_93 : C_LoginV1_DC_93 {
  SC_MeetUserExtension_DC_V3 extension;
} __packed_ws__(C_LoginExtendedV1_DC_93, 0x110);

// 93 (C->S): Log in (BB)

struct C_LoginBase_BB_93 {
  le_uint32_t player_tag = 0x00010000;
  le_uint32_t guild_card_number = 0;
  le_uint32_t sub_version = 0;
  uint8_t language = 0;
  int8_t character_slot = 0;
  // Values for connection_phase:
  // 00 - initial connection (client will request system file, characters, etc.)
  // 01 - choose character
  // 02 - create character
  // 03 - apply updates from dressing room
  // 04 - login server
  // 05 - lobby server
  // 06 - lobby server (with Meet User fields specified)
  uint8_t connection_phase = 0;
  uint8_t client_code = 0;
  le_uint32_t security_token = 0;
  pstring<TextEncoding::ASCII, 0x30> username;
  pstring<TextEncoding::ASCII, 0x30> password;

  // These fields map to the same fields in SC_MeetUserExtensionT. There is no
  // equivalent of the name field from that structure on BB (though newserv
  // doesn't use it anyway).
  le_uint32_t menu_id = 0;
  le_uint32_t preferred_lobby_id = 0;
} __packed_ws__(C_LoginBase_BB_93, 0x7C);

struct C_LoginWithoutHardwareInfo_BB_93 : C_LoginBase_BB_93 {
  // Note: Unlike other versions, BB puts the version string in the client
  // config at connect time. So the first time the server gets this command, it
  // will be something like "Ver. 1.24.3". This format is used on older client
  // versions (before 1.23.8?)
  parray<uint8_t, 0x28> client_config;
} __packed_ws__(C_LoginWithoutHardwareInfo_BB_93, 0xA4);

struct C_LoginWithHardwareInfo_BB_93 : C_LoginBase_BB_93 {
  // See the comment in the above structure. This format is used on newer client
  // versions.
  parray<le_uint32_t, 2> hardware_info;
  parray<uint8_t, 0x28> client_config;
} __packed_ws__(C_LoginWithHardwareInfo_BB_93, 0xAC);

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
} __packed_ws__(C_CharSaveInfo_DCv2_PC_V3_BB_96, 8);

// 97 (S->C): Save to memory card
// Internal name: RcvSaveCountCheck
// No arguments
// Internally, this command is called RcvSaveCountCheck, even though the counter
// in the 96 command (to which 97 is a reply) counts more events than saves.
// Sending this command with header.flag == 0 will show a message saying that
// "character data was improperly saved", and will delete the character's items
// and challenge mode records. newserv (and all other unofficial servers) always
// send this command with flag == 1, which causes the client to save normally.
// If a PSO PC client receives this command multiple times during a session, the
// player will see the "character data may be damaged" message and be asked if
// they want to restore the pre-session backup data.
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
  pstring<TextEncoding::ASCII, 0x10> v1_serial_number;
  pstring<TextEncoding::ASCII, 0x10> v1_access_key;
  pstring<TextEncoding::ASCII, 0x10> serial_number;
  pstring<TextEncoding::ASCII, 0x10> access_key;
  le_uint32_t player_tag = 0x00010000;
  le_uint32_t guild_card_number = 0;
  le_uint32_t sub_version = 0;
  pstring<TextEncoding::ASCII, 0x30> serial_number2; // On DCv2, this is the hardware ID
  pstring<TextEncoding::ASCII, 0x30> access_key2;
  pstring<TextEncoding::ASCII, 0x30> email_address;
} __packed_ws__(C_Login_DC_PC_V3_9A, 0xDC);

// 9A (S->C): Account verification result
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
  /* 00 */ be_uint64_t hardware_id;
  /* 08 */ le_uint32_t sub_version = 0;
  /* 0C */ uint8_t unused1 = 0;
  /* 0D */ uint8_t language = 0;
  /* 0E */ parray<uint8_t, 2> unused2;
  /* 10 */ pstring<TextEncoding::ASCII, 0x30> serial_number; // On XB, this is the XBL gamertag
  /* 40 */ pstring<TextEncoding::ASCII, 0x30> access_key; // On XB, this is the XBL user ID
  /* 70 */ pstring<TextEncoding::ASCII, 0x30> password; // On XB, this contains "xbox-pso"
  /* A0 */
} __packed_ws__(C_Register_DC_PC_V3_9C, 0xA0);

struct C_Register_BB_9C {
  le_uint32_t sub_version = 0;
  uint8_t unused1 = 0;
  uint8_t language = 0;
  parray<uint8_t, 2> unused2;
  pstring<TextEncoding::ASCII, 0x30> username;
  pstring<TextEncoding::ASCII, 0x30> password;
  pstring<TextEncoding::ASCII, 0x30> game_tag; // "psopc2" on BB
} __packed_ws__(C_Register_BB_9C, 0x98);

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
  /* 00 */ le_uint32_t player_tag = 0x00010000; // 0x00010000 if guild card is set (via 04)
  /* 04 */ le_uint32_t guild_card_number = 0; // 0xFFFFFFFF if not set
  // The hardware ID is different for various PSO versions:
  // - All DC versions: the hardware ID comes from the FUNC_SYSINFO_ID syscall
  //   (as KallistiOS refers to it), which returns a 64-bit integer. PSO uses
  //   the low 48 bits; the high 16 bits are masked out and are always zero.
  // - PC V2: the hardware ID is always 0000FFFFFFFFFFFF
  // - GC NTE: the last byte of the hardware ID is uninitialized memory from
  //   the TProtocol constructor's stack; the other bytes are all zeroes.
  // - V3: the hardware ID is all zeroes.
  // On the client, this is actually an array of 8 bytes, but we treat it as a
  // single integer for simplicity.
  /* 08 */ be_uint64_t hardware_id;
  /* 10 */ le_uint32_t sub_version = 0;
  /* 14 */ uint8_t is_extended = 0; // If 1, structure has extended format
  /* 15 */ uint8_t language = 0; // 0 = JP, 1 = EN, 2 = DE, 3 = FR, 4 = ES
  /* 16 */ parray<uint8_t, 0x2> unused3; // Always zeroes
  /* 18 */ pstring<TextEncoding::ASCII, 0x10> v1_serial_number;
  /* 28 */ pstring<TextEncoding::ASCII, 0x10> v1_access_key;
  /* 38 */ pstring<TextEncoding::ASCII, 0x10> serial_number; // On XB, this is the XBL gamertag
  /* 48 */ pstring<TextEncoding::ASCII, 0x10> access_key; // On XB, this is the XBL user ID
  /* 58 */ pstring<TextEncoding::ASCII, 0x30> serial_number2; // On DCv2, this is the hardware ID; on XB, this is the XBL gamertag
  /* 88 */ pstring<TextEncoding::ASCII, 0x30> access_key2; // On XB, this is the XBL user ID
  /* B8 */ pstring<TextEncoding::ASCII, 0x10> name;
  /* C8 */
} __packed_ws__(C_Login_DC_PC_GC_9D, 0xC8);

struct C_LoginExtended_DC_GC_9D : C_Login_DC_PC_GC_9D {
  SC_MeetUserExtension_DC_V3 extension;
} __packed_ws__(C_LoginExtended_DC_GC_9D, 0x12C);

struct C_LoginExtended_PC_9D : C_Login_DC_PC_GC_9D {
  SC_MeetUserExtension_PC_BB extension;
} __packed_ws__(C_LoginExtended_PC_9D, 0x14C);

// 9E (C->S): Log in with client config (V3/BB)
// Not used on GC Episodes 1&2 Trial Edition.
// The extended version of this command is used in the same circumstances as
// when PSO PC uses the extended version of the 9D command.
// PSO XB does not send the client config (security data) in the 9E command,
// even though there is a space for it. The server must use 9F instead to
// retrieve the client config.
// header.flag is 1 if the client has UDP disabled.

struct C_Login_GC_9E : C_Login_DC_PC_GC_9D {
  parray<uint8_t, 0x20> client_config;
} __packed_ws__(C_Login_GC_9E, 0xE8);

struct C_LoginExtended_GC_9E : C_Login_GC_9E {
  SC_MeetUserExtension_DC_V3 extension;
} __packed_ws__(C_LoginExtended_GC_9E, 0x14C);

struct C_Login_XB_9E : C_Login_DC_PC_GC_9D {
  /* 00C8 */ parray<uint8_t, 0x20> unused;
  /* 00E8 */ XBNetworkLocation netloc;
  /* 0118 */ parray<le_uint32_t, 3> unknown_a1a;
  /* 0124 */ le_uint32_t xb_user_id_high = 0;
  /* 0128 */ le_uint32_t xb_user_id_low = 0;
  /* 012C */ le_uint32_t unknown_a1b = 0;
  /* 0130 */
} __packed_ws__(C_Login_XB_9E, 0x130);

struct C_LoginExtended_XB_9E : C_Login_XB_9E {
  SC_MeetUserExtension_DC_V3 extension;
} __packed_ws__(C_LoginExtended_XB_9E, 0x194);

struct C_LoginExtended_BB_9E {
  /* 0000 */ le_uint32_t player_tag = 0x00010000;
  /* 0004 */ le_uint32_t guild_card_number = 0; // == account_id when on newserv
  /* 0008 */ le_uint32_t sub_version = 0;
  /* 000C */ le_uint32_t language = 0;
  /* 0010 */ le_uint32_t unknown_a2 = 0;
  /* 0014 */ pstring<TextEncoding::ASCII, 0x10> v1_serial_number; // Always blank?
  /* 0024 */ pstring<TextEncoding::ASCII, 0x10> v1_access_key; // == "?"
  /* 0034 */ pstring<TextEncoding::ASCII, 0x10> serial_number; // Always blank?
  /* 0044 */ pstring<TextEncoding::ASCII, 0x10> access_key; // Always blank?
  /* 0054 */ pstring<TextEncoding::ASCII, 0x30> username;
  /* 0084 */ pstring<TextEncoding::ASCII, 0x30> password;
  /* 00B4 */ pstring<TextEncoding::ASCII, 0x10> guild_card_number_str;
  /* 00C4 */ parray<uint8_t, 0x28> client_config;
  /* 00EC */ SC_MeetUserExtension_PC_BB extension;
  /* 0170 */
} __packed_ws__(C_LoginExtended_BB_9E, 0x170);

// 9F (S->C): Request client config / security data (V3/BB)
// This command is not valid on PSO GC Episodes 1&2 Trial Edition, nor any
// pre-V3 PSO versions. Client will respond with a 9F command.
// No arguments

// 9F (C->S): Client config / security data response (V3/BB)
// The data is opaque to the client, as described at the top of this file.
// If newserv ever sent a 9F command (it currently does not). On BB, this
// command does not work during the data server phase.

struct C_ClientConfig_V3_9F {
  parray<uint8_t, 0x20> data;
} __packed_ws__(C_ClientConfig_V3_9F, 0x20);

struct C_ClientConfig_BB_9F {
  parray<uint8_t, 0x28> data;
} __packed_ws__(C_ClientConfig_BB_9F, 0x28);

// A0 (C->S): Change ship
// Internal name: SndShipList
// This structure is for documentation only; newserv ignores the arguments here.

struct C_ChangeShipOrBlock_A0_A1 {
  le_uint32_t player_tag = 0x00010000;
  le_uint32_t guild_card_number = 0;
  parray<uint8_t, 0x10> unused;
} __packed_ws__(C_ChangeShipOrBlock_A0_A1, 0x18);

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

template <TextEncoding Encoding, size_t ShortDescLength>
struct S_QuestMenuEntryT {
  // Note: The game treats menu_id as two 8-bit fields followed by a 16-bit
  // field. In most situations, this is opaque to the server, so we treat it as
  // a single 32-bit field; however, in the case of the quest menu, the first
  // and second bytes have meaning on the client.
  //
  // The first byte is used as the quest episode number, which is only relevant
  // for showing the Challenge Mode times window when a quest is selected.
  // This byte must be set correctly on the quest category entry, not the quest
  // itself, so the Episode 1 Challenge quests category should have a value of
  // 1 in this byte, and the Episode 2 Challenge quests category should have a
  // value of 2. (This is not the only condition required for the Challenge
  // Mode times window to work; see the description of command A3 also.)
  //
  // The second byte of the menu ID is used to determine which icon appears to
  // the left of the quest name.
  // Specifically:
  //   0 = online quest icon (green diamond)
  //   1 = download quest icon (green square with outlined diamond)
  //   2 = completed download quest icon (orange square with outlined diamond)
  //   Anything else = same as 1
  le_uint32_t menu_id = 0;
  le_uint32_t item_id = 0;
  pstring<Encoding, 0x20> name;
  pstring<Encoding, ShortDescLength> short_description;
} __packed__;
using S_QuestMenuEntry_PC_A2_A4 = S_QuestMenuEntryT<TextEncoding::UTF16, 0x70>;
using S_QuestMenuEntry_DC_GC_A2_A4 = S_QuestMenuEntryT<TextEncoding::MARKED, 0x70>;
using S_QuestMenuEntry_XB_A2_A4 = S_QuestMenuEntryT<TextEncoding::MARKED, 0x80>;
check_struct_size(S_QuestMenuEntry_PC_A2_A4, 0x128);
check_struct_size(S_QuestMenuEntry_DC_GC_A2_A4, 0x98);
check_struct_size(S_QuestMenuEntry_XB_A2_A4, 0xA8);

struct S_QuestMenuEntry_BB_A2_A4 {
  le_uint32_t menu_id = 0;
  le_uint32_t item_id = 0;
  pstring<TextEncoding::UTF16, 0x20> name;
  pstring<TextEncoding::UTF16, 0x78> short_description;
  // If this field is set, a yellow hex icon is displayed instead of the green
  // or orange diamond icon, and the quest is grayed out and cannot be selected.
  // This field is ignored if the icon type (see S_QuestMenuEntry) isn't 1 or 2.
  uint8_t disabled = 0;
  parray<uint8_t, 3> unused;
} __packed_ws__(S_QuestMenuEntry_BB_A2_A4, 0x13C);

// A3 (S->C): Quest information
// Same format as 1A/D5 command (plain text). The header.flag field is used to
// inform the client of the Challenge stage number, so it can show the correct
// timing window when the stage is selected. The Episode 1 stage numbers should
// be specified as 51-59 (decimal) in header.flag, and the Episode 2 stage
// numbers should be specified as 61-65 (decimal). If the header.flag value is
// outside this range, it is ignored, and the Challenge Mode times window does
// not update.

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
// This command is sent when the in-game quest menu (A2) is closed. This is used
// by the server to unlock the game if the players don't select a quest, since
// players are forbidden from joining while the quest menu is open. When the
// download quest menu is closed, either by downloading a quest or canceling,
// the client sends A0 instead.
// Curiously, PSO GC sends uninitialized data in header.flag.

// AA (C->S): Send quest statistic (V3/BB)
// This command is generated when an opcode F92E is executed in a quest.
// The server should respond with an AB command.
// This command is likely never sent by PSO GC Episodes 1&2 Trial Edition,
// because the following command (AB) is definitely not valid on that version.

struct C_SendQuestStatistic_V3_BB_AA {
  le_uint16_t stat_id = 0;
  le_uint16_t unused = 0;
  le_uint16_t label1 = 0;
  le_uint16_t label2 = 0;
  parray<le_uint32_t, 8> params;
} __packed_ws__(C_SendQuestStatistic_V3_BB_AA, 0x28);

// AB (S->C): Call quest function (V3/BB)
// This command is not valid on PSO GC Episodes 1&2 Trial Edition.
// Upon receipt, the client starts a quest thread running the given function.
// Probably this is supposed to be one of the function IDs previously sent in
// the AA command, but the client does not check for this. The server can
// presumably use this command to call any function at any time during a quest.

struct S_CallQuestFunction_V3_BB_AB {
  le_uint16_t label = 0;
  parray<uint8_t, 2> unused;
} __packed_ws__(S_CallQuestFunction_V3_BB_AB, 4);

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
// The time_flags fields are used on V3 and later. Time flags are a 24-bit
// little-endian integer, only 2 bits of which are used:
//   time_flags_low & 1 specifies whether the client should send a ping (1D)
//     every 900 frames (30 seconds).
//   time_flags_low & 2 disables system interrupts during a part of the GBA game
//     loading procedure. (Predictably, this is only used on GC versions.) It's
//     not clear what the downstream effects of this are, or why the server
//     should have any control over this behavior in the first place.
// Client will respond with a 99 command.

struct S_ServerTime_B1 {
  /* 00 */ pstring<TextEncoding::ASCII, 0x19> time_str;
  /* 19 */ uint8_t time_flags_low = 0x01;
  /* 1A */ uint8_t time_flags_mid = 0x00;
  /* 1B */ uint8_t time_flags_high = 0x00;
  /* 1C */
} __packed_ws__(S_ServerTime_B1, 0x1C);

// B2 (S->C): Execute code and/or checksum memory (DCv2 and all later versions)
// Internal name: RcvProgramPatch
// Client will respond with a B3 command with the same header.flag value as was
// sent in the B2.
// On PSO PC, the code section (if included in the B2 command) is parsed and
// relocated, but is not actually executed, so the return_value field in the
// resulting B3 command is always 0. The checksum functionality does work on PSO
// PC, just like the other versions.
// This command doesn't work on some PSO GC versions, namely the later JP PSO
// Plus (v1.5), US PSO Plus (v1.2), or US/EU Episode 3. Sega presumably removed
// it after taking heat from Nintendo about enabling homebrew on the GameCube.
// On the earlier JP PSO Plus (v1.4) and JP Episode 3, this command is
// implemented as described here, with some additional compression and
// encryption steps added, similarly to how download quests are encoded. See
// send_function_call in SendCommands.cc for more details on how this works.

// newserv supports exploiting a bug in the USA version of Episode 3, which
// re-enables the use of this command on that version of the game. See
// system/client-functions/Episode3USAQuestBufferOverflow.ppc.s for further
// details.

struct S_ExecuteCode_B2 {
  // If code_size == 0, no code is executed, but checksumming may still occur.
  // In that case, this structure is the entire body of the command (no code
  // or footer is sent).
  le_uint32_t code_size = 0; // Size of code (following this struct) and footer
  le_uint32_t checksum_start = 0; // May be null if size is zero
  le_uint32_t checksum_size = 0; // If zero, no checksum is computed
  // The code immediately follows, ending with a RELFileFooter. The REL's root
  // offset is relative to the start of the code object here, so an offset of 0
  // refers to the byte after checksum_size. The root offset points to the
  // offset ot the entrypoint (that is, the entrypoint offset is doubly
  // indirect). This is presumably done so the entrypoint can be optionally
  // relocated.
} __packed_ws__(S_ExecuteCode_B2, 0x0C);

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
} __packed_ws__(C_ExecuteCodeResult_B3, 8);

// B4: Invalid command
// B5: Invalid command
// B6: Invalid command

// B7 (S->C): Rank update (Episode 3)

struct S_RankUpdate_Ep3_B7 {
  // On non-NTE versions, if rank is not zero, the client sets its rank text to
  // "<rank>:<rank_text>", truncated to 11 characters. If rank is zero or the
  // client is NTE, the client uses rank_text without modifying it.
  le_uint32_t rank = 0;
  pstring<TextEncoding::CHALLENGE8, 0x0C> rank_text;
  // The remaining fields are ignored by NTE:
  le_uint32_t current_meseta = 0;
  le_uint32_t total_meseta_earned = 0;
  le_uint32_t unlocked_jukebox_songs = 0xFFFFFFFF;
} __packed_ws__(S_RankUpdate_Ep3_B7, 0x1C);

// B7 (C->S): Confirm rank update (Episode 3)
// No arguments
// The client sends this after it receives a B7 from the server.

// B8 (S->C): Update card definitions (Episode 3)
// Contents is a single le_uint32_t specifying the size of the (PRS-compressed)
// data, followed immediately by the data. The maximum size of the compressed
// data is 0x9000 bytes, although the receive buffer size limit applies first in
// practice, which limits this to 0x7BF8 bytes. The maximum size of the
// decompressed data is 0x36EC0 bytes on retail Episode 3, or 0x32960 bytes on
// Trial Edition.
// Note: PSO BB accepts this command as well, but ignores it.

// B8 (C->S): Confirm updated card definitions (Episode 3)
// No arguments
// The client sends this after it receives a B8 from the server.

// B9 (S->C): Update CARD lobby media (Episode 3)
// This command is not valid on Episode 3 Trial Edition.

struct S_UpdateMediaHeader_Ep3_B9 {
  // Valid values for the type field:
  //   1: Texture set (GVM file)
  //   2: Model
  //   3: Animation
  //   4: Delete all previous media updates
  //   Any other value: entire command is ignored
  // A texture can be displayed without a model or animation. A model requires a
  // texture (sent in a separate B9 command with the same location_flags), but
  // does not require an animation - it will just stand still without one. An
  // animation requires both a texture and model with the same location_flags.
  // For models and animations, the game looks for various tokens in the
  // decompressed data; specifically '****', 'GCAM', 'GJBM', 'GJTL', 'GLIM',
  // 'GMDM', 'GSSM', 'NCAM', 'NJBM', 'NJCA', 'NLIM', 'NMDM', and 'NSSM'.
  le_uint32_t type = 0;
  // location_flags is a bit field specifying where the banner or object should
  // appear. The bits are:
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
  // 00020000: Immediately left of 00010000 (2 banners)
  // 00040000: Immediately right of 00010000 (2 banners)
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
  // each may appear in more than one position). If 8 banners or objects already
  // exist, further media sent via B9 is ignored.
  le_uint32_t location_flags = 0x00000000;
  // This field specifies the size of the compressed data. The uncompressed size
  // is not sent anywhere in this command.
  le_uint16_t size = 0;
  le_uint16_t unused = 0;
  // The PRS-compressed data immediately follows this header. The maximum size
  // of the compressed data is 0x3800 bytes, and it must decompress to fewer
  // than 0x37000 bytes of output. If either of these limits are violated, the
  // client ignores the command.
} __packed_ws__(S_UpdateMediaHeader_Ep3_B9, 0x0C);

// B9 (C->S): Confirm media update (Episode 3)
// No arguments
// This command is not valid on Episode 3 Trial Edition.
// The client sends this even if it ignores the contents of a B9 command.

// BA: Meseta transaction (Episode 3)
// This command is not valid on Episode 3 Trial Edition.
// header.flag specifies the transaction purpose. Specific known values:
// 00 = unknown
// 01 = Initialize Meseta subsystem (C->S; always has a value of 0)
// 02 = Spend meseta (at e.g. lobby jukebox or Pinz's shop) (C->S)
// 03 = Spend meseta response (S->C; request_token must match the last token
//      sent by client)
// 04 = unknown (C->S; request_token must match the last token sent by client)

struct C_MesetaTransaction_Ep3_BA {
  le_uint32_t transaction_num = 0;
  le_uint32_t value = 0;
  le_uint32_t request_token = 0;
} __packed_ws__(C_MesetaTransaction_Ep3_BA, 0x0C);

struct S_MesetaTransaction_Ep3_BA {
  le_uint32_t current_meseta = 0;
  le_uint32_t total_meseta_earned = 0;
  le_uint32_t request_token = 0; // Should match the token sent by the client
} __packed_ws__(S_MesetaTransaction_Ep3_BA, 0x0C);

// BB (S->C): Tournament match information (Episode 3)
// This command is not valid on Episode 3 Trial Edition. Because of this, it
// must have been added fairly late in development, but it seems to be unused,
// perhaps because the E1/E3 commands are generally more useful... but the E1/E3
// commands exist in Trial Edition! So why was this added? Was it just never
// finished? We may never know...
// header.flag is the number of valid match entries.

struct S_TournamentMatchInformation_Ep3_BB {
  pstring<TextEncoding::MARKED, 0x20> tournament_name;
  struct TeamEntry {
    le_uint16_t win_count = 0;
    le_uint16_t is_active = 0;
    pstring<TextEncoding::MARKED, 0x20> name;
  } __packed_ws__(TeamEntry, 0x24);
  parray<TeamEntry, 0x20> team_entries;
  le_uint16_t num_teams = 0;
  le_uint16_t unknown_a3 = 0; // Probably actually unused
  struct MatchEntry {
    pstring<TextEncoding::MARKED, 0x20> name;
    uint8_t locked = 0;
    uint8_t count = 0;
    uint8_t max_count = 0;
    uint8_t unused = 0;
  } __packed_ws__(MatchEntry, 0x24);
  parray<MatchEntry, 0x40> match_entries;
} __packed_ws__(S_TournamentMatchInformation_Ep3_BB, 0xDA4);

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
template <TextEncoding Encoding>
struct S_ChoiceSearchEntryT {
  // Category IDs are nonzero; if the high byte of the ID is nonzero then the
  // category can be set by the user at any time; otherwise it can't.
  le_uint16_t parent_choice_id = 0; // 0 for top-level categories
  le_uint16_t choice_id = 0;
  pstring<Encoding, 0x1C> text;
} __packed__;
using S_ChoiceSearchEntry_DC_V3_C0 = S_ChoiceSearchEntryT<TextEncoding::MARKED>;
using S_ChoiceSearchEntry_PC_BB_C0 = S_ChoiceSearchEntryT<TextEncoding::UTF16>;
check_struct_size(S_ChoiceSearchEntry_DC_V3_C0, 0x20);
check_struct_size(S_ChoiceSearchEntry_PC_BB_C0, 0x3C);

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

template <TextEncoding Encoding>
struct C_CreateGameBaseT {
  // menu_id and item_id are only used for the E7 (create spectator team) form
  // of this command
  le_uint32_t menu_id = 0;
  le_uint32_t item_id = 0;
  pstring<Encoding, 0x10> name;
  pstring<Encoding, 0x10> password;
} __packed__;
using C_CreateGame_DCNTE = C_CreateGameBaseT<TextEncoding::SJIS>;
check_struct_size(C_CreateGame_DCNTE, 0x28);

template <TextEncoding Encoding>
struct C_CreateGameT : C_CreateGameBaseT<Encoding> {
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
using C_CreateGame_DC_V3_0C_C1_Ep3_EC = C_CreateGameT<TextEncoding::MARKED>;
using C_CreateGame_PC_C1 = C_CreateGameT<TextEncoding::UTF16>;
check_struct_size(C_CreateGame_DC_V3_0C_C1_Ep3_EC, 0x2C);
check_struct_size(C_CreateGame_PC_C1, 0x4C);

struct C_CreateGame_BB_C1 : C_CreateGameT<TextEncoding::UTF16> {
  uint8_t solo_mode = 0;
  parray<uint8_t, 3> unused2;
} __packed_ws__(C_CreateGame_BB_C1, 0x50);

// C2 (C->S): Set choice search parameters (DCv2 and later versions)
// Internal name: PutChoiceList
// Server does not respond.
// Contents is a ChoiceSearchConfig, which is defined in PlayerSubordinates.hh.

// C3 (C->S): Execute choice search (DCv2 and later versions)
// Internal name: SndChoiceSeq
// Same format as C2. The disabled field is unused.
// Server should respond with a C4 command.

// C4 (S->C): Choice search results (DCv2 and later versions)
// Internal name: RcvChoiceAns
// There is a bug that can cause the client to crash or display garbage if this
// command is sent with no entries. To work around this, newserv sends a blank
// entry (but still with header.flag = 0) if there are no results.

// Command is a list of these; header.flag is the entry count
template <typename HeaderT, TextEncoding NameEncoding, TextEncoding DescEncoding, TextEncoding LocatorEncoding>
struct S_ChoiceSearchResultEntryT_C4 {
  le_uint32_t guild_card_number = 0;
  pstring<NameEncoding, 0x10> name;
  pstring<DescEncoding, 0x20> info_string; // Usually something like "<class> Lvl <level>"
  // Format is stricter here; this is "LOBBYNAME,BLOCKNUM,SHIPNAME"
  // If target is in game, for example, "Game Name,BLOCK01,Alexandria"
  // If target is in lobby, for example, "BLOCK01-1,BLOCK01,Alexandria"
  pstring<LocatorEncoding, 0x30> location_string;
  HeaderT reconnect_command_header; // Ignored by the client
  S_Reconnect_19 reconnect_command;
  SC_MeetUserExtensionT<NameEncoding> meet_user;
} __packed__;
using S_ChoiceSearchResultEntry_DC_V3_C4 = S_ChoiceSearchResultEntryT_C4<PSOCommandHeaderDCV3, TextEncoding::ASCII, TextEncoding::MARKED, TextEncoding::ASCII>;
using S_ChoiceSearchResultEntry_PC_C4 = S_ChoiceSearchResultEntryT_C4<PSOCommandHeaderPC, TextEncoding::UTF16, TextEncoding::UTF16, TextEncoding::UTF16>;
using S_ChoiceSearchResultEntry_BB_C4 = S_ChoiceSearchResultEntryT_C4<PSOCommandHeaderBB, TextEncoding::UTF16_ALWAYS_MARKED, TextEncoding::UTF16, TextEncoding::UTF16>;
check_struct_size(S_ChoiceSearchResultEntry_DC_V3_C4, 0xD4);
check_struct_size(S_ChoiceSearchResultEntry_PC_C4, 0x154);
check_struct_size(S_ChoiceSearchResultEntry_BB_C4, 0x158);

// C5 (S->C): Player records update (DCv2 and later versions)
// Internal name: RcvChallengeData
// Command is a list of PlayerRecordsEntry structures; header.flag specifies
// the entry count.
// The server sends this command when a player joins a lobby to update the
// challenge mode records of all the present players.

// C6 (C->S): Set blocked senders list (V3/BB)
// The command always contains the same number of entries, even if the entries
// at the end are blank (zero).

template <size_t Count>
struct C_SetBlockedSendersT_C6 {
  parray<le_uint32_t, Count> blocked_senders;
} __packed__;
using C_SetBlockedSenders_V3_C6 = C_SetBlockedSendersT_C6<30>;
using C_SetBlockedSenders_BB_C6 = C_SetBlockedSendersT_C6<28>;
check_struct_size(C_SetBlockedSenders_V3_C6, 0x78);
check_struct_size(C_SetBlockedSenders_BB_C6, 0x70);

// C7 (C->S): Enable simple mail auto-reply (V3/BB)
// Same format as 1A/D5 command (plain text).
// Server does not respond

// C8 (C->S): Disable simple mail auto-reply (V3/BB)
// No arguments
// Server does not respond

// C9: Broadcast command (Episode 3)
// Internal name: SndCardClientData
// Same as 60, but should be forwarded only to Episode 3 clients.
// newserv uses this command for all server-generated events (in response to CA
// commands), except for map data requests. This differs from Sega's original
// implementation, which sent CA responses via 60 commands instead.

// C9 (C->S): Change connection status (Xbox)
// header.flag specifies if the player's online status should be hidden; 1 means
// shown, 2 means hidden.

// CA (C->S): Server data request (Episode 3)
// Internal name: SndCardServerData
// The CA command format is the same as that of the 6xB3 commands, and the
// subsubcommands formats are shared as well. Unlike the 6x commands, the server
// is expected to respond to the command appropriately instead of forwarding it.
// Because the formats are shared, the 6xB3 commands are also referred to as CAx
// commands in the comments and structure names.

// CB: Broadcast command (Episode 3)
// Internal name: SndKansenPsoData
// Same as 60, but only send to Episode 3 clients.
// This command's format is identical to C9, except that CB is not valid on
// Episode 3 Trial Edition (whereas C9 is valid).
// Unlike the 6x and C9 commands, subcommands sent with the CB command are
// forwarded from spectator teams to the primary team. The client only uses this
// behavior for the 6xBE command (sound chat), and newserv enforces that no
// other subcommand can be sent via CB.

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

struct S_ConfirmTournamentEntry_Ep3_CC {
  pstring<TextEncoding::MARKED, 0x40> tournament_name;
  le_uint16_t num_teams = 0;
  le_uint16_t players_per_team = 0;
  le_uint16_t unknown_a2 = 0;
  le_uint16_t unknown_a3 = 0;
  pstring<TextEncoding::MARKED, 0x20> server_name;
  pstring<TextEncoding::MARKED, 0x20> start_time; // e.g. "15:09:30" or "13:03 PST"
  struct TeamEntry {
    le_uint16_t win_count = 0;
    le_uint16_t is_active = 0;
    pstring<TextEncoding::MARKED, 0x20> name;
  } __packed_ws__(TeamEntry, 0x24);
  parray<TeamEntry, 0x20> team_entries;
} __packed_ws__(S_ConfirmTournamentEntry_Ep3_CC, 0x508);

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
  parray<ItemData, 0x20> item_datas;
} __packed_ws__(SC_TradeItems_D0_D3, 0x284);

// D1 (S->C): Advance trade state (V3/BB)
// No arguments
// See D0 description for usage information.

// D2 (C->S): Trade can proceed (V3/BB)
// No arguments
// See D0 description for usage information.

// D3 (S->C): Execute trade (V3/BB)
// On V3, this command has the same format as D0. See the D0 description for
// usage information.
// On BB, this command has no arguments (and the server generates the
// appropriate delete and create inventory item commands), but the D3 command
// must still must be sent before the D4 command to advance the trade state.

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
// DC, PC, and BB do not send this command at all. GC US v1.0 and v1.1 will send
// this command when any large message box (1A/D5) is closed; GC Plus and
// Episode 3 will send D6 only for large message boxes that occur before the
// client has joined a lobby. (After joining a lobby, large message boxes will
// still be displayed if sent by the server, but the client won't send a D6 when
// they are closed.) In some of these versions, there is a bug that sets an
// incorrect interaction mode when the message box is closed while the player is
// in the lobby; some servers (e.g. Schtserv) send a lobby welcome message
// anyway, along with an 01 (lobby message box) which properly sets the
// interaction mode when closed.

// D7 (C->S): Request GBA game file (V3)
// This command is sent when the client executes the file_dl_req (F8C0) quest
// opcode. header.flag contains the value of the opcode's first argument; the
// second argument is a pointer to the filename.
// The server should send the requested file using A6/A7 commands; if the file
// does not exist, the server should reply with a D7 command.
// This command exists on XB as well, but it presumably is never sent by the
// client.

struct C_GBAGameRequest_V3_D7 {
  pstring<TextEncoding::MARKED, 0x10> filename;
} __packed_ws__(C_GBAGameRequest_V3_D7, 0x10);

// D7 (S->C): GBA file not found (V3/BB)
// No arguments
// This command is not valid on PSO GC Episodes 1&2 Trial Edition.
// This command tells the client that the file it requested via a D7 command
// does not exist. This causes the F8C1 (get_dl_status) quest opcode to return
// 0 (file not found), rather than 1 (download in progress) or 2 (complete).
// PSO BB accepts but completely ignores this command.

// D8 (C->S): Info board request (V3/BB)
// No arguments
// The server should respond with a D8 command (described below).

// D8 (S->C): Info board contents (V3/BB)
// This command is not valid on PSO GC Episodes 1&2 Trial Edition.

// Command is a list of these; header.flag is the entry count. There should be
// one entry for each player in the current lobby/game.
template <TextEncoding NameEncoding, TextEncoding MessageEncoding>
struct S_InfoBoardEntryT_D8 {
  pstring<NameEncoding, 0x10> name;
  pstring<MessageEncoding, 0xAC> message;
} __packed__;
using S_InfoBoardEntry_V3_D8 = S_InfoBoardEntryT_D8<TextEncoding::ASCII, TextEncoding::MARKED>;
using S_InfoBoardEntry_BB_D8 = S_InfoBoardEntryT_D8<TextEncoding::UTF16_ALWAYS_MARKED, TextEncoding::UTF16>;
check_struct_size(S_InfoBoardEntry_V3_D8, 0xBC);
check_struct_size(S_InfoBoardEntry_BB_D8, 0x178);

// D9 (C->S): Write info board (V3/BB)
// Contents are plain text, like 1A/D5.
// Server does not respond

// DA (S->C): Change lobby event (V3/BB)
// header.flag = new event number; no other arguments.
// This command is not valid on PSO GC Episodes 1&2 Trial Edition.

// DB (C->S): Verify license (V3/BB)
// Server should respond with a 9A command.

struct C_VerifyAccount_V3_DB {
  pstring<TextEncoding::ASCII, 0x10> v1_serial_number; // Unused
  pstring<TextEncoding::ASCII, 0x10> v1_access_key; // Unused
  pstring<TextEncoding::ASCII, 0x10> serial_number; // On XB, this is the XBL gamertag
  pstring<TextEncoding::ASCII, 0x10> access_key; // On XB, this is the XBL user ID
  pstring<TextEncoding::ASCII, 0x08> unused2;
  le_uint32_t sub_version = 0;
  pstring<TextEncoding::ASCII, 0x30> serial_number2; // On XB, this is the XBL gamertag
  pstring<TextEncoding::ASCII, 0x30> access_key2; // On XB, this is the XBL user ID
  pstring<TextEncoding::ASCII, 0x30> password; // On XB, this contains "xbox-pso"
} __packed_ws__(C_VerifyAccount_V3_DB, 0xDC);

// Note: This login pathway generally isn't used on BB (and isn't supported at
// all during the data server phase). All current servers use 03/93 instead.
struct C_VerifyAccount_BB_DB {
  // Note: These four fields are likely the same as those used in BB's 9E
  pstring<TextEncoding::ASCII, 0x10> unknown_a3; // Always blank?
  pstring<TextEncoding::ASCII, 0x10> unknown_a4; // == "?"
  pstring<TextEncoding::ASCII, 0x10> unknown_a5; // Always blank?
  pstring<TextEncoding::ASCII, 0x10> unknown_a6; // Always blank?
  le_uint32_t sub_version = 0;
  pstring<TextEncoding::ASCII, 0x30> username;
  pstring<TextEncoding::ASCII, 0x30> password;
  pstring<TextEncoding::ASCII, 0x30> game_tag; // "psopc2"
} __packed_ws__(C_VerifyAccount_BB_DB, 0xD4);

// DC: Set battle in progress flag (Episode 3)
// No arguments except header.flag when sent by the client. When header.flag is
// 1, the game should be locked - no players should be allowed to join. In this
// case, the client waits for the server to respond with another DC command
// before proceeding with battle setup. When header.flag is 0, the game should
// be unlocked, and the client does not wait for a response from the server.

// DC: Guild card data (BB)

struct S_GuildCardHeader_BB_01DC {
  le_uint32_t unknown = 1;
  le_uint32_t filesize = 0x0000D590;
  le_uint32_t checksum = 0; // CRC32 of entire guild card file (0xD590 bytes)
} __packed_ws__(S_GuildCardHeader_BB_01DC, 0x0C);

struct S_GuildCardFileChunk_02DC {
  le_uint32_t unknown = 0; // 0
  le_uint32_t chunk_index = 0;
  parray<uint8_t, 0x6800> data; // Command may be shorter if this is the last chunk
} __packed_ws__(S_GuildCardFileChunk_02DC, 0x6808);

struct C_GuildCardDataRequest_BB_03DC {
  le_uint32_t unknown = 0;
  le_uint32_t chunk_index = 0;
  le_uint32_t cont = 0;
} __packed_ws__(C_GuildCardDataRequest_BB_03DC, 0x0C);

// DD (S->C): Send quest state to joining player (BB)
// When a player joins a game with a quest already in progress, the server
// should send this command to the leader. header.flag is the client ID that the
// leader should send quest state to; the leader will then send a series of
// target commands (62/6D) that the server can forward to the joining player.
// No other arguments

// DE (S->C): Rare monster list (BB)

struct S_RareMonsterList_BB_DE {
  // Unused entries are set to FFFF
  parray<le_uint16_t, 0x10> enemy_indexes;
} __packed_ws__(S_RareMonsterList_BB_DE, 0x20);

// DF (C->S): Set Challenge Mode parameters (BB)
// This command has 7 subcommands, most of which are self-explanatory.

struct C_SetChallengeModeStageNumber_BB_01DF {
  le_uint32_t stage = 0;
} __packed_ws__(C_SetChallengeModeStageNumber_BB_01DF, 4);

struct C_SetChallengeModeCharacterTemplate_BB_02DF {
  le_uint32_t template_index = 0;
} __packed_ws__(C_SetChallengeModeCharacterTemplate_BB_02DF, 4);

struct C_SetChallengeModeDifficulty_BB_03DF {
  // No existing challenge mode quest sets this to a value other than zero.
  le_uint32_t difficulty = 0;
} __packed_ws__(C_SetChallengeModeDifficulty_BB_03DF, 4);

struct C_SetChallengeModeEXPMultiplier_BB_04DF {
  le_float exp_multiplier = 1.0f;
} __packed_ws__(C_SetChallengeModeEXPMultiplier_BB_04DF, 4);

struct C_SetChallengeRankText_BB_05DF {
  le_uint32_t rank_color = 0; // ARGB8888
  pstring<TextEncoding::CHALLENGE16, 0x0C> rank_text;
} __packed_ws__(C_SetChallengeRankText_BB_05DF, 0x1C);

// This is sent once for each rank (so, 3 times in total)
struct C_SetChallengeRankThreshold_BB_06DF {
  le_uint32_t rank = 0; // 0 = B, 1 = A, 2 = S
  le_uint32_t seconds = 0;
  le_uint32_t rank_bitmask = 0; // 1 = B, 2 = A, 4 = S
} __packed_ws__(C_SetChallengeRankThreshold_BB_06DF, 0x0C);

struct C_CreateChallengeModeAwardItem_BB_07DF {
  le_uint32_t prize_rank = 0xFFFFFFFF; // 0 = B, 1 = A, 2 = S, anything else = error
  le_uint32_t rank_bitmask = 0; // Same as in 06DF
  ItemData item;
} __packed_ws__(C_CreateChallengeModeAwardItem_BB_07DF, 0x1C);

// E0 (S->C): Tournament list (Episode 3)
// The client will send 09 and 10 commands to inspect or enter a tournament. The
// server should respond to an 09 command with an E3 command; the server should
// respond to a 10 command with an E2 command.
// header.flag is the count of filled-in entries.

struct S_TournamentList_Ep3NTE_E0 {
  struct Entry {
    le_uint32_t menu_id = 0;
    le_uint32_t item_id = 0;
    uint8_t unknown_a1 = 0;
    uint8_t locked = 0;
    uint8_t state = 0;
    uint8_t unknown_a2 = 0;
    le_uint32_t start_time = 0; // In seconds since Unix epoch
    pstring<TextEncoding::MARKED, 0x20> name;
    le_uint16_t num_teams = 0;
    le_uint16_t max_teams = 0;
  } __packed_ws__(Entry, 0x34);
  parray<Entry, 0x20> entries;
} __packed_ws__(S_TournamentList_Ep3NTE_E0, 0x680);

struct S_TournamentList_Ep3_E0 {
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
    pstring<TextEncoding::MARKED, 0x20> name;
    le_uint16_t num_teams = 0;
    le_uint16_t max_teams = 0;
    le_uint16_t unknown_a3 = 0xFFFF;
    le_uint16_t unknown_a4 = 0xFFFF;
  } __packed_ws__(Entry, 0x38);
  parray<Entry, 0x20> entries;
} __packed_ws__(S_TournamentList_Ep3_E0, 0x700);

// E0 (C->S): Request system file (BB)
// No arguments. The server should respond with an E1 or E2 command.

// E1 (S->C): Game information (Episode 3)
// The header.flag argument determines which fields are valid (and which panes
// should be shown in the information window). The values are the same as for
// the E3 command, but each value only makes sense for one command. That is, 00,
// 01, and 04 should be used with the E1 command, while 02, 03, and 05 should be
// used with the E3 command. See the E3 command for descriptions of what each
// flag value means.

template <typename RulesT>
struct S_GameInformationBaseT_Ep3_E1 {
  /* 0000 */ pstring<TextEncoding::MARKED, 0x20> game_name;
  struct PlayerEntry {
    pstring<TextEncoding::ASCII, 0x10> name; // From disp.name
    pstring<TextEncoding::MARKED, 0x20> description; // Usually something like "FOmarl CLv30 J"
  } __packed_ws__(PlayerEntry, 0x30);
  /* 0020 */ parray<PlayerEntry, 4> player_entries;
  /* 00E0 */ pstring<TextEncoding::MARKED, 0x20> map_name;
  /* 0100 */ RulesT rules;
  /* 0114 */ parray<PlayerEntry, 8> spectator_entries;
  /* 0294 */
} __packed__;
using S_GameInformation_Ep3NTE_E1 = S_GameInformationBaseT_Ep3_E1<Episode3::RulesTrial>;
using S_GameInformation_Ep3_E1 = S_GameInformationBaseT_Ep3_E1<Episode3::Rules>;
check_struct_size(S_GameInformation_Ep3NTE_E1, 0x28C);
check_struct_size(S_GameInformation_Ep3_E1, 0x294);

// E1 (S->C): System file created (BB)
// This seems to take the place of 00E2 in certain cases. Perhaps it was used
// when a client hadn't logged in before and didn't have a system file, so the
// client should use appropriate defaults.

struct S_SystemFileCreated_00E1_BB {
  // If success is not equal to 1, the client shows a message saying "Forced
  // server disconnect (907)" and disconnects. Otherwise, the client proceeeds
  // as if it had received an 00E2 command, and sends its first 00E3.
  le_uint32_t success = 1;
} __packed_ws__(S_SystemFileCreated_00E1_BB, 4);

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

struct S_TournamentEntryList_Ep3_E2 {
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
    pstring<TextEncoding::MARKED, 0x20> name;
  } __packed_ws__(Entry, 0x2C);
  parray<Entry, 0x20> entries;
} __packed_ws__(S_TournamentEntryList_Ep3_E2, 0x584);

// E2 (S->C): Set system file contents (BB)

struct S_SyncSystemFile_BB_E2 {
  PSOBBBaseSystemFile system_file;
  PSOBBTeamMembership team_membership;
} __packed_ws__(S_SyncSystemFile_BB_E2, 0xAF0);

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

template <typename RulesT>
struct S_TournamentGameDetailsBaseT_Ep3_E3 {
  // These fields are used only if the Rules pane is shown
  /* 0000 */ pstring<TextEncoding::MARKED, 0x20> name;
  /* 0020 */ pstring<TextEncoding::MARKED, 0x20> map_name;
  /* 0040 */ RulesT rules;

  // This field is used only if the bracket pane is shown
  struct BracketEntry {
    le_uint16_t win_count = 0;
    le_uint16_t is_active = 0;
    pstring<TextEncoding::MARKED, 0x18> team_name;
    parray<uint8_t, 8> unused;
  } __packed_ws__(BracketEntry, 0x24);
  /* 0054 */ parray<BracketEntry, 0x20> bracket_entries;

  // This field is used only if the Opponents pane is shown. If players_per_team
  // is 2, all fields are shown; if player_per_team is 1, team_name and
  // players[1] is ignored (only players[0] is shown).
  struct PlayerEntry {
    pstring<TextEncoding::ASCII, 0x10> name;
    pstring<TextEncoding::MARKED, 0x20> description; // Usually something like "RAmarl CLv24 E"
  } __packed_ws__(PlayerEntry, 0x30);
  struct TeamEntry {
    pstring<TextEncoding::MARKED, 0x10> team_name;
    parray<PlayerEntry, 2> players;
  } __packed_ws__(TeamEntry, 0x70);
  /* 04D4 */ parray<TeamEntry, 2> team_entries;

  /* 05B4 */ le_uint16_t num_bracket_entries = 0;
  /* 05B6 */ le_uint16_t players_per_team = 0;
  /* 05B8 */ le_uint16_t unknown_a4 = 0;
  /* 05BA */ le_uint16_t num_spectators = 0;
  /* 05BC */ parray<PlayerEntry, 8> spectator_entries;
  /* 073C */
} __packed__;
using S_TournamentGameDetails_Ep3NTE_E3 = S_TournamentGameDetailsBaseT_Ep3_E3<Episode3::RulesTrial>;
using S_TournamentGameDetails_Ep3_E3 = S_TournamentGameDetailsBaseT_Ep3_E3<Episode3::Rules>;
check_struct_size(S_TournamentGameDetails_Ep3NTE_E3, 0x734);
check_struct_size(S_TournamentGameDetails_Ep3_E3, 0x73C);

// E3 (C->S): Player preview request (BB)

struct C_PlayerPreviewRequest_BB_E3 {
  le_int32_t character_index = 0;
  le_uint32_t unused = 0;
} __packed_ws__(C_PlayerPreviewRequest_BB_E3, 0x08);

// E4: CARD lobby battle table state (Episode 3)
// When client sends an E4, server should respond with another E4 (but these
// commands have different formats).
// When the client has received an E4 command in which all entries have state 0
// or 2, the client will stop the player from moving and show a message saying
// that the game will begin shortly. The server should send a 64 command shortly
// thereafter.

// header.flag = seated state (1 = present, 0 = leaving)
struct C_CardBattleTableState_Ep3_E4 {
  le_uint16_t table_number = 0;
  le_uint16_t seat_number = 0;
} __packed_ws__(C_CardBattleTableState_Ep3_E4, 4);

// header.flag = table number
struct S_CardBattleTableState_Ep3_E4 {
  struct Entry {
    // State values:
    // 0 = no player present
    // 1 = player present, not confirmed
    // 2 = player present, confirmed
    // 3 = player present, declined
    le_uint16_t state = 0;
    le_uint16_t unknown_a1 = 0;
    le_uint32_t guild_card_number = 0;
  } __packed_ws__(Entry, 8);
  parray<Entry, 4> entries;
} __packed_ws__(S_CardBattleTableState_Ep3_E4, 0x20);

// E4 (S->C): Player choice or no player present (BB)

struct S_ApprovePlayerChoice_BB_00E4 {
  le_int32_t character_index = 0;
  le_uint32_t result = 0; // 1 = approved
} __packed_ws__(S_ApprovePlayerChoice_BB_00E4, 8);

struct S_PlayerPreview_NoPlayer_BB_00E4 {
  le_int32_t character_index = 0;
  le_uint32_t error = 0; // 2 = no player present
} __packed_ws__(S_PlayerPreview_NoPlayer_BB_00E4, 8);

// E5 (C->S): Confirm CARD lobby battle table choice (Episode 3)
// header.flag specifies whether the client answered "Yes" (1) or "No" (0).

struct S_CardBattleTableConfirmation_Ep3_E5 {
  le_uint16_t table_number = 0;
  le_uint16_t seat_number = 0;
} __packed_ws__(S_CardBattleTableConfirmation_Ep3_E5, 4);

// E5 (S->C): Player preview (BB)
// E5 (C->S): Create character (BB)

struct SC_PlayerPreview_CreateCharacter_BB_00E5 {
  le_int32_t character_index = 0;
  PlayerDispDataBBPreview preview;
} __packed_ws__(SC_PlayerPreview_CreateCharacter_BB_00E5, 0x80);

// E6 (C->S): Spectator team control (Episode 3)

// With header.flag == 0, this command has no arguments and is used for
// requesting the spectator team list. The server responds with an E6 command.

// With header.flag == 1, this command is used for joining a tournament
// spectator team. The following arguments are given in this form:

struct C_JoinSpectatorTeam_Ep3_E6_Flag01 {
  le_uint32_t menu_id = 0;
  le_uint32_t item_id = 0;
} __packed_ws__(C_JoinSpectatorTeam_Ep3_E6_Flag01, 8);

// E6 (S->C): Spectator team list (Episode 3)
// Same format as 08 command.

// E6 (S->C): Set guild card number and update client config (BB)
// This command sets the player's guild card number. During the data server
// phase, it also sets the client config and enabled features (these fields are
// ignored during the game server phase).

struct S_ClientInit_BB_00E6 {
  // The error codes are (error_code => internal_error_code => string)
  // 00    => (no error; client proceeds normally)
  //       => 01 => "(No901)\nUnable to obtain server address.\nPlease confirm your DNS settings.",
  //       => 02 => "(No902)\nNetwork initialization failed.\nPlease check your Internet connection settings.",
  // 01    => 03 => "(No903)\nServer connection failed.\nThe server may be under maintenance.\nPlease check the current news updates on the Official Site.",
  // 02    => 04 => "(No904)\nIncorrect Game ID or Game Password.",
  // 03    => 05 => "(No905)\nIncorrect Game ID or Game Password.",
  // 04    => 06 => "(No906)\nThis server is under maintenance.\nPlease see the Official Site for details.",
  // (any) => 07 => "(No907)\nForced server disconnect.\nPlease check your Game ID and individual settings.",
  // 06/07 => 08 => "(No910)\nThis Game ID has been suspended.",
  // 05    => 09 => "(No911)\nThis Game ID is in use by another user.",
  // 08    => 0A => "(No912)\nNo record for this Game ID.\nPlease register your user information at the Official Site.",
  // 09    => 0B => "(No913)\nYour paid time has expired.\nPlease renew your account at the Official Site.",
  // 0A    => 0C => "(No914)\nDue to the program not being shut down properly, data is locked. Please try connecting again in 10 minutes.",
  // 0B    => 0D => "(No915)\nThis program has not been updated.  The patch may not have run properly.  Please try shutting down and restarting the program.\nThe most recent news updates can be found on the Official Site.",
  //       => 0E => "(No916)\nThis server is full.\nPlease try connecting again later."
  le_uint32_t error_code = 0;
  le_uint32_t player_tag = 0x00010000;
  le_uint32_t guild_card_number = 0;
  // If security_token is zero, the client scrambles client_config before
  // sending it back in a later 93 command. See scramble_bb_security_data in
  // ReceiveCommands.cc for details on how this is done.
  le_uint32_t security_token = 0;
  parray<uint8_t, 0x28> client_config;
  uint8_t can_create_team = 1;
  uint8_t episode_4_unlocked = 1;
  parray<uint8_t, 2> unused;
} __packed_ws__(S_ClientInit_BB_00E6, 0x3C);

// E7 (C->S): Create spectator team (Episode 3)
// This command is used to create speectator teams for both tournaments and
// regular games. The server should be able to tell these cases apart by the
// menu and/or item ID.

struct C_CreateSpectatorTeam_Ep3_E7 {
  le_uint32_t menu_id = 0;
  le_uint32_t item_id = 0;
  pstring<TextEncoding::ASCII, 0x10> name;
  pstring<TextEncoding::MARKED, 0x10> password;
  le_uint32_t unused = 0;
} __packed_ws__(C_CreateSpectatorTeam_Ep3_E7, 0x2C);

// E7 (S->C): Tournament entry list for spectating (Episode 3)
// Same format as E2 command.

// E7: Sync save files (BB)

struct SC_SyncSaveFiles_BB_E7 {
  /* 0000 */ PSOBBCharacterFile char_file;
  /* 2EA4 */ PSOBBBaseSystemFile system_file;
  /* 30DC */ PSOBBTeamMembership team_membership;
  /* 3994 */
} __packed_ws__(SC_SyncSaveFiles_BB_E7, 0x3994);

// E8 (S->C): Join spectator team (Episode 3)
// header.flag = player count (including spectators)
// The client will crash if leader_id == client_id. Presumably one of the
// primary game's players should be the leader (this is what newserv does).

struct S_JoinSpectatorTeam_Ep3_E8 {
  /* 0000 */ Variations variations; // unused
  struct PlayerEntry {
    /* 0000 */ PlayerLobbyDataDCGC lobby_data;
    /* 0020 */ PlayerInventory inventory;
    /* 036C */ PlayerDispDataDCPCV3 disp;
    /* 043C */
  } __packed_ws__(PlayerEntry, 0x43C);
  /* 0080 */ parray<PlayerEntry, 4> players;
  /* 1170 */ uint8_t client_id = 0;
  /* 1171 */ uint8_t leader_id = 0;
  /* 1172 */ uint8_t disable_udp = 1;
  /* 1173 */ uint8_t difficulty = 0;
  /* 1174 */ uint8_t battle_mode = 0;
  /* 1175 */ uint8_t event = 0;
  /* 1176 */ uint8_t section_id = 0;
  /* 1177 */ uint8_t challenge_mode = 0;
  /* 1178 */ le_uint32_t random_seed = 0;
  /* 117C */ uint8_t episode = 0;
  /* 117D */ parray<uint8_t, 3> unused;
  struct SpectatorEntry {
    // It seems that at some point Sega intended to show each player's rank in
    // spectator teams. The unused1 and unused3 fields are intended for the
    // player's encrypted rank text and rank color (according to old Sega logs),
    // but the client ignores them. It's not clear what unused4 may have been
    // for, but the client also completely ignores it.
    /* 00 */ le_uint32_t player_tag = 0;
    /* 04 */ le_uint32_t guild_card_number = 0;
    /* 08 */ pstring<TextEncoding::ASCII, 0x10> name;
    /* 18 */ pstring<TextEncoding::CHALLENGE8, 0x10> unused1;
    /* 28 */ uint8_t present = 0;
    /* 29 */ uint8_t unused2 = 0;
    /* 2A */ le_uint16_t level = 0;
    /* 2C */ le_uint32_t unused3 = 0xFFFFFFFF;
    /* 30 */ le_uint32_t name_color = 0xFFFFFFFF; // ARGB8888
    /* 34 */ parray<le_uint16_t, 2> unused4;
    /* 38 */
  } __packed_ws__(SpectatorEntry, 0x38);
  // Somewhat misleadingly, this array also includes the players actually in the
  // battle - they appear in the first positions. Presumably the first 4 are
  // always for battlers, and the last 8 are always for spectators.
  /* 1180 */ parray<SpectatorEntry, 12> entries;
  /* 1420 */ pstring<TextEncoding::MARKED, 0x20> spectator_team_name;
  // This field doesn't appear to be actually used by the game, but some servers
  // send it anyway (and the game ignores it)
  /* 1440 */ parray<PlayerEntry, 8> spectator_players;
  /* 3620 */
} __packed_ws__(S_JoinSpectatorTeam_Ep3_E8, 0x3620);

// E8 (C->S): Guild card commands (BB)

// 01E8 (C->S): Check guild card file checksum

// This struct is for documentation purposes only; newserv ignores the contents
// of this command.
struct C_GuildCardChecksum_01E8 {
  le_uint32_t checksum = 0;
  le_uint32_t unused = 0;
} __packed_ws__(C_GuildCardChecksum_01E8, 8);

// 02E8 (S->C): Accept/decline guild card file checksum
// If needs_update is nonzero, the client will request the guild card file by
// sending an 03E8 command. If needs_update is zero, the client will skip
// downloading the guild card file and send a 04EB command (requesting the
// stream file) instead.

struct S_GuildCardChecksumResponse_BB_02E8 {
  le_uint32_t needs_update = 0;
  le_uint32_t unused = 0;
} __packed_ws__(S_GuildCardChecksumResponse_BB_02E8, 8);

// 03E8 (C->S): Request guild card file
// No arguments
// Server should send the guild card file data using DC commands.

// 04E8 (C->S): Add guild card
// Format is GuildCardBB (see PlayerSubordinates.hh)

// 05E8 (C->S): Delete guild card

struct C_DeleteGuildCard_BB_05E8_08E8 {
  le_uint32_t guild_card_number = 0;
} __packed_ws__(C_DeleteGuildCard_BB_05E8_08E8, 4);

// 06E8 (C->S): Update (overwrite) guild card
// Note: This command is also sent when the player writes a comment on their own
// guild card.
// Format is GuildCardBB (see PlayerSubordinates.hh)

// 07E8 (C->S): Add blocked user
// Format is GuildCardBB (see PlayerSubordinates.hh)

// 08E8 (C->S): Delete blocked user
// Same format as 05E8.

// 09E8 (C->S): Write comment

struct C_WriteGuildCardComment_BB_09E8 {
  le_uint32_t guild_card_number = 0;
  pstring<TextEncoding::UTF16, 0x58> comment;
} __packed_ws__(C_WriteGuildCardComment_BB_09E8, 0xB4);

// 0AE8 (C->S): Swap positions of guild cards in list

struct C_SwapGuildCardPositions_BB_0AE8 {
  le_uint32_t guild_card_number1 = 0;
  le_uint32_t guild_card_number2 = 0;
} __packed_ws__(C_SwapGuildCardPositions_BB_0AE8, 8);

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

struct S_TimedMessageBoxHeader_Ep3_EA {
  le_uint32_t duration = 0; // In frames; 30 frames = 1 second
  // Message data follows here (up to 0x1000 chars)
} __packed_ws__(S_TimedMessageBoxHeader_Ep3_EA, 4);

// EA: Team control (BB)

// 01EA (C->S): Create team

struct C_CreateTeam_BB_01EA {
  pstring<TextEncoding::UTF16, 0x10> name;
} __packed_ws__(C_CreateTeam_BB_01EA, 0x20);

// 02EA (S->C): Create team result
// No arguments except header.flag, which specifies the error code. Values:
// 0 = success
// 1 = generic error
// 2 = name already registered
// 3 = generic error
// 4 = generic error
// 5 = generic error
// 6 = generic error
// Anything else = command is ignored

// 03EA (C->S): Add team member

struct C_AddOrRemoveTeamMember_BB_03EA_05EA {
  le_uint32_t guild_card_number = 0;
} __packed_ws__(C_AddOrRemoveTeamMember_BB_03EA_05EA, 4);

// 04EA (S->C): Add team member result
// No arguments except header.flag, which specifies the error code. Values:
// 0 = success
// 5 = team is full
// Anything else = generic error

// 05EA (C->S): Remove team member
// Same format as 03EA.

// 06EA (S->C): Remove team member result
// No arguments except header.flag, which specifies the error code. 0 means
// success, but it's not known what any other values mean. The client expects
// the error code to be less than 7.

// 07EA: Team chat

struct SC_TeamChat_BB_07EA {
  pstring<TextEncoding::UTF16_ALWAYS_MARKED, 0x10> sender_name;
  // Text follows here. The message is truncated by the client if it is longer
  // than 0x8F wchar_ts.
} __packed_ws__(SC_TeamChat_BB_07EA, 0x20);

// 08EA (C->S): Get team member list
// No arguments

// 09EA (S->C): Team member list

struct S_TeamMemberList_BB_09EA {
  le_uint32_t entry_count = 0;
  struct Entry {
    // This is displayed as "<%04d> %s" % (rank, name)
    le_uint32_t rank = 0;
    le_uint32_t privilege_level = 0; // 0x10 or 0x20 = green, 0x30 = blue, 0x40 = red, anything else = white
    le_uint32_t guild_card_number = 0;
    pstring<TextEncoding::UTF16_ALWAYS_MARKED, 0x10> name;
  } __packed_ws__(Entry, 0x2C);
  // Variable-length field:
  // Entry entries[entry_count];
} __packed_ws__(S_TeamMemberList_BB_09EA, 4);

// 0CEA (S->C): Unknown
// The client appears to ignore this command.

struct S_Unknown_BB_0CEA {
  parray<uint8_t, 0x20> unknown_a1;
  // Text follows here
} __packed_ws__(S_Unknown_BB_0CEA, 0x20);

// 0DEA (C->S): Get team name
// No arguments

// 0EEA (S->C): Team name

struct S_TeamName_BB_0EEA {
  parray<uint8_t, 0x10> unused;
  pstring<TextEncoding::UTF16_ALWAYS_MARKED, 0x10> team_name;
} __packed_ws__(S_TeamName_BB_0EEA, 0x30);

// 0FEA (C->S): Set team flag
// The client also accepts this command but completely ignores it.

struct C_SetTeamFlag_BB_0FEA {
  parray<le_uint16_t, 0x20 * 0x20> flag_data;
} __packed_ws__(C_SetTeamFlag_BB_0FEA, 0x800);

// 10EA: Delete team (C->S) and result (S->C)
// No arguments (C->S)
// No arguments except header.flag (S->C)

// 11EA: Change team member privilege level
// The format below is used only when the client sends this command; when the
// server sends it, only header.flag is used. As with various other team
// commands, header.flag specifies the error code in this case.
// header.flag specifies the new privilege level for the specified team member.
// Known values: 0 = normal, 0x30 = leader, 0x40 = master

struct C_ChangeTeamMemberPrivilegeLevel_BB_11EA {
  le_uint32_t guild_card_number = 0;
} __packed_ws__(C_ChangeTeamMemberPrivilegeLevel_BB_11EA, 4);

// 12EA (S->C): Team membership information
// If the client is not in a team, all fields should be zero.

struct S_TeamMembershipInformation_BB_12EA {
  le_uint32_t unknown_a1 = 0;
  le_uint32_t guild_card_number = 0;
  le_uint32_t team_id = 0;
  le_uint32_t unknown_a4 = 0;
  le_uint32_t unknown_a6 = 0;
  uint8_t privilege_level = 0;
  uint8_t team_member_count = 0;
  uint8_t unknown_a8 = 0;
  uint8_t unknown_a9 = 0;
  pstring<TextEncoding::UTF16_ALWAYS_MARKED, 0x10> team_name;
} __packed_ws__(S_TeamMembershipInformation_BB_12EA, 0x38);

// 13EA: Team info for lobby players
// header.flag specifies the number of entries.

struct S_TeamInfoForPlayer_BB_13EA_15EA_Entry {
  // The client uses the first four of these to determine if the player is in a
  // team or not - if they are all zero, the player is not in a team.
  /* 0000 */ le_uint32_t guild_card_number = 0;
  /* 0004 */ le_uint32_t team_id = 0;
  /* 0008 */ le_uint32_t reward_flags = 0;
  /* 000C */ le_uint32_t unknown_a6 = 0;
  /* 0010 */ uint8_t privilege_level = 0;
  /* 0011 */ uint8_t team_member_count = 0;
  /* 0012 */ uint8_t unknown_a8 = 0;
  /* 0013 */ uint8_t unknown_a9 = 0;
  /* 0014 */ pstring<TextEncoding::UTF16_ALWAYS_MARKED, 0x10> team_name;
  /* 0034 */ le_uint32_t guild_card_number2 = 0;
  /* 0038 */ le_uint32_t lobby_client_id = 0;
  /* 003C */ pstring<TextEncoding::UTF16_ALWAYS_MARKED, 0x10> player_name;
  /* 005C */ parray<le_uint16_t, 0x20 * 0x20> flag_data;
  /* 085C */
} __packed_ws__(S_TeamInfoForPlayer_BB_13EA_15EA_Entry, 0x85C);

// 14EA (C->S): Get team info for lobby players
// No arguments. Client always sends 1 in the header.flag field.

// 15EA (S->C): Team info for lobby players
// header.flag specifies the number of entries. The entry format appears to be
// the same as for the 13EA command.

// 16EA (S->C): Transfer item via Simple Mail result
// No arguments except header.flag, which is 0 if the transfer failed and
// nonzero if it succeeded.

// 18EA: Intra-team ranking information
// No arguments (C->S)

struct S_IntraTeamRanking_BB_18EA {
  /* 0000 */ le_uint32_t ranking_points = 0;
  /* 0004 */ le_uint32_t unknown_a2 = 0;
  /* 0008 */ le_uint32_t points_remaining = 0;
  /* 000C */ le_uint32_t num_entries = 1;
  struct Entry {
    /* 00 */ le_uint32_t rank = 0;
    /* 04 */ le_uint32_t privilege_level = 0;
    /* 08 */ le_uint32_t guild_card_number = 0;
    /* 0C */ pstring<TextEncoding::UTF16_ALWAYS_MARKED, 0x10> player_name;
    /* 2C */ le_uint32_t points = 0;
    /* 30 */
  } __packed_ws__(Entry, 0x30);
  // Variable-length field:
  /* 0010 */ // Entry entries[num_entries];
} __packed_ws__(S_IntraTeamRanking_BB_18EA, 0x10);

// 19EA: Team reward list
// No arguments (C->S)

struct S_TeamRewardList_BB_19EA_1AEA {
  le_uint32_t num_entries;
  struct Entry {
    /* 0000 */ pstring<TextEncoding::UTF16, 0x40> name;
    /* 0080 */ pstring<TextEncoding::UTF16, 0x80> description;
    /* 0180 */ le_uint32_t team_points = 0;
    /* 0184 */ le_uint32_t reward_id = 0;
    /* 0188 */
  } __packed_ws__(Entry, 0x188);
  // Variable length field:
  // Entry entries[num_entries];
} __packed_ws__(S_TeamRewardList_BB_19EA_1AEA, 4);

// 1AEA: Team rewards available for purchase
// Same format as 19EA.

// 1BEA (C->S): Buy team reward
// No arguments except header.flag, which specifies a reward_id from a preceding
// 1AEA command.

// 1CEA: Cross-team ranking information
// No arguments when sent by the client.

struct S_CrossTeamRanking_BB_1CEA {
  le_uint32_t num_entries;
  struct Entry {
    /* 00 */ pstring<TextEncoding::UTF16_ALWAYS_MARKED, 0x10> team_name;
    /* 20 */ le_uint32_t team_points = 0;
    /* 24 */ le_uint32_t unknown_a1 = 0;
    /* 28 */
  } __packed_ws__(Entry, 0x28);
  // Variable length field:
  // Entry entries[num_entries];
} __packed_ws__(S_CrossTeamRanking_BB_1CEA, 4);

// 1DEA (S->C): Update team rewards bitmask
// header.flag specifies the new rewards bitmask.

// 1EEA (C->S): Rename team
// header.flag is used, but it's unknown what the value means.

struct C_RenameTeam_BB_1EEA {
  pstring<TextEncoding::UTF16_ALWAYS_MARKED, 0x10> new_team_name;
} __packed_ws__(C_RenameTeam_BB_1EEA, 0x20);

// 1FEA (S->C): Rename team result
// This command behaves like 02EA, but is sent in response to 1EEA instead.

// 20EA: Unknown
// header.flag is used, but no other arguments. When sent by the server,
// header.flag is an error code, similar to various other result commands in
// this section.

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
  pstring<TextEncoding::ASCII, 0x40> filename;
} __packed_ws__(S_StreamFileIndexEntry_BB_01EB, 0x4C);

// 02EB (S->C): Send stream file chunk (BB)

struct S_StreamFileChunk_BB_02EB {
  le_uint32_t chunk_index = 0;
  parray<uint8_t, 0x6800> data;
} __packed_ws__(S_StreamFileChunk_BB_02EB, 0x6804);

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
} __packed_ws__(C_LeaveCharacterSelect_BB_00EC, 4);

// ED (S->C): Force leave lobby/game (Episode 3)
// No arguments
// This command forces the client out of the game or lobby they're currently in
// and sends them to the lobby. If the client is in a lobby (and not a game),
// the client sends a 98 in response as if they were in a game. Curiously, the
// client also sends a meseta transaction (BA) with a value of zero before
// sending an 84 to be added to a lobby. This is used when a spectator team is
// disbanded because the target game ends.

// ED (C->S): Update save file data (BB)
// There are several subcommands (noted in the structs below) that each update a
// specific kind of data.

struct C_UpdateOptionFlags_BB_01ED {
  le_uint32_t option_flags = 0;
} __packed_ws__(C_UpdateOptionFlags_BB_01ED, 4);

struct C_UpdateSymbolChats_BB_02ED {
  parray<SaveFileSymbolChatEntryBB, 12> symbol_chats;
} __packed_ws__(C_UpdateSymbolChats_BB_02ED, 0x4E0);

struct C_UpdateChatShortcuts_BB_03ED {
  parray<SaveFileShortcutEntryBB, 0x10> chat_shortcuts;
} __packed_ws__(C_UpdateChatShortcuts_BB_03ED, 0xA40);

struct C_UpdateKeyConfig_BB_04ED {
  // TODO: Define this structure instead of treating it as raw data
  parray<uint8_t, 0x16C> key_config;
} __packed_ws__(C_UpdateKeyConfig_BB_04ED, 0x16C);

struct C_UpdatePadConfig_BB_05ED {
  // TODO: Define this structure instead of treating it as raw data
  parray<uint8_t, 0x38> pad_config;
} __packed_ws__(C_UpdatePadConfig_BB_05ED, 0x38);

struct C_UpdateTechMenu_BB_06ED {
  parray<le_uint16_t, 0x14> tech_menu;
} __packed_ws__(C_UpdateTechMenu_BB_06ED, 0x28);

struct C_UpdateCustomizeMenu_BB_07ED {
  // TODO: Define this structure instead of treating it as raw data
  parray<uint8_t, 0xE8> customize;
} __packed_ws__(C_UpdateCustomizeMenu_BB_07ED, 0xE8);

struct C_UpdateChallengeRecords_BB_08ED {
  PlayerRecordsChallengeBB records;
} __packed_ws__(C_UpdateChallengeRecords_BB_08ED, 0x140);

// EE: Trade cards (Episode 3)
// This command has different forms depending on the header.flag value; the flag
// values match the command numbers from the Episodes 1&2 trade window sequence.
// The sequence of events with the EE command also matches that of the Episodes
// 1&2 trade window; see the description of the D0 command above for details.

// EE D0 (C->S): Begin trade
struct SC_TradeCards_Ep3_EE_FlagD0_FlagD3 {
  le_uint16_t target_client_id = 0;
  le_uint16_t entry_count = 0;
  struct Entry {
    le_uint32_t card_type = 0;
    le_uint32_t count = 0;
  } __packed_ws__(Entry, 8);
  parray<Entry, 4> entries;
} __packed_ws__(SC_TradeCards_Ep3_EE_FlagD0_FlagD3, 0x24);

// EE D1 (S->C): Advance trade state
struct S_AdvanceCardTradeState_Ep3_EE_FlagD1 {
  le_uint32_t unused = 0;
} __packed_ws__(S_AdvanceCardTradeState_Ep3_EE_FlagD1, 4);

// EE D2 (C->S): Trade can proceed
// No arguments

// EE D3 (S->C): Execute trade
// Same format as EE D0

// EE D4 (C->S): Trade failed
// EE D4 (S->C): Trade complete

struct S_CardTradeComplete_Ep3_EE_FlagD4 {
  le_uint32_t success = 0; // 0 = failed, 1 = success, anything else = invalid
} __packed_ws__(S_CardTradeComplete_Ep3_EE_FlagD4, 4);

// EE (S->C): Scrolling message (BB)
// Same format as 01. The message appears at the top of the screen and slowly
// scrolls to the left. The maximum length of the message is 0x400 bytes (0x200
// UTF-16 characters).

// EF (C->S): Join card auction (Episode 3)
// When a card auction is ready to begin, the leader sends this command to
// request the card list. The server then sends an EF command to all players
// to start the auction.

// EF (S->C): Start card auction (Episode 3)

struct S_StartCardAuction_Ep3_EF {
  le_uint16_t points_available = 0;
  le_uint16_t unused = 0;
  struct Entry {
    le_uint16_t card_id = 0xFFFF; // Must be < 0x02F1
    le_uint16_t min_price = 0; // Must be > 0 and < 100
  } __packed_ws__(Entry, 4);
  parray<Entry, 0x14> entries;
} __packed_ws__(S_StartCardAuction_Ep3_EF, 0x54);

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
  pstring<TextEncoding::ASCII, 0x200> command;
} __packed_ws__(S_SetShutdownCommand_BB_01EF, 0x200);

// F0 (S->C): Force update player lobby data (BB)
// Format is PlayerLobbyDataBB (in PlayerSubordinates.hh). This command
// overwrites the lobby data for the player given by .client_id without
// reloading the game or lobby.

// This command probably exists to handle cases like the following:
// 1. Player A is in a team and is not the team master. Player A creates a game.
// 2. The master of the team changes to player B during this game.
// 3. Player B then joins the game that A is in.
// Some effects (e.g. Commander Blade) depend on the team master ID in the
// PlayerLobbyDataBB structure, and this is the only way to update that
// structure without reloading the lobby or game. If this command did not exist,
// then player A would not know that B was the master when they join the game,
// so A would not see the bonus from Commander Blade if B uses it.

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

// Some subcommands are "protected" on V3 and later (not including GC NTE);
// these commands are blocked by the client if they affect the local player. If
// a V3 or later client receives a protected subcommand that would affect its
// own player, it instead ignores the entire subcommand. This means that the
// server or other players cannot send these subcommands to affect other
// players; they can only send these commands to inform other clients about
// changes or actions from their own player.
// The protected subcommands are marked (protected) in the listings below.

// These common structures are used my many subcommands.
struct G_ClientIDHeader {
  uint8_t subcommand = 0;
  uint8_t size = 0;
  le_uint16_t client_id = 0; // <= 12
} __packed_ws__(G_ClientIDHeader, 4);
struct G_EntityIDHeader {
  uint8_t subcommand = 0;
  uint8_t size = 0;
  le_uint16_t entity_id = 0; // 0-B=client, 1000-3FFF=enemy, 4000-FFFF=object
} __packed_ws__(G_EntityIDHeader, 4);
struct G_ParameterHeader {
  uint8_t subcommand = 0;
  uint8_t size = 0;
  le_uint16_t param = 0;
} __packed_ws__(G_ParameterHeader, 4);
struct G_UnusedHeader {
  uint8_t subcommand = 0;
  uint8_t size = 0;
  le_uint16_t unused = 0;
} __packed_ws__(G_UnusedHeader, 4);

template <typename HeaderT>
struct G_ExtendedHeaderT {
  HeaderT basic_header;
  le_uint32_t size = 0;
} __packed__;

// 6x00: Invalid subcommand
// 6x01: Invalid subcommand

// 6x02: Unknown
// 6x03: Unknown
// These subcommands are completely ignored on V3 and later.
// On all known DC versions (NTE through V2), these commands writes their
// contents to a global array, but nothing reads from this array.

struct G_Unknown_6x02_6x03 {
  G_ClientIDHeader header;
  le_uint16_t unknown_a1;
  le_uint16_t unknown_a2;
  le_uint32_t unknown_a3;
  le_float unknown_a4;
  le_float unknown_a5;
} __packed_ws__(G_Unknown_6x02_6x03, 0x14);

// 6x04: Unknown
// TODO: This does something with TObjDoorKey objects

struct G_Unknown_6x04 {
  G_ParameterHeader header; // param = door token (NOT entity ID or index)
  le_uint16_t unused1 = 0;
  le_uint16_t unused2 = 0;
} __packed_ws__(G_Unknown_6x04, 8);

// 6x05: Switch state changed
// Some things that don't look like switches are implemented as switches using
// this subcommand. For example, when all enemies in a room are defeated, this
// subcommand is used to unlock the doors.
// Note: In the client, this is a subclass of 6x04, similar to how 6xA2 is a
// subclass of 6x60.

struct G_SwitchStateChanged_6x05 {
  // Note: header.object_id is 0xFFFF for room clear when all enemies defeated
  G_EntityIDHeader header;
  // TODO: Some of these might be big-endian on GC; it only byteswaps
  // switch_flag_num. Are the others actually uint16, or are they uint8[2]?
  le_uint16_t client_id = 0;
  le_uint16_t unknown_a2 = 0;
  le_uint16_t switch_flag_num = 0;
  uint8_t switch_flag_floor = 0;
  // Only two bits in flags have meanings:
  // 01 - set unlock flag (if not set, the flag is cleared instead)
  // 02 - play room unlock sound if floor matches client's floor
  uint8_t flags = 0;
} __packed_ws__(G_SwitchStateChanged_6x05, 0x0C);

// 6x06: Send guild card

struct G_SendGuildCard_DCNTE_6x06 {
  G_UnusedHeader header;
  GuildCardDCNTE guild_card;
  uint8_t unused;
} __packed_ws__(G_SendGuildCard_DCNTE_6x06, 0x80);

struct G_SendGuildCard_DC_6x06 {
  G_UnusedHeader header;
  GuildCardDC guild_card;
  parray<uint8_t, 3> unused;
} __packed_ws__(G_SendGuildCard_DC_6x06, 0x84);

struct G_SendGuildCard_PC_6x06 {
  G_UnusedHeader header;
  GuildCardPC guild_card;
} __packed_ws__(G_SendGuildCard_PC_6x06, 0xF4);

struct G_SendGuildCard_GCNTE_6x06 {
  G_UnusedHeader header;
  GuildCardGCNTE guild_card;
} __packed_ws__(G_SendGuildCard_GCNTE_6x06, 0xA8);

struct G_SendGuildCard_GC_6x06 {
  G_UnusedHeader header;
  GuildCardGC guild_card;
} __packed_ws__(G_SendGuildCard_GC_6x06, 0x94);

struct G_SendGuildCard_XB_6x06 {
  G_UnusedHeader header;
  GuildCardXB guild_card;
} __packed_ws__(G_SendGuildCard_XB_6x06, 0x230);

struct G_SendGuildCard_BB_6x06 {
  G_UnusedHeader header;
  GuildCardBB guild_card;
} __packed_ws__(G_SendGuildCard_BB_6x06, 0x10C);

// 6x07: Symbol chat
// If UDP mode is enabled, this command is sent via UDP.

struct G_SymbolChat_6x07 {
  G_UnusedHeader header;
  le_uint32_t client_id = 0;
  SymbolChat data;
} __packed_ws__(G_SymbolChat_6x07, 0x44);

// 6x08: Invalid subcommand

// 6x09: Unknown

struct G_Unknown_6x09 {
  G_EntityIDHeader header;
} __packed_ws__(G_Unknown_6x09, 4);

// 6x0A: Update enemy state

template <bool BE>
struct G_UpdateEnemyStateT_6x0A {
  G_EntityIDHeader header;
  le_uint16_t enemy_index = 0; // [0, 0xB50)
  le_uint16_t total_damage = 0;
  // Flags:
  // 00000400 - should play hit animation
  // 00000800 - is dead
  typename std::conditional_t<BE, be_uint32_t, le_uint32_t> flags = 0;
} __packed__;
using G_UpdateEnemyState_GC_6x0A = G_UpdateEnemyStateT_6x0A<true>;
using G_UpdateEnemyState_DC_PC_XB_BB_6x0A = G_UpdateEnemyStateT_6x0A<false>;
check_struct_size(G_UpdateEnemyState_GC_6x0A, 0x0C);
check_struct_size(G_UpdateEnemyState_DC_PC_XB_BB_6x0A, 0x0C);

// 6x0B: Update object state

struct G_UpdateObjectState_6x0B {
  G_EntityIDHeader header;
  le_uint32_t flags = 0;
  le_uint32_t object_index = 0;
} __packed_ws__(G_UpdateObjectState_6x0B, 0x0C);

// 6x0C: Add status effect (poison/slow/etc.) (protected on V3/V4)

struct G_AddStatusEffect_6x0C {
  G_ClientIDHeader header;
  // Each status effect has an assigned slot; there are 5 slots and each slot
  // may only hold one effect at a time. (The last slot, slot 4, is unused.)
  // If a new status effect is added to a slot that already contains one, the
  // existing status effect is replaced. Non-technique status effects have
  // fixed or indefinite durations; technique-based effects have durations
  // based on the technique's level.
  // Values for effect_type:
  // 02 = Freeze (slot 1; 5 seconds)
  // 03 = Shock (slot 1; 10 seconds)
  // 07 = Clears negative status effects (healing ring?)
  // 09 = Shifta (slot 2; ((lv * 10) + 30) seconds; ATP (8.3 + 1.3 * lv)%)
  // 0A = Deband (slot 3; ((lv * 10) + 30) seconds; DFP (8.3 + 1.3 * lv)%)
  // 0B = Jellen (slot 2; ((lv * 10) + 30) seconds; ATP (8.3 + 1.3 * lv)%)
  // 0C = Zalure (slot 3; ((lv * 10) + 30) seconds; DFP (8.3 + 1.3 * lv)%)
  // 0F = Poison (slot 0)
  // 10 = Paralysis (slot 0; 7 seconds for enemies, indefinite for players)
  // 11 = Slow (slot 1; 7 seconds)
  // 12 = Confuse (slot 1; 10 seconds)
  // Anything else = command is ignored
  le_uint32_t effect_type = 0;
  le_uint32_t level = 0; // Only used for Shifta/Deband/Jellen/Zalure
} __packed_ws__(G_AddStatusEffect_6x0C, 0x0C);

// 6x0D: Clear status effect slot (protected on V3/V4)

struct G_RemoveStatusEffect_6x0D {
  G_ClientIDHeader header;
  le_uint32_t slot = 0; // See 6x0C description for slot values
  le_uint32_t unused = 0;
} __packed_ws__(G_RemoveStatusEffect_6x0D, 0x0C);

// 6x0E: Unknown (protected on V3/V4)

struct G_Unknown_6x0E {
  G_ClientIDHeader header;
} __packed_ws__(G_Unknown_6x0E, 4);

// 6x0F: Invalid subcommand

// 6x10: Unknown (not valid on Episode 3)

struct G_Unknown_6x10_6x11_6x14 {
  G_EntityIDHeader header;
  le_uint16_t unknown_a2 = 0;
  le_uint16_t unknown_a3 = 0;
  le_uint32_t unknown_a4 = 0;
} __packed_ws__(G_Unknown_6x10_6x11_6x14, 0x0C);

// 6x11: Unknown (not valid on Episode 3)
// Same format as 6x10

// 6x12: Dragon boss actions (not valid on Episode 3)

template <bool BE>
struct G_DragonBossActionsT_6x12 {
  G_EntityIDHeader header;
  le_uint16_t unknown_a2 = 0;
  le_uint16_t unknown_a3 = 0;
  le_uint32_t unknown_a4 = 0;
  F32T<BE> x = 0.0f;
  F32T<BE> z = 0.0f;
} __packed__;
using G_DragonBossActions_DC_PC_XB_BB_6x12 = G_DragonBossActionsT_6x12<false>;
using G_DragonBossActions_GC_6x12 = G_DragonBossActionsT_6x12<true>;
check_struct_size(G_DragonBossActions_DC_PC_XB_BB_6x12, 0x14);
check_struct_size(G_DragonBossActions_GC_6x12, 0x14);

// 6x13: De Rol Le boss actions (not valid on Episode 3)

struct G_DeRolLeBossActions_6x13 {
  G_EntityIDHeader header;
  le_uint16_t unknown_a2 = 0;
  le_uint16_t unknown_a3 = 0;
} __packed_ws__(G_DeRolLeBossActions_6x13, 8);

// 6x14: De Rol Le boss actions (not valid on Episode 3)
// Same format as 6x10

// 6x15: Vol Opt boss actions (not valid on Episode 3)

struct G_VolOptBossActions_6x15 {
  G_EntityIDHeader header;
  le_uint16_t unknown_a2 = 0;
  le_uint16_t unknown_a3 = 0;
  le_uint16_t unknown_a4 = 0;
  le_uint16_t unknown_a5 = 0;
} __packed_ws__(G_VolOptBossActions_6x15, 0x0C);

// 6x16: Vol Opt boss actions (not valid on Episode 3)

struct G_VolOptBossActions_6x16 {
  G_EntityIDHeader header;
  parray<uint8_t, 6> unknown_a2;
  le_uint16_t unknown_a3 = 0;
} __packed_ws__(G_VolOptBossActions_6x16, 0x0C);

// 6x17: Vol Opt phase 2 boss actions (not valid on Episode 3)

struct G_VolOpt2BossActions_6x17 {
  G_EntityIDHeader header;
  le_float unknown_a2 = 0.0f;
  le_float unknown_a3 = 0.0f;
  le_float unknown_a4 = 0.0f;
  le_uint32_t unknown_a5 = 0;
} __packed_ws__(G_VolOpt2BossActions_6x17, 0x14);

// 6x18: Vol Opt phase 2 boss actions (not valid on Episode 3)

struct G_VolOpt2BossActions_6x18 {
  G_EntityIDHeader header;
  parray<le_uint16_t, 4> unknown_a2;
} __packed_ws__(G_VolOpt2BossActions_6x18, 0x0C);

// 6x19: Dark Falz boss actions (not valid on Episode 3)

struct G_DarkFalzActions_6x19 {
  G_EntityIDHeader header;
  le_uint16_t unknown_a2 = 0;
  le_uint16_t unknown_a3 = 0;
  le_uint32_t unknown_a4 = 0;
  le_uint32_t unused = 0;
} __packed_ws__(G_DarkFalzActions_6x19, 0x10);

// 6x1A: Invalid subcommand

// 6x1B: Enable PK mode for player (not valid on Episode 3) (protected on V3/V4)

struct G_EnablePKModeForPlayer_6x1B {
  G_ClientIDHeader header;
} __packed_ws__(G_EnablePKModeForPlayer_6x1B, 4);

// 6x1C: Disable PK mode for player (protected on V3/V4)

struct G_DisablePKModeForPlayer_6x1C {
  G_ClientIDHeader header;
} __packed_ws__(G_DisablePKModeForPlayer_6x1C, 4);

// 6x1D: Invalid subcommand
// 6x1E: Invalid subcommand

// 6x1F: Set player floor and request positions

struct G_SetPlayerFloor_DCNTE_6x1F {
  G_ClientIDHeader header;
} __packed_ws__(G_SetPlayerFloor_DCNTE_6x1F, 4);

struct G_SetPlayerFloor_6x1F {
  G_ClientIDHeader header;
  le_int32_t floor = 0;
} __packed_ws__(G_SetPlayerFloor_6x1F, 8);

// 6x20: Set position (protected on V3/V4)
// Existing clients send this in response to a 6x1F command when a new client
// joins a lobby or game, so the new client knows where to place them.

struct G_SetPosition_6x20 {
  G_ClientIDHeader header;
  le_int32_t floor = 0;
  VectorXYZF pos;
  le_uint32_t unknown_a1 = 0;
} __packed_ws__(G_SetPosition_6x20, 0x18);

// 6x21: Inter-level warp (protected on V3/V4)

struct G_InterLevelWarp_6x21 {
  G_ClientIDHeader header;
  le_int32_t floor = 0;
} __packed_ws__(G_InterLevelWarp_6x21, 8);

// 6x22: Set player invisible (protected on V3/V4)
// 6x23: Set player visible (protected on V3/V4)
// These are generally used while a player is in the process of changing floors.

struct G_SetPlayerVisibility_6x22_6x23 {
  G_ClientIDHeader header;
} __packed_ws__(G_SetPlayerVisibility_6x22_6x23, 4);

// 6x24: Teleport player (protected on V3/V4)

struct G_TeleportPlayer_6x24 {
  G_ClientIDHeader header;
  le_uint32_t unknown_a1 = 0;
  VectorXYZF pos;
} __packed_ws__(G_TeleportPlayer_6x24, 0x14);

// 6x25: Equip item (protected on V3/V4)

struct G_EquipItem_6x25 {
  G_ClientIDHeader header;
  le_uint32_t item_id = 0;
  // Values here match the EquipSlot enum (in ItemData.hh)
  le_uint32_t equip_slot = 0;
} __packed_ws__(G_EquipItem_6x25, 0x0C);

// 6x26: Unequip item (protected on V3/V4)

struct G_UnequipItem_6x26 {
  G_ClientIDHeader header;
  le_uint32_t item_id = 0;
  le_uint32_t unused = 0;
} __packed_ws__(G_UnequipItem_6x26, 0x0C);

// 6x27: Use item (protected on V3/V4)

struct G_UseItem_6x27 {
  G_ClientIDHeader header;
  le_uint32_t item_id = 0;
} __packed_ws__(G_UseItem_6x27, 8);

// 6x28: Feed MAG (protected on V3/V4)

struct G_FeedMAG_6x28 {
  G_ClientIDHeader header;
  le_uint32_t mag_item_id = 0;
  le_uint32_t fed_item_id = 0;
} __packed_ws__(G_FeedMAG_6x28, 0x0C);

// 6x29: Delete inventory item (via bank deposit / sale / feeding MAG) (protected on V3 but not V4)
// This subcommand is also used for reducing the size of stacks - if amount is
// less than the stack count, the item is not deleted and its ID remains valid.

struct G_DeleteInventoryItem_6x29 {
  G_ClientIDHeader header;
  le_uint32_t item_id = 0;
  le_uint32_t amount = 0;
} __packed_ws__(G_DeleteInventoryItem_6x29, 0x0C);

// 6x2A: Drop item (protected on V3/V4)

struct G_DropItem_6x2A {
  G_ClientIDHeader header;
  le_uint16_t amount = 0;
  le_uint16_t floor = 0;
  le_uint32_t item_id = 0;
  VectorXYZF pos;
} __packed_ws__(G_DropItem_6x2A, 0x18);

// 6x2B: Create item in inventory (e.g. via tekker or bank withdraw) (protected on V3/V4)
// On BB, the 6xBE command is used instead of 6x2B to create inventory items.

struct G_CreateInventoryItem_DC_6x2B {
  G_ClientIDHeader header;
  ItemData item_data;
} __packed_ws__(G_CreateInventoryItem_DC_6x2B, 0x18);

struct G_CreateInventoryItem_PC_V3_BB_6x2B : G_CreateInventoryItem_DC_6x2B {
  uint8_t unused1 = 0;
  uint8_t unknown_a2 = 0; // Does something with equipped items, but logic isn't the same as 6x25
  parray<uint8_t, 2> unused2 = 0;
} __packed_ws__(G_CreateInventoryItem_PC_V3_BB_6x2B, 0x1C);

// 6x2C: Talk to NPC (protected on V3/V4)

struct G_TalkToNPC_6x2C {
  G_ClientIDHeader header;
  le_uint16_t unknown_a1 = 0;
  le_uint16_t unknown_a2 = 0;
  le_float unknown_a3 = 0.0f;
  le_float unknown_a4 = 0.0f;
  le_float unknown_a5 = 0.0f;
} __packed_ws__(G_TalkToNPC_6x2C, 0x14);

// 6x2D: Done talking to NPC (protected on V3/V4)

struct G_EndTalkToNPC_6x2D {
  G_ClientIDHeader header;
} __packed_ws__(G_EndTalkToNPC_6x2D, 4);

// 6x2E: Set and/or clear player flags (protected on V3/V4)

struct G_SetOrClearPlayerFlags_6x2E {
  G_ClientIDHeader header;
  le_uint32_t and_mask = 0;
  le_uint32_t or_mask = 0;
} __packed_ws__(G_SetOrClearPlayerFlags_6x2E, 0x0C);

// 6x2F: Change player HP

struct G_ChangePlayerHP_6x2F {
  G_UnusedHeader header;
  le_uint32_t type = 0; // 0 = set HP, 1 = add/subtract HP, 2 = add/sub fixed HP
  le_uint16_t amount = 0;
  le_uint16_t client_id = 0;
} __packed_ws__(G_ChangePlayerHP_6x2F, 0x0C);

// 6x30: Change player level
// On DC NTE, the updated stats aren't sent, and the client may only gain a
// single level at once. On other versions, this is not the case.

struct G_ChangePlayerLevel_DCNTE_6x30 {
  G_ClientIDHeader header;
} __packed_ws__(G_ChangePlayerLevel_DCNTE_6x30, 4);

struct G_ChangePlayerLevel_6x30 {
  G_ClientIDHeader header;
  le_uint16_t atp = 0;
  le_uint16_t mst = 0;
  le_uint16_t evp = 0;
  le_uint16_t hp = 0;
  le_uint16_t dfp = 0;
  le_uint16_t ata = 0;
  le_uint16_t level = 0;
  le_uint16_t is_level_down = 0; // If == 1, the text "Level Down" appears instead of "Level Up"
} __packed_ws__(G_ChangePlayerLevel_6x30, 0x14);

// 6x31: Use medical center (protected on V3/V4)

struct G_UseMedicalCenter_6x31 {
  G_ClientIDHeader header;
} __packed_ws__(G_UseMedicalCenter_6x31, 4);

// 6x32: Revive player (Medical Center)

struct G_MedicalCenterRevivePlayer_6x32 {
  G_UnusedHeader header;
} __packed_ws__(G_MedicalCenterRevivePlayer_6x32, 4);

// 6x33: Revive player (with Moon Atomizer) (protected on V3/V4)

struct G_RevivePlayer_6x33 {
  G_ClientIDHeader header;
  le_uint16_t client_id2 = 0;
  le_uint16_t unused = 0;
} __packed_ws__(G_RevivePlayer_6x33, 8);

// 6x34: Unknown
// This subcommand is completely ignored (at least, by PSO GC).

// 6x35: Invalid subcommand

// 6x36: Unknown (supported; game only)
// This subcommand is completely ignored (at least, by PSO GC).

// 6x37: Photon blast (protected on V3/V4)

struct G_PhotonBlast_6x37 {
  G_ClientIDHeader header;
  le_uint16_t unknown_a1 = 0;
  le_uint16_t unused = 0;
} __packed_ws__(G_PhotonBlast_6x37, 8);

// 6x38: Donate to photon blast (protected on V3/V4)

struct G_DonateToPhotonBlast_6x38 {
  G_ClientIDHeader header;
  le_uint16_t unknown_a1 = 0;
  le_uint16_t unused = 0;
} __packed_ws__(G_DonateToPhotonBlast_6x38, 8);

// 6x39: Photon blast ready (protected on V3/V4)

struct G_PhotonBlastReady_6x38 {
  G_ClientIDHeader header;
} __packed_ws__(G_PhotonBlastReady_6x38, 4);

// 6x3A: Unknown (supported; game only) (protected on V3/V4)

struct G_Unknown_6x3A {
  G_ClientIDHeader header;
} __packed_ws__(G_Unknown_6x3A, 4);

// 6x3B: Unknown (supported; lobby & game) (protected on V3/V4)

struct G_Unknown_6x3B {
  G_ClientIDHeader header;
} __packed_ws__(G_Unknown_6x3B, 4);

// 6x3C: Unknown (DCv1 and earlier)
// This command has a handler, but it does nothing, even on DC NTE.

// 6x3D: Invalid subcommand

// 6x3E: Stop moving (protected on V3/V4)

struct G_StopAtPosition_6x3E {
  G_ClientIDHeader header;
  le_uint16_t unknown_a1 = 0;
  le_uint16_t angle = 0;
  le_int16_t floor = 0;
  le_int16_t room = 0;
  VectorXYZF pos;
} __packed_ws__(G_StopAtPosition_6x3E, 0x18);

// 6x3F: Set position (protected on V3/V4)

struct G_SetPosition_6x3F {
  G_ClientIDHeader header;
  le_uint16_t unknown_a1 = 0;
  le_uint16_t angle = 0;
  le_int16_t floor = 0;
  le_int16_t room = 0;
  VectorXYZF pos;
} __packed_ws__(G_SetPosition_6x3F, 0x18);

// 6x40: Walk (protected on V3/V4)
// If UDP mode is enabled, this command is sent via UDP.

struct G_WalkToPosition_6x40 {
  G_ClientIDHeader header;
  VectorXZF pos;
  le_uint32_t action = 0;
} __packed_ws__(G_WalkToPosition_6x40, 0x10);

// 6x41: Move to position (v1)
// 6x42: Run (protected on V3/V4)
// This subcommand is completely ignored by v2 and later.
// If UDP mode is enabled, this command is sent via UDP.

struct G_MoveToPosition_6x41_6x42 {
  G_ClientIDHeader header;
  VectorXZF pos;
} __packed_ws__(G_MoveToPosition_6x41_6x42, 0x0C);

// 6x43: First attack (protected on V3/V4)
// 6x44: Second attack (protected on V3/V4)
// 6x45: Third attack (protected on V3/V4)
// If UDP mode is enabled, these commands are sent via UDP.

struct G_Attack_6x43_6x44_6x45 {
  G_ClientIDHeader header;
  le_uint16_t unknown_a1 = 0;
  le_uint16_t unknown_a2 = 0;
} __packed_ws__(G_Attack_6x43_6x44_6x45, 8);

// 6x46: Attack finished (sent after each of 43, 44, and 45) (protected on V3/V4)
// The number of targets is not bounds-checked during byteswapping on GC
// clients. The client only expects up to 10 entries here, so if the number of
// targets is too large, the client will byteswap the function's return address
// on the stack, and it will crash.

struct TargetEntry {
  le_uint16_t entity_id = 0;
  le_uint16_t unknown_a2 = 0;
} __packed_ws__(TargetEntry, 4);

struct G_AttackFinished_6x46 {
  G_ClientIDHeader header;
  le_uint32_t target_count = 0;
  // The client may send a shorter command if not all of these are used.
  parray<TargetEntry, 10> targets;
} __packed_ws__(G_AttackFinished_6x46, 0x30);

// 6x47: Cast technique (protected on V3/V4)
// On GC, this command has the same bounds-check bug as 6x46.

struct G_CastTechnique_6x47 {
  G_ClientIDHeader header;
  uint8_t technique_number = 0;
  uint8_t unused = 0; // Must not be negative
  // Note: The level here isn't the actual tech level that was cast, if the
  // actual level is > 15. In that case, a 6x8D is sent first, which contains
  // the additional level which is added to this level at cast time. They
  // probably did this for legacy reasons when dealing with v1/v2
  // compatibility, and never cleaned it up.
  uint8_t level = 0;
  uint8_t target_count = 0; // Must be in [0, 10]
  // The client may send a shorter command if not all of these are used.
  parray<TargetEntry, 10> targets;
} __packed_ws__(G_CastTechnique_6x47, 0x30);

// 6x48: Cast technique complete (protected on V3/V4)

struct G_CastTechniqueComplete_6x48 {
  G_ClientIDHeader header;
  le_uint16_t technique_number = 0;
  // This level matches the level sent in the 6x47 command, even if that level
  // was overridden by a preceding 6x8D command.
  le_uint16_t level = 0;
} __packed_ws__(G_CastTechniqueComplete_6x48, 8);

// 6x49: Execute Photon Blast (protected on V3/V4)
// On GC, this command has the same bounds-check bug as 6x46.

struct G_ExecutePhotonBlast_6x49 {
  G_ClientIDHeader header;
  uint8_t unknown_a1 = 0;
  uint8_t unknown_a2 = 0;
  le_uint16_t target_count = 0;
  le_uint16_t unknown_a3 = 0;
  le_uint16_t unknown_a4 = 0;
  // The client may send a shorter command if not all of these are used.
  parray<TargetEntry, 10> targets;
} __packed_ws__(G_ExecutePhotonBlast_6x49, 0x34);

// 6x4A: Fully shield attack (protected on V3/V4)

struct G_ShieldAttack_6x4A {
  G_ClientIDHeader header;
} __packed_ws__(G_ShieldAttack_6x4A, 4);

// 6x4B: Hit by enemy (protected on V3/V4)
// 6x4C: Hit by enemy (protected on V3/V4)

struct G_HitByEnemy_6x4B_6x4C {
  G_ClientIDHeader header;
  le_uint16_t angle = 0;
  le_uint16_t current_hp = 0;
  VectorXZF velocity;
} __packed_ws__(G_HitByEnemy_6x4B_6x4C, 0x10);

// 6x4D: Player died (protected on V3/V4)

struct G_PlayerDied_6x4D {
  G_ClientIDHeader header;
  le_uint32_t unknown_a1 = 0;
} __packed_ws__(G_PlayerDied_6x4D, 8);

// 6x4E: Player is dead can be revived (protected on V3/V4)

struct G_PlayerRevivable_6x4E {
  G_ClientIDHeader header;
} __packed_ws__(G_PlayerRevivable_6x4E, 4);

// 6x4F: Player revived (protected on V3/V4)

struct G_PlayerRevived_6x4F {
  G_ClientIDHeader header;
} __packed_ws__(G_PlayerRevived_6x4F, 4);

// 6x50: Switch interaction (protected on V3/V4)
// If UDP mode is enabled, this command is sent via UDP.

struct G_SwitchInteraction_6x50 {
  G_ClientIDHeader header;
  le_uint32_t unknown_a1 = 0;
} __packed_ws__(G_SwitchInteraction_6x50, 8);

// 6x51: Set player angle
// If UDP mode is enabled, this command is sent via UDP.
// This command appears to be vestigial - no version of the game has a handler
// for it (it is always ignored), but PSO GC has a function that sends it. It's
// not known if this function is ever called, or how to trigger it.

struct G_SetPlayerAngle_6x51 {
  G_ClientIDHeader header;
  le_uint16_t angle = 0;
  parray<uint8_t, 2> unused;
} __packed_ws__(G_SetPlayerAngle_6x51, 8);

// 6x52: Set animation state (protected on V3/V4)

struct G_SetAnimationState_6x52 {
  G_ClientIDHeader header;
  le_uint16_t animation = 0;
  le_uint16_t unknown_a2 = 0;
  le_uint32_t angle = 0;
} __packed_ws__(G_SetAnimationState_6x52, 0x0C);

// 6x53: Unknown (supported; game only) (protected on V3/V4)

struct G_Unknown_6x53 {
  G_ClientIDHeader header;
} __packed_ws__(G_Unknown_6x53, 4);

// 6x54: Unknown
// This subcommand is completely ignored by DCv2 and later. On DC NTE, 11/2000,
// and DCv1, the handler has some logic in it and it calls a virtual function
// on TObjPlayer, but that codepath ends up doing nothing.

struct G_Unknown_6x54 {
  G_ClientIDHeader header;
} __packed_ws__(G_Unknown_6x54, 4);

// 6x55: Intra-map warp (protected on V3/V4)

struct G_IntraMapWarp_6x55 {
  G_ClientIDHeader header;
  le_uint32_t unknown_a1 = 0;
  VectorXYZF pos1;
  VectorXYZF pos2;
} __packed_ws__(G_IntraMapWarp_6x55, 0x20);

// 6x56: Set player position and angle (protected on V3/V4)

struct G_SetPlayerPositionAndAngle_6x56 {
  G_ClientIDHeader header;
  le_uint32_t angle_y = 0;
  VectorXYZF pos;
} __packed_ws__(G_SetPlayerPositionAndAngle_6x56, 0x14);

// 6x57: Unknown (supported; lobby & game) (protected on V3/V4)

struct G_Unknown_6x57 {
  G_ClientIDHeader header;
} __packed_ws__(G_Unknown_6x57, 4);

// 6x58: Lobby animation (protected on V3/V4)
// If UDP mode is enabled, this command is sent via UDP.

struct G_LobbyAnimation_6x58 {
  G_ClientIDHeader header;
  le_uint16_t animation_number = 0;
  le_uint16_t unused = 0;
} __packed_ws__(G_LobbyAnimation_6x58, 8);

// 6x59: Pick up item

struct G_PickUpItem_6x59 {
  G_ClientIDHeader header;
  le_uint16_t client_id2 = 0;
  le_uint16_t floor = 0;
  le_uint32_t item_id = 0;
} __packed_ws__(G_PickUpItem_6x59, 0x0C);

// 6x5A: Request to pick up item

struct G_PickUpItemRequest_6x5A {
  G_ClientIDHeader header;
  le_uint32_t item_id = 0;
  le_uint16_t floor = 0;
  le_uint16_t unused = 0;
} __packed_ws__(G_PickUpItemRequest_6x5A, 0x0C);

// 6x5B: Unused (DCv1 and earlier)
// This command has a handler, but it does nothing, even on DC NTE.

// 6x5C: Destroy floor item
// Same format as 6x63. It appears this version should not be used because it
// removes the item from the floor just like 6x63 does, but 6x5C doesn't call
// the item's destructor.

// 6x5D: Drop meseta or stacked item
// On DC NTE, this command has the same format, but is subcommand 6x4F instead.

struct G_DropStackedItem_DC_6x5D {
  G_ClientIDHeader header;
  le_uint16_t floor = 0;
  le_uint16_t unknown_a2 = 0; // Corresponds to FloorItem::unknown_a2
  VectorXZF pos;
  ItemData item_data;
} __packed_ws__(G_DropStackedItem_DC_6x5D, 0x24);

struct G_DropStackedItem_PC_V3_BB_6x5D : G_DropStackedItem_DC_6x5D {
  le_uint32_t unused3 = 0;
} __packed_ws__(G_DropStackedItem_PC_V3_BB_6x5D, 0x28);

// 6x5E: Buy item at shop

struct G_BuyShopItem_6x5E {
  G_ClientIDHeader header;
  ItemData item_data;
} __packed_ws__(G_BuyShopItem_6x5E, 0x18);

// 6x5F: Drop item from box/enemy

struct FloorItem {
  /* 00 */ uint8_t floor = 0;
  /* 01 */ uint8_t source_type = 0; // 1 = enemy, 2 = box
  /* 02 */ le_uint16_t entity_index = 0; // < 0x0B50 if source_type == 1; otherwise < 0x0BA0
  /* 04 */ VectorXZF pos;
  /* 0C */ le_uint16_t unknown_a2 = 0;
  // The drop number is scoped to the floor and increments by 1 each time an
  // item is dropped. The last item dropped in each floor has drop_number equal
  // to total_items_dropped_per_floor[floor - 1] - 1.
  /* 0E */ le_uint16_t drop_number = 0;
  /* 10 */ ItemData item;
  /* 24 */
} __packed_ws__(FloorItem, 0x24);

struct G_DropItem_DC_6x5F {
  G_UnusedHeader header;
  FloorItem item;
} __packed_ws__(G_DropItem_DC_6x5F, 0x28);

struct G_DropItem_PC_V3_BB_6x5F : G_DropItem_DC_6x5F {
  le_uint32_t unused3 = 0;
} __packed_ws__(G_DropItem_PC_V3_BB_6x5F, 0x2C);

// 6x60: Request for item drop (handled by the server on BB)

struct G_StandardDropItemRequest_DC_6x60 {
  /* 00 */ G_UnusedHeader header;
  /* 04 */ uint8_t floor = 0;
  /* 05 */ uint8_t rt_index = 0;
  /* 06 */ le_uint16_t entity_index = 0;
  /* 08 */ VectorXZF pos;
  /* 10 */ le_uint16_t section = 0;
  /* 12 */ le_uint16_t ignore_def = 0;
  /* 14 */
} __packed_ws__(G_StandardDropItemRequest_DC_6x60, 0x14);

struct G_StandardDropItemRequest_PC_V3_BB_6x60 : G_StandardDropItemRequest_DC_6x60 {
  /* 14 */ uint8_t effective_area = 0;
  /* 15 */ parray<uint8_t, 3> unused;
  /* 18 */
} __packed_ws__(G_StandardDropItemRequest_PC_V3_BB_6x60, 0x18);

// 6x61: Activate MAG effect

struct G_ActivateMagEffect_6x61 {
  G_UnusedHeader header;
  le_uint32_t mag_item_id = 0;
  le_uint32_t effect_number = 0;
} __packed_ws__(G_ActivateMagEffect_6x61, 0x0C);

// 6x62: Unused
// This command has a handler, but it does nothing even on DC NTE.

// 6x63: Destroy floor item
// This is sent by the leader to destroy a floor item when there are 50 or more
// items already on the ground on the current floor.

struct G_DestroyFloorItem_6x5C_6x63 {
  G_UnusedHeader header;
  le_uint32_t item_id = 0;
  le_uint32_t floor = 0;
} __packed_ws__(G_DestroyFloorItem_6x5C_6x63, 0x0C);

// 6x64: Unused (not valid on Episode 3)
// This command has a handler, but it does nothing even on DC NTE.

// 6x65: Unused (not valid on Episode 3)
// This command has a handler, but it does nothing even on DC NTE.

// 6x66: Use star atomizer

struct G_UseStarAtomizer_6x66 {
  G_UnusedHeader header;
  parray<le_uint16_t, 4> target_client_ids;
} __packed_ws__(G_UseStarAtomizer_6x66, 0x0C);

// 6x67: Trigger set event

struct G_TriggerSetEvent_6x67 {
  G_UnusedHeader header;
  le_uint32_t floor = 0;
  le_uint32_t event_id = 0; // NOT event index
  le_uint32_t client_id = 0;
} __packed_ws__(G_TriggerSetEvent_6x67, 0x10);

// 6x68: Set telepipe state

struct G_SetTelepipeState_6x68 {
  G_UnusedHeader header;
  le_uint16_t client_id2 = 0;
  le_uint16_t floor = 0;
  le_uint16_t unknown_b1 = 0;
  uint8_t unknown_b2 = 0;
  uint8_t unknown_b3 = 0;
  VectorXYZF pos;
  le_uint32_t unknown_a3 = 0;
} __packed_ws__(G_SetTelepipeState_6x68, 0x1C);

// 6x69: NPC control
// Note: NPCs cannot be destroyed with 6x69; 6x1C is used instead for that.

// For commands 0 and 3, the template indexes are:
//    0: NOL        1: CICIL      2: CICIL      3: MARACA     4: ELLY
//    5: SHINO      6: DONOPH     7: MOME       8: ALICIA     9: ASH
//   10: ASH       11: SUE       12: KIREEK    13: BERNIE    14: GILLIAM
//   15: ELENOR    16: ALICIA    17: MONTAGUE  18: RUPIKA    19: MATHA
//   20: ANNA      21: TONZLAR   22: TOBOKKE   23: GEKIGASKY 24: TYPE:O
//   25: TYPE:W    26: GIZEL     27: DACCI     28: HOPKINS   29: DORONBO
//   30: KROE      31: MUJO      32: RACTON    33: LIONEL    34: ZOKE
//   35: SUE       36: NADJA     37: ELENOR    38: KIREEK    39: BERNIE
//   40: CHRIS     41: RENEE     42: KAREN     43: BEIRON    44: NAKA
//   45: LEO       46: HOUND     47: MADELEINE 48: VALLETTA  49: BOGARDE
//   50: ULT       51: TYPE:I    52: TYPE:V    53: TACHIBANA 54: OSMAN
//   55: VIVIENNE  56: BP        57: SHINTARO  58: KEN       59: TAKUYA
//   60: SOKON     61: UKON      62: CANTONA   63: HASE
// Created NPCs have a base level according to their template index. On Hard,
// 25 is added to their base level; on Very Hard, 50 is added; on Ultimate, 150
// is added. In all cases the NPC's level is clamped to [1, 199].

struct G_NPCControl_6x69 {
  G_UnusedHeader header;
  le_uint16_t param1; // Commands 0/3: state; command 1: npc_entity_id; command 2: player to follow
  le_uint16_t param2; // Commands 0/3: npc_entity_id; commands 1/2: unused
  le_uint16_t command = 0; // 0 = create follower NPC, 1 = stop acting, 2 = start acting, 3 = create attacker NPC
  le_uint16_t param3; // Commands 0/3: npc_template_index; commands 1/2: unused
} __packed_ws__(G_NPCControl_6x69, 0x0C);

// 6x6A: Set boss warp flags (not valid on Episode 3)

struct G_SetBossWarpFlags_6x6A {
  G_EntityIDHeader header; // entity_id = boss warp object ID
  le_uint16_t flags;
  le_uint16_t unused;
} __packed_ws__(G_SetBossWarpFlags_6x6A, 8);

// 6x6B: Sync enemy state (used while loading into game)

// Note that DC NTE doesn't send the decompressed size in the header here. This
// is a bug that can cause the client to write more entries than it should when
// it receives one of these commands. All later versions, including 11/2000,
// use the second header structure here, which prevents this issue.

struct G_SyncGameStateHeader_DCNTE_6x6B_6x6C_6x6D_6x6E {
  G_ExtendedHeaderT<G_UnusedHeader> header;
  le_uint32_t decompressed_size = 0;
  // BC0-compressed data follows here (see bc0_decompress)
} __packed_ws__(G_SyncGameStateHeader_DCNTE_6x6B_6x6C_6x6D_6x6E, 0x0C);

struct G_SyncGameStateHeader_6x6B_6x6C_6x6D_6x6E {
  G_ExtendedHeaderT<G_UnusedHeader> header;
  le_uint32_t decompressed_size = 0;
  le_uint32_t compressed_size = 0; // Must be <= subcommand_size - 0x10
  // BC0-compressed data follows here (see bc0_decompress)
} __packed_ws__(G_SyncGameStateHeader_6x6B_6x6C_6x6D_6x6E, 0x10);

// Decompressed format is a list of SyncEnemyStateEntry (from Map.hh).

// 6x6C: Sync object state (used while loading into game)
// Compressed format is the same as 6x6B.
// Decompressed format is a list of SyncObjectStateEntry (from Map.hh).

// 6x6D: Sync item state (used while loading into game)
// Internal name: RcvItemCondition
// Compressed format is the same as 6x6B.

// There is a bug in the client that can cause desync between players' item IDs
// if the 6x6D command is sent too quickly. Under normal operation, the client
// keeps track of the next item ID to be assigned for items it creates, and uses
// that to assign item IDs to its inventory items when it's done loading (just
// before it sends the 6F command). The loading process triggered by the 64
// (game join) command resets these next item ID variables to their default
// values, and the 6x6D command sent by the leader resets them again to match
// the corresponding variables in the leader's item state. However, the loading
// process doesn't actually start until the next frame after the 64 command is
// received, so if the 6x6D command is received on the same frame as the 64
// command, it will set the next item ID variables correctly, and the loading
// process will then clear all of them on the next frame. The client will then
// assign its own inventory item IDs based on the default base item ID, which
// will result in incorrect IDs if another player had previously been in the
// game in the same slot (since the leader's next item ID for the joining player
// will not match the default value). Fortunately, the game processes commands
// in two phases: first, it receives as much data as possible, then it processes
// as many commands as possible. So, to prevent this bug, we delay all commands
// after a 64 is sent until the client responds to a ping command (sent
// immediately after the 64 command), which ensures that the 64 and 6x6D
// commands cannot be processed on the same frame.

struct G_SyncItemState_6x6D_Decompressed {
  // Note: 16 vs. 15 is not a bug here - there really is an extra field in the
  // next drop number vs. the floor item count. Despite this, Pioneer 2 or Lab
  // (floor 0) isn't included in next_drop_number_per_floor (so Forest 1 is [0]
  // in that array) but it is included in floor_item_count_per_floor (so Forest
  // 1 is [1] there).
  /* 00 */ parray<le_uint16_t, 16> next_drop_number_per_floor;
  // Only [0]-[3] in this array are ever actually used in normal gameplay, but
  // the client fills in all 12 of these with reasonable values.
  /* 20 */ parray<le_uint32_t, 12> next_item_id_per_player;
  /* 50 */ parray<le_uint32_t, 15> floor_item_count_per_floor;
  // Variable-length field:
  /* 8C */ // FloorItem items[sum(floor_item_count_per_floor)];
} __packed_ws__(G_SyncItemState_6x6D_Decompressed, 0x8C);

// 6x6E: Sync set flag state (used while loading into game)
// Compressed format is the same as 6x6B.

struct G_SyncSetFlagState_6x6E_Decompressed {
  le_uint16_t total_size = 0; // == sum of the following 3 fields
  le_uint16_t entity_set_flags_size = 0;
  le_uint16_t event_set_flags_size = 0;
  le_uint16_t switch_flags_size = 0;
  // Variable-length fields follow here:
  // EntitySetFlags entity_set_flags; // Total size is entity_set_flags_size
  // le_uint16_t event_set_flags[event_set_flags_size / 2]; // Same order as in map files (NOT sorted by event_id)
  // SwitchFlags switch_flags; // 0x200 bytes (0x10 floors) on v1 and earlier; 0x240 bytes (0x12 floors) on v2 and later

  struct EntitySetFlags {
    le_uint32_t object_set_flags_offset = 0;
    le_uint32_t num_object_sets = 0;
    le_uint32_t enemy_set_flags_offset = 0;
    le_uint32_t num_enemy_sets = 0;
    // Variable-length fields follow here:
    // le_uint16_t object_set_flags[num_object_sets];
    // le_uint16_t enemy_set_flags[num_enemy_sets];
  } __packed_ws__(EntitySetFlags, 0x10);
} __packed_ws__(G_SyncSetFlagState_6x6E_Decompressed, 8);

// 6x6F: Set quest flags (used while loading into game)

struct G_SetQuestFlags_DCv1_6x6F {
  G_UnusedHeader header;
  QuestFlagsV1 quest_flags;
} __packed_ws__(G_SetQuestFlags_DCv1_6x6F, 0x184);

struct G_SetQuestFlags_V2_V3_6x6F {
  G_UnusedHeader header;
  QuestFlags quest_flags;
} __packed_ws__(G_SetQuestFlags_V2_V3_6x6F, 0x204);

struct G_SetQuestFlags_BB_6x6F {
  G_UnusedHeader header;
  QuestFlags quest_flags;
  // If use_apply_mask is 1, only the flags set in bb_quest_flag_apply_mask
  // (in PlayerSubordinates.cc) are overwritten on the receiving client's end.
  // The client always sends this with use_apply_mask = 1.
  le_uint32_t use_apply_mask = 1;
} __packed_ws__(G_SetQuestFlags_BB_6x6F, 0x208);

// 6x70: Sync player disp data and inventory (used while loading into game)
// Annoyingly, they didn't use the same format as the 65/67/68 commands here,
// and instead rearranged a bunch of things. This is presumably because this
// structure also includes transient state (e.g. current HP).

struct G_6x70_Sub_Telepipe {
  /* 00 */ le_uint16_t owner_client_id = 0xFFFF;
  /* 02 */ le_uint16_t floor = 0;
  /* 04 */ le_uint32_t unknown_a1 = 0;
  /* 08 */ VectorXYZF pos;
  /* 14 */ le_uint32_t unknown_a3 = 0;
  /* 18 */ le_uint32_t unknown_a4 = 0x0000FFFF;
  /* 1C */
} __packed_ws__(G_6x70_Sub_Telepipe, 0x1C);

struct G_6x70_Sub_UnknownA1 {
  // This is used in all versions of this command except DCNTE and 11/2000.
  /* 00 */ le_uint16_t unknown_a1 = 0;
  /* 02 */ le_uint16_t unknown_a2 = 0;
  /* 04 */ le_uint32_t unknown_a3 = 0;
  /* 08 */ le_float unknown_a4 = 0.0f;
  /* 0C */ le_uint32_t unknown_a5 = 0;
  /* 10 */ le_uint32_t unknown_a6 = 0;
  /* 14 */
} __packed_ws__(G_6x70_Sub_UnknownA1, 0x14);

struct G_6x70_StatusEffectState {
  /* 00 */ le_uint32_t effect_type = 0;
  /* 04 */ le_float multiplier = 0.0f;
  /* 08 */ le_uint32_t remaining_frames = 0;
  /* 0C */
} __packed_ws__(G_6x70_StatusEffectState, 0x0C);

struct G_6x70_Base_DCNTE {
  /* 0000 */ le_uint16_t client_id = 0;
  /* 0002 */ le_uint16_t room_id = 0;
  /* 0004 */ le_uint32_t flags1 = 0;
  /* 0008 */ VectorXYZF pos;
  /* 0014 */ le_uint32_t angle_x = 0;
  /* 0018 */ le_uint32_t angle_y = 0;
  /* 001C */ le_uint32_t angle_z = 0;
  /* 0020 */ le_uint16_t unknown_a3a = 0;
  /* 0022 */ le_uint16_t current_hp = 0;
} __packed_ws__(G_6x70_Base_DCNTE, 0x24);

struct G_SyncPlayerDispAndInventory_DCNTE_6x70 {
  // Offsets in this struct are relative to the overall command header
  /* 0004 */ G_ExtendedHeaderT<G_ClientIDHeader> header = {{0x60, 0x00, 0x0000}, sizeof(G_SyncPlayerDispAndInventory_DCNTE_6x70)};
  /* 000C */ G_6x70_Base_DCNTE base;
  // The following two fields appear to contain uninitialized data
  /* 0030 */ le_uint32_t unknown_a5 = 0;
  /* 0034 */ le_uint32_t unknown_a6 = 0;
  /* 0038 */ G_6x70_Sub_Telepipe telepipe;
  /* 0054 */ le_uint32_t unknown_a8 = 0;
  /* 0058 */ parray<uint8_t, 0x10> unknown_a9;
  /* 0068 */ le_uint32_t area = 0;
  /* 006C */ le_uint32_t flags2 = 0;
  /* 0070 */ PlayerVisualConfig visual;
  /* 00C0 */ PlayerStats stats;
  /* 00E4 */ le_uint32_t num_items = 0;
  /* 00E8 */ parray<PlayerInventoryItem, 0x1E> items;
  /* 0430 */
} __packed_ws__(G_SyncPlayerDispAndInventory_DCNTE_6x70, 0x42C);

struct G_SyncPlayerDispAndInventory_DC112000_6x70 {
  // Offsets in this struct are relative to the overall command header
  /* 0004 */ G_ExtendedHeaderT<G_ClientIDHeader> header = {{0x67, 0x00, 0x0000}, sizeof(G_SyncPlayerDispAndInventory_DC112000_6x70)};
  /* 000C */ G_6x70_Base_DCNTE base;
  /* 0030 */ le_uint16_t bonus_hp_from_materials = 0;
  /* 0032 */ le_uint16_t bonus_tp_from_materials = 0;
  /* 0034 */ parray<uint8_t, 0x10> unknown_a5;
  /* 0044 */ G_6x70_Sub_Telepipe telepipe;
  /* 0060 */ le_uint32_t unknown_a8 = 0;
  /* 0064 */ parray<uint8_t, 0x10> unknown_a9;
  /* 0074 */ le_uint32_t area = 0;
  /* 0078 */ le_uint32_t flags2 = 0;
  /* 007C */ PlayerVisualConfig visual;
  /* 00CC */ PlayerStats stats;
  /* 00F0 */ le_uint32_t num_items = 0;
  /* 00F4 */ parray<PlayerInventoryItem, 0x1E> items;
  /* 043C */
} __packed_ws__(G_SyncPlayerDispAndInventory_DC112000_6x70, 0x438);

struct G_6x70_Base_V1 {
  /* 0000 */ G_6x70_Base_DCNTE base;
  /* 0024 */ le_uint16_t bonus_hp_from_materials = 0;
  /* 0026 */ le_uint16_t bonus_tp_from_materials = 0;
  /* 0028 */ G_6x70_StatusEffectState permanent_status_effect;
  /* 0034 */ G_6x70_StatusEffectState temporary_status_effect;
  /* 0040 */ G_6x70_StatusEffectState attack_status_effect;
  /* 004C */ G_6x70_StatusEffectState defense_status_effect;
  /* 0058 */ G_6x70_StatusEffectState unused_status_effect;
  /* 0064 */ le_uint32_t language = 0;
  /* 0068 */ le_uint32_t player_tag = 0;
  /* 006C */ le_uint32_t guild_card_number = 0;
  /* 0070 */ le_uint32_t unknown_a6 = 0; // Probably battle-related (assigned together with battle_team_number)
  /* 0074 */ le_uint32_t battle_team_number = 0;
  /* 0078 */ G_6x70_Sub_Telepipe telepipe;
  /* 0094 */ le_uint32_t unknown_a8 = 0;
  /* 0098 */ G_6x70_Sub_UnknownA1 unknown_a9;
  /* 00AC */ le_uint32_t area = 0;
  /* 00B0 */ le_uint32_t flags2 = 0;
  /* 00B4 */ parray<uint8_t, 0x14> technique_levels_v1 = 0xFF; // Last byte is uninitialized
  /* 00C8 */ PlayerVisualConfig visual;
  /* 0118 */
} __packed_ws__(G_6x70_Base_V1, 0x118);

struct G_SyncPlayerDispAndInventory_DC_PC_6x70 {
  // Offsets in this struct are relative to the overall command header
  /* 0004 */ G_ExtendedHeaderT<G_ClientIDHeader> header = {{0x70, 0x00, 0x0000}, sizeof(G_SyncPlayerDispAndInventory_DC_PC_6x70)};
  /* 000C */ G_6x70_Base_V1 base;
  /* 0124 */ PlayerStats stats;
  /* 0148 */ le_uint32_t num_items = 0;
  /* 014C */ parray<PlayerInventoryItem, 0x1E> items;
  /* 0494 */
} __packed_ws__(G_SyncPlayerDispAndInventory_DC_PC_6x70, 0x490);

// GC NTE also uses this format.
struct G_SyncPlayerDispAndInventory_GC_6x70 {
  // Offsets in this struct are relative to the overall command header
  /* 0004 */ G_ExtendedHeaderT<G_ClientIDHeader> header = {{0x70, 0x00, 0x0000}, sizeof(G_SyncPlayerDispAndInventory_GC_6x70)};
  /* 000C */ G_6x70_Base_V1 base;
  /* 0124 */ PlayerStats stats;
  /* 0148 */ le_uint32_t num_items = 0;
  /* 014C */ parray<PlayerInventoryItem, 0x1E> items;
  /* 0494 */ le_uint32_t floor = 0;
  /* 0498 */
} __packed_ws__(G_SyncPlayerDispAndInventory_GC_6x70, 0x494);

struct G_SyncPlayerDispAndInventory_XB_6x70 {
  // Offsets in this struct are relative to the overall command header
  /* 0004 */ G_ExtendedHeaderT<G_ClientIDHeader> header = {{0x70, 0x00, 0x0000}, sizeof(G_SyncPlayerDispAndInventory_XB_6x70)};
  /* 000C */ G_6x70_Base_V1 base;
  /* 0124 */ PlayerStats stats;
  /* 0148 */ le_uint32_t num_items = 0;
  /* 014C */ parray<PlayerInventoryItem, 0x1E> items;
  /* 0494 */ le_uint32_t floor = 0;
  /* 0498 */ le_uint32_t xb_user_id_high = 0;
  /* 049C */ le_uint32_t xb_user_id_low = 0;
  /* 04A0 */ le_uint32_t unknown_a16 = 0;
  /* 04A4 */
} __packed_ws__(G_SyncPlayerDispAndInventory_XB_6x70, 0x4A0);

struct G_SyncPlayerDispAndInventory_BB_6x70 {
  // Offsets in this struct are relative to the overall command header
  /* 0008 */ G_ExtendedHeaderT<G_ClientIDHeader> header = {{0x70, 0x00, 0x0000}, sizeof(G_SyncPlayerDispAndInventory_BB_6x70)};
  /* 0010 */ G_6x70_Base_V1 base;
  /* 0128 */ pstring<TextEncoding::UTF16_ALWAYS_MARKED, 0x10> name;
  /* 0148 */ PlayerStats stats;
  /* 016C */ le_uint32_t num_items = 0;
  /* 0170 */ parray<PlayerInventoryItem, 0x1E> items;
  /* 04B8 */ le_uint32_t floor = 0;
  /* 04BC */ le_uint32_t xb_user_id_high = 0;
  /* 04C0 */ le_uint32_t xb_user_id_low = 0;
  /* 04C4 */ le_uint32_t unknown_a16 = 0;
  /* 04C8 */
} __packed_ws__(G_SyncPlayerDispAndInventory_BB_6x70, 0x4C0);

// 6x71: Unblock game join (used while loading into game)

struct G_UnblockGameJoin_6x71 {
  G_UnusedHeader header;
} __packed_ws__(G_UnblockGameJoin_6x71, 4);

// 6x72: Player done loading into game

struct G_DoneLoadingIntoGame_6x72 {
  G_UnusedHeader header;
} __packed_ws__(G_DoneLoadingIntoGame_6x72, 4);

// 6x73: Exit quest
// This command misbehaves if sent in a lobby or in a game when no quest is
// loaded.

struct G_ExitQuest_6x73 {
  G_UnusedHeader header;
} __packed_ws__(G_ExitQuest_6x73, 4);

// 6x74: Word select
// There is a bug in PSO GC with regard to this command: the client does not
// byteswap the header, which means the client_id field is big-endian.

template <bool BE>
struct G_WordSelectT_6x74 {
  uint8_t subcommand = 0;
  uint8_t size = 0;
  U16T<BE> client_id = 0;
  WordSelectMessage message;
} __packed__;
using G_WordSelect_6x74 = G_WordSelectT_6x74<false>;
using G_WordSelectBE_6x74 = G_WordSelectT_6x74<true>;
check_struct_size(G_WordSelect_6x74, 0x20);
check_struct_size(G_WordSelectBE_6x74, 0x20);

// 6x75: Update quest flag

struct G_UpdateQuestFlag_DC_PC_6x75 {
  G_UnusedHeader header;
  le_uint16_t flag = 0; // Must be < 0x400
  le_uint16_t action = 0; // 0 = set flag, 1 = clear flag
} __packed_ws__(G_UpdateQuestFlag_DC_PC_6x75, 8);

struct G_UpdateQuestFlag_V3_BB_6x75 : G_UpdateQuestFlag_DC_PC_6x75 {
  le_uint16_t difficulty = 0;
  le_uint16_t unused = 0;
} __packed_ws__(G_UpdateQuestFlag_V3_BB_6x75, 0x0C);

// 6x76: Set entity set flags
// This command can only be used to set set flags, since the game performs a
// bitwise OR operation instead of a simple assignment.

struct G_SetEntitySetFlags_6x76 {
  G_EntityIDHeader header; // 1000-3FFF = enemy, 4000-FFFF = object
  le_uint16_t floor = 0;
  le_uint16_t flags = 0;
} __packed_ws__(G_SetEntitySetFlags_6x76, 8);

// 6x77: Sync quest register
// This is sent by the client when an opcode D9 is executed within a quest.

struct G_SyncQuestRegister_6x77 {
  G_UnusedHeader header;
  le_uint16_t register_number = 0; // Must be < 0x100
  le_uint16_t unused = 0;
  union {
    le_uint32_t as_int;
    le_float as_float;
  } __packed__ value;
} __packed_ws__(G_SyncQuestRegister_6x77, 0x0C);

// 6x78: Unknown

struct G_Unknown_6x78 {
  G_UnusedHeader header;
  le_uint16_t client_id = 0; // Must be < 12
  le_uint16_t unused1 = 0;
  le_uint32_t unused2 = 0;
} __packed_ws__(G_Unknown_6x78, 0x0C);

// 6x79: Lobby 14/15 gogo ball (soccer game)

struct G_GogoBall_6x79 {
  G_UnusedHeader header;
  le_uint32_t unknown_a1 = 0;
  le_uint32_t unknown_a2 = 0;
  le_float unknown_a3 = 0.0f;
  le_float unknown_a4 = 0.0f;
  uint8_t unknown_a5 = 0;
  parray<uint8_t, 3> unused;
} __packed_ws__(G_GogoBall_6x79, 0x18);

// 6x7A: Unknown (protected on V3/V4)

struct G_Unknown_6x7A {
  G_ClientIDHeader header;
} __packed_ws__(G_Unknown_6x7A, 4);

// 6x7B: Unknown (protected on V3/V4)

struct G_Unknown_6x7B {
  G_ClientIDHeader header;
} __packed_ws__(G_Unknown_6x7B, 4);

// 6x7C: Set Challenge records (not valid on Episode 3)

struct G_SetChallengeRecordsBase_6x7C {
  G_UnusedHeader header;
  le_uint16_t client_id = 0;
  parray<uint8_t, 2> unused;
} __packed_ws__(G_SetChallengeRecordsBase_6x7C, 8);

struct G_SetChallengeRecords_DC_6x7C : G_SetChallengeRecordsBase_6x7C {
  PlayerRecordsChallengeDC records;
} __packed_ws__(G_SetChallengeRecords_DC_6x7C, 0xA8);
struct G_SetChallengeRecords_PC_6x7C : G_SetChallengeRecordsBase_6x7C {
  PlayerRecordsChallengePC records;
} __packed_ws__(G_SetChallengeRecords_PC_6x7C, 0xE0);
struct G_SetChallengeRecords_V3_6x7C : G_SetChallengeRecordsBase_6x7C {
  PlayerRecordsChallengeV3 records;
} __packed_ws__(G_SetChallengeRecords_V3_6x7C, 0x108);
struct G_SetChallengeRecords_BB_6x7C : G_SetChallengeRecordsBase_6x7C {
  PlayerRecordsChallengeBB records;
} __packed_ws__(G_SetChallengeRecords_BB_6x7C, 0x148);

// 6x7D: Set battle mode data (not valid on Episode 3)

struct G_SetBattleModeData_6x7D {
  G_UnusedHeader header;
  // Values for what (0-6; values 7 and above are not valid):
  // 0 = Unknown (params[0] and [1] are used)
  // 2 = Unknown (no params are used)
  // 3 = Set player score (params[0] = client ID, [1] = score)
  // 4 = Unknown (params[0] = client ID)
  // 5 = Unknown (no params are used)
  // 6 = Unknown (all params are used)
  // Anything else = command is ignored
  uint8_t what = 0;
  uint8_t unknown_a1 = 0; // Only used when what == 0
  uint8_t unused = 0;
  uint8_t is_alive = 0; // Only used when what == 3
  parray<le_uint32_t, 4> params;
} __packed_ws__(G_SetBattleModeData_6x7D, 0x18);

// 6x7E: Unknown (not valid on Episode 3)
// This subcommand is completely ignored (at least, by PSO GC).

// 6x7F: Battle scores and places (not valid on Episode 3)

template <bool BE>
struct G_BattleScoresT_6x7F {
  struct Entry {
    U16T<BE> client_id = 0;
    U16T<BE> place = 0;
    U32T<BE> score = 0;
  } __packed_ws__(Entry, 8);
  G_UnusedHeader header;
  parray<Entry, 4> entries;
} __packed__;
using G_BattleScores_6x7F = G_BattleScoresT_6x7F<false>;
using G_BattleScoresBE_6x7F = G_BattleScoresT_6x7F<true>;
check_struct_size(G_BattleScores_6x7F, 0x24);
check_struct_size(G_BattleScoresBE_6x7F, 0x24);

// 6x80: Trigger trap (not valid on Episode 3)

struct G_TriggerTrap_6x80 {
  G_ClientIDHeader header;
  // Traps set by players are numbered according to their type and who set
  // them. The trap number is (client_id * 80) + (trap_type * 20) + trap_index.
  // trap_index comes from the 6x83 command, described below.
  // Note that the trap number does not directly correspond to a specific
  // object ID. Instead, object IDs past the end of the map data are
  // dynamically allocated when players place traps.
  // TODO: What happens in the case of data races, e.g. if two players set
  // traps at the same time? Does the game just desync because they get
  // different object IDs on different clients?
  le_uint16_t trap_number = 0;
  le_uint16_t what = 0; // Must be 0, 1, or 2
} __packed_ws__(G_TriggerTrap_6x80, 8);

// 6x81: Disable drop weapon on death (protected on V3/V4)

struct G_DisableDropWeaponOnDeath_6x81 {
  G_ClientIDHeader header;
} __packed_ws__(G_DisableDropWeaponOnDeath_6x81, 4);

// 6x82: Enable drop weapon on death (protected on V3/V4)

struct G_EnableDropWeaponOnDeath_6x82 {
  G_ClientIDHeader header;
} __packed_ws__(G_EnableDropWeaponOnDeath_6x82, 4);

// 6x83: Place trap (protected on V3/V4)

struct G_PlaceTrap_6x83 {
  G_ClientIDHeader header;
  le_uint16_t trap_type = 0;
  le_uint16_t trap_index = 0;
} __packed_ws__(G_PlaceTrap_6x83, 8);

// 6x84: Vol Opt boss actions (not valid on Episode 3)
// Same format and usage as 6x16, except unknown_a2 is ignored in 6x84.

struct G_VolOptBossActions_6x84 {
  G_UnusedHeader header;
  parray<uint8_t, 6> unknown_a1;
  le_uint16_t unknown_a2 = 0;
  le_uint16_t unknown_a3 = 0;
  le_uint16_t unused = 0;
} __packed_ws__(G_VolOptBossActions_6x84, 0x10);

// 6x85: Unknown (supported; game only; not valid on Episode 3)

struct G_Unknown_6x85 {
  G_UnusedHeader header;
  le_uint16_t unknown_a1 = 0; // Command is ignored unless this is 0
  parray<le_uint16_t, 7> unknown_a2; // Only the first 3 appear to be used
} __packed_ws__(G_Unknown_6x85, 0x14);

// 6x86: Hit destructible object (not valid on Episode 3)

struct G_HitDestructibleObject_6x86 : G_UpdateObjectState_6x0B {
  le_uint16_t unknown_a3 = 0;
  le_uint16_t unknown_a4 = 0;
} __packed_ws__(G_HitDestructibleObject_6x86, 0x10);

// 6x87: Shrink player (protected on V3/V4)

struct G_ShrinkPlayer_6x87 {
  G_ClientIDHeader header;
  le_float unknown_a1 = 0.0f;
} __packed_ws__(G_ShrinkPlayer_6x87, 8);

// 6x88: Restore shrunken player (protected on V3/V4)

struct G_RestoreShrunkenPlayer_6x88 {
  G_ClientIDHeader header;
} __packed_ws__(G_RestoreShrunkenPlayer_6x88, 4);

// 6x89: Set killer entity ID (protected on V3/V4)
// This is used to specify which enemy killed a player.

struct G_SetKillerEntityID_6x89 {
  G_ClientIDHeader header;
  le_uint16_t killer_entity_id = 0;
  le_uint16_t unused = 0;
} __packed_ws__(G_SetKillerEntityID_6x89, 8);

// 6x8A: Show Challenge time records window (not valid on Episode 3)
// The leader sends this command to tell other clients to show, hide, or update
// the window that shows Challenge Mode stage competion and time records during
// Challenge quest selection.

struct G_ShowChallengeTimeRecordsWindow_6x8A {
  G_ClientIDHeader header;
  // Values for which (decimal):
  // 0 = hide window
  // 1 = show Episode 1 completion state per player
  // 2 = show Episode 2 completion state per player
  // 3-11 = show times for Episode 1 stage (which - 2)
  // 12-16 = show times for Episode 2 stage (which - 11)
  // Anything else = command is ignored
  le_uint32_t which = 0;
} __packed_ws__(G_ShowChallengeTimeRecordsWindow_6x8A, 8);

// 6x8B: Unknown (not valid on Episode 3)
// This command has a handler, but it does nothing.

// 6x8C: Unknown (not valid on Episode 3)
// This command has a handler, but it does nothing.

// 6x8D: Set technique level override (protected on V3/V4)
// This command is sent immediately before 6x47 if the technique level is above
// 15. Presumably this was done for compatibility between v1 and v2.

struct G_SetTechniqueLevelOverride_6x8D {
  G_ClientIDHeader header;
  uint8_t level_upgrade = 0;
  uint8_t unused1 = 0;
  le_uint16_t unused2 = 0;
} __packed_ws__(G_SetTechniqueLevelOverride_6x8D, 8);

// 6x8E: Unknown (not valid on Episode 3)
// This command has a handler, but it does nothing.

// 6x8F: Add battle damage scores (not valid on Episode 3)

struct G_AddBattleDamageScores_6x8F {
  G_ClientIDHeader header; // client_id is the attacking player
  le_uint16_t target_entity_id = 0;
  le_uint16_t amount = 0;
} __packed_ws__(G_AddBattleDamageScores_6x8F, 8);

// 6x90: Set player battle team (not valid on Episode 3) (protected on V3/V4)

struct G_SetPlayerBattleTeam_6x90 {
  G_ClientIDHeader header;
  le_uint32_t team_number = 0; // 0 or 1
} __packed_ws__(G_SetPlayerBattleTeam_6x90, 8);

// 6x91: Unknown (supported; game only)
// TODO: Deals with TOAttackableCol objects. Figure out exactly what it does.

struct G_UpdateAttackableColState_6x91 : G_UpdateObjectState_6x0B {
  le_uint16_t unknown_a3 = 0;
  le_uint16_t unknown_a4 = 0;
  le_uint16_t switch_flag_num = 0;
  uint8_t should_set = 0; // The switch flag is only set if this is equal to 1; otherwise it's cleared
  uint8_t floor = 0;
} __packed_ws__(G_UpdateAttackableColState_6x91, 0x14);

// 6x92: Unknown (not valid on Episode 3)
// This does something with the TObjOnlineEndingHexMove object. TODO: Figure
// out exactly what.

struct G_Unknown_6x92 {
  G_UnusedHeader header;
  le_uint32_t unknown_a1 = 0;
  le_float unknown_a2 = 0.0f;
} __packed_ws__(G_Unknown_6x92, 0x0C);

// 6x93: Activate timed switch (not valid on Episode 3)

struct G_ActivateTimedSwitch_6x93 {
  G_UnusedHeader header;
  le_uint16_t switch_flag_floor = 0;
  le_uint16_t switch_flag_num = 0;
  uint8_t should_set = 0; // The switch flag is only set if this is equal to 1; otherwise it's cleared
  parray<uint8_t, 3> unused;
} __packed_ws__(G_ActivateTimedSwitch_6x93, 0x0C);

// 6x94: Warp (not valid on Episode 3)

struct G_InterLevelWarp_6x94 {
  G_UnusedHeader header;
  le_uint16_t floor = 0;
  parray<uint8_t, 2> unused;
} __packed_ws__(G_InterLevelWarp_6x94, 8);

// 6x95: Set challenge time (not valid on Episode 3)
// This command sets the time returned by the F88E quest opcode.

struct G_SetChallengeTime_6x95 {
  G_UnusedHeader header;
  le_uint32_t client_id = 0;
  ChallengeTime challenge_time;
  // On BB, the token_v4 field is set to (local_client_id + 1) ^ regB (from the
  // chl_set_timerecord opcode). This appears to be a basic anti-cheating
  // measure. The field is unused on other versions, and even on BB, the client
  // doesn't check token_v4 upon receipt, so it's likely just for server-side
  // verification.
  le_uint32_t token_v4 = 0;
  le_uint32_t unused = 0;
} __packed_ws__(G_SetChallengeTime_6x95, 0x14);

// 6x96: Unknown (not valid on Episode 3)
// This command has a handler, but it does nothing.

// 6x97: Select Challenge Mode failure option (not valid on Episode 3)

struct G_SelectChallengeModeFailureOption_6x97 {
  G_UnusedHeader header;
  le_uint32_t unused1 = 0;
  le_uint32_t is_retry = 0;
  le_uint32_t unused2 = 0;
  le_uint32_t unused3 = 0;
} __packed_ws__(G_SelectChallengeModeFailureOption_6x97, 0x14);

// 6x98: Unknown
// This subcommand is completely ignored.

// 6x99: Unknown
// This subcommand is completely ignored.

// 6x9A: Update player stat (not valid on Episode 3)

struct G_UpdatePlayerStat_6x9A {
  G_ClientIDHeader header;
  le_uint16_t client_id2 = 0;
  // Values for what:
  // 0 = subtract HP
  // 1 = subtract TP
  // 2 = subtract Meseta
  // 3 = add HP
  // 4 = add TP
  uint8_t what = 0;
  uint8_t amount = 0;
} __packed_ws__(G_UpdatePlayerStat_6x9A, 8);

// 6x9B: Level up all techniques (protected on V3/V4)
// Used in battle mode if the rules specify that techniques should level up
// upon character death.

struct G_LevelUpAllTechniques_6x9B {
  G_UnusedHeader header;
  uint8_t num_levels = 0;
  parray<uint8_t, 3> unused;
} __packed_ws__(G_LevelUpAllTechniques_6x9B, 8);

// 6x9C: Unknown (supported; game only; not valid on Episode 3)
// This command only has an effect in Ultimate mode.
// TODO: Figure out what this does.

struct G_Unknown_6x9C {
  G_EntityIDHeader header;
  le_uint32_t unknown_a1 = 0;
} __packed_ws__(G_Unknown_6x9C, 8);

// 6x9D: Set dead flag (Challenge mode; not valid on Episode 3)
// This command causes the specified client to appear in the dead players list
// when the Challenge mode retry menu appears.

struct G_SetChallengeDeadFlag_6x9D {
  G_UnusedHeader header;
  le_uint32_t client_id = 0;
} __packed_ws__(G_SetChallengeDeadFlag_6x9D, 8);

// 6x9E: Play camera shutter sound
// This subcommand is only used on PSO PC and PC NTE. It is not implemented (and
// therefore ignored) by all prior versions, and all later versions have
// handlers for this command, but the handlers do nothing.

struct G_PlayerCameraShutterSound_6x9E {
  G_ClientIDHeader header;
} __packed_ws__(G_PlayerCameraShutterSound_6x9E, 4);

// 6x9F: Gal Gryphon boss actions (not valid on pre-V3 or Episode 3)

struct G_GalGryphonBossActions_6x9F {
  G_EntityIDHeader header;
  le_uint32_t unknown_a1 = 0;
  le_float unknown_a2 = 0.0f;
  le_float unknown_a3 = 0.0f;
} __packed_ws__(G_GalGryphonBossActions_6x9F, 0x10);

// 6xA0: Gal Gryphon boss actions (not valid on pre-V3 or Episode 3)

struct G_GalGryphonBossActions_6xA0 {
  G_EntityIDHeader header;
  VectorXYZF pos;
  le_uint32_t unknown_a1 = 0;
  le_uint16_t unknown_a2 = 0;
  le_uint16_t unknown_a3 = 0;
  parray<le_uint32_t, 4> unknown_a4;
} __packed_ws__(G_GalGryphonBossActions_6xA0, 0x28);

// 6xA1: Revive player (not valid on pre-V3) (protected on V3/V4)

struct G_RevivePlayer_V3_BB_6xA1 {
  G_ClientIDHeader header;
} __packed_ws__(G_RevivePlayer_V3_BB_6xA1, 4);

// 6xA2: Specializable item drop request (not valid on pre-V3; handled by
// server on BB)

struct G_SpecializableItemDropRequest_6xA2 : G_StandardDropItemRequest_PC_V3_BB_6x60 {
  /* 18 */ le_float param3 = 0.0f;
  /* 1C */ le_uint32_t param4 = 0;
  /* 20 */ le_uint32_t param5 = 0;
  /* 24 */ le_uint32_t param6 = 0;
  /* 28 */
} __packed_ws__(G_SpecializableItemDropRequest_6xA2, 0x28);

// 6xA3: Olga Flow boss actions (not valid on pre-V3 or Episode 3)

struct G_OlgaFlowBossActions_6xA3 {
  G_EntityIDHeader header;
  uint8_t unknown_a1 = 0;
  uint8_t unknown_a2 = 0;
  parray<uint8_t, 2> unknown_a3;
} __packed_ws__(G_OlgaFlowBossActions_6xA3, 8);

// 6xA4: Olga Flow phase 1 boss actions (not valid on pre-V3 or Episode 3)

struct G_OlgaFlowPhase1BossActions_6xA4 {
  G_EntityIDHeader header;
  uint8_t what = 0;
  parray<uint8_t, 3> unknown_a3;
} __packed_ws__(G_OlgaFlowPhase1BossActions_6xA4, 8);

// 6xA5: Olga Flow phase 2 boss actions (not valid on pre-V3 or Episode 3)

struct G_OlgaFlowPhase2BossActions_6xA5 {
  G_EntityIDHeader header;
  uint8_t what = 0;
  parray<uint8_t, 3> unknown_a3;
} __packed_ws__(G_OlgaFlowPhase2BossActions_6xA5, 8);

// 6xA6: Modify trade proposal (not valid on pre-V3)

struct G_ModifyTradeProposal_6xA6 {
  G_ClientIDHeader header;
  // Values for what:
  // 0 = Propose (result = 0), accept (result = 2), or reject (result = 3)
  //     TODO: amount is used when what=0; what is it used for?
  // 1 = Add item
  // 2 = Remove item
  // 3 = First confirm (result = 0) or cancel first confirm (result = 4)
  // 4 = Second confirm (result = 0) or cancel second confirm (result = 4)
  // 5 = Cancel trade proposal (result = 4)
  // 6 = Reject proposal (currently trading with another player)
  // 7 = ??? (TODO)
  // Anything else = command is ignored
  uint8_t what = 0;
  uint8_t result = 0;
  be_uint16_t unknown_a1; // Only used if what = 7
  le_uint32_t item_id = 0; // Only used if what = 1 or 2
  le_uint32_t amount = 0; // Only used if what = 1 or 2
} __packed_ws__(G_ModifyTradeProposal_6xA6, 0x10);

// 6xA7: Unknown (not valid on pre-V3)
// This subcommand is completely ignored.

// 6xA8: Gol Dragon boss actions (not valid on pre-V3 or Episode 3)

template <bool BE>
struct G_GolDragonBossActionsT_6xA8 {
  G_EntityIDHeader header;
  le_uint16_t unknown_a2 = 0;
  le_uint16_t unknown_a3 = 0;
  le_uint32_t unknown_a4 = 0;
  F32T<BE> x = 0.0f;
  F32T<BE> z = 0.0f;
  uint8_t unknown_a5 = 0;
  parray<uint8_t, 3> unused;
} __packed__;
using G_GolDragonBossActions_XB_BB_6xA8 = G_GolDragonBossActionsT_6xA8<false>;
using G_GolDragonBossActions_GC_6xA8 = G_GolDragonBossActionsT_6xA8<true>;
check_struct_size(G_GolDragonBossActions_XB_BB_6xA8, 0x18);
check_struct_size(G_GolDragonBossActions_GC_6xA8, 0x18);

// 6xA9: Barba Ray boss actions (not valid on pre-V3 or Episode 3)

struct G_BarbaRayBossActions_6xA9 {
  G_EntityIDHeader header;
  le_uint16_t unknown_a1 = 0;
  le_uint16_t unknown_a2 = 0;
} __packed_ws__(G_BarbaRayBossActions_6xA9, 8);

// 6xAA: Barba Ray boss actions (not valid on pre-V3 or Episode 3)

struct G_BarbaRayBossActions_6xAA {
  G_EntityIDHeader header;
  le_uint16_t unknown_a1 = 0;
  le_uint16_t unknown_a2 = 0;
  le_uint32_t unknown_a3 = 0;
} __packed_ws__(G_BarbaRayBossActions_6xAA, 0x0C);

// 6xAB: Create lobby chair (not valid on pre-V3) (protected on V3/V4)
// This command appears to be different on GC NTE than on any other version.
// It's not known what it does on GC NTE.

struct G_Unknown_GCNTE_6xAB {
  G_EntityIDHeader header;
  le_uint16_t unknown_a1 = 0;
  le_uint16_t unknown_a2 = 0;
} __packed_ws__(G_Unknown_GCNTE_6xAB, 8);

struct G_CreateLobbyChair_6xAB {
  G_ClientIDHeader header;
  le_uint16_t unknown_a1 = 0;
  le_uint16_t unknown_a2 = 0;
} __packed_ws__(G_CreateLobbyChair_6xAB, 8);

// 6xAC: Unknown (not valid on pre-V3) (protected on V3/V4)
// This command appears to be different on GC NTE than on any other version. It
// also seems that no version (except perhaps GC NTE) ever sends this command.

struct G_Unknown_GCNTE_6xAC {
  G_EntityIDHeader header;
  le_uint16_t unknown_a1 = 0;
  le_uint16_t unknown_a2 = 0;
  le_uint32_t unknown_a3 = 0;
} __packed_ws__(G_Unknown_GCNTE_6xAC, 0x0C);

struct G_Unknown_6xAC {
  G_ClientIDHeader header;
  le_uint32_t num_items = 0;
  parray<le_uint32_t, 0x1E> item_ids;
} __packed_ws__(G_Unknown_6xAC, 0x80);

// 6xAD: Olga Flow subordinate boss actions (not valid on pre-V3, Episode 3, or
// GC Trial Edition)

struct G_OlgaFlowSubordinateBossActions_6xAD {
  G_UnusedHeader header;
  // The first byte in this array seems to have a special meaning
  parray<uint8_t, 0x40> unknown_a1;
} __packed_ws__(G_OlgaFlowSubordinateBossActions_6xAD, 0x44);

// 6xAE: Set lobby chair state (sent by existing clients at join time)
// This subcommand is not valid on DC, PC, or GC Trial Edition.

struct G_SetLobbyChairState_6xAE {
  G_ClientIDHeader header;
  le_uint16_t unknown_a1 = 0;
  le_uint16_t unused = 0;
  // This field contains the flags field on the sender's TObjPlayer object.
  // If the bit 04000000 is set in this field, then (flags & 1C000000) is or'ed
  // into the TObjPlayer's flags field. All other bits are ignored.
  le_uint32_t flags = 0;
  le_float unknown_a4 = 0;
} __packed_ws__(G_SetLobbyChairState_6xAE, 0x10);

// 6xAF: Turn lobby chair (not valid on pre-V3 or GC Trial Edition) (protected on V3/V4)

struct G_TurnLobbyChair_6xAF {
  G_ClientIDHeader header;
  le_uint32_t angle = 0; // In range [0x0000, 0xFFFF]
} __packed_ws__(G_TurnLobbyChair_6xAF, 8);

// 6xB0: Move lobby chair (not valid on pre-V3 or GC Trial Edition) (protected on V3/V4)

struct G_MoveLobbyChair_6xB0 {
  G_ClientIDHeader header;
  le_uint32_t unknown_a1 = 0;
} __packed_ws__(G_MoveLobbyChair_6xB0, 8);

// 6xB1: Unknown (not valid on pre-V3 or GC Trial Edition)
// This subcommand is completely ignored.

// 6xB2: Play sound from player (not valid on pre-V3 or GC Trial Edition)
// This command is sent when a snapshot is taken on PSO GC, but it can be used
// to play any sound, centered on the local player. If localize is FFFF, then
// the sound is not centered on the local player and is just played globally.

struct G_PlaySoundFromPlayer_6xB2 {
  G_UnusedHeader header;
  uint8_t floor = 0;
  uint8_t unused = 0;
  le_uint16_t localize = 0;
  le_uint32_t sound_id = 0; // 0x00051720 = camera shutter sound
} __packed_ws__(G_PlaySoundFromPlayer_6xB2, 0x0C);

// 6xB3: Unknown (Xbox; voice chat)

struct G_Unknown_XB_6xB3 {
  G_ClientIDHeader header;
  le_uint32_t num_frames;
  // (0x0A * num_frames) bytes of data follows here.
} __packed_ws__(G_Unknown_XB_6xB3, 8);

// 6xB3: CARD battle server data request (Episode 3)

// CARD battle subcommands have multiple subsubcommands, which we name 6xBYxZZ,
// where Y = 3, 4, 5, or 6, and ZZ is any byte. The formats of these
// subsubcommands are described at the end of this file.

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
  // This only applies to Episode 3 final - the Trial Edition does not support
  // masking and may send uninitialized data in this field.
  uint8_t mask_key = 0x00;
  uint8_t unused2 = 0x00;
} __packed_ws__(G_CardBattleCommandHeader, 8);

// Unlike all other 6x subcommands, the 6xB3 subcommand is sent to the server in
// a CA command instead of a 6x, C9, or CB command. (For this reason, we
// generally refer to 6xB3xZZ commands as CAxZZ commands instead.) The server is
// expected to reply to CA commands with one or more 6xB4 subcommands instead of
// forwarding them. The logic for doing so is implemented in Episode3/Server.cc
// and the surrounding classes.

// The 6xB3 subcommand has a longer header than 6xB4 and 6xB5. This header is
// common to all 6xB3x (CAx) subcommands.
struct G_CardServerDataCommandHeader {
  /* 00 */ uint8_t subcommand = 0xB3;
  /* 01 */ uint8_t size = 0x00;
  /* 02 */ le_uint16_t unused1 = 0x0000;
  /* 04 */ uint8_t subsubcommand = 0x00; // See 6xBx subcommand table (after this table)
  /* 05 */ uint8_t sender_client_id = 0x00;
  /* 06 */ uint8_t mask_key = 0x00; // Same meaning as in G_CardBattleCommandHeader
  /* 07 */ uint8_t unused2 = 0x00;
  // The sequence number space is split into 30-bit subspaces by the last 2
  // bits, which are the sender's client ID. That is, player 0 will send
  // sequence number 0, then 4, then 8, etc; player 1 will send 1, then 5, then
  // 9, etc. and so on for players 2 and 3.
  /* 08 */ be_uint32_t sequence_num = 0;
  /* 0C */ be_uint32_t context_token = 0;
  /* 10 */
} __packed_ws__(G_CardServerDataCommandHeader, 0x10);

// 6xB4: Unknown (Xbox; voice chat)

struct G_Unknown_XB_6xB4 {
  G_ClientIDHeader header;
  le_uint32_t unknown_a1;
} __packed_ws__(G_Unknown_XB_6xB4, 8);

// 6xB4: CARD battle server response (Episode 3) - see 6xB3 above
// 6xB5: CARD battle client command (Episode 3) - see 6xB3 above

// 6xB5: BB shop request (handled by the server)

struct G_ShopContentsRequest_BB_6xB5 {
  G_UnusedHeader header;
  le_uint32_t shop_type = 0;
} __packed_ws__(G_ShopContentsRequest_BB_6xB5, 8);

// 6xB6: Episode 3 map list and map contents (server->client only)
// Unlike 6xB3-6xB5, these commands cannot be masked. Also unlike 6xB3-6xB5,
// there are only two subsubcommands, so we list them inline here.
// These subcommands can be rather large, so they should be sent with the 6C
// command instead of the 60 command. (The difference in header format,
// including the extended size field, is likely the reason for 6xB6 being a
// separate subcommand from the other CARD battle subcommands.)

struct G_MapSubsubcommand_Ep3_6xB6 {
  G_ExtendedHeaderT<G_UnusedHeader> header;
  uint8_t subsubcommand = 0; // 0x40 or 0x41
  parray<uint8_t, 3> unused;
} __packed_ws__(G_MapSubsubcommand_Ep3_6xB6, 0x0C);

struct G_MapList_Ep3_6xB6x40 {
  G_MapSubsubcommand_Ep3_6xB6 header;
  le_uint16_t compressed_data_size = 0;
  le_uint16_t unused = 0;
  // PRS-compressed map list data follows here. newserv generates this from the
  // map index when requested; see the MapList struct in Episode3/DataIndexes.hh
  // and Episode3::MapIndex::get_compressed_map_list for details on the format.
} __packed_ws__(G_MapList_Ep3_6xB6x40, 0x10);

struct G_MapData_Ep3_6xB6x41 {
  G_MapSubsubcommand_Ep3_6xB6 header;
  le_uint32_t map_number = 0;
  le_uint16_t compressed_data_size = 0;
  le_uint16_t unused = 0;
  // PRS-compressed map data follows here (which decompresses to an
  // Episode3::MapDefinition).
} __packed_ws__(G_MapData_Ep3_6xB6x41, 0x14);

// 6xB6: BB shop contents (server->client only)

struct G_ShopContents_BB_6xB6 {
  G_UnusedHeader header;
  uint8_t shop_type = 0;
  uint8_t num_items = 0;
  le_uint16_t unused = 0;
  // Note: data2d of these entries should be the price
  parray<ItemData, 20> item_datas;
} __packed_ws__(G_ShopContents_BB_6xB6, 0x198);

// 6xB7: Alias for 6xB3 (Episode 3 Trial Edition)
// This command behaves exactly the same as 6xB3. This alias exists only in
// Episode 3 Trial Edition; it was removed in the final release.

// 6xB7: BB buy shop item (handled by the server)

struct G_BuyShopItem_BB_6xB7 {
  G_UnusedHeader header;
  le_uint32_t shop_item_id = 0;
  uint8_t shop_type = 0;
  uint8_t item_index = 0;
  uint8_t amount = 0;
  uint8_t unused = 0;
} __packed_ws__(G_BuyShopItem_BB_6xB7, 0x0C);

// 6xB8: Alias for 6xB4 (Episode 3 Trial Edition)
// This command behaves exactly the same as 6xB4. This alias exists only in
// Episode 3 Trial Edition; it was removed in the final release.

// 6xB8: BB identify item request (via tekker) (handled by the server)

struct G_IdentifyItemRequest_6xB8 {
  G_UnusedHeader header;
  le_uint32_t item_id = 0;
} __packed_ws__(G_IdentifyItemRequest_6xB8, 8);

// 6xB9: Alias for 6xB5 (Episode 3 Trial Edition)
// This command behaves exactly the same as 6xB5. This alias exists only in
// Episode 3 Trial Edition; it was removed in the final release.

// 6xB9: BB provisional tekker result

struct G_IdentifyResult_BB_6xB9 {
  G_ClientIDHeader header;
  ItemData item_data;
} __packed_ws__(G_IdentifyResult_BB_6xB9, 0x18);

// 6xBA: Sync card trade state (Episode 3)
// This command calls various member functions in TCardTrade. This is used
// after both players are standing at the respective kiosks and are ready to
// trade cards.

struct G_SyncCardTradeState_Ep3_6xBA {
  G_ClientIDHeader header;
  // Values for what:
  // 1 = add card to trade (card_id and count used)
  // 2 = remove card from trade (card_id and count used)
  // 3 = first confirmation
  // 4 = cancel first confirmation
  // 5 = second confirmation
  // 6 = cancel second confirmation
  // 7 = leave trade window
  // Anything else = does nothing
  le_uint16_t what = 0;
  le_uint16_t unused = 0;
  le_uint32_t card_id = 0; // Only used when what = 1 or 2
  le_uint32_t count = 0; // Only used when what = 1 or 2
} __packed_ws__(G_SyncCardTradeState_Ep3_6xBA, 0x10);

// 6xBA: BB accept tekker result (handled by the server)

struct G_AcceptItemIdentification_BB_6xBA {
  G_UnusedHeader header;
  le_uint32_t item_id = 0;
} __packed_ws__(G_AcceptItemIdentification_BB_6xBA, 8);

// 6xBB: Sync card trade server state (Episode 3)
// This command calls various member functions in TCardTradeServer. This is
// used before both players have entered the card trade sequence (as opposed to
// 6xBA, which is used during that sequence).

struct G_SyncCardTradeServerState_Ep3_6xBB {
  G_ClientIDHeader header;
  // Values for what:
  // 0 = request slot (leader sends accept message with what=1)
  // 1 = accept slot (args[0] is the accepted client ID)
  // 2 = cancel all slot requests
  // 3 = replace all slots (args[0, 1] are the two client IDs to accept into
  //     the two slots)
  // 4 = relinquish all slots
  // Anything else = does nothing
  le_uint16_t what = 0;
  le_uint16_t slot = 0; // Must be 0 or 1 (not bounds checked!)
  parray<le_uint32_t, 4> args;
} __packed_ws__(G_SyncCardTradeServerState_Ep3_6xBB, 0x18);

// 6xBB: BB bank request (handled by the server)

struct G_RequestBankContents_BB_6xBB {
  G_UnusedHeader header;
  le_uint32_t checksum; // crc32 of the bank contents in memory
} __packed_ws__(G_RequestBankContents_BB_6xBB, 0x08);

// 6xBC: Card counts (Episode 3)
// This is sent by the client in response to a 6xB5x38 command. This is used
// along with 6xB5x38 so clients can see each other's card counts. Curiously,
// this command is smaller than 0x400 bytes (even on NTE) but uses the extended
// subcommand format anyway.
// An Episode 3 client will crash if it receives this command when the card
// trade window is not active.

struct G_CardCounts_Ep3NTE_6xBC {
  G_ExtendedHeaderT<G_UnusedHeader> header;
  parray<uint8_t, 0x3E8> card_counts;
} __packed_ws__(G_CardCounts_Ep3NTE_6xBC, 0x3F0);

struct G_CardCounts_Ep3_6xBC {
  G_ExtendedHeaderT<G_UnusedHeader> header;
  parray<uint8_t, 0x2F1> card_counts;
  // The client sends uninitialized data in this field
  parray<uint8_t, 3> unused;
} __packed_ws__(G_CardCounts_Ep3_6xBC, 0x2FC);

// 6xBC: BB bank contents (server->client only)
// This is sent in response to a 6xBB command. If the checksum in this command
// doesn't match the checksum the client sent in its 6xBB command, the client
// overwrites its bank data with the data sent in this command.

struct G_BankContentsHeader_BB_6xBC {
  G_ExtendedHeaderT<G_UnusedHeader> header;
  le_uint32_t checksum = 0; // can be random; client won't notice
  le_uint32_t num_items = 0;
  le_uint32_t meseta = 0;
  // Item data follows
} __packed_ws__(G_BankContentsHeader_BB_6xBC, 0x14);

// 6xBD: Word select during battle (Episode 3; not Trial Edition)

// Note: This structure does not have a normal header - the client ID field is
// big-endian!
struct G_PrivateWordSelect_Ep3_6xBD {
  G_ClientIDHeader header;
  WordSelectMessage message;
  // This field has the same meaning as the first byte in an 06 command's
  // message when sent during an Episode 3 battle.
  uint8_t private_flags = 0;
  parray<uint8_t, 3> unused;
} __packed_ws__(G_PrivateWordSelect_Ep3_6xBD, 0x24);

// 6xBD: BB bank action (take/deposit meseta/item) (handled by the server)

struct G_BankAction_BB_6xBD {
  G_UnusedHeader header;
  le_uint32_t item_id = 0;
  le_uint32_t meseta_amount = 0;
  uint8_t action = 0; // 0 = deposit, 1 = take, 3 = done (close bank window)
  uint8_t item_amount = 0;
  le_uint16_t item_index = 0; // 0xFFFF = meseta
} __packed_ws__(G_BankAction_BB_6xBD, 0x10);

// 6xBE: Sound chat (Episode 3; not Trial Edition)
// This is the only subcommand ever sent with the CB command.

struct G_SoundChat_Ep3_6xBE {
  G_UnusedHeader header;
  le_uint32_t sound_id = 0; // Must be < 0x27
  be_uint32_t unused = 0;
} __packed_ws__(G_SoundChat_Ep3_6xBE, 0x0C);

// 6xBE: BB create inventory item (server->client only)

struct G_CreateInventoryItem_BB_6xBE {
  G_ClientIDHeader header;
  ItemData item_data;
  le_uint32_t unused = 0;
} __packed_ws__(G_CreateInventoryItem_BB_6xBE, 0x1C);

// 6xBF: Change lobby music (Episode 3; not Trial Edition)

struct G_ChangeLobbyMusic_Ep3_6xBF {
  G_UnusedHeader header;
  le_uint32_t song_number = 0; // Must be < 0x34
} __packed_ws__(G_ChangeLobbyMusic_Ep3_6xBF, 8);

// 6xBF: Give EXP (BB) (server->client only)

struct G_GiveExperience_BB_6xBF {
  G_ClientIDHeader header;
  le_uint32_t amount = 0;
} __packed_ws__(G_GiveExperience_BB_6xBF, 8);

// 6xC0: Sell item at shop (BB) (protected on V3/V4)

struct G_SellItemAtShop_BB_6xC0 {
  G_UnusedHeader header;
  le_uint32_t item_id = 0;
  le_uint32_t amount = 0;
} __packed_ws__(G_SellItemAtShop_BB_6xC0, 0x0C);

// 6xC1: Invite to team (BB)
// 6xC2: Accept invitation to team (BB)

struct G_TeamInvitationAction_BB_6xC1_6xC2_6xCD_6xCE {
  G_ClientIDHeader header;
  le_uint32_t guild_card_number = 0;
  le_uint32_t action = 0; // 0 or 1 for 6xC1, 2 (or not 2) for 6xC2
  parray<uint8_t, 0x54> unknown_a1; // TODO
} __packed_ws__(G_TeamInvitationAction_BB_6xC1_6xC2_6xCD_6xCE, 0x60);

// 6xC3: Split stacked item (BB; handled by the server)
// Note: This is not sent if an entire stack is dropped; in that case, a normal
// item drop subcommand is generated instead.

struct G_SplitStackedItem_BB_6xC3 {
  G_ClientIDHeader header;
  le_uint16_t floor = 0;
  le_uint16_t unused2 = 0;
  VectorXZF pos;
  le_uint32_t item_id = 0;
  le_uint32_t amount = 0;
} __packed_ws__(G_SplitStackedItem_BB_6xC3, 0x18);

// 6xC4: Sort inventory (BB; handled by the server)

struct G_SortInventory_BB_6xC4 {
  G_UnusedHeader header;
  parray<le_uint32_t, 30> item_ids;
} __packed_ws__(G_SortInventory_BB_6xC4, 0x7C);

// 6xC5: Medical center used (BB)

struct G_MedicalCenterUsed_BB_6xC5 {
  G_ClientIDHeader header;
} __packed_ws__(G_MedicalCenterUsed_BB_6xC5, 4);

// 6xC6: Steal experience (BB; handled by the server)

struct G_StealEXP_BB_6xC6 {
  G_ClientIDHeader header;
  le_uint16_t entity_id = 0;
  le_uint16_t enemy_index = 0;
} __packed_ws__(G_StealEXP_BB_6xC6, 8);

// 6xC7: Charge attack (BB)

struct G_ChargeAttack_BB_6xC7 {
  G_ClientIDHeader header;
  // Tethealla (at least, the ancient public version of it) treats this as
  // signed, and gives the player money in that case. We don't do so.
  le_uint32_t meseta_amount = 0;
} __packed_ws__(G_ChargeAttack_BB_6xC7, 8);

// 6xC8: Enemy EXP request (BB; handled by the server)

struct G_EnemyEXPRequest_BB_6xC8 {
  G_EntityIDHeader header;
  le_uint16_t enemy_index = 0;
  le_uint16_t requesting_client_id = 0;
  uint8_t is_killer = 0;
  parray<uint8_t, 3> unused;
} __packed_ws__(G_EnemyEXPRequest_BB_6xC8, 0x0C);

// 6xC9: Adjust player Meseta (BB; handled by server)

struct G_AdjustPlayerMeseta_BB_6xC9 {
  G_UnusedHeader header;
  le_int32_t amount = 0;
} __packed_ws__(G_AdjustPlayerMeseta_BB_6xC9, 8);

// 6xCA: Request item reward from quest (BB; handled by server)

struct G_ItemRewardRequest_BB_6xCA {
  G_UnusedHeader header;
  ItemData item_data;
} __packed_ws__(G_ItemRewardRequest_BB_6xCA, 0x18);

// 6xCB: Transfer item via mail message (BB)

struct G_TransferItemViaMailMessage_BB_6xCB {
  G_ClientIDHeader header;
  le_uint32_t item_id = 0;
  le_uint32_t amount = 0;
  le_uint32_t target_guild_card_number = 0;
} __packed_ws__(G_TransferItemViaMailMessage_BB_6xCB, 0x10);

// 6xCC: Exchange item for team points (BB) (protected on V3/V4)

struct G_ExchangeItemForTeamPoints_BB_6xCC {
  G_ClientIDHeader header;
  le_uint32_t item_id = 0;
  le_uint32_t amount = 0;
} __packed_ws__(G_ExchangeItemForTeamPoints_BB_6xCC, 0x0C);

// 6xCD: Transfer master (BB)
// 6xCE: Accept master transfer (BB)
// Same format as 6xC1

// 6xCF: Start battle (BB)

struct G_StartBattle_BB_6xCF {
  G_UnusedHeader header;
  BattleRules rules;
} __packed_ws__(G_StartBattle_BB_6xCF, 0x34);

// 6xD0: Battle mode level up (BB; handled by server)
// Requests the client to be leveled up by num_levels levels. The server should
// respond with a 6x30 command.

struct G_BattleModeLevelUp_BB_6xD0 {
  G_ClientIDHeader header;
  le_uint32_t num_levels = 0;
} __packed_ws__(G_BattleModeLevelUp_BB_6xD0, 8);

// 6xD1: Request Challenge Mode grave recovery item (BB; handled by server)

struct G_ChallengeModeGraveRecoveryItemRequest_BB_6xD1 {
  G_ClientIDHeader header;
  le_uint16_t floor = 0;
  le_uint16_t unknown_a1 = 0;
  VectorXZF pos;
  le_uint32_t item_type = 0; // Should be < 6
} __packed_ws__(G_ChallengeModeGraveRecoveryItemRequest_BB_6xD1, 0x14);

// 6xD2: Set quest counter (BB)
// Writes 4 bytes to the 32-bit field specified by index.

struct G_SetQuestCounter_BB_6xD2 {
  G_ClientIDHeader header;
  le_uint32_t index = 0; // There are 0x10 of them (0x00-0x0F)
  le_uint32_t value = 0;
} __packed_ws__(G_SetQuestCounter_BB_6xD2, 0x0C);

// 6xD3: Invalid subcommand

// 6xD4: Unknown (BB)
// Related to object type 0340.

struct G_Unknown_BB_6xD4 {
  G_UnusedHeader header;
  le_uint16_t action = 0; // Must be in [0, 5]
  uint8_t unknown_a1 = 0; // Must be in [0, 15]
  uint8_t unused = 0;
} __packed_ws__(G_Unknown_BB_6xD4, 8);

// 6xD5: Exchange item in quest (BB; handled by server)
// The client sends this when it executes an F953 quest opcode.

struct G_ExchangeItemInQuest_BB_6xD5 {
  G_ClientIDHeader header;
  ItemData find_item; // Only data1[0]-[2] are used
  ItemData replace_item; // Only data1[0]-[2] are used
  le_uint16_t success_label = 0;
  le_uint16_t failure_label = 0;
} __packed_ws__(G_ExchangeItemInQuest_BB_6xD5, 0x30);

// 6xD6: Wrap item (BB; handled by server)

struct G_WrapItem_BB_6xD6 {
  G_ClientIDHeader header;
  ItemData item;
  uint8_t present_color = 0; // 00-0F
  parray<uint8_t, 3> unused;
} __packed_ws__(G_WrapItem_BB_6xD6, 0x1C);

// 6xD7: Paganini Photon Drop exchange (BB; handled by server)
// The client sends this when it executes an F955 quest opcode.

struct G_PaganiniPhotonDropExchange_BB_6xD7 {
  G_ClientIDHeader header;
  ItemData new_item; // Only data1[0]-[2] are used
  le_uint16_t success_label = 0;
  le_uint16_t failure_label = 0;
} __packed_ws__(G_PaganiniPhotonDropExchange_BB_6xD7, 0x1C);

// 6xD8: Add S-rank weapon special (BB; handled by server)
// The client sends this when it executes an F956 quest opcode.

struct G_AddSRankWeaponSpecial_BB_6xD8 {
  G_ClientIDHeader header;
  ItemData unknown_a1; // Only data1[0]-[2] are used
  le_uint32_t item_id = 0;
  le_uint32_t special_type = 0;
  le_uint16_t success_label = 0;
  le_uint16_t failure_label = 0;
} __packed_ws__(G_AddSRankWeaponSpecial_BB_6xD8, 0x24);

// 6xD9: Momoka item exchange (BB; handled by server)
// The client sends this when it executes an F95B quest opcode.

struct G_MomokaItemExchange_BB_6xD9 {
  G_ClientIDHeader header;
  ItemData find_item; // Only data1[0]-[2] are used
  ItemData replace_item; // Only data1[0]-[2] are used
  le_uint32_t token1 = 0; // valueC (from F95B opcode) ^ sender client ID
  le_uint32_t token2 = 0; // valueD (from F95B opcode) ^ sender client ID
  le_uint16_t success_label = 0;
  le_uint16_t failure_label = 0;
} __packed_ws__(G_MomokaItemExchange_BB_6xD9, 0x38);

// 6xDA: Upgrade weapon attribute (BB; handled by server)
// The client sends this when it executes an F957 or F958 quest opcode.

struct G_UpgradeWeaponAttribute_BB_6xDA {
  G_ClientIDHeader header;
  ItemData item; // Only data1[0-2] are used (valueB-valueD)
  le_uint32_t item_id = 0; // valueA
  le_uint32_t attribute = 0; // valueE
  le_uint32_t payment_count = 0; // Number of PD or PS (valueF)
  le_uint32_t payment_type = 0; // 0 = Photon Drops, 1 = Photon Spheres
  le_uint16_t success_label = 0; // labelG
  le_uint16_t failure_label = 0; // labelH
} __packed_ws__(G_UpgradeWeaponAttribute_BB_6xDA, 0x2C);

// 6xDB: Exchange item in quest (BB)

struct G_ExchangeItemInQuest_BB_6xDB {
  G_ClientIDHeader header;
  // If this is 0, the command is identical to 6x29. If this is 1, a function
  // similar to find_item_by_id is called instead of find_item_by_id, but I
  // don't yet know what exactly the logic differences are.
  le_uint32_t unknown_a1 = 0;
  le_uint32_t item_id = 0;
  le_uint32_t amount = 0;
} __packed_ws__(G_ExchangeItemInQuest_BB_6xDB, 0x10);

// 6xDC: Saint-Million boss actions (BB)

struct G_SaintMillionBossActions_BB_6xDC {
  G_UnusedHeader header;
  le_uint16_t unknown_a1 = 0;
  le_uint16_t unknown_a2 = 0;
} __packed_ws__(G_SaintMillionBossActions_BB_6xDC, 8);

// 6xDD: Set EXP multiplier (BB)
// header.param specifies the EXP multiplier. It is 1-based, so the value 2
// means all EXP is doubled, for example.

struct G_SetEXPMultiplier_BB_6xDD {
  G_ParameterHeader header;
} __packed_ws__(G_SetEXPMultiplier_BB_6xDD, 4);

// 6xDE: Exchange Secret Lottery Ticket (BB; handled by server)
// The client sends this when it executes an F95C quest opcode.
// There appears to be a bug in the client here: it sets the subcommand size to
// 2 instead of 3, so the last relevant field (failure_label) is not sent to
// the server.

struct G_ExchangeSecretLotteryTicket_BB_6xDE {
  G_ClientIDHeader header;
  uint8_t index = 0;
  uint8_t unknown_a1 = 0;
  le_uint16_t success_label = 0;
  // le_uint16_t failure_label = 0;
  // parray<uint8_t, 2> unused;
} __packed_ws__(G_ExchangeSecretLotteryTicket_BB_6xDE, 8);

// 6xDF: Exchange Photon Crystals (BB; handled by server)
// The client sends this when it executes an F95D quest opcode.

struct G_ExchangePhotonCrystals_BB_6xDF {
  G_ClientIDHeader header;
} __packed_ws__(G_ExchangePhotonCrystals_BB_6xDF, 4);

// 6xE0: Request item drop from quest (BB; handled by server)
// The client sends this when it executes an F95E quest opcode.

struct G_RequestItemDropFromQuest_BB_6xE0 {
  G_ClientIDHeader header;
  uint8_t floor = 0;
  uint8_t type = 0; // valueA
  uint8_t unknown_a3 = 0;
  uint8_t unused = 0;
  VectorXZF pos; // x = valueB, z = valueC
} __packed_ws__(G_RequestItemDropFromQuest_BB_6xE0, 0x10);

// 6xE1: Exchange Photon Tickets (BB; handled by server)
// The client sends this when it executes an F95F quest opcode.

struct G_ExchangePhotonTickets_BB_6xE1 {
  G_ClientIDHeader header;
  uint8_t unknown_a1 = 0; // valueA
  uint8_t unknown_a2 = 0; // valueB
  uint8_t result_index = 0; // valueC
  uint8_t unused = 0;
  le_uint16_t success_label = 0; // valueD
  le_uint16_t failure_label = 0; // valueE
} __packed_ws__(G_ExchangePhotonTickets_BB_6xE1, 0x0C);

// 6xE2: Get Meseta slot prize (BB)
// The client sends this when it executes an F960 quest opcode.

struct G_GetMesetaSlotPrize_BB_6xE2 {
  G_ClientIDHeader header;
  uint8_t result_tier; // This contains the argument value from the F960 opcode
  uint8_t floor;
  uint8_t unknown_a2;
  uint8_t unused;
  VectorXZF pos; // TODO: Verify this guess
} __packed_ws__(G_GetMesetaSlotPrize_BB_6xE2, 0x10);

// 6xE3: Set Meseta slot prize result (BB)
// The client only uses this to populate the <meseta_slot_prize> quest text
// replacement token.

struct G_SetMesetaSlotPrizeResult_BB_6xE3 {
  G_ClientIDHeader header;
  ItemData item;
} __packed_ws__(G_SetMesetaSlotPrizeResult_BB_6xE3, 0x18);

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

struct G_UpdateHand_Ep3_6xB4x02 {
  /* 00 */ G_CardBattleCommandHeader header = {0xB4, sizeof(G_UpdateHand_Ep3_6xB4x02) / 4, 0, 0x02, 0, 0, 0};
  /* 08 */ le_uint16_t client_id = 0;
  /* 0A */ le_uint16_t unused = 0;
  /* 0C */ Episode3::HandAndEquipState state;
  /* 60 */
} __packed_ws__(G_UpdateHand_Ep3_6xB4x02, 0x60);

// 6xB4x03: Set state flags

struct G_SetStateFlags_Ep3_6xB4x03 {
  /* 00 */ G_CardBattleCommandHeader header = {0xB4, sizeof(G_SetStateFlags_Ep3_6xB4x03) / 4, 0, 0x03, 0, 0, 0};
  /* 08 */ Episode3::StateFlags state;
  /* 20 */
} __packed_ws__(G_SetStateFlags_Ep3_6xB4x03, 0x20);

// 6xB4x04: Update SC/FC short statuses

struct G_UpdateShortStatuses_Ep3_6xB4x04 {
  /* 0000 */ G_CardBattleCommandHeader header = {0xB4, sizeof(G_UpdateShortStatuses_Ep3_6xB4x04) / 4, 0, 0x04, 0, 0, 0};
  /* 0008 */ le_uint16_t client_id = 0;
  /* 000A */ le_uint16_t unused = 0;
  // The slots in this array have heterogeneous meanings. Specifically:
  // [0] is the SC card status
  // [1] through [6] are hand cards
  // [7] through [14] are set FC cards (items/creatures)
  // [15] is the set assist card
  /* 000C */ parray<Episode3::CardShortStatus, 0x10> card_statuses;
  /* 010C */
} __packed_ws__(G_UpdateShortStatuses_Ep3_6xB4x04, 0x10C);

// 6xB4x05: Update map state

struct G_UpdateMap_Ep3NTE_6xB4x05 {
  /* 0000 */ G_CardBattleCommandHeader header = {0xB4, sizeof(G_UpdateMap_Ep3NTE_6xB4x05) / 4, 0, 0x05, 0, 0, 0};
  /* 0008 */ Episode3::MapAndRulesStateTrial state;
  /* 0138 */ uint8_t start_battle = 0;
  /* 0139 */ parray<uint8_t, 3> unused;
  /* 013C */
} __packed_ws__(G_UpdateMap_Ep3NTE_6xB4x05, 0x13C);

struct G_UpdateMap_Ep3_6xB4x05 {
  /* 0000 */ G_CardBattleCommandHeader header = {0xB4, sizeof(G_UpdateMap_Ep3_6xB4x05) / 4, 0, 0x05, 0, 0, 0};
  /* 0008 */ Episode3::MapAndRulesState state;
  /* 0140 */ uint8_t start_battle = 0;
  /* 0141 */ parray<uint8_t, 3> unused;
  /* 0144 */
} __packed_ws__(G_UpdateMap_Ep3_6xB4x05, 0x144);

// 6xB4x06: Apply condition effect

struct G_ApplyConditionEffect_Ep3_6xB4x06 {
  /* 00 */ G_CardBattleCommandHeader header = {0xB4, sizeof(G_ApplyConditionEffect_Ep3_6xB4x06) / 4, 0, 0x06, 0, 0, 0};
  /* 08 */ Episode3::EffectResult effect;
  /* 14 */
} __packed_ws__(G_ApplyConditionEffect_Ep3_6xB4x06, 0x14);

// 6xB4x07: Set battle decks

struct G_UpdateDecks_Ep3_6xB4x07 {
  /* 0000 */ G_CardBattleCommandHeader header = {0xB4, sizeof(G_UpdateDecks_Ep3_6xB4x07) / 4, 0, 0x07, 0, 0, 0};
  /* 0008 */ parray<uint8_t, 4> entries_present;
  /* 000C */ parray<Episode3::DeckEntry, 4> entries;
  /* 016C */
} __packed_ws__(G_UpdateDecks_Ep3_6xB4x07, 0x16C);

// 6xB4x09: Set action state

struct G_SetActionState_Ep3_6xB4x09 {
  /* 00 */ G_CardBattleCommandHeader header = {0xB4, sizeof(G_SetActionState_Ep3_6xB4x09) / 4, 0, 0x09, 0, 0, 0};
  /* 08 */ le_uint16_t client_id = 0;
  /* 0A */ parray<uint8_t, 2> unused;
  /* 0C */ Episode3::ActionState state;
  /* 70 */
} __packed_ws__(G_SetActionState_Ep3_6xB4x09, 0x70);

// 6xB4x0A: Update action chain and metadata
// This command is used by Trial Edition. The final version sends 6xB4x4C,
// 6xB4x4D, and 6xB4x4E instead, but still has a handler for this command.

struct G_UpdateActionChainAndMetadata_Ep3NTE_6xB4x0A {
  /* 0000 */ G_CardBattleCommandHeader header = {0xB4, sizeof(G_UpdateActionChainAndMetadata_Ep3NTE_6xB4x0A) / 4, 0, 0x0A, 0, 0, 0};
  /* 0008 */ le_uint16_t client_id = 0;
  // set_index must be 0xFF, or be in the range [0, 9]. If it's 0xFF, all nine
  // chains and metadatas are cleared for the client; otherwise, the provided
  // chain and metadata are copied into the slot specified by set_index.
  /* 000A */ int8_t index = 0;
  /* 000B */ uint8_t unused = 0;
  /* 000C */ Episode3::ActionChainWithCondsTrial chain;
  /* 010C */ Episode3::ActionMetadata metadata;
  /* 0180 */
} __packed_ws__(G_UpdateActionChainAndMetadata_Ep3NTE_6xB4x0A, 0x180);

struct G_UpdateActionChainAndMetadata_Ep3_6xB4x0A {
  /* 0000 */ G_CardBattleCommandHeader header = {0xB4, sizeof(G_UpdateActionChainAndMetadata_Ep3_6xB4x0A) / 4, 0, 0x0A, 0, 0, 0};
  /* 0008 */ le_uint16_t client_id = 0;
  /* 000A */ int8_t index = 0;
  /* 000B */ uint8_t unused = 0;
  /* 000C */ Episode3::ActionChainWithConds chain;
  /* 010C */ Episode3::ActionMetadata metadata;
  /* 0180 */
} __packed_ws__(G_UpdateActionChainAndMetadata_Ep3_6xB4x0A, 0x180);

// 6xB3x0B / CAx0B: Redraw initial hand (immediately before battle)
// Internal name: ReInitCard

struct G_RedrawInitialHand_Ep3_CAx0B {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_RedrawInitialHand_Ep3_CAx0B) / 4, 0, 0x0B, 0, 0, 0, 0, 0};
  le_uint16_t client_id = 0;
  parray<uint8_t, 2> unused2;
} __packed_ws__(G_RedrawInitialHand_Ep3_CAx0B, 0x14);

// 6xB3x0C / CAx0C: End initial redraw phase
// Internal name: StartGame

struct G_EndInitialRedrawPhase_Ep3_CAx0C {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_EndInitialRedrawPhase_Ep3_CAx0C) / 4, 0, 0x0C, 0, 0, 0, 0, 0};
  le_uint16_t client_id = 0;
  parray<uint8_t, 2> unused2;
} __packed_ws__(G_EndInitialRedrawPhase_Ep3_CAx0C, 0x14);

// 6xB3x0D / CAx0D: End non-action phase
// Internal names: EndPhaseDice, EndPhaseSet, EndPhaseMove, EndPhaseDraw
// This command is sent when the client has no more actions to take during the
// current phase. This command isn't used for ending the attack or defense
// phases; for those phases, CAx12 and CAx28 are used instead.

struct G_EndNonAttackPhase_Ep3_CAx0D {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_EndNonAttackPhase_Ep3_CAx0D) / 4, 0, 0x0D, 0, 0, 0, 0, 0};
  le_uint16_t client_id = 0;
  le_uint16_t battle_phase = 0; // Episode3::BattlePhase enum
  le_uint16_t param1; // If battle_phase == DICE, this is 1 if the ATK value is larger than the DEF value
  parray<le_uint16_t, 3> unused2;
} __packed_ws__(G_EndNonAttackPhase_Ep3_CAx0D, 0x1C);

// 6xB3x0E / CAx0E: Discard card from hand
// Internal name: ThrowCard

struct G_DiscardCardFromHand_Ep3_CAx0E {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_DiscardCardFromHand_Ep3_CAx0E) / 4, 0, 0x0E, 0, 0, 0, 0, 0};
  le_uint16_t client_id = 0;
  le_uint16_t card_ref = 0xFFFF;
} __packed_ws__(G_DiscardCardFromHand_Ep3_CAx0E, 0x14);

// 6xB3x0F / CAx0F: Set card from hand
// Internal names: SetCardItem, SetCardEnemy, SetCardCreature

struct G_SetCardFromHand_Ep3_CAx0F {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_SetCardFromHand_Ep3_CAx0F) / 4, 0, 0x0F, 0, 0, 0, 0, 0};
  le_uint16_t client_id = 0;
  le_uint16_t card_ref = 0xFFFF;
  le_uint16_t set_index = 0;
  le_uint16_t assist_target_player = 0;
  Episode3::Location loc;
} __packed_ws__(G_SetCardFromHand_Ep3_CAx0F, 0x1C);

// 6xB3x10 / CAx10: Move field character
// Internal name: MoveCard

struct G_MoveFieldCharacter_Ep3_CAx10 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_MoveFieldCharacter_Ep3_CAx10) / 4, 0, 0x10, 0, 0, 0, 0, 0};
  le_uint16_t client_id = 0;
  le_uint16_t set_index = 0;
  Episode3::Location loc;
} __packed_ws__(G_MoveFieldCharacter_Ep3_CAx10, 0x18);

// 6xB3x11 / CAx11: Enqueue action (play card(s) during action phase)
// Internal name: ExecActionReq
// This command is used for playing both attacks (and the associated action
// cards), and for playing defense cards. In the attack case, this command is
// sent once for each attack (even if it includes multiple cards); in the
// defense case, this command is sent once for each defense card.

struct G_EnqueueAttackOrDefense_Ep3_CAx11 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_EnqueueAttackOrDefense_Ep3_CAx11) / 4, 0, 0x11, 0, 0, 0, 0, 0};
  le_uint16_t client_id = 0;
  parray<uint8_t, 2> unused2;
  Episode3::ActionState entry;
} __packed_ws__(G_EnqueueAttackOrDefense_Ep3_CAx11, 0x78);

// 6xB3x12 / CAx12: End attack list (done playing cards during action phase)
// Internal name: ExecActionCalc
// This command informs the server that the client is done playing attacks in
// the current round. (In the defense phase, CAx28 is used instead.)

struct G_EndAttackList_Ep3_CAx12 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_EndAttackList_Ep3_CAx12) / 4, 0, 0x12, 0, 0, 0, 0, 0};
  le_uint16_t client_id = 0;
  parray<uint8_t, 2> unused2;
} __packed_ws__(G_EndAttackList_Ep3_CAx12, 0x14);

// 6xB3x13 / CAx13: Set map state during setup

struct G_SetMapState_Ep3NTE_CAx13 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_SetMapState_Ep3NTE_CAx13) / 4, 0, 0x13, 0, 0, 0, 0, 0};
  Episode3::MapAndRulesStateTrial map_and_rules_state;
  Episode3::OverlayState overlay_state;
} __packed_ws__(G_SetMapState_Ep3NTE_CAx13, 0x2B4);

struct G_SetMapState_Ep3_CAx13 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_SetMapState_Ep3_CAx13) / 4, 0, 0x13, 0, 0, 0, 0, 0};
  Episode3::MapAndRulesState map_and_rules_state;
  Episode3::OverlayState overlay_state;
} __packed_ws__(G_SetMapState_Ep3_CAx13, 0x2BC);

// 6xB3x14 / CAx14: Set player deck during setup

struct G_SetPlayerDeck_Ep3_CAx14 {
  /* 00 */ G_CardServerDataCommandHeader header = {0xB3, sizeof(G_SetPlayerDeck_Ep3_CAx14) / 4, 0, 0x14, 0, 0, 0, 0, 0};
  /* 10 */ le_uint16_t client_id = 0;
  /* 12 */ uint8_t is_cpu_player = 0;
  /* 13 */ uint8_t unused2 = 0;
  /* 14 */ Episode3::DeckEntry entry;
  /* 6C */
} __packed_ws__(G_SetPlayerDeck_Ep3_CAx14, 0x6C);

// 6xB3x15 / CAx15: Hard-reset server state
// This command appears to be completely unused; the client never sends it.

struct G_HardResetServerState_Ep3_CAx15 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_HardResetServerState_Ep3_CAx15) / 4, 0, 0x15, 0, 0, 0, 0, 0};
  // No arguments
} __packed_ws__(G_HardResetServerState_Ep3_CAx15, 0x10);

// 6xB5x17: Unknown
// TODO: Document this from Episode 3 client/server disassembly

struct G_Unknown_Ep3_6xB5x17 {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_Unknown_Ep3_6xB5x17) / 4, 0, 0x17, 0, 0, 0};
  // No arguments
} __packed_ws__(G_Unknown_Ep3_6xB5x17, 8);

// 6xB5x1A: Force disconnect
// This command seems to cause the client to unconditionally disconnect. The
// player is returned to the main menu (the "The line was disconnected" message
// box is skipped). Unlike all other known ways to disconnect, the client does
// not save when it receives this command, and instead returns directly to the
// main menu.

struct G_ForceDisconnect_Ep3_6xB5x1A {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_ForceDisconnect_Ep3_6xB5x1A) / 4, 0, 0x1A, 0, 0, 0};
  // No arguments
} __packed_ws__(G_ForceDisconnect_Ep3_6xB5x1A, 8);

// 6xB3x1B / CAx1B: Set player name during setup
// Curiously, this command can be used during a non-setup phase; the server
// should ignore the command's contents but still send a 6xB4x1C in response.

struct G_SetPlayerName_Ep3_CAx1B {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_SetPlayerName_Ep3_CAx1B) / 4, 0, 0x1B, 0, 0, 0, 0, 0};
  Episode3::NameEntry entry;
} __packed_ws__(G_SetPlayerName_Ep3_CAx1B, 0x24);

// 6xB4x1C: Set all player names

struct G_SetPlayerNames_Ep3_6xB4x1C {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_SetPlayerNames_Ep3_6xB4x1C) / 4, 0, 0x1C, 0, 0, 0};
  parray<Episode3::NameEntry, 4> entries;
} __packed_ws__(G_SetPlayerNames_Ep3_6xB4x1C, 0x58);

// 6xB3x1D / CAx1D: Request for battle start
// The battle actually begins when the server sends a state flags update (in
// response to this command) that includes RegistrationPhase::BATTLE_STARTED and
// a SetupPhase value other than REGISTRATION.

struct G_StartBattle_Ep3_CAx1D {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_StartBattle_Ep3_CAx1D) / 4, 0, 0x1D, 0, 0, 0, 0, 0};
} __packed_ws__(G_StartBattle_Ep3_CAx1D, 0x10);

// 6xB4x1E: Action result

struct G_ActionResult_Ep3_6xB4x1E {
  /* 00 */ G_CardBattleCommandHeader header = {0xB4, sizeof(G_ActionResult_Ep3_6xB4x1E) / 4, 0, 0x1E, 0, 0, 0};
  /* 08 */ be_uint32_t sequence_num = 0;
  /* 0C */ uint8_t error_code = 0;
  /* 0D */ uint8_t response_phase = 0;
  /* 0E */ parray<uint8_t, 2> unused;
  /* 10 */
} __packed_ws__(G_ActionResult_Ep3_6xB4x1E, 0x10);

// 6xB4x1F: Set context token
// This token is sent back in the context_token field of all CA commands from
// the client. It seems Sega never used this functionality.

struct G_SetContextToken_Ep3_6xB4x1F {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_SetContextToken_Ep3_6xB4x1F) / 4, 0, 0x1F, 0, 0, 0};
  // Note that this field is little-endian, but the corresponding context_token
  // field in G_CardServerDataCommandHeader is big-endian!
  le_uint32_t context_token = 0;
} __packed_ws__(G_SetContextToken_Ep3_6xB4x1F, 0x0C);

// 6xB5x20: Unknown
// TODO: Document this from Episode 3 client/server disassembly

struct G_Unknown_Ep3_6xB5x20 {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_Unknown_Ep3_6xB5x20) / 4, 0, 0x20, 0, 0, 0};
  le_uint32_t player_tag = 0x00010000;
  le_uint32_t guild_card_number = 0;
  uint8_t client_id = 0; // Not bounds checked! Should be < 0x0C
  parray<uint8_t, 3> unused;
} __packed_ws__(G_Unknown_Ep3_6xB5x20, 0x14);

// 6xB3x21 / CAx21: End battle

struct G_EndBattle_Ep3_CAx21 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_EndBattle_Ep3_CAx21) / 4, 0, 0x21, 0, 0, 0, 0, 0};
  le_uint32_t unused2 = 0;
} __packed_ws__(G_EndBattle_Ep3_CAx21, 0x14);

// 6xB4x22: Unknown
// This command appears to be completely unused. The client's handler for this
// command sets a variable on some data structure if it exists, but it appears
// that that data structure is never allocated.

struct G_Unknown_Ep3_6xB4x22 {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_Unknown_Ep3_6xB4x22) / 4, 0, 0x22, 0, 0, 0};
  // No arguments
} __packed_ws__(G_Unknown_Ep3_6xB4x22, 8);

// 6xB4x23: Unknown
// This command was actually sent by Sega's original servers, but it does
// nothing on the client.

struct G_Unknown_Ep3_6xB4x23 {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_Unknown_Ep3_6xB4x23) / 4, 0, 0x23, 0, 0, 0};
  uint8_t present = 0; // Handler expects this to be equal to 1
  uint8_t client_id = 0;
  parray<uint8_t, 2> unused;
} __packed_ws__(G_Unknown_Ep3_6xB4x23, 0x0C);

// 6xB5x27: Unknown
// TODO: Document this from Episode 3 client/server disassembly

struct G_Unknown_Ep3_6xB5x27 {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_Unknown_Ep3_6xB5x27) / 4, 0, 0x27, 0, 0, 0};
  le_uint32_t unknown_a1 = 0; // Probably client ID (must be < 4)
  le_uint32_t unknown_a2 = 0; // Must be < 0x10
  le_uint32_t unknown_a3 = 0;
  le_uint32_t unused = 0; // Curiously, this usually contains a memory address
} __packed_ws__(G_Unknown_Ep3_6xB5x27, 0x18);

// 6xB3x28 / CAx28: End defense list
// This command informs the server that the client is done playing defense
// cards. (In the attack phase, CAx12 is used instead.) attack_number matches
// the attack_number sent in the previous 6xB4x29 command.

struct G_EndDefenseList_Ep3_CAx28 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_EndDefenseList_Ep3_CAx28) / 4, 0, 0x28, 0, 0, 0, 0, 0};
  uint8_t attack_number = 0;
  uint8_t client_id = 0;
  parray<uint8_t, 2> unused;
} __packed_ws__(G_EndDefenseList_Ep3_CAx28, 0x14);

// 6xB4x29: Update attack targets
// This is sent when the server begins computing the results of an attack. It
// updates the targets used in the attack (e.g. if targeted items were
// destroyed by a previous attack).

struct G_UpdateAttackTargets_Ep3_6xB4x29 {
  /* 00 */ G_CardBattleCommandHeader header = {0xB4, sizeof(G_UpdateAttackTargets_Ep3_6xB4x29) / 4, 0, 0x29, 0, 0, 0};
  /* 08 */ uint8_t attack_number = 0;
  /* 09 */ parray<uint8_t, 3> unused;
  /* 0C */ Episode3::ActionState state;
  /* 70 */
} __packed_ws__(G_UpdateAttackTargets_Ep3_6xB4x29, 0x70);

// 6xB4x2A: Unknown
// This appears to be unused, even on NTE. It writes an entry into an array of
// four 6-byte structures (doing nothing if there are already 4 present), but
// nothing reads from this array.

struct G_Unknown_Ep3_6xB4x2A {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_Unknown_Ep3_6xB4x2A) / 4, 0, 0x2A, 0, 0, 0};
  parray<uint8_t, 4> unknown_a1;
  le_uint16_t unknown_a2 = 0;
  parray<uint8_t, 2> unused;
} __packed_ws__(G_Unknown_Ep3_6xB4x2A, 0x10);

// 6xB3x2B / CAx2B: Legacy set card
// It seems Sega's servers completely ignored this command. The command name is
// based on a debug message found nearby.

struct G_ExecLegacyCard_Ep3_CAx2B {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_ExecLegacyCard_Ep3_CAx2B) / 4, 0, 0x2B, 0, 0, 0, 0, 0};
  le_uint16_t unused2 = 0;
  parray<uint8_t, 2> unused3;
} __packed_ws__(G_ExecLegacyCard_Ep3_CAx2B, 0x14);

// 6xB4x2C: Enqueue animation
// This is used for playing the trap and teleport animations (with change_type
// = 1). It's also used for playing the discard entire hand animation (with
// change_type = 3).

struct G_EnqueueAnimation_Ep3_6xB4x2C {
  /* 00 */ G_CardBattleCommandHeader header = {0xB4, sizeof(G_EnqueueAnimation_Ep3_6xB4x2C) / 4, 0, 0x2C, 0, 0, 0};
  /* 08 */ uint8_t change_type = 0;
  /* 09 */ uint8_t client_id = 0;
  /* 0A */ parray<le_uint16_t, 3> card_refs;
  /* 10 */ Episode3::Location loc;
  /* 14 */ parray<le_uint32_t, 2> unknown_a2;
  /* 1C */
} __packed_ws__(G_EnqueueAnimation_Ep3_6xB4x2C, 0x1C);

// 6xB5x2D: Recreate multiple players

struct G_RecreateMultiplePlayers_Ep3_6xB5x2D {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_RecreateMultiplePlayers_Ep3_6xB5x2D) / 4, 0, 0x2D, 0, 0, 0};
  // This array is indexed by client ID. When a client receives this command
  // and its corresponding entry in this array is not zero, it sends a 6x70
  // command to itself containing its own player data. It's not clear what the
  // function of this is intended to be.
  // TODO: Figure out if tournament fast loading can be implemented using this
  // to fix the stuck-in-wall glitch.
  parray<uint8_t, 4> client_ids_to_recreate;
} __packed_ws__(G_RecreateMultiplePlayers_Ep3_6xB5x2D, 0x0C);

// 6xB5x2E: Notify other players that battle is about to end

struct G_BattleEndNotification_Ep3_6xB5x2E {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_BattleEndNotification_Ep3_6xB5x2E) / 4, 0, 0x2E, 0, 0, 0};
  // Values for end_type:
  // 0 = Battle results screen
  // 1 = Go directly to Morgue? (TODO: test this)
  // Anything else = command is ignored
  uint8_t end_type = 0;
  parray<uint8_t, 3> unused;
} __packed_ws__(G_BattleEndNotification_Ep3_6xB5x2E, 0x0C);

// 6xB5x2F: Set deck in battle setup menu

struct Ep3CounterPlayerEntry {
  /* 00 */ uint8_t is_valid;
  /* 01 */ uint8_t entry_type;
  /* 02 */ uint8_t client_id;
  /* 03 */ uint8_t client_id2;
  /* 04 */ pstring<TextEncoding::MARKED, 20> player_name;
  /* 18 */ pstring<TextEncoding::MARKED, 25> deck_name;
  /* 31 */ uint8_t deck_type;
  /* 32 */ uint8_t unknown_a1a;
  /* 33 */ uint8_t unknown_a1b;
  /* 34 */ be_uint16_t unknown_a3;
  /* 36 */ le_uint16_t unknown_a2;
  /* 38 */ parray<le_uint16_t, 0x1F> card_ids;
  /* 76 */ parray<uint8_t, 2> unused;
  /* 78 */ le_uint32_t unknown_a5;
  /* 7C */ be_uint16_t unknown_a6;
  /* 7E */ be_uint16_t unknown_a7;
  /* 80 */
} __packed_ws__(Ep3CounterPlayerEntry, 0x80);

struct G_SetDeckInBattleSetupMenu_Ep3_6xB5x2F {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_SetDeckInBattleSetupMenu_Ep3_6xB5x2F) / 4, 0, 0x2F, 0, 0, 0};
  parray<uint8_t, 4> unknown_a1;
  Ep3CounterPlayerEntry entry;
} __packed_ws__(G_SetDeckInBattleSetupMenu_Ep3_6xB5x2F, 0x8C);

// 6xB5x30: Unused
// The client never sends this command, and when the client receives this
// command, it does nothing. It's likely that there was some commented-out code
// in the original source, since the handler byteswaps the command and calls
// is_online() and local_client_is_leader(), then ignores the results and
// returns immediately.

struct G_Unused_Ep3_6xB5x30 {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_Unused_Ep3_6xB5x30) / 4, 0, 0x30, 0, 0, 0};
  // No arguments
} __packed_ws__(G_Unused_Ep3_6xB5x30, 8);

// 6xB5x31: Confirm deck selection

struct G_ConfirmDeckSelection_Ep3_6xB5x31 {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_ConfirmDeckSelection_Ep3_6xB5x31) / 4, 0, 0x31, 0, 0, 0};
  uint8_t unknown_a1 = 0; // Must be 0 or 1
  uint8_t unknown_a2 = 0; // Must be < 4
  uint8_t unknown_a3 = 0; // Must be 0xFF or < 4
  uint8_t unknown_a4 = 0; // Must be < 0x14
  uint8_t menu_type = 0; // Not bounds-checked; should be < 0x15
  parray<uint8_t, 3> unused;
} __packed_ws__(G_ConfirmDeckSelection_Ep3_6xB5x31, 0x10);

// 6xB5x32: Move shared menu cursor

struct G_MoveSharedMenuCursor_Ep3_6xB5x32 {
  /* 00 */ G_CardBattleCommandHeader header = {0xB5, sizeof(G_MoveSharedMenuCursor_Ep3_6xB5x32) / 4, 0, 0x32, 0, 0, 0};
  /* 08 */ le_int16_t selected_item_index = -1; // Must not be < -1
  /* 0A */ le_int16_t chosen_item_index = -1; // Must not be < -1
  /* 0C */ uint8_t unknown_a1 = 0; // Must be 0, 1, or 2
  /* 0D */ uint8_t unknown_a2 = 0; // Must be less than 0x14
  /* 0E */ uint8_t unknown_a3 = 0; // Must be less than 0x14
  /* 0F */ uint8_t unknown_a4 = 0; // Must be 0 or 1
  /* 10 */ uint8_t menu_type = 0; // Not bounds-checked; should be < 0x15
  /* 11 */ parray<uint8_t, 3> unused;
  /* 14 */
} __packed_ws__(G_MoveSharedMenuCursor_Ep3_6xB5x32, 0x14);

// 6xB4x33: Subtract ally ATK points (e.g. for photon blast)

struct G_SubtractAllyATKPoints_Ep3_6xB4x33 {
  /* 00 */ G_CardBattleCommandHeader header = {0xB4, sizeof(G_SubtractAllyATKPoints_Ep3_6xB4x33) / 4, 0, 0x33, 0, 0, 0};
  /* 08 */ uint8_t client_id = 0;
  /* 09 */ uint8_t ally_cost = 0;
  /* 0A */ le_uint16_t card_ref = 0xFFFF;
  /* 0C */
} __packed_ws__(G_SubtractAllyATKPoints_Ep3_6xB4x33, 0x0C);

// 6xB3x34 / CAx34: Photon blast request

struct G_PhotonBlastRequest_Ep3_CAx34 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_PhotonBlastRequest_Ep3_CAx34) / 4, 0, 0x34, 0, 0, 0, 0, 0};
  uint8_t ally_client_id = 0;
  uint8_t reason = 0;
  le_uint16_t card_ref = 0xFFFF;
} __packed_ws__(G_PhotonBlastRequest_Ep3_CAx34, 0x14);

// 6xB4x35: Update photon blast status

struct G_PhotonBlastStatus_Ep3_6xB4x35 {
  /* 00 */ G_CardBattleCommandHeader header = {0xB4, sizeof(G_PhotonBlastStatus_Ep3_6xB4x35) / 4, 0, 0x35, 0, 0, 0};
  /* 08 */ uint8_t client_id = 0;
  /* 09 */ uint8_t accepted = 0;
  /* 0A */ le_uint16_t card_ref = 0xFFFF;
  /* 0C */
} __packed_ws__(G_PhotonBlastStatus_Ep3_6xB4x35, 0x0C);

// 6xB5x36: Recreate player
// Setting client_id to a value 4 or greater while in a game causes the player
// to be temporarily replaced with a default HUmar and placed inside the central
// column in the Morgue, rendering them unable to move. The only ways out of
// this predicament appear to be either to disconnect (e.g. select Quit Game
// from the pause menu) or receive an ED (force leave game) command.

struct G_RecreatePlayer_Ep3_6xB5x36 {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_RecreatePlayer_Ep3_6xB5x36) / 4, 0, 0x36, 0, 0, 0};
  uint8_t client_id = 0;
  parray<uint8_t, 3> unused;
} __packed_ws__(G_RecreatePlayer_Ep3_6xB5x36, 0x0C);

// 6xB3x37 / CAx37: Ready to advance from starting rolls phase

struct G_AdvanceFromStartingRollsPhase_Ep3_CAx37 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_AdvanceFromStartingRollsPhase_Ep3_CAx37) / 4, 0, 0x37, 0, 0, 0, 0, 0};
  uint8_t client_id = 0;
  parray<uint8_t, 3> unused2;
} __packed_ws__(G_AdvanceFromStartingRollsPhase_Ep3_CAx37, 0x14);

// 6xB5x38: Card counts request
// This command causes the client identified by requested_client_id to send a
// 6xBC command to the client identified by reply_to_client_id (privately, via
// the 6D command). This is sent at the beginning of the card trade window
// sequence.

struct G_CardCountsRequest_Ep3_6xB5x38 {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_CardCountsRequest_Ep3_6xB5x38) / 4, 0, 0x38, 0, 0, 0};
  uint8_t requested_client_id = 0;
  uint8_t reply_to_client_id = 0;
  parray<uint8_t, 2> unused;
} __packed_ws__(G_CardCountsRequest_Ep3_6xB5x38, 0x0C);

// 6xB4x39: Update all player statistics

struct G_UpdateAllPlayerStatistics_Ep3NTE_6xB4x39 {
  /* 00 */ G_CardBattleCommandHeader header = {0xB4, sizeof(G_UpdateAllPlayerStatistics_Ep3NTE_6xB4x39) / 4, 0, 0x39, 0, 0, 0};
  /* 08 */ parray<Episode3::PlayerBattleStatsTrial, 4> stats;
  /* 58 */
} __packed_ws__(G_UpdateAllPlayerStatistics_Ep3NTE_6xB4x39, 0x58);

struct G_UpdateAllPlayerStatistics_Ep3_6xB4x39 {
  /* 00 */ G_CardBattleCommandHeader header = {0xB4, sizeof(G_UpdateAllPlayerStatistics_Ep3_6xB4x39) / 4, 0, 0x39, 0, 0, 0};
  /* 08 */ parray<Episode3::PlayerBattleStats, 4> stats;
  /* A8 */
} __packed_ws__(G_UpdateAllPlayerStatistics_Ep3_6xB4x39, 0xA8);

// 6xB3x3A / CAx3A: Overall time limit expired
// It seems Sega's servers completely ignored this command and used server-side
// timing instead. newserv does the same.

struct G_OverallTimeLimitExpired_Ep3_CAx3A {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_OverallTimeLimitExpired_Ep3_CAx3A) / 4, 0, 0x3A, 0, 0, 0, 0, 0};
} __packed_ws__(G_OverallTimeLimitExpired_Ep3_CAx3A, 0x10);

// 6xB4x3B: Load current environment
// This command is used to send spectators in a spectator team to the main
// battle. A 6xB4x05 and 6xB6x41 command should have been sent before this, to
// set the map state that should appear for the new spectator.

struct G_LoadCurrentEnvironment_Ep3_6xB4x3B {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_LoadCurrentEnvironment_Ep3_6xB4x3B) / 4, 0, 0x3B, 0, 0, 0};
  parray<uint8_t, 4> unused;
} __packed_ws__(G_LoadCurrentEnvironment_Ep3_6xB4x3B, 0x0C);

// 6xB5x3C: Set player substatus
// This command sets the text that appears under the player's name in the HUD.

struct G_SetPlayerSubstatus_Ep3_6xB5x3C {
  // Note: header.sender_client_id specifies which client's status to update
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_SetPlayerSubstatus_Ep3_6xB5x3C) / 4, 0, 0x3C, 0, 0, 0};
  // Status values:
  // 00 (or any value not listed below) = (nothing)
  // 01 = Editing
  // 02 = Trading...
  // 03 = At Counter
  uint8_t status = 0;
  parray<uint8_t, 3> unused;
} __packed_ws__(G_SetPlayerSubstatus_Ep3_6xB5x3C, 0x0C);

// 6xB4x3D: Set tournament player decks
// This is sent before the counter sequence in a tournament game, to reserve the
// player and COM slots and set the map number.

template <typename RulesT>
struct G_SetTournamentPlayerDecksT_Ep3_6xB4x3D {
  /* NTE :Final */
  /* 0000:0000 */ G_CardBattleCommandHeader header = {0xB4, sizeof(G_SetTournamentPlayerDecksT_Ep3_6xB4x3D<RulesT>) / 4, 0, 0x3D, 0, 0, 0};
  /* 0008:0008 */ RulesT rules;
  struct Entry {
    /* 00 */ uint8_t type = 0; // 0 = no player, 1 = human, 2 = COM
    /* 01 */ pstring<TextEncoding::MARKED, 0x10> player_name;
    /* 11 */ pstring<TextEncoding::MARKED, 0x10> deck_name; // Only used for COM players
    /* 21 */ parray<uint8_t, 5> unknown_a1;
    /* 26 */ parray<le_uint16_t, 0x1F> card_ids; // Can be blank for human players
    /* 64 */ uint8_t client_id = 0; // Unused for COMs
    /* 65 */ uint8_t unknown_a4 = 0;
    /* 66 */ le_uint16_t unknown_a2 = 0;
    /* 68 */ le_uint16_t unknown_a3 = 0;
    /* 6A */
  } __packed_ws__(Entry, 0x6A);
  /* 0014:001C */ parray<Entry, 4> entries;
  /* 01BC:01C4 */ le_uint32_t map_number = 0;
  /* 01C0:01C8 */ uint8_t player_slot = 0; // Which deck slot is editable by the client
  /* 01C1:01C9 */ uint8_t unknown_a3 = 0;
  /* 01C2:01CA */ uint8_t unknown_a4 = 0;
  /* 01C3:01CB */ uint8_t unknown_a5 = 0;
  /* 01C4:01CC */
} __packed__;

using G_SetTournamentPlayerDecks_Ep3NTE_6xB4x3D = G_SetTournamentPlayerDecksT_Ep3_6xB4x3D<Episode3::RulesTrial>;
check_struct_size(G_SetTournamentPlayerDecks_Ep3NTE_6xB4x3D, 0x1C4);
using G_SetTournamentPlayerDecks_Ep3_6xB4x3D = G_SetTournamentPlayerDecksT_Ep3_6xB4x3D<Episode3::Rules>;
check_struct_size(G_SetTournamentPlayerDecks_Ep3_6xB4x3D, 0x1CC);

// 6xB5x3E: Make card auction bid

struct G_MakeCardAuctionBid_Ep3_6xB5x3E {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_MakeCardAuctionBid_Ep3_6xB5x3E) / 4, 0, 0x3E, 0, 0, 0};
  uint8_t card_index = 0; // Index of card in EF command
  uint8_t bid_value = 0; // 1-99
  parray<uint8_t, 2> unused;
} __packed_ws__(G_MakeCardAuctionBid_Ep3_6xB5x3E, 0x0C);

// 6xB5x3F: Open blocking menu
// This command opens a shared menu between all clients in a game. The client
// specified in .client_id is able to control the menu; the other clients see
// that player's actions but cannot control anything.

struct G_OpenBlockingMenu_Ep3_6xB5x3F {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_OpenBlockingMenu_Ep3_6xB5x3F) / 4, 0, 0x3F, 0, 0, 0};
  // Menu type should be one of these values:
  // 0xFF = close all menus? (TODO: verify this)
  // 0x01/0x02 = battle prep menu
  // 0x11 = card auction counter menu (join or cancel)
  // 0x12 = go directly to card auction state (client sends EF command)
  int8_t menu_type = 0; // Must be in the range [-1, 0x14]
  uint8_t client_id = 0;
  parray<uint8_t, 2> unused1;
  le_uint32_t unknown_a3 = 0;
  parray<uint8_t, 4> unused2;
} __packed_ws__(G_OpenBlockingMenu_Ep3_6xB5x3F, 0x14);

// 6xB3x40 / CAx40: Request map list
// The server should respond with a 6xB6x40 command.

struct G_MapListRequest_Ep3_CAx40 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_MapListRequest_Ep3_CAx40) / 4, 0, 0x40, 0, 0, 0, 0, 0};
} __packed_ws__(G_MapListRequest_Ep3_CAx40, 0x10);

// 6xB3x41 / CAx41: Request map data
// The server should respond with a 6xB6x41 command containing the definition of
// the specified map.

struct G_MapDataRequest_Ep3_CAx41 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_MapDataRequest_Ep3_CAx41) / 4, 0, 0x41, 0, 0, 0, 0, 0};
  le_uint32_t map_number = 0;
} __packed_ws__(G_MapDataRequest_Ep3_CAx41, 0x14);

// 6xB5x42: Initiate card auction
// Sending this command to a client has the same effect as sending a 6xB5x3F
// command to tell it to open the auction menu. (This works even if the client
// doesn't have a VIP card or there are fewer than 4 players in the current
// game.) Under normal operation, the server doesn't need to do this - the
// client sends this when all of the following conditions are met:
// 1. The client has a VIP card. (This is stored client-side in seq flag 7000.)
// 2. The client is in a game with 4 players.
// 3. All clients are at the auction counter.

struct G_InitiateCardAuction_Ep3_6xB5x42 {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_InitiateCardAuction_Ep3_6xB5x42) / 4, 0, 0x42, 0, 0, 0};
} __packed_ws__(G_InitiateCardAuction_Ep3_6xB5x42, 8);

// 6xB5x43: Unknown
// This command stores the card IDs and counts in a global array on the client,
// but this array is never read from. It's likely this is a remnant of an
// unimplemented or removed feature, or an earlier implementation of the card
// trade window.

struct G_Unknown_Ep3_6xB5x43 {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_Unknown_Ep3_6xB5x43) / 4, 0, 0x43, 0, 0, 0};
  struct Entry {
    // Both fields here are masked. To get the actual values used by the game,
    // XOR the values here with 0x39AB.
    le_uint16_t masked_card_id = 0xFFFF; // Must be < 0x2F1 (when unmasked)
    le_uint16_t masked_count = 0; // Must be in [1, 99] (when unmasked)
  } __packed_ws__(Entry, 4);
  parray<Entry, 0x14> entries;
} __packed_ws__(G_Unknown_Ep3_6xB5x43, 0x58);

// 6xB5x44: Card auction bid summary

struct G_CardAuctionBidSummary_Ep3_6xB5x44 {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_CardAuctionBidSummary_Ep3_6xB5x44) / 4, 0, 0x44, 0, 0, 0};
  parray<le_uint16_t, 8> bids; // In same order as cards in the EF command
} __packed_ws__(G_CardAuctionBidSummary_Ep3_6xB5x44, 0x18);

// 6xB5x45: Card auction results

struct G_CardAuctionResults_Ep3_6xB5x45 {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_CardAuctionResults_Ep3_6xB5x45) / 4, 0, 0x45, 0, 0, 0};
  // This array is indexed by [card_index][client_id], and contains the final
  // bid for each player on each card (or 0 if they did not bid on that card).
  parray<parray<le_uint16_t, 4>, 8> bids_by_player;
} __packed_ws__(G_CardAuctionResults_Ep3_6xB5x45, 0x48);

// 6xB4x46: Server version strings
// This command doesn't seem to be necessary to actually play the game; the
// client just copies the included strings to global buffers and then ignores
// them. Sega's servers sent this twice for each battle, however: once after the
// initial setup phase (before starter rolls) and once when the results screen
// appeared. The second instance of this command appears to be caused by them
// recreating the TCardServer object (implemented in newserv's Episode3::Server)
// in order to support multiple sequential battles in the same team.

struct G_ServerVersionStrings_Ep3_6xB4x46 {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_ServerVersionStrings_Ep3_6xB4x46) / 4, 0, 0x46, 0, 0, 0};
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
  pstring<TextEncoding::MARKED, 0x40> version_signature;
  pstring<TextEncoding::MARKED, 0x40> date_str1; // Probably card definitions revision date
  // In Sega's implementation, it seems this field is blank when starting a
  // battle, and contains the current time (in the format "YYYY/MM/DD hh:mm:ss")
  // when ending a battle. This may have been used for identifying debug logs.
  pstring<TextEncoding::MARKED, 0x40> date_str2;
  // This field contains uninitialized memory when the client generates this
  // command. In normal operation, however, the client would never send this;
  // it would be handled locally in offline mode, and generated by the server
  // in online mode. The client completely ignores this field in either case.
  le_uint32_t unused = 0;
} __packed_ws__(G_ServerVersionStrings_Ep3_6xB4x46, 0xCC);

struct G_ServerVersionStrings_Ep3NTE_6xB4x46 {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_ServerVersionStrings_Ep3NTE_6xB4x46) / 4, 0, 0x46, 0, 0, 0};
  // Ep3 NTE uses the following strings:
  // "03/05/29 18:00 by K.Toya"
  pstring<TextEncoding::MARKED, 0x40> version_signature;
  // "Jun 11 2003 05:02:36"
  pstring<TextEncoding::MARKED, 0x40> date_str1;
} __packed_ws__(G_ServerVersionStrings_Ep3NTE_6xB4x46, 0x88);

// 6xB5x47: Set online player's CARD level
// header.sender_client_id is the player's client ID.

struct G_SetPlayerCARDLevel_Ep3_6xB5x47 {
  G_CardBattleCommandHeader header = {0xB5, sizeof(G_SetPlayerCARDLevel_Ep3_6xB5x47) / 4, 0, 0x47, 0, 0, 0};
  le_uint32_t clv = 0;
} __packed_ws__(G_SetPlayerCARDLevel_Ep3_6xB5x47, 0x0C);

// 6xB3x48 / CAx48: End turn
// Internal name: DrawCardReq

struct G_EndTurn_Ep3_CAx48 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_EndTurn_Ep3_CAx48) / 4, 0, 0x48, 0, 0, 0, 0, 0};
  uint8_t client_id = 0;
  parray<uint8_t, 3> unused2;
} __packed_ws__(G_EndTurn_Ep3_CAx48, 0x14);

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
// Episode 3 Trial Edition does not send this command.

struct G_CardCounts_Ep3_CAx49 {
  G_CardServerDataCommandHeader header = {0xB3, sizeof(G_CardCounts_Ep3_CAx49) / 4, 0, 0x49, 0, 0, 0, 0, 0};
  uint8_t basis = 0;
  parray<uint8_t, 3> unused;
  // This is encrypted with the trivial algorithm (see decrypt_trivial_gci_data)
  // using the basis in the preceding field
  parray<uint8_t, 0x2F0> card_id_to_count;
} __packed_ws__(G_CardCounts_Ep3_CAx49, 0x304);

// 6xB4x4A: Add to set card log
// This command is not valid on Episode 3 Trial Edition.

struct G_AddToSetCardLog_Ep3_6xB4x4A {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_AddToSetCardLog_Ep3_6xB4x4A) / 4, 0, 0x4A, 0, 0, 0};
  // Note: entry_count appears not to be bounds-checked; presumably the server
  // could send up to 0xFF entries, but those after the 8th would not be
  // byteswapped before the client handles them.
  uint8_t client_id = 0;
  uint8_t entry_count = 0;
  le_uint16_t round_num = 0;
  parray<le_uint16_t, 8> card_refs;
} __packed_ws__(G_AddToSetCardLog_Ep3_6xB4x4A, 0x1C);

// 6xB4x4B: Set EX result values
// This command is not valid on Episode 3 Trial Edition.
// This command specifies how much EX the player should get based on the
// difference between their level and the levels of their opponents. (For
// multi-player opponent teams, the average of the opponents' levels is used.)
// The game scans the appropriate list for the entry whose threshold is less
// than or equal to than the level difference, and returns the corresponding
// value. For example, if the first two entries in the win list are {20, 40}
// and {10, 30}, and the player defeats an opponent who is 15 levels above the
// player's level, the player will get 30 EX when they win the battle. If all
// thresholds are greater than the level difference, the last entry's value is
// used. Finally, if the opponent team has no humans on it, the resulting EX
// values are divided by 2 (so in the example above, the player would only get
// 15 EX for defeating COMs).

// If any entry in either list has .value < -100 or > 100, the entire command is
// ignored and the EX thresholds and values are reset to their default values.
// These default values are:
// win_entries = {50, 100}, {30, 80}, {15, 70}, {10, 55}, {7, 45}, {4, 35},
//               {1, 25}, {-1, 20}, {-9, 15}, {0, 10}
// lose_entries = {1, 0}, {-2, 0}, {-3, 0}, {-4, 0}, {-5, 0}, {-6, 0}, {-7, 0},
//                {-10, -10}, {-30, -10}, {0, -15}

struct G_SetEXResultValues_Ep3_6xB4x4B {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_SetEXResultValues_Ep3_6xB4x4B) / 4, 0, 0x4B, 0, 0, 0};
  struct Entry {
    le_int16_t threshold = 0;
    le_int16_t value = 0;
  } __packed_ws__(Entry, 4);
  parray<Entry, 10> win_entries;
  parray<Entry, 10> lose_entries;
} __packed_ws__(G_SetEXResultValues_Ep3_6xB4x4B, 0x58);

// 6xB4x4C: Update action chain
// This command is not valid on Episode 3 Trial Edition.

struct G_UpdateActionChain_Ep3_6xB4x4C {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_UpdateActionChain_Ep3_6xB4x4C) / 4, 0, 0x4C, 0, 0, 0};
  uint8_t client_id = 0;
  int8_t index = 0;
  parray<uint8_t, 2> unused;
  Episode3::ActionChain chain;
} __packed_ws__(G_UpdateActionChain_Ep3_6xB4x4C, 0x7C);

// 6xB4x4D: Update action metadata
// This command is not valid on Episode 3 Trial Edition.

struct G_UpdateActionMetadata_Ep3_6xB4x4D {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_UpdateActionMetadata_Ep3_6xB4x4D) / 4, 0, 0x4D, 0, 0, 0};
  uint8_t client_id = 0;
  int8_t index = 0;
  parray<uint8_t, 2> unused;
  Episode3::ActionMetadata metadata;
} __packed_ws__(G_UpdateActionMetadata_Ep3_6xB4x4D, 0x80);

// 6xB4x4E: Update card conditions
// This command is not valid on Episode 3 Trial Edition.

struct G_UpdateCardConditions_Ep3_6xB4x4E {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_UpdateCardConditions_Ep3_6xB4x4E) / 4, 0, 0x4E, 0, 0, 0};
  uint8_t client_id = 0;
  int8_t index = 0;
  parray<uint8_t, 2> unused;
  parray<Episode3::Condition, 9> conditions;
} __packed_ws__(G_UpdateCardConditions_Ep3_6xB4x4E, 0x9C);

// 6xB4x4F: Clear set card conditions
// This command is not valid on Episode 3 Trial Edition.

struct G_ClearSetCardConditions_Ep3_6xB4x4F {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_ClearSetCardConditions_Ep3_6xB4x4F) / 4, 0, 0x4F, 0, 0, 0};
  uint8_t client_id = 0;
  uint8_t unused = 0;
  // For each 1 bit in this mask, the conditions of the corresponding card
  // should be deleted. The low bit corresponds to the SC card; the next bit
  // corresponds to set slot 0, the next bit to set slot 1, etc. (The upper 7
  // bits of this field are unused.)
  le_uint16_t clear_mask = 0;
} __packed_ws__(G_ClearSetCardConditions_Ep3_6xB4x4F, 0x0C);

// 6xB4x50: Set trap tile locations
// This command is not valid on Episode 3 Trial Edition.

struct G_SetTrapTileLocations_Ep3_6xB4x50 {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_SetTrapTileLocations_Ep3_6xB4x50) / 4, 0, 0x50, 0, 0, 0};
  // Each entry in this array corresponds to one of the 5 trap types, in order.
  // Each entry is an [x, y] pair; if that trap type is not present, its
  // location entry is FF FF.
  parray<parray<uint8_t, 2>, 5> locations;
  parray<uint8_t, 2> unused;
} __packed_ws__(G_SetTrapTileLocations_Ep3_6xB4x50, 0x14);

// 6xB4x51: Tournament match result
// This command is not valid on Episode 3 Trial Edition.
// This is sent as soon as the battle result is determined (before the battle
// results screen). If the client is in tournament mode (tournament_flag is 1 in
// the StateFlags struct), then it will use this information to show the
// tournament match result screen before the battle results screen.

struct G_TournamentMatchResult_Ep3_6xB4x51 {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_TournamentMatchResult_Ep3_6xB4x51) / 4, 0, 0x51, 0, 0, 0};
  pstring<TextEncoding::MARKED, 0x40> match_description;
  struct NamesEntry {
    pstring<TextEncoding::MARKED, 0x20> team_name;
    parray<pstring<TextEncoding::MARKED, 0x10>, 2> player_names;
  } __packed_ws__(NamesEntry, 0x40);
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
  pstring<TextEncoding::MARKED, 0x20> meseta_reward_text;
} __packed_ws__(G_TournamentMatchResult_Ep3_6xB4x51, 0xF4);

// 6xB4x52: Set game metadata
// This command is not valid on Episode 3 Trial Edition.
// This is sent to all players in a game and all attached spectator teams when
// any player joins or leaves any spectator team watching the same game.

struct G_SetGameMetadata_Ep3_6xB4x52 {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_SetGameMetadata_Ep3_6xB4x52) / 4, 0, 0x52, 0, 0, 0};
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
  // If text_size is not zero, the text is shown in the top bar instead of the
  // usual message ("Viewing Battle", "Time left: XX:XX", and the like).
  le_uint16_t text_size = 0;
  pstring<TextEncoding::MARKED, 0x100> text;
} __packed_ws__(G_SetGameMetadata_Ep3_6xB4x52, 0x110);

// 6xB4x53: Reject battle start request
// This command is not valid on Episode 3 Trial Edition.
// This is sent in response to a CAx1D command if setup isn't complete (e.g. if
// some names/decks are missing or invalid). Under normal operation, this should
// never happen.
// Note: It seems the client ignores everything in this structure; the command
// handler just sets a global state flag and returns immediately.

struct G_RejectBattleStartRequest_Ep3_6xB4x53 {
  G_CardBattleCommandHeader header = {0xB4, sizeof(G_RejectBattleStartRequest_Ep3_6xB4x53) / 4, 0, 0x53, 0, 0, 0};
  Episode3::SetupPhase setup_phase;
  Episode3::RegistrationPhase registration_phase;
  parray<uint8_t, 2> unused;
  Episode3::MapAndRulesState state;
} __packed_ws__(G_RejectBattleStartRequest_Ep3_6xB4x53, 0x144);

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// EXTENDED COMMANDS ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// These commands are not part of the official protocol; newserv uses these
// along with client functions to implement extended functionality.

// 30 (C->S): Extended player info
// Requested with the GetExtendedPlayerInfo patch. Format depends on version:
//   DC v2: PSODCV2CharacterFile
//   GC v3: PSOGCCharacterFile::Character
//   XB v3: PSOXBCharacterFileCharacter
