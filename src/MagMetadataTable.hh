#pragma once

#include "WindowsPlatform.hh"

#include <stdint.h>

#include <memory>
#include <string>

#include "CommonFileFormats.hh"
#include "Text.hh"
#include "Types.hh"
#include "Version.hh"

class MagMetadataTable {
public:
  struct MotionReferences {
    // These entries specify which entry in ItemMagMotion.dat is used. The file is just a list of 0x64-byte structures.
    // 0xFF = no TItemMagSub is created
    struct Side {
      uint8_t eff_1;
      uint8_t eff_2;
      uint8_t eff_3;
      uint8_t eff_4_8;
      uint8_t eff_5;
      uint8_t eff_6_7;

      bool operator==(const Side&) const = default;
      bool operator!=(const Side&) const = default;
    } __packed_ws__(Side, 6);
    parray<Side, 2> sides; // [0] = right side, [1] = left side

    bool operator==(const MotionReferences&) const = default;
    bool operator!=(const MotionReferences&) const = default;

    static MotionReferences from_json(const phosg::JSON& json);
    phosg::JSON json() const;
  } __packed_ws__(MotionReferences, 0x0C);

  struct UnknownA3Entry {
    uint8_t flags;
    uint8_t unknown_a2;
    int16_t unknown_a3;
    int16_t unknown_a4;
    int16_t unknown_a5;

    static UnknownA3Entry from_json(const phosg::JSON& json);
    phosg::JSON json() const;
  };

  virtual ~MagMetadataTable() = default;

  virtual size_t num_mags() const = 0;

  virtual size_t num_motion_entries(bool use_second_table) const = 0;
  virtual const MotionReferences& get_motion_references(bool use_second_table, size_t index) const = 0;

  virtual std::pair<uint8_t, uint8_t> get_unknown_a2(size_t index) const = 0;

  virtual size_t num_unknown_a3_entries() const = 0;
  virtual const UnknownA3Entry& get_unknown_a3(size_t index) const = 0;

  virtual uint8_t get_render_flags(size_t index) const = 0;

  virtual size_t num_colors() const = 0;
  virtual const VectorXYZTF& get_color_rgba(size_t index) const = 0;

  virtual uint8_t get_evolution_number(uint8_t data1_1) const = 0;

  static std::shared_ptr<MagMetadataTable> from_binary(std::shared_ptr<const std::string> data, Version version);
  static std::shared_ptr<MagMetadataTable> from_json(const phosg::JSON& json);

  phosg::JSON json() const;
  std::string serialize_binary(Version version) const;

protected:
  MagMetadataTable() = default;
};
