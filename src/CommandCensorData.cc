#include "CommandCensorData.hh"

#include "CommandFormats.hh"

std::pair<const void*, size_t> censor_data_for_client_command(Version version, uint16_t command) {
  switch (command) {
    case 0x03: {
      static const C_LegacyLogin_PC_V3_03 ret{
          .hardware_id = 0,
          .sub_version = 0,
          .is_extended = 0,
          .language = Language::JAPANESE,
          .unused = 0,
          .serial_number2 = 1,
          .access_key2 = 1,
      };
      return std::make_pair(&ret, sizeof(ret));
    }
    case 0x04:
      if (is_patch(version)) {
        static const C_Login_Patch_04 ret{.unused{0}, .username{1}, .password{1}, .email_address{1}};
        return std::make_pair(&ret, sizeof(ret));
      } else if (!is_v4(version)) {
        static const C_LegacyLogin_PC_V3_04 ret{
            .hardware_id = 0,
            .sub_version = 0,
            .is_extended = 0,
            .language = Language::JAPANESE,
            .unused = 0,
            .serial_number = 1,
            .access_key = 1,
        };
        return std::make_pair(&ret, sizeof(ret));
      } else {
        static const C_LegacyLogin_BB_04 ret{
            .sub_version = 0,
            .is_extended = 0,
            .language = Language::JAPANESE,
            .unused = 0,
            .username = 1,
            .password = 1};
        return std::make_pair(&ret, sizeof(ret));
      }
    case 0x88:
      if (version == Version::DC_NTE) {
        static const C_Login_DCNTE_88 ret{.serial_number{1}, .access_key{1}};
        return std::make_pair(&ret, sizeof(ret));
      } else {
        return std::make_pair(nullptr, 0);
      }
    case 0x8A:
      if (version == Version::DC_NTE) {
        static const C_ConnectionInfo_DCNTE_8A ret{
            .hardware_id = 0, .sub_version = 0, .unused = 0, .username = 1, .password = 1, .email_address = 1};
        return std::make_pair(&ret, sizeof(ret));
      } else {
        return std::make_pair(nullptr, 0);
      }
    case 0x8B:
      if (version == Version::DC_NTE) {
        static const C_Login_DCNTE_8B ret{
            .player_tag = 0,
            .guild_card_number = 0,
            .hardware_id = 0,
            .sub_version = 0,
            .is_extended = 0,
            .language = Language::JAPANESE,
            .unused1 = 0,
            .serial_number = 1,
            .access_key = 1,
            .username = 1,
            .password = 1,
            .login_character_name = 0,
            .unused = 0,
        };
        return std::make_pair(&ret, sizeof(ret));
      } else {
        return std::make_pair(nullptr, 0);
      }
    case 0x90: {
      static const C_LoginV1_DC_PC_V3_90 ret{.serial_number = 1, .access_key = 1};
      return std::make_pair(&ret, sizeof(ret));
    }
    case 0x92: {
      static const C_RegisterV1_DC_92 ret{
          .hardware_id = 0,
          .sub_version = 0,
          .unused1 = 0,
          .language = Language::JAPANESE,
          .unused2 = 0,
          .serial_number2 = 1,
          .access_key2 = 1,
          .email_address = 1,
      };
      return std::make_pair(&ret, sizeof(ret));
    }
    case 0x93:
      if (!is_v4(version)) {
        static const C_LoginV1_DC_93 ret{
            .player_tag = 0,
            .guild_card_number = 0,
            .hardware_id = 0,
            .sub_version = 0,
            .is_extended = 0,
            .language = Language::JAPANESE,
            .unused1 = 0,
            .serial_number = 1,
            .access_key = 1,
            .serial_number2 = 1,
            .access_key2 = 1,
            .login_character_name = 0,
            .unused2 = 0,
        };
        return std::make_pair(&ret, sizeof(ret));
      } else {
        static const C_LoginBase_BB_93 ret{
            .player_tag = 0,
            .guild_card_number = 0,
            .sub_version = 0,
            .language = Language::JAPANESE,
            .character_slot = 0,
            .connection_phase = 0,
            .client_code = 0,
            .security_token = 0,
            .username = 1,
            .password = 1,
            .menu_id = 0,
            .preferred_lobby_id = 0,
        };
        return std::make_pair(&ret, sizeof(ret));
      }
    case 0x9A: {
      static const C_Login_DC_PC_V3_9A ret{
          .v1_serial_number = 1,
          .v1_access_key = 1,
          .serial_number = 1,
          .access_key = 1,
          .player_tag = 0,
          .guild_card_number = 0,
          .sub_version = 0,
          .serial_number2 = 1,
          .access_key2 = 1,
          .email_address = 1,
      };
      return std::make_pair(&ret, sizeof(ret));
    }
    case 0x9C:
      if (!is_v4(version)) {
        static const C_Register_DC_PC_V3_9C ret{
            .hardware_id = 0,
            .sub_version = 0,
            .unused1 = 0,
            .language = Language::JAPANESE,
            .unused2 = 0,
            .serial_number = 1,
            .access_key = 1,
            .password = 1,
        };
        return std::make_pair(&ret, sizeof(ret));
      } else {
        static const C_Register_BB_9C ret{
            .sub_version = 0,
            .unused1 = 0,
            .language = Language::JAPANESE,
            .unused2 = 0,
            .username = 1,
            .password = 1,
            .game_tag = 0,
        };
        return std::make_pair(&ret, sizeof(ret));
      }
    case 0x9D:
    case 0x9E:
      if (!is_v4(version)) {
        static const C_Login_DC_PC_GC_9D ret{
            .player_tag = 0,
            .guild_card_number = 0,
            .hardware_id = 0,
            .sub_version = 0,
            .is_extended = 0,
            .language = Language::JAPANESE,
            .unused3 = 0,
            .v1_serial_number = 1,
            .v1_access_key = 1,
            .serial_number = 1,
            .access_key = 1,
            .serial_number2 = 1,
            .access_key2 = 1,
            .login_character_name = 0,
        };
        return std::make_pair(&ret, sizeof(ret));
      } else {
        static const C_LoginExtended_BB_9E ret{
            .player_tag = 0,
            .guild_card_number = 0,
            .sub_version = 0,
            .language32 = 0,
            .unknown_a2 = 0,
            .v1_serial_number = 1,
            .v1_access_key = 1,
            .serial_number = 1,
            .access_key = 1,
            .username = 1,
            .password = 1,
            .guild_card_number_str = 0,
            .client_config = 0,
            .extension{},
        };
        return std::make_pair(&ret, sizeof(ret));
      }
    case 0xDB:
      if (!is_v4(version)) {
        static const C_VerifyAccount_V3_DB ret{
            .v1_serial_number = 1,
            .v1_access_key = 1,
            .serial_number = 1,
            .access_key = 1,
            .hardware_id = 0,
            .sub_version = 0,
            .serial_number2 = 1,
            .access_key2 = 1,
            .password = 1,
        };
        return std::make_pair(&ret, sizeof(ret));
      } else {
        static const C_VerifyAccount_BB_DB ret{
            .v1_serial_number = 1,
            .v1_access_key = 1,
            .serial_number = 1,
            .access_key = 1,
            .sub_version = 0,
            .username = 1,
            .password = 1,
            .game_tag = 0,
        };
        return std::make_pair(&ret, sizeof(ret));
      }
    default:
      return std::make_pair(nullptr, 0);
  }
}
