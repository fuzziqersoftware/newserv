#include "ItemData.hh"

#include "StaticGameData.hh"

using namespace std;

static string S_RANK_NAME_CHARACTERS("\0ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_", 0x20);

ItemData::ItemData() {
  this->clear();
}

ItemData::ItemData(const ItemData& other) {
  this->data1d = other.data1d;
  this->id = other.id;
  this->data2d = other.data2d;
}

ItemData& ItemData::operator=(const ItemData& other) {
  this->data1d = other.data1d;
  this->id = other.id;
  this->data2d = other.data2d;
  return *this;
}

bool ItemData::operator==(const ItemData& other) const {
  return ((this->data1d[0] == other.data1d[0]) &&
      (this->data1d[1] == other.data1d[1]) &&
      (this->data1d[2] == other.data1d[2]) &&
      (this->id == other.id) &&
      (this->data2d == other.data2d));
}

bool ItemData::operator!=(const ItemData& other) const {
  return !this->operator==(other);
}

void ItemData::clear() {
  this->data1d.clear(0);
  this->id = 0xFFFFFFFF;
  this->data2d = 0;
}

void ItemData::bswap_data2_if_mag() {
  if (this->data1[0] == 0x02) {
    this->data2d = bswap32(this->data2d);
  }
}

bool ItemData::empty() const {
  return (this->data1d[0] == 0) &&
      (this->data1d[1] == 0) &&
      (this->data1d[2] == 0) &&
      (this->data2d == 0);
}

uint32_t ItemData::primary_identifier() const {
  // The game treats any item starting with 04 as Meseta, and ignores the rest
  // of data1 (the value is in data2)
  if (this->data1[0] == 0x04) {
    return 0x040000;
  }
  if (this->data1[0] == 0x03 && this->data1[1] == 0x02) {
    return 0x030200; // Tech disk (data1[2] is level, so omit it)
  } else if (this->data1[0] == 0x02) {
    return 0x020000 | (this->data1[1] << 8); // Mag
  } else {
    return (this->data1[0] << 16) | (this->data1[1] << 8) | this->data1[2];
  }
}

bool ItemData::is_wrapped() const {
  switch (this->data1[0]) {
    case 0:
    case 1:
      return this->data1[4] & 0x40;
    case 2:
      return this->data2[2] & 0x40;
    case 3:
      return !this->is_stackable() && (this->data1[3] & 0x40);
    case 4:
      return false;
    default:
      throw runtime_error("invalid item data");
  }
}

void ItemData::unwrap() {
  switch (this->data1[0]) {
    case 0:
    case 1:
      this->data1[4] &= 0xBF;
      break;
    case 2:
      this->data2[2] &= 0xBF;
      break;
    case 3:
      if (!this->is_stackable()) {
        this->data1[3] &= 0xBF;
      }
      break;
    case 4:
      break;
    default:
      throw runtime_error("invalid item data");
  }
}

bool ItemData::is_stackable() const {
  return this->max_stack_size() > 1;
}

size_t ItemData::stack_size() const {
  if (max_stack_size_for_item(this->data1[0], this->data1[1]) > 1) {
    return this->data1[5];
  }
  return 1;
}

size_t ItemData::max_stack_size() const {
  return max_stack_size_for_item(this->data1[0], this->data1[1]);
}

bool ItemData::is_common_consumable(uint32_t primary_identifier) {
  if (primary_identifier == 0x030200) {
    return false;
  }
  return (primary_identifier >= 0x030000) && (primary_identifier < 0x030A00);
}

bool ItemData::is_common_consumable() const {
  return this->is_common_consumable(this->primary_identifier());
}

void ItemData::assign_mag_stats(const ItemMagStats& mag) {
  // this->data1[0] and [1] unchanged
  this->data1[2] = mag.level();
  this->data1[3] = mag.photon_blasts;
  this->data1w[2] = mag.def & 0x7FFE;
  this->data1w[3] = mag.pow & 0x7FFE;
  this->data1w[4] = mag.dex & 0x7FFE;
  this->data1w[5] = mag.mind & 0x7FFE;
  // this->id unchanged
  this->data2[0] = mag.synchro;
  this->data2[1] = mag.iq;
  this->data2[2] = mag.flags;
  this->data2[3] = mag.color;
}

void ItemData::clear_mag_stats() {
  if (this->data1[0] == 2) {
    this->data1[1] = '\0';
    this->assign_mag_stats(ItemMagStats());
  }
}

uint16_t ItemData::compute_mag_level() const {
  return (this->data1w[2] / 100) +
      (this->data1w[3] / 100) +
      (this->data1w[4] / 100) +
      (this->data1w[5] / 100);
}

uint16_t ItemData::compute_mag_strength_flags() const {
  uint16_t pow = this->data1w[3] / 100;
  uint16_t dex = this->data1w[4] / 100;
  uint16_t mind = this->data1w[5] / 100;

  uint16_t ret = 0;
  if ((dex < pow) && (mind < pow)) {
    ret = 0x008;
  }
  if ((pow < dex) && (mind < dex)) {
    ret |= 0x010;
  }
  if ((dex < mind) && (pow < mind)) {
    ret |= 0x020;
  }

  uint16_t highest = max<uint16_t>(dex, max<uint16_t>(pow, mind));
  if ((pow == highest) + (dex == highest) + (mind == highest) > 1) {
    ret |= 0x100;
  }
  return ret;
}

uint8_t ItemData::mag_photon_blast_for_slot(uint8_t slot) const {
  uint8_t flags = this->data2[2];
  uint8_t pb_nums = this->data1[3];

  if (slot == 0) { // Center
    return (flags & 1) ? (pb_nums & 0x07) : 0xFF;

  } else if (slot == 1) { // Right
    return (flags & 2) ? ((pb_nums & 0x38) >> 3) : 0xFF;

  } else if (slot == 2) { // Left
    if (!(flags & 4)) {
      return 0xFF;
    }

    uint8_t used_pbs[6] = {0, 0, 0, 0, 0, 0};
    used_pbs[pb_nums & 0x07] = '\x01';
    used_pbs[(pb_nums & 0x38) >> 3] = '\x01';
    uint8_t left_pb_num = (pb_nums & 0xC0) >> 6;
    for (size_t z = 0; z < 6; z++) {
      if (!used_pbs[z]) {
        if (!left_pb_num) {
          return z;
        }
        left_pb_num--;
      }
    }
    throw logic_error("failed to find unused photon blast number");

  } else {
    throw logic_error("invalid slot index");
  }
}

bool ItemData::mag_has_photon_blast_in_any_slot(uint8_t pb_num) const {
  if (pb_num < 6) {
    for (size_t slot = 0; slot < 3; slot++) {
      if (this->mag_photon_blast_for_slot(slot) == pb_num) {
        return true;
      }
    }
  }
  return false;
}

void ItemData::add_mag_photon_blast(uint8_t pb_num) {
  if (pb_num >= 6) {
    return;
  }
  if (this->mag_has_photon_blast_in_any_slot(pb_num)) {
    return;
  }

  uint8_t& flags = this->data2[2];
  uint8_t& pb_nums = this->data1[3];

  if (!(flags & 1)) { // Center
    pb_nums |= pb_num;
    flags |= 1;
  } else if (!(flags & 2)) { // Right
    pb_nums |= (pb_num << 3);
    flags |= 2;
  } else if (!(flags & 4)) {
    uint8_t orig_pb_num = pb_num;
    if (this->mag_photon_blast_for_slot(0) < orig_pb_num) {
      pb_num--;
    }
    if (this->mag_photon_blast_for_slot(1) < orig_pb_num) {
      pb_num--;
    }
    if (pb_num >= 4) {
      throw runtime_error("left photon blast number is too high");
      pb_nums |= (pb_num << 6);
    }
    flags |= 4;
  }
}

void ItemData::set_sealed_item_kill_count(uint16_t v) {
  this->data1[10] = (v >> 8) | 0x80;
  this->data1[11] = v;
}

uint8_t ItemData::get_tool_item_amount() const {
  return this->is_stackable() ? this->data1[5] : 1;
}

void ItemData::set_tool_item_amount(uint8_t amount) {
  if (this->is_stackable()) {
    this->data1[5] = amount;
  } else if (this->data1[0] == 0x03) {
    this->data1[5] = 0x00;
  }
}

int16_t ItemData::get_armor_or_shield_defense_bonus() const {
  return this->data1w[3];
}

void ItemData::set_armor_or_shield_defense_bonus(int16_t bonus) {
  this->data1w[3] = bonus;
}

int16_t ItemData::get_common_armor_evasion_bonus() const {
  return this->data1w[4];
}

void ItemData::set_common_armor_evasion_bonus(int16_t bonus) {
  this->data1w[4] = bonus;
}

int16_t ItemData::get_unit_bonus() const {
  return this->data1w[3];
}

void ItemData::set_unit_bonus(int16_t bonus) {
  this->data1w[3] = bonus;
}

bool ItemData::has_bonuses() const {
  switch (this->data1[0]) {
    case 0:
      for (size_t z = 6; z <= 10; z += 2) {
        if (this->data1[z] != 0) {
          return true;
        }
      }
      return false;
    case 1:
      switch (this->data1[1]) {
        case 1:
          if (this->data1[5] != 0) {
            return true;
          }
          [[fallthrough]];
        case 2:
          return ((this->get_armor_or_shield_defense_bonus() > 0) ||
              (this->get_common_armor_evasion_bonus() > 0));
        case 3:
          return (this->get_unit_bonus() > 0);
        default:
          throw runtime_error("invalid item");
      }
    case 2:
      if (this->data1[1] < 0x23) {
        return ((this->data1[1] == 0x1D) || (this->data1[1] == 0x22));
      } else {
        return (this->data1[1] == 0x27);
      }
    case 3:
    case 4:
      return false;
    default:
      throw runtime_error("invalid item");
  }
}

bool ItemData::is_s_rank_weapon() const {
  if (this->data1[0] == 0) {
    if ((this->data1[1] > 0x6F) && (this->data1[1] < 0x89)) {
      return true;
    }
    if ((this->data1[1] > 0xA4) && (this->data1[1] < 0xAA)) {
      return true;
    }
  }
  return false;
}

bool ItemData::compare_for_sort(const ItemData& a, const ItemData& b) {
  for (size_t z = 0; z < 12; z++) {
    if (a.data1[z] < b.data1[z]) {
      return true;
    } else if (a.data1[z] > b.data1[z]) {
      return false;
    }
  }
  for (size_t z = 0; z < 4; z++) {
    if (a.data2[z] < b.data2[z]) {
      return true;
    } else if (a.data2[z] > b.data2[z]) {
      return false;
    }
  }
  return false;
}

const unordered_map<uint8_t, const char*> name_for_weapon_special({
    {0x00, nullptr},
    {0x01, "Draw"},
    {0x02, "Drain"},
    {0x03, "Fill"},
    {0x04, "Gush"},
    {0x05, "Heart"},
    {0x06, "Mind"},
    {0x07, "Soul"},
    {0x08, "Geist"},
    {0x09, "Master\'s"},
    {0x0A, "Lord\'s"},
    {0x0B, "King\'s"},
    {0x0C, "Charge"},
    {0x0D, "Spirit"},
    {0x0E, "Berserk"},
    {0x0F, "Ice"},
    {0x10, "Frost"},
    {0x11, "Freeze"},
    {0x12, "Blizzard"},
    {0x13, "Bind"},
    {0x14, "Hold"},
    {0x15, "Seize"},
    {0x16, "Arrest"},
    {0x17, "Heat"},
    {0x18, "Fire"},
    {0x19, "Flame"},
    {0x1A, "Burning"},
    {0x1B, "Shock"},
    {0x1C, "Thunder"},
    {0x1D, "Storm"},
    {0x1E, "Tempest"},
    {0x1F, "Dim"},
    {0x20, "Shadow"},
    {0x21, "Dark"},
    {0x22, "Hell"},
    {0x23, "Panic"},
    {0x24, "Riot"},
    {0x25, "Havoc"},
    {0x26, "Chaos"},
    {0x27, "Devil\'s"},
    {0x28, "Demon\'s"},
});

const unordered_map<uint8_t, const char*> name_for_s_rank_special({
    {0x01, "Jellen"},
    {0x02, "Zalure"},
    {0x05, "Burning"},
    {0x06, "Tempest"},
    {0x07, "Blizzard"},
    {0x08, "Arrest"},
    {0x09, "Chaos"},
    {0x0A, "Hell"},
    {0x0B, "Spirit"},
    {0x0C, "Berserk"},
    {0x0D, "Demon\'s"},
    {0x0E, "Gush"},
    {0x0F, "Geist"},
    {0x10, "King\'s"},
});

struct ItemNameInfo {
  const char* name;
  bool is_rare;

  ItemNameInfo(const char* name, bool is_rare = true)
      : name(name),
        is_rare(is_rare) {}
};

const unordered_map<uint32_t, ItemNameInfo> name_info_for_primary_identifier({
    // Weapons (00xxxx)
    {0x000100, {"Saber", false}},
    {0x000101, {"Brand", false}},
    {0x000102, {"Buster", false}},
    {0x000103, {"Pallasch", false}},
    {0x000104, {"Gladius", false}},
    {0x000105, "DB\'s SABER"},
    {0x000106, "KALADBOLG"},
    {0x000107, "DURANDAL"},
    {0x000108, "GALATINE"},
    {0x000200, {"Sword", false}},
    {0x000201, {"Gigush", false}},
    {0x000202, {"Breaker", false}},
    {0x000203, {"Claymore", false}},
    {0x000204, {"Calibur", false}},
    {0x000205, "FLOWEN\'S SWORD"},
    {0x000206, "LAST SURVIVOR"},
    {0x000207, "DRAGON SLAYER"},
    {0x000300, {"Dagger", false}},
    {0x000301, {"Knife", false}},
    {0x000302, {"Blade", false}},
    {0x000303, {"Edge", false}},
    {0x000304, {"Ripper", false}},
    {0x000305, "BLADE DANCE"},
    {0x000306, "BLOODY ART"},
    {0x000307, "CROSS SCAR"},
    {0x000308, "ZERO DIVIDE"},
    {0x000309, "TWIN KAMUI"},
    {0x000400, {"Partisan", false}},
    {0x000401, {"Halbert", false}},
    {0x000402, {"Glaive", false}},
    {0x000403, {"Berdys", false}},
    {0x000404, {"Gungnir", false}},
    {0x000405, "BRIONAC"},
    {0x000406, "VJAYA"},
    {0x000407, "GAE BOLG"},
    {0x000408, "ASTERON BELT"},
    {0x000500, {"Slicer", false}},
    {0x000501, {"Spinner", false}},
    {0x000502, {"Cutter", false}},
    {0x000503, {"Sawcer", false}},
    {0x000504, {"Diska", false}},
    {0x000505, "SLICER OF ASSASSIN"},
    {0x000506, "DISKA OF LIBERATOR"},
    {0x000507, "DISKA OF BRAVEMAN"},
    {0x000508, "IZMAELA"},
    {0x000600, {"Handgun", false}},
    {0x000601, {"Autogun", false}},
    {0x000602, {"Lockgun", false}},
    {0x000603, {"Railgun", false}},
    {0x000604, {"Raygun", false}},
    {0x000605, "VARISTA"},
    {0x000606, "CUSTOM RAY ver.00"},
    {0x000607, "BRAVACE"},
    {0x000608, "TENSION BLASTER"},
    {0x000700, {"Rifle", false}},
    {0x000701, {"Sniper", false}},
    {0x000702, {"Blaster", false}},
    {0x000703, {"Beam", false}},
    {0x000704, {"Laser", false}},
    {0x000705, "VISK-235W"},
    {0x000706, "WALS-MK2"},
    {0x000707, "JUSTY-23ST"},
    {0x000708, "RIANOV 303SNR"},
    {0x000709, "RIANOV 303SNR-1"},
    {0x00070A, "RIANOV 303SNR-2"},
    {0x00070B, "RIANOV 303SNR-3"},
    {0x00070C, "RIANOV 303SNR-4"},
    {0x00070D, "RIANOV 303SNR-5"},
    {0x000800, {"Mechgun", false}},
    {0x000801, {"Assault", false}},
    {0x000802, {"Repeater", false}},
    {0x000803, {"Gatling", false}},
    {0x000804, {"Vulcan", false}},
    {0x000805, "M&A60 VISE"},
    {0x000806, "H&S25 JUSTICE"},
    {0x000807, "L&K14 COMBAT"},
    {0x000900, {"Shot", false}},
    {0x000901, {"Spread", false}},
    {0x000902, {"Cannon", false}},
    {0x000903, {"Launcher", false}},
    {0x000904, {"Arms", false}},
    {0x000905, "CRUSH BULLET"},
    {0x000906, "METEOR SMASH"},
    {0x000907, "FINAL IMPACT"},
    {0x000A00, {"Cane", false}},
    {0x000A01, {"Stick", false}},
    {0x000A02, {"Mace", false}},
    {0x000A03, {"Club", false}},
    {0x000A04, "CLUB OF LACONIUM"},
    {0x000A05, "MACE OF ADAMAN"},
    {0x000A06, "CLUB OF ZUMIURAN"},
    {0x000A07, "LOLLIPOP"},
    {0x000B00, {"Rod", false}},
    {0x000B01, {"Pole", false}},
    {0x000B02, {"Pillar", false}},
    {0x000B03, {"Striker", false}},
    {0x000B04, "BATTLE VERGE"},
    {0x000B05, "BRAVE HAMMER"},
    {0x000B06, "ALIVE AQHU"},
    {0x000B07, "VALKYRIE"},
    {0x000C00, {"Wand", false}},
    {0x000C01, {"Staff", false}},
    {0x000C02, {"Baton", false}},
    {0x000C03, {"Scepter", false}},
    {0x000C04, "FIRE SCEPTER:AGNI"},
    {0x000C05, "ICE STAFF:DAGON"},
    {0x000C06, "STORM WAND:INDRA"},
    {0x000C07, "EARTH WAND BROWNIE"},
    {0x000D00, "PHOTON CLAW"},
    {0x000D01, "SILENCE CLAW"},
    {0x000D02, "NEI\'S CLAW (REPLICA)"},
    {0x000D03, "PHOENIX CLAW"},
    {0x000E00, "DOUBLE SABER"},
    {0x000E01, "STAG CUTLERY"},
    {0x000E02, "TWIN BRAND"},
    {0x000F00, "BRAVE KNUCKLE"},
    {0x000F01, "ANGRY FIST"},
    {0x000F02, "GOD HAND"},
    {0x000F03, "SONIC KNUCKLE"},
    {0x001000, "OROTIAGITO"},
    {0x001001, "AGITO (AUW 1975)"},
    {0x001002, "AGITO (AUW 1983)"},
    {0x001003, "AGITO (AUW 2001)"},
    {0x001004, "AGITO (AUW 1991)"},
    {0x001005, "AGITO (AUW 1977)"},
    {0x001006, "AGITO (AUW 1980)"},
    {0x001007, "RAIKIRI"},
    {0x001100, "SOUL EATER"},
    {0x001101, "SOUL BANISH"},
    {0x001200, "SPREAD NEEDLE"},
    {0x001300, "HOLY RAY"},
    {0x001400, "INFERNO BAZOOKA"},
    {0x001401, "RAMBLING MAY"},
    {0x001402, "L&K38 COMBAT"},
    {0x001500, "FLAME VISIT"},
    {0x001501, "BURNING VISIT"},
    {0x001600, "AKIKO\'S FRYING PAN"},
    {0x001700, "SORCERER\'S CANE"},
    {0x001800, "S-BEAT\'S BLADE"},
    {0x001900, "P-ARMS\'S BLADE"},
    {0x001A00, "DELSABER\'S BUSTER"},
    {0x001B00, "BRINGER\'S RIFLE"},
    {0x001C00, "EGG BLASTER"},
    {0x001D00, "PSYCHO WAND"},
    {0x001E00, "HEAVEN PUNISHER"},
    {0x001F00, "LAVIS CANNON"},
    {0x002000, "VICTOR AXE"},
    {0x002001, "LACONIUM AXE"},
    {0x002100, "CHAIN SAWD"},
    {0x002200, "CADUCEUS"},
    {0x002201, "MERCURIUS ROD"},
    {0x002300, "STING TIP"},
    {0x002400, "MAGICAL PIECE"},
    {0x002500, "TECHNICAL CROZIER"},
    {0x002600, "SUPPRESSED GUN"},
    {0x002700, "ANCIENT SABER"},
    {0x002800, "HARISEN BATTLE FAN"},
    {0x002900, "YAMIGARASU"},
    {0x002A00, "AKIKO\'S WOK"},
    {0x002B00, "TOY HAMMER"},
    {0x002C00, "ELYSION"},
    {0x002D00, "RED SABER"},
    {0x002E00, "METEOR CUDGEL"},
    {0x002F00, "MONKEY KING BAR"},
    {0x002F01, "BLACK KING BAR"},
    {0x003000, "DOUBLE CANNON"},
    {0x003001, "GIRASOLE"},
    {0x003100, "HUGE BATTLE FAN"},
    {0x003200, "TSUMIKIRI J-SWORD"},
    {0x003300, "SEALED J-SWORD"},
    {0x003400, "RED SWORD"},
    {0x003500, "CRAZY TUNE"},
    {0x003600, "TWIN CHAKRAM"},
    {0x003700, "WOK OF AKIKO\'S SHOP"},
    {0x003800, "LAVIS BLADE"},
    {0x003900, "RED DAGGER"},
    {0x003A00, "MADAM\'S PARASOL"},
    {0x003B00, "MADAM\'S UMBRELLA"},
    {0x003C00, "IMPERIAL PICK"},
    {0x003D00, "BERDYSH"},
    {0x003E00, "RED PARTISAN"},
    {0x003F00, "FLIGHT CUTTER"},
    {0x004000, "FLIGHT FAN"},
    {0x004100, "RED SLICER"},
    {0x004200, "HANDGUN:GULD"},
    {0x004201, "MASTER RAVEN"},
    {0x004300, "HANDGUN:MILLA"},
    {0x004301, "LAST SWAN"},
    {0x004400, "RED HANDGUN"},
    {0x004500, "FROZEN SHOOTER"},
    {0x004501, "SNOW QUEEN"},
    {0x004600, "ANTI ANDROID RIFLE"},
    {0x004700, "ROCKET PUNCH"},
    {0x004800, "SAMBA MARACAS"},
    {0x004900, "TWIN PSYCHOGUN"},
    {0x004A00, "DRILL LAUNCHER"},
    {0x004B00, "GULD MILLA"},
    {0x004B01, "DUAL BIRD"},
    {0x004C00, "RED MECHGUN"},
    {0x004D00, "BELRA CANNON"},
    {0x004E00, "PANZER FAUST"},
    {0x004E01, "IRON FAUST"},
    {0x004F00, "SUMMIT MOON"},
    {0x005000, "WINDMILL"},
    {0x005100, "EVIL CURST"},
    {0x005200, "FLOWER CANE"},
    {0x005300, "HILDEBEAR\'S CANE"},
    {0x005400, "HILDEBLUE\'S CANE"},
    {0x005500, "RABBIT WAND"},
    {0x005600, "PLANTAIN LEAF"},
    {0x005601, "FATSIA"},
    {0x005700, "DEMONIC FORK"},
    {0x005800, "STRIKER OF CHAO"},
    {0x005900, "BROOM"},
    {0x005A00, "PROPHETS OF MOTAV"},
    {0x005B00, "THE SIGH OF A GOD"},
    {0x005C00, "TWINKLE STAR"},
    {0x005D00, "PLANTAIN FAN"},
    {0x005E00, "TWIN BLAZE"},
    {0x005F00, "MARINA\'S BAG"},
    {0x006000, "DRAGON\'S CLAW"},
    {0x006100, "PANTHER\'S CLAW"},
    {0x006200, "S-RED\'S BLADE"},
    {0x006300, "PLANTAIN HUGE FAN"},
    {0x006400, "CHAMELEON SCYTHE"},
    {0x006500, "YASMINKOV 3000R"},
    {0x006600, "ANO RIFLE"},
    {0x006700, "BARANZ LAUNCHER"},
    {0x006800, "BRANCH OF PAKUPAKU"},
    {0x006900, "HEART OF POUMN"},
    {0x006A00, "YASMINKOV 2000H"},
    {0x006B00, "YASMINKOV 7000V"},
    {0x006C00, "YASMINKOV 9000M"},
    {0x006D00, "MASER BEAM"},
    {0x006D01, "POWER MASER"},
    {0x006E00, "GAME MAGAZINE"},
    {0x006F00, "FLOWER BOUQUET"},
    {0x007000, {"S-RANK SABER", true}},
    {0x007100, {"S-RANK SWORD", true}},
    {0x007200, {"S-RANK BLADE", true}},
    {0x007300, {"S-RANK PARTISAN", true}},
    {0x007400, {"S-RANK SLICER", true}},
    {0x007500, {"S-RANK GUN", true}},
    {0x007600, {"S-RANK RIFLE", true}},
    {0x007700, {"S-RANK MECHGUN", true}},
    {0x007800, {"S-RANK SHOT", true}},
    {0x007900, {"S-RANK CANE", true}},
    {0x007A00, {"S-RANK ROD", true}},
    {0x007B00, {"S-RANK WAND", true}},
    {0x007C00, {"S-RANK TWIN", true}},
    {0x007D00, {"S-RANK CLAW", true}},
    {0x007E00, {"S-RANK BAZOOKA", true}},
    {0x007F00, {"S-RANK NEEDLE", true}},
    {0x008000, {"S-RANK SCYTHE", true}},
    {0x008100, {"S-RANK HAMMER", true}},
    {0x008200, {"S-RANK MOON", true}},
    {0x008300, {"S-RANK PSYCHOGUN", true}},
    {0x008400, {"S-RANK PUNCH", true}},
    {0x008500, {"S-RANK WINDMILL", true}},
    {0x008600, {"S-RANK HARISEN", true}},
    {0x008700, {"S-RANK KATANA", true}},
    {0x008800, {"S-RANK J-CUTTER", true}},
    {0x008900, "MUSASHI"},
    {0x008901, "YAMATO"},
    {0x008902, "ASUKA"},
    {0x008903, "SANGE & YASHA"},
    {0x008A00, "SANGE"},
    {0x008A01, "YASHA"},
    {0x008A02, "KAMUI"},
    {0x008B00, "PHOTON LAUNCHER"},
    {0x008B01, "GUILTY LIGHT"},
    {0x008B02, "RED SCORPIO"},
    {0x008B03, "PHONON MASER"},
    {0x008C00, "TALIS"},
    {0x008C01, "MAHU"},
    {0x008C02, "HITOGATA"},
    {0x008C03, "DANCING HITOGATA"},
    {0x008C04, "KUNAI"},
    {0x008D00, "NUG-2000 BAZOOKA"},
    {0x008E00, "S-BERILL\'S HANDS #0"},
    {0x008E01, "S-BERILL\'S HANDS #1"},
    {0x008F00, "FLOWEN\'S SWORD (AUW 3060; GREENILL)"},
    {0x008F01, "FLOWEN\'S SWORD (AUW 3064; SKYLY)"},
    {0x008F02, "FLOWEN\'S SWORD (AUW 3067; BLUEFULL)"},
    {0x008F03, "FLOWEN\'S SWORD (AUW 3073; PURPLENUM)"},
    {0x008F04, "FLOWEN\'S SWORD (AUW 3077; PINKAL)"},
    {0x008F05, "FLOWEN\'S SWORD (AUW 3082; REDRIA)"},
    {0x008F06, "FLOWEN\'S SWORD (AUW 3083; ORAN)"},
    {0x008F07, "FLOWEN\'S SWORD (AUW 3084; YELLOWBOZE)"},
    {0x008F08, "FLOWEN\'S SWORD (AUW 3079; WHITILL)"},
    {0x009000, "DB\'S SWORD (AUW 3062; GREENILL)"},
    {0x009001, "DB\'S SWORD (AUW 3067; SKYLY)"},
    {0x009002, "DB\'S SWORD (AUW 3069; BLUEFULL)"},
    {0x009003, "DB\'S SWORD (AUW 3064; PURPLENUM)"},
    {0x009004, "DB\'S SWORD (AUW 3069; PINKAL)"},
    {0x009005, "DB\'S SWORD (AUW 3073; REDRIA)"},
    {0x009006, "DB\'S SWORD (AUW 3070; ORAN)"},
    {0x009007, "DB\'S SWORD (AUW 3075; YELLOWBOZE)"},
    {0x009008, "DB\'S SWORD (AUW 3077; WHITILL)"},
    {0x009100, "GI GUE BAZOOKA"},
    {0x009200, "GUARDIANNA"},
    {0x009300, "VIRIDIA CARD"},
    {0x009301, "GREENILL CARD"},
    {0x009302, "SKYLY CARD"},
    {0x009303, "BLUEFULL CARD"},
    {0x009304, "PURPLENUM CARD"},
    {0x009305, "PINKAL CARD"},
    {0x009306, "REDRIA CARD"},
    {0x009307, "ORAN CARD"},
    {0x009308, "YELLOWBOZE CARD"},
    {0x009309, "WHITILL CARD"},
    {0x009400, "MORNING GLORY"},
    {0x009500, "PARTISAN OF LIGHTING"},
    {0x009600, "GAL WIND"},
    {0x009700, "ZANBA"},
    {0x009800, "RIKA\'S CLAW"},
    {0x009900, "ANGEL HARP"},
    {0x009A00, "DEMOLITION COMET"},
    {0x009B00, "NEI\'S CLAW"},
    {0x009C00, "RAINBOW BATON"},
    {0x009D00, "DARK FLOW"},
    {0x009E00, "DARK METEOR"},
    {0x009F00, "DARK BRIDGE"},
    {0x00A000, "G-ASSASSIN\'S SABERS"},
    {0x00A100, "RAPPY\'S FAN"},
    {0x00A200, "BOOMA\'S CLAW"},
    {0x00A201, "GOBOOMA\'S CLAW"},
    {0x00A202, "GIGOBOOMA\'S CLAW"},
    {0x00A300, "RUBY BULLET"},
    {0x00A400, "AMORE ROSE"},
    {0x00A500, {"S-RANK SWORDS", true}},
    {0x00A600, {"S-RANK LAUNCHER", true}},
    {0x00A700, {"S-RANK CARD", true}},
    {0x00A800, {"S-RANK KNUCKLE", true}},
    {0x00A900, {"S-RANK AXE", true}},
    {0x00AA00, "SLICER OF FANATIC"},
    {0x00AB00, "LAME D\'ARGENT"},
    {0x00AC00, "EXCALIBUR"},
    {0x00AD03, "RAGE DE FEU"},
    {0x00AE00, "DAISY CHAIN"},
    {0x00AF00, "OPHELIE SEIZE"},
    {0x00B000, "MILLE MARTEAUX"},
    {0x00B100, "LE COGNEUR"},
    {0x00B200, "COMMANDER BLADE"},
    {0x00B300, "VIVIENNE"},
    {0x00B400, "KUSANAGI"},
    {0x00B500, "SACRED DUSTER"},
    {0x00B600, "GUREN"},
    {0x00B700, "SHOUREN"},
    {0x00B800, "JIZAI"},
    {0x00B900, "FLAMBERGE"},
    {0x00BA00, "YUNCHANG"},
    {0x00BB00, "SNAKE SPIRE"},
    {0x00BC00, "FLAPJACK FLAPPER"},
    {0x00BD00, "GETSUGASAN"},
    {0x00BE00, "MAGUWA"},
    {0x00BF00, "HEAVEN STRIKER"},
    {0x00C000, "CANNON ROUGE"},
    {0x00C100, "METEOR ROUGE"},
    {0x00C200, "SOLFERINO"},
    {0x00C300, "CLIO"},
    {0x00C400, "SIREN GLASS HAMMER"},
    {0x00C500, "GLIDE DIVINE"},
    {0x00C600, "SHICHISHITO"},
    {0x00C700, "MURASAME"},
    {0x00C800, "DAYLIGHT SCAR"},
    {0x00C900, "DECALOG"},
    {0x00CA00, "5TH ANNIV. BLADE"},
    {0x00CB00, "PRINCIPAL\'S GIFT PARASOL"},
    {0x00CC00, "AKIKO\'S CLEAVER"},
    {0x00CD00, "TANEGASHIMA"},
    {0x00CE00, "TREE CLIPPERS"},
    {0x00CF00, "NICE SHOT"},
    {0x00D200, "ANO BAZOOKA"},
    {0x00D300, "SYNTHESIZER"},
    {0x00D400, "BAMBOO SPEAR"},
    {0x00D500, "KAN\'EI TSUHO"},
    {0x00D600, "JITTE"},
    {0x00D700, "BUTTERFLY NET"},
    {0x00D800, "SYRINGE"},
    {0x00D900, "BATTLEDORE"},
    {0x00DA00, "RACKET"},
    {0x00DB00, "HAMMER"},
    {0x00DC00, "GREAT BOUQUET"},
    {0x00DD00, "TypeSA/Saber"},
    {0x00DE00, "TypeSL/Saber"},
    {0x00DE01, "TypeSL/Slicer"},
    {0x00DE02, "TypeSL/Claw"},
    {0x00DE03, "TypeSL/Katana"},
    {0x00DF00, "TypeJS/Saber"},
    {0x00DF01, "TypeJS/Slicer"},
    {0x00DF02, "TypeJS/J-Sword"},
    {0x00E000, "TypeSW/Sword"},
    {0x00E001, "TypeSW/Slicer"},
    {0x00E002, "TypeSW/J-Sword"},
    {0x00E100, "TypeRO/Sword"},
    {0x00E101, "TypeRO/Halbert"},
    {0x00E102, "TypeRO/Rod"},
    {0x00E200, "TypeBL/BLADE"},
    {0x00E300, "TypeKN/Blade"},
    {0x00E301, "TypeKN/Claw"},
    {0x00E400, "TypeHA/Halbert"},
    {0x00E401, "TypeHA/Rod"},
    {0x00E500, "TypeDS/D.Saber"},
    {0x00E501, "TypeDS/Rod"},
    {0x00E502, "TypeDS"},
    {0x00E600, "TypeCL/Claw"},
    {0x00E700, "TypeSS/SW"},
    {0x00E800, "TypeGU/Handgun"},
    {0x00E801, "TypeGU/Mechgun"},
    {0x00E900, "TypeRI/Rifle"},
    {0x00EA00, "TypeME/Mechgun"},
    {0x00EB00, "TypeSH/Shot"},
    {0x00EC00, "TypeWA/Wand"},

    // Armors (0101xx)
    {0x010100, {"Frame", false}},
    {0x010101, {"Armor", false}},
    {0x010102, {"Psy Armor", false}},
    {0x010103, {"Giga Frame", false}},
    {0x010104, {"Soul Frame", false}},
    {0x010105, {"Cross Armor", false}},
    {0x010106, {"Solid Frame", false}},
    {0x010107, {"Brave Armor", false}},
    {0x010108, {"Hyper Frame", false}},
    {0x010109, {"Grand Armor", false}},
    {0x01010A, {"Shock Frame", false}},
    {0x01010B, {"King\'s Frame", false}},
    {0x01010C, {"Dragon Frame", false}},
    {0x01010D, {"Absorb Armor", false}},
    {0x01010E, {"Protect Frame", false}},
    {0x01010F, {"General Armor", false}},
    {0x010110, {"Perfect Frame", false}},
    {0x010111, {"Valiant Frame", false}},
    {0x010112, {"Imperial Armor", false}},
    {0x010113, {"Holiness Armor", false}},
    {0x010114, {"Guardian Armor", false}},
    {0x010115, {"Divinity Armor", false}},
    {0x010116, {"Ultimate Frame", false}},
    {0x010117, {"Celestial Armor", false}},
    {0x010118, "HUNTER FIELD"},
    {0x010119, "RANGER FIELD"},
    {0x01011A, "FORCE FIELD"},
    {0x01011B, "REVIVAL GARMENT"},
    {0x01011C, "SPIRIT GARMENT"},
    {0x01011D, "STINK FRAME"},
    {0x01011E, "D-PARTS Ver1.01"},
    {0x01011F, "D-PARTS Ver2.10"},
    {0x010120, "PARASITE WEAR:De Rol"},
    {0x010121, "PARASITE WEAR:Nelgal"},
    {0x010122, "PARASITE WEAR:Vajulla"},
    {0x010123, "SENSE PLATE"},
    {0x010124, "GRAVITON PLATE"},
    {0x010125, "ATTRIBUTE PLATE"},
    {0x010126, "FLOWEN\'S FRAME"},
    {0x010127, "CUSTOM FRAME Ver.00"},
    {0x010128, "DB\'s ARMOR"},
    {0x010129, "GUARD WAVE"},
    {0x01012A, "DF FIELD"},
    {0x01012B, "LUMINOUS FIELD"},
    {0x01012C, "CHU CHU FEVER"},
    {0x01012D, "LOVE HEART"},
    {0x01012E, "FLAME GARMENT"},
    {0x01012F, "VIRUS ARMOR:Lafuteria"},
    {0x010130, "BRIGHTNESS CIRCLE"},
    {0x010131, "AURA FIELD"},
    {0x010132, "ELECTRO FRAME"},
    {0x010133, "SACRED CLOTH"},
    {0x010134, "SMOKING PLATE"},
    {0x010135, "STAR CUIRASS"},
    {0x010136, "BLACK HOUND CUIRASS"},
    {0x010137, "MORNING PRAYER"},
    {0x010138, "BLACK ODOSHI DOMARU"},
    {0x010139, "RED ODOSHI DOMARU"},
    {0x01013A, "BLACK ODOSHI RED NIMAIDOU"},
    {0x01013B, "BLUE ODOSHI VIOLET NIMAIDOU"},
    {0x01013C, "DIRTY LIFE JACKET"},
    {0x01013E, "WEDDING DRESS"},
    {0x010140, "RED COAT"},
    {0x010141, "THIRTEEN"},
    {0x010142, "MOTHER GARB"},
    {0x010143, "MOTHER GARB+"},
    {0x010144, "DRESS PLATE"},
    {0x010145, "SWEETHEART"},
    {0x010146, "IGNITION CLOAK"},
    {0x010147, "CONGEAL CLOAK"},
    {0x010148, "TEMPEST CLOAK"},
    {0x010149, "CURSED CLOAK"},
    {0x01014A, "SELECT CLOAK"},
    {0x01014B, "SPIRIT CUIRASS"},
    {0x01014C, "REVIVAL CUIRASS"},
    {0x01014D, "ALLIANCE UNIFORM"},
    {0x01014E, "OFFICER UNIFORM"},
    {0x01014F, "COMMANDER UNIFORM"},
    {0x010150, "CRIMSON COAT"},
    {0x010151, "INFANTRY GEAR"},
    {0x010152, "LIEUTENANT GEAR"},
    {0x010153, "INFANTRY MANTLE"},
    {0x010154, "LIEUTENANT MANTLE"},
    {0x010155, "UNION FIELD"},
    {0x010156, "SAMURAI ARMOR"},
    {0x010157, "STEALTH SUIT"},

    // Shields (0102xx)
    {0x010200, {"Barrier", false}},
    {0x010201, {"Shield", false}},
    {0x010202, {"Core Shield", false}},
    {0x010203, {"Giga Shield", false}},
    {0x010204, {"Soul Barrier", false}},
    {0x010205, {"Hard Shield", false}},
    {0x010206, {"Brave Barrier", false}},
    {0x010207, {"Solid Shield", false}},
    {0x010208, {"Flame Barrier", false}},
    {0x010209, {"Plasma Barrier", false}},
    {0x01020A, {"Freeze Barrier", false}},
    {0x01020B, {"Psychic Barrier", false}},
    {0x01020C, {"General Shield", false}},
    {0x01020D, {"Protect Barrier", false}},
    {0x01020E, {"Glorious Shield", false}},
    {0x01020F, {"Imperial Barrier", false}},
    {0x010210, {"Guardian Shield", false}},
    {0x010211, {"Divinity Barrier", false}},
    {0x010212, {"Ultimate Shield", false}},
    {0x010213, {"Spiritual Shield", false}},
    {0x010214, {"Celestial Shield", false}},
    {0x010215, "INVISIBLE GUARD"},
    {0x010216, "SACRED GUARD"},
    {0x010217, "S-PARTS Ver1.16"},
    {0x010218, "S-PARTS Ver2.01"},
    {0x010219, "LIGHT RELIEF"},
    {0x01021A, "SHIELD OF DELSABER"},
    {0x01021B, "FORCE WALL"},
    {0x01021C, "RANGER WALL"},
    {0x01021D, "HUNTER WALL"},
    {0x01021E, "ATTRIBUTE WALL"},
    {0x01021F, "SECRET GEAR"},
    {0x010220, "COMBAT GEAR"},
    {0x010221, "PROTO REGENE GEAR"},
    {0x010222, "REGENERATE GEAR"},
    {0x010223, "REGENE GEAR ADV."},
    {0x010224, "FLOWEN\'S SHIELD"},
    {0x010225, "CUSTOM BARRIER Ver.00"},
    {0x010226, "DB\'S SHIELD"},
    {0x010227, "RED RING"},
    {0x010228, "TRIPOLIC SHIELD"},
    {0x010229, "STANDSTILL SHIELD"},
    {0x01022A, "SAFETY HEART"},
    {0x01022B, "KASAMI BRACER"},
    {0x01022C, "GODS SHIELD SUZAKU"},
    {0x01022D, "GODS SHIELD GENBU"},
    {0x01022E, "GODS SHIELD BYAKKO"},
    {0x01022F, "GODS SHIELD SEIRYU"},
    {0x010230, "HUNTER\'S SHELL"},
    {0x010231, "RICO\'S GLASSES"},
    {0x010232, "RICO\'S EARRING"},
    {0x010235, {"SECURE FEET", false}},
    {0x01023A, {"RESTA MERGE", false}},
    {0x01023B, {"ANTI MERGE", false}},
    {0x01023C, {"SHIFTA MERGE", false}},
    {0x01023D, {"DEBAND MERGE", false}},
    {0x01023E, {"FOIE MERGE", false}},
    {0x01023F, {"GIFOIE MERGE", false}},
    {0x010240, {"RAFOIE MERGE", false}},
    {0x010241, {"RED MERGE", false}},
    {0x010242, {"BARTA MERGE", false}},
    {0x010243, {"GIBARTA MERGE", false}},
    {0x010244, {"RABARTA MERGE", false}},
    {0x010245, {"BLUE MERGE", false}},
    {0x010246, {"ZONDE MERGE", false}},
    {0x010247, {"GIZONDE MERGE", false}},
    {0x010248, {"RAZONDE MERGE", false}},
    {0x010249, {"YELLOW MERGE", false}},
    {0x01024A, {"RECOVERY BARRIER", false}},
    {0x01024B, {"ASSIST BARRIER", false}},
    {0x01024C, {"RED BARRIER", false}},
    {0x01024D, {"BLUE BARRIER", false}},
    {0x01024E, {"YELLOW BARRIER", false}},
    {0x01024F, "WEAPONS GOLD SHIELD"},
    {0x010250, "BLACK GEAR"},
    {0x010251, "WORKS GUARD"},
    {0x010252, "RAGOL RING"},
    {0x010253, "BLUE RING (7 Colors)"},
    {0x010259, "BLUE RING"},
    {0x01025F, "GREEN RING"},
    {0x010266, "YELLOW RING"},
    {0x01026C, "PURPLE RING"},
    {0x010275, "WHITE RING"},
    {0x010280, "BLACK RING"},
    {0x010283, "WEAPONS SILVER SHIELD"},
    {0x010284, "WEAPONS COPPER SHIELD"},
    {0x010285, "GRATIA"},
    {0x010286, "TRIPOLIC REFLECTOR"},
    {0x010287, "STRIKER PLUS"},
    {0x010288, "REGENERATE GEAR B.P."},
    {0x010289, "RUPIKA"},
    {0x01028A, "YATA MIRROR"},
    {0x01028B, "BUNNY EARS"},
    {0x01028C, "CAT EARS"},
    {0x01028D, "THREE SEALS"},
    {0x01028F, "DF SHIELD"},
    {0x010290, "FROM THE DEPTHS"},
    {0x010291, "DE ROL LE SHIELD"},
    {0x010292, "HONEYCOMB REFLECTOR"},
    {0x010293, "EPSIGUARD"},
    {0x010294, "ANGEL RING"},
    {0x010295, "UNION GUARD"},
    {0x010297, "UNION"},
    {0x010298, "BLACK SHIELD UNION GUARD"},
    {0x010299, "STINK SHIELD"},
    {0x01029A, "BLACK"},
    {0x01029B, "GENPEI Heightened"},
    {0x01029C, "GENPEI Greenill"},
    {0x01029D, "GENPEI Skyly"},
    {0x01029E, "GENPEI Bluefull"},
    {0x01029F, "GENPEI Purplenum"},
    {0x0102A0, "GENPEI Pinkal"},
    {0x0102A1, "GENPEI Redria"},
    {0x0102A2, "GENPEI Oran"},
    {0x0102A3, "GENPEI Yellowboze"},
    {0x0102A4, "GENPEI Whitill"},

    // Units (0103xx)
    {0x010300, {"Knight/Power", false}},
    {0x010301, {"General/Power", false}},
    {0x010302, {"Ogre/Power", false}},
    {0x010303, "God/Power"},
    {0x010304, {"Priest/Mind", false}},
    {0x010305, {"General/Mind", false}},
    {0x010306, {"Angel/Mind", false}},
    {0x010307, "God/Mind"},
    {0x010308, {"Marksman/Arm", false}},
    {0x010309, {"General/Arm", false}},
    {0x01030A, {"Elf/Arm", false}},
    {0x01030B, "God/Arm"},
    {0x01030C, {"Thief/Legs", false}},
    {0x01030D, {"General/Legs", false}},
    {0x01030E, {"Elf/Legs", false}},
    {0x01030F, "God/Legs"},
    {0x010310, {"Digger/HP", false}},
    {0x010311, {"General/HP", false}},
    {0x010312, {"Dragon/HP", false}},
    {0x010313, "God/HP"},
    {0x010314, {"Magician/TP", false}},
    {0x010315, {"General/TP", false}},
    {0x010316, {"Angel/TP", false}},
    {0x010317, "God/TP"},
    {0x010318, {"Warrior/Body", false}},
    {0x010319, {"General/Body", false}},
    {0x01031A, {"Metal/Body", false}},
    {0x01031B, "God/Body"},
    {0x01031C, {"Angel/Luck", false}},
    {0x01031D, "God/Luck"},
    {0x01031E, {"Master/Ability", false}},
    {0x01031F, {"Hero/Ability", false}},
    {0x010320, "God/Ability"},
    {0x010321, {"Resist/Fire", false}},
    {0x010322, {"Resist/Flame", false}},
    {0x010323, {"Resist/Burning", false}},
    {0x010324, {"Resist/Cold", false}},
    {0x010325, {"Resist/Freeze", false}},
    {0x010326, {"Resist/Blizzard", false}},
    {0x010327, {"Resist/Shock", false}},
    {0x010328, {"Resist/Thunder", false}},
    {0x010329, {"Resist/Storm", false}},
    {0x01032A, {"Resist/Light", false}},
    {0x01032B, {"Resist/Saint", false}},
    {0x01032C, {"Resist/Holy", false}},
    {0x01032D, {"Resist/Dark", false}},
    {0x01032E, {"Resist/Evil", false}},
    {0x01032F, {"Resist/Devil", false}},
    {0x010330, {"All/Resist", false}},
    {0x010331, {"Super/Resist", false}},
    {0x010332, "Perfect/Resist"},
    {0x010333, {"HP/Restorate", false}},
    {0x010334, {"HP/Generate", false}},
    {0x010335, {"HP/Revival", false}},
    {0x010336, {"TP/Restorate", false}},
    {0x010337, {"TP/Generate", false}},
    {0x010338, {"TP/Revival", false}},
    {0x010339, {"PB/Amplifier", false}},
    {0x01033A, {"PB/Generate", false}},
    {0x01033B, {"PB/Create", false}},
    {0x01033C, {"Wizard/Technique", false}},
    {0x01033D, {"Devil/Technique", false}},
    {0x01033E, "God/Technique"},
    {0x01033F, {"General/Battle", false}},
    {0x010340, {"Devil/Battle", false}},
    {0x010341, "God/Battle"},
    {0x010342, "Cure/Poison"},
    {0x010343, "Cure/Paralysis"},
    {0x010344, "Cure/Slow"},
    {0x010345, "Cure/Confuse"},
    {0x010346, "Cure/Freeze"},
    {0x010347, "Cure/Shock"},
    {0x010348, "Yasakani Magatama"},
    {0x010349, "V101"},
    {0x01034A, "V501"},
    {0x01034B, "V502"},
    {0x01034C, "V801"},
    {0x01034D, "LIMITER"},
    {0x01034E, "ADEPT"},
    {0x01034F, "SWORDSMAN LORE"},
    {0x010350, "PROOF OF SWORD-SAINT"},
    {0x010351, "SMARTLINK"},
    {0x010352, "DIVINE PROTECTION"},
    {0x010353, "Heavenly/Battle"},
    {0x010354, "Heavenly/Power"},
    {0x010355, "Heavenly/Mind"},
    {0x010356, "Heavenly/Arms"},
    {0x010357, "Heavenly/Legs"},
    {0x010358, "Heavenly/Body"},
    {0x010359, "Heavenly/Luck"},
    {0x01035A, "Heavenly/Ability"},
    {0x01035B, "Centurion/Ability"},
    {0x01035C, "Friend Ring"},
    {0x01035D, "Heavenly/HP"},
    {0x01035E, "Heavenly/TP"},
    {0x01035F, "Heavenly/Resist"},
    {0x010360, "Heavenly/Technique"},
    {0x010361, "HP/Resurrection"},
    {0x010362, "TP/Resurrection"},
    {0x010363, "PB/Increase"},

    // Mags (02xxxx)
    {0x020000, {"Mag", false}},
    {0x020100, {"Varuna", false}},
    {0x020200, {"Mitra", false}},
    {0x020300, {"Surya", false}},
    {0x020400, {"Vayu", false}},
    {0x020500, {"Varaha", false}},
    {0x020600, {"Kama", false}},
    {0x020700, {"Ushasu", false}},
    {0x020800, {"Apsaras", false}},
    {0x020900, {"Kumara", false}},
    {0x020A00, {"Kaitabha", false}},
    {0x020B00, {"Tapas", false}},
    {0x020C00, {"Bhirava", false}},
    {0x020D00, {"Kalki", false}},
    {0x020E00, {"Rudra", false}},
    {0x020F00, {"Marutah", false}},
    {0x021000, {"Yaksa", false}},
    {0x021100, {"Sita", false}},
    {0x021200, {"Garuda", false}},
    {0x021300, {"Nandin", false}},
    {0x021400, {"Ashvinau", false}},
    {0x021500, {"Ribhava", false}},
    {0x021600, {"Soma", false}},
    {0x021700, {"Ila", false}},
    {0x021800, {"Durga", false}},
    {0x021900, {"Vritra", false}},
    {0x021A00, {"Namuci", false}},
    {0x021B00, {"Sumba", false}},
    {0x021C00, {"Naga", false}},
    {0x021D00, {"Pitri", false}},
    {0x021E00, {"Kabanda", false}},
    {0x021F00, {"Ravana", false}},
    {0x022000, {"Marica", false}},
    {0x022100, {"Soniti", false}},
    {0x022200, {"Preta", false}},
    {0x022300, {"Andhaka", false}},
    {0x022400, {"Bana", false}},
    {0x022500, {"Naraka", false}},
    {0x022600, {"Madhu", false}},
    {0x022700, {"Churel", false}},
    {0x022800, "ROBOCHAO"},
    {0x022900, "OPA-OPA"},
    {0x022A00, "PIAN"},
    {0x022B00, "CHAO"},
    {0x022C00, "CHU CHU"},
    {0x022D00, "KAPU KAPU"},
    {0x022E00, "ANGEL\'S WING"},
    {0x022F00, "DEVIL\'S WING"},
    {0x023000, "ELENOR"},
    {0x023100, "MARK3"},
    {0x023200, "MASTER SYSTEM"},
    {0x023300, "GENESIS"},
    {0x023400, "SEGA SATURN"},
    {0x023500, "DREAMCAST"},
    {0x023600, "HAMBURGER"},
    {0x023700, "PANZER\'S TAIL"},
    {0x023800, "DAVIL\'S TAIL"},
    {0x023900, "Deva"},
    {0x023A00, "Rati"},
    {0x023B00, "Savitri"},
    {0x023C00, "Rukmin"},
    {0x023D00, "Pushan"},
    {0x023E00, "Diwari"},
    {0x023F00, "Sato"},
    {0x024000, "Bhima"},
    {0x024100, "Nidra"},

    // Tools (03xxxx)
    {0x030000, {"Monomate", false}},
    {0x030001, {"Dimate", false}},
    {0x030002, {"Trimate", false}},
    {0x030100, {"Monofluid", false}},
    {0x030101, {"Difluid", false}},
    {0x030102, {"Trifluid", false}},
    {0x030200, {"<TECH-DISK>", false}}, // Special-cased in name_for_item
    {0x030300, {"Sol Atomizer", false}},
    {0x030400, {"Moon Atomizer", false}},
    {0x030500, {"Star Atomizer", false}},
    {0x030600, {"Antidote", false}},
    {0x030601, {"Antiparalysis", false}},
    {0x030700, {"Telepipe", false}},
    {0x030800, {"Trap Vision", false}},
    {0x030900, {"Scape Doll", false}},
    {0x030A00, {"Monogrinder", false}},
    {0x030A01, {"Digrinder", false}},
    {0x030A02, {"Trigrinder", false}},
    {0x030B00, {"Power Material", false}},
    {0x030B01, {"Mind Material", false}},
    {0x030B02, {"Evade Material", false}},
    {0x030B03, {"HP Material", false}},
    {0x030B04, {"TP Material", false}},
    {0x030B05, {"Def Material", false}},
    {0x030B06, {"Luck Material", false}},
    {0x030C00, "Cell of MAG 502"},
    {0x030C01, "Cell of MAG 213"},
    {0x030C02, "Parts of RoboChao"},
    {0x030C03, "Heart of Opa Opa"},
    {0x030C04, "Heart of Pian"},
    {0x030C05, "Heart of Chao"},
    {0x030D00, "Sorcerer\'s Right Arm"},
    {0x030D01, "S-beat\'s Arms"},
    {0x030D02, "P-arm\'s Arms"},
    {0x030D03, "Delsaber\'s Right Arm"},
    {0x030D04, "C-bringer\'s Right Arm"},
    {0x030D05, "Delsaber\'s Left Arm"},
    {0x030D06, "S-red\'s Arms"},
    {0x030D07, "Dragon\'s Claw"},
    {0x030D08, "Hildebear\'s Head"},
    {0x030D09, "Hildeblue\'s Head"},
    {0x030D0A, "Parts of Baranz"},
    {0x030D0B, "Belra\'s Right Arm"},
    {0x030D0C, "Gi Gue\'s Body"},
    {0x030D0D, "Sinow Berill\'s Arms"},
    {0x030D0E, "G-Assassin\'s Arms"},
    {0x030D0F, "Booma\'s Right Arm"},
    {0x030D10, "Gobooma\'s Right Arm"},
    {0x030D11, "Gigobooma\'s Right Arm"},
    {0x030D12, "Gal Gryphon's Wing"},
    {0x030D13, "Rappy\'s Wing"},
    {0x030D14, "Cladding of Epsilon"},
    {0x030D15, "De Rol Le Shell"},
    {0x030E00, "Berill Photon"},
    {0x030E01, "Parasitic gene \"Flow\""},
    {0x030E02, "Magic stone \"Iritista\""},
    {0x030E03, "Blue-black stone"},
    {0x030E04, "Syncesta"},
    {0x030E05, "Magic Water"},
    {0x030E06, "Parasitic cell Type-D"},
    {0x030E07, "magic rock \"Heart Key\""},
    {0x030E08, "magic rock \"Moola\""},
    {0x030E09, "Star Amplifier"},
    {0x030E0A, "Book of HITOGATA"},
    {0x030E0B, "Heart of Chu Chu"},
    {0x030E0C, "Parts of EGG BLASTER"},
    {0x030E0D, "Heart of Angel"},
    {0x030E0E, "Heart of Devil"},
    {0x030E0F, "Kit of Hamburger"},
    {0x030E10, "Panther\'s Spirit"},
    {0x030E11, "Kit of MARK3"},
    {0x030E12, "Kit of MASTER SYSTEM"},
    {0x030E13, "Kit of GENESIS"},
    {0x030E14, "Kit of SEGA SATURN"},
    {0x030E15, "Kit of DREAMCAST"},
    {0x030E16, {"Amplifier of Resta", false}},
    {0x030E17, {"Amplifier of Anti", false}},
    {0x030E18, {"Amplifier of Shifta", false}},
    {0x030E19, {"Amplifier of Deband", false}},
    {0x030E1A, {"Amplifier of Foie", false}},
    {0x030E1B, {"Amplifier of Gifoie", false}},
    {0x030E1C, {"Amplifier of Rafoie", false}},
    {0x030E1D, {"Amplifier of Barta", false}},
    {0x030E1E, {"Amplifier of Gibarta", false}},
    {0x030E1F, {"Amplifier of Rabarta", false}},
    {0x030E20, {"Amplifier of Zonde", false}},
    {0x030E21, {"Amplifier of Gizonde", false}},
    {0x030E22, {"Amplifier of Razonde", false}},
    {0x030E23, {"Amplifier of Red", false}},
    {0x030E24, {"Amplifier of Blue", false}},
    {0x030E25, {"Amplifier of Yellow", false}},
    {0x030E26, "Heart of KAPU KAPU"},
    {0x030E27, "Photon Booster"},
    {0x030F00, "AddSlot"},
    {0x031000, "Photon Drop"},
    {0x031001, "Photon Sphere"},
    {0x031002, "Photon Crystal"},
    {0x031003, "Secret Lottery Ticket"},
    {0x031100, "Book of KATANA1"},
    {0x031101, "Book of KATANA2"},
    {0x031102, "Book of KATANA3"},
    {0x031200, "Weapons Bronze Badge"},
    {0x031201, "Weapons Silver Badge"},
    {0x031202, "Weapons Gold Badge"},
    {0x031203, "Weapons Crystal Badge"},
    {0x031204, "Weapons Steel Badge"},
    {0x031205, "Weapons Aluminum Badge"},
    {0x031206, "Weapons Leather Badge"},
    {0x031207, "Weapons Bone Badge"},
    {0x031208, "Letter of appreciation"},
    {0x031209, "Autograph Album"},
    {0x03120A, "Valentine\'s Chocolate"},
    {0x03120B, "New Year\'s Card"},
    {0x03120C, "Christmas Card"},
    {0x03120D, "Birthday Card"},
    {0x03120E, "Proof of Sonic Team"},
    {0x03120F, "Special Event Ticket"},
    {0x031210, "Flower Bouquet"},
    {0x031211, "Cake"},
    {0x031212, "Accessories"},
    {0x031213, "Mr.Naka\'s Business Card"},
    {0x031300, "Present"},
    {0x031400, "Chocolate"},
    {0x031401, "Candy"},
    {0x031402, "Cake"},
    {0x031403, "Silver Badge"},
    {0x031404, "Gold Badge"},
    {0x031405, "Crystal Badge"},
    {0x031406, "Iron Badge"},
    {0x031407, "Aluminum Badge"},
    {0x031408, "Leather Badge"},
    {0x031409, "Bone Badge"},
    {0x03140A, "Bouquet"},
    {0x03140B, "Decoction"},
    {0x031500, "Christmas Present"},
    {0x031501, "Easter Egg"},
    {0x031502, "Jack-O\'-Lantern"},
    {0x031600, "DISK Vol.1"},
    {0x031601, "DISK Vol.2"},
    {0x031602, "DISK Vol.3"},
    {0x031603, "DISK Vol.4"},
    {0x031604, "DISK Vol.5"},
    {0x031605, "DISK Vol.6"},
    {0x031606, "DISK Vol.7"},
    {0x031607, "DISK Vol.8"},
    {0x031608, "DISK Vol.9"},
    {0x031609, "DISK Vol.10"},
    {0x03160A, "DISK Vol.11"},
    {0x03160B, "DISK Vol.12"},
    {0x031700, "Hunters Report"},
    {0x031701, "Hunters Report (Rank A)"},
    {0x031702, "Hunters Report (Rank B)"},
    {0x031703, "Hunters Report (Rank C)"},
    {0x031704, "Hunters Report (Rank F)"},
    {0x031800, "Tablet"},
    {0x031802, "Dragon Scale"},
    {0x031803, "Heaven Striker Coat"},
    {0x031804, "Pioneer Parts"},
    {0x031805, "Amitie\'s Memo"},
    {0x031806, "Heart of Morolian"},
    {0x031807, "Rappy\'s Beak"},
    {0x031809, "D-Photon Core"},
    {0x03180A, "Liberta Kit"},
    {0x03180B, "Cell of MAG 0503"},
    {0x03180C, "Cell of MAG 0504"},
    {0x03180D, "Cell of MAG 0505"},
    {0x03180E, "Cell of MAG 0506"},
    {0x03180F, "Cell of MAG 0507"},
    {0x031900, "Team Points 500"},
    {0x031901, "Team Points 1000"},
    {0x031902, "Team Points 5000"},
    {0x031903, "Team Points 10000"},
});

ItemData::ItemData(const string& orig_description, bool skip_special) {
  this->data1d.clear(0);
  this->id = 0xFFFFFFFF;
  this->data2d = 0;

  string desc = tolower(orig_description);
  if (ends_with(desc, " meseta")) {
    this->data1[0] = 0x04;
    this->data2d = stol(desc, nullptr, 10);
    return;
  }

  if (starts_with(desc, "disk:")) {
    auto tokens = split(desc, ' ');
    if (tokens.size() != 2) {
      throw runtime_error("invalid tech disk name");
    }
    if (!starts_with(tokens[0], "disk:") || !starts_with(tokens[1], "lv.")) {
      throw runtime_error("invalid tech disk format");
    }
    uint8_t tech = technique_for_name(tokens[0].substr(5));
    uint8_t level = stoul(tokens[1].substr(3), nullptr, 10);
    this->data1[0] = 0x03;
    this->data1[1] = 0x02;
    this->data1[2] = level;
    this->data1[4] = tech;
    return;
  }

  bool is_wrapped = starts_with(desc, "wrapped ");
  if (is_wrapped) {
    desc = desc.substr(8);
  }

  uint8_t weapon_special = 0;
  if (!skip_special) {
    for (const auto& it : name_for_weapon_special) {
      if (!it.second) {
        continue;
      }
      string prefix = tolower(it.second);
      prefix += ' ';
      if (starts_with(desc, prefix)) {
        weapon_special = it.first;
        desc = desc.substr(prefix.size());
        break;
      }
    }
  }

  static map<string, uint32_t> primary_identifier_for_name;
  if (primary_identifier_for_name.empty()) {
    for (const auto& it : name_info_for_primary_identifier) {
      primary_identifier_for_name.emplace(tolower(it.second.name), it.first);
    }
  }
  auto name_it = primary_identifier_for_name.lower_bound(desc);
  // Look up to 3 places before the lower bound. We have to do this to catch
  // cases like Sange vs. Sange & Yasha - if the input is like "Sange 0/...",
  // then we'll see Sange & Yasha first, which we should skip.
  size_t lookback = 0;
  while (lookback < 4) {
    if (name_it != primary_identifier_for_name.end() &&
        desc.starts_with(name_it->first)) {
      break;
    } else if (name_it == primary_identifier_for_name.begin()) {
      throw runtime_error("no such item");
    } else {
      name_it--;
      lookback++;
    }
  }
  if (lookback >= 4) {
    throw runtime_error("item not found");
  }

  desc = desc.substr(name_it->first.size());
  if (starts_with(desc, " ")) {
    desc = desc.substr(1);
  }

  uint32_t primary_identifier = name_it->second;
  this->data1[0] = (primary_identifier >> 16) & 0xFF;
  this->data1[1] = (primary_identifier >> 8) & 0xFF;
  this->data1[2] = primary_identifier & 0xFF;

  if (this->data1[0] == 0x00) {
    // Weapons: add special, grind and percentages (or name, if S-rank)
    this->data1[4] = weapon_special | (is_wrapped ? 0x40 : 0x00);

    auto tokens = split(desc, ' ');
    for (auto& token : tokens) {
      if (token.empty()) {
        continue;
      }
      if (starts_with(token, "+")) {
        token = token.substr(1);
        this->data1[3] = stoul(token, nullptr, 10);

      } else if (this->is_s_rank_weapon()) {
        if (token.size() > 8) {
          throw runtime_error("s-rank name too long");
        }

        uint8_t char_indexes[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        for (size_t z = 0; z < token.size(); z++) {
          char ch = toupper(token[z]);
          size_t pos = S_RANK_NAME_CHARACTERS.find(ch);
          if (pos == string::npos) {
            throw runtime_error(string_printf("s-rank name contains invalid character %02hhX (%c)", ch, ch));
          }
          char_indexes[z] = pos;
        }

        this->data1w[3] = (char_indexes[1] & 0x1F) | ((char_indexes[0] & 0x1F) << 5);
        this->data1w[4] = (char_indexes[4] & 0x1F) | ((char_indexes[3] & 0x1F) << 5) | ((char_indexes[2] & 0x1F) << 10);
        this->data1w[5] = (char_indexes[7] & 0x1F) | ((char_indexes[6] & 0x1F) << 5) | ((char_indexes[5] & 0x1F) << 10);

      } else {
        auto p_tokens = split(token, '/');
        if (p_tokens.size() > 5) {
          throw runtime_error("invalid bonuses token");
        }
        uint8_t bonus_index = 0;
        for (size_t z = 0; z < p_tokens.size(); z++) {
          int8_t bonus_value = stol(p_tokens[z], nullptr, 10);
          if (bonus_value == 0) {
            continue;
          }
          if (bonus_index >= 3) {
            throw runtime_error("weapon has too many bonuses");
          }
          this->data1[6 + (2 * bonus_index)] = z + 1;
          this->data1[7 + (2 * bonus_index)] = static_cast<uint8_t>(bonus_value);
          bonus_index++;
        }
      }
    }

  } else if (this->data1[0] == 0x01) {
    if (this->data1[1] == 0x03) { // Unit
      static const unordered_map<string, uint16_t> modifiers({
          {"--", 0xFFFC},
          {"-", 0xFFFE},
          {"", 0x0000},
          {"+", 0x0002},
          {"++", 0x0004},
      });
      uint16_t modifier = modifiers.at(desc);
      this->data1[7] = modifier & 0xFF;
      this->data1[8] = (modifier >> 8) & 0xFF;

    } else { // Armor/shield
      for (const auto& token : split(desc, ' ')) {
        if (token.empty()) {
          continue;
        } else if (!starts_with(token, "+")) {
          throw runtime_error("invalid armor/shield modifier");
        }
        if (ends_with(token, "def")) {
          this->data1w[3] = static_cast<uint16_t>(stol(token.substr(1, token.size() - 4), nullptr, 10));
        } else if (ends_with(token, "evp")) {
          this->data1w[4] = static_cast<uint16_t>(stol(token.substr(1, token.size() - 4), nullptr, 10));
        } else {
          this->data1[5] = stoul(token.substr(1), nullptr, 10);
        }
      }
    }

    if (is_wrapped) {
      this->data1[4] |= 0x40;
    }

  } else if (this->data1[0] == 0x02) {
    for (const auto& token : split(desc, ' ')) {
      if (token.empty()) {
        continue;
      } else if (starts_with(token, "pb:")) { // Photon blasts
        auto pb_tokens = split(token.substr(3), ',');
        if (pb_tokens.size() > 3) {
          throw runtime_error("too many photon blasts specified");
        }
        static const unordered_map<string, uint8_t> name_to_pb_num({
            {"f", 0},
            {"e", 1},
            {"g", 2},
            {"p", 3},
            {"l", 4},
            {"m&y", 5},
        });
        for (const auto& pb_token : pb_tokens) {
          this->add_mag_photon_blast(name_to_pb_num.at(pb_token));
        }
      } else if (ends_with(token, "%")) { // Synchro
        this->data2[0] = stoul(token.substr(0, token.size() - 1), nullptr, 10);
      } else if (ends_with(token, "iq")) { // IQ
        this->data2[1] = stoul(token.substr(0, token.size() - 2), nullptr, 10);
      } else if (!token.empty() && isdigit(token[0])) { // Stats
        auto s_tokens = split(token, '/');
        if (s_tokens.size() != 4) {
          throw runtime_error("incorrect stat count");
        }
        this->data1w[2] = stoul(s_tokens[0], nullptr, 10);
        this->data1w[3] = stoul(s_tokens[1], nullptr, 10);
        this->data1w[4] = stoul(s_tokens[2], nullptr, 10);
        this->data1w[5] = stoul(s_tokens[3], nullptr, 10);
      } else { // Color
        this->data2[3] = mag_color_for_name.at(token);
      }
    }

    if (is_wrapped) {
      this->data2[2] |= 0x40;
    }

  } else if (this->data1[0] == 0x03) {
    if (this->max_stack_size() > 1) {
      if (starts_with(desc, "x")) {
        this->data1[5] = stoul(desc.substr(1), nullptr, 10);
      } else {
        this->data1[5] = 1;
      }
    } else if (!desc.empty()) {
      throw runtime_error("item cannot be stacked");
    }

    if (is_wrapped) {
      this->data1[3] |= 0x40;
    }
  } else {
    throw logic_error("invalid item class");
  }
}

string ItemData::hex() const {
  return string_printf("%02hhX%02hhX%02hhX%02hhX %02hhX%02hhX%02hhX%02hhX %02hhX%02hhX%02hhX%02hhX (%08" PRIX32 ") %02hhX%02hhX%02hhX%02hhX",
      this->data1[0], this->data1[1], this->data1[2], this->data1[3],
      this->data1[4], this->data1[5], this->data1[6], this->data1[7],
      this->data1[8], this->data1[9], this->data1[10], this->data1[11],
      this->id.load(),
      this->data2[0], this->data2[1], this->data2[2], this->data2[3]);
}

string ItemData::name(bool include_color_codes) const {
  if (this->data1[0] == 0x04) {
    return string_printf("%s%" PRIu32 " Meseta",
        include_color_codes ? "$C7" : "", this->data2d.load());
  }

  vector<string> ret_tokens;

  // For weapons, specials appear before the weapon name
  if ((this->data1[0] == 0x00) && (this->data1[4] != 0x00)) {
    // 0x80 is the unidentified flag, but we always return the identified name
    // of the item here, so we ignore it
    bool is_present = this->data1[4] & 0x40;
    uint8_t special_id = this->data1[4] & 0x3F;
    if (is_present) {
      ret_tokens.emplace_back("Wrapped");
    }
    if (special_id) {
      try {
        ret_tokens.emplace_back(name_for_weapon_special.at(special_id));
      } catch (const out_of_range&) {
        ret_tokens.emplace_back(string_printf("!SP:%02hhX", special_id));
      }
    }
  }
  // Mags can be wrapped as well
  if ((this->data1[0] == 0x02) && (this->data2[2] & 0x40)) {
    ret_tokens.emplace_back("Wrapped");
  }
  // And tools can be wrapped if they aren't stackable
  if ((this->data1[0] == 0x03) && !this->is_stackable() && (this->data1[3] & 0x40)) {
    ret_tokens.emplace_back("Wrapped");
  }

  // Add the item name. Technique disks are special because the level is part of
  // the primary identifier, so we manually generate the name instead of looking
  // it up.
  bool is_rare = false;
  uint32_t primary_identifier = this->primary_identifier();
  if ((primary_identifier & 0xFFFFFF00) == 0x00030200) {
    string technique_name;
    try {
      technique_name = tech_id_to_name.at(this->data1[4]);
      technique_name[0] = toupper(technique_name[0]);
    } catch (const out_of_range&) {
      technique_name = string_printf("!TECH:%02hhX", this->data1[4]);
    }
    ret_tokens.emplace_back(string_printf(
        "Disk:%s Lv.%d", technique_name.c_str(), this->data1[2] + 1));
  } else {
    try {
      const auto& name_info = name_info_for_primary_identifier.at(primary_identifier);
      ret_tokens.emplace_back(name_info.name);
      is_rare = name_info.is_rare;
    } catch (const out_of_range&) {
      ret_tokens.emplace_back(string_printf("!ID:%06" PRIX32, primary_identifier));
    }
  }

  // For weapons, add the grind and percentages, or S-rank name if applicable
  if (this->data1[0] == 0x00) {
    if (this->data1[3] > 0) {
      ret_tokens.emplace_back(string_printf("+%hhu", this->data1[3]));
    }

    if (this->is_s_rank_weapon()) {
      // S-rank (has name instead of percent bonuses)
      uint8_t char_indexes[8] = {
          static_cast<uint8_t>((this->data1w[3] >> 5) & 0x1F),
          static_cast<uint8_t>(this->data1w[3] & 0x1F),
          static_cast<uint8_t>((this->data1w[4] >> 10) & 0x1F),
          static_cast<uint8_t>((this->data1w[4] >> 5) & 0x1F),
          static_cast<uint8_t>(this->data1w[4] & 0x1F),
          static_cast<uint8_t>((this->data1w[5] >> 10) & 0x1F),
          static_cast<uint8_t>((this->data1w[5] >> 5) & 0x1F),
          static_cast<uint8_t>(this->data1w[5] & 0x1F),
      };

      string name;
      for (size_t x = 0; x < 8; x++) {
        char ch = S_RANK_NAME_CHARACTERS[char_indexes[x]];
        if (ch == 0) {
          break;
        }
        name += ch;
      }
      if (!name.empty()) {
        ret_tokens.emplace_back("(" + name + ")");
      }

    } else { // Not S-rank (extended name bits not set)
      uint8_t percentages[5] = {0, 0, 0, 0, 0};
      for (size_t x = 0; x < 3; x++) {
        uint8_t which = this->data1[6 + 2 * x];
        uint8_t value = this->data1[7 + 2 * x];
        if (which == 0) {
          continue;
        }
        if (which > 5) {
          ret_tokens.emplace_back(string_printf("!PC:%02hhX%02hhX", which, value));
        } else {
          percentages[which - 1] = value;
        }
      }
      ret_tokens.emplace_back(string_printf("%hhu/%hhu/%hhu/%hhu/%hhu",
          percentages[0], percentages[1], percentages[2], percentages[3], percentages[4]));
    }

    // For armors, add the slots, unit modifiers, and/or DEF/EVP bonuses
  } else if (this->data1[0] == 0x01) {
    if (this->data1[1] == 0x03) { // Units
      uint16_t modifier = (this->data1[8] << 8) | this->data1[7];
      if (modifier == 0x0001 || modifier == 0x0002) {
        ret_tokens.back().append("+");
      } else if (modifier == 0x0003 || modifier == 0x0004) {
        ret_tokens.back().append("++");
      } else if (modifier == 0xFFFF || modifier == 0xFFFE) {
        ret_tokens.back().append("-");
      } else if (modifier == 0xFFFD || modifier == 0xFFFC) {
        ret_tokens.back().append("--");
      } else if (modifier != 0x0000) {
        ret_tokens.emplace_back(string_printf("!MD:%04hX", modifier));
      }

    } else { // Armor/shields
      if (this->data1[5] > 0) {
        if (this->data1[5] == 1) {
          ret_tokens.emplace_back("(1 slot)");
        } else {
          ret_tokens.emplace_back(string_printf("(%hhu slots)", this->data1[5]));
        }
      }
      if (this->data1w[3] != 0) {
        ret_tokens.emplace_back(string_printf("+%hdDEF",
            static_cast<int16_t>(this->data1w[3].load())));
      }
      if (this->data1w[4] != 0) {
        ret_tokens.emplace_back(string_printf("+%hdEVP",
            static_cast<int16_t>(this->data1w[4].load())));
      }
    }

    // For mags, add tons of info
  } else if (this->data1[0] == 0x02) {
    ret_tokens.emplace_back(string_printf("LV%hhu", this->data1[2]));

    ret_tokens.emplace_back(string_printf("%d/%d/%d/%d",
        this->data1w[2] / 100, this->data1w[3] / 100,
        this->data1w[4] / 100, this->data1w[5] / 100));
    ret_tokens.emplace_back(string_printf("%hhu%%", this->data2[0]));
    ret_tokens.emplace_back(string_printf("%hhuIQ", this->data2[1]));

    uint8_t flags = this->data2[2];
    if (flags & 7) {
      static const vector<const char*> pb_shortnames = {
          "F", "E", "G", "P", "L", "M&Y", "MG", "GR"};

      const char* pb_names[3] = {nullptr, nullptr, nullptr};
      uint8_t left_pb = this->mag_photon_blast_for_slot(2);
      uint8_t center_pb = this->mag_photon_blast_for_slot(0);
      uint8_t right_pb = this->mag_photon_blast_for_slot(1);
      if (left_pb != 0xFF) {
        pb_names[0] = pb_shortnames[left_pb];
      }
      if (center_pb != 0xFF) {
        pb_names[1] = pb_shortnames[center_pb];
      }
      if (right_pb != 0xFF) {
        pb_names[2] = pb_shortnames[right_pb];
      }

      string token = "PB:";
      for (size_t x = 0; x < 3; x++) {
        if (pb_names[x] == nullptr) {
          continue;
        }
        if (token.size() > 3) {
          token += ',';
        }
        token += pb_names[x];
      }
      ret_tokens.emplace_back(std::move(token));
    }

    try {
      ret_tokens.emplace_back(string_printf("(%s)", name_for_mag_color.at(this->data2[3])));
    } catch (const out_of_range&) {
      ret_tokens.emplace_back(string_printf("(!CL:%02hhX)", this->data2[3]));
    }

    // For tools, add the amount (if applicable)
  } else if (this->data1[0] == 0x03) {
    if (this->max_stack_size() > 1) {
      ret_tokens.emplace_back(string_printf("x%hhu", this->data1[5]));
    }
  }

  string ret = join(ret_tokens, " ");
  if (include_color_codes) {
    if (this->is_s_rank_weapon()) {
      return "$C4" + ret;
    } else if (is_rare) {
      return "$C6" + ret;
    } else if (this->has_bonuses()) {
      return "$C2" + ret;
    } else {
      return "$C7" + ret;
    }
  } else {
    return ret;
  }
}

ItemData item_for_string(const string& desc) {
  try {
    return ItemData(desc);
  } catch (const exception&) {
  }
  try {
    return ItemData(desc, true);
  } catch (const exception&) {
  }

  string data = parse_data_string(desc);
  if (data.size() < 2) {
    throw runtime_error("item code too short");
  }
  if (data[0] > 4) {
    throw runtime_error("invalid item class");
  }
  if (data.size() > 16) {
    throw runtime_error("item code too long");
  }

  ItemData ret;
  if (data.size() <= 12) {
    memcpy(ret.data1.data(), data.data(), data.size());
  } else {
    memcpy(ret.data1.data(), data.data(), 12);
    memcpy(ret.data2.data(), data.data() + 12, data.size() - 12);
  }
  return ret;
}
