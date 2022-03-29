#include <memory>
#include <string>

#include "Client.hh"
#include "ServerState.hh"



// These commands' structures are defined here because they're used by both the
// game server and proxy server

struct VerifyLicenseCommand_GC_DB {
  char unused[0x20];
  char serial_number[0x10];
  char access_key[0x10];
  char unused2[0x08];
  uint32_t sub_version;
  char serial_number2[0x30];
  char access_key2[0x30];
  char password[0x30];
} __attribute__((packed));

struct LoginCommand_GC_9E {
  uint32_t player_tag; // 00 00 01 00 if guild card is set (via 04)
  uint32_t guild_card_number; // FF FF FF FF if not set
  uint32_t unused1[2];
  uint32_t sub_version;
  uint8_t unused2[0x24]; // 00 01 00 00 ... (rest is 00)
  char serial_number[0x10];
  char access_key[0x10];
  char serial_number2[0x30];
  char access_key2[0x30];
  char name[0x10];
  // Note: there are 8 bytes at the end of cfg that are technically not
  // included in the client config on GC, but the field after it is
  // sufficiently large and unused anyway
  ClientConfig cfg;
  uint8_t unused4[0x5C];
} __attribute__((packed));



void process_connect(std::shared_ptr<ServerState> s, std::shared_ptr<Client> c);
void process_disconnect(std::shared_ptr<ServerState> s,
    std::shared_ptr<Client> c);
void process_command(std::shared_ptr<ServerState> s, std::shared_ptr<Client> c,
    uint16_t command, uint32_t flag, uint16_t size, const void* data);
