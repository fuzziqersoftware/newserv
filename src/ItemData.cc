#include "ItemData.hh"

#include <array>
#include <map>

#include "ItemParameterTable.hh"
#include "StaticGameData.hh"

using namespace std;

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
  } else if (this->is_s_rank_weapon()) {
    return (this->data1[0] << 16) | (this->data1[1] << 8);
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

void ItemData::wrap() {
  switch (this->data1[0]) {
    case 0:
    case 1:
      this->data1[4] |= 0x40;
      break;
    case 2:
      this->data2[2] |= 0x40;
      break;
    case 3:
      if (!this->is_stackable()) {
        this->data1[3] |= 0x40;
      }
      break;
    case 4:
      break;
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

void ItemData::decode_for_version(GameVersion from_version) {
  bool is_v2 = (from_version == GameVersion::DC) || (from_version == GameVersion::PC);

  uint8_t encoded_v2_data = this->get_encoded_v2_data();
  bool should_decode_v2_data = is_v2 && (encoded_v2_data != 0x00) && this->has_encoded_v2_data();

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

      if (from_version == GameVersion::GC) {
        // PSO GC erroneously byteswaps the data2d field, even though it's actually
        // just four individual bytes, so we correct for that here.
        this->data2d = bswap32(this->data2d);

      } else if (from_version == GameVersion::DC || from_version == GameVersion::PC) {
        // PSO PC encodes mags in a tediously annoying manner. The first four bytes are the same, but then...
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
        this->data1w[2] &= 0x7FFE;
        this->data1w[3] &= 0x7FFE;
        this->data1w[4] &= 0xFFFE;
        this->data1w[5] &= 0xFFFE;
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

void ItemData::encode_for_version(GameVersion to_version, shared_ptr<const ItemParameterTable> item_parameter_table) {
  bool is_v2 = (to_version == GameVersion::DC) || (to_version == GameVersion::PC);
  bool should_encode_v2_data = is_v2 && !this->has_encoded_v2_data();

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
      if (to_version == GameVersion::GC) {
        this->data2d = bswap32(this->data2d);
      } else if (to_version == GameVersion::DC || to_version == GameVersion::PC) {
        this->data1w[2] = (this->data1w[2] & 0x7FFE) | ((this->data2[2] << 14) & 0x8000) | (this->data2[3] & 1);
        this->data1w[3] = (this->data1w[3] & 0x7FFE) | ((this->data2[2] << 13) & 0x8000) | ((this->data2[3] >> 1) & 1);
        this->data1w[4] = (this->data1w[4] & 0xFFFE) | ((this->data2[3] >> 2) & 1);
        this->data1w[5] = (this->data1w[5] & 0xFFFE) | ((this->data2[3] >> 3) & 1);
        // Order is important; data2w[0] must not be written before data2[0] is read
        this->data2w[1] = this->data2[0] | ((this->data2[2] << 15) & 0x8000);
        this->data2w[0] = this->data2[1];
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

uint16_t ItemData::get_sealed_item_kill_count() const {
  return ((this->data1[10] << 8) | this->data1[11]) & 0x7FFF;
}

void ItemData::set_sealed_item_kill_count(uint16_t v) {
  if (v > 0x7FFF) {
    this->data1w[5] = 0xFFFF;
  } else {
    this->data1[10] = (v >> 8) | 0x80;
    this->data1[11] = v;
  }
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

string ItemData::hex() const {
  return string_printf("%02hhX%02hhX%02hhX%02hhX %02hhX%02hhX%02hhX%02hhX %02hhX%02hhX%02hhX%02hhX (%08" PRIX32 ") %02hhX%02hhX%02hhX%02hhX",
      this->data1[0], this->data1[1], this->data1[2], this->data1[3],
      this->data1[4], this->data1[5], this->data1[6], this->data1[7],
      this->data1[8], this->data1[9], this->data1[10], this->data1[11],
      this->id.load(),
      this->data2[0], this->data2[1], this->data2[2], this->data2[3]);
}
