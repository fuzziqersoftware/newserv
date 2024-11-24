#include "ItemData.hh"

#include <array>
#include <map>

#include "ItemParameterTable.hh"
#include "StaticGameData.hh"

using namespace std;

const vector<uint8_t> ItemData::StackLimits::DEFAULT_TOOL_LIMITS_DC_NTE(
    {10});
const vector<uint8_t> ItemData::StackLimits::DEFAULT_TOOL_LIMITS_V1_V2(
    {10, 10, 1, 10, 10, 10, 10, 10, 10, 1});
const vector<uint8_t> ItemData::StackLimits::DEFAULT_TOOL_LIMITS_V3_V4(
    {10, 10, 1, 10, 10, 10, 10, 10, 10, 1, 1, 1, 1, 1, 1, 1, 99, 1});

const ItemData::StackLimits ItemData::StackLimits::DEFAULT_STACK_LIMITS_DC_NTE(
    Version::DC_NTE, ItemData::StackLimits::DEFAULT_TOOL_LIMITS_DC_NTE, 999999);
const ItemData::StackLimits ItemData::StackLimits::DEFAULT_STACK_LIMITS_V1_V2(
    Version::DC_V1, ItemData::StackLimits::DEFAULT_TOOL_LIMITS_V1_V2, 999999);
const ItemData::StackLimits ItemData::StackLimits::DEFAULT_STACK_LIMITS_V3_V4(
    Version::GC_V3, ItemData::StackLimits::DEFAULT_TOOL_LIMITS_V3_V4, 999999);

ItemData::StackLimits::StackLimits(
    Version version, const vector<uint8_t>& max_tool_stack_sizes_by_data1_1, uint32_t max_meseta_stack_size)
    : version(version),
      max_tool_stack_sizes_by_data1_1(max_tool_stack_sizes_by_data1_1),
      max_meseta_stack_size(max_meseta_stack_size) {}

ItemData::StackLimits::StackLimits(Version version, const phosg::JSON& json)
    : version(version) {
  this->max_tool_stack_sizes_by_data1_1.clear();
  for (const auto& limit_json : json.at("ToolLimits").as_list()) {
    this->max_tool_stack_sizes_by_data1_1.emplace_back(limit_json->as_int());
  }
  this->max_meseta_stack_size = json.at("MesetaLimit").as_int();
}

uint8_t ItemData::StackLimits::get(uint8_t data1_0, uint8_t data1_1) const {
  if (data1_0 == 4) {
    return this->max_meseta_stack_size;
  }
  if (data1_0 == 3) {
    const auto& vec = this->max_tool_stack_sizes_by_data1_1;
    return vec.at(min<size_t>(data1_1, vec.size() - 1));
  }
  return 1;
}

ItemData::ItemData() {
  this->clear();
}

ItemData::ItemData(const ItemData& other) {
  this->data1d = other.data1d;
  this->id = other.id;
  this->data2d = other.data2d;
}

ItemData::ItemData(uint64_t first, uint64_t second) {
  *reinterpret_cast<be_uint64_t*>(&this->data1[0]) = first;
  this->data1d[2] = phosg::bswap32((second >> 32) & 0xFFFFFFFF);
  this->data2d = phosg::bswap32(second & 0xFFFFFFFF);
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

bool ItemData::operator<(const ItemData& other) const {
  for (size_t z = 0; z < 3; z++) {
    if (this->data1db[z] < other.data1db[z]) {
      return true;
    } else if (this->data1db[z] > other.data1db[z]) {
      return false;
    }
  }
  return (this->data2db < other.data2db);
}

void ItemData::clear() {
  this->data1d.clear(0);
  this->id = 0xFFFFFFFF;
  this->data2d = 0;
}

bool ItemData::empty() const {
  return (this->data1d[0] == 0) &&
      (this->data1d[1] == 0) &&
      (this->data1d[2] == 0) &&
      (this->data2d == 0);
}

uint32_t ItemData::primary_identifier() const {
  // Primary identifiers are like:
  // - 00TTSS00 = weapon (T = type, S = subtype; subtype is 0 for ES weapons)
  // - 01TTSS00 = armor/shield/unit
  // - 02TT0000 = mag
  // - 0302ZZLL = tech disk (Z = tech number, L = level)
  // - 03TTSS00 = tool
  // - 04000000 = meseta

  // The game treats any item starting with 04 as Meseta, and ignores the rest
  // of data1 (the value is in data2)
  if (this->data1[0] == 0x04) {
    return 0x04000000;
  }
  if (this->data1[0] == 0x03 && this->data1[1] == 0x02) {
    // Tech disk (tech ID is data1[4], not [2])
    return 0x03020000 | (this->data1[4] << 8) | this->data1[2];
  } else if (this->data1[0] == 0x02) {
    return 0x02000000 | (this->data1[1] << 16); // Mag
  } else if (this->is_s_rank_weapon()) {
    return (this->data1[0] << 24) | (this->data1[1] << 16);
  } else {
    return (this->data1[0] << 24) | (this->data1[1] << 16) | (this->data1[2] << 8);
  }
}

bool ItemData::is_wrapped(const StackLimits& limits) const {
  switch (this->data1[0]) {
    case 0:
    case 1:
      return this->data1[4] & 0x40;
    case 2:
      return this->data2[2] & 0x40;
    case 3:
      return !this->is_stackable(limits) && (this->data1[3] & 0x40);
    case 4:
      return false;
    default:
      throw runtime_error("invalid item data");
  }
}

void ItemData::wrap(const StackLimits& limits, uint8_t present_color) {
  switch (this->data1[0]) {
    case 0:
      this->data1[4] |= 0x40;
      this->data1[5] = (this->data1[5] & 0xF0) | (present_color & 0x0F);
      break;
    case 1:
      this->data1[4] = (this->data1[4] & 0xF0) | 0x40 | (present_color & 0x0F);
      break;
    case 2:
      // Mags cannot have custom present colors
      this->data2[2] |= 0x40;
      break;
    case 3:
      if (!this->is_stackable(limits)) {
        this->data1[3] = (this->data1[3] & 0xF0) | 0x40 | (present_color & 0x0F);
      }
      break;
    case 4:
      break;
    default:
      throw runtime_error("invalid item data");
  }
}

void ItemData::unwrap(const StackLimits& limits) {
  switch (this->data1[0]) {
    case 0:
    case 1:
      this->data1[4] &= 0xB0;
      break;
    case 2:
      this->data2[2] &= 0xB0;
      break;
    case 3:
      if (!this->is_stackable(limits)) {
        this->data1[3] &= 0xB0;
      }
      break;
    case 4:
      break;
    default:
      throw runtime_error("invalid item data");
  }
}

bool ItemData::is_stackable(const StackLimits& limits) const {
  return this->max_stack_size(limits) > 1;
}

size_t ItemData::stack_size(const StackLimits& limits) const {
  if (this->max_stack_size(limits) > 1) {
    return this->data1[5];
  }
  return 1;
}

size_t ItemData::max_stack_size(const StackLimits& limits) const {
  return limits.get(this->data1[0], this->data1[1]);
}

void ItemData::enforce_min_stack_size(const StackLimits& limits) {
  if (this->stack_size(limits) == 0) {
    this->data1[5] = 1;
  }
}

bool ItemData::is_common_consumable(uint32_t primary_identifier) {
  return (primary_identifier >= 0x03000000) &&
      (primary_identifier < 0x030A0000) &&
      ((primary_identifier & 0xFFFF0000) != 0x03020000);
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
    }
    pb_nums |= (pb_num << 6);
    flags |= 4;
  }
}

void ItemData::decode_for_version(Version from_version) {
  uint8_t encoded_v2_data = this->get_encoded_v2_data();
  bool should_decode_v2_data = (is_v1(from_version) || is_v2(from_version)) && (from_version != Version::GC_NTE) &&
      (encoded_v2_data != 0x00) && this->has_encoded_v2_data();

  switch (this->data1[0]) {
    case 0x00:
      if (should_decode_v2_data) {
        this->data1[5] = ((encoded_v2_data < 0x89) || (this->data1[1] == 0x0F)) ? 0x00 : this->data1[1];
        this->data1[1] = encoded_v2_data;
      }
      break;

    case 0x01:
      if (should_decode_v2_data) {
        this->data1[3] = 0x00;
        this->data1[2] = encoded_v2_data;
      }
      break;

    case 0x02:
      if (should_decode_v2_data) {
        this->data1[2] = 0xC8;
        this->data1[1] = encoded_v2_data + 0x2B;
      }

      if (is_v1(from_version) || is_v2(from_version)) {
        // PSO PC and GC NTE encode mags in a tediously annoying manner. The
        // first four bytes are the same, but then...
        // V2: pHHHHHHHHHHHHHHc pIIIIIIIIIIIIIIc JJJJJJJJJJJJJJJc KKKKKKKKKKKKKKKc QQQQQQQQ QQQQQQQQ YYYYYYYY pYYYYYYY
        // V3: HHHHHHHHHHHHHHHH IIIIIIIIIIIIIIII JJJJJJJJJJJJJJJJ KKKKKKKKKKKKKKKK YYYYYYYY QQQQQQQQ PPPPPPPP CCCCCCCC
        // c = color in V2 (4 bits; low bit first)
        // C = color in V3
        // p = PB flag bits in V2 (3 bits; ordered 1, 2, 0)
        // P = PB flag bits in V3
        // H, I, J, K = DEF, POW, DEX, MIND
        // Q = IQ (little-endian in V2)
        // Y = synchro (little-endian in V2)

        // Order is important; data2[0] must not be written before data2w[0] is read
        this->data2[1] = this->data2w[0]; // IQ
        this->data2[0] = this->data2w[1] & 0x7FFF; // Synchro
        this->data2[2] = ((this->data2[3] >> 7) & 1) | ((this->data1w[2] >> 14) & 2) | ((this->data1w[3] >> 13) & 4); // PB flags
        this->data2[3] = (this->data1w[2] & 1) | ((this->data1w[3] & 1) << 1) | ((this->data1w[4] & 1) << 2) | ((this->data1w[5] & 1) << 3); // Color
        // 01000080
        this->data1w[2] &= 0x7FFE;
        this->data1w[3] &= 0x7FFE;
        this->data1w[4] &= 0xFFFE;
        this->data1w[5] &= 0xFFFE;

      } else if (is_big_endian(from_version)) {
        // PSO GC (but not GC NTE, which uses the above logic) byteswaps the
        // data2d field, since internally it's actually a uint32_t. We treat it
        // as individual bytes instead, so we correct for the client's
        // byteswapping here.
        this->data2d = phosg::bswap32(this->data2d);
      }
      break;

    case 0x03:
      if (should_decode_v2_data) {
        this->data1[3] = 0x00;
        if (this->data1[1] == 0x02) {
          this->data1[2] = encoded_v2_data + 0x0E;
        } else if (this->data1[1] == 0x0D) {
          if (this->data1[2] == 0x06) {
            this->data1[1] = 0x0E;
            this->data1[2] = encoded_v2_data + -1;
          } else if (this->data1[2] == 0x07) {
            this->data1[1] = this->data1[6];
            this->data1[2] = encoded_v2_data + -1;
            this->data1[6] = 0x00;
          }
        }
      }
      break;

    case 0x04:
      break;

    default:
      throw runtime_error("invalid item class");
  }
}

void ItemData::encode_for_version(Version to_version, shared_ptr<const ItemParameterTable> item_parameter_table) {
  bool should_encode_v2_data = item_parameter_table &&
      (is_v1(to_version) || is_v2(to_version)) &&
      (to_version != Version::GC_NTE) &&
      !this->has_encoded_v2_data();

  switch (this->data1[0]) {
    case 0x00:
      if (should_encode_v2_data && (this->data1[1] > 0x26)) {
        if (this->data1[1] < 0x89) {
          this->data1[5] = this->data1[1];
          this->data1[1] = item_parameter_table->get_weapon_v1_replacement(this->data1[1]);
          if (this->data1[1] == 0x00) {
            this->data1[1] = 0x0F;
          }
        } else {
          uint8_t data1_5 = this->data1[5];
          if (this->data1[1] > data1_5) {
            this->data1[5] = this->data1[1];
            this->data1[1] = (data1_5 != 0) ? data1_5 : 0x0F;
          }
        }
      }
      break;

    case 0x01: {
      static const array<uint8_t, 4> armor_limits = {0x00, 0x29, 0x27, 0x44};
      if (should_encode_v2_data && (this->data1[2] >= armor_limits[this->data1[1]])) {
        this->data1[3] = this->data1[2];
        this->data1[2] = 0x00;
      }
      break;
    }

    case 0x02:
      if (should_encode_v2_data && (this->data1[1] > 0x2B)) {
        this->data1[2] = this->data1[1] - 0x2B - 0x38;
        this->data1[1] = 0x00;
      }

      // This logic is the inverse of the corresponding logic in
      // decode_for_version; see that function for a description of what's
      // going on here.
      if (is_v1(to_version) || is_v2(to_version)) {
        this->data1w[2] = (this->data1w[2] & 0x7FFE) | ((this->data2[2] << 14) & 0x8000) | (this->data2[3] & 1);
        this->data1w[3] = (this->data1w[3] & 0x7FFE) | ((this->data2[2] << 13) & 0x8000) | ((this->data2[3] >> 1) & 1);
        this->data1w[4] = (this->data1w[4] & 0xFFFE) | ((this->data2[3] >> 2) & 1);
        this->data1w[5] = (this->data1w[5] & 0xFFFE) | ((this->data2[3] >> 3) & 1);
        // Order is important; data2w[0] must not be written before data2[0] is read
        this->data2w[1] = this->data2[0] | ((this->data2[2] << 15) & 0x8000);
        this->data2w[0] = this->data2[1];
      } else if (is_big_endian(to_version)) {
        this->data2d = phosg::bswap32(this->data2d);
      }
      break;

    case 0x03:
      if (should_encode_v2_data) {
        if (this->data1[1] == 2) {
          if (this->data1[2] > 0x0E) {
            this->data1[3] = this->data1[2] - 0x0E;
            this->data1[2] %= 0x0F;
          }
        } else if (this->data1[1] == 0x0E) {
          this->data1[3] = this->data1[2] + 1;
          this->data1[1] = 0x0D;
          this->data1[2] = 0x06;
        } else if (this->data1[1] > 0x0E) {
          this->data1[6] = this->data1[1];
          this->data1[3] = this->data1[2] + 0x01;
          this->data1[1] = 0x0D;
          this->data1[2] = 0x07;
        }
      }
      break;

    case 0x04:
      break;

    default:
      throw runtime_error("invalid item class");
  }
}

uint8_t ItemData::get_encoded_v2_data() const {
  switch (this->data1[0]) {
    case 0x00:
      return this->data1[5];
    case 0x01:
    case 0x03:
      return this->data1[3];
    case 0x02:
      if (this->data1[2] > 0xC8) {
        return this->data1[2] + 0x38;
      }
      return 0x00;
    default:
      return 0x00;
  }
}

bool ItemData::has_encoded_v2_data() const {
  return (this->data1[0] == 0)
      ? (this->data1[1] < this->get_encoded_v2_data())
      : (this->get_encoded_v2_data() != 0);
}

bool ItemData::has_kill_count() const {
  return !this->is_s_rank_weapon() && (this->data1[10] & 0x80);
}

uint16_t ItemData::get_kill_count() const {
  return this->has_kill_count() ? (((this->data1[10] << 8) | this->data1[11]) & 0x7FFF) : 0;
}

void ItemData::set_kill_count(uint16_t v) {
  if (!this->is_s_rank_weapon()) {
    if (v > 0x7FFF) {
      this->data1w[5] = 0xFFFF;
    } else {
      this->data1[10] = (v >> 8) | 0x80;
      this->data1[11] = v;
    }
  }
}

uint8_t ItemData::get_tool_item_amount(const StackLimits& limits) const {
  return this->is_stackable(limits) ? this->data1[5] : 1;
}

void ItemData::set_tool_item_amount(const StackLimits& limits, uint8_t amount) {
  if (this->is_stackable(limits)) {
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

EquipSlot ItemData::default_equip_slot() const {
  switch (this->data1[0]) {
    case 0x00:
      return EquipSlot::WEAPON;
    case 0x01:
      switch (this->data1[1]) {
        case 0x01:
          return EquipSlot::ARMOR;
        case 0x02:
          return EquipSlot::SHIELD;
        case 0x03:
          return EquipSlot::UNIT_1;
      }
      break;
    case 0x02:
      return EquipSlot::MAG;
  }
  throw runtime_error("item cannot be equipped");
}

bool ItemData::can_be_equipped_in_slot(EquipSlot slot) const {
  switch (slot) {
    case EquipSlot::MAG:
      return (this->data1[0] == 0x02);
    case EquipSlot::ARMOR:
      return ((this->data1[0] == 0x01) && (this->data1[1] == 0x01));
    case EquipSlot::SHIELD:
      return ((this->data1[0] == 0x01) && (this->data1[1] == 0x02));
    case EquipSlot::UNIT_1:
    case EquipSlot::UNIT_2:
    case EquipSlot::UNIT_3:
    case EquipSlot::UNIT_4:
      return ((this->data1[0] == 0x01) && (this->data1[1] == 0x03));
    case EquipSlot::WEAPON:
      return (this->data1[0] == 0x00);
    default:
      throw runtime_error("invalid equip slot");
  }
}

bool ItemData::can_be_encoded_in_rel_rare_table() const {
  return !(this->data1[3] || this->data1d[1] || this->data1d[2] || this->data2d);
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

ItemData ItemData::from_data(const string& data) {
  if (data.size() < 2) {
    throw runtime_error("data is too short");
  }
  if (data.size() > 0x10) {
    throw runtime_error("data is too long");
  }

  ItemData ret;
  for (size_t z = 0; z < min<size_t>(data.size(), 12); z++) {
    ret.data1[z] = data[z];
  }
  for (size_t z = 12; z < min<size_t>(data.size(), 16); z++) {
    ret.data2[z - 12] = data[z];
  }
  if (ret.data1[0] > 4) {
    throw runtime_error("invalid item class");
  }
  return ret;
}

ItemData ItemData::from_primary_identifier(const StackLimits& limits, uint32_t primary_identifier) {
  ItemData ret;
  if (primary_identifier > 0x04000000) {
    throw runtime_error("invalid item class");
  }
  ret.data1[0] = (primary_identifier >> 24) & 0xFF;
  ret.data1[1] = (primary_identifier >> 16) & 0xFF;
  if ((primary_identifier & 0xFFFF0000) == 0x03020000) {
    ret.data1[4] = (primary_identifier >> 8) & 0xFF;
    ret.data1[2] = primary_identifier & 0xFF;
  } else {
    ret.data1[2] = (primary_identifier >> 8) & 0xFF;
  }
  ret.set_tool_item_amount(limits, 1);
  return ret;
}

string ItemData::hex() const {
  return phosg::string_printf("%08" PRIX32 " %08" PRIX32 " %08" PRIX32 " (%08" PRIX32 ") %08" PRIX32,
      this->data1db[0].load(), this->data1db[1].load(), this->data1db[2].load(), this->id.load(), this->data2db.load());
}

string ItemData::short_hex() const {
  auto ret = phosg::string_printf("%08" PRIX32 "%08" PRIX32 "%08" PRIX32 "%08" PRIX32,
      this->data1db[0].load(), this->data1db[1].load(), this->data1db[2].load(), this->data2db.load());
  size_t offset = ret.find_last_not_of('0');
  if (offset != string::npos) {
    offset += (offset & 1) ? 1 : 2;
    if (offset < ret.size()) {
      ret.resize(offset);
    }
  }
  return ret;
}
