#include "AddressTranslator.hh"

#include <array>
#include <filesystem>
#include <future>
#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>
#include <resource_file/Emulators/X86Emulator.hh>
#include <resource_file/ExecutableFormats/DOLFile.hh>
#include <resource_file/ExecutableFormats/PEFile.hh>
#include <resource_file/ExecutableFormats/XBEFile.hh>

#include "Map.hh"
#include "Text.hh"
#include "Types.hh"

using namespace std;

class AddressTranslator {
public:
  enum class ExpandMethod {
    PPC_TEXT_FORWARD = 0,
    PPC_TEXT_FORWARD_WITH_BARRIER,
    PPC_TEXT_BACKWARD,
    PPC_TEXT_BACKWARD_WITH_BARRIER,
    PPC_TEXT_BOTH,
    PPC_TEXT_BOTH_WITH_BARRIER,
    PPC_TEXT_BOTH_IGNORE_ORIGIN,
    PPC_DATA_FORWARD,
    PPC_DATA_BACKWARD,
    PPC_DATA_BOTH,
    RAW_FORWARD,
    RAW_BACKWARD,
    RAW_BOTH,
  };

  static const char* name_for_expand_method(ExpandMethod method) {
    switch (method) {
      case ExpandMethod::PPC_TEXT_FORWARD:
        return "PPC_TEXT_FORWARD";
      case ExpandMethod::PPC_TEXT_FORWARD_WITH_BARRIER:
        return "PPC_TEXT_FORWARD_WITH_BARRIER";
      case ExpandMethod::PPC_TEXT_BACKWARD:
        return "PPC_TEXT_BACKWARD";
      case ExpandMethod::PPC_TEXT_BACKWARD_WITH_BARRIER:
        return "PPC_TEXT_BACKWARD_WITH_BARRIER";
      case ExpandMethod::PPC_TEXT_BOTH:
        return "PPC_TEXT_BOTH";
      case ExpandMethod::PPC_TEXT_BOTH_WITH_BARRIER:
        return "PPC_TEXT_BOTH_WITH_BARRIER";
      case ExpandMethod::PPC_TEXT_BOTH_IGNORE_ORIGIN:
        return "PPC_TEXT_BOTH_IGNORE_ORIGIN";
      case ExpandMethod::PPC_DATA_FORWARD:
        return "PPC_DATA_FORWARD";
      case ExpandMethod::PPC_DATA_BACKWARD:
        return "PPC_DATA_BACKWARD";
      case ExpandMethod::PPC_DATA_BOTH:
        return "PPC_DATA_BOTH";
      case ExpandMethod::RAW_FORWARD:
        return "RAW_FORWARD";
      case ExpandMethod::RAW_BACKWARD:
        return "RAW_BACKWARD";
      case ExpandMethod::RAW_BOTH:
        return "RAW_BOTH";
      default:
        throw logic_error("invalid expand method");
    }
  }

  static bool is_ppc_expand_method(ExpandMethod method) {
    switch (method) {
      case ExpandMethod::PPC_TEXT_FORWARD:
      case ExpandMethod::PPC_TEXT_FORWARD_WITH_BARRIER:
      case ExpandMethod::PPC_TEXT_BACKWARD:
      case ExpandMethod::PPC_TEXT_BACKWARD_WITH_BARRIER:
      case ExpandMethod::PPC_TEXT_BOTH:
      case ExpandMethod::PPC_TEXT_BOTH_WITH_BARRIER:
      case ExpandMethod::PPC_TEXT_BOTH_IGNORE_ORIGIN:
      case ExpandMethod::PPC_DATA_FORWARD:
      case ExpandMethod::PPC_DATA_BACKWARD:
      case ExpandMethod::PPC_DATA_BOTH:
        return true;
      case ExpandMethod::RAW_FORWARD:
      case ExpandMethod::RAW_BACKWARD:
      case ExpandMethod::RAW_BOTH:
        return false;
      default:
        throw logic_error("invalid expand method");
    }
  }

  static bool is_ppc_data_expand_method(ExpandMethod method) {
    switch (method) {
      case ExpandMethod::PPC_DATA_FORWARD:
      case ExpandMethod::PPC_DATA_BACKWARD:
      case ExpandMethod::PPC_DATA_BOTH:
        return true;
      case ExpandMethod::PPC_TEXT_FORWARD:
      case ExpandMethod::PPC_TEXT_FORWARD_WITH_BARRIER:
      case ExpandMethod::PPC_TEXT_BACKWARD:
      case ExpandMethod::PPC_TEXT_BACKWARD_WITH_BARRIER:
      case ExpandMethod::PPC_TEXT_BOTH:
      case ExpandMethod::PPC_TEXT_BOTH_WITH_BARRIER:
      case ExpandMethod::PPC_TEXT_BOTH_IGNORE_ORIGIN:
      case ExpandMethod::RAW_FORWARD:
      case ExpandMethod::RAW_BACKWARD:
      case ExpandMethod::RAW_BOTH:
        return false;
      default:
        throw logic_error("invalid expand method");
    }
  }

  AddressTranslator(const string& directory)
      : log("[addr-trans] "),
        directory(directory) {
    while (this->directory.ends_with("/")) {
      this->directory.pop_back();
    }
    for (const auto& item : std::filesystem::directory_iterator(this->directory)) {
      string filename = item.path().filename().string();
      if (filename.size() < 4) {
        continue;
      }
      string name = filename.substr(0, filename.size() - 4);
      string path = directory + "/" + filename;

      if (filename.ends_with(".dol")) {
        ResourceDASM::DOLFile dol(path.c_str());
        auto mem = make_shared<ResourceDASM::MemoryContext>();
        dol.load_into(mem);
        this->mems.emplace(name, mem);
        this->ppc_mems.emplace(mem);
        this->log.info_f("Loaded {}", name);
      } else if (filename.ends_with(".xbe")) {
        ResourceDASM::XBEFile xbe(path.c_str());
        auto mem = make_shared<ResourceDASM::MemoryContext>();
        xbe.load_into(mem);
        this->mems.emplace(name, mem);
        this->log.info_f("Loaded {}", name);
      } else if (filename.ends_with(".exe")) {
        ResourceDASM::PEFile pe(path.c_str());
        auto mem = make_shared<ResourceDASM::MemoryContext>();
        pe.load_into(mem);
        this->mems.emplace(name, mem);
        this->log.info_f("Loaded {}", name);
      } else if (filename.ends_with(".bin")) {
        string data = phosg::load_file(path);
        auto mem = make_shared<ResourceDASM::MemoryContext>();
        mem->allocate_at(0x8C010000, data.size());
        mem->memcpy(0x8C010000, data.data(), data.size());
        this->mems.emplace(name, mem);
        this->log.info_f("Loaded {}", name);
      }
    }
  }
  ~AddressTranslator() = default;

  const string& get_source_filename() const {
    return this->src_filename;
  }
  void set_source_file(const string& filename) {
    this->src_filename = filename;
    this->src_mem = this->mems.at(this->src_filename);
  }

  void find_ppc_rtoc_global_regs() const {
    for (const auto& it : this->mems) {
      bool r2_high_found = false;
      bool r2_low_found = false;
      bool r13_high_found = false;
      bool r13_low_found = false;
      uint32_t r2 = 0;
      uint32_t r13 = 0;
      for (const auto& block : it.second->allocated_blocks()) {
        phosg::StringReader r = it.second->reader(block.first, block.second);
        while (!r.eof() && r.where()) {
          uint32_t opcode = r.get_u32b();
          if ((opcode & 0xFFFF0000) == 0x3DA00000) {
            if (r13_high_found) {
              throw runtime_error("multiple values for r13_high");
            }
            r13_high_found = true;
            r13 |= (opcode << 16);
          } else if ((opcode & 0xFFFF0000) == 0x3C400000) {
            if (r2_high_found) {
              throw runtime_error("multiple values for r2_high");
            }
            r2_high_found = true;
            r2 |= (opcode << 16);
          } else if ((opcode & 0xFFFF0000) == 0x61AD0000) {
            if (r13_low_found) {
              throw runtime_error("multiple values for r13_low");
            }
            r13_low_found = true;
            r13 |= (opcode & 0xFFFF);
          } else if ((opcode & 0xFFFF0000) == 0x60420000) {
            if (r2_low_found) {
              throw runtime_error("multiple values for r2_low");
            }
            r2_low_found = true;
            r2 |= (opcode & 0xFFFF);
          }
        }
      }
      if (r2_low_found && r2_high_found) {
        phosg::fwrite_fmt(stderr, "({}) r2 = {:08X}\n", it.first, r2);
      } else {
        phosg::fwrite_fmt(stderr, "({}) r2 = __MISSING__\n", it.first);
      }
      if (r13_low_found && r13_high_found) {
        phosg::fwrite_fmt(stderr, "({}) r13 = {:08X}\n", it.first, r13);
      } else {
        phosg::fwrite_fmt(stderr, "({}) r13 = __MISSING__\n", it.first);
      }
    }
  }

  struct ParseDATConstructorTableSpec {
    string src_name;
    uint32_t index_addr;
    size_t num_areas;
    bool has_names;
    vector<uint32_t> x86_constructor_calls;

    ParseDATConstructorTableSpec(const phosg::JSON& json) {
      this->src_name = json.at("SourceName").as_string();
      this->index_addr = json.at("IndexAddress").as_int();
      this->num_areas = json.at("AreaCount").as_int();
      this->has_names = json.at("HasNames").as_bool();
      for (const auto& z : json.at("X86ConstructorCalls").as_list()) {
        this->x86_constructor_calls.emplace_back(z->as_int());
      }
    }

    static vector<ParseDATConstructorTableSpec> from_json_list(const phosg::JSON& json) {
      vector<ParseDATConstructorTableSpec> ret;
      for (const auto& z : json.as_list()) {
        ret.emplace_back(*z);
      }
      return ret;
    }
  };

  template <bool BE>
  struct DATConstructorTableEntry {
    static constexpr bool IsBE = BE;

    U16T<BE> type;
    U16T<BE> unused;
    U32T<BE> constructor_addr;
    F32T<BE> max_dist2; // Only applies for objects
    U32T<BE> default_num_children;
  } __attribute__((packed));

  template <bool BE>
  struct DATConstructorTableEntryWithName {
    static constexpr bool IsBE = BE;

    pstring<TextEncoding::ASCII, 0x10> debug_name;
    U16T<BE> type;
    U16T<BE> unused;
    U32T<BE> constructor_addr;
    F32T<BE> max_dist2; // Only applies for objects
    U32T<BE> default_num_children;
  } __attribute__((packed));

  // Returns {type: {constructor_addr: [(start_area, end_area), ...]}}
  template <typename EntryT>
  map<uint32_t, map<uint32_t, vector<pair<size_t, size_t>>>> parse_dat_constructor_table_t(
      shared_ptr<const ResourceDASM::MemoryContext>& mem, const ParseDATConstructorTableSpec& spec) {
    if (!mem) {
      throw runtime_error("no file selected");
    }

    // On some of the x86 builds of the game (PCv2 and Xbox), the constructor tables aren't entirely static in the data
    // sections - some parts are written during static initialization instead. To handle this, we make a copy of the
    // immutable MemoryContext and run the static initialization functions using resource_dasm's emulator before
    // parsing the constructor table.
    shared_ptr<const ResourceDASM::MemoryContext> effective_mem = mem;
    if (!spec.x86_constructor_calls.empty()) {
      auto constructed_mem = make_shared<ResourceDASM::MemoryContext>(mem->duplicate());
      uint32_t esp = constructed_mem->allocate(0x1000) + 0x1000;
      for (uint32_t constructor_addr : spec.x86_constructor_calls) {
        ResourceDASM::X86Emulator emu(constructed_mem);

        // Uncomment for debugging
        // auto debugger = make_shared<ResourceDASM::EmulatorDebugger<ResourceDASM::X86Emulator>>();
        // debugger->bind(emu);
        // debugger->state.mode = ResourceDASM::DebuggerMode::TRACE;

        auto& regs = emu.registers();
        regs.eip = constructor_addr;
        regs.esp().u = esp - 4;
        constructed_mem->write_u32l(esp - 4, 0xFFFFFFFF); // Return addr
        try {
          emu.execute();
        } catch (const out_of_range&) {
          if (regs.eip != 0xFFFFFFFF) {
            throw;
          }
        }
      }
      effective_mem = constructed_mem;
    }

    map<uint32_t, map<uint32_t, vector<pair<size_t, size_t>>>> table;

    auto index_r = effective_mem->reader(spec.index_addr, spec.num_areas * sizeof(uint32_t));
    for (size_t area = 0; area < spec.num_areas; area++) {
      uint32_t entries_addr = EntryT::IsBE ? index_r.get_u32b() : index_r.get_u32l();
      if (!entries_addr) {
        continue;
      }
      auto entries_r = effective_mem->reader(entries_addr, 0x4000); // 0x4000 is probably enough
      while (!entries_r.eof()) {
        const auto& entry = entries_r.get<EntryT>();
        if (entry.type == 0xFFFF) {
          break;
        }
        auto& group = table[entry.type][entry.constructor_addr];
        if (!group.empty() && (group.back().second == (area - 1))) {
          group.back().second = area;
        } else {
          group.emplace_back(make_pair(area, area));
        }
      }
      if (entries_r.eof()) {
        throw runtime_error("did not find end-of-entries marker");
      }
    }

    return table;
  }

  static uint64_t area_mask_for_ranges(const vector<pair<size_t, size_t>>& ranges) {
    uint64_t ret = 0;
    for (const auto& [start, end] : ranges) {
      for (size_t z = start; z <= end; z++) {
        ret |= static_cast<uint64_t>(1ULL << z);
      }
    }
    return ret;
  }

  void parse_dat_constructor_table(const ParseDATConstructorTableSpec& spec) {
    map<uint32_t, map<uint32_t, vector<pair<size_t, size_t>>>> table;
    auto spec_mem = this->mems.at(spec.src_name);
    if (this->ppc_mems.count(spec_mem)) {
      table = this->parse_dat_constructor_table_t<DATConstructorTableEntry<true>>(spec_mem, spec);
    } else if (!spec.has_names) {
      table = this->parse_dat_constructor_table_t<DATConstructorTableEntry<false>>(spec_mem, spec);
    } else {
      table = this->parse_dat_constructor_table_t<DATConstructorTableEntryWithName<false>>(spec_mem, spec);
    }

    for (const auto& [type, constructor_to_area_ranges] : table) {
      phosg::fwrite_fmt(stdout, "{:04X} =>", type);
      for (const auto& [constructor, area_ranges] : constructor_to_area_ranges) {
        phosg::fwrite_fmt(stdout, " {:08X}", constructor);
        bool is_first = true;
        for (const auto& [start, end] : area_ranges) {
          fputc(is_first ? ':' : ',', stdout);
          if (start == end) {
            phosg::fwrite_fmt(stdout, "{:02X}", start);
          } else {
            phosg::fwrite_fmt(stdout, "{:02X}-{:02X}", start, end);
          }
          is_first = false;
        }
      }
      fputc('\n', stdout);
    }
  }

  void parse_dat_constructor_table_multi(
      const vector<ParseDATConstructorTableSpec>& specs, bool is_enemies, bool print_area_masks) {
    map<string, map<uint32_t, map<uint32_t, vector<pair<size_t, size_t>>>>> all_tables;
    for (const auto& spec : specs) {
      map<uint32_t, map<uint32_t, vector<pair<size_t, size_t>>>> table;
      auto spec_mem = this->mems.at(spec.src_name);
      if (this->ppc_mems.count(spec_mem)) {
        table = this->parse_dat_constructor_table_t<DATConstructorTableEntry<true>>(spec_mem, spec);
      } else if (!spec.has_names) {
        table = this->parse_dat_constructor_table_t<DATConstructorTableEntry<false>>(spec_mem, spec);
      } else {
        table = this->parse_dat_constructor_table_t<DATConstructorTableEntryWithName<false>>(spec_mem, spec);
      }
      all_tables.emplace(spec.src_name, std::move(table));
    }

    map<string, size_t> version_widths;
    map<uint32_t, map<string, string>> formatted_cells_for_type;
    for (const auto& spec : specs) {
      const auto& table = all_tables.at(spec.src_name);
      size_t max_width = 0;

      for (const auto& [type, constructor_to_area_ranges] : table) {
        string cell_data;
        for (const auto& [constructor, area_ranges] : constructor_to_area_ranges) {
          if (!cell_data.empty()) {
            cell_data.push_back(' ');
          }
          cell_data += std::format("{:08X}", constructor);
          if (print_area_masks) {
            cell_data += std::format(":{:016X}", this->area_mask_for_ranges(area_ranges));
          } else {
            bool is_first = true;
            for (const auto& [start, end] : area_ranges) {
              cell_data.push_back(is_first ? ':' : ',');
              if (start == end) {
                cell_data += std::format("{:02X}", start);
              } else {
                cell_data += std::format("{:02X}-{:02X}", start, end);
              }
              is_first = false;
            }
          }
        }
        max_width = max<size_t>(max_width, cell_data.size());
        formatted_cells_for_type[type][spec.src_name] = std::move(cell_data);
      }
      version_widths[spec.src_name] = max_width;
    }

    vector<string> formatted_lines;
    string header_line = "TYPE =>";
    for (const auto& spec : specs) {
      size_t width = version_widths.at(spec.src_name);
      header_line.push_back(' ');
      header_line += spec.src_name;
      if (width > spec.src_name.size()) {
        header_line.resize(header_line.size() + (width - spec.src_name.size()), '-');
      }
    }
    header_line += " NAME";

    for (const auto& [type, formatted_cells] : formatted_cells_for_type) {
      string line = std::format("{:04X} =>", type);
      for (const auto& spec : specs) {
        size_t width = version_widths.at(spec.src_name);
        try {
          const auto& cell_data = formatted_cells.at(spec.src_name);
          line.push_back(' ');
          line += cell_data;
          if (width > cell_data.size()) {
            line.resize(line.size() + (width - cell_data.size()), ' ');
          }
        } catch (const out_of_range&) {
          line.resize(line.size() + (width + 1), ' ');
        }
      }
      line.push_back(' ');
      line += is_enemies ? MapFile::name_for_enemy_type(type) : MapFile::name_for_object_type(type);

      if ((formatted_lines.size() % 40) == 0) {
        formatted_lines.emplace_back(header_line);
      }
      formatted_lines.emplace_back(std::move(line));
    }

    for (auto& line : formatted_lines) {
      phosg::strip_trailing_whitespace(line);
      phosg::fwrite_fmt(stdout, "{}\n", line);
    }
  }

  uint32_t find_match(
      shared_ptr<const ResourceDASM::MemoryContext> dest_mem,
      uint32_t src_addr,
      uint32_t src_size,
      ExpandMethod expand_method) const {
    bool is_ppc = this->is_ppc_expand_method(expand_method);
    bool is_ppc_data = this->is_ppc_data_expand_method(expand_method);
    if (!this->src_mem) {
      throw runtime_error("no source file selected");
    }

    if (src_size == 0) {
      src_size = is_ppc ? 4 : 1;
    }

    pair<uint32_t, uint32_t> src_section = make_pair(0, 0);
    for (const auto& sec : this->src_mem->allocated_blocks()) {
      if (src_addr >= sec.first && src_addr + src_size <= sec.first + sec.second) {
        src_section = sec;
        break;
      }
    }
    if (!src_section.second) {
      throw runtime_error("source address not within any section");
    }

    const char* method_token = this->name_for_expand_method(expand_method);

    size_t src_offset = src_addr - src_section.first;
    size_t src_bytes_available_before = src_offset;
    size_t src_bytes_available_after = src_section.second - src_offset - 4;
    this->log.info_f("(find_match/{}) Source offset = {:08X} with {:X}/{:X} bytes available before/after",
        method_token, src_offset, src_bytes_available_before, src_bytes_available_after);

    size_t match_bytes_before = 0;
    size_t match_bytes_after = 0;
    while (match_bytes_before + match_bytes_after + 4 < 0x100) {
      size_t num_matches = 0;
      size_t last_match_address = 0;
      size_t match_length = match_bytes_before + match_bytes_after + 4;
      phosg::StringReader src_r = this->src_mem->reader(src_section.first + src_offset - match_bytes_before, match_length);
      for (const auto& dest_section : dest_mem->allocated_blocks()) {
        for (size_t dest_match_offset = 0;
            dest_match_offset + match_length < dest_section.second;
            dest_match_offset += (is_ppc ? 4 : 1)) {
          src_r.go(0);
          phosg::StringReader dest_r = dest_mem->reader(dest_section.first + dest_match_offset, match_length);
          size_t z;
          if (is_ppc) {
            for (z = 0; z < match_length; z += 4) {
              if ((expand_method == ExpandMethod::PPC_TEXT_BOTH_IGNORE_ORIGIN) && (z == match_bytes_before)) {
                src_r.skip(4);
                dest_r.skip(4);
              } else if (!is_ppc_data) {
                uint32_t src_opcode = src_r.get_u32b();
                uint32_t dest_opcode = dest_r.get_u32b();
                uint32_t src_class = src_opcode & 0xFC000000;
                if (src_class != (dest_opcode & 0xFC000000)) {
                  break;
                }
                if (src_class == 0x48000000) {
                  // b +-offset
                  src_opcode &= 0xFC000003;
                  dest_opcode &= 0xFC000003;
                } else if (((src_opcode & 0xAC1F0000) == 0x800D0000) || ((src_opcode & 0xAC1F0000) == 0x80020000)) {
                  // lwz/lfs rXX/fXX, [r2/r13 +- offset] OR stw/stfs [r2/r13 +- offset], rXX/fXX
                  src_opcode &= 0xFFFF0000;
                  dest_opcode &= 0xFFFF0000;
                }
                if (src_opcode != dest_opcode) {
                  break;
                }
              } else {
                uint32_t src_data = src_r.get_u32b();
                uint32_t dest_data = dest_r.get_u32b();
                if ((src_data & 0xFE000000) == 0x80000000) {
                  src_data &= 0xFE000003;
                }
                if ((dest_data & 0xFE000000) == 0x80000000) {
                  dest_data &= 0xFE000003;
                }
                if (src_data != dest_data) {
                  break;
                }
              }
            }
          } else {
            for (z = 0; z < match_length; z++) {
              uint8_t src_data = src_r.get_u8();
              uint8_t dest_data = dest_r.get_u8();
              if (src_data != dest_data) {
                break;
              }
            }
          }
          if (z == match_length) {
            num_matches++;
            last_match_address = dest_section.first + dest_match_offset + match_bytes_before;
          }
        }
      }
      this->log.info_f("(find_match/{}) For match length {:X}, {} matches found", method_token, match_length, num_matches);
      if (num_matches == 1) {
        return last_match_address;
      } else if (num_matches == 0) {
        throw runtime_error("did not find exactly one match");
      }
      bool can_expand_backward = false;
      bool can_expand_forward = false;
      switch (expand_method) {
        case ExpandMethod::PPC_TEXT_BACKWARD_WITH_BARRIER:
          can_expand_backward = (src_r.pget_u32b(0) != 0x4E800020) &&
              (src_bytes_available_before >= match_bytes_before + 4);
          break;
        case ExpandMethod::PPC_TEXT_BACKWARD:
        case ExpandMethod::PPC_DATA_BACKWARD:
          can_expand_backward = (src_bytes_available_before >= match_bytes_before + 4);
          break;
        case ExpandMethod::PPC_TEXT_FORWARD_WITH_BARRIER:
          can_expand_forward = (src_r.pget_u32b(src_r.size() - 4) != 0x4E800020) &&
              (src_bytes_available_after >= match_bytes_after + 4);
          break;
        case ExpandMethod::PPC_TEXT_FORWARD:
        case ExpandMethod::PPC_DATA_FORWARD:
          can_expand_forward = (src_bytes_available_after >= match_bytes_after + 4);
          break;
        case ExpandMethod::PPC_TEXT_BOTH_WITH_BARRIER:
        case ExpandMethod::PPC_TEXT_BOTH_IGNORE_ORIGIN:
          can_expand_backward = (src_r.pget_u32b(0) != 0x4E800020) &&
              (src_bytes_available_before >= match_bytes_before + 4);
          can_expand_forward = (src_r.pget_u32b(src_r.size() - 4) != 0x4E800020) &&
              (src_bytes_available_after >= match_bytes_after + 4);
          break;
        case ExpandMethod::PPC_TEXT_BOTH:
        case ExpandMethod::PPC_DATA_BOTH:
          can_expand_backward = (src_bytes_available_before >= match_bytes_before + 4);
          can_expand_forward = (src_bytes_available_after >= match_bytes_after + 4);
          break;
        case ExpandMethod::RAW_BACKWARD:
          can_expand_backward = (src_bytes_available_before > match_bytes_before);
          break;
        case ExpandMethod::RAW_FORWARD:
          can_expand_forward = (src_bytes_available_after > match_bytes_after);
          break;
        case ExpandMethod::RAW_BOTH:
          can_expand_backward = (src_bytes_available_before > match_bytes_before);
          can_expand_forward = (src_bytes_available_after > match_bytes_after);
          break;
        default:
          throw logic_error("invalid expand method");
      }
      if (!can_expand_backward && !can_expand_forward) {
        throw runtime_error("no further expansion is allowed");
      }
      if (can_expand_backward) {
        match_bytes_before += (is_ppc ? 4 : 1);
      }
      if (can_expand_forward) {
        match_bytes_after += (is_ppc ? 4 : 1);
      }
    }
    throw runtime_error("scan field too long; too many matches");
  }

  enum class MatchType {
    ANY = 0,
    TEXT,
    DATA,
  };

  void find_all_matches(uint32_t src_addr, uint32_t src_size, MatchType type) const {
    if (!this->src_mem) {
      throw runtime_error("no source file selected");
    }

    map<string, uint32_t> results;
    for (const auto& it : this->mems) {
      if (it.second == this->src_mem) {
        log.info_f("({}) {:08X} (from source)", it.first, src_addr);
        results.emplace(it.first, src_addr);

      } else {
        vector<future<uint32_t>> futures;
        static const vector<ExpandMethod> ppc_methods = {
            ExpandMethod::PPC_TEXT_FORWARD,
            ExpandMethod::PPC_TEXT_FORWARD_WITH_BARRIER,
            ExpandMethod::PPC_TEXT_BACKWARD,
            ExpandMethod::PPC_TEXT_BACKWARD_WITH_BARRIER,
            ExpandMethod::PPC_TEXT_BOTH,
            ExpandMethod::PPC_TEXT_BOTH_WITH_BARRIER,
            ExpandMethod::PPC_TEXT_BOTH_IGNORE_ORIGIN,
            ExpandMethod::PPC_DATA_FORWARD,
            ExpandMethod::PPC_DATA_BACKWARD,
            ExpandMethod::PPC_DATA_BOTH,
        };
        static const vector<ExpandMethod> ppc_text_methods = {
            ExpandMethod::PPC_TEXT_FORWARD,
            ExpandMethod::PPC_TEXT_FORWARD_WITH_BARRIER,
            ExpandMethod::PPC_TEXT_BACKWARD,
            ExpandMethod::PPC_TEXT_BACKWARD_WITH_BARRIER,
            ExpandMethod::PPC_TEXT_BOTH,
            ExpandMethod::PPC_TEXT_BOTH_WITH_BARRIER,
            ExpandMethod::PPC_TEXT_BOTH_IGNORE_ORIGIN,
        };
        static const vector<ExpandMethod> ppc_data_methods = {
            ExpandMethod::PPC_DATA_FORWARD,
            ExpandMethod::PPC_DATA_BACKWARD,
            ExpandMethod::PPC_DATA_BOTH,
        };
        static const vector<ExpandMethod> raw_methods = {
            ExpandMethod::RAW_FORWARD,
            ExpandMethod::RAW_BACKWARD,
            ExpandMethod::RAW_BOTH,
        };

        const vector<ExpandMethod>* methods;
        if (this->ppc_mems.count(it.second)) {
          if (type == MatchType::ANY) {
            methods = &ppc_methods;
          } else if (type == MatchType::TEXT) {
            methods = &ppc_text_methods;
          } else if (type == MatchType::DATA) {
            methods = &ppc_data_methods;
          } else {
            throw logic_error("invalid match type");
          }
        } else {
          methods = &raw_methods;
        }

        for (size_t z = 0; z < methods->size(); z++) {
          futures.emplace_back(async(&AddressTranslator::find_match, this, it.second, src_addr, src_size, methods->at(z)));
        }

        unordered_set<uint32_t> match_addrs;
        for (size_t z = 0; z < futures.size(); z++) {
          const char* method_name = this->name_for_expand_method(methods->at(z));
          try {
            uint32_t ret = futures[z].get();
            log.info_f("({}) ({}) {:08X}", it.first, method_name, ret);
            match_addrs.emplace(ret);
          } catch (const exception& e) {
            log.error_f("({}) ({}) failed: {}", it.first, method_name, e.what());
          }
        }

        if (match_addrs.empty()) {
          log.error_f("({}) no match found", it.first);
        } else if (match_addrs.size() > 1) {
          log.error_f("({}) different matches found by different methods", it.first);
        } else {
          results.emplace(it.first, *match_addrs.begin());
        }
      }
    }
    for (const auto& it : results) {
      phosg::fwrite_fmt(stdout, "{} => {:08X}\n", it.first, it.second);
    }
  }

  uint32_t find_be_to_le_data_match(
      shared_ptr<const ResourceDASM::MemoryContext> dest_mem, uint32_t src_addr, uint32_t src_size) const {
    if (src_size == 0) {
      src_size = 4;
    }

    pair<uint32_t, uint32_t> src_section = make_pair(0, 0);
    for (const auto& sec : this->src_mem->allocated_blocks()) {
      if (src_addr >= sec.first && src_addr + src_size <= sec.first + sec.second) {
        src_section = sec;
        break;
      }
    }
    if (!src_section.second) {
      throw runtime_error("source address not within any section");
    }

    size_t src_offset = src_addr - src_section.first;
    size_t src_bytes_available_before = src_offset;
    size_t src_bytes_available_after = src_section.second - src_offset - 4;

    size_t match_bytes_before = 0;
    size_t match_bytes_after = 0;
    while (match_bytes_before + match_bytes_after + 4 < 0x100) {
      size_t num_matches = 0;
      size_t last_match_address = 0;
      size_t match_length = match_bytes_before + match_bytes_after + 4;
      uint32_t src_addr = src_section.first + src_offset - match_bytes_before;
      phosg::StringReader src_r = this->src_mem->reader(src_addr, match_length);
      for (const auto& dest_section : dest_mem->allocated_blocks()) {
        for (size_t dest_match_offset = 0;
            dest_match_offset + match_length < dest_section.second;
            dest_match_offset += 4) {
          src_r.go(0);
          phosg::StringReader dest_r = dest_mem->reader(dest_section.first + dest_match_offset, match_length);
          size_t z;
          for (z = 0; z < match_length; z += 4) {
            uint32_t src_v = src_r.get_u32b();
            uint32_t dest_v = dest_r.get_u32l();
            bool src_is_addr = ((src_v & 0xFE000003) == 0x80000000);
            bool dest_is_addr = ((dest_v >= 0x00010000) && (dest_v <= 0x00800000));
            if (src_is_addr != dest_is_addr) {
              break;
            } else if (src_v != dest_v) {
              break;
            }
          }
          if (z == match_length) {
            num_matches++;
            last_match_address = dest_section.first + dest_match_offset + match_bytes_before;
          }
        }
      }
      this->log.info_f("... For match length {:X}, {} matches found", match_length, num_matches);
      if (num_matches == 1) {
        return last_match_address;
      } else if (num_matches == 0) {
        throw runtime_error("did not find exactly one match");
      }
      bool can_expand_backward = (src_bytes_available_before >= match_bytes_before + 4);
      bool can_expand_forward = (src_bytes_available_after >= match_bytes_after + 4);
      if (!can_expand_backward && !can_expand_forward) {
        throw runtime_error("no further expansion is allowed");
      }
      if (can_expand_backward) {
        match_bytes_before += 4;
      }
      if (can_expand_forward) {
        match_bytes_after += 4;
      }
    }
    throw runtime_error("scan field too long; too many matches");
  }

  void find_all_be_to_le_data_matches(uint32_t src_addr, uint32_t src_size) const {
    if (!this->src_mem) {
      throw runtime_error("no source file selected");
    }

    map<string, uint32_t> results;
    for (const auto& it : this->mems) {
      if (it.second == this->src_mem) {
        log.info_f("({}) {:08X} (from source)", it.first, src_addr);
        results.emplace(it.first, src_addr);

      } else {
        uint32_t ret = 0;
        try {
          ret = this->find_be_to_le_data_match(it.second, src_addr, src_size);
          log.info_f("({}) {:08X}", it.first, ret);
        } catch (const exception& e) {
          log.error_f("({}) failed: {}", it.first, e.what());
        }

        if (ret == 0) {
          log.error_f("({}) no match found", it.first);
        } else {
          results.emplace(it.first, ret);
        }
      }
    }
    for (const auto& it : results) {
      phosg::fwrite_fmt(stdout, "{} => {:08X}\n", it.first, it.second);
    }
  }

  void find_data(const string& data) const {
    for (const auto& [name, mem] : this->mems) {
      for (const auto& [sec_addr, sec_size] : mem->allocated_blocks()) {
        uint32_t last_addr = sec_addr + sec_size - data.size();
        for (uint32_t addr = sec_addr; addr < last_addr; addr++) {
          if (!mem->memcmp(addr, data.data(), data.size())) {
            phosg::fwrite_fmt(stderr, "{} => {:08X}\n", name, addr);
          }
        }
      }
    }
  }

  void handle_command(const string& command) {
    auto tokens = phosg::split(command, ' ');
    if (tokens.empty()) {
      throw runtime_error("no command given");
    }
    phosg::strip_trailing_whitespace(tokens[tokens.size() - 1]);

    if (tokens[0] == "use") {
      this->set_source_file(tokens.at(1));
    } else if (tokens[0] == "find") {
      this->find_data(phosg::parse_data_string(tokens.at(1)));
    } else if (tokens[0] == "only") {
      unordered_set<string> to_keep{tokens.begin() + 1, tokens.end()};
      for (auto it = this->mems.begin(); it != this->mems.end();) {
        if (to_keep.count(it->first)) {
          it++;
        } else {
          it = this->mems.erase(it);
        }
      }
    } else if (tokens[0] == "match") {
      this->find_all_matches(
          stoul(tokens.at(1), nullptr, 16),
          tokens.size() >= 3 ? stoul(tokens[2], nullptr, 16) : 0,
          MatchType::ANY);
    } else if (tokens[0] == "match-text") {
      this->find_all_matches(
          stoul(tokens.at(1), nullptr, 16),
          tokens.size() >= 3 ? stoul(tokens[2], nullptr, 16) : 0,
          MatchType::TEXT);
    } else if (tokens[0] == "match-data") {
      this->find_all_matches(
          stoul(tokens.at(1), nullptr, 16),
          tokens.size() >= 3 ? stoul(tokens[2], nullptr, 16) : 0,
          MatchType::DATA);
    } else if (tokens[0] == "match-be-le") {
      this->find_all_be_to_le_data_matches(
          stoul(tokens.at(1), nullptr, 16),
          tokens.size() >= 3 ? stoul(tokens[2], nullptr, 16) : 0);
    } else if (tokens[0] == "find-ppc-globals") {
      this->find_ppc_rtoc_global_regs();
    } else if ((tokens[0] == "parse-dat-object-constructor-tables") ||
        (tokens[0] == "parse-dat-enemy-constructor-tables")) {
      bool is_enemies = (tokens[0] == "parse-dat-enemy-constructor-tables");
      auto specs = ParseDATConstructorTableSpec::from_json_list(phosg::JSON::parse(phosg::load_file(tokens.at(1))));
      this->parse_dat_constructor_table_multi(specs, is_enemies, true);
    } else if (!tokens[0].empty()) {
      throw runtime_error("unknown command");
    }
  }

  void run_shell() {
    while (!feof(stdin)) {
      if (!this->src_filename.empty()) {
        phosg::fwrite_fmt(stdout, "addr-trans:{}/{}> ", this->directory, this->src_filename);
      } else {
        phosg::fwrite_fmt(stdout, "addr-trans:{}> ", this->directory);
      }
      fflush(stdout);

      string command = phosg::fgets(stdin);
      try {
        this->handle_command(command);
      } catch (const exception& e) {
        this->log.error_f("Failed: {}", e.what());
      }
    }
    fputc('\n', stdout);
  }

private:
  phosg::PrefixedLogger log;
  string directory;
  unordered_map<string, shared_ptr<const ResourceDASM::MemoryContext>> mems;
  unordered_set<shared_ptr<const ResourceDASM::MemoryContext>> ppc_mems;
  string src_filename;
  shared_ptr<const ResourceDASM::MemoryContext> src_mem;
};

void run_address_translator(const string& directory, const string& use_filename, const string& command) {
  AddressTranslator trans(directory);
  if (!use_filename.empty()) {
    trans.set_source_file(use_filename);
  }

  if (!command.empty()) {
    trans.handle_command(command);
  } else {
    trans.run_shell();
  }
}

vector<DiffEntry> diff_dol_files(const string& a_filename, const string& b_filename) {
  ResourceDASM::DOLFile a(a_filename.c_str());
  ResourceDASM::DOLFile b(b_filename.c_str());
  auto a_mem = make_shared<ResourceDASM::MemoryContext>();
  auto b_mem = make_shared<ResourceDASM::MemoryContext>();
  a.load_into(a_mem);
  b.load_into(b_mem);

  uint32_t min_addr = 0xFFFFFFFF;
  uint32_t max_addr = 0x00000000;
  for (const auto& sec : a.sections) {
    min_addr = min<uint32_t>(min_addr, sec.address);
    max_addr = max<uint32_t>(max_addr, sec.address + sec.data.size());
  }
  for (const auto& sec : b.sections) {
    min_addr = min<uint32_t>(min_addr, sec.address);
    max_addr = max<uint32_t>(max_addr, sec.address + sec.data.size());
  }

  vector<DiffEntry> ret;
  for (uint32_t addr = min_addr; addr < max_addr; addr += 4) {
    bool a_exists = a_mem->exists(addr, 4);
    bool b_exists = b_mem->exists(addr, 4);
    if (a_exists && b_exists) {
      string a_value = a_mem->read(addr, 4);
      string b_value = b_mem->read(addr, 4);
      if (a_value != b_value) {
        if (!ret.empty() && (ret.back().address + ret.back().b_data.size() == addr)) {
          ret.back().a_data += a_value;
          ret.back().b_data += b_value;
        } else {
          ret.emplace_back(DiffEntry{.address = addr, .a_data = a_value, .b_data = b_value});
        }
      }
    }
  }
  return ret;
}

vector<DiffEntry> diff_xbe_files(const string& a_filename, const string& b_filename) {
  ResourceDASM::XBEFile a(a_filename.c_str());
  ResourceDASM::XBEFile b(b_filename.c_str());
  auto a_mem = make_shared<ResourceDASM::MemoryContext>();
  auto b_mem = make_shared<ResourceDASM::MemoryContext>();
  a.load_into(a_mem);
  b.load_into(b_mem);

  uint32_t min_addr = 0xFFFFFFFF;
  uint32_t max_addr = 0x00000000;
  for (const auto& sec : a.sections) {
    min_addr = min<uint32_t>(min_addr, sec.addr);
    max_addr = max<uint32_t>(max_addr, sec.addr + sec.size);
  }
  for (const auto& sec : b.sections) {
    min_addr = min<uint32_t>(min_addr, sec.addr);
    max_addr = max<uint32_t>(max_addr, sec.addr + sec.size);
  }

  vector<DiffEntry> ret;
  for (uint32_t addr = min_addr; addr < max_addr; addr++) {
    bool a_exists = a_mem->exists(addr, 1);
    bool b_exists = b_mem->exists(addr, 1);
    if (a_exists && b_exists) {
      uint8_t a_value = a_mem->read_u8(addr);
      uint8_t b_value = b_mem->read_u8(addr);
      if (a_value != b_value) {
        if (!ret.empty() && (ret.back().address + ret.back().b_data.size() == addr)) {
          auto& entry = ret.back();
          entry.a_data.push_back(a_value);
          entry.b_data.push_back(b_value);
        } else {
          auto& entry = ret.emplace_back();
          entry.address = addr;
          entry.a_data.push_back(a_value);
          entry.b_data.push_back(b_value);
        }
      }
    }
  }
  return ret;
}
