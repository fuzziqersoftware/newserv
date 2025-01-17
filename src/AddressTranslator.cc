#include "AddressTranslator.hh"

#include <array>
#include <future>
#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>
#include <resource_file/ExecutableFormats/DOLFile.hh>
#include <resource_file/ExecutableFormats/PEFile.hh>
#include <resource_file/ExecutableFormats/XBEFile.hh>

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
        directory(directory),
        enable_ppc(false) {
    while (phosg::ends_with(this->directory, "/")) {
      this->directory.pop_back();
    }
    for (const auto& filename : phosg::list_directory(this->directory)) {
      if (filename.size() < 4) {
        continue;
      }
      string name = filename.substr(0, filename.size() - 4);
      string path = directory + "/" + filename;

      if (phosg::ends_with(filename, ".dol")) {
        ResourceDASM::DOLFile dol(path.c_str());
        auto mem = make_shared<ResourceDASM::MemoryContext>();
        dol.load_into(mem);
        this->mems.emplace(name, mem);
        this->enable_ppc = true;
        this->log.info("Loaded %s", name.c_str());
      } else if (phosg::ends_with(filename, ".xbe")) {
        ResourceDASM::XBEFile xbe(path.c_str());
        auto mem = make_shared<ResourceDASM::MemoryContext>();
        xbe.load_into(mem);
        this->mems.emplace(name, mem);
        this->log.info("Loaded %s", name.c_str());
      } else if (phosg::ends_with(filename, ".exe")) {
        ResourceDASM::PEFile pe(path.c_str());
        auto mem = make_shared<ResourceDASM::MemoryContext>();
        pe.load_into(mem);
        this->mems.emplace(name, mem);
        this->log.info("Loaded %s", name.c_str());
      } else if (phosg::ends_with(filename, ".bin")) {
        string data = phosg::load_file(path);
        auto mem = make_shared<ResourceDASM::MemoryContext>();
        mem->allocate_at(0x8C010000, data.size());
        mem->memcpy(0x8C010000, data.data(), data.size());
        this->mems.emplace(name, mem);
        this->log.info("Loaded %s", name.c_str());
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
        fprintf(stderr, "(%s) r2 = %08" PRIX32 "\n", it.first.c_str(), r2);
      } else {
        fprintf(stderr, "(%s) r2 = __MISSING__\n", it.first.c_str());
      }
      if (r13_low_found && r13_high_found) {
        fprintf(stderr, "(%s) r13 = %08" PRIX32 "\n", it.first.c_str(), r13);
      } else {
        fprintf(stderr, "(%s) r13 = __MISSING__\n", it.first.c_str());
      }
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
    this->log.info("(find_match/%s) Source offset = %08zX with %zX/%zX bytes available before/after",
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
      this->log.info("(find_match/%s) For match length %zX, %zu matches found", method_token, match_length, num_matches);
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

  void find_all_matches(uint32_t src_addr, uint32_t src_size) const {
    if (!this->src_mem) {
      throw runtime_error("no source file selected");
    }

    map<string, uint32_t> results;
    for (const auto& it : this->mems) {
      if (it.second == this->src_mem) {
        log.info("(%s) %08" PRIX32 " (from source)", it.first.c_str(), src_addr);
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
        static const vector<ExpandMethod> raw_methods = {
            ExpandMethod::RAW_FORWARD,
            ExpandMethod::RAW_BACKWARD,
            ExpandMethod::RAW_BOTH,
        };
        const auto& methods = this->enable_ppc ? ppc_methods : raw_methods;
        for (size_t z = 0; z < methods.size(); z++) {
          futures.emplace_back(async(&AddressTranslator::find_match, this, it.second, src_addr, src_size, methods[z]));
        }

        unordered_set<uint32_t> match_addrs;
        for (size_t z = 0; z < futures.size(); z++) {
          const char* method_name = this->name_for_expand_method(methods[z]);
          try {
            uint32_t ret = futures[z].get();
            log.info("(%s) (%s) %08" PRIX32, it.first.c_str(), method_name, ret);
            match_addrs.emplace(ret);
          } catch (const exception& e) {
            log.error("(%s) (%s) failed: %s", it.first.c_str(), method_name, e.what());
          }
        }

        if (match_addrs.empty()) {
          log.error("(%s) no match found", it.first.c_str());
        } else if (match_addrs.size() > 1) {
          log.error("(%s) different matches found by different methods", it.first.c_str());
        } else {
          results.emplace(it.first, *match_addrs.begin());
        }
      }
    }
    for (const auto& it : results) {
      fprintf(stdout, "%s => %08" PRIX32 "\n", it.first.c_str(), it.second);
    }
  }

  uint32_t find_be_to_le_data_match(
      shared_ptr<const ResourceDASM::MemoryContext> dest_mem,
      uint32_t src_addr,
      uint32_t src_size) const {
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
      this->log.info("... For match length %zX, %zu matches found", match_length, num_matches);
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
        log.info("(%s) %08" PRIX32 " (from source)", it.first.c_str(), src_addr);
        results.emplace(it.first, src_addr);

      } else {
        uint32_t ret = 0;
        try {
          ret = this->find_be_to_le_data_match(it.second, src_addr, src_size);
          log.info("(%s) %08" PRIX32, it.first.c_str(), ret);
        } catch (const exception& e) {
          log.error("(%s) failed: %s", it.first.c_str(), e.what());
        }

        if (ret == 0) {
          log.error("(%s) no match found", it.first.c_str());
        } else {
          results.emplace(it.first, ret);
        }
      }
    }
    for (const auto& it : results) {
      fprintf(stdout, "%s => %08" PRIX32 "\n", it.first.c_str(), it.second);
    }
  }

  void find_data(const std::string& data) const {
    for (const auto& [name, mem] : this->mems) {
      for (const auto& [sec_addr, sec_size] : mem->allocated_blocks()) {
        uint32_t last_addr = sec_addr + sec_size - data.size();
        for (uint32_t addr = sec_addr; addr < last_addr; addr++) {
          if (!mem->memcmp(addr, data.data(), data.size())) {
            fprintf(stderr, "%s => %08" PRIX32 "\n", name.c_str(), addr);
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
    } else if (tokens[0] == "match") {
      this->find_all_matches(
          stoul(tokens.at(1), nullptr, 16),
          tokens.size() >= 3 ? stoul(tokens[2], nullptr, 16) : 0);
    } else if (tokens[0] == "match-be-le") {
      this->find_all_be_to_le_data_matches(
          stoul(tokens.at(1), nullptr, 16),
          tokens.size() >= 3 ? stoul(tokens[2], nullptr, 16) : 0);
    } else if (tokens[0] == "find-ppc-globals") {
      this->find_ppc_rtoc_global_regs();
    } else if (!tokens[0].empty()) {
      throw runtime_error("unknown command");
    }
  }

  void run_shell() {
    while (!feof(stdin)) {
      if (!this->src_filename.empty()) {
        fprintf(stdout, "addr-trans:%s/%s> ", this->directory.c_str(), this->src_filename.c_str());
      } else {
        fprintf(stdout, "addr-trans:%s> ", this->directory.c_str());
      }
      fflush(stdout);

      string command = phosg::fgets(stdin);
      try {
        this->handle_command(command);
      } catch (const exception& e) {
        this->log.error("Failed: %s", e.what());
      }
    }
    fputc('\n', stdout);
  }

private:
  phosg::PrefixedLogger log;
  string directory;
  unordered_map<string, shared_ptr<const ResourceDASM::MemoryContext>> mems;
  string src_filename;
  shared_ptr<const ResourceDASM::MemoryContext> src_mem;
  bool enable_ppc;
};

void run_address_translator(const std::string& directory, const std::string& use_filename, const std::string& command) {
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

vector<pair<uint32_t, string>> diff_dol_files(const string& a_filename, const string& b_filename) {
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

  vector<pair<uint32_t, string>> ret;
  for (uint32_t addr = min_addr; addr < max_addr; addr += 4) {
    bool a_exists = a_mem->exists(addr, 4);
    bool b_exists = b_mem->exists(addr, 4);
    if (a_exists && b_exists) {
      string a_value = a_mem->read(addr, 4);
      string b_value = b_mem->read(addr, 4);
      if (a_value != b_value) {
        if (!ret.empty() && (ret.back().first + ret.back().second.size() == addr)) {
          ret.back().second += b_value;
        } else {
          ret.emplace_back(make_pair(addr, b_value));
        }
      }
    }
  }
  return ret;
}
