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
  virtual ~MagEvolutionTable() = default;

  static std::shared_ptr<MagEvolutionTable> create(std::shared_ptr<const std::string> data, Version version);

  virtual VectorXYZTF get_color_rgba(size_t index) const = 0;
  virtual uint8_t get_evolution_number(uint8_t data1_1) const = 0;

protected:
  MagEvolutionTable() = default;
};
