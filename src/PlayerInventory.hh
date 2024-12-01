#pragma once

#include <inttypes.h>
#include <stddef.h>

#include <algorithm>
#include <array>
#include <phosg/Encoding.hh>
#include <phosg/JSON.hh>
#include <string>
#include <utility>
#include <vector>

#include "ChoiceSearch.hh"
#include "FileContentsCache.hh"
#include "ItemData.hh"
#include "PSOEncryption.hh"
#include "Text.hh"
#include "Version.hh"

class Client;
class ItemParameterTable;

// PSO V2 stored some extra data in the character structs in a format that I'm
// sure Sega thought was very clever for backward compatibility, but for us is
// just plain annoying. Specifically, they used the third and fourth bytes of
// the InventoryItem struct to store some things not present in V1. The game
// stores arrays of bytes striped across these structures. In newserv, we call
// those fields extension_data. They contain:
//   items[0].extension_data1 through items[19].extension_data1:
//       Extended technique levels. The values in the technique_levels_v1 array
//       only go up to 14 (tech level 15); if the player has a technique above
//       level 15, the corresponding extension_data1 field holds the remaining
//       levels (so a level 20 tech would have 14 in technique_levels_v1 and 5
//       in the corresponding item's extension_data1 field).
//   items[0].extension_data2 through items[3].extension_data2:
//       The flags field from the PSOGCCharacterFile::Character struct; see
//       SaveFileFormats.hh for details.
//   items[4].extension_data2 through items[7].extension_data2:
//       The timestamp when the character was last saved, in seconds since
//       January 1, 2000. Stored little-endian, so items[4] contains the LSB.
//   items[8].extension_data2 through items[12].extension_data2:
//       Number of power materials, mind materials, evade materials, def
//       materials, and luck materials (respectively) used by the player.
//   items[13].extension_data2 through items[15].extension_data2:
//       Unknown. These are not an array, but do appear to be related.

template <bool BE>
struct PlayerInventoryItemT {
  /* 00 */ uint8_t present = 0;
  /* 01 */ uint8_t unknown_a1 = 0;
  // See note above about these fields
  /* 02 */ uint8_t extension_data1 = 0;
  /* 03 */ uint8_t extension_data2 = 0;
  /* 04 */ U32T<BE> flags = 0; // 8 = equipped
  /* 08 */ ItemData data;
  /* 1C */

  PlayerInventoryItemT() = default;

  PlayerInventoryItemT(const ItemData& item, bool equipped)
      : present(1),
        unknown_a1(0),
        extension_data1(0),
        extension_data2(0),
        flags(equipped ? 8 : 0),
        data(item) {}

  operator PlayerInventoryItemT<!BE>() const {
    PlayerInventoryItemT<!BE> ret;
    ret.present = this->present;
    ret.unknown_a1 = this->unknown_a1;
    ret.extension_data1 = this->extension_data1;
    ret.extension_data2 = this->extension_data2;
    ret.flags = this->flags.load();
    ret.data = this->data;
    ret.data.id.store_raw(phosg::bswap32(ret.data.id.load_raw()));
    return ret;
  }
} __packed__;
using PlayerInventoryItem = PlayerInventoryItemT<false>;
using PlayerInventoryItemBE = PlayerInventoryItemT<true>;
check_struct_size(PlayerInventoryItem, 0x1C);
check_struct_size(PlayerInventoryItemBE, 0x1C);

template <bool BE>
struct PlayerBankItemT {
  /* 00 */ ItemData data;
  /* 14 */ U16T<BE> amount = 0;
  /* 16 */ U16T<BE> present = 0;
  /* 18 */

  inline bool operator<(const PlayerBankItemT<BE>& other) const {
    return this->data < other.data;
  }

  operator PlayerBankItemT<!BE>() const {
    PlayerBankItemT<!BE> ret;
    ret.data = this->data;
    ret.amount = this->amount.load();
    ret.present = this->present.load();
    return ret;
  }
} __packed__;
using PlayerBankItem = PlayerBankItemT<false>;
using PlayerBankItemBE = PlayerBankItemT<true>;
check_struct_size(PlayerBankItem, 0x18);
check_struct_size(PlayerBankItemBE, 0x18);

template <bool BE>
struct PlayerInventoryT {
  /* 0000 */ uint8_t num_items = 0;
  /* 0001 */ uint8_t hp_from_materials = 0;
  /* 0002 */ uint8_t tp_from_materials = 0;
  /* 0003 */ uint8_t language = 0;
  /* 0004 */ parray<PlayerInventoryItemT<BE>, 30> items;
  /* 034C */

  size_t find_item(uint32_t item_id) const {
    for (size_t x = 0; x < this->num_items; x++) {
      if (this->items[x].data.id == item_id) {
        return x;
      }
    }
    throw std::out_of_range("item not present");
  }

  size_t find_item_by_primary_identifier(uint32_t primary_identifier) const {
    for (size_t x = 0; x < this->num_items; x++) {
      if (this->items[x].data.primary_identifier() == primary_identifier) {
        return x;
      }
    }
    throw std::out_of_range("item not present");
  }

  size_t find_equipped_item(EquipSlot slot) const {
    ssize_t ret = -1;
    for (size_t y = 0; y < this->num_items; y++) {
      const auto& i = this->items[y];
      if (!(i.flags & 0x00000008)) {
        continue;
      }
      if (!i.data.can_be_equipped_in_slot(slot)) {
        continue;
      }

      // Units can be equipped in multiple slots, so the currently-equipped slot
      // is stored in the item data itself.
      if (((slot == EquipSlot::UNIT_1) && (i.data.data1[4] != 0x00)) ||
          ((slot == EquipSlot::UNIT_2) && (i.data.data1[4] != 0x01)) ||
          ((slot == EquipSlot::UNIT_3) && (i.data.data1[4] != 0x02)) ||
          ((slot == EquipSlot::UNIT_4) && (i.data.data1[4] != 0x03))) {
        continue;
      }

      if (ret < 0) {
        ret = y;
      } else {
        throw std::runtime_error("multiple items are equipped in the same slot");
      }
    }
    if (ret < 0) {
      throw std::out_of_range("no item is equipped in this slot");
    }
    return ret;
  }

  bool has_equipped_item(EquipSlot slot) const {
    try {
      this->find_equipped_item(slot);
      return true;
    } catch (const std::out_of_range&) {
      return false;
    }
  }

  void equip_item_id(uint32_t item_id, EquipSlot slot, bool allow_overwrite) {
    this->equip_item_index(this->find_item(item_id), slot, allow_overwrite);
  }

  void equip_item_index(size_t index, EquipSlot slot, bool allow_overwrite) {
    auto& item = this->items[index];

    if ((slot == EquipSlot::UNKNOWN) || !item.data.can_be_equipped_in_slot(slot)) {
      slot = item.data.default_equip_slot();
    }

    if (this->has_equipped_item(slot)) {
      if (allow_overwrite) {
        this->unequip_item_slot(slot);
      } else {
        throw std::runtime_error("equip slot is already in use");
      }
    }

    item.flags |= 0x00000008;
    // Units store which slot they're equipped in within the item data itself
    if ((item.data.data1[0] == 0x01) && (item.data.data1[1] == 0x03)) {
      item.data.data1[4] = static_cast<uint8_t>(slot) - 9;
    }
  }

  void unequip_item_id(uint32_t item_id) {
    this->unequip_item_index(this->find_item(item_id));
  }

  void unequip_item_slot(EquipSlot slot) {
    this->unequip_item_index(this->find_equipped_item(slot));
  }

  void unequip_item_index(size_t index) {
    auto& item = this->items[index];

    item.flags &= (~0x00000008);
    // Units store which slot they're equipped in within the item data itself
    if ((item.data.data1[0] == 0x01) && (item.data.data1[1] == 0x03)) {
      item.data.data1[4] = 0x00;
    }
    // If the item is an armor, remove all units too
    if ((item.data.data1[0] == 0x01) && (item.data.data1[1] == 0x01)) {
      for (size_t z = 0; z < 30; z++) {
        auto& unit = this->items[z];
        if ((unit.data.data1[0] == 0x01) && (unit.data.data1[1] == 0x03)) {
          unit.flags &= (~0x00000008);
          unit.data.data1[4] = 0x00;
        }
      }
    }
  }

  size_t remove_all_items_of_type(uint8_t data1_0, int16_t data1_1 = -1) {

    size_t write_offset = 0;
    for (size_t read_offset = 0; read_offset < this->num_items; read_offset++) {
      bool should_delete = ((this->items[read_offset].data.data1[0] == data1_0) &&
          ((data1_1 < 0) || (this->items[read_offset].data.data1[1] == static_cast<uint8_t>(data1_1))));
      if (!should_delete) {
        if (read_offset != write_offset) {
          this->items[write_offset].present = this->items[read_offset].present;
          this->items[write_offset].unknown_a1 = this->items[read_offset].unknown_a1;
          this->items[write_offset].flags = this->items[read_offset].flags;
          this->items[write_offset].data = this->items[read_offset].data;
        }
        write_offset++;
      }
    }
    size_t ret = this->num_items - write_offset;
    this->num_items = write_offset;
    return ret;
  }

  void decode_from_client(Version v) {
    for (size_t z = 0; z < this->items.size(); z++) {
      this->items[z].data.decode_for_version(v);
    }
  }

  void encode_for_client(Version v, std::shared_ptr<const ItemParameterTable> item_parameter_table) {
    if (v == Version::DC_NTE) {
      // DC NTE has the item count as a 32-bit value here, whereas every other
      // version uses a single byte. To stop DC NTE from crashing by trying to
      // construct far more than 30 TItem objects, we clear the fields DC NTE
      // doesn't know about. Note that the 11/2000 prototype does not have this
      // issue - its inventory format matches the rest of the versions.
      this->hp_from_materials = 0;
      this->tp_from_materials = 0;
      this->language = 0;
    } else if ((v != Version::PC_NTE) && (v != Version::PC_V2)) {
      if (this->language > 4) {
        this->language = 0;
      }
    } else {
      if (this->language > 7) {
        this->language = 0;
      }
    }

    // For pre-V2 clients, use the V2 parameter table, since the V1 table
    // doesn't have correct encodings for backward-compatible V2 items.
    for (size_t z = 0; z < this->items.size(); z++) {
      this->items[z].data.encode_for_version(v, item_parameter_table);
    }
  }

  operator PlayerInventoryT<!BE>() const {
    PlayerInventoryT<!BE> ret;
    ret.num_items = this->num_items;
    ret.hp_from_materials = this->hp_from_materials;
    ret.tp_from_materials = this->tp_from_materials;
    ret.language = this->language;
    ret.items = this->items;
    return ret;
  }
} __packed__;
using PlayerInventory = PlayerInventoryT<false>;
using PlayerInventoryBE = PlayerInventoryT<true>;
check_struct_size(PlayerInventory, 0x34C);
check_struct_size(PlayerInventoryBE, 0x34C);

template <size_t SlotCount, bool BE>
struct PlayerBankT {
  /* 0000 */ U32T<BE> num_items = 0;
  /* 0004 */ U32T<BE> meseta = 0;
  /* 0008 */ parray<PlayerBankItemT<BE>, SlotCount> items;
  /* 05A8 for 60 items (v1/v2), 12C8 for 200 items (v3/v4) */

  uint32_t checksum() const {
    return phosg::crc32(this, 2 * sizeof(U32T<BE>) + sizeof(PlayerBankItemT<BE>) * std::min<size_t>(SlotCount, this->num_items));
  }

  void add_item(const ItemData& item, const ItemData::StackLimits& limits) {
    uint32_t primary_identifier = item.primary_identifier();

    if (primary_identifier == 0x04000000) {
      this->meseta += item.data2d;
      if (this->meseta > 999999) {
        this->meseta = 999999;
      }
      return;
    }

    size_t combine_max = item.max_stack_size(limits);
    if (combine_max > 1) {
      size_t y;
      for (y = 0; y < this->num_items; y++) {
        if (this->items[y].data.primary_identifier() == primary_identifier) {
          break;
        }
      }

      if (y < this->num_items) {
        uint8_t new_count = this->items[y].data.data1[5] + item.data1[5];
        if (new_count > combine_max) {
          throw std::runtime_error("stack size would exceed limit");
        }
        this->items[y].data.data1[5] = new_count;
        this->items[y].amount = new_count;
        return;
      }
    }

    if (this->num_items >= SlotCount) {
      throw std::runtime_error("no free space in bank");
    }
    auto& last_item = this->items[this->num_items];
    last_item.data = item;
    last_item.amount = (item.max_stack_size(limits) > 1) ? item.data1[5] : 1;
    last_item.present = 1;
    this->num_items++;
  }

  ItemData remove_item(uint32_t item_id, uint32_t amount, const ItemData::StackLimits& limits) {
    size_t index = this->find_item(item_id);
    auto& bank_item = this->items[index];

    ItemData ret;
    if (amount && (bank_item.data.stack_size(limits) > 1) && (amount < bank_item.data.data1[5])) {
      ret = bank_item.data;
      ret.data1[5] = amount;
      bank_item.data.data1[5] -= amount;
      bank_item.amount -= amount;
      return ret;
    }

    ret = bank_item.data;
    this->num_items--;
    for (size_t x = index; x < this->num_items; x++) {
      this->items[x] = this->items[x + 1];
    }
    auto& last_item = this->items[this->num_items];
    last_item.amount = 0;
    last_item.present = 0;
    last_item.data.clear();
    return ret;
  }

  size_t find_item(uint32_t item_id) {
    for (size_t x = 0; x < this->num_items; x++) {
      if (this->items[x].data.id == item_id) {
        return x;
      }
    }
    throw std::out_of_range("item not present");
  }

  void sort() {
    std::sort(this->items.data(), this->items.data() + this->num_items);
  }

  void assign_ids(uint32_t base_id) {
    for (size_t z = 0; z < this->num_items; z++) {
      this->items[z].data.id = base_id + z;
    }
  }

  void decode_from_client(Version v) {
    for (size_t z = 0; z < this->items.size(); z++) {
      this->items[z].data.decode_for_version(v);
    }
  }

  void encode_for_client(Version v) {
    for (size_t z = 0; z < this->items.size(); z++) {
      this->items[z].data.encode_for_version(v, nullptr);
    }
  }

  template <size_t DestSlotCount, bool DestBE>
  operator PlayerBankT<DestSlotCount, DestBE>() const {
    PlayerBankT<DestSlotCount, DestBE> ret;
    ret.num_items = std::min<size_t>(ret.items.size(), this->num_items.load());
    ret.meseta = this->meseta.load();
    for (size_t z = 0; z < std::min<size_t>(ret.items.size(), this->items.size()); z++) {
      ret.items[z] = this->items[z];
    }
    return ret;
  }
} __packed__;
using PlayerBank60 = PlayerBankT<60, false>;
using PlayerBank200 = PlayerBankT<200, false>;
using PlayerBank200BE = PlayerBankT<200, true>;
check_struct_size(PlayerBank60, 0x05A8);
check_struct_size(PlayerBank200, 0x12C8);
check_struct_size(PlayerBank200BE, 0x12C8);
