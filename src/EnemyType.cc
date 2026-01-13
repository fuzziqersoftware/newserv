#include "BattleParamsIndex.hh"

#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>

#include "Loggers.hh"
#include "PSOEncryption.hh"
#include "StaticGameData.hh"

using namespace std;

static constexpr uint8_t EP1 = EnemyTypeDefinition::Flag::VALID_EP1;
static constexpr uint8_t EP2 = EnemyTypeDefinition::Flag::VALID_EP2;
static constexpr uint8_t EP4 = EnemyTypeDefinition::Flag::VALID_EP4;
static constexpr uint8_t RARE = EnemyTypeDefinition::Flag::IS_RARE;
static constexpr uint8_t BOSS = EnemyTypeDefinition::Flag::IS_BOSS;

static constexpr uint8_t NONE = 0xFF;
static const vector<EnemyTypeDefinition> type_defs{
    // clang-format off
    // TYPE                              FLAGS                          RT    OLDRT BP-STATS      BP-ATTACK                       BP-RESIST     ENUM NAME                  IN-GAME NAME   ULTIMATE NAME
    {EnemyType::UNKNOWN,                 0,                             NONE, NONE, {},           {},                             {},           "UNKNOWN",                 "__UNKNOWN__", nullptr},
    {EnemyType::NONE,                    0,                             NONE, NONE, {},           {},                             {},           "NONE",                    "__NONE__", nullptr},
    {EnemyType::NON_ENEMY_NPC,           EP1 | EP2 | EP4,               NONE, NONE, {},           {},                             {},           "NON_ENEMY_NPC",           "__NPC__", nullptr},
    {EnemyType::AL_RAPPY,                EP1 |             RARE,        0x06, 0x06, {0x19},       {0x19},                         {0x19},       "AL_RAPPY",                "Al Rappy", "Pal Rappy"},
    {EnemyType::ASTARK,                              EP4,               0x58, 0x41, {0x09},       {0x0B, 0x0A, 0x0C},             {0x09},       "ASTARK",                  "Astark", nullptr},
    {EnemyType::BA_BOOTA,                            EP4,               0x62, 0x4F, {0x03},       {0x03, 0x02, 0x04},             {0x03},       "BA_BOOTA",                "Ba Boota", nullptr},
    {EnemyType::BARBA_RAY,                     EP2 |              BOSS, 0x49, 0x49, {0x0F},       {0x0F},                         {0x0F},       "BARBA_RAY",               "Barba Ray", nullptr},
    {EnemyType::BARBA_RAY_JOINT,               EP2 |              BOSS, 0x49, 0x49, {0x10},       {0x0F},                         {0x10},       "BARBA_RAY_JOINT",         "Barba Ray (joint)", nullptr},
    {EnemyType::BARBAROUS_WOLF,          EP1 | EP2,                     0x08, 0x08, {0x03},       {0x03},                         {0x03},       "BARBAROUS_WOLF",          "Barbarous Wolf", "Gulgus-gue"},
    {EnemyType::BEE_L,                   EP1 | EP2,                     NONE, NONE, {0x0C},       {0x0C},                         {0x0C},       "BEE_L",                   "Bee L", "Gee L"},
    {EnemyType::BEE_R,                   EP1 | EP2,                     NONE, NONE, {0x0B},       {0x0B},                         {0x0B},       "BEE_R",                   "Bee R", "Gee R"},
    {EnemyType::BOOMA,                   EP1,                           0x09, 0x09, {0x4B},       {0x4E},                         {0x4A},       "BOOMA",                   "Booma", "Bartle"},
    {EnemyType::BOOTA,                               EP4,               0x60, 0x4D, {0x00},       {0x00, 0x02, 0x04},             {0x00},       "BOOTA",                   "Boota", nullptr},
    {EnemyType::BULCLAW,                 EP1,                           0x28, 0x28, {0x1F},       {0x1F},                         {0x1F},       "BULCLAW",                 "Bulclaw", nullptr},
    {EnemyType::BULK,                    EP1,                           0x27, 0x27, {0x1F},       {0x1F},                         {0x1F},       "BULK",                    "Bulk", nullptr},
    {EnemyType::CANADINE,                EP1,                           0x1C, 0x1C, {0x07},       {0x07},                         {0x07},       "CANADINE",                "Canadine", "Canabin"},
    {EnemyType::CANADINE_GROUP,          EP1,                           0x1C, 0x1C, {0x08},       {0x08},                         {0x08},       "CANADINE_GROUP",          "Canadine (group)", "Canabin (group)"},
    {EnemyType::CANANE,                  EP1,                           0x1D, 0x1D, {0x09},       {0x09},                         {0x09},       "CANANE",                  "Canane", "Canune"},
    {EnemyType::CHAOS_BRINGER,           EP1,                           0x24, 0x24, {0x0D},       {0x0D},                         {0x0D},       "CHAOS_BRINGER",           "Chaos Bringer", "Dark Bringer"},
    {EnemyType::CHAOS_SORCERER,          EP1 | EP2,                     0x1F, 0x1F, {0x0A},       {0x0A},                         {0x0A},       "CHAOS_SORCERER",          "Chaos Sorceror", "Gran Sorceror"},
    {EnemyType::CLAW,                    EP1,                           0x26, 0x26, {0x20},       {0x20},                         {0x20},       "CLAW",                    "Claw", nullptr},
    {EnemyType::DARK_BELRA,              EP1 | EP2,                     0x25, 0x25, {0x0E},       {0x0E, 0x13},                   {0x0E},       "DARK_BELRA",              "Dark Belra", "Indi Belra"},
    {EnemyType::DARK_FALZ_1,             EP1                    | BOSS, NONE, NONE, {0x36},       {0x36},                         {0x36},       "DARK_FALZ_1",             "Dark Falz (phase 1)", nullptr},
    {EnemyType::DARK_FALZ_2,             EP1                    | BOSS, 0x2F, 0x2F, {0x37},       {0x37},                         {0x37},       "DARK_FALZ_2",             "Dark Falz (phase 2)", nullptr},
    {EnemyType::DARK_FALZ_3,             EP1                    | BOSS, 0x2F, 0x2F, {0x38},       {0x38},                         {0x38},       "DARK_FALZ_3",             "Dark Falz (phase 3)", nullptr},
    {EnemyType::DARK_GUNNER,             EP1,                           0x22, 0x22, {0x1E},       {0x1E},                         {0x1E},       "DARK_GUNNER",             "Dark Gunner", nullptr},
    {EnemyType::DARK_GUNNER_CONTROL,     EP1,                           NONE, NONE, {},           {},                             {},           "DARK_GUNNER_CONTROL",     "Dark Gunner (control)", nullptr},
    {EnemyType::DARVANT,                 EP1,                           NONE, NONE, {0x35},       {0x35},                         {0x35},       "DARVANT",                 "Darvant", nullptr},
    {EnemyType::DE_ROL_LE,               EP1                    | BOSS, 0x2D, 0x2D, {0x0F},       {0x0F},                         {0x0F},       "DE_ROL_LE",               "De Rol Le", "Dal Ral Lie"},
    {EnemyType::DE_ROL_LE_BODY,          EP1                    | BOSS, NONE, NONE, {0x10},       {0x0F},                         {0x10},       "DE_ROL_LE_BODY",          "De Rol Le (body)", "Dal Ral Lie (body)"},
    {EnemyType::DE_ROL_LE_MINE,          EP1                    | BOSS, NONE, NONE, {0x11},       {0x0F},                         {0x11},       "DE_ROL_LE_MINE",          "De Rol Le (mine)", "Dal Ral Lie (mine)"},
    {EnemyType::DEATH_GUNNER,            EP1,                           0x23, 0x23, {0x1E},       {0x1E},                         {0x1E},       "DEATH_GUNNER",            "Death Gunner", nullptr},
    {EnemyType::DEL_LILY,                      EP2,                     0x53, 0x53, {0x25},       {0x25},                         {0x25},       "DEL_LILY",                "Del Lily", nullptr},
    {EnemyType::DEL_RAPPY_CRATER,                    EP4,               0x69, 0x57, {0x06},       {0x06},                         {0x06},       "DEL_RAPPY_CRATER",        "Del Rappy (crater)", nullptr},
    {EnemyType::DEL_RAPPY_DESERT,                    EP4,               0x69, 0x58, {0x18},       {0x18},                         {0x18},       "DEL_RAPPY_DESERT",        "Del Rappy (desert)", nullptr},
    {EnemyType::DELBITER,                      EP2,                     0x48, 0x48, {0x0D},       {0x0D},                         {0x0D},       "DELBITER",                "Delbiter", nullptr},
    {EnemyType::DELDEPTH,                      EP2,                     0x47, 0x47, {0x30},       {0x30},                         {0x30},       "DELDEPTH",                "Deldepth", nullptr},
    {EnemyType::DELSABER,                EP1 | EP2,                     0x1E, 0x1E, {0x52},       {0x57, 0x58, 0x59},             {0x51},       "DELSABER",                "Delsaber", nullptr},
    {EnemyType::DIMENIAN,                EP1 | EP2,                     0x29, 0x29, {0x53},       {0x5A},                         {0x52},       "DIMENIAN",                "Dimenian", "Arlan"},
    {EnemyType::DOLMDARL,                      EP2,                     0x41, 0x41, {0x50},       {0x55},                         {0x4F},       "DOLMDARL",                "Dolmdarl", nullptr},
    {EnemyType::DOLMOLM,                       EP2,                     0x40, 0x40, {0x4F},       {0x54},                         {0x4E},       "DOLMOLM",                 "Dolmolm", nullptr},
    {EnemyType::DORPHON,                             EP4,               0x63, 0x50, {0x0F},       {0x0F},                         {0x0F},       "DORPHON",                 "Dorphon", nullptr},
    {EnemyType::DORPHON_ECLAIR,                      EP4 | RARE,        0x64, 0x51, {0x10},       {0x10},                         {0x10},       "DORPHON_ECLAIR",          "Dorphon Eclair", nullptr},
    {EnemyType::DRAGON,                  EP1                    | BOSS, 0x2C, 0x2C, {0x12},       {0x12, 0x14, 0x15, 0x16, 0x17}, {0x12},       "DRAGON",                  "Dragon", "Sil Dragon"},
    {EnemyType::DUBCHIC,                 EP1 | EP2,                     0x18, 0x18, {0x1B},       {0x1B},                         {0x1B},       "DUBCHIC",                 "Dubchic", "Dubchich"},
    {EnemyType::DUBWITCH,                EP1 | EP2,                     NONE, NONE, {},           {},                             {},           "DUBWITCH",                "Dubwitch", "Duvuik"},
    {EnemyType::EGG_RAPPY,                     EP2,                     0x51, 0x51, {0x19},       {0x19},                         {0x19},       "EGG_RAPPY",               "Egg Rappy", nullptr},
    {EnemyType::EPSIGARD,                      EP2,                     NONE, NONE, {0x24},       {0x24},                         {0x24},       "EPSIGARD",                "Episgard", nullptr},
    {EnemyType::EPSILON,                       EP2,                     0x54, 0x54, {0x23},       {0x23},                         {0x23},       "EPSILON",                 "Epsilon", nullptr},
    {EnemyType::EVIL_SHARK,              EP1,                           0x10, 0x10, {0x4F},       {0x54},                         {0x4E},       "EVIL_SHARK",              "Evil Shark", "Vulmer"},
    {EnemyType::GAEL_OR_GIEL,                  EP2,                     NONE, NONE, {0x2E},       {0x2E},                         {0x2E},       "GAEL_OR_GIEL",            "Gael/Giel", nullptr},
    {EnemyType::GAL_GRYPHON,                   EP2              | BOSS, 0x4D, 0x4D, {0x1E},       {0x1E, 0x1F, 0x20, 0x21, 0x22}, {0x1E},       "GAL_GRYPHON",             "Gal Gryphon", nullptr},
    {EnemyType::GARANZ,                  EP1 | EP2,                     0x19, 0x19, {0x1D},       {0x1D},                         {0x1D},       "GARANZ",                  "Garanz", "Baranz"},
    {EnemyType::GEE,                           EP2,                     0x36, 0x36, {0x07},       {0x07},                         {0x07},       "GEE",                     "Gee", nullptr},
    {EnemyType::GI_GUE,                        EP2,                     0x37, 0x37, {0x1A},       {0x1A},                         {0x1A},       "GI_GUE",                  "Gi Gue", nullptr},
    {EnemyType::GIBBLES,                       EP2,                     0x3D, 0x3D, {0x3D},       {0x3D, 0x3E, 0x3F},             {0x3D},       "GIBBLES",                 "Gibbles", nullptr},
    {EnemyType::GIGOBOOMA,               EP1,                           0x0B, 0x0B, {0x4D},       {0x50},                         {0x4C},       "GIGOBOOMA",               "Gigobooma", "Tollaw"},
    {EnemyType::GILLCHIC,                EP1 | EP2,                     0x32, 0x32, {0x1C},       {0x1C},                         {0x1C},       "GILLCHIC",                "Gillchic", "Gillchich"},
    {EnemyType::GIRTABLULU,                          EP4,               0x5D, 0x48, {0x1F},       {0x1F},                         {0x1F},       "GIRTABLULU",              "Girtablulu", nullptr},
    {EnemyType::GOBOOMA,                 EP1,                           0x0A, 0x0A, {0x4C},       {0x4F},                         {0x4B},       "GOBOOMA",                 "Gobooma", "Barble"},
    {EnemyType::GOL_DRAGON,                    EP2              | BOSS, 0x4C, 0x4C, {0x12},       {0x12, 0x14, 0x15, 0x16, 0x17}, {0x12},       "GOL_DRAGON",              "Gol Dragon", nullptr},
    {EnemyType::GORAN,                               EP4,               0x65, 0x52, {0x11},       {0x11, 0x14},                   {0x11},       "GORAN",                   "Goran", nullptr},
    {EnemyType::GORAN_DETONATOR,                     EP4,               0x66, 0x53, {0x13},       {0x13, 0x16},                   {0x13},       "GORAN_DETONATOR",         "Goran Detonator", nullptr},
    {EnemyType::GRASS_ASSASSIN,          EP1 | EP2,                     0x0C, 0x0C, {0x4E},       {0x51, 0x52, 0x53},             {0x4D},       "GRASS_ASSASSIN",          "Grass Assassin", "Crimson Assassin"},
    {EnemyType::GUIL_SHARK,              EP1,                           0x12, 0x12, {0x51},       {0x56},                         {0x50},       "GUIL_SHARK",              "Guil Shark", "Melqueek"},
    {EnemyType::HALLO_RAPPY,                   EP2,                     0x50, 0x50, {0x19},       {0x19},                         {0x19},       "HALLO_RAPPY",             "Hallo Rappy", nullptr},
    {EnemyType::HIDOOM,                  EP1 | EP2,                     0x17, 0x17, {0x32},       {0x32},                         {0x32},       "HIDOOM",                  "Hidoom", nullptr},
    {EnemyType::HILDEBEAR,               EP1 | EP2,                     0x01, 0x01, {0x49},       {0x48, 0x49, 0x4A},             {0x48},       "HILDEBEAR",               "Hildebear", "Hildelt"},
    {EnemyType::HILDEBLUE,               EP1 | EP2       | RARE,        0x02, 0x02, {0x4A},       {0x4B, 0x4C, 0x4D},             {0x49},       "HILDEBLUE",               "Hildeblue", "Hildetorr"},
    {EnemyType::ILL_GILL,                      EP2,                     0x52, 0x52, {0x26},       {0x26, 0x27, 0x28, 0x29},       {0x26},       "ILL_GILL",                "Ill Gill", nullptr},
    {EnemyType::KONDRIEU,                            EP4 | RARE | BOSS, 0x6C, 0x5B, {0x28, 0x2A}, {0x28, 0x2A},                   {0x28, 0x2A}, "KONDRIEU",                "Kondrieu", nullptr},
    {EnemyType::KONDRIEU_SPINNER,                    EP4 | RARE | BOSS, 0x6C, 0x5B, {0x29, 0x2B}, {0x29, 0x2B},                   {0x29, 0x2B}, "KONDRIEU_SPINNER",        "Kondrieu (spinner)", nullptr},
    {EnemyType::LA_DIMENIAN,             EP1 | EP2,                     0x2A, 0x2A, {0x54},       {0x5B},                         {0x53},       "LA_DIMENIAN",             "La Dimenian", "Merlan"},
    {EnemyType::LOVE_RAPPY,                    EP2,                     0x33, 0x33, {0x19},       {0x19},                         {0x19},       "LOVE_RAPPY",              "Love Rappy", nullptr},
    {EnemyType::MERICARAND,                    EP2,                     0x38, 0x38, {0x3A},       {0x3A},                         {0x3A},       "MERICARAND",              "Mericarand", nullptr},
    {EnemyType::MERICAROL,                     EP2,                     0x38, 0x38, {0x3A},       {0x3A},                         {0x3A},       "MERICAROL",               "Mericarol", nullptr},
    {EnemyType::MERICUS,                       EP2       | RARE,        0x3A, 0x3A, {0x46},       {0x46},                         {0x46},       "MERICUS",                 "Mericus", nullptr},
    {EnemyType::MERIKLE,                       EP2       | RARE,        0x39, 0x39, {0x45},       {0x45},                         {0x45},       "MERIKLE",                 "Merikle", nullptr},
    {EnemyType::MERILLIA,                      EP2,                     0x34, 0x34, {0x4B},       {0x4E},                         {0x4A},       "MERILLIA",                "Merillia", nullptr},
    {EnemyType::MERILTAS,                      EP2,                     0x35, 0x35, {0x4C},       {0x4F},                         {0x4B},       "MERILTAS",                "Meriltas", nullptr},
    {EnemyType::MERISSA_A,                           EP4,               0x5B, 0x46, {0x19},       {0x19},                         {0x19},       "MERISSA_A",               "Merissa A", nullptr},
    {EnemyType::MERISSA_AA,                          EP4 | RARE,        0x5C, 0x47, {0x1A},       {0x1A},                         {0x1A},       "MERISSA_AA",              "Merissa AA", nullptr},
    {EnemyType::MIGIUM,                  EP1 | EP2,                     0x16, 0x16, {0x33},       {0x33},                         {0x33},       "MIGIUM",                  "Migium", nullptr},
    {EnemyType::MONEST,                  EP1 | EP2,                     0x04, 0x04, {0x01},       {0x01},                         {0x01},       "MONEST",                  "Monest", "Mothvist"},
    {EnemyType::MORFOS,                        EP2,                     0x42, 0x42, {0x40},       {0x40, 0x50},                   {0x40},       "MORFOS",                  "Morfos", nullptr},
    {EnemyType::MOTHMANT,                EP1 | EP2,                     0x03, 0x03, {0x00},       {0x00},                         {0x00},       "MOTHMANT",                "Mothmant", "Mothvert"},
    {EnemyType::NANO_DRAGON,             EP1,                           0x0F, 0x0F, {0x1A},       {0x1A},                         {0x1A},       "NANO_DRAGON",             "Nano Dragon", nullptr},
    {EnemyType::NAR_LILY,                EP1 | EP2       | RARE,        0x0E, 0x0E, {0x05},       {0x05},                         {0x05},       "NAR_LILY",                "Nar Lily", "Mil Lily"},
    {EnemyType::OLGA_FLOW_1,                   EP2              | BOSS, NONE, NONE, {0x2B},       {0x2B},                         {0x2B},       "OLGA_FLOW_1",             "Olga Flow (phase 1)", nullptr},
    {EnemyType::OLGA_FLOW_2,                   EP2              | BOSS, 0x4E, 0x4E, {0x2C},       {0x2C},                         {0x2C},       "OLGA_FLOW_2",             "Olga Flow (phase 2)", nullptr},
    {EnemyType::PAL_SHARK,               EP1,                           0x11, 0x11, {0x50},       {0x55},                         {0x4F},       "PAL_SHARK",               "Pal Shark", "Govulmer"},
    {EnemyType::PAN_ARMS,                EP1 | EP2,                     0x15, 0x15, {0x31},       {0x31},                         {0x31},       "PAN_ARMS",                "Pan Arms", nullptr},
    {EnemyType::PAZUZU_CRATER,                       EP4 | RARE,        0x5F, 0x4B, {0x08},       {0x08},                         {0x08},       "PAZUZU_CRATER",           "Pazuzu (crater)", nullptr},
    {EnemyType::PAZUZU_DESERT,                       EP4 | RARE,        0x5F, 0x4C, {0x1C},       {0x1C},                         {0x1C},       "PAZUZU_DESERT",           "Pazuzu (desert)", nullptr},
    {EnemyType::PIG_RAY,                       EP2,                     0x4A, NONE, {0x08},       {0x08},                         {0x08},       "PIG_RAY",                 "Pig Ray", nullptr},
    {EnemyType::POFUILLY_SLIME,          EP1,                           0x13, 0x13, {0x30},       {0x30},                         {0x30},       "POFUILLY_SLIME",          "Pofuilly Slime", nullptr},
    {EnemyType::POUILLY_SLIME,           EP1             | RARE,        0x14, 0x14, {0x34},       {0x34},                         {0x34},       "POUILLY_SLIME",           "Pouilly Slime", nullptr},
    {EnemyType::POISON_LILY,             EP1 | EP2,                     0x0D, 0x0D, {0x04},       {0x04},                         {0x04},       "POISON_LILY",             "Poison Lily", "Ob Lily"},
    {EnemyType::PYRO_GORAN,                          EP4,               0x67, 0x54, {0x12},       {0x12, 0x15},                   {0x12},       "PYRO_GORAN",              "Pyro Goran", nullptr},
    {EnemyType::RAG_RAPPY,               EP1 | EP2,                     0x05, 0x05, {0x18},       {0x18},                         {0x18},       "RAG_RAPPY",               "Rag Rappy", "El Rappy"},
    {EnemyType::RECOBOX,                       EP2,                     0x43, 0x43, {0x41},       {0x41},                         {0x41},       "RECOBOX",                 "Recobox", nullptr},
    {EnemyType::RECON,                         EP2,                     0x44, 0x44, {0x42},       {0x42},                         {0x42},       "RECON",                   "Recon", nullptr},
    {EnemyType::SAINT_MILION,                        EP4        | BOSS, 0x6A, 0x59, {0x20, 0x22}, {0x20, 0x22},                   {0x20, 0x22}, "SAINT_MILION",            "Saint-Milion", nullptr},
    {EnemyType::SAINT_MILION_SPINNER,                EP4        | BOSS, 0x6A, 0x59, {0x21, 0x23}, {0x21, 0x23},                   {0x21, 0x23}, "SAINT_MILION_SPINNER",    "Saint-Milion (spinner)", nullptr},
    {EnemyType::SAINT_RAPPY,                   EP2,                     0x4F, 0x4F, {0x19},       {0x19},                         {0x19},       "SAINT_RAPPY",             "Saint Rappy", nullptr},
    {EnemyType::SAND_RAPPY_CRATER,                   EP4,               0x68, 0x55, {0x05},       {0x05},                         {0x05},       "SAND_RAPPY_CRATER",       "Sand Rappy (crater)", nullptr},
    {EnemyType::SAND_RAPPY_DESERT,                   EP4,               0x68, 0x56, {0x17},       {0x17},                         {0x17},       "SAND_RAPPY_DESERT",       "Sand Rappy (desert)", nullptr},
    {EnemyType::SATELLITE_LIZARD_CRATER,             EP4,               0x5A, 0x44, {0x0D},       {0x0D},                         {0x0D},       "SATELLITE_LIZARD_CRATER", "Satellite Lizard (crater)", nullptr},
    {EnemyType::SATELLITE_LIZARD_DESERT,             EP4,               0x5A, 0x45, {0x1D},       {0x1D},                         {0x1D},       "SATELLITE_LIZARD_DESERT", "Satellite Lizard (desert)", nullptr},
    {EnemyType::SAVAGE_WOLF,             EP1 | EP2,                     0x07, 0x07, {0x02},       {0x02},                         {0x02},       "SAVAGE_WOLF",             "Savage Wolf", "Gulgus"},
    {EnemyType::SHAMBERTIN,                          EP4        | BOSS, 0x6B, 0x5A, {0x24, 0x26}, {0x24, 0x26},                   {0x24, 0x26}, "SHAMBERTIN",              "Shambertin", nullptr},
    {EnemyType::SHAMBERTIN_SPINNER,                  EP4        | BOSS, 0x6B, 0x5A, {0x25, 0x27}, {0x25, 0x27},                   {0x25, 0x27}, "SHAMBERTIN_SPINNER",      "Shambertin (spinner)", nullptr},
    {EnemyType::SINOW_BEAT,              EP1,                           0x1A, 0x1A, {0x06},       {0x06},                         {0x06},       "SINOW_BEAT",              "Sinow Beat", "Sinow Blue"},
    {EnemyType::SINOW_BERILL,                  EP2,                     0x3E, 0x3E, {0x06},       {0x06},                         {0x06},       "SINOW_BERILL",            "Sinow Berill", nullptr},
    {EnemyType::SINOW_GOLD,              EP1,                           0x1B, 0x1B, {0x13},       {0x47},                         {0x13},       "SINOW_GOLD",              "Sinow Gold", "Sinow Red"},
    {EnemyType::SINOW_SPIGELL,                 EP2,                     0x3F, 0x3F, {0x13},       {0x47},                         {0x13},       "SINOW_SPIGELL",           "Sinow Spigell", nullptr},
    {EnemyType::SINOW_ZELE,                    EP2,                     0x46, 0x46, {0x44},       {0x44},                         {0x44},       "SINOW_ZELE",              "Sinow Zele", nullptr},
    {EnemyType::SINOW_ZOA,                     EP2,                     0x45, 0x45, {0x43},       {0x43},                         {0x43},       "SINOW_ZOA",               "Sinow Zoa", nullptr},
    {EnemyType::SO_DIMENIAN,             EP1 | EP2,                     0x2B, 0x2B, {0x55},       {0x5C},                         {0x54},       "SO_DIMENIAN",             "So Dimenian", "Del-D"},
    {EnemyType::UL_GIBBON,                     EP2,                     0x3B, 0x3B, {0x3B},       {0x3B},                         {0x3B},       "UL_GIBBON",               "Ul Gibbon", nullptr},
    {EnemyType::UL_RAY,                        EP2,                     0x4B, NONE, {0x09},       {0x09},                         {0x09},       "UL_RAY",                  "Ul Ray", nullptr},
    {EnemyType::VOL_OPT_1,               EP1                    | BOSS, NONE, NONE, {0x21},       {0x21},                         {0x21},       "VOL_OPT_1",               "Vol Opt (phase 1)", "Vol Opt ver.2 (phase 1)"},
    {EnemyType::VOL_OPT_2,               EP1                    | BOSS, 0x2E, 0x2E, {0x25},       {0x25},                         {0x25},       "VOL_OPT_2",               "Vol Opt (phase 2)", "Vol Opt ver.2 (phase 2)"},
    {EnemyType::VOL_OPT_AMP,             EP1                    | BOSS, NONE, NONE, {0x24},       {0x24},                         {0x24},       "VOL_OPT_AMP",             "Vol Opt (amp)", "Vol Opt ver.2 (amp)"},
    {EnemyType::VOL_OPT_CORE,            EP1                    | BOSS, NONE, NONE, {0x26},       {0x26},                         {0x26},       "VOL_OPT_CORE",            "Vol Opt (core)", "Vol Opt ver.2 (core)"},
    {EnemyType::VOL_OPT_MONITOR,         EP1                    | BOSS, NONE, NONE, {0x23},       {0x23},                         {0x23},       "VOL_OPT_MONITOR",         "Vol Opt (monitor)", "Vol Opt ver.2 (monitor)"},
    {EnemyType::VOL_OPT_PILLAR,          EP1                    | BOSS, NONE, NONE, {0x22},       {0x22},                         {0x22},       "VOL_OPT_PILLAR",          "Vol Opt (pillar)", "Vol Opt ver.2 (pillar)"},
    {EnemyType::YOWIE_CRATER,                        EP4,               0x59, 0x42, {0x0E},       {0x0E},                         {0x0E},       "YOWIE_CRATER",            "Yowie (crater)", nullptr},
    {EnemyType::YOWIE_DESERT,                        EP4,               0x59, 0x43, {0x1E},       {0x1E},                         {0x1E},       "YOWIE_DESERT",            "Yowie (desert)", nullptr},
    {EnemyType::ZE_BOOTA,                            EP4,               0x61, 0x4E, {0x01},       {0x01, 0x02, 0x04},             {0x01},       "ZE_BOOTA",                "Ze Boota", nullptr},
    {EnemyType::ZOL_GIBBON,                    EP2,                     0x3C, 0x3C, {0x3C},       {0x3C},                         {0x3C},       "ZOL_GIBBON",              "Zol Gibbon", nullptr},
    {EnemyType::ZU_CRATER,                           EP4,               0x5E, 0x49, {0x07},       {0x07},                         {0x07},       "ZU_CRATER",               "Zu (crater)", nullptr},
    {EnemyType::ZU_DESERT,                           EP4,               0x5E, 0x4A, {0x1B},       {0x1B},                         {0x1B},       "ZU_DESERT",               "Zu (desert)", nullptr},
    // clang-format on
};

const EnemyTypeDefinition& type_definition_for_enemy(EnemyType type) {
  return type_defs.at(static_cast<size_t>(type));
}

template <>
const char* phosg::name_for_enum<EnemyType>(EnemyType type) {
  return type_definition_for_enemy(type).enum_name;
}

template <>
EnemyType phosg::enum_for_name<EnemyType>(const char* name) {
  static unordered_map<string, EnemyType> index;
  if (index.empty()) {
    for (const auto& def : type_defs) {
      if (!index.emplace(def.enum_name, def.type).second) {
        throw logic_error(std::format("duplicate enemy enum name: {}", def.enum_name));
      }
    }
  }
  return index.at(name);
}

const vector<EnemyType>& enemy_types_for_rare_table_index(Episode episode, uint8_t rt_index) {
  static array<vector<vector<EnemyType>>, 5> data;
  auto& ret = data.at(static_cast<size_t>(episode));
  if (ret.empty()) {
    for (const auto& def : type_defs) {
      if (!def.valid_in_episode(episode)) {
        continue;
      }
      if (def.rt_index != 0xFF) {
        if (def.rt_index >= ret.size()) {
          ret.resize(def.rt_index + 1);
        }
        ret[def.rt_index].emplace_back(def.type);
      }
    }
  }
  try {
    return ret.at(rt_index);
  } catch (const out_of_range&) {
    static const vector<EnemyType> empty_vec;
    return empty_vec;
  }
}

const vector<EnemyType>& enemy_types_for_battle_param_stats_index(Episode episode, uint8_t bp_index) {
  static array<vector<vector<EnemyType>>, 5> data;
  auto& ret = data.at(static_cast<size_t>(episode));
  if (ret.empty()) {
    for (const auto& def : type_defs) {
      if (def.valid_in_episode(episode) && !def.bp_stats_indexes.empty()) {
        // Some enemies (e.g. Ep4 bosses) have multiple stats structures; we use the last one, since that's the only
        // one used when giving EXP
        size_t bp_index = def.bp_stats_indexes.back();
        if (bp_index >= ret.size()) {
          ret.resize(bp_index + 1);
        }
        ret[bp_index].emplace_back(def.type);
      }
    }
  }
  try {
    return ret.at(bp_index);
  } catch (const out_of_range&) {
    static const vector<EnemyType> empty_vec;
    return empty_vec;
  }
}

EnemyType EnemyTypeDefinition::rare_type(uint8_t area, uint8_t event) const {
  switch (this->type) {
    case EnemyType::HILDEBEAR:
      return EnemyType::HILDEBLUE;
    case EnemyType::RAG_RAPPY:
      if (area < 0x12) {
        return EnemyType::AL_RAPPY;
      } else if (area < 0x24) {
        switch (event) {
          case 0x01: // rappy_type 1
            return EnemyType::SAINT_RAPPY;
          case 0x04: // rappy_type 2
            return EnemyType::EGG_RAPPY;
          case 0x05: // rappy_type 3
            return EnemyType::HALLO_RAPPY;
          default:
            return EnemyType::LOVE_RAPPY;
        }
      } else if (area <= 0x28) {
        return EnemyType::DEL_RAPPY_CRATER;
      } else {
        return EnemyType::DEL_RAPPY_DESERT;
      }
    case EnemyType::POISON_LILY:
      return EnemyType::NAR_LILY;
    case EnemyType::POFUILLY_SLIME:
      return EnemyType::POUILLY_SLIME;
    case EnemyType::SAND_RAPPY_CRATER:
      return EnemyType::DEL_RAPPY_CRATER;
    case EnemyType::SAND_RAPPY_DESERT:
      return EnemyType::DEL_RAPPY_DESERT;
    case EnemyType::MERISSA_A:
      return EnemyType::MERISSA_AA;
    case EnemyType::ZU_CRATER:
      return EnemyType::PAZUZU_CRATER;
    case EnemyType::ZU_DESERT:
      return EnemyType::PAZUZU_DESERT;
    case EnemyType::DORPHON:
      return EnemyType::DORPHON_ECLAIR;
    case EnemyType::SAINT_MILION:
    case EnemyType::SHAMBERTIN:
      return EnemyType::KONDRIEU;
    default:
      return this->type;
  }
}
