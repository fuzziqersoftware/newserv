#include "ARCodeTranslator.hh"

#include <future>
#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>
#include <resource_file/ExecutableFormats/DOLFile.hh>

using namespace std;

class ARCodeTranslator {
public:
  enum class ExpandMethod {
    FORWARD = 0,
    FORWARD_WITH_BARRIER,
    BACKWARD,
    BACKWARD_WITH_BARRIER,
    BOTH,
    BOTH_WITH_BARRIER,
    BOTH_IGNORE_ORIGIN,
  };

  static const char* name_for_expand_method(ExpandMethod method) {
    switch (method) {
      case ExpandMethod::FORWARD:
        return "FORWARD";
      case ExpandMethod::FORWARD_WITH_BARRIER:
        return "FORWARD_WITH_BARRIER";
      case ExpandMethod::BACKWARD:
        return "BACKWARD";
      case ExpandMethod::BACKWARD_WITH_BARRIER:
        return "BACKWARD_WITH_BARRIER";
      case ExpandMethod::BOTH:
        return "BOTH";
      case ExpandMethod::BOTH_WITH_BARRIER:
        return "BOTH_WITH_BARRIER";
      case ExpandMethod::BOTH_IGNORE_ORIGIN:
        return "BOTH_IGNORE_ORIGIN";
      default:
        throw logic_error("invalid expand method");
    }
  }

  ARCodeTranslator(const string& directory)
      : log("[ar-trans] "),
        directory(directory) {
    while (ends_with(this->directory, "/")) {
      this->directory.pop_back();
    }
    for (const auto& filename : list_directory(this->directory)) {
      if (ends_with(filename, ".dol")) {
        string name = filename.substr(0, filename.size() - 4);
        string path = directory + "/" + filename;
        this->files.emplace(name, make_shared<DOLFile>(path.c_str()));
        this->log.info("Loaded %s", name.c_str());
      }
    }
  }
  ~ARCodeTranslator() = default;

  const string& get_source_filename() const {
    return this->src_filename;
  }
  void set_source_file(const string& filename) {
    this->src_filename = filename;
    this->src_file = files.at(this->src_filename);
  }

  void find_rtoc_global_regs() const {
    for (const auto& it : files) {
      bool r2_high_found = false;
      bool r2_low_found = false;
      bool r13_high_found = false;
      bool r13_low_found = false;
      uint32_t r2 = 0;
      uint32_t r13 = 0;
      for (const auto& section : it.second->sections) {
        if (!section.is_text) {
          continue;
        }
        StringReader r(section.data);
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

  uint32_t find_match(shared_ptr<const DOLFile> dest_file, uint32_t src_address, ExpandMethod expand_method) const {
    if (!this->src_file) {
      throw runtime_error("no source file selected");
    }

    const DOLFile::Section* src_section = nullptr;
    for (const auto& sec : this->src_file->sections) {
      if (src_address >= sec.address && src_address < sec.address + sec.data.size()) {
        src_section = &sec;
        break;
      }
    }
    if (!src_section) {
      throw runtime_error("source address not within any section");
    }

    const char* method_token = this->name_for_expand_method(expand_method);

    size_t src_offset = src_address - src_section->address;
    size_t src_bytes_available_before = src_offset;
    size_t src_bytes_available_after = src_section->data.size() - src_offset - 4;
    this->log.info("(find_match/%s) Source offset = %08zX with %zX/%zX bytes available before/after",
        method_token, src_offset, src_bytes_available_before, src_bytes_available_after);

    size_t match_bytes_before = 0;
    size_t match_bytes_after = 0;
    while (match_bytes_before + match_bytes_after + 4 < 0x100) {
      size_t num_matches = 0;
      size_t last_match_address = 0;
      size_t match_length = match_bytes_before + match_bytes_after + 4;
      StringReader src_r(src_section->data.data() + src_offset - match_bytes_before, match_length);
      for (const auto& dest_section : dest_file->sections) {
        for (size_t dest_match_offset = 0;
             dest_match_offset < dest_section.data.size();
             dest_match_offset += 4) {
          src_r.go(0);
          StringReader dest_r(dest_section.data.data() + dest_match_offset, match_length);
          size_t z;
          for (z = 0; z < match_length; z += 4) {
            if (expand_method == ExpandMethod::BOTH_IGNORE_ORIGIN && z == match_bytes_before) {
              src_r.skip(4);
              dest_r.skip(4);
            } else if (src_section->is_text) {
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
          if (z == match_length) {
            num_matches++;
            last_match_address = dest_section.address + dest_match_offset + match_bytes_before;
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
        case ExpandMethod::BACKWARD_WITH_BARRIER:
          can_expand_backward = (src_r.pget_u32b(0) != 0x4E800020) &&
              (src_bytes_available_before >= match_bytes_before + 4);
          break;
        case ExpandMethod::BACKWARD:
          can_expand_backward = (src_bytes_available_before >= match_bytes_before + 4);
          break;
        case ExpandMethod::FORWARD_WITH_BARRIER:
          can_expand_forward = (src_r.pget_u32b(src_r.size() - 4) != 0x4E800020) &&
              (src_bytes_available_after >= match_bytes_after + 4);
          break;
        case ExpandMethod::FORWARD:
          can_expand_forward = (src_bytes_available_after >= match_bytes_after + 4);
          break;
        case ExpandMethod::BOTH_WITH_BARRIER:
        case ExpandMethod::BOTH_IGNORE_ORIGIN:
          can_expand_backward = (src_r.pget_u32b(0) != 0x4E800020) &&
              (src_bytes_available_before >= match_bytes_before + 4);
          can_expand_forward = (src_r.pget_u32b(src_r.size() - 4) != 0x4E800020) &&
              (src_bytes_available_after >= match_bytes_after + 4);
          break;
        case ExpandMethod::BOTH:
          can_expand_backward = (src_bytes_available_before >= match_bytes_before + 4);
          can_expand_forward = (src_bytes_available_after >= match_bytes_after + 4);
          break;
        default:
          throw logic_error("invalid expand method");
      }
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

  void find_all_matches(uint32_t src_addr) const {
    if (!this->src_file) {
      throw runtime_error("no source file selected");
    }

    unordered_map<string, uint32_t> results;
    for (const auto& it : files) {
      if (it.second == this->src_file) {
        log.info("(%s) %08" PRIX32 " (from source)", it.first.c_str(), src_addr);
        results.emplace(it.first, src_addr);
      } else {

        array<future<uint32_t>, 7> futures;
        static const array<ExpandMethod, 7> methods = {
            ExpandMethod::FORWARD,
            ExpandMethod::FORWARD_WITH_BARRIER,
            ExpandMethod::BACKWARD,
            ExpandMethod::BACKWARD_WITH_BARRIER,
            ExpandMethod::BOTH,
            ExpandMethod::BOTH_WITH_BARRIER,
            ExpandMethod::BOTH_IGNORE_ORIGIN,
        };
        for (size_t z = 0; z < methods.size(); z++) {
          futures[z] = async(&ARCodeTranslator::find_match, this, it.second, src_addr, methods[z]);
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

  void handle_command(const string& command) {
    auto tokens = split(command, ' ');
    if (tokens.empty()) {
      throw runtime_error("no command given");
    }
    strip_trailing_whitespace(tokens[tokens.size() - 1]);

    if (tokens[0] == "use") {
      this->set_source_file(tokens.at(1));
    } else if (tokens[0] == "match") {
      this->find_all_matches(stoul(tokens.at(1), nullptr, 16));
    } else if (tokens[0] == "find-globals") {
      this->find_rtoc_global_regs();
    } else if (!tokens[0].empty()) {
      throw runtime_error("unknown command");
    }
  }

  void run_shell() {
    while (!feof(stdin)) {
      if (!this->src_filename.empty()) {
        fprintf(stdout, "ar-trans:%s/%s> ", this->directory.c_str(), this->src_filename.c_str());
      } else {
        fprintf(stdout, "ar-trans:%s> ", this->directory.c_str());
      }
      fflush(stdout);

      string command = fgets(stdin);
      try {
        this->handle_command(command);
      } catch (const exception& e) {
        this->log.error("Failed: %s", e.what());
      }
    }
    fputc('\n', stdout);
  }

private:
  PrefixedLogger log;
  string directory;
  unordered_map<string, shared_ptr<const DOLFile>> files;
  string src_filename;
  shared_ptr<const DOLFile> src_file;
};

void run_ar_code_translator(const std::string& directory, const std::string& use_filename, const std::string& command) {
  ARCodeTranslator trans(directory);
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
  DOLFile a(a_filename.c_str());
  DOLFile b(b_filename.c_str());
  auto a_mem = make_shared<MemoryContext>();
  auto b_mem = make_shared<MemoryContext>();
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
