#pragma once

#include "WindowsPlatform.hh"

#include <stdint.h>

#include <memory>
#include <string>

#include "CommonFileFormats.hh"
#include "Text.hh"
#include "Types.hh"
#include "Version.hh"

class MagEvolutionTable {
public:
  struct MotionReference {
    struct Side {
      // This specifies which entry in ItemMagMotion.dat is used. The file is just a list of 0x64-byte structures.
      // 0xFF = no TItemMagSub is created
      uint8_t motion_table_entry = 0xFF;
      parray<uint8_t, 5> unknown_a1 = 0;
    } __packed_ws__(Side, 0x06);
    parray<Side, 2> sides; // [0] = right side, [1] = left side
  } __packed_ws__(MotionReference, 0x0C);

  struct UnknownA3Entry {
    uint8_t flags;
    uint8_t unknown_a2;
    uint16_t unknown_a3;
    uint16_t unknown_a4;
    uint16_t unknown_a5;
  };

  virtual ~MagEvolutionTable() = default;

  static std::shared_ptr<MagEvolutionTable> create(std::shared_ptr<const std::string> data, Version version);

  virtual size_t num_mags() const = 0;

  virtual size_t num_motion_entries(bool use_second_table) const = 0;
  virtual const MotionReference& get_motion_reference(bool use_second_table, size_t index) const = 0;

  virtual std::pair<uint8_t, uint8_t> get_unknown_a2(size_t index) const = 0;

  virtual size_t num_unknown_a3_entries() const = 0;
  virtual const UnknownA3Entry& get_unknown_a3(size_t index) const = 0;

  virtual uint8_t get_unknown_a4(size_t index) const = 0;

  virtual size_t num_colors() const = 0;
  virtual const VectorXYZTF& get_color_rgba(size_t index) const = 0;

  virtual uint8_t get_evolution_number(uint8_t data1_1) const = 0;

protected:
  MagEvolutionTable() = default;
};
